// =====================================================================
//  Smart Workstation Environmental Regulator  -  DESK NODE
//  010123119 Embedded System Design, KMUTNB
//
//  Board : ESP32-DevKitC V4 (ESP32-WROOM-32E)          A2
//  Data  : 02_interface/A1_interface.md v1.0           A1
//  Comms : 04_logic/A3_communication.md v1.0           A3
//  Tasks : 05_tasks/B3_task_table.md (RMS priorities)  B3
//  Module owners: presence.cpp/display.cpp = B, light.cpp = D, comm.cpp = A
//
//  Serial commands (115200): help, stats, status, wifi off|on,
//    fault ld2410|bh1750, ok ld2410|bh1750, presence on|off|auto, away, time HH:MM, time auto
// =====================================================================
#include <Wire.h>
#include "config.h"
#include "shared.h"
#include "presence.h"
#include "display.h"
#include "light.h"
#include "comm.h"

DeskShared        g_shared;
SemaphoreHandle_t g_dataMutex;
SemaphoreHandle_t g_i2cLock;
QueueHandle_t     g_alertQ;
TaskHandle_t      g_commTask = nullptr;
TestHooks         g_test;
TaskStat g_stats[TID_COUNT] = {
  {"presence_task", T_PRESENCE_MS}, {"button_event_task", T_BUTTON_MS},
  {"light_control_task", T_LIGHT_MS}, {"display_alert_task", T_DISPLAY_MS},
  {"break_timer_task", T_BREAK_MS}, {"desk_communication_task", 1000},
};
static SemaphoreHandle_t s_logMutex;

DeskShared shared_snapshot() {
  xSemaphoreTake(g_dataMutex, portMAX_DELAY);
  DeskShared c = g_shared;
  xSemaphoreGive(g_dataMutex);
  return c;
}

void post_alert(AlertSource s, AlertType t, uint8_t prio, AlertAction a) {
  AlertRequest r{ s, t, prio, a };
  xQueueSend(g_alertQ, &r, 0);     // never block the caller; queue is sized for bursts
}

void stat_record(TaskId id, uint32_t us) {
  TaskStat& st = g_stats[id];
  st.runs++; st.total_us += us;
  if (us > st.wcet_us) st.wcet_us = us;
}

void logf(const char* fmt, ...) {
  char buf[160];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  xSemaphoreTake(s_logMutex, portMAX_DELAY);
  Serial.printf("[%9.3f] %s\n", millis() / 1000.0f, buf);
  xSemaphoreGive(s_logMutex);
}

