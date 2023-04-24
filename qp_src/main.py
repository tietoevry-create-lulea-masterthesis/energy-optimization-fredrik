# ==================================================================================
#       Copyright (c) 2020 AT&T Intellectual Property.
#       Copyright (c) 2020 HCL Technologies Limited.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#          http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# ==================================================================================
"""
qp module main -- using Time series ML predictor

RMR Messages:
HP_INVESTIGATE    30036
HP_HANDOVERS      30037

The 30036 RMR message that is received from the EE-xApp contains a list of RUs to
investigate in order to find potential beneficial handovers of UEs

The 30037 RMR message is then meant to be sent back, containing UE UIDs along with
tuples of RU UIDs that symbolize beneficial handovers

"""
import os
import json
from mdclogpy import Logger
from ricxappframe.xapp_frame import RMRXapp, rmr
from prediction import forecast
from qptrain import train
from database import DATABASE, DUMMY
from exceptions import DataNotMatchError
import warnings
# import schedule
warnings.filterwarnings("ignore")

# pylint: disable=invalid-name
qp_xapp = None
db = None
logger = Logger(name=__name__)


def post_init(self):
    """
    Function that runs when xapp initialization is complete
    """
    self.predict_requests = 0
    logger.debug("HP xApp started")


def qp_default_handler(self, summary, sbuf):
    """
    Function that processes messages for which no handler is defined
    """
    logger.debug("default handler received message type {}".format(summary[rmr.RMR_MS_MSG_TYPE]))
    # we don't use rts here; free this
    self.rmr_free(sbuf)

def handover_predict_handler(self, summary, sbuf):
    """
    Function that processes messages for type 30036
    """
    logger.debug("handover predict handler received payload {}".format(summary[rmr.RMR_MS_PAYLOAD]))
    pred_msg = predict_handovers(summary[rmr.RMR_MS_PAYLOAD])
    print("pred_msg:", pred_msg)
    self.predict_requests += 1
    # we don't use rts here; free this
    self.rmr_free(sbuf)

    if (pred_msg == "{}"):
        print("Empty handover prediction, potential database misread?")
    else:
        success = self.rmr_send(pred_msg.encode(), 30037)
        logger.debug("Sending message to EE-xApp : {}".format(pred_msg))  # For debug purpose
        if success:
            logger.debug("predict handler: sent message successfully")
        else:
            logger.warning("predict handler: failed to send message")

def qp_predict_handler(self, summary, sbuf):
    """
    Function that processes messages for type 30000
    """
    logger.debug("predict handler received payload {}".format(summary[rmr.RMR_MS_PAYLOAD]))
    pred_msg = predict(summary[rmr.RMR_MS_PAYLOAD])
    self.predict_requests += 1
    # we don't use rts here; free this
    self.rmr_free(sbuf)
    success = self.rmr_send(pred_msg.encode(), 30002)
    logger.debug("Sending message to ts : {}".format(pred_msg))  # For debug purpose
    if success:
        logger.debug("predict handler: sent message successfully")
    else:
        logger.warning("predict handler: failed to send message")


def cells(ue):
    """
        Extract neighbor cell id for a given UE
    """
    db.read_data(ueid=ue)
    df = db.data
    cells = []
    if df is not None:
        nbc = df.filter(regex=db.nbcells).values[0].tolist()
        srvc = df.filter(regex=db.servcell).values[0].tolist()
        cells = srvc+nbc
    return cells

