// =====================================================================
//  comm.cpp  -  Module A (owner: A), room side.  A3 v1.0.
//   Publishes room/air (retained) after every SCD41 reading.
//   Subscribes desk/+/report and desk/+/online and keeps a desk table:
//     alive  = online flag && last report within 3 x iv  (A3 section 6)
//     room_desks_online   = number of alive desks
//     room_occupied_count = alive desks with occ = 1
//   New desks are added automatically from the topic name (NFR-13).
// =====================================================================
#include "comm.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient   s_net;
static PubSubClient s_mqtt(s_net);
static char s_topicAir[64], s_topicOnline[64], s_topicDeskSub[64], s_topicDeskOnSub[64], s_clientId[40];

struct Desk { bool used; uint8_t id; bool occ; uint8_t iv; uint32_t lastSeen; bool online; int32_t lastSeq; uint32_t lost; };
static Desk s_desks[MAX_DESKS];
static SemaphoreHandle_t s_deskMutex;
static uint16_t s_seq = 0;
static uint32_t s_published = 0, s_badMsg = 0;
static uint32_t s_backoffMs = 1000, s_nextMqttTry = 0, s_nextWifiTry = 0;

static Desk* desk_for(uint8_t id) {
  Desk* free = nullptr;
  for (auto& d : s_desks) { if (d.used && d.id == id) return &d; if (!d.used && !free) free = &d; }
  if (free) { *free = {true, id, false, 5, 0, false, -1, 0}; logf("A: new desk %u", id); }
  return free;
}

// topic: <prefix>desk/<id>/report|online
static int parse_desk_id(const char* topic) {
  const char* p = topic + strlen(TOPIC_PREFIX "desk/");
  int id = atoi(p);
  return (id >= 1 && id <= 254) ? id : -1;
}

static void on_message(char* topic, byte* payload, unsigned int len) {
  if (strncmp(topic, TOPIC_PREFIX "desk/", strlen(TOPIC_PREFIX "desk/")) != 0) return;
  int id = parse_desk_id(topic);
  if (id < 0) { s_badMsg++; return; }
  bool isReport = strstr(topic, "/report") != nullptr;
  xSemaphoreTake(s_deskMutex, portMAX_DELAY);
  Desk* d = desk_for(id);
  if (d) {
    if (isReport) {
      JsonDocument doc;
      if (!deserializeJson(doc, payload, len) && doc["occ"].is<int>()) {
        int iv = doc["iv"] | 5; int32_t seq = doc["seq"] | -1;
        if (d->lastSeq >= 0 && seq >= 0 && seq != ((d->lastSeq + 1) & 0xFFFF)) {
          uint16_t gap = (uint16_t)(seq - d->lastSeq - 1);
          if (gap < SEQ_GAP_MAX) d->lost += gap;   // a bigger jump = desk restart / retained replay
        }
        d->lastSeq = seq;
        bool occ = (int)doc["occ"] != 0;
        if (occ != d->occ) logf("A: desk %u occupied=%d", id, occ);
        d->occ = occ; d->iv = constrain(iv, 1, 60); d->lastSeen = millis(); d->online = true;
      } else s_badMsg++;
    } else {
      d->online = (len > 0 && payload[0] == '1');
      if (d->online) d->lastSeen = millis();
      logf("A: desk %u online=%d", id, d->online);
    }
  }
  xSemaphoreGive(s_deskMutex);
}

// Recount live desks into room_desks_online / room_occupied_count (A1 section 4).
// Called by air_sensor_task right before each FSM step so C never decides on
// counts that are one reading (5 s) old, and by the comm task before publishing.
void comm_update_counts() {
  uint8_t online = 0, occ = 0; uint32_t now = millis();
  xSemaphoreTake(s_deskMutex, portMAX_DELAY);
  for (auto& d : s_desks) {
    if (!d.used) continue;
    bool alive = d.online && (now - d.lastSeen) <= 3000UL * d.iv;
    if (alive) { online++; if (d.occ) occ++; }
  }
  xSemaphoreGive(s_deskMutex);
  RoomShared s = shared_snapshot();
  if (s.room_desks_online != online || s.room_occupied_count != occ)
    logf("A: room_desks_online=%u room_occupied_count=%u", online, occ);
  SHARED_WRITE( g_shared.room_desks_online = online; g_shared.room_occupied_count = occ; );
}

