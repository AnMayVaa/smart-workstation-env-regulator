// =====================================================================
//  presence.cpp  -  Module B (owner: B)  implemented by A from B2 v1.1
//
//  Implementation notes for B (see 08_code/README.md "Notes for module owners"):
//   * Occupancy transitions (UNOCCUPIED->WORKING, WORKING->AWAY_PENDING,
//     AWAY_PENDING->WORKING) are evaluated in presence_task (100 ms) right after
//     the 1 s stability filter, not in break_timer_task (1 s). Same FSM as B2,
//     but `occupied` is published <= 100 ms after it becomes stable, so D can
//     meet NFR-03 (light starts within 1 s).
//   * Minute-scale timers use vmillis() so TIME_SCALE can accelerate tests.
// =====================================================================
#include "presence.h"
#include "config.h"

// Module lock: recursive mutex (not a spinlock) because logging/queue calls
// happen while the FSM is being updated. Lock order: s_lock -> g_dataMutex.
static SemaphoreHandle_t s_lock;
#define LOCK()   xSemaphoreTakeRecursive(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGiveRecursive(s_lock)

// ---- sensor / stability ----
static uint32_t s_lastValidMs   = 0;
static bool     s_candidate     = false;
static uint32_t s_candidateSince = 0;
static bool     s_stable        = false;   // stable_presence
static bool     s_stableKnown   = false;   // filter has settled at least once

// ---- FSM ----
static PresenceState s_state   = P_UNOCCUPIED;
static uint64_t s_sitV         = 0;         // sit_elapsed (system ms)
static uint64_t s_lastTickV    = 0;
static uint64_t s_nextEyeV     = MIN_MS(EYE_INTERVAL_MIN);
static uint64_t s_nextStandV   = MIN_MS(STAND_INTERVAL_MIN);
static uint64_t s_awayStartV   = 0;
static bool     s_awayCounting = true;      // UNOCCUPIED at boot counts as away
static uint8_t  s_snoozeCount  = 0;
static bool     s_snoozePending = false;
static AlertType s_snoozedAlert = ALERT_EYE_BREAK;
static uint64_t s_snoozeDueV   = 0;
static bool     s_snoozeRejected = false;
static uint32_t s_snoozeRejectedMs = 0;
static bool     s_faultAlertShown = false;

// ---- outputs mirrored to g_shared ----
static bool     s_occupied     = false;
static bool     s_awayTimeout  = false;
static uint16_t s_luxTarget    = LUX_DEFAULT;
static bool     s_lateMode     = false;

// ---- UI ----
static UiPage   s_page         = UI_STATUS;
static uint32_t s_lastBtnMs    = 0;

// ---- button ISR queue ----
struct BtnEdge { bool pressed; uint32_t ms; };
static QueueHandle_t s_btnQ;

const char* presence_state_name(PresenceState s) {
  switch (s) {
    case P_UNOCCUPIED: return "UNOCCUPIED";
    case P_WORKING: return "WORKING";
    case P_EYE_ALERT: return "EYE_ALERT";
    case P_STAND_ALERT: return "STAND_ALERT";
    case P_AWAY_PENDING: return "AWAY_PENDING";
    case P_SENSOR_FAULT: return "SENSOR_FAULT";
  }
  return "?";
}

// Publish B-owned values to the shared table; notify A when occupied changes.
static void publish_outputs(bool occupiedChanged) {
  uint16_t sitMin = (uint16_t)(s_sitV / 60000ULL);
  SHARED_WRITE(
    g_shared.occupied = s_occupied;
    g_shared.away_timeout = s_awayTimeout;
    g_shared.user_lux_target = s_luxTarget;
    g_shared.late_mode = s_lateMode;
    g_shared.sit_minutes = sitMin;
  );
  if (occupiedChanged && g_commTask) xTaskNotifyGive(g_commTask);
}

static void set_state(PresenceState ns) {
  if (ns != s_state) {
    logf("B: %s -> %s", presence_state_name(s_state), presence_state_name(ns));
    s_state = ns;
  }
}

static void reset_snooze_control() { s_snoozeCount = 0; s_snoozePending = false; }

static void start_new_session() {
  s_sitV = 0;
  s_nextEyeV = MIN_MS(EYE_INTERVAL_MIN);
  s_nextStandV = MIN_MS(STAND_INTERVAL_MIN);
  reset_snooze_control();
  s_awayCounting = false;
  s_lastTickV = vmillis();
}

static void clear_presence_alerts() {
  post_alert(SRC_PRESENCE, ALERT_EYE_BREAK, 1, ALERT_CLEAR);
  post_alert(SRC_PRESENCE, ALERT_STAND_BREAK, 1, ALERT_CLEAR);
}

