// =====================================================================
//  config.h  -  Room node configuration
//  Thresholds: 01_requirements/D1_requirements.md (v1)
//  Pins:       03_hardware/A2_board_power.md section 4
//  Interface:  02_interface/A1_interface.md (v1.0)
// =====================================================================
#pragma once
#include <Arduino.h>

#define SIMULATION      1     // 1 = potentiometer + DHT22 instead of SCD41 (A2 section 7)

// ---------------- Network (A3) ----------------
#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASS       ""
#define WIFI_CHANNEL    6
#define MQTT_HOST       "broker.hivemq.com"
#define MQTT_PORT       1883
#define MQTT_KEEPALIVE_S (SIMULATION ? 60 : 15)  // sim can run below real time; a 15 s keepalive gets dropped by the broker
#define SEQ_GAP_MAX     1000                 // seq jump >= this = sender restarted, not lost messages
#define TOPIC_PREFIX    "swer-7k3q/r1/"      // must match the desk nodes
#define MAX_DESKS       16

// ---------------- Pins (A2 section 4) ----------------
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22
#define PIN_FAN         25    // AO3400A gate (Wokwi: blue LED)
#define PIN_RGB_R       18
#define PIN_RGB_G       19
#define PIN_RGB_B       23
#define PIN_CO2_ADC     35    // Wokwi potentiometer (ADC1_CH7)
#define PIN_DHT         4     // Wokwi DHT22
#define SCD41_ADDR      0x62
#define I2C_FREQ_HZ     100000

// ---------------- PWM ----------------
#define FAN_PWM_FREQ    50     // period 20 ms (ch.13 p.95, C1)
#define FAN_PWM_BITS    12
#define FAN_PWM_MAX     4095
#define RGB_PWM_FREQ    5000
#define RGB_PWM_BITS    8

// ---------------- C2 / D1 thresholds ----------------
#define CO2_HIGH        1000   // open fan  (FR-13)
#define CO2_HYST_MID    950    // POOR -> MODERATE (fan 60%)  (C2)
#define CO2_LOW         800    // close fan (FR-13)
#define TEMP_LOW_C      20.0f  // FR-17
#define TEMP_HIGH_C     26.0f
#define CO2_VALID_MIN   300    // C2 sanity check
#define CO2_VALID_MAX   5000   // C2 v2 sanity check (SCD41 range, NFR-12)
#define MAX_SENSOR_ERRORS 3    // NFR-07
#define RECOVER_READS     2
#define FAN_POOR_PCT    100
#define FAN_MODERATE_PCT 60
#define FAN_FAULT_PCT   80
#define SIM_CO2_MIN     400    // potentiometer 0..4095 -> 400..2000 ppm (simulation only):
#define SIM_CO2_MAX     2000   // the 800-950 ppm hysteresis band is ~9% of the knob (was ~3% with 5000)

// ---------------- RTOS (B3 room table, A-T) ----------------
#define T_AIR_MS        5000
#define T_COMM_POLL_MS  100
#define PRIO_AIR        4      // air_sensor_task + fan decision (C-T)
#define PRIO_BLINK      3
#define PRIO_COMM       2
#define PRIO_CONSOLE    1
#define APP_CORE        1
#define NET_CORE        0
#define I2C_LOCK_WAIT_MS 50

// ---------------- PWM wrapper: Arduino-ESP32 core 2.x and 3.x ----------------
// Core 3.x drives LEDC by pin (ledcAttach/ledcWrite(pin)); core 2.x by channel.
#include <esp_arduino_version.h>
#if ESP_ARDUINO_VERSION_MAJOR >= 3
// ledcAttach() takes the lowest free channel, and channels 2n/2n+1 share one timer:
// a second pin with another frequency would silently retime the first one (fan 50 Hz
// vs RGB 5 kHz). Give every pin an even channel so each pin owns its timer.
// 'inline' (not static): one shared counter for all .cpp files.
inline void pwm_attach(uint8_t pin, uint32_t freq, uint8_t bits) {
  static uint8_t next = 0; ledcAttachChannel(pin, freq, bits, next); next += 2; }
static inline void pwm_write(uint8_t pin, uint32_t duty) { ledcWrite(pin, duty); }
static inline void pwm_tone(uint8_t pin, uint32_t freq) { ledcWriteTone(pin, freq); }
#else
// 'inline' (not static) so every .cpp shares one channel table; each pin gets its
// own channel pair because core 2.x channels 2n/2n+1 share one timer (frequency).
inline int8_t& __pwm_ch(uint8_t pin) { static int8_t map[40] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}; return map[pin]; }
inline void pwm_attach(uint8_t pin, uint32_t freq, uint8_t bits) {
  static uint8_t next = 0; uint8_t ch = next; next += 2; __pwm_ch(pin) = ch; ledcSetup(ch, freq, bits); ledcAttachPin(pin, ch); }
inline void pwm_write(uint8_t pin, uint32_t duty) { if (__pwm_ch(pin) >= 0) ledcWrite(__pwm_ch(pin), duty); }
inline void pwm_tone(uint8_t pin, uint32_t freq) { if (__pwm_ch(pin) >= 0) ledcWriteTone(__pwm_ch(pin), freq); }
#endif
