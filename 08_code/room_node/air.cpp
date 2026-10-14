// =====================================================================
//  air.cpp  -  Module C (owner: C)  implemented by A from C2 v1
//  Adaptations required by A1 v1.0 (see issue #6):
//   * Field names co2_ppm / temp_c_x10 / rh_pct / co2_level / temp_flag /
//     sensor_ok / fan_pct (A1 section 5).
//   * No alert_request is sent from the room node; desk nodes derive alerts
//     from air_status (A1 Q2). temp_flag is always computed (FR-17).
//   * ROOM_EMPTY only when room_desks_online > 0 AND room_occupied_count == 0;
//     if no desk is reachable the fan keeps following CO2 (A1 section 4, NFR-06).
//   C2 (v2, merged in PR #11) states, thresholds and hysteresis:
//     >= 1000 ppm -> POOR (fan 100%); POOR stays until < 950 ppm, then
//     MODERATE with fan 60%; < 800 ppm -> GOOD (fan off); fault -> fan 80%.
// =====================================================================
#include "air.h"
#include "comm.h"
#include "config.h"
#include <Wire.h>
#if SIMULATION
#include <DHT.h>
static DHT dht(PIN_DHT, DHT22);
#endif

static AirState s_state = S0_ROOM_EMPTY;
static bool s_fanWasActive = false;
static uint8_t s_errCount = 0, s_okCount = 0;
static uint16_t s_co2 = 0; static float s_temp = 0, s_rh = 0;

const char* air_state_name(AirState s) {
  switch (s) { case S0_ROOM_EMPTY: return "S0_ROOM_EMPTY"; case S1_AIR_GOOD: return "S1_AIR_GOOD";
               case S2_AIR_MODERATE: return "S2_AIR_MODERATE"; case S3_AIR_POOR: return "S3_AIR_POOR";
               case S4_SENSOR_FAULT: return "S4_SENSOR_FAULT"; }
  return "?";
}
AirState air_state() { return s_state; }

static void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  pwm_write(PIN_RGB_R, r); pwm_write(PIN_RGB_G, g); pwm_write(PIN_RGB_B, b);
}
static void set_fan(uint8_t pct) { pwm_write(PIN_FAN, (uint32_t)pct * FAN_PWM_MAX / 100); }

void air_init() {
  pwm_attach(PIN_FAN, FAN_PWM_FREQ, FAN_PWM_BITS);
  pwm_attach(PIN_RGB_R, RGB_PWM_FREQ, RGB_PWM_BITS);
  pwm_attach(PIN_RGB_G, RGB_PWM_FREQ, RGB_PWM_BITS);
  pwm_attach(PIN_RGB_B, RGB_PWM_FREQ, RGB_PWM_BITS);
  set_fan(0); set_rgb(0, 0, 0);
#if SIMULATION
  analogReadResolution(12);
  dht.begin();
#else
  // SCD41: start_periodic_measurement (0x21B1) - one result every 5 s
  xSemaphoreTake(g_i2cLock, portMAX_DELAY);
  Wire.beginTransmission(SCD41_ADDR); Wire.write(0x21); Wire.write(0xB1); Wire.endTransmission();
  xSemaphoreGive(g_i2cLock);
#endif
}

#if !SIMULATION
static uint8_t scd_crc(const uint8_t* d) {          // Sensirion CRC-8, poly 0x31, init 0xFF
  uint8_t crc = 0xFF;
  for (int i = 0; i < 2; i++) { crc ^= d[i]; for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1; }
  return crc;
}
#endif

// Read CO2 / T / RH. Real: SCD41 read_measurement (0xEC05). Sim: pot + DHT22.
static bool read_air(uint16_t* co2, float* t, float* rh) {
  if (g_test.fault_scd41) return false;
#if SIMULATION
  if (xSemaphoreTake(g_i2cLock, pdMS_TO_TICKS(I2C_LOCK_WAIT_MS)) != pdTRUE) return false;
  int raw = analogRead(PIN_CO2_ADC);
  xSemaphoreGive(g_i2cLock);
  *co2 = g_test.co2_override ? g_test.co2_ppm
                             : SIM_CO2_MIN + (uint32_t)raw * (SIM_CO2_MAX - SIM_CO2_MIN) / 4095;
  *t = dht.readTemperature(); *rh = dht.readHumidity();
  if (g_test.temp_override) *t = g_test.temp_c;
  return !isnan(*t) && !isnan(*rh);
#else
  if (xSemaphoreTake(g_i2cLock, pdMS_TO_TICKS(I2C_LOCK_WAIT_MS)) != pdTRUE) return false;
  Wire.beginTransmission(SCD41_ADDR); Wire.write(0xEC); Wire.write(0x05);
  bool ok = Wire.endTransmission() == 0;
  xSemaphoreGive(g_i2cLock);              // do not hold the lock during the 1 ms wait
  if (!ok) return false;
  vTaskDelay(pdMS_TO_TICKS(2));
  uint8_t b[9];
  if (xSemaphoreTake(g_i2cLock, pdMS_TO_TICKS(I2C_LOCK_WAIT_MS)) != pdTRUE) return false;
  ok = Wire.requestFrom(SCD41_ADDR, (uint8_t)9) == 9;
  for (int i = 0; ok && i < 9; i++) b[i] = Wire.read();
  xSemaphoreGive(g_i2cLock);
  if (!ok || scd_crc(b) != b[2] || scd_crc(b + 3) != b[5] || scd_crc(b + 6) != b[8]) return false;
  *co2 = (b[0] << 8) | b[1];
  *t  = -45.0f + 175.0f * (((b[3] << 8) | b[4]) / 65535.0f);
  *rh = 100.0f * (((b[6] << 8) | b[7]) / 65535.0f);
  return true;
#endif
}

