#include <unistd.h>
#include <chrono>
#include <ctime>
#include <vector>
#include <cmath>
#include <algorithm>
#include "sim.h"
#include <InfluxDBFactory.h>

using namespace std;

void print_ue_conn(int ru_index)
{
    cout << sim_RUs[ru_index].get_UID() + ":\n";
    for (auto &&ue : RU_conn[ru_index])
    {
        cout << ue.get_UID() + "\n";
    }
}

bool handover(string ue_uid, int from_RU, int to_RU)
{
    UE *ue_ptr = nullptr;

    // Find UE that is to be handed over
    for (auto ue : RU_conn[from_RU])
    {
        if (ue.get_UID().compare(ue_uid) == 0)
        {
            ue_ptr = &ue;
            break;
        }
    }

    // If UE was not found, return false
    if (!ue_ptr)
    {
        cout << "!!! ERROR: Couldn't find UE to hand over !!!\n";
        return false;
    }

    // Remove (THE CORRECT) UE from current RU and add to new RU
    RU_conn[from_RU].remove(*ue_ptr);
    RU_conn[to_RU].push_back(*ue_ptr);

    // Calc new load for each RU
    sim_RUs[from_RU].set_alloc_PRB(from_RU);
    sim_RUs[to_RU].set_alloc_PRB(to_RU);

    cout << "Moved " + ue_ptr->get_UID() + " from RU_" << from_RU << " to RU_" << to_RU << endl;

    return true;
}

float calc_dist(RU ru, UE ue)
{
    const float *ru_coords = ru.get_coords();
    const float *ue_coords = ue.get_coords();

    /* std::cout << "ru coords: \n";
    std::cout << ru_coords[0] << "\n";
    std::cout << ru_coords[1] << "\n";

    std::cout << "ue coords: \n";
    std::cout << ue_coords[0] << "\n";
    std::cout << ue_coords[1] << "\n"; */

    float dist = sqrt(pow(ru_coords[0] - ue_coords[0], 2) + pow(ru_coords[1] - ue_coords[1], 2));

    // std::cout << "dist: " << dist << "\n";

    return dist;
}

int calc_alloc_PRB(int ru_index)
{
    int alloc_PRB = 2; // 2 slots allocated by default??
    for (auto &&ue : RU_conn[ru_index])
    {
        alloc_PRB += ue.get_demand();
    }

    // Sanity check
    if (alloc_PRB > sim_RUs[ru_index].get_num_PRB())
    {
        cout << "!!! ERROR: More PRBs allocated for " + sim_RUs[ru_index].get_UID() + " than available !!!\n";
        cout << "Allocated: " << sim_RUs[ru_index].get_alloc_PRB() << ", Available: " << sim_RUs[ru_index].get_num_PRB() << "\n";
    }

    return alloc_PRB;
}

string find_closest_rus(UE *ue, int n_closest)
{
    RU_entry candidates[UE_CLOSEST_RUS];
    float dist;
    for (size_t i = 0; i < RU_NUM; i++)
    {
        // Check if distance is less than RU furthest away
        dist = calc_dist(sim_RUs[i], *ue);
        if (dist < candidates[UE_CLOSEST_RUS - 1].dist)
        {
            // Swap last candidate with newfound RU and sort
            candidates[UE_CLOSEST_RUS - 1] = RU_entry(sim_RUs[i].get_UID(), dist);
            sort(begin(candidates), end(candidates));
        }
    }

    ue->set_dist_arr(candidates);

    return candidates[0].ru_uid;
}

string stringify_connected_ues(int ru_index)
{
    string ue_string = "";

    for (auto &&ue : RU_conn[ru_index])
    {
        ue_string += ue.get_UID() + ",";
    }

    return ue_string;
}

struct HandoverPoint
{
    int decision_no;
    string handover_decisions;

    HandoverPoint(string fields)
    {
        // fields string arrives in format: decisions=UE_5,RU_61,RU_52:UE_43,RU_61,RU_52:UE_15,RU_62,RU_52:UE_65,RU_62,RU_52:decision_no=18
        string decisions = fields.substr(fields.find("decisions=") + 10, fields.find(":decision_no") - 10);
        string decision_no = fields.substr(fields.find("decision_no=") + 12);

        this->decision_no = atoi(decision_no.c_str());
        this->handover_decisions = decisions;
    }

