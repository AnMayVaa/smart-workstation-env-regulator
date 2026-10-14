// =====================================================================
//  light.cpp  -  Module D (owner: D)  implemented by A from D3 v1
//   Part A: light state FSM (OFF / REGULATING / DIMMING / SENSOR_FAULT)
//           with proportional feedback on lux_now (K_P, deadband, step limit)
//   Part B: colour ratio r (cool-white share) by time of day / late mode
//   Notes:
//   * D3's reset_user_settings() is NOT done here: B owns user_lux_target and
//     late_mode and resets them on away_timeout (A1 v1.0 section 3, Q6).
//   * fade_B_to() is a software ramp evaluated each 200 ms tick (same effect as
//     LEDC hardware fade, but visible in Wokwi and in the log).
//   * Simulation: the photoresistor cannot see the simulated LEDs, so
//     lux_now = lux_from_slider + SIM_LUX_PER_PCT * B  (D2 section 5, D3 notes).
// =====================================================================
#include "light.h"
#include "config.h"
#include <Wire.h>

static LightState s_state = L_OFF;
static float s_B = 0, s_Bsaved = 0, s_Blast = B_START_DEFAULT;
static float s_r = R_NEUTRAL;
static int s_err = 0, s_ok = 0;
static uint32_t s_dimStartMs = 0;
// fade ramp
static bool s_fading = false; static float s_fadeFrom = 0, s_fadeTo = 0;
static uint32_t s_fadeStart = 0, s_fadeMs = 1;

const char* light_state_name(LightState s) {
  switch (s) { case L_OFF: return "OFF"; case L_REGULATING: return "REGULATING";
               case L_DIMMING: return "DIMMING"; case L_SENSOR_FAULT: return "SENSOR_FAULT"; }
  return "?";
}
LightState light_state() { return s_state; }

static void set_state(LightState n) {
  if (n != s_state) { logf("D: %s -> %s (B=%.1f%%)", light_state_name(s_state), light_state_name(n), s_B); s_state = n; }
}

static void fade_B_to(float target, uint32_t ms) {
  s_fading = true; s_fadeFrom = s_B; s_fadeTo = target; s_fadeStart = millis(); s_fadeMs = ms ? ms : 1;
}
static void fade_step() {
  if (!s_fading) return;
  float k = (float)(millis() - s_fadeStart) / s_fadeMs;
  if (k >= 1.0f) { s_B = s_fadeTo; s_fading = false; }
  else s_B = s_fadeFrom + (s_fadeTo - s_fadeFrom) * k;
}

void light_init() {
  pwm_attach(PIN_LED_CW, LED_PWM_FREQ, LED_PWM_BITS);   // NFR-09: 5 kHz, 13-bit
  pwm_attach(PIN_LED_WW, LED_PWM_FREQ, LED_PWM_BITS);
  pwm_write(PIN_LED_CW, 0); pwm_write(PIN_LED_WW, 0);
#if SIMULATION
  analogReadResolution(12);
#else
  // BH1750: continuous H-resolution mode (0x10)
  xSemaphoreTake(g_i2cLock, portMAX_DELAY);
  Wire.beginTransmission(BH1750_ADDR); Wire.write(0x10); Wire.endTransmission();
  xSemaphoreGive(g_i2cLock);
#endif
}

// Read the latest light value. Real: BH1750 2-byte result. Sim: photoresistor.
static bool read_lux(float* lux) {
  if (g_test.fault_bh1750) return false;
  if (xSemaphoreTake(g_i2cLock, pdMS_TO_TICKS(I2C_LOCK_WAIT_MS)) != pdTRUE) return false;  // A1 section 7
  bool ok;
#if SIMULATION
  // The lock is still taken so bus timing/contention matches the real design.
  int raw = analogRead(PIN_LIGHT_ADC);
  xSemaphoreGive(g_i2cLock);
  // Wokwi photoresistor module: 10k divider, GAMMA 0.7, RL10 50 kOhm (scaled for 3.3 V)
  float v = raw / 4095.0f * 3.3f;
  if (v >= 3.29f) v = 3.29f;
  float res = 10000.0f * v / (3.3f - v);   // 10k pull-down: R = 10k * V / (VCC - V)
  float ambient = powf(50.0f * 1e3f * powf(10.0f, 0.7f) / res, 1.0f / 0.7f);
  if (!isfinite(ambient) || ambient > 100000.0f) ambient = 100000.0f;
  *lux = ambient + SIM_LUX_PER_PCT * s_B;
  ok = true;
#else
  ok = Wire.requestFrom(BH1750_ADDR, (uint8_t)2) == 2;
  if (ok) { uint16_t v = (Wire.read() << 8) | Wire.read(); *lux = v / 1.2f; }
  xSemaphoreGive(g_i2cLock);
#endif
  return ok;
}

