#include <iostream>
#include "components.h"

using namespace std;
using namespace chrono;

// ============
// RU Functions
// ============

// Default (will create invalid RU)
RU::RU()
{
}

RU::RU(string uid, float coords[2], int antennae, int bandwidth)
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
}

const string RU::get_UID()
{
    return this->uid;
}

const float *RU::get_coords()
{
    return this->coords;
}

const RUType RU::getType()
{
    return this->type;
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
}

// ============
// UE Functions
// ============

UE::UE(string uid, float coords[2])
{
    this->uid = uid;
    this->coords[0] = coords[0];
    this->coords[1] = coords[1];
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

void UE::set_sig_arr(RU_entry new_sig_arr[UE_CLOSEST_RUS])
{
    for (size_t i = 0; i < UE_CLOSEST_RUS; i++)
    {
        this->sig_arr[i] = new_sig_arr[i];
    }
}
