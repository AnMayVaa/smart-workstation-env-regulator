// =====================================================================
//  comm.cpp  -  Module A (owner: A), desk side.  A3 v1.0.
//   Topics (prefix TOPIC_PREFIX):
//     desk/<id>/report   desk -> room  QoS0  {"seq","occ","iv"}
//     desk/<id>/online   desk -> room  retained "1", LWT "0"
//     room/air           room -> desk  retained {"seq","co2","t","rh","lvl","tf","ok","fan"}
//     room/online        room -> desk  retained "1", LWT "0"
//   Runs on core 0 next to the Wi-Fi/TCP-IP stack (A-T note 2); control
//   tasks of B and D on core 1 are never blocked by network waits.
// =====================================================================
#include "comm.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <esp_sntp.h>

static WiFiClient   s_net;
static PubSubClient s_mqtt(s_net);
static char s_topicReport[64], s_topicOnline[64], s_topicAir[64], s_topicRoomOnline[64], s_clientId[40];
static volatile bool s_roomOnline = false;
static uint16_t s_seq = 0;
static int32_t  s_lastAirSeq = -1;
static uint32_t s_airRx = 0, s_airLost = 0, s_badMsg = 0, s_reports = 0;
static uint32_t s_backoffMs = 1000, s_nextMqttTry = 0, s_nextWifiTry = 0;
static bool     s_linkOk = false, s_linkAlertShown = false;
static bool     s_ntpStarted = false;
static volatile bool s_sntpSynced = false;

static void on_message(char* topic, byte* payload, unsigned int len) {
  if (strcmp(topic, s_topicRoomOnline) == 0) {
    s_roomOnline = (len > 0 && payload[0] == '1');
    logf("A: room/online=%d", s_roomOnline);
    return;
  }
  if (strcmp(topic, s_topicAir) != 0) return;
  JsonDocument doc;
  if (deserializeJson(doc, payload, len) || !doc["co2"].is<int>()) { s_badMsg++; return; }
  int co2 = doc["co2"], t = doc["t"], rh = doc["rh"], lvl = doc["lvl"], tf = doc["tf"], fan = doc["fan"], ok = doc["ok"];
  // range check (A3 section 6): drop malformed values, keep previous ones
  if (co2 < 0 || co2 > 10000 || lvl < 0 || lvl > 2 || tf < 0 || tf > 2 || fan < 0 || fan > 100 || rh < 0 || rh > 100) { s_badMsg++; return; }
  int32_t seq = doc["seq"] | -1;
  if (s_lastAirSeq >= 0 && seq >= 0 && seq != (int32_t)((s_lastAirSeq + 1) & 0xFFFF)) {
    uint16_t gap = (uint16_t)(seq - s_lastAirSeq - 1);
    if (gap < SEQ_GAP_MAX) s_airLost += gap;   // a bigger jump = room restart / retained replay, not loss
  }
  s_lastAirSeq = seq;
  s_airRx++;
  SHARED_WRITE(
    g_shared.air.co2_ppm = co2; g_shared.air.temp_c_x10 = t; g_shared.air.rh_pct = rh;
    g_shared.air.co2_level = lvl; g_shared.air.temp_flag = tf; g_shared.air.sensor_ok = ok != 0;
    g_shared.air.fan_pct = fan; g_shared.air_rx_ms = millis(); g_shared.air_ever_rx = true;
  );
}

// Age of the last room/air message. A message received during this loop pass has
// air_rx_ms > now, so the difference is taken signed (no unsigned underflow).
static uint32_t air_age(const DeskShared& s, uint32_t now) {
  if (!s.air_ever_rx) return 0xFFFFFFFF;
  int32_t a = (int32_t)(now - s.air_rx_ms);
  return a < 0 ? 0 : (uint32_t)a;
}

static void on_sntp_sync(struct timeval*) { s_sntpSynced = true; }

void comm_init() {
  snprintf(s_topicReport, sizeof s_topicReport, TOPIC_PREFIX "desk/%d/report", DESK_ID);
  snprintf(s_topicOnline, sizeof s_topicOnline, TOPIC_PREFIX "desk/%d/online", DESK_ID);
  snprintf(s_topicAir, sizeof s_topicAir, TOPIC_PREFIX "room/air");
  snprintf(s_topicRoomOnline, sizeof s_topicRoomOnline, TOPIC_PREFIX "room/online");
  snprintf(s_clientId, sizeof s_clientId, "swer-desk%d-%04x", DESK_ID, (unsigned)(esp_random() & 0xFFFF));
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);                       // modem-sleep between transmissions (A3 section 8)
  WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
  s_nextWifiTry = millis() + 15000;          // let the first attempt finish
  s_mqtt.setServer(MQTT_HOST, MQTT_PORT);
  s_mqtt.setCallback(on_message);
  s_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  s_mqtt.setSocketTimeout(3);
  s_mqtt.setBufferSize(256);
}

static bool mqtt_connect() {
  if (!s_mqtt.connect(s_clientId, nullptr, nullptr, s_topicOnline, 1, true, "0")) return false;
  s_mqtt.publish(s_topicOnline, "1", true);
  s_mqtt.subscribe(s_topicAir, 0);
  s_mqtt.subscribe(s_topicRoomOnline, 1);
  logf("A: MQTT connected as %s", s_clientId);
  return true;
}

static void publish_report(bool occ, uint8_t iv) {
  char buf[64];
  snprintf(buf, sizeof buf, "{\"seq\":%u,\"occ\":%d,\"iv\":%u}", s_seq++, occ ? 1 : 0, iv);
  if (s_mqtt.publish(s_topicReport, buf)) s_reports++;
}

