// =====================================================================
//  Smart Workstation Environmental Regulator  -  ROOM NODE
//  010123119 Embedded System Design, KMUTNB
//
//  Board : ESP32-DevKitC V4 (ESP32-WROOM-32E)          A2
//  Data  : 02_interface/A1_interface.md v1.0           A1
//  Comms : 04_logic/A3_communication.md v1.0           A3
//  Module owners: air.cpp = C, comm.cpp = A
//
//  Serial commands (115200): help, stats, status, wifi off|on,
//    fault scd41, ok scd41, co2 <ppm>, co2 pot, desks <online> <occupied>, desks auto
// =====================================================================
#include <Wire.h>
#include "config.h"
#include "shared.h"
#include "air.h"
#include "comm.h"

RoomShared        g_shared;
SemaphoreHandle_t g_dataMutex;
SemaphoreHandle_t g_i2cLock;
TaskHandle_t      g_commTask = nullptr;
TestHooks         g_test;
TaskStat g_stats[TID_COUNT] = { {"air_sensor_task", T_AIR_MS}, {"room_communication_task", T_AIR_MS} };
static SemaphoreHandle_t s_logMutex;

RoomShared shared_snapshot() {
  xSemaphoreTake(g_dataMutex, portMAX_DELAY);
  RoomShared c = g_shared;
  xSemaphoreGive(g_dataMutex);
  return c;
}

void stat_record(TaskId id, uint32_t us) {
  TaskStat& st = g_stats[id];
  st.runs++; st.total_us += us; if (us > st.wcet_us) st.wcet_us = us;
}

void logf(const char* fmt, ...) {
  char buf[160];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  xSemaphoreTake(s_logMutex, portMAX_DELAY);
  Serial.printf("[%9.3f] %s\n", millis() / 1000.0f, buf);
  xSemaphoreGive(s_logMutex);
}

static void print_stats() {
  Serial.println("---- task statistics (measured) ----");
  Serial.println("task                     T(ms)   runs   WCET(us)  avg(us)  U=C/T     late  stack_free(B)");
  double U = 0;
  for (int i = 0; i < TID_COUNT; i++) {
    TaskStat& s = g_stats[i];
    double u = (s.wcet_us / 1000.0) / s.period_ms; U += u;
    Serial.printf("%-24s %6lu %6lu %9lu %8lu  %.6f %5lu  %lu\n", s.name, (unsigned long)s.period_ms, (unsigned long)s.runs,
      (unsigned long)s.wcet_us, (unsigned long)(s.runs ? s.total_us / s.runs : 0), u, (unsigned long)s.late,
      s.handle ? (unsigned long)uxTaskGetStackHighWaterMark(s.handle) : 0UL);
  }
  Serial.printf("Total U (WCET-based) = %.4f%%\nheap free=%lu  min_free=%lu\n", U * 100,
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap());
}

static void print_status() {
  RoomShared s = shared_snapshot();
  Serial.printf("C: state=%s co2=%u lvl=%u t=%.1f rh=%u tf=%u ok=%d fan=%u%%\n", air_state_name(air_state()),
    s.air.co2_ppm, s.air.co2_level, s.air.temp_c_x10 / 10.0f, s.air.rh_pct, s.air.temp_flag, s.air.sensor_ok, s.air.fan_pct);
  Serial.printf("A: room_desks_online=%u room_occupied_count=%u%s\n", s.room_desks_online, s.room_occupied_count,
                g_test.desks_override ? " (overridden for C unit tests)" : "");
  comm_print_status();
}

static void console_task(void*) {
  String line;
  for (;;) {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c != '\n') { line += c; continue; }
      line.trim(); line.toLowerCase();
      if (line == "help") Serial.println("cmds: stats | status | wifi off|on | fault scd41 | ok scd41 | co2 <ppm> | co2 pot | temp <C> | temp auto | desks <online> <occupied> | desks auto");
      else if (line == "stats") print_stats();
      else if (line == "status") print_status();
      else if (line == "wifi off") g_test.wifi_off = true;
      else if (line == "wifi on") { g_test.wifi_off = false; logf("TEST: Wi-Fi allowed"); }
      else if (line == "fault scd41") { g_test.fault_scd41 = true;  logf("TEST: SCD41 read error"); }
      else if (line == "ok scd41")    { g_test.fault_scd41 = false; logf("TEST: SCD41 restored"); }
      else if (line == "temp auto")   { g_test.temp_override = false; logf("TEST: temperature from DHT22"); }
      else if (line.startsWith("temp ")) { g_test.temp_c = line.substring(5).toFloat(); g_test.temp_override = true; logf("TEST: temperature fixed at %.1f C", g_test.temp_c); }
      else if (line == "co2 pot")     { g_test.co2_override = false; logf("TEST: CO2 from potentiometer"); }
      else if (line.startsWith("co2 ")) {
        int v = line.substring(4).toInt();
        if (v >= 0 && v <= 20000) { g_test.co2_ppm = v; g_test.co2_override = true; logf("TEST: CO2 fixed at %d ppm (next reading)", v); }
      }
      else if (line == "desks auto")  { g_test.desks_override = false; logf("TEST: desk counts from MQTT"); }
      else if (line.startsWith("desks ")) {
        int sp = line.indexOf(' ', 6);
        if (sp > 0) {
          g_test.ov_online = line.substring(6, sp).toInt(); g_test.ov_occupied = line.substring(sp + 1).toInt();
          g_test.desks_override = true;
          logf("TEST: desks online=%u occupied=%u", g_test.ov_online, g_test.ov_occupied);
        }
      } else if (line.length()) Serial.println("unknown command, type help");
      line = "";
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  // Drive every actuator pin LOW first (fan must not spin at boot; logic analyzer showed
  // ~1.6 s HIGH on GPIO25 before LEDC was configured).
  for (uint8_t p : {PIN_FAN, PIN_RGB_R, PIN_RGB_G, PIN_RGB_B}) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== SWER room node  (SIMULATION=%d) ===\n", SIMULATION);
  s_logMutex  = xSemaphoreCreateMutex();
  g_dataMutex = xSemaphoreCreateMutex();
  g_i2cLock   = xSemaphoreCreateMutex();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);   // SCD41: 100 kHz standard mode
  air_init();
  comm_init();
  xTaskCreatePinnedToCore(room_communication_task, "comm", 8192, nullptr, PRIO_COMM, &g_commTask, NET_CORE);
  g_stats[TID_COMM].handle = g_commTask;
  xTaskCreatePinnedToCore(air_sensor_task,   "air",     4096, nullptr, PRIO_AIR,   &g_stats[TID_AIR].handle, APP_CORE);
  xTaskCreatePinnedToCore(status_blink_task, "blink",   2048, nullptr, PRIO_BLINK, nullptr, APP_CORE);
  xTaskCreatePinnedToCore(console_task,      "console", 4096, nullptr, PRIO_CONSOLE, nullptr, APP_CORE);
  logf("tasks started; type 'help' for test commands");
}

void loop() { vTaskDelete(nullptr); }
