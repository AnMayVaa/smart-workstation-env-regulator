// =====================================================================
//  display.cpp  -  Module B (owner: B)  implemented by A from B2 v1.1
//   * Consumes alert_request queue (all sources), picks priority 2 > 1 > 0,
//     oldest first on ties (B2 section 7).
//   * Air alerts are derived here from air_status (A1 v1.0 section 6, Q2):
//       co2_level == 2   -> ALERT_CO2_HIGH      (priority 1)
//       temp_flag != 0   -> ALERT_TEMP          (priority 1)
//       sensor_ok false  -> ALERT_SENSOR_FAULT  (priority 2, SRC_AIR)
//     (priorities as in B2 v1.2, branch B_presence_tasks 183f379)
//   * Presence alerts (eye/stand) and priority-2 alerts stay full-screen until cleared.
//     Other alerts (air, link, light sensor) are full-screen for 10 s, then shown as a
//     one-line banner on the status page so sit time stays visible (design choice of A,
//     B to confirm).
//   * OLED text is English: the default Adafruit GFX font has no Thai glyphs.
// =====================================================================
#include "display.h"
#include "presence.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);
static bool s_oledOk = false;

struct ActiveAlert { bool used; AlertSource src; AlertType type; uint8_t prio; uint32_t since; };
static ActiveAlert s_active[10];
static int s_shownIdx = -1;
static AlertSource s_shownSrc; static AlertType s_shownType;
static uint32_t s_lastRepeatBeepMs = 0;
static uint32_t s_displayErrors = 0;
static esp_timer_handle_t s_beepTimer;

static void beep_off(void*) { pwm_write(PIN_BUZZER, 0); }
static void beep(uint32_t ms) {                // never blocks the task
  pwm_tone(PIN_BUZZER, BUZZER_FREQ);
  esp_timer_stop(s_beepTimer);
  esp_timer_start_once(s_beepTimer, (uint64_t)ms * 1000ULL);
}

bool display_init() {
  pwm_attach(PIN_BUZZER, BUZZER_FREQ, BUZZER_BITS);
  pwm_write(PIN_BUZZER, 0);
  esp_timer_create_args_t a = {}; a.callback = beep_off; a.name = "beep";
  esp_timer_create(&a, &s_beepTimer);
  xSemaphoreTake(g_i2cLock, portMAX_DELAY);
  s_oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (s_oledOk) { oled.clearDisplay(); oled.display(); }
  xSemaphoreGive(g_i2cLock);
  return s_oledOk;
}

static void set_alert(AlertSource src, AlertType t, uint8_t prio, AlertAction act) {
  int freeIdx = -1;
  for (int i = 0; i < 10; i++) {
    if (s_active[i].used && s_active[i].src == src && s_active[i].type == t) {
      if (act == ALERT_CLEAR) s_active[i].used = false;
      else s_active[i].prio = prio;
      return;
    }
    if (!s_active[i].used && freeIdx < 0) freeIdx = i;
  }
  if (act == ALERT_SHOW && freeIdx >= 0) {
    s_active[freeIdx] = { true, src, t, prio, millis() };
    presence_ui_reset();     // B2: a new alert closes the settings menu
  }
}

static int choose_alert() {
  int best = -1;
  for (int i = 0; i < 10; i++) {
    if (!s_active[i].used) continue;
    if (best < 0 || s_active[i].prio > s_active[best].prio ||
        (s_active[i].prio == s_active[best].prio && s_active[i].since < s_active[best].since)) best = i;
  }
  return best;
}

static const char* alert_title(AlertType t) {
  switch (t) {
    case ALERT_EYE_BREAK:    return "EYE BREAK";
    case ALERT_STAND_BREAK:  return "STAND UP";
    case ALERT_CO2_HIGH:     return "CO2 HIGH";
    case ALERT_TEMP:         return "ROOM TEMP";
    case ALERT_SENSOR_FAULT: return "SENSOR FAULT";
    case ALERT_LINK_LOST:    return "LINK LOST";
  }
  return "ALERT";
}