static void update_clock() {
  TimeOfDay tod; bool valid;
  if (g_test.clock_override) {                     // test hook: "time HH:MM" (runs at TIME_SCALE)
    uint32_t m = (g_test.clock_override_min + (uint32_t)((vmillis() - g_test.clock_override_set_v) / 60000ULL)) % 1440;
    tod.hour = m / 60; tod.minute = m % 60; valid = true;
  } else {
    struct tm t;
    valid = s_sntpSynced && getLocalTime(&t, 0) && t.tm_year > (2020 - 1900);
    if (valid) { tod.hour = t.tm_hour; tod.minute = t.tm_min; }
  }
  SHARED_WRITE( if (valid) g_shared.time_of_day = tod; g_shared.time_valid = valid || g_shared.time_valid; );
}

void desk_communication_task(void*) {
  uint32_t nextReport = 0;
  bool lastOcc = false;
  for (;;) {
    // wake every 100 ms to service the MQTT socket, or at once when B notifies (occupied changed)
    bool notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(T_COMM_POLL_MS)) > 0;
    uint32_t now = millis();

    // ---- test hook: cut Wi-Fi (TC-A-03) ----
    if (g_test.wifi_off) {
      if (WiFi.status() == WL_CONNECTED) { s_mqtt.disconnect(); WiFi.disconnect(); logf("A: Wi-Fi OFF (test)"); }
      s_nextWifiTry = now; s_backoffMs = 1000;
    } else if (WiFi.status() != WL_CONNECTED) {
      if ((int32_t)(now - s_nextWifiTry) >= 0) {
        WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
        s_nextWifiTry = now + s_backoffMs; s_backoffMs = min<uint32_t>(s_backoffMs * 2, 60000);
      }
    } else {
      if (!s_ntpStarted) {
        sntp_set_time_sync_notification_cb(on_sntp_sync);
        configTzTime(NTP_TZ, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);   // SNTP re-syncs every hour
        s_ntpStarted = true;
        logf("A: Wi-Fi up %s, SNTP started", WiFi.localIP().toString().c_str());
      }
      if (!s_mqtt.connected() && (int32_t)(now - s_nextMqttTry) >= 0) {
        if (mqtt_connect()) { s_backoffMs = 1000; nextReport = 0; }
        else { s_nextMqttTry = now + s_backoffMs; s_backoffMs = min<uint32_t>(s_backoffMs * 2, 60000);
               logf("A: MQTT connect failed, retry in %lu ms", (unsigned long)s_backoffMs); }
      }
    }
    s_mqtt.loop();

    // ---- publish desk_report: every iv seconds, or immediately on change ----
    DeskShared snap = shared_snapshot();
    uint8_t iv = snap.away_timeout ? REPORT_IV_AWAY_S : REPORT_IV_NORMAL_S;
    bool due = (int32_t)(now - nextReport) >= 0;
    if (s_mqtt.connected() && (due || notified || snap.occupied != lastOcc)) {
      STAT_BEGIN();
      publish_report(snap.occupied, iv);
      if (snap.occupied != lastOcc) logf("A: desk_report occ=%d (immediate)", snap.occupied);
      lastOcc = snap.occupied;
      nextReport = now + iv * 1000UL;
      update_clock();
      // link_ok = broker connected + room online + fresh room/air (A1 section 3)
      uint32_t age = air_age(snap, now);
      s_linkOk = s_roomOnline && age <= STALE_MS;
      STAT_END(TID_COMM);
    } else if (!s_mqtt.connected()) {
      s_linkOk = false;
    }
    if (!s_mqtt.connected() && due) { update_clock(); nextReport = now + 1000; }   // clock keeps running offline (FR-20)

    // ---- air age + link state + LINK_LOST alert (priority 0) ----
    uint32_t age = air_age(snap, now);
    if (age > STALE_MS) s_linkOk = false;
    SHARED_WRITE(
      g_shared.air.age_s = age == 0xFFFFFFFF ? 0xFFFF : (uint16_t)min<uint32_t>(age / 1000, 0xFFFE);
      g_shared.link_ok = s_linkOk;
    );
    if (!s_linkOk && !s_linkAlertShown && now > 20000) {      // allow boot time before first alert
      post_alert(SRC_COMM, ALERT_LINK_LOST, 0, ALERT_SHOW); s_linkAlertShown = true; logf("A: link_ok=0");
    } else if (s_linkOk && s_linkAlertShown) {
      post_alert(SRC_COMM, ALERT_LINK_LOST, 0, ALERT_CLEAR); s_linkAlertShown = false; logf("A: link_ok=1");
    }
  }
}

// SNTP itself re-syncs hourly; this task turns the result into time_valid and
// keeps time_of_day fresh every minute (A-T: ntp_sync_task).
void ntp_sync_task(void*) {
  for (;;) {
    update_clock();
    vTaskDelay(pdMS_TO_TICKS(g_shared.time_valid ? 60000 / TIME_SCALE + 1 : 1000));
  }
}

void comm_print_status() {
  Serial.printf("A: wifi=%d mqtt=%d room_online=%d link_ok=%d reports=%lu air_rx=%lu air_lost=%lu bad=%lu rssi=%d\n",
    WiFi.status() == WL_CONNECTED, s_mqtt.connected(), s_roomOnline, s_linkOk,
    (unsigned long)s_reports, (unsigned long)s_airRx, (unsigned long)s_airLost, (unsigned long)s_badMsg, WiFi.RSSI());
}
