// =====================================================================
//  comm.h  -  Module A (room side): Wi-Fi, MQTT, desk table (A3 v1.0)
// =====================================================================
#pragma once
#include "shared.h"
void comm_init();
void room_communication_task(void*);
void comm_print_status();
void comm_update_counts();
