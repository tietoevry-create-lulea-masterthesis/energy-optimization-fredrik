#include <iostream>
#include "components.h"

using namespace std;
using namespace chrono;

// ============
// RU Functions
// ============

/// @brief Recalculates the current power consumption of the RU, measured in milliwatts
void RU::calc_p()
{
    switch (this->type)
    {
    case RUType::macro:
        // Using macro values:

        // if no users, set to sleep mode level of power consumption
        if (this->alloc_PRB == 0) this->p = 225000 * 0.15;

        // Calculate current p as P_sleep + P_max * current load
        this->p = 225000 + 30000 * ((float)this->alloc_PRB / (float)this->num_PRB);
        break;
    
    case RUType::micro:
        // Using micro values:

        // if no users, set to sleep mode level of power consumption
        if (this->alloc_PRB == 0) this->p = 40000 * 0.15;

        // Calculate current p as P_sleep + P_max * current load
        else this->p = 40000 + 20000 * ((float)this->alloc_PRB / (float)this->num_PRB);
        break;
    }
}

// Default (will create invalid RU, should only be used when initializing arrays of RUs)
RU::RU()
{
}

RU::RU(string uid, float coords[2], int antennae, int bandwidth, bool macro)
{
    this->uid = uid;
    this->coords[0] = coords[0];
    this->coords[1] = coords[1];
    this->antennae = antennae;
    this->bandwidth = bandwidth;

    // Calculate numPRBs
    this->num_PRB = bandwidth / 180000;               // num PRBs defined as bandwidth divided by size of 1 PRB (180 kHz)
    this->alloc_PRB = 2;                              // initial allocated PRBs: 2 (allocated for scanning purposes if i remember correct)
    this->last_meas_t = high_resolution_clock::now(); // initialize last measure time to init time
    this->calc_p();

    if (macro) this->type = RUType::macro;
}

const string RU::get_UID()
{
    return this->uid;
}

const float *RU::get_coords()
{
    return this->coords;
}

const RUType RU::get_type()
{
    return this->type;
}

/// @brief c++ is way too basic for my taste
/// @return a string representation of the RU's type ("macro" or "micro")
const std::string RU::get_type_string()
{
    switch (this->type)
    {
        case RUType::macro:
            return "macro";
        break;

        case  RUType::micro:
            return "micro";
        break;
    }

    return ""; // should be unreachable
}

const int RU::get_num_PRB()
{
    return this->num_PRB;
}

const int RU::get_alloc_PRB()
{
    return this->alloc_PRB;
}

float RU::calc_delta_p()
{
    duration<float> delta_t = duration_cast<duration<float>>(high_resolution_clock::now() - last_meas_t);
    last_meas_t = high_resolution_clock::now();
    // cout << this->uid << " delta_t: " << delta_t.count() << "\n";
    float delta_p = delta_t.count() * this->p;
    // cout << this->uid << " delta_p: " << delta_p << "\n";
    p_tot += delta_p;
    return delta_p;
}

const float RU::get_p()
{
    return this->p;
}

const float RU::get_p_tot()
{
    return this->p_tot;
}

void RU::set_alloc_PRB(int a_PRB)
{
    this->alloc_PRB = a_PRB;
    this->calc_p();
}

// ============
// UE Functions
// ============

UE::UE(string uid, float coords[2], float timer)
{
    this->uid = uid;
    this->coords[0] = coords[0];
    this->coords[1] = coords[1];
    this->timer = timer;
    this->last_meas_t = high_resolution_clock::now(); // initialize last measure time to init time
}

const string UE::get_UID()
{
    return this->uid;
}

const float *UE::get_coords()
{
    return this->coords;
}

const int UE::get_demand()
{
    return this->prb_demand;
}

const RU_entry *UE::get_sig_arr()
{
    return this->sig_arr;
}

bool UE::decrement_timer()
{
    duration<float> delta_t = duration_cast<duration<float>>(high_resolution_clock::now() - last_meas_t);
    last_meas_t = high_resolution_clock::now();
    this->timer -= delta_t.count();
    if (this->timer <= 0) return true;

    return false;
}

void UE::set_sig_arr(RU_entry new_sig_arr[UE_CLOSEST_RUS])
{
    for (size_t i = 0; i < UE_CLOSEST_RUS; i++)
    {
        this->sig_arr[i] = new_sig_arr[i];
    }
}