static void render_alert(const ActiveAlert& a, const DeskShared& s, const PresenceView& v) {
  // size-2 font is 12 px per char: titles longer than 10 chars would wrap on 128 px
  const char* title = a.type == ALERT_SENSOR_FAULT ? "SENSOR ERR" : alert_title(a.type);
  oled.setTextSize(2); oled.setCursor(0, 0); oled.println(title);
  oled.setTextSize(1); oled.setCursor(0, 20);
  switch (a.type) {
    case ALERT_EYE_BREAK:   oled.println("Look 6 m away"); oled.println("for 20 seconds"); break;
    case ALERT_STAND_BREAK: oled.println("Stand up and walk"); oled.println("for 3 minutes"); break;
    case ALERT_CO2_HIGH:    oled.printf("CO2 %u ppm\n", s.air.co2_ppm); oled.printf("Fan %u%% venting\n", s.air.fan_pct); break;
    case ALERT_TEMP:        oled.printf("Room %.1f C\n", s.air.temp_c_x10 / 10.0f);
                            oled.println(s.air.temp_flag == 1 ? "Too cold (<20 C)" : "Too warm (>26 C)"); break;
    case ALERT_SENSOR_FAULT:oled.println(a.src == SRC_PRESENCE ? "Presence sensor" :
                                         a.src == SRC_LIGHT ? "Light sensor" : "Air sensor");
                            oled.println("Safe mode active"); break;
    case ALERT_LINK_LOST:   oled.println("Room node offline"); oled.println("Desk keeps working"); break;
  }
  if (a.src == SRC_PRESENCE && (a.type == ALERT_EYE_BREAK || a.type == ALERT_STAND_BREAK)) {
    oled.setCursor(0, 46); oled.println("short=OK");
    if (v.snooze_rejected) oled.println("max 2 snoozes");
    else oled.printf("long=snooze (%u/%u)\n", v.snooze_count, MAX_SNOOZE);
  }
}

static void render_status(const DeskShared& s, const PresenceView& v, const ActiveAlert* banner) {
  oled.setTextSize(1); oled.setCursor(0, 0);
  if (s.time_valid) oled.printf("Desk %d  %02u:%02u", DESK_ID, s.time_of_day.hour, s.time_of_day.minute);
  else              oled.printf("Desk %d  --:--", DESK_ID);
  oled.setCursor(98, 0); oled.print(s.link_ok ? "LINK" : "L:--");
  oled.setCursor(0, 11);
  if (s.occupied) oled.printf("Sitting %u min", s.sit_minutes);
  else            oled.printf("Away %lu min", (unsigned long)(v.away_s / 60));
  if (v.snooze_pending) oled.print(" z");
  oled.setCursor(0, 22);
  if (s.air_ever_rx && s.air.age_s <= STALE_MS / 1000 && s.air.sensor_ok) {
    static const char* lvl[] = {"GOOD", "MODERATE", "POOR"};
    oled.printf("CO2 %u %s", s.air.co2_ppm, lvl[s.air.co2_level > 2 ? 2 : s.air.co2_level]);
    oled.setCursor(0, 33);
    oled.printf("%.1fC %u%%RH fan%u%%", s.air.temp_c_x10 / 10.0f, s.air.rh_pct, s.air.fan_pct);
  } else {
    oled.print("No air data");
  }
  oled.setCursor(0, 44);
  oled.printf("Light %ulx tgt %u", s.lux_now, s.user_lux_target);
  oled.setCursor(0, 55);
  if (banner) oled.printf("! %s", alert_title(banner->type));
  else oled.printf("Late:%s  hold=menu", s.late_mode ? "ON" : "OFF");
}

