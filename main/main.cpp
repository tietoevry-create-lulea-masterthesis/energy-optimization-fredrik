#include <random>
#include <string>
#include <InfluxDBFactory.h>
#include "constants.h"
#include "components.h"
#include "sim.h"
#include <unistd.h>

using namespace std;

RU sim_RUs[RU_NUM];
list<UE> sim_UEs;
list<UE> RU_conn[RU_NUM]; // Array of lists, one list for each RU that keeps track of all UEs connected to it

const float max_coord = 5000; // Determines the maximum x and y coordinate of the simulation map

unsigned int seed = 42;

extern int main(int argc, char **argv)
{
    srand(seed);

    int ru_i = 0;

    // Place RUs
    for (size_t y = 0; y < sqrt(RU_NUM); y++)
    {
        for (size_t x = 0; x < sqrt(RU_NUM); x++)
        {
            // Forms an even grid of RUs in coordinate space
            sim_RUs[ru_i] = *new RU("RU_" + to_string(ru_i), new float[2]
            {
                // if coord space = 0-100 and RU_NUM = 100,
                // coords go from 5, 10, 15 ... 95, i.e. equal margins on all sides
                x * max_coord / sqrtf(RU_NUM) + max_coord / sqrtf(RU_NUM) / 2,
                y * max_coord / sqrtf(RU_NUM) + max_coord / sqrtf(RU_NUM) / 2
            },
            4, 4000000);

            ru_i++; // iterate ru_i for each RU created
        }
    }

    // Spawn UEs
    for (size_t i = 0; i < 100; i++)
    {
        sim_UEs.push_back(*new UE("UE_" + to_string(i), new float[2]{fmodf(rand(), max_coord), fmodf(rand(), max_coord)}));
    }

    // Debug RU placement
    /* for (auto &&ru : sim_RUs)
    {
        cout << "ru UID: " + ru.get_UID() + ", coords: " + to_string(ru.coords[0]) + "," + to_string(ru.coords[1]) << "\n";
    } */

    // Connect each UE to closest RU
    for (auto &&ue : sim_UEs)
    {
        string closest = find_closest_rus(&ue, 5); // very idiotic and O(n) time for each UE
        RU_conn[stoi(closest.substr(3))].push_back(ue);
    }

    // Calculate resulting load for each RU
    for (size_t i = 0; i < RU_NUM; i++)
    {
        sim_RUs[i].set_alloc_PRB(calc_alloc_PRB(i));
    }
    
    

    /* cout << "Connections to RU_52: \n";
    for (auto &&i : RU_conn[52])
    {
        const std::map<std::string, float> dmap = i.get_dist_map();
        cout << i.get_UID() << ", entries: " << dmap.size() << "\n";
        for (auto &&val : dmap)
        {
            cout << val.first << ", dist: " << val.second << "\n";
        }
    } */
    

    // hand over via bool checks, to see if errors happen
    /* if (!handover("UE_95", 0, 8)) {
        cout << "ERROR while handing over\n";
    } */

    pthread_t sim_thread; // create the threrhede at some point ig
    pthread_create(&sim_thread, NULL, &sim_loop, NULL);

    // Connects to database and inserts points for each RU and UE
    //auto influxdb = influxdb::InfluxDBFactory::Get("http://root:rootboot@localhost:8086?db=RIC-Test");
    
    /* influxdb->write(influxdb::Point{"sim_RU"}
    .addTag("uid", sim_RUs[52].get_UID())
    .addField("connections", (int) RU_conn[52].size())
    );

    influxdb->write(influxdb::Point{"sim_UE"}
    .addTag("uid", sim_UEs.front().get_UID())
    .addField("closest RUs", (int) sim_UEs.front().get_dist_map().size())
    ); */

    // u most choose
    //sim_loop(nullptr);    // enter loop forever, or
    sleep(999);             // go 2 sleep forever (16 minutes)
    
    return 0;
}