void print_stats() {
  xSemaphoreTake(s_logMutex, portMAX_DELAY);
  Serial.println("---- task statistics (measured) ----");
  Serial.println("task                     T(ms)   runs   WCET(us)  avg(us)  U=C/T     late  stack_free(B)");
  double U = 0;
  for (int i = 0; i < TID_COUNT; i++) {
    TaskStat& s = g_stats[i];
    double u = s.period_ms ? (s.wcet_us / 1000.0) / s.period_ms : 0;
    U += u;
    Serial.printf("%-24s %6lu %6lu %9lu %8lu  %.6f %5lu  %lu\n", s.name, (unsigned long)s.period_ms,
      (unsigned long)s.runs, (unsigned long)s.wcet_us, (unsigned long)(s.runs ? s.total_us / s.runs : 0),
      u, (unsigned long)s.late, s.handle ? (unsigned long)uxTaskGetStackHighWaterMark(s.handle) : 0UL);
  }
  Serial.printf("Total U (WCET-based) = %.4f%%   RMS bound n=%d: %.2f%%\n", U * 100, TID_COUNT,
                (int)TID_COUNT * (pow(2.0, 1.0 / (int)TID_COUNT) - 1) * 100);
  Serial.printf("heap free=%lu  min_free=%lu  TIME_SCALE=%d\n", (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMinFreeHeap(), TIME_SCALE);
  xSemaphoreGive(s_logMutex);
}

static void print_status() {
  DeskShared s = shared_snapshot();
  PresenceView v = presence_view();
  Serial.printf("B: state=%s occupied=%d sit=%umin away_timeout=%d lux_target=%u late=%d snooze=%u\n",
    presence_state_name(v.state), s.occupied, s.sit_minutes, s.away_timeout, s.user_lux_target, s.late_mode, v.snooze_count);
  Serial.printf("D: state=%s lux_now=%u B=%.1f%% r=%.3f\n", light_state_name(light_state()), s.lux_now, s.light_pct, s.light_r);
  Serial.printf("A: time_valid=%d %02u:%02u link_ok=%d air: co2=%u lvl=%u t=%.1f rh=%u tf=%u ok=%d fan=%u age=%us\n",
    s.time_valid, s.time_of_day.hour, s.time_of_day.minute, s.link_ok, s.air.co2_ppm, s.air.co2_level,
    s.air.temp_c_x10 / 10.0f, s.air.rh_pct, s.air.temp_flag, s.air.sensor_ok, s.air.fan_pct, s.air.age_s);
  comm_print_status();
}

// Lowest-priority task: test commands (SIMULATION_GUIDE section 4.2)
static void console_task(void*) {
  String line;
  for (;;) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c != '\n') { line += c; continue; }
      line.trim(); line.toLowerCase();
      if (line == "help") Serial.println("cmds: stats | status | wifi off|on | presence on|off|auto | fault ld2410|bh1750 | ok ld2410|bh1750 | away | time HH:MM | time auto");
      else if (line == "stats") print_stats();
      else if (line == "status") print_status();
      else if (line == "wifi off") g_test.wifi_off = true;
      else if (line == "wifi on")  { g_test.wifi_off = false; logf("TEST: Wi-Fi allowed"); }
      else if (line == "fault ld2410") { g_test.fault_ld2410 = true;  logf("TEST: LD2410 frames stopped"); }
      else if (line == "ok ld2410")    { g_test.fault_ld2410 = false; logf("TEST: LD2410 frames restored"); }
      else if (line == "fault bh1750") { g_test.fault_bh1750 = true;  logf("TEST: BH1750 read error"); }
      else if (line == "ok bh1750")    { g_test.fault_bh1750 = false; logf("TEST: BH1750 restored"); }
      else if (line == "presence on")   { g_test.presence_value = true;  g_test.presence_override = true; logf("TEST: presence forced ON"); }
      else if (line == "presence off")  { g_test.presence_value = false; g_test.presence_override = true; logf("TEST: presence forced OFF"); }
      else if (line == "presence auto") { g_test.presence_override = false; logf("TEST: presence from switch"); }
      else if (line == "away") { g_test.force_away = true; logf("TEST: force away_timeout"); }
      else if (line == "time auto") { g_test.clock_override = false; logf("TEST: clock from NTP"); }
      else if (line.startsWith("time ") && line.length() >= 10) {
        int h = line.substring(5, 7).toInt(), m = line.substring(8, 10).toInt();
        g_test.clock_override_min = (h % 24) * 60 + (m % 60);
        g_test.clock_override_set_v = vmillis();
        g_test.clock_override = true;
        logf("TEST: clock set to %02d:%02d (advances x%d)", h, m, TIME_SCALE);
      } else if (line.length()) Serial.println("unknown command, type help");
      line = "";
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  // Drive every actuator pin LOW first: before LEDC is configured the pins float,
  // which showed up as ~1.5 s of HIGH on the logic analyzer (light/buzzer flash at boot).
  for (uint8_t p : {PIN_LED_CW, PIN_LED_WW, PIN_BUZZER}) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== SWER desk node %d  (SIMULATION=%d, TIME_SCALE=%d) ===\n", DESK_ID, SIMULATION, TIME_SCALE);
  s_logMutex  = xSemaphoreCreateMutex();
  g_dataMutex = xSemaphoreCreateMutex();
  g_i2cLock   = xSemaphoreCreateMutex();      // mutex: priority inheritance (A1 section 7)
  g_alertQ    = xQueueCreate(24, sizeof(AlertRequest));
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);

  presence_init();
  if (!display_init()) Serial.println("WARN: OLED not found");
  light_init();
  comm_init();

  // Control tasks on core 1 (RMS: shorter period = higher priority, B3)
  xTaskCreatePinnedToCore(presence_task,      "presence", 4096, nullptr, PRIO_PRESENCE, &g_stats[TID_PRESENCE].handle, APP_CORE);
  xTaskCreatePinnedToCore(button_task,        "button",   3072, nullptr, PRIO_BUTTON,   &g_stats[TID_BUTTON].handle,   APP_CORE);
  xTaskCreatePinnedToCore(light_control_task, "light",    4096, nullptr, PRIO_LIGHT,    &g_stats[TID_LIGHT].handle,    APP_CORE);
  xTaskCreatePinnedToCore(display_alert_task, "display",  4096, nullptr, PRIO_DISPLAY,  &g_stats[TID_DISPLAY].handle,  APP_CORE);
  xTaskCreatePinnedToCore(break_timer_task,   "break",    4096, nullptr, PRIO_BREAK,    &g_stats[TID_BREAK].handle,    APP_CORE);
  // Network tasks on core 0 with the Wi-Fi/TCP-IP stack (A-T note 2)
  xTaskCreatePinnedToCore(desk_communication_task, "comm", 8192, nullptr, PRIO_COMM, &g_commTask, NET_CORE);
  g_stats[TID_COMM].handle = g_commTask;
  xTaskCreatePinnedToCore(ntp_sync_task,      "ntp",      3072, nullptr, PRIO_CONSOLE, nullptr, NET_CORE);
  xTaskCreatePinnedToCore(console_task,       "console",  4096, nullptr, PRIO_CONSOLE, nullptr, APP_CORE);
  logf("tasks started; type 'help' for test commands");
}

void loop() { vTaskDelete(nullptr); }   // all work is done in FreeRTOS tasks
