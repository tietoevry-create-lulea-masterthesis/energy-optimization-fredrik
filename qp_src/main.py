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

    # stupid test: for each RU, see if connected UEs are close to RU_52, if so, handover them to RU_52
    db.read_ru_data() # gather all RU data
    ru_data = db.data # store RU data

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