// FR-11: B owns user settings and resets them when away_timeout becomes true.
static void set_away_timeout_true_once() {
  if (!s_awayTimeout) {
    s_awayTimeout = true;
    s_luxTarget = LUX_DEFAULT;
    s_lateMode = false;
    s_page = UI_STATUS;
    logf("B: away_timeout=1 (reset user_lux_target=500, late_mode=0)");
    publish_outputs(true);   // A changes report interval (A3 section 8)
  }
}

void presence_init() {
  s_lock = xSemaphoreCreateRecursiveMutex();
  pinMode(PIN_PRESENCE, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  s_btnQ = xQueueCreate(16, sizeof(BtnEdge));
  s_lastValidMs = millis();
  s_candidateSince = millis();
  s_awayStartV = vmillis();
  s_lastTickV = vmillis();
  publish_outputs(false);
}

// -------- read sensor without blocking --------
// Real hardware: parse the latest LD2410 UART frame (UART2, 256000 baud).
// Simulation:    level of PIN_PRESENCE (slide switch) = LD2410 OUT.
static bool read_presence_frame(bool* present) {
  if (g_test.fault_ld2410) return false;
#if SIMULATION
  if (g_test.presence_override) { *present = g_test.presence_value; return true; }
  *present = digitalRead(PIN_PRESENCE) == HIGH;
  return true;
#else
  // TODO(real hardware): LD2410 engineering-frame parser on Serial2.
  *present = digitalRead(PIN_PRESENCE) == HIGH;
  return true;
#endif
}

void presence_task(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    STAT_BEGIN();
    uint32_t now = millis();
    bool raw = false;
    bool valid = read_presence_frame(&raw);
    bool occChanged = false;

    LOCK();
    if (valid) {
      s_lastValidMs = now;
      if (raw != s_candidate) { s_candidate = raw; s_candidateSince = now; }
    }
    bool settled = valid && (now - s_candidateSince >= PRESENCE_STABLE_MS);
    if (settled) { s_stable = s_candidate; s_stableKnown = true; }

    // ---- SENSOR_FAULT: no valid frame for > 500 ms -> fail-safe occupied=true ----
    if (now - s_lastValidMs > SENSOR_TIMEOUT_MS) {
      if (s_state != P_SENSOR_FAULT) {
        set_state(P_SENSOR_FAULT);
        occChanged = !s_occupied;
        s_occupied = true; s_awayTimeout = false;
        s_faultAlertShown = true;
        UNLOCK();
        clear_presence_alerts();
        post_alert(SRC_PRESENCE, ALERT_SENSOR_FAULT, 2, ALERT_SHOW);
        publish_outputs(occChanged);
        LOCK();
      }
    } else if (s_state == P_SENSOR_FAULT && settled) {
      // recover after 1 s of stable valid frames
      if (s_stable) { start_new_session(); set_state(P_WORKING); s_occupied = true; }
      else { s_awayStartV = vmillis(); s_awayCounting = true; set_state(P_UNOCCUPIED);
             occChanged = s_occupied; s_occupied = false; }
      s_faultAlertShown = false;
      UNLOCK();
      post_alert(SRC_PRESENCE, ALERT_SENSOR_FAULT, 2, ALERT_CLEAR);
      publish_outputs(occChanged);
      LOCK();
    } else if (settled) {
      // ---- occupancy transitions (B2 table) ----
      switch (s_state) {
        case P_UNOCCUPIED:
          if (s_stable) {
            start_new_session();
            s_occupied = true; s_awayTimeout = false; occChanged = true;
            set_state(P_WORKING);
          }
          break;
        case P_WORKING: case P_EYE_ALERT: case P_STAND_ALERT:
          if (!s_stable) {
            bool wasAlert = (s_state != P_WORKING);
            reset_snooze_control();
            s_awayStartV = vmillis(); s_awayCounting = true;
            s_occupied = false; occChanged = true;
            set_state(P_AWAY_PENDING);
            if (wasAlert) { UNLOCK(); clear_presence_alerts(); LOCK(); }
          }
          break;
        case P_AWAY_PENDING:
          if (s_stable) {   // back before 3 min -> resume same session
            s_occupied = true; s_awayTimeout = false; s_awayCounting = false;
            s_lastTickV = vmillis();
            occChanged = true;
            set_state(P_WORKING);
          }
          break;
        default: break;
      }
      if (occChanged) { UNLOCK(); publish_outputs(true); LOCK(); }
    }
    UNLOCK();
    STAT_END(TID_PRESENCE);
    if (xTaskDelayUntil(&last, pdMS_TO_TICKS(T_PRESENCE_MS)) == pdFALSE) g_stats[TID_PRESENCE].late++;
  }
}

