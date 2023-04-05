# Energy Optimization xApp
Software repository for the 'RIC Based Energy Optimization' Master Thesis work by Fredrik Borg.

Contains source code applicable for the use case of increasing energy efficiency of a 5G network, along with a simple 5G network mock simulation which requires an InfluxDB time series database.

# How it works
## RAN Simulator
The simulation of the RAN is done under main/, where network components are defined in components.h/.cpp and instantiated in main.cpp, where the program is also expected to enter a simulation loop where each component is simulated and has its resulting state documented inside a time series database. Helper functions for simulating and uploading state information to the Influx database are found in sim.h/.cpp.

## xApps
Similarly to the use case of the TS-xApp, the energy efficiency use case will require three different xApps.
The AD-xApp is thought to be repurposed as a traffic monitoring xApp, actively scanning the network for interesting situation where increasing energy efficiency is relevant.
The TS-xApp will be repurposed to carry over situations detected by the AD-xApp to the QP-xApp via RMR messages.
The QP-xApp's sole purpose is to be alerted of relevant use case situations where it can investigate the related RUs and UEs (or other relevant information that has been stored in the database) in order to find a possible solution, which should be communicated and detailed to the TS-xApp, which is meant to be responsible for executing handovers and actually steering the traffic.
Ideally the TS-xApp would be responsible for directly redirecting traffic via connections to network components, however as no real components exist in the simulation, the traffic steering decisions made by the TS-xApp will need to be forwarded back to the database where the RAN simulator can read the decisions and adjust the simulation accordingly.