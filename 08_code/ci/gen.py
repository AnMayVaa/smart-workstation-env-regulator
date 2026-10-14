#!/usr/bin/env python3
"""Generate Wokwi CLI automation scenarios for the CI firmware (desk TIME_SCALE 120).
At x120: 1 sim second = 2 system minutes -> EYE 10 s, STAND 15 s, snooze 2.5 s,
valid break 1.5 s, away timeout 2.5 s. DIM hold (60 s) and fades use real millis()."""
import yaml, os

ROOT = os.path.dirname(os.path.abspath(__file__))

class S:
    def __init__(self, run, name):
        self.run, self.name, self.steps, self.n = run, name, [], 0
    def w(self, text): self.steps.append({'wait-serial': text}); return self
    def d(self, t): self.steps.append({'delay': t}); return self
    def c(self, cmd): self.steps.append({'write-serial': cmd + '\n'}); return self
    def ctl(self, part, control, value):
        self.steps.append({'set-control': {'part-id': part, 'control': control, 'value': value}}); return self
    def press(self, ms=150, gap='450ms'):
        self.ctl('btn', 'pressed', 1).d(f'{ms}ms').ctl('btn', 'pressed', 0)
        if gap: self.d(gap)
        return self
    def long(self, gap='450ms'): return self.press(1700, gap)
    def shot(self, label, part='oled'):
        self.n += 1
        self.steps.append({'take-screenshot': {'part-id': part,
                           'save-to': f'out/{self.run}/{self.n:02d}_{label}.png'}}); return self
    def save(self, proj, fname):
        doc = {'name': self.name, 'version': 1, 'author': 'A', 'steps': self.steps}
        with open(os.path.join(ROOT, proj, fname), 'w') as f:
            yaml.safe_dump(doc, f, sort_keys=False, allow_unicode=True, width=200)

# ---------------- ROOM A: C2 FSM, hysteresis, room empty vs unknown ----------------
r = S('room_a', 'Room A: C2 FSM, 950 ppm hysteresis, empty vs unknown')
r.w('tasks started').w('MQTT connected').c('desks 1 1')
r.c('co2 1050').w('-> S3_AIR_POOR').d('300ms').c('status')            # R1 POOR fan 100
r.c('co2 970').d('5500ms').c('status')                                # R2 stays POOR (>= 950 after fan on)
r.c('co2 920').w('-> S2_AIR_MODERATE').d('300ms').c('status')         # R3 MODERATE fan 60
r.c('co2 750').w('-> S1_AIR_GOOD').d('300ms').c('status')             # R4 GOOD fan 0
r.c('co2 900').w('-> S2_AIR_MODERATE').d('300ms').c('status')         # R4b MODERATE from GOOD -> fan 0
r.c('co2 1200').w('-> S3_AIR_POOR')
r.c('desks 1 0').w('-> S0_ROOM_EMPTY').d('300ms').c('status')         # R5 empty -> fan 0
r.c('desks 0 0').w('-> S3_AIR_POOR').d('300ms').c('status')           # R5b unknown (0 online) -> not empty
r.c('stats').d('1500ms')
r.save('room_node', 'room_a.yaml')

# ---------------- ROOM B: sensor fault / recovery, temperature flag ----------------
r = S('room_b', 'Room B: SCD41 fault + 2-read recovery, temperature flags')
r.w('tasks started').c('desks 1 1').c('co2 700').w('-> S1_AIR_GOOD')
r.c('fault scd41').w('-> S4_SENSOR_FAULT').d('1200ms').c('status')    # R6 fan 80 + blink
r.d('3s').c('ok scd41').w('S4_SENSOR_FAULT ->').d('300ms').c('status')  # 2 good reads
r.c('temp 27').d('5500ms').c('status')                                # R7 tf=2
r.c('temp 18').d('5500ms').c('status')                                # tf=1
r.c('temp auto').c('co2 5200').d('16s').c('status')                   # out-of-range value -> fault
r.c('co2 pot').c('stats').d('1500ms')
r.save('room_node', 'room_b.yaml')

# ---------------- DESK A: menu, EYE/STAND, ack, snooze x2, reject 3rd ----------------
s = S('desk_a', 'Desk A: settings menu, break alerts, ack, snooze limit')
s.w('tasks started').w('away_timeout=1').d('500ms').shot('boot_status')
s.long().d('200ms').shot('menu_lux_500')
s.press().press().press().d('200ms').shot('menu_lux_800')
s.long().d('200ms').shot('menu_late_off')
s.press().d('200ms').shot('menu_late_on')
s.long().d('200ms').c('status')
s.c('presence on').w('UNOCCUPIED -> WORKING').d('2s').shot('working_sitting')
s.w('display alert EYE BREAK').d('300ms').shot('eye_break_alert')
s.press(gap=None).w('short press -> ack')
s.w('display alert STAND UP').d('300ms').shot('stand_alert')
s.long(gap=None).w('long press -> snooze (snooze 1/2)').d('200ms').shot('snoozed_1')
s.w('snoozed STAND re-raised').d('200ms').shot('stand_reraised_1')
s.long(gap=None).w('long press -> snooze (snooze 2/2)')
s.w('snoozed STAND re-raised').d('200ms').shot('stand_reraised_2')
s.long().d('100ms').shot('snooze_rejected_max2').c('status')
s.press(gap=None).w('short press -> ack').d('300ms').shot('after_ack').c('status').c('stats').d('1500ms')
s.save('desk_node', 'desk_a.yaml')

