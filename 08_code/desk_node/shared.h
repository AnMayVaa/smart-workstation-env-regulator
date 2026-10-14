// =====================================================================
//  shared.h  -  Shared data of the desk node (A1 v1.0 section 3, 5, 6)
//  Every field has exactly one producer; other modules read snapshots.
// =====================================================================
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ---- air_status_t (A1 section 5) ----
struct AirStatus {
  uint16_t co2_ppm    = 0;
  int16_t  temp_c_x10 = 0;
  uint8_t  rh_pct     = 0;
  uint8_t  co2_level  = 0;     // 0 good / 1 moderate / 2 poor
  uint8_t  temp_flag  = 0;     // 0 ok / 1 < 20 C / 2 > 26 C
  bool     sensor_ok  = false;
  uint8_t  fan_pct    = 0;
  uint16_t age_s      = 0xFFFF; // computed on the desk side
};

// ---- alert_request_t (A1 section 6) ----
enum AlertSource : uint8_t { SRC_PRESENCE, SRC_AIR, SRC_LIGHT, SRC_COMM };
enum AlertType   : uint8_t { ALERT_EYE_BREAK, ALERT_STAND_BREAK, ALERT_CO2_HIGH,
                             ALERT_TEMP, ALERT_SENSOR_FAULT, ALERT_LINK_LOST };
enum AlertAction : uint8_t { ALERT_SHOW, ALERT_CLEAR };
struct AlertRequest {
  AlertSource source;
  AlertType   type;
  uint8_t     priority;       // 0 screen / 1 screen + short beep / 2 screen + repeated beeps
  AlertAction action;
};

struct TimeOfDay { uint8_t hour = 0; uint8_t minute = 0; };

// ---- Shared data (A1 section 3) ----
struct DeskShared {
  // producer: B
  bool     occupied        = false;
  uint16_t sit_minutes     = 0;
  bool     away_timeout    = false;
  uint16_t user_lux_target = 500;
  bool     late_mode       = false;
  // producer: A
  TimeOfDay time_of_day;
  bool     time_valid      = false;
  bool     link_ok         = false;
  AirStatus air;                       // copy of room_report (age_s computed on read)
  uint32_t air_rx_ms       = 0;
  bool     air_ever_rx     = false;
  // producer: D
  uint16_t lux_now         = 0;
  float    light_pct       = 0;        // for display / stats only
  float    light_r         = 0.5f;
};

extern DeskShared        g_shared;
extern SemaphoreHandle_t g_dataMutex;   // protects g_shared (short critical sections only)
extern SemaphoreHandle_t g_i2cLock;     // i2c_lock (A1 section 7)
extern QueueHandle_t     g_alertQ;      // alert_request queue -> B display task
extern TaskHandle_t      g_commTask;    // notified when occupied changes (A3)

// Snapshot helpers
DeskShared shared_snapshot();
#define SHARED_WRITE(stmt) do { xSemaphoreTake(g_dataMutex, portMAX_DELAY); stmt; xSemaphoreGive(g_dataMutex); } while (0)

void post_alert(AlertSource s, AlertType t, uint8_t prio, AlertAction a);

// ---- Task statistics for real-time evidence (SIMULATION_GUIDE section 6) ----
enum TaskId { TID_PRESENCE, TID_BUTTON, TID_LIGHT, TID_DISPLAY, TID_BREAK, TID_COMM, TID_COUNT };
struct TaskStat {
  const char* name; uint32_t period_ms;
  uint32_t runs; uint32_t wcet_us; uint64_t total_us; uint32_t late;
  TaskHandle_t handle;
};
extern TaskStat g_stats[TID_COUNT];
#define STAT_BEGIN()        int64_t __t0 = esp_timer_get_time()
#define STAT_END(id)        stat_record(id, (uint32_t)(esp_timer_get_time() - __t0))
void stat_record(TaskId id, uint32_t us);
void print_stats();

// ---- Test hooks (SIMULATION_GUIDE section 4.2) ----
struct TestHooks {
  volatile bool fault_ld2410 = false;
  volatile bool presence_override = false;  // "presence on|off" (automated tests: the slide switch has no automation control)
  volatile bool presence_value = false;
  volatile bool fault_bh1750 = false;
  volatile bool wifi_off     = false;
  volatile bool force_away   = false;
  volatile bool clock_override = false;
  volatile uint16_t clock_override_min = 0;   // minutes since 00:00
  volatile uint64_t clock_override_set_v = 0; // vmillis() when set
};
extern TestHooks g_test;

// Log with system time stamp: "[  12.345] ..."
void logf(const char* fmt, ...);