static void render_settings(const DeskShared& s, const PresenceView& v) {
  oled.setTextSize(1); oled.setCursor(0, 0);
  if (v.ui_page == UI_LUX_SETTING) {
    oled.println("SET BRIGHTNESS");
    oled.setTextSize(2); oled.setCursor(0, 18); oled.printf("%u lux", s.user_lux_target);
    oled.setTextSize(1); oled.setCursor(0, 44); oled.println("short: +100");
    oled.println("long : next");
  } else {
    oled.println("LATE WORK MODE");
    oled.setTextSize(2); oled.setCursor(0, 18); oled.print(s.late_mode ? "ON" : "OFF");
    oled.setTextSize(1); oled.setCursor(0, 44); oled.println("short: toggle");
    oled.println("long : save & exit");
  }
}

void display_alert_task(void*) {
  uint8_t lastCo2Level = 0, lastTempFlag = 0; bool lastAirFault = false;
  for (;;) {
    AlertRequest r;
    // wake every 500 ms or as soon as an alert arrives
    bool got = xQueueReceive(g_alertQ, &r, pdMS_TO_TICKS(T_DISPLAY_MS)) == pdTRUE;
    STAT_BEGIN();
    while (got) {
      set_alert(r.source, r.type, r.priority, r.action);
      got = xQueueReceive(g_alertQ, &r, 0) == pdTRUE;
    }
    DeskShared s = shared_snapshot();
    PresenceView v = presence_view();

    // ---- derive air alerts from air_status (A1 v1.0 Q2) ----
    bool airFresh = s.air_ever_rx && s.air.age_s <= STALE_MS / 1000;
    uint8_t co2L = airFresh && s.air.sensor_ok ? s.air.co2_level : 0;
    uint8_t tf   = airFresh && s.air.sensor_ok ? s.air.temp_flag : 0;
    bool airFault = airFresh && !s.air.sensor_ok;
    if ((co2L == 2) != (lastCo2Level == 2)) set_alert(SRC_AIR, ALERT_CO2_HIGH, 1, co2L == 2 ? ALERT_SHOW : ALERT_CLEAR);
    if ((tf != 0) != (lastTempFlag != 0))   set_alert(SRC_AIR, ALERT_TEMP, 1, tf ? ALERT_SHOW : ALERT_CLEAR);
    if (airFault != lastAirFault)           set_alert(SRC_AIR, ALERT_SENSOR_FAULT, 2, airFault ? ALERT_SHOW : ALERT_CLEAR);
    lastCo2Level = co2L; lastTempFlag = tf; lastAirFault = airFault;

    // ---- choose alert and drive buzzer (NFR-14: screen + sound for prio >= 1) ----
    int idx = choose_alert();
    bool newlyShown = idx >= 0 && (s_shownIdx < 0 || s_active[idx].src != s_shownSrc || s_active[idx].type != s_shownType);
    if (idx >= 0) {
      const ActiveAlert& a = s_active[idx];
      if (newlyShown && a.prio >= 1) { beep(150); s_lastRepeatBeepMs = millis(); }
      else if (a.prio == 2 && millis() - s_lastRepeatBeepMs >= 2000) { beep(150); s_lastRepeatBeepMs = millis(); }
      if (newlyShown) logf("B: display alert %s prio %u", alert_title(a.type), a.prio);
      s_shownSrc = a.src; s_shownType = a.type;
    }
    s_shownIdx = idx;

    // ---- render into RAM buffer, then one I2C transaction under i2c_lock ----
    if (s_oledOk) {
      oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE);
      bool fullScreen = false;
      if (idx >= 0) {
        const ActiveAlert& a = s_active[idx];
        fullScreen = a.src == SRC_PRESENCE || a.prio >= 2 || millis() - a.since < 10000;
      }
      if (fullScreen) render_alert(s_active[idx], s, v);
      else if (v.ui_page != UI_STATUS) render_settings(s, v);
      else render_status(s, v, idx >= 0 ? &s_active[idx] : nullptr);
      if (xSemaphoreTake(g_i2cLock, pdMS_TO_TICKS(I2C_LOCK_WAIT_MS)) == pdTRUE) {
        oled.display();
        xSemaphoreGive(g_i2cLock);
      } else {
        s_displayErrors++;     // skip this frame (B2 section 7)
      }
    }
    STAT_END(TID_DISPLAY);
  }
}
