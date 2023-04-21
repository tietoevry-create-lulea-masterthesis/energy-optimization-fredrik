#include <random>
#include <string>
#include <thread>
#include <InfluxDBFactory.h>
#include "constants.h"
#include "components.h"
#include "sim.h"

using namespace std;

RU sim_RUs[RU_NUM];
list<UE> sim_UEs;
list<UE> RU_conn[RU_NUM]; // Array of lists, one list for each RU that keeps track of all UEs connected to it

const float max_coord = 5000; // Determines the maximum x and y coordinate of the simulation map
const float margin = 0.2;     // Determines how far from the edges micro-RU grid should be (0.2 margin on 5000 max_coord means grid will stretch from 1000-4000)

unsigned int seed = 42;
default_random_engine rng;
normal_distribution<float> coord_distribution(max_coord / 2, max_coord / 2 / 5);

int i_ue = 0; // iterator for UE uid's

extern int main(int argc, char **argv)
{
    srand(seed);
    rng = default_random_engine(seed);

    int ru_i = 0;

    // Place Micro-RUs
    for (size_t y = 0; y < sqrt(RU_NUM); y++)
    {
        for (size_t x = 0; x < sqrt(RU_NUM); x++)
        {
            // Forms an even grid of RUs in coordinate space
            sim_RUs[ru_i] = *new RU("RU_" + to_string(ru_i), new float[2]
            {
                x / (sqrtf(RU_NUM) - 1) * max_coord * (1 - margin * 2) + max_coord * margin,
                y / (sqrtf(RU_NUM) - 1) * max_coord * (1 - margin * 2) + max_coord * margin
            },
            2, (rand() % 1 + 1) * 2000000); // 2T2R with either 2 MHz or 4MHz bandwidth

            ru_i++; // iterate ru_i for each RU created
        }
    }

    // Exchange 3 Micro-RUs with Macro-RUs where each one has 20 MHz bandwidth (100 PRBs)
    sim_RUs[25] = *new RU("RU_25", new float[2]{2500, 1000}, 4, 20000000, true);
    sim_RUs[50] = *new RU("RU_50", new float[2]{1000, 3500}, 4, 20000000, true);
    sim_RUs[75] = *new RU("RU_75", new float[2]{4000, 3500}, 4, 20000000, true);

    // Spawn UEs
    for (size_t i = 0; i < 150; i++)
    {
        // initialize a bunch of UEs with timers ranging from 60-120
        sim_UEs.push_back(*new UE("UE_" + to_string(i_ue), new float[2]{fmodf(coord_distribution(rng), max_coord), fmodf(coord_distribution(rng), max_coord)}, fmodf(rand(), 6000) / 100 + 60));
        i_ue++;
    }

    // Debug RU placement
    /* for (auto &&ru : sim_RUs)
    {
        cout << "ru UID: " + ru.get_UID() + ", coords: " + to_string(ru.coords[0]) + "," + to_string(ru.coords[1]) << "\n";
    } */

    // Connect each UE to closest RU
    for (auto &&ue : sim_UEs)
    {
        string closest = find_closest_rus(&ue); // O(n) time for each UE
        RU_conn[stoi(closest.substr(3))].push_back(ue);
    }

    // Calculate resulting load for each RU
    for (size_t i = 0; i < RU_NUM; i++)
    {
        sim_RUs[i].set_alloc_PRB(calc_alloc_PRB(i));
    }

    long simulation_duration = 60;                    // Determines how long the simulation should last (in seconds)
    thread sim_thread(sim_loop, simulation_duration); // Starts the simulation concurrently

    auto influxdb = influxdb::InfluxDBFactory::Get("http://root:rootboot@localhost:8086?db=RIC-Test");

    // For as long as simulation is running, keep instancing new, seeded UEs
    while (sim_running())
    {
        this_thread::sleep_for(chrono::milliseconds(rand() % 1000 + 300)); // sleep for 0.3 - 1.3 seconds
        lock_ue_mutex();

        UE spawn_ue = *new UE("UE_" + to_string(i_ue), new float[2]{fmodf(coord_distribution(rng), max_coord), fmodf(coord_distribution(rng), max_coord)}, fmodf(rand(), 6000) / 100 + 60);

        sim_UEs.push_back(spawn_ue);
        find_closest_rus(&spawn_ue);
        const RU_entry *sig_arr = spawn_ue.get_sig_arr();

        bool insuff_capacity = true;
        for (size_t i = 0; i < UE_CLOSEST_RUS; i++)
        {
            // If the current RU has enough free PRBs to handle the spawned UE's demand, connect to it and break loop
            if (sig_arr[i].ru->get_alloc_PRB() + spawn_ue.get_demand() < sig_arr[i].ru->get_num_PRB())
            {
                int ru_index = stoi(sig_arr[i].ru->get_UID().substr(3));
                RU_conn[ru_index].push_back(spawn_ue);
                sim_RUs[ru_index].set_alloc_PRB(calc_alloc_PRB(ru_index));
                insuff_capacity = false;
                cout << spawn_ue.get_UID() + " connected to " + sig_arr[i].ru->get_UID() << endl;
                break;
            }
        }

        if (insuff_capacity) cout << "Warning! " << spawn_ue.get_UID() << " was unable to connect due to insufficient capacity" << endl;

        i_ue++;

        // Also write UE info to database
        influxdb->write(influxdb::Point{"sim_UEs"}
                            .addField("demand", spawn_ue.get_demand())
                            .addField("near_RU", stringify_sig_str_arr(&spawn_ue))
                            .addField("near_RU_sig", stringify_sig_str_arr(&spawn_ue, true))
                            .addTag("uid", spawn_ue.get_UID()));
        
        unlock_ue_mutex();
    }
    
    sim_thread.join(); // wait for sim thread to exit and rejoin

    return 0;
}