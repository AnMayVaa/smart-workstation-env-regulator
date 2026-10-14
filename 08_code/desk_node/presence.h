// =====================================================================
//  presence.h  -  Module B: presence, break reminders, button UI
//  Logic: 04_logic/B2_logic_presence_v1.md (v1.1, incl. settings menu)
// =====================================================================
#pragma once
#include "shared.h"

enum PresenceState : uint8_t { P_UNOCCUPIED, P_WORKING, P_EYE_ALERT, P_STAND_ALERT,
                               P_AWAY_PENDING, P_SENSOR_FAULT };
enum UiPage : uint8_t { UI_STATUS, UI_LUX_SETTING, UI_LATE_SETTING };

struct PresenceView {
  PresenceState state;
  UiPage   ui_page;
  uint8_t  snooze_count;
  bool     snooze_pending;
  bool     snooze_rejected;     // show "max 2 snoozes" for a moment
  uint32_t away_s;              // system seconds away (for display/log)
};

void presence_init();
void presence_task(void*);      // 100 ms  - read sensor, stability, fault, occupancy transitions
void break_timer_task(void*);   // 1000 ms - sit time, 20/30 min alerts, 3/5 min away timers
void button_task(void*);        // 100 ms + ISR queue - short/long press, settings menu
PresenceView presence_view();
void presence_ui_reset();       // called by display when a new alert appears
const char* presence_state_name(PresenceState s);