    vector<string> separate_handovers()
    {
        // decisions string will look like: UE_5,RU_61,RU_52:UE_43,RU_61,RU_52:UE_15,RU_62,RU_52:UE_65,RU_62,RU_52
        // where each handover decision is separated by a colon

        //cout << "handovers: " << this->handover_decisions << endl;

        string delimiter = ":";
        vector<string> decision_list;

        size_t pos = 0;
        while ((pos = handover_decisions.find(delimiter)) != string::npos)
        {
            decision_list.push_back(handover_decisions.substr(0, pos));
            handover_decisions.erase(0, pos + delimiter.length());
        }
        decision_list.push_back(handover_decisions); // also add last element

        /* cout << "separated handovers:" << endl;
        for (auto &&h : decision_list)
        {
            cout << h << endl;
        } */

        return decision_list;
    }

    void execute_handovers()
    {
        vector<string> decision_list = separate_handovers();

        for (auto &&d : decision_list)
        {
            // parses individual decisions (formatted like: UE_5,RU_61,RU_52) and outputs them into a vector so that
            // components.at(0) holds the UE uid
            // components.at(1) holds the from_RU uid
            // components.at(2) holds the to_RU uid

            cout << "executing handover: " << d << endl;

            string delimiter = ",";
            vector<string> components;

            size_t pos = 0;
            while ((pos = d.find(delimiter)) != string::npos)
            {
                components.push_back(d.substr(0, pos));
                d.erase(0, pos + delimiter.length());
            }
            components.push_back(d); // also add last element

            /* cout << "ue: " << components.at(0) << endl;
            cout << "from_ru: " << components.at(1) << endl;
            cout << "to_ru: " << components.at(2) << endl; */

            if (!handover(components.at(0), atoi(components.at(1).substr(3).c_str()), atoi(components.at(2).substr(3).c_str()))) {
                cout << "ERROR while handing over " + components.at(0) << endl;
            }
        }
    }
};

void *sim_loop(void *arg)
{
    auto influxdb = influxdb::InfluxDBFactory::Get("http://root:rootboot@localhost:8086?db=RIC-Test");
    influxdb->batchOf(100); // creates buffer for writes, only writes to database once 100 points of data have accumulated

    // write all UE data to db (should be done along with each new UE popping up)
    for (auto &&ue : sim_UEs)
    {
        auto ue_point = influxdb::Point{"sim_UEs"}
                            .addField("demand", ue.get_demand());

        for (size_t i = 0; i < UE_CLOSEST_RUS; i++)
        {
            ue_point.addTag("ru_close_" + to_string(i), ue.get_dist_arr()[i].ru_uid)
                .addField("ru_close_dist_" + to_string(i), ue.get_dist_arr()[i].dist); // bad idea?
        }

        influxdb->write(ue_point.addTag("uid", ue.get_UID())); // weird workaround, easiest way to write something that is a Point&&
    }

    int latest_decision_no = 0; // keeps track of ID of latest handover decision that was treated, should probably only increase in value

    while (true)
    {
        // Loop through each RU and simulate power consumption + connections
        for (size_t i = 0; i < RU_NUM; i++)
        {
            sim_RUs[i].calc_delta_p(); // value gotten from delta_p is dependent on last update time and is not interesting
            influxdb->write(influxdb::Point{"sim_RUs"}
                                .addTag("uid", sim_RUs[i].get_UID())
                                .addField("free_PRB", sim_RUs[i].get_num_PRB() - sim_RUs[i].get_alloc_PRB())
                                .addField("current_load", (float)sim_RUs[i].get_alloc_PRB() / (float)sim_RUs[i].get_num_PRB())
                                .addField("p", sim_RUs[i].get_p())
                                .addField("p_tot", sim_RUs[i].get_p_tot())
                                .addField("connections", stringify_connected_ues(i)));
        }

        cout << "written all RU points\n";

        vector<influxdb::Point> handovers = influxdb->query("select * from handovers where time > now() - 10s");

        for (auto &&h : handovers)
        {
            h.floatsPrecision = 0;                                                     // makes parsing decision_no simpler, as it is an integer and would otherwise show up as 1.00000000
            HandoverPoint handover_point = HandoverPoint(h.getTags() + h.getFields()); // for some reason influxDB thinks all fields with string values are tags
            if (handover_point.decision_no > latest_decision_no)
            {
                latest_decision_no = handover_point.decision_no;
                handover_point.execute_handovers();
            }
        }
    }
}