def predict_handovers(payload):
    """
        Investigates all UEs connected to each reported RU and finds possible ways to move them
    """
    output = {}
    payload = json.loads(payload)
    ru_list = payload['RUPredictionSet']

    sleep_targets = []      # init collection of RUs that should be offloaded/slept
    ru_fame_dict = {}       # dict containing number of UEs that know each RU, "famous" RUs should be prioritized and kept awake
    ru_PRB_dict = {}        # dict containing number of free PRBs for each known RU
    
    db.read_ru_data() # gather all RU data
    ru_data = db.data # store RU data

    if ru_data is None:
        return "{}" # return empty json, "{}" makes sure no message is sent

    # first, find famous RUs
    for ru_uid in ru_list:
        # gather last data point for current RU
        latest_ru_entry = ru_data.loc[ru_data['uid'] == ru_uid].tail(1)

        # check if RU's latest entry has any connected UEs
        if (len(latest_ru_entry["connections"]) > 0):
            conn_list = latest_ru_entry["connections"][0] # gather string containing all connected UEs

            if (conn_list is not None):
                # If RU has connected UEs, split into array, removing last entry since it will be an empty string
                # conn_list will look like "UE_1,UE_2,UE_3,"
                conn_list = conn_list.split(",")[:-1]

                # loop through and gather data on each UE to find closest RUs
                for ue in conn_list:
                    db.read_ue_data(ue)
                    latest_ue_entry = db.data.tail(1) # only interested in last entry (most recent UE status report)

                    # latest_ue_entry["near_RU"][0] will look like "RU_52,RU_51,RU_62,RU_42,RU_53,RU_61,RU_41,RU_63,RU_43,RU_50,"
                    # take string defining all known RUs and split it into list, excluding last element (blank string)
                    known_ru_list = latest_ue_entry["near_RU"][0].split(",")[:-1]

                    for known_ru in known_ru_list:
                        if known_ru in list(ru_fame_dict.keys()):
                            # if RU has been encountered before, increase counter by one
                            ru_fame_dict[known_ru] += 1
                        else:
                            # else, initialize RU encounter counter to 1 while also entering the RU's free PRBs
                            ru_fame_dict[known_ru] = 0
                            ru_info = ru_data.loc[ru_data['uid'] == known_ru].tail(1) # get last data point for RU
                            ru_PRB_dict[known_ru] = ru_info['free_PRB'][0]

    # second, sort dictionary tuples by famousness, from least to most famous
    famous_ru_list = {key: val for key, val in sorted(ru_fame_dict.items(), key = lambda ele: ele[1])}
    
    # for each non-famous RU, iterate through all connected UEs
    for ru_uid in list(famous_ru_list.keys()):
        print("investigating ", ru_uid)
        latest_ru_entry = ru_data.loc[ru_data['uid'] == ru_uid].tail(1)

        # check if RU's latest entry has any connected UEs
        if (len(latest_ru_entry["connections"]) > 0):
            conn_list = latest_ru_entry["connections"][0] # gather string containing all connected UEs

            if (conn_list is not None):
                # If RU has connected UEs, split into array, removing last entry since it will be an empty string
                # conn_list will look like "UE_1,UE_2,UE_3,"
                conn_list = conn_list.split(",")[:-1]

                sleep_possible = True
                potential_handovers = {}
                for ue in conn_list:
                    db.read_ue_data(ue)
                    latest_ue_entry = db.data.tail(1) # only interested in last entry (most recent UE status report)

                    known_ru_list = latest_ue_entry["near_RU"][0].split(",")[:-1]

                    # for each connected UE, iterate through all known RUs that are not the current RU and are not in sleep_targets
                    for known_ru in known_ru_list:
                        if known_ru == ru_uid or known_ru in list(sleep_targets):
                            continue
                        else:
                            # if the known RU has enough PRBs for the UE, deduct the PRB number by the UE's demand and create a temporary handover request
                            if ru_PRB_dict[known_ru] >= latest_ue_entry["demand"][0]:
                                ru_PRB_dict[known_ru] -= latest_ue_entry["demand"][0]
                                potential_handovers[ue] = ru_uid + "," + known_ru
                                print("\t\t ", ue, " can be handed to ", known_ru)
                                break
                            
                            # else if UE has reached end of list of known RUs without being able to connect to any of them, sleep is impossible
                            elif known_ru == known_ru_list[len(known_ru_list) - 1]:
                                print("\t\t ", ue, " can NOT be handed anywhere, ", ru_uid, " will not be slept")
                                sleep_possible = False

                # if all UEs can be handed over to some other RUs, add current RU to sleep_targets and add temporary handover requests to final handover request list
                if sleep_possible:
                    sleep_targets.append(ru_uid)
                    for key in list(potential_handovers.keys()):
                        output[key] = potential_handovers[key]
                else:
                    print(ru_uid, " will not be slept")

    """
    # stupid test: for each RU, see if connected UEs are close to RU_52, if so, handover them to RU_52
    for ru_uid in ru_list:
        # gather last data point for current RU
        latest_ru_entry = ru_data.loc[ru_data['uid'] == ru_uid].tail(1)

        # check if RU's latest entry has any connected UEs
        if (len(latest_ru_entry["connections"]) > 0):
            conn_list = latest_ru_entry["connections"][0] # gather string containing all connected UEs

            if (conn_list is not None):
                # If RU has connected UEs, split into array, removing last entry since it will be an empty string
                # conn_list will look like "UE_1,UE_2,UE_3,"
                conn_list = conn_list.split(",")[:-1]

                # loop through and gather data on each UE to find closest RUs
                for ue in conn_list:
                    db.read_ue_data(ue)
                    latest_ue_entry = db.data.tail(1) # only interested in last entry (most recent UE status report)
                    
                    # see if RU_52 is in the array of nearby RUs, and is not already the closest
                    if (latest_ue_entry["near_RU"][0].find("RU_52") > 0):
                        # ok but finding signal strength is a little tougher, since u need index which depends on number of commas.
                        # print("Found RU_52, signal strength: " + str(latest_ue_entry[][0]))
                        
                        # construct handover prediction
                        # will look like "UE_1": "RU_61,RU_52"
                        output[ue] = ru_uid + ",RU_52"
    """

    # return all handovers
    return json.dumps(output)

