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

float calc_dist(RU ru, UE ue);

int calc_alloc_PRB(int ru_index);

/// @brief Finds the n closest RUs to a given UE, and inserts these into the UE's dist_map
/// @param ue the ue to find RUs and replace dist_map of
/// @param n_closest The amount of nearby RUs to insert into the dist_map
/// @return Returns the closest RU, since that is probably the most interesting one
std::string find_closest_rus(UE *ue, int n_closest);

void *sim_loop(void *arg);