void air_sensor_task(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    STAT_BEGIN();
    uint16_t co2 = 0; float t = 0, rh = 0;
    bool ok = read_air(&co2, &t, &rh);
    // ---- fault detection (C2 part 2, NFR-07) ----
    if (!ok || co2 < CO2_VALID_MIN || co2 > CO2_VALID_MAX || t < -10 || t > 60) { s_errCount++; s_okCount = 0; }
    else { s_errCount = 0; s_okCount++; s_co2 = co2; s_temp = t; s_rh = rh; }

    comm_update_counts();                         // fresh desk counts for this step (A3)
    RoomShared snap = shared_snapshot();
    uint8_t online = snap.room_desks_online, occ = snap.room_occupied_count;
    if (g_test.desks_override) { online = g_test.ov_online; occ = g_test.ov_occupied; }
    bool roomEmpty = online > 0 && occ == 0;      // A1 section 4

    // ---- FSM (C2 part 3) ----
    AirState prev = s_state, ns = s_state;
    if (s_errCount >= MAX_SENSOR_ERRORS || (s_state == S4_SENSOR_FAULT && s_okCount < RECOVER_READS)) {
      ns = S4_SENSOR_FAULT;
    } else if (roomEmpty) {
      ns = S0_ROOM_EMPTY; s_fanWasActive = false;
    } else if (s_co2 >= CO2_HIGH) {
      ns = S3_AIR_POOR; s_fanWasActive = true;
    } else if (s_co2 < CO2_LOW) {
      ns = S1_AIR_GOOD; s_fanWasActive = false;
    } else if (s_fanWasActive && s_co2 >= CO2_HYST_MID) {   // C2 v2: keep 100% until < 950 ppm
      ns = S3_AIR_POOR;
    } else {
      ns = S2_AIR_MODERATE;
    }
    s_state = ns;

    // ---- actuators (C2 part 4) ----
    uint8_t fan = 0, level = 0;
    switch (s_state) {
      case S0_ROOM_EMPTY:   fan = 0; level = 0; set_rgb(0, 0, 0); break;
      case S1_AIR_GOOD:     fan = 0; level = 0; set_rgb(0, 255, 0); break;
      case S2_AIR_MODERATE: fan = s_fanWasActive ? FAN_MODERATE_PCT : 0; level = 1; set_rgb(255, 160, 0); break;
      case S3_AIR_POOR:     fan = FAN_POOR_PCT; level = 2; set_rgb(255, 0, 0); break;
      case S4_SENSOR_FAULT: fan = FAN_FAULT_PCT; level = 2; break;   // blink task drives the red LED
    }
    set_fan(fan);
    uint8_t tflag = (s_state == S4_SENSOR_FAULT) ? 0 : (s_temp < TEMP_LOW_C ? 1 : (s_temp > TEMP_HIGH_C ? 2 : 0));

    SHARED_WRITE(
      g_shared.air.co2_ppm = s_co2; g_shared.air.temp_c_x10 = (int16_t)lroundf(s_temp * 10);
      g_shared.air.rh_pct = (uint8_t)constrain(lroundf(s_rh), 0L, 100L);
      g_shared.air.co2_level = level; g_shared.air.temp_flag = tflag;
      g_shared.air.sensor_ok = s_state != S4_SENSOR_FAULT; g_shared.air.fan_pct = fan;
    );
    if (prev != s_state)
      logf("C: %s -> %s co2=%u fan=%u%% desks online=%u occupied=%u", air_state_name(prev), air_state_name(s_state), s_co2, fan, online, occ);
    STAT_END(TID_AIR);
    if (g_commTask) xTaskNotifyGive(g_commTask);   // publish room/air right after each reading (A3)
    if (xTaskDelayUntil(&last, pdMS_TO_TICKS(T_AIR_MS)) == pdFALSE) g_stats[TID_AIR].late++;
  }
}

void status_blink_task(void*) {
  bool on = false;
  for (;;) {
    if (s_state == S4_SENSOR_FAULT) { on = !on; set_rgb(on ? 255 : 0, 0, 0); }   // NFR-14: not colour only
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