def predict(payload):
    """
     Function that forecast the time series
    """
    output = {}
    payload = json.loads(payload)
    ue_list = payload['UEPredictionSet']
    for ueid in ue_list:
        tp = {}
        cell_list = cells(ueid)
        for cid in cell_list:
            train_model(cid)
            mcid = cid.replace('/', '')
            db.read_data(cellid=cid, limit=101)
            if db.data is not None and len(db.data) != 0:
                try:
                    inp = db.data[db.thptparam]
                except DataNotMatchError:
                    logger.debug("UL/DL parameters do not exist in provided data")
                df_f = forecast(inp, mcid, 1)
                if df_f is not None:
                    tp[cid] = df_f.values.tolist()[0]
                    df_f[db.cid] = cid
                    db.write_prediction(df_f)
                else:
                    tp[cid] = [None, None]
        output[ueid] = tp
    return json.dumps(output)


def train_model(cid):
    if not os.path.isfile('src/'+cid):
        train(db, cid)


def start(thread=False):
    """
    This is a convenience function that allows this xapp to run in Docker
    for "real" (no thread, real SDL), but also easily modified for unit testing
    (e.g., use_fake_sdl). The defaults for this function are for the Dockerized xapp.
    """
    logger.debug("HP xApp starting")
    global qp_xapp
    connectdb(thread)
    fake_sdl = os.environ.get("USE_FAKE_SDL", None)
    qp_xapp = RMRXapp(qp_default_handler, rmr_port=4560, post_init=post_init, use_fake_sdl=bool(fake_sdl))
    qp_xapp.register_callback(handover_predict_handler, 30036)
    qp_xapp.run(thread)


def connectdb(thread=False):
    # Create a connection to InfluxDB if thread=True, otherwise it will create a dummy data instance
    global db
    if thread:
        db = DUMMY()
    else:
        db = DATABASE()
    success = False
    while not success and not thread:
        success = db.connect()


def stop():
    """
    can only be called if thread=True when started
    TODO: could we register a signal handler for Docker SIGTERM that calls this?
    """
    global qp_xapp
    qp_xapp.stop()


def get_stats():
    """
    hacky for now, will evolve
    """
    global qp_xapp
    return {"PredictRequests": qp_xapp.predict_requests}
