// =====================================================================
//  light.h  -  Module D: constant illuminance + colour by time of day
//  Logic: 04_logic/D3_logic_light.md (v1)
// =====================================================================
#pragma once
#include "shared.h"

enum LightState : uint8_t { L_OFF, L_REGULATING, L_DIMMING, L_SENSOR_FAULT };
void light_init();
void light_control_task(void*);   // 200 ms
LightState light_state();
const char* light_state_name(LightState s);
