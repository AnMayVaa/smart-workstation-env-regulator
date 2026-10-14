// =====================================================================
//  config.h  -  Desk node configuration
//  Smart Workstation Environmental Regulator (010123119)
//  Thresholds: 01_requirements/D1_requirements.md (v1)
//  Pins:       03_hardware/A2_board_power.md section 3
//  Interface:  02_interface/A1_interface.md (v1.0)
// =====================================================================
#pragma once
#include <Arduino.h>

// ---------------- Build options ----------------
#define SIMULATION      1     // 1 = Wokwi stand-ins (A2 section 7), 0 = real hardware
#define TIME_SCALE      60    // 1 = real time, 60 = 1 system minute per real second
                              // affects minute-scale timers only (B2, D3 colour, clock override)
#ifndef DESK_ID
#define DESK_ID         1     // 1..254, unique per desk (A1 section 8, NFR-13); build with -DDESK_ID=2 for a 2nd desk
#endif

// ---------------- Network (A3) ----------------
#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASS       ""
#define WIFI_CHANNEL    6
#define MQTT_HOST       "broker.hivemq.com"
#define MQTT_PORT       1883
#define MQTT_KEEPALIVE_S (SIMULATION ? 60 : 15)  // sim can run below real time; a 15 s keepalive gets dropped by the broker
#define TOPIC_PREFIX    "swer-7k3q/r1/"      // team-unique prefix (A3 section 3)
#define NTP_TZ          "ICT-7"
#define NTP_SERVER_1    "time.navy.mi.th"
#define NTP_SERVER_2    "clock.nectec.or.th"
#define NTP_SERVER_3    "pool.ntp.org"
#define REPORT_IV_NORMAL_S 5
#define REPORT_IV_AWAY_S   15
#define STALE_MS        15000UL              // 3 x 5 s (A1 section 1)
#define SEQ_GAP_MAX     1000                 // seq jump >= this = sender restarted, not lost messages

// ---------------- Pins (A2 section 3) ----------------
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22
#define PIN_LD2410_RX   16    // ESP32 U2RXD  <- LD2410 TX (real hardware)
#define PIN_LD2410_TX   17    // ESP32 U2TXD  -> LD2410 RX (real hardware)
#define PIN_PRESENCE    13    // LD2410 OUT   (Wokwi: slide switch)
#define PIN_BUTTON      27    // B3F active-low, interrupt
#define PIN_BUZZER      26    // PS1240, LEDC 4 kHz
#define PIN_LED_CW      18    // AO3400A gate, cool white
#define PIN_LED_WW      19    // AO3400A gate, warm white
#define PIN_LIGHT_ADC   34    // Wokwi photoresistor AO (ADC1_CH6)

#define I2C_FREQ_HZ     400000   // OLED + BH1750 both support 400 kHz fast mode
#define OLED_ADDR       0x3C
#define BH1750_ADDR     0x23

// ---------------- PWM (D1 NFR-09, B1) ----------------
#define LED_PWM_FREQ    5000
#define LED_PWM_BITS    13
#define LED_PWM_MAX     8191
#define BUZZER_FREQ     4000
#define BUZZER_BITS     8

// ---------------- B2 presence / breaks (D1 section 6) ----------------
#define EYE_INTERVAL_MIN      20
#define STAND_INTERVAL_MIN    30
#define VALID_BREAK_MIN       3
#define AWAY_TIMEOUT_MIN      5
#define SNOOZE_MIN            5
#define MAX_SNOOZE            2
#define SENSOR_TIMEOUT_MS     500
#define PRESENCE_STABLE_MS    1000
#define BUTTON_DEBOUNCE_MS    30
#define LONG_PRESS_MS         1500
#define SETTINGS_TIMEOUT_MS   10000
#define LUX_DEFAULT           500
#define LUX_STEP              100
#define LUX_USER_MAX          1000

// ---------------- D3 light (D1 section 6) ----------------
#define LUX_DEADBAND      25
#define K_P               0.02f
#define STEP_MAX          2.0f
#define B_START_DEFAULT   50.0f
#define B_FAULT           60.0f
#define DIM_RATIO         0.5f
#define DIM_HOLD_MS       60000UL
#define R_DAY             0.80f
#define R_EVENING         0.00f
#define R_NEUTRAL         0.50f
#define R_LATE            0.80f
#define R_RATE_PER_MIN    0.01f       // NFR-10
#define FADE_ON_MS        2000
#define FADE_OFF_MS       5000
#define FADE_USER_MS      10000
#define LIGHT_FAULT_COUNT   3         // NFR-07
#define LIGHT_RECOVER_COUNT 2
#define SIM_LUX_PER_PCT   6.0f        // D2 section 5: full light adds ~600 lux

// ---------------- Air display on desk (A1 section 5-6) ----------------
#define AIR_TEMP_LOW_C    20.0f       // D1 FR-17 (computed by room node; shown here)
#define AIR_TEMP_HIGH_C   26.0f

// ---------------- RTOS (B3 RMS order, A-T) ----------------
//                        period(ms)   priority (higher = more urgent)
#define T_PRESENCE_MS     100
#define T_BUTTON_MS       100
#define T_LIGHT_MS        200
#define T_DISPLAY_MS      500
#define T_BREAK_MS        1000
#define T_COMM_POLL_MS    100
#define PRIO_PRESENCE     7
#define PRIO_BUTTON       6
#define PRIO_LIGHT        5
#define PRIO_DISPLAY      4
#define PRIO_BREAK        3
#define PRIO_COMM         2
#define PRIO_CONSOLE      1
#define APP_CORE          1           // control tasks (B, C, D)
#define NET_CORE          0           // Wi-Fi / TCP-IP / MQTT (A-T note 2)
#define I2C_LOCK_WAIT_MS  50          // A1 section 7

// Minute-scale "system time" used by B2/D3 timers so tests can be accelerated.
static inline uint64_t vmillis() { return (uint64_t)millis() * TIME_SCALE; }
#define MIN_MS(m) ((uint64_t)(m) * 60000ULL)

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
