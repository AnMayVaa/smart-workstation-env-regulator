// =====================================================================
//  shared.h  -  Shared data of the room node (A1 v1.0 section 4, 5)
// =====================================================================
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct AirStatus {                 // air_status_t (A1 section 5)
  uint16_t co2_ppm    = 0;
  int16_t  temp_c_x10 = 0;
  uint8_t  rh_pct     = 0;
  uint8_t  co2_level  = 0;
  uint8_t  temp_flag  = 0;
  bool     sensor_ok  = false;
  uint8_t  fan_pct    = 0;
};

struct RoomShared {
  AirStatus air;                   // producer: C
  uint8_t room_occupied_count = 0; // producer: A
  uint8_t room_desks_online   = 0; // producer: A
};

extern RoomShared        g_shared;
extern SemaphoreHandle_t g_dataMutex;
extern SemaphoreHandle_t g_i2cLock;
extern TaskHandle_t      g_commTask;   // notified after every air reading
RoomShared shared_snapshot();
#define SHARED_WRITE(stmt) do { xSemaphoreTake(g_dataMutex, portMAX_DELAY); stmt; xSemaphoreGive(g_dataMutex); } while (0)

enum TaskId { TID_AIR, TID_COMM, TID_COUNT };
struct TaskStat { const char* name; uint32_t period_ms; uint32_t runs; uint32_t wcet_us; uint64_t total_us; uint32_t late; TaskHandle_t handle; };
extern TaskStat g_stats[TID_COUNT];
#define STAT_BEGIN()   int64_t __t0 = esp_timer_get_time()
#define STAT_END(id)   stat_record(id, (uint32_t)(esp_timer_get_time() - __t0))
void stat_record(TaskId id, uint32_t us);

struct TestHooks {
  volatile bool fault_scd41 = false;
  volatile bool co2_override = false;       // "co2 <ppm>" sets an exact value (simulation)
  volatile uint16_t co2_ppm = 0;
  volatile bool temp_override = false;      // "temp <C>" (DHT22 has no automation control)
  volatile float temp_c = 24.0f;
  volatile bool wifi_off = false;
  volatile bool desks_override = false;     // "desks <online> <occupied>"
  volatile uint8_t ov_online = 0, ov_occupied = 0;
};
extern TestHooks g_test;
void logf(const char* fmt, ...);
