// =====================================================================
//  display.h  -  Module B: OLED + buzzer owner, alert queue consumer
//  Logic: B2 v1.1 section 4 (display_alert_task), A1 v1.0 section 6
// =====================================================================
#pragma once
#include "shared.h"

bool display_init();            // returns false if the OLED does not answer
void display_alert_task(void*); // 500 ms, woken early by the alert queue
