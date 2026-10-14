// =====================================================================
//  comm.h  -  Module A (desk side): Wi-Fi, MQTT, NTP, link state
//  Design: 04_logic/A3_communication.md (v1.0)
// =====================================================================
#pragma once
#include "shared.h"

void comm_init();
void desk_communication_task(void*);   // core 0; publishes every iv s or when notified
void ntp_sync_task(void*);             // checks SNTP, sets time_valid
void comm_print_status();