void break_timer_task(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    STAT_BEGIN();
    uint64_t vnow = vmillis();
    bool changed = false;
    LOCK();
    uint64_t delta = vnow - s_lastTickV;
    s_lastTickV = vnow;

    if (g_test.force_away) {   // test hook: fast-forward the away timer to 5 min
      g_test.force_away = false;
      if (s_state == P_UNOCCUPIED || s_state == P_AWAY_PENDING) {
        s_awayStartV = vnow - MIN_MS(AWAY_TIMEOUT_MIN); s_awayCounting = true;
      } else {
        logf("TEST: 'away' ignored - switch presence OFF first (state %s)", presence_state_name(s_state));
      }
    }

    switch (s_state) {
      case P_WORKING:
        s_sitV += delta; changed = true;
        if (s_snoozePending && vnow >= s_snoozeDueV) {
          s_snoozePending = false;
          AlertType t = s_snoozedAlert;
          if (t == ALERT_STAND_BREAK && s_sitV >= s_nextEyeV) s_nextEyeV += MIN_MS(EYE_INTERVAL_MIN);  // stand covers eye
          set_state(t == ALERT_STAND_BREAK ? P_STAND_ALERT : P_EYE_ALERT);
          s_page = UI_STATUS;
          UNLOCK(); post_alert(SRC_PRESENCE, t, 1, ALERT_SHOW); LOCK();
          logf("B: snoozed %s re-raised", t == ALERT_STAND_BREAK ? "STAND" : "EYE");
        } else if (s_sitV >= s_nextStandV) {
          reset_snooze_control();
          s_nextStandV += MIN_MS(STAND_INTERVAL_MIN);
          if (s_sitV >= s_nextEyeV) s_nextEyeV += MIN_MS(EYE_INTERVAL_MIN);  // stand covers eye
          set_state(P_STAND_ALERT); s_page = UI_STATUS;
          UNLOCK(); post_alert(SRC_PRESENCE, ALERT_STAND_BREAK, 1, ALERT_SHOW); LOCK();
        } else if (s_sitV >= s_nextEyeV && s_snoozePending && s_snoozedAlert == ALERT_STAND_BREAK) {
          s_nextEyeV += MIN_MS(EYE_INTERVAL_MIN);   // a snoozed STAND is about to re-raise: it covers this eye break
        } else if (s_sitV >= s_nextEyeV) {
          reset_snooze_control();
          s_nextEyeV += MIN_MS(EYE_INTERVAL_MIN);
          set_state(P_EYE_ALERT); s_page = UI_STATUS;
          UNLOCK(); post_alert(SRC_PRESENCE, ALERT_EYE_BREAK, 1, ALERT_SHOW); LOCK();
        }
        break;
      case P_EYE_ALERT:
        s_sitV += delta; changed = true;
        if (s_sitV >= s_nextStandV) {   // stand replaces eye
          reset_snooze_control();
          s_nextStandV += MIN_MS(STAND_INTERVAL_MIN);
          set_state(P_STAND_ALERT);
          UNLOCK();
          post_alert(SRC_PRESENCE, ALERT_EYE_BREAK, 1, ALERT_CLEAR);
          post_alert(SRC_PRESENCE, ALERT_STAND_BREAK, 1, ALERT_SHOW);
          LOCK();
        }
        break;
      case P_STAND_ALERT:
        s_sitV += delta; changed = true;
        break;
      case P_AWAY_PENDING:
        if (vnow - s_awayStartV >= MIN_MS(VALID_BREAK_MIN)) {
          // a valid 3-minute break: reset session and due times
          s_sitV = 0; s_nextEyeV = MIN_MS(EYE_INTERVAL_MIN); s_nextStandV = MIN_MS(STAND_INTERVAL_MIN);
          reset_snooze_control();
          set_state(P_UNOCCUPIED); changed = true;
        }
        break;
      default: break;
    }
    if ((s_state == P_UNOCCUPIED || s_state == P_AWAY_PENDING) && s_awayCounting &&
        vnow - s_awayStartV >= MIN_MS(AWAY_TIMEOUT_MIN)) {
      UNLOCK(); set_away_timeout_true_once(); LOCK();
    }
    if (s_snoozeRejected && millis() - s_snoozeRejectedMs > 3000) s_snoozeRejected = false;
    UNLOCK();
    if (changed) publish_outputs(false);
    STAT_END(TID_BREAK);
    if (xTaskDelayUntil(&last, pdMS_TO_TICKS(T_BREAK_MS)) == pdFALSE) g_stats[TID_BREAK].late++;
  }
}

