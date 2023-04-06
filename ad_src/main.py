# ==================================================================================
#  Copyright (c) 2020 HCL Technologies Limited.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
# ==================================================================================

import json
import os
import time
import pandas as pd
import schedule
from ricxappframe.xapp_frame import Xapp
from ricxappframe.xapp_sdl import SDLWrapper
from mdclogpy import Logger
from ad_model import modelling, CAUSE
from ad_train import ModelTraining
from database import DATABASE, DUMMY

db = None
cp = None
sdl = SDLWrapper(use_fake_sdl=True)

logger = Logger(name=__name__)


def entry(self):
    """  If ML model is not present in the path, It will trigger training module to train the model.
      Calls predict function every 10 millisecond(for now as we are using simulated data).
    """
    connectdb()
    #train_model() # won't be needing any ML for this I think
    #load_model()
    schedule.every(0.5).seconds.do(predict, self)
    while True:
        schedule.run_pending()


def load_model():
    global md
    global cp
    md = modelling()
    cp = CAUSE()


def train_model():
    if not os.path.isfile('src/model'):
        mt = ModelTraining(db)
        mt.train()


def predict(self):
    """Read the latest ue sample from influxDB and detects if that is anomalous or normal..
      Send the UEID, DUID, Degradation type and timestamp for the anomalous samples to Traffic Steering (rmr with the message type as 30003)
      Get the acknowledgement of sent message from the traffic steering.
    """
    db.read_data()
    val = None
    if db.data is not None:
        db.data = db.data.dropna(axis=0)
        if len(db.data) > 0:
            val = investigate_RUs(self, db.data)
        else:
            logger.warning("Data undefined")
    else:
        logger.warning("No data in last 3 seconds")
        time.sleep(3)
    if (val is not None) and (len(val) > 2):
        msg_to_ts(self, val)

def investigate_RUs(self, df):
    """ Searches through RU-related data in order to
    1) find large RUs with low loads that can be handed UEs
    2) find small RUs with low loads that can be offloaded of UEs

    Parameter
    ........
    df: array or dataframe

    Return
    ......
    val: situation info(RUID, TimeStamp, SituationType)
    """

    low_ru = df.loc[df['current_load'] < 0.5]
    if (len(low_ru) > 0):
        low_ru = df['uid'].drop_duplicates() # array containing each relevant RU
        for ru in low_ru:
            pass # send an rmr message to ts xapp detailing interesting RUs

def predict_anomaly(self, df):
    """ calls ad_predict to detect if given sample is normal or anomalous
    find out the degradation type if sample is anomalous
    write given sample along with predicted label to AD measurement

    Parameter
    ........
    ue: array or dataframe

    Return
    ......
    val: anomalus sample info(UEID, DUID, TimeStamp, Degradation type)
    """
    df['Anomaly'] = md.predict(df)
    df.loc[:, 'Degradation'] = ''
    val = None
    if 1 in df.Anomaly.unique():
        df.loc[:, ['Anomaly', 'Degradation']] = cp.cause(df, db)
        df_a = df.loc[df['Anomaly'] == 1].copy()
        if len(df_a) > 0:
            df_a['time'] = df_a.index
            cols = [db.ue, 'time', 'Degradation']
            # rmr send 30003(TS_ANOMALY_UPDATE), should trigger registered callback
            result = json.loads(df_a.loc[:, cols].to_json(orient='records'))
            val = json.dumps(result).encode()
    df.loc[:, 'RRU.PrbUsedDl'] = df['RRU.PrbUsedDl'].astype('float')
    df.index = pd.date_range(start=df.index[0], periods=len(df), freq='1ms')
    db.write_anomaly(df)
    return val


def msg_to_ts(self, val):
    # send message from ad to ts
    logger.debug("Sending Anomalous UE to TS")
    success = self.rmr_send(val, 30003)
    if success:
        logger.info(" Message to TS: message sent Successfully")
    # rmr receive to get the acknowledgement message from the traffic steering.
    for (summary, sbuf) in self.rmr_get_messages():
        logger.info("Received acknowldgement from TS (TS_ANOMALY_ACK): {}".format(summary))
        self.rmr_free(sbuf)

def msg_to_ee(self, val):
    # legally distinct from original msg_to_ts (not really)

    # send message from tm to ee
    logger.debug("Sending Traffic Situations to TS")
    success = self.rmr_send(val, 30034) # NOTE !!!!! <---- NEEDS SETTING UP IN ROUTING TABLE IN FOLDER WHERE xApp AND DOCKERFILE IS CONTAINED
    if success:
        logger.info(" Message to TS: message sent Successfully")
    # rmr receive to get the acknowledgement message from the traffic steering.
    for (summary, sbuf) in self.rmr_get_messages():
        logger.info("Received acknowldgement from TS (TS_ANOMALY_ACK): {}".format(summary))
        self.rmr_free(sbuf)


def connectdb(thread=False):
    # Create a connection to InfluxDB if thread=True, otherwise it will create a dummy data instance
    global db
    if thread:
        db = DUMMY()
    else:
        db = DATABASE()
    success = False
    while not success:
        success = db.connect()


def start(thread=False):
    # Initiates xapp api and runs the entry() using xapp.run()
    xapp = Xapp(entrypoint=entry, rmr_port=4560, use_fake_sdl=False)
    xapp.logger.debug("AD xApp starting")
    xapp.run()