void comm_init() {
  s_deskMutex = xSemaphoreCreateMutex();
  snprintf(s_topicAir, sizeof s_topicAir, TOPIC_PREFIX "room/air");
  snprintf(s_topicOnline, sizeof s_topicOnline, TOPIC_PREFIX "room/online");
  snprintf(s_topicDeskSub, sizeof s_topicDeskSub, TOPIC_PREFIX "desk/+/report");
  snprintf(s_topicDeskOnSub, sizeof s_topicDeskOnSub, TOPIC_PREFIX "desk/+/online");
  snprintf(s_clientId, sizeof s_clientId, "swer-room-%04x", (unsigned)(esp_random() & 0xFFFF));
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
  s_nextWifiTry = millis() + 15000;
  s_mqtt.setServer(MQTT_HOST, MQTT_PORT);
  s_mqtt.setCallback(on_message);
  s_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  s_mqtt.setSocketTimeout(3);
  s_mqtt.setBufferSize(256);
}

static bool mqtt_connect() {
  if (!s_mqtt.connect(s_clientId, nullptr, nullptr, s_topicOnline, 1, true, "0")) return false;
  s_mqtt.publish(s_topicOnline, "1", true);
  s_mqtt.subscribe(s_topicDeskSub, 0);
  s_mqtt.subscribe(s_topicDeskOnSub, 1);
  logf("A: MQTT connected as %s", s_clientId);
  return true;
}

void room_communication_task(void*) {
  for (;;) {
    bool notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(T_COMM_POLL_MS)) > 0;   // air task notifies every 5 s
    uint32_t now = millis();
    if (g_test.wifi_off) {
      if (WiFi.status() == WL_CONNECTED) { s_mqtt.disconnect(); WiFi.disconnect(); logf("A: Wi-Fi OFF (test)"); }
      s_nextWifiTry = now; s_backoffMs = 1000;
    } else if (WiFi.status() != WL_CONNECTED) {
      if ((int32_t)(now - s_nextWifiTry) >= 0) {
        WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
        s_nextWifiTry = now + s_backoffMs; s_backoffMs = min<uint32_t>(s_backoffMs * 2, 60000);
      }
    } else if (!s_mqtt.connected() && (int32_t)(now - s_nextMqttTry) >= 0) {
      if (mqtt_connect()) s_backoffMs = 1000;
      else { s_nextMqttTry = now + s_backoffMs; s_backoffMs = min<uint32_t>(s_backoffMs * 2, 60000);
             logf("A: MQTT connect failed, retry in %lu ms", (unsigned long)s_backoffMs); }
    }
    s_mqtt.loop();

    if (notified) {
      STAT_BEGIN();
      comm_update_counts();
      if (s_mqtt.connected()) {
        RoomShared s = shared_snapshot();
        char buf[128];
        snprintf(buf, sizeof buf, "{\"seq\":%u,\"co2\":%u,\"t\":%d,\"rh\":%u,\"lvl\":%u,\"tf\":%u,\"ok\":%d,\"fan\":%u}",
          s_seq++, s.air.co2_ppm, s.air.temp_c_x10, s.air.rh_pct, s.air.co2_level, s.air.temp_flag, s.air.sensor_ok ? 1 : 0, s.air.fan_pct);
        if (s_mqtt.publish(s_topicAir, buf, true)) s_published++;
      }
      STAT_END(TID_COMM);
    }
  }
}

void comm_print_status() {
  Serial.printf("A: wifi=%d mqtt=%d published=%lu bad=%lu\n", WiFi.status() == WL_CONNECTED, s_mqtt.connected(),
                (unsigned long)s_published, (unsigned long)s_badMsg);
  xSemaphoreTake(s_deskMutex, portMAX_DELAY);
  uint32_t now = millis();
  for (auto& d : s_desks) if (d.used)
    Serial.printf("   desk %u online=%d occ=%d iv=%us last=%lus ago lost=%lu\n", d.id, d.online, d.occ, d.iv,
                  (unsigned long)((now - d.lastSeen) / 1000), (unsigned long)d.lost);
  xSemaphoreGive(s_deskMutex);
}