// -------- button: ISR only queues the edge (B1: ISR must not draw or delay) --------
static void IRAM_ATTR button_isr() {
  BtnEdge e{ digitalRead(PIN_BUTTON) == LOW, (uint32_t)millis() };
  BaseType_t hp = pdFALSE;
  xQueueSendFromISR(s_btnQ, &e, &hp);
  if (hp) portYIELD_FROM_ISR();
}

static void on_press(bool longPress) {
  bool outChanged = false;
  AlertType clearType = ALERT_EYE_BREAK; bool doClear = false;
  LOCK();
  s_lastBtnMs = millis();
  if (s_state == P_EYE_ALERT || s_state == P_STAND_ALERT) {
    AlertType cur = (s_state == P_STAND_ALERT) ? ALERT_STAND_BREAK : ALERT_EYE_BREAK;
    if (!longPress) {                       // acknowledge (does not reset sit time)
      reset_snooze_control();
      clearType = cur; doClear = true;
      set_state(P_WORKING);
    } else if (s_snoozeCount < MAX_SNOOZE) { // snooze 5 min (FR-05)
      s_snoozeCount++;
      s_snoozedAlert = cur;
      s_snoozeDueV = vmillis() + MIN_MS(SNOOZE_MIN);
      s_snoozePending = true;
      clearType = cur; doClear = true;
      set_state(P_WORKING);
    } else {                                 // max 2 snoozes: keep alert
      s_snoozeRejected = true; s_snoozeRejectedMs = millis();
    }
  } else {
    // settings menu (FR-07, FR-10)
    switch (s_page) {
      case UI_STATUS:
        if (longPress) s_page = UI_LUX_SETTING;
        break;
      case UI_LUX_SETTING:
        if (!longPress) {
          s_luxTarget += LUX_STEP;
          if (s_luxTarget > LUX_USER_MAX) s_luxTarget = LUX_DEFAULT;   // NFR-15: never out of range
          outChanged = true;
        } else s_page = UI_LATE_SETTING;
        break;
      case UI_LATE_SETTING:
        if (!longPress) { s_lateMode = !s_lateMode; outChanged = true; }
        else s_page = UI_STATUS;
        break;
    }
  }
  uint8_t sc = s_snoozeCount;
  UNLOCK();
  if (doClear) {
    post_alert(SRC_PRESENCE, clearType, 1, ALERT_CLEAR);
    logf("B: %s press -> %s (snooze %u/%u)", longPress ? "long" : "short",
         longPress ? "snooze" : "ack", sc, MAX_SNOOZE);
  }
  if (outChanged) {
    logf("B: user_lux_target=%u late_mode=%d", s_luxTarget, s_lateMode);
    publish_outputs(false);
  }
}

void button_task(void*) {
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), button_isr, CHANGE);
  // Debounce on the settled level: every edge (bounce included) restarts a 30 ms quiet
  // window; only after the line is quiet do we read the pin. The level the ISR samples
  // during a bounce burst can be either value, so it is used only as a wake-up.
  bool down = false, pending = false; uint32_t downMs = 0, lastEdgeMs = 0, burstStartMs = 0;
  for (;;) {
    BtnEdge e;
    bool got = xQueueReceive(s_btnQ, &e, pdMS_TO_TICKS(pending ? 5 : T_BUTTON_MS)) == pdTRUE;
    STAT_BEGIN();
    if (got) {
      if (!pending) burstStartMs = e.ms;
      pending = true; lastEdgeMs = e.ms;
    }
    if (pending && millis() - lastEdgeMs >= BUTTON_DEBOUNCE_MS) {
      pending = false;
      bool pressed = digitalRead(PIN_BUTTON) == LOW;
      if (pressed && !down) { down = true; downMs = burstStartMs; }
      else if (!pressed && down) {
        down = false;
        uint32_t dur = burstStartMs - downMs;
        if (dur >= BUTTON_DEBOUNCE_MS) on_press(dur >= LONG_PRESS_MS);
      }
    }
    LOCK();
    if (s_page != UI_STATUS && millis() - s_lastBtnMs >= SETTINGS_TIMEOUT_MS) s_page = UI_STATUS;
    UNLOCK();
    STAT_END(TID_BUTTON);
  }
}

PresenceView presence_view() {
  PresenceView v;
  LOCK();
  v.state = s_state; v.ui_page = s_page; v.snooze_count = s_snoozeCount;
  v.snooze_pending = s_snoozePending; v.snooze_rejected = s_snoozeRejected;
  v.away_s = s_awayCounting ? (uint32_t)((vmillis() - s_awayStartV) / 1000ULL) : 0;
  UNLOCK();
  return v;
}

void presence_ui_reset() { LOCK(); s_page = UI_STATUS; UNLOCK(); }
