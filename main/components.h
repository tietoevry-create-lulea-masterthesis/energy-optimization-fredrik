#pragma once
#include <chrono>
#include <map>
#include "constants.h"

class RU
{
private:
    std::string uid;
    float coords[2];         // x, y coords
    int antennae = 4;        // default: 4T4R
    int bandwidth = 4000000; // default: 4 MHz
    int num_PRB;             // number of physical resource blocks, depends on the bandwidth
    int alloc_PRB;           // number of physical resource blocks that have been allocated to UE

    float p = 3;                                            // power consumption, dependent on current traffic load, default = sleep power consumption, 3 mW or smth
    float p_tot = 0;                                        // total power consumption since t = 0
    std::chrono::_V2::system_clock::time_point last_meas_t; // time since last delta measurement of power consumption

public:
    RU();
    RU(std::string uid, float coords[2], int antennae, int bandwidth);

    const std::string get_UID();
    const float *get_coords();
    const int get_num_PRB();
    const int get_alloc_PRB();
    float calc_delta_p();
    const float get_p();
    const float get_p_tot();
    void set_alloc_PRB(int a_PRB);
};

// RU entry for use in the dist_list in UE class
struct RU_entry
{
    std::string ru_uid;
    float dist;

    RU_entry() {
        ru_uid = "NULL";
        dist = INT32_MAX;
    }

    RU_entry(std::string ru_uid, float dist) : ru_uid(ru_uid), dist(dist)
    {
    }

    bool operator<(RU_entry const &e)
    {
        return (this->dist < e.dist);
    }
};

class UE
{
private:
    std::string uid;
    float coords[2]; // x, y coords

    int prb_demand = 2;                    // amount of physical resource blocks that the traffic of this UE demands
    RU_entry dist_arr[UE_CLOSEST_RUS]; // array of n closest RUs

public:
    UE(std::string uid, float coords[2]);

    const std::string get_UID();
    const float *get_coords();
    const int get_demand();
    const RU_entry *get_dist_arr();

    bool operator==(UE const &ue)
    {
        return (ue.uid.compare(this->uid) == 0);
    }

    void set_dist_arr(RU_entry *new_dist_arr);
};