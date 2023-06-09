#pragma once
#include <chrono>
#include <map>
#include "constants.h"

enum RUType
{
    macro,
    micro
};

class RU
{
private:
    std::string uid;
    float coords[2];             // x, y coords
    RUType type = RUType::micro; // default: micro-RU
    int antennae = 2;            // default: 2T2R
    int bandwidth = 4000000;     // default: 4 MHz
    int num_PRB;                 // number of physical resource blocks, depends on the bandwidth
    int alloc_PRB;               // number of physical resource blocks that have been allocated to UE

    float p;                                                // power consumption, dependent on current traffic load, measured in mW
    float p_tot = 0;                                        // total power consumption since t = 0, measured in mWs (milliwattsecond which is 0,001 J)
    std::chrono::_V2::system_clock::time_point last_meas_t; // time since last delta measurement of power consumption

    void calc_p();

public:
    RU();
    RU(std::string uid, float coords[2], int antennae, int bandwidth, bool macro = false);

    const std::string get_UID();
    const float *get_coords();
    const RUType get_type();
    const std::string get_type_string();
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
    RU *ru;
    float sig_str;

    RU_entry()
    {
        ru = nullptr;
        sig_str = -1;
    }

    RU_entry(RU *ru, float sig_str)
    {
        this->ru = ru;
        this->sig_str = sig_str;
    }

    bool operator<(RU_entry const &e)
    {
        return (this->sig_str < e.sig_str);
    }

    bool operator>(RU_entry const &e)
    {
        return (this->sig_str > e.sig_str);
    }
};

class UE
{
private:
    std::string uid;
    float coords[2]; // x, y coords

    int prb_demand = 2;                                     // amount of physical resource blocks that the traffic of this UE demands
    float timer;                                            // time until UE expires
    RU_entry sig_arr[UE_CLOSEST_RUS];                       // array of n closest RUs
    std::chrono::_V2::system_clock::time_point last_meas_t; // time since last timer decrement

public:
    UE(std::string uid, float coords[2], float timer);

    const std::string get_UID();
    const float *get_coords();
    const int get_demand();
    const RU_entry *get_sig_arr();

    /// @brief Decrements the UE's timer by the time that's passed since last measurement
    /// @return Returns true if resulting time after decrementing reaches zero or below, false otherwise
    bool decrement_timer();

    bool operator==(UE const &ue)
    {
        return (ue.uid.compare(this->uid) == 0);
    }

    void set_sig_arr(RU_entry *new_sig_arr);
};