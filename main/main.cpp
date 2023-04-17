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

int i_ue = 0; // iterator for UE uid's

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
        sim_UEs.push_back(*new UE("UE_" + to_string(i_ue), new float[2]{fmodf(rand(), max_coord), fmodf(rand(), max_coord)}));
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

    pthread_t sim_thread; // create the threrhede at some point ig
    pthread_create(&sim_thread, NULL, &sim_loop, NULL);

    // For as long as simulation is running, keep instancing new, seeded UEs
    while (true) 
    {
        sleep(rand() % 5 + 5); // sleep for 5-10 seconds
        UE *spawn_ue = *new UE("UE_" + to_string(i_ue), new float[2]{fmodf(rand(), max_coord), fmodf(rand(), max_coord)});

        sim_UEs.push_back(spawn_ue);
        find_closest_rus(&spawn_ue);

        bool insuff_capacity = true;
        for (size_t i = 0; i < UE_CLOSEST_RUS; i++)
        {
            // If the current RU has enough free PRBs to handle the spawned UE's demand, connect to it and break loop
            if (spawn_ue->get_sig_arr()[i].ru->get_alloc_PRB() + spawn_ue->get_demand() < spawn_ue->get_sig_arr()[i].ru->get_num_PRB())
            {
                RU_conn[stoi(spawn_ue->get_sig_arr()[i].ru->get_UID().substr(3))].push_back(spawn_ue);
            }
        }
        i_ue++;
    }
    
    return 0;
}