// D1 colour schedule (minutes since 00:00)
static float schedule(uint16_t m) {
  if (m >= 7 * 60 && m < 17 * 60) return R_DAY;
  if (m >= 17 * 60 && m < 19 * 60) return R_DAY * (1.0f - (m - 17 * 60) / 120.0f);
  return R_EVENING;
}

static void enter_dimming() {
  s_Bsaved = s_B;
  fade_B_to(s_B * DIM_RATIO, FADE_ON_MS);
  s_dimStartMs = millis();
  set_state(L_DIMMING);
}

void light_control_task(void*) {
  TickType_t last = xTaskGetTickCount();
  bool lastLate = false; uint32_t userFadeUntil = 0;
  for (;;) {
    STAT_BEGIN();
    DeskShared snap = shared_snapshot();
    float lux = 0;
    bool ok = read_lux(&lux);
    if (ok) { s_err = 0; s_ok++; } else { s_err++; s_ok = 0; }
    uint16_t target = snap.user_lux_target;                          // NFR-15 clamp
    if (target < LUX_DEFAULT) target = LUX_DEFAULT;
    if (target > LUX_USER_MAX) target = LUX_USER_MAX;

    fade_step();
    switch (s_state) {
      case L_OFF:
        if (snap.occupied) { fade_B_to(s_Blast, FADE_ON_MS); set_state(L_REGULATING); }
        break;
      case L_REGULATING:
        if (snap.away_timeout) enter_dimming();
        else if (s_err >= LIGHT_FAULT_COUNT) {
          fade_B_to(B_FAULT, FADE_ON_MS);
          post_alert(SRC_LIGHT, ALERT_SENSOR_FAULT, 1, ALERT_SHOW);
          set_state(L_SENSOR_FAULT);
        } else if (ok && !s_fading) {
          float error = (float)target - lux;
          if (fabsf(error) > LUX_DEADBAND) {
            float step = constrain(K_P * error, -STEP_MAX, STEP_MAX);
            s_B = constrain(s_B + step, 0.0f, 100.0f);
          }
          s_Blast = s_B;
        }
        break;
      case L_DIMMING:
        if (snap.occupied && !snap.away_timeout) { fade_B_to(s_Bsaved, FADE_ON_MS); set_state(L_REGULATING); }
        else if (millis() - s_dimStartMs >= DIM_HOLD_MS && !(s_fading && s_fadeTo == 0)) {
          fade_B_to(0, FADE_OFF_MS); set_state(L_OFF);
        }
        break;
      case L_SENSOR_FAULT:
        if (snap.away_timeout) enter_dimming();
        else if (s_ok >= LIGHT_RECOVER_COUNT) {
          post_alert(SRC_LIGHT, ALERT_SENSOR_FAULT, 1, ALERT_CLEAR);
          set_state(L_REGULATING);
        }
        break;
    }

    // ---- Part B: colour ratio r ----
    float rTarget;
    const float autoRate = R_RATE_PER_MIN * (T_LIGHT_MS * (float)TIME_SCALE / 60000.0f);  // NFR-10
    const float userRate = R_LATE / (FADE_USER_MS / (float)T_LIGHT_MS);                   // 10 s user fade
    if (snap.late_mode != lastLate) { lastLate = snap.late_mode; userFadeUntil = millis() + FADE_USER_MS + T_LIGHT_MS; }
    uint16_t nowMin = snap.time_of_day.hour * 60 + snap.time_of_day.minute;
    if (snap.late_mode) rTarget = R_LATE;
    else if (!snap.time_valid) rTarget = R_NEUTRAL;
    else rTarget = schedule(nowMin);
    float ratePerTick = (int32_t)(userFadeUntil - millis()) > 0 ? userRate : autoRate;
    if (s_r < rTarget) s_r = fminf(s_r + ratePerTick, rTarget);
    else if (s_r > rTarget) s_r = fmaxf(s_r - ratePerTick, rTarget);

    // ---- outputs: duty_CW = B*r, duty_WW = B*(1-r) ----
    uint32_t cw = (uint32_t)lroundf(s_B * s_r / 100.0f * LED_PWM_MAX);
    uint32_t ww = (uint32_t)lroundf(s_B * (1.0f - s_r) / 100.0f * LED_PWM_MAX);
    pwm_write(PIN_LED_CW, cw);
    pwm_write(PIN_LED_WW, ww);
    SHARED_WRITE(
      if (ok) g_shared.lux_now = (uint16_t)constrain(lux, 0.0f, 65535.0f);
      g_shared.light_pct = s_B; g_shared.light_r = s_r;
    );
    STAT_END(TID_LIGHT);
    if (xTaskDelayUntil(&last, pdMS_TO_TICKS(T_LIGHT_MS)) == pdFALSE) g_stats[TID_LIGHT].late++;
  }
}
