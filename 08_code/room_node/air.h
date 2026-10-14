// =====================================================================
//  air.h  -  Module C: air quality, exhaust fan, status light
//  Logic: 04_logic/C2_logic_air.md (v1) adapted to A1 v1.0
// =====================================================================
#pragma once
#include "shared.h"

enum AirState : uint8_t { S0_ROOM_EMPTY, S1_AIR_GOOD, S2_AIR_MODERATE, S3_AIR_POOR, S4_SENSOR_FAULT };
void air_init();
void air_sensor_task(void*);   // 5000 ms: read sensor + FSM + fan + RGB, then notify comm
void status_blink_task(void*); // 500 ms: blink red in SENSOR_FAULT
AirState air_state();
const char* air_state_name(AirState s);
