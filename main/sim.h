#pragma once
#include <list>
#include <iostream>
#include "constants.h"
#include "components.h"

extern RU sim_RUs[RU_NUM];
extern std::list<UE> sim_UEs;
extern std::list<UE> RU_conn[]; // Array of lists, one list for each RU that keeps track of all UEs connected to it

/// @brief Prints all UEs connected to a given RU
/// @param ru_index The RUs index in the sim_RUs array
void print_ue_conn(int ru_index);

/// @brief Simulates a UE handover by moving a UE from one RU to another in the RU_conn array
/// @param ue_uid the uid of the UE to be moved
/// @param from_RU the RU that currently holds the UE
/// @param to_RU the RU that the UE should be moved to
/// @return true if handover is successful, false otherwise
bool handover(std::string ue_uid, int from_RU, int to_RU);

/// @brief Removes a UE from the simulation
/// @param ue the ue that should be removed
/// @param ru_index the ru that the UE is currently connected to
void remove_ue(UE *ue, int ru_index);

void get_ue_mutex();
void release_ue_mutex();

float calc_sig_str(RU ru, UE ue);

/// @brief Calculates the number of PRBs allocated to UEs for a given RU
/// @param ru_index the index of the RU in the sim_RUs array
/// @return the number of allocated PRBs, will return 0 if no UEs are connected
int calc_alloc_PRB(int ru_index);

/// @brief Finds the n closest RUs to a given UE, and inserts these into the UE's sig_arr
/// @param ue the ue to find RUs and replace sig_arr of
/// @return Returns the closest RU, since that is probably the most interesting one
std::string find_closest_rus(UE *ue);

std::string stringify_connected_ues(int ru_index);

/// @brief Stringifies signal strength array in order to update database
/// @param ue The UE to stringify the array of
/// @param dist If false, returns a string containing the UID's of each of the closest RUs. If true, returns the signal strengths to each of the closest RUs
/// @return A string dependent on the value of the dist bool.
std::string stringify_sig_str_arr(UE *ue, bool dist = false);

void *sim_loop(void *arg);