# ---------------- DESK B: light regulation, light/presence sensor faults, colour schedule ----------------
s = S('desk_b', 'Desk B: BH1750 + LD2410 faults, lux regulation, colour schedule')
s.w('tasks started').c('presence on').w('UNOCCUPIED -> WORKING')
s.c('fault bh1750').w('REGULATING -> SENSOR_FAULT').d('400ms').shot('light_sensor_fault').c('status')   # within 10 s, before EYE
s.c('ok bh1750').w('SENSOR_FAULT -> REGULATING').d('300ms').c('status')
s.c('fault ld2410').w('-> SENSOR_FAULT').d('400ms').shot('presence_sensor_fault').c('status')
s.c('ok ld2410').w('SENSOR_FAULT ->').d('300ms').c('status')
s.ctl('ldr', 'lux', 50).d('6s').c('status')                             # dark room -> B up
s.ctl('ldr', 'lux', 800).d('6s').c('status')                            # bright room -> B down
s.ctl('ldr', 'lux', 200).d('4s').c('status')
s.c('time 16:58').d('1s').c('status').d('10s').c('status').d('10s').c('status')
s.c('time auto').c('stats').d('1500ms')
s.save('desk_node', 'desk_b.yaml')

# ---------------- DESK C: FR-11 away reset + DIMMING -> back -> REGULATING ----------------
s = S('desk_c', 'Desk C: away_timeout resets user settings, dimming, return')
s.w('tasks started').w('away_timeout=1').d('500ms')
s.long().press().press().press().long().press().long().d('300ms').c('status')   # lux 800, late ON
s.c('presence on').w('UNOCCUPIED -> WORKING').d('2s')
s.c('presence off').w('WORKING -> AWAY_PENDING').w('AWAY_PENDING -> UNOCCUPIED')
s.w('away_timeout=1').w('REGULATING -> DIMMING').d('500ms').c('status').shot('away_dimming')
s.c('presence on').w('DIMMING -> REGULATING').d('2500ms').c('status')
s.c('stats').d('1500ms')
s.save('desk_node', 'desk_c.yaml')

# ---------------- DESK D: dimming hold 60 s -> OFF (Wi-Fi off to speed up the simulation) ----------------
s = S('desk_d', 'Desk D: 60 s dim hold then fade to OFF')
s.w('tasks started').c('wifi off').c('presence on').w('UNOCCUPIED -> WORKING').d('3s').c('status')
s.c('presence off').w('REGULATING -> DIMMING').d('2500ms').c('status')
s.d('30s').c('status')
s.w('DIMMING -> OFF').d('5500ms').c('status').shot('light_off').d('1500ms')
s.save('desk_node', 'desk_d.yaml')

# ---------------- INTEGRATION: room <-> desk over MQTT ----------------
r = S('int', 'INT room: occupancy from desk, CO2 high, sensor fault, desk link loss')
r.w('tasks started').w('MQTT connected')
r.w('desk 1 occupied=1').w('room_desks_online=1 room_occupied_count=1').c('status')   # TC-A-04
r.c('co2 1200').w('-> S3_AIR_POOR')                                                   # TC-A-01
r.d('8s').c('fault scd41').w('-> S4_SENSOR_FAULT')                                    # TC-A-02
r.d('6s').c('ok scd41').c('co2 700').w('S4_SENSOR_FAULT ->')
r.w('room_desks_online=0 room_occupied_count=0').c('status')                          # TC-A-03 desk link lost (stale 3 x iv)
r.w('room_desks_online=1 room_occupied_count=1').c('status').c('stats').d('1500ms')
r.save('room_int', 'int_room.yaml')

s = S('int', 'INT desk (real-time build): alerts from room, link lost / restore')
s.w('tasks started').w('MQTT connected').w('room/online=1').d('4s')
s.c('presence on').w('desk_report occ=1 (immediate)').d('2s').shot('working_link_ok')
s.w('display alert CO2 HIGH').d('600ms').shot('co2_high_from_room').c('status')
s.w('display alert SENSOR FAULT').d('600ms').shot('room_sensor_fault').c('status')
s.d('16s').c('status')
s.c('wifi off').w('link_ok=0').d('500ms').shot('link_lost').c('status')
s.d('18s').c('wifi on').w('link_ok=1').d('1500ms').shot('link_restored').c('status')
s.c('stats').d('1500ms')
s.save('desk_node_int', 'int_desk.yaml')

# ---------------- MULTI-DESK (NFR-13): room + desk 1 + desk 2 ----------------
r = S('multi', 'MULTI room: two desks counted separately')
r.w('tasks started').w('MQTT connected').w('desk 1 online=1').w('desk 2 online=1')
r.w('desk 1 occupied=1').d('6s').c('status').c('stats').d('1500ms')
r.save('room_multi', 'multi_room.yaml')

s = S('multi', 'MULTI desk 1: occupied')
s.w('tasks started').w('MQTT connected').w('room/online=1').d('8s')
s.c('presence on').w('desk_report occ=1').d('12s').c('status').shot('desk1_status').d('1500ms')
s.save('desk_multi1', 'multi_desk.yaml')

s = S('multi', 'MULTI desk 2: free')
s.w('tasks started').w('MQTT connected').w('room/online=1').d('20s').c('status').shot('desk2_status').d('1500ms')
s.save('desk_node_2', 'multi_desk.yaml')
print('ok')
