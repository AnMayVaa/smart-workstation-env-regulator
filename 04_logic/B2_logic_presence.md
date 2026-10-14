# B2 Logic โมดูลตรวจจับคนและการเตือนพัก

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.4 |
| วันที่ | 2026-10-15 |
| เจ้าของ | B |
| รูปแบบ | Timed FSM ร่วมกับ alert queue และ display manager |

## 1. สถานะทั้งหมด

| สถานะ | ความหมาย |
|---|---|
| `UNOCCUPIED` | ไม่พบคนและยังไม่เริ่ม session การนั่ง |
| `WORKING` | พบคนและกำลังนับเวลานั่งต่อเนื่อง |
| `EYE_ALERT` | ถึงกำหนด 20 นาที กำลังเตือนให้พักสายตา |
| `STAND_ALERT` | ถึงกำหนด 30 นาที กำลังเตือนให้ลุกเดิน 3 นาที |
| `AWAY_PENDING` | ไม่พบคนชั่วคราว แต่ยังไม่ครบ 3 นาที จึงยังไม่ถือว่าพักสำเร็จ |
| `SENSOR_FAULT` | ไม่ได้รับ frame ที่ใช้ได้จากเซนเซอร์เกิน 500 ms; ใช้ fail-safe ว่ามีคน |

`away_timeout` ไม่เป็นสถานะหลัก แต่เป็น output ที่เปลี่ยนเป็น `true` เมื่ออยู่ใน `AWAY_PENDING` หรือ `UNOCCUPIED` ต่อเนื่องครบ 5 นาที และกลับเป็น `false` ทันทีเมื่อพบคนหรือเมื่อเซนเซอร์เสีย

เมนูตั้งค่าเป็นสถานะย่อยของ UI และไม่เปลี่ยนสถานะ FSM การตรวจคน เมื่อไม่มี alert ผู้ใช้กดยาวเพื่อเข้า `LUX_SETTING`, กดสั้นเพิ่มค่าทีละ 100 lux แบบวนในช่วง 500–1,000 lux, กดยาวเพื่อยืนยันและไป `LATE_MODE_SETTING`, กดสั้นเพื่อสลับเปิด/ปิด และกดยาวเพื่อยืนยัน/ออก เมื่อตรวจพบ alert ระบบออกจากเมนูและให้การรับทราบ/เลื่อนเตือนมีสิทธิ์เหนือกว่า

## 2. ตารางเปลี่ยนสถานะ

| จากสถานะ | ไปสถานะ | เงื่อนไข | สิ่งที่ระบบทำ |
|---|---|---|---|
| เริ่มระบบ | `UNOCCUPIED` | เซนเซอร์ปกติและไม่พบคน | ตั้ง `occupied=false`, ล้างตัวนับ session, เริ่มตัวนับเวลาว่าง |
| เริ่มระบบ/ทุกสถานะ | `SENSOR_FAULT` | ไม่มี valid frame เกิน 500 ms หรือข้อมูล UART ผิดรูปแบบต่อเนื่อง | ตั้ง `occupied=true`, `away_timeout=false`, ส่ง `ALERT_SENSOR_FAULT` priority 2; ห้ามสั่งปิดไฟ |
| `SENSOR_FAULT` | `WORKING` | ได้ valid frame และพบคนต่อเนื่อง 1 s | clear fault alert, ตั้ง `occupied=true`, เริ่ม session ใหม่เพื่อไม่ใช้เวลาที่ไม่น่าเชื่อถือ |
| `SENSOR_FAULT` | `UNOCCUPIED` | ได้ valid frame และไม่พบคนต่อเนื่อง 1 s | clear fault alert, ตั้ง `occupied=false`, เริ่มตัวนับเวลาว่าง |
| `UNOCCUPIED` | `WORKING` | พบคนต่อเนื่อง 1 s | ตั้ง `occupied=true`, `away_timeout=false`, รีเซ็ต `sit_elapsed`, `next_eye_due=20 min`, `next_stand_due=30 min` |
| `UNOCCUPIED` | `UNOCCUPIED` | ไม่พบคนครบ 5 min | ตั้ง `away_timeout=true` เพียงครั้งเดียวเมื่อค่าเปลี่ยน |
| `WORKING` | `EYE_ALERT` | ถึง `next_eye_due` และยังไม่ถึง stand alert | ส่ง `ALERT_EYE_BREAK` priority 1, วาดข้อความพักสายตา 20 s, buzzer ดังสั้น; เลื่อน due รอบถัดไปอีก 20 min เพื่อไม่ส่งซ้ำทุกวินาที |
| `WORKING`/`EYE_ALERT` | `STAND_ALERT` | ถึง `next_stand_due` | ส่ง `ALERT_STAND_BREAK` priority 1 และแทน eye alert ถ้าเกิดพร้อมกัน, แสดงให้ลุกเดิน 3 min; เลื่อน due รอบถัดไปอีก 30 min |
| `EYE_ALERT`/`STAND_ALERT` | `WORKING` | กดสั้น | รับทราบและหยุดเสียง แต่ไม่รีเซ็ตเวลานั่ง เพราะยังไม่ได้พักจริง |
| `EYE_ALERT`/`STAND_ALERT` | `WORKING` | กดยาวอย่างน้อย 1.5 s และ `snooze_count < 2` | เพิ่ม `snooze_count`, เก็บชนิด alert ไว้ใน `snoozed_alert`, ตั้ง `snooze_due` อีก 5 min, หยุดเสียง และแสดงเวลาเตือนใหม่ |
| `EYE_ALERT`/`STAND_ALERT` | สถานะเดิม | กดยาวเมื่อ `snooze_count == 2` | ไม่เลื่อนและไม่ clear alert; OLED แสดงว่าเลื่อนได้สูงสุด 2 ครั้ง ผู้ใช้ยังรับทราบด้วยกดสั้นหรือพักจริงได้ |
| `WORKING` | `WORKING` | มี `ALERT_STAND_BREAK` ที่ snooze รออยู่และถึงกำหนด eye alert | เลื่อน `next_eye_due` ไปรอบถัดไปโดยไม่ล้าง snooze; stand alert ครอบ eye alert จนกว่าจะเตือนซ้ำ |
| `WORKING`/alert | `AWAY_PENDING` | ไม่พบคนต่อเนื่อง 1 s | ตั้ง `occupied=false`, จำ `sit_elapsed`, เริ่ม `away_elapsed`; clear alert ของ B บนจอแต่เก็บ due time |
| `AWAY_PENDING` | `WORKING` | พบคนก่อนครบ 3 min | ตั้ง `occupied=true`, `away_timeout=false`, ทำ session เดิมต่อโดยไม่นับช่วงที่ลุกออกไป |
| `AWAY_PENDING` | `UNOCCUPIED` | ไม่พบคนครบ 3 min | ถือว่าพักสำเร็จตามช่วงลุกเดิน 3 นาที [B2-2], รีเซ็ต session และ due time; ถ้าเวลาว่างรวมถึง 5 min ให้ตั้ง `away_timeout=true` |

## 3. ภาพ FSM

- ภาพพร้อมใช้: `07_visuals/B2_fsm_presence_v1.png`
- ภาพเวกเตอร์: `07_visuals/B2_fsm_presence_v1.svg`
- ต้นฉบับแก้ไขใน diagrams.net: `07_visuals/B2_fsm_presence_v1.drawio`

ในการจำลอง `read_latest_ld2410_frame_without_blocking()` ถูกแทนด้วยการอ่าน slide switch ที่ GPIO 13 เพื่อคงสถานะมีคนได้นาน ส่วนฮาร์ดแวร์จริงยังอ่าน frame จาก LD2410 ทาง UART2 ตามเดิม

## 4. Pseudocode

```text
CONSTANT EYE_INTERVAL       = 20 min
CONSTANT STAND_INTERVAL     = 30 min
CONSTANT VALID_BREAK        = 3 min
CONSTANT AWAY_TIMEOUT       = 5 min
CONSTANT SNOOZE_INTERVAL    = 5 min
CONSTANT MAX_SNOOZE         = 2
CONSTANT SENSOR_TIMEOUT     = 500 ms
CONSTANT PRESENCE_STABLE    = 1 s
CONSTANT BUTTON_DEBOUNCE    = 30 ms
CONSTANT LONG_PRESS         = 1.5 s
CONSTANT SETTINGS_TIMEOUT   = 10 s
CONSTANT DEFAULT_LUX_TARGET = 500 lux
CONSTANT LUX_STEP           = 100 lux
CONSTANT MAX_LUX_TARGET     = 1000 lux

VARIABLE snooze_count       = 0
VARIABLE snooze_pending     = false
VARIABLE snoozed_alert      = NONE
VARIABLE snooze_due         = 0
VARIABLE user_lux_target    = 500 lux
VARIABLE late_mode          = false
VARIABLE ui_page            = STATUS

presence_task every 100 ms:
    frame = read_latest_ld2410_frame_without_blocking()

    if frame is valid:
        last_valid_frame = now
        raw_presence = frame.presence
        update_stable_presence(raw_presence, PRESENCE_STABLE)

    if now - last_valid_frame > SENSOR_TIMEOUT:
        sensor_ok = false
        occupied = true                 // fail-safe: do not turn lights off on a person
        away_timeout = false
        post_alert_once(SRC_PRESENCE, ALERT_SENSOR_FAULT, priority=2, ALERT_SHOW)
        state = SENSOR_FAULT
        continue

    if state == SENSOR_FAULT and presence_is_stable:
        sensor_ok = true
        post_alert(SRC_PRESENCE, ALERT_SENSOR_FAULT, priority=2, ALERT_CLEAR)
        if stable_presence:
            start_new_session()
            state = WORKING
        else:
            start_away_timer()
            state = UNOCCUPIED

    publish occupied only when its value changes

break_timer_task every 1000 ms:
    switch state:
        UNOCCUPIED:
            if stable_presence:
                start_new_session()
                state = WORKING
            else if away_elapsed >= AWAY_TIMEOUT:
                set_away_timeout_true_once()

        WORKING:
            sit_elapsed += 1 s
            sit_minutes = floor(sit_elapsed / 60 s)

            if not stable_presence:
                reset_snooze_control()
                begin_away_without_resetting_session()
                occupied = false
                state = AWAY_PENDING
            else if snooze_pending and now >= snooze_due:
                if snoozed_alert == ALERT_STAND_BREAK and now >= next_eye_due:
                    next_eye_due += EYE_INTERVAL
                enqueue snoozed_alert with its original priority
                snooze_pending = false
                state = alert_state_for(snoozed_alert)
            else if now >= next_stand_due:
                reset_snooze_control()
                enqueue ALERT_STAND_BREAK with priority 1
                next_stand_due += STAND_INTERVAL
                if now >= next_eye_due:
                    next_eye_due += EYE_INTERVAL
                state = STAND_ALERT
            else if now >= next_eye_due and snooze_pending
                    and snoozed_alert == ALERT_STAND_BREAK:
                next_eye_due += EYE_INTERVAL
                // pending STAND remains intact and covers this EYE due time
            else if now >= next_eye_due:
                reset_snooze_control()
                enqueue ALERT_EYE_BREAK with priority 1
                next_eye_due += EYE_INTERVAL
                state = EYE_ALERT

        EYE_ALERT or STAND_ALERT:
            sit_elapsed += 1 s
            if not stable_presence:
                reset_snooze_control()
                clear_current_presence_alert()
                begin_away_without_resetting_session()
                state = AWAY_PENDING
            else if stand alert becomes due:
                reset_snooze_control()
                replace current eye alert with ALERT_STAND_BREAK
                next_stand_due += STAND_INTERVAL
                state = STAND_ALERT
            else if short_press:
                reset_snooze_control()
                clear_current_presence_alert()
                state = WORKING
            else if long_press:
                if snooze_count < MAX_SNOOZE:
                    snooze_count += 1
                    snoozed_alert = current alert type
                    snooze_due = now + SNOOZE_INTERVAL
                    snooze_pending = true
                    clear_current_presence_alert()
                    state = WORKING
                else:
                    keep current alert active
                    show "เลื่อนได้สูงสุด 2 ครั้ง"

        AWAY_PENDING:
            away_elapsed += 1 s
            if stable_presence and away_elapsed < VALID_BREAK:
                occupied = true
                away_timeout = false
                resume_same_session_excluding_away_time()
                state = WORKING
            else if away_elapsed >= VALID_BREAK:
                reset_session_and_all_presence_alerts()
                sit_minutes = 0
                state = UNOCCUPIED
            if away_elapsed >= AWAY_TIMEOUT:
                set_away_timeout_true_once()

set_away_timeout_true_once():
    if away_timeout == false:
        away_timeout = true
        user_lux_target = DEFAULT_LUX_TARGET
        late_mode = false
        ui_page = STATUS
        publish user_lux_target and late_mode only if each value changed

button_event_task every 100 ms and on queued edge:
    when any CHANGE edge arrives from ISR:
        mark a debounce burst pending
        restart a 30 ms quiet window for every following edge
    after the line has stayed quiet for 30 ms:
        read the settled GPIO level; do not trust the level sampled by ISR
        if settled level changes to pressed:
            remember the first edge time as press_start
        else if settled level changes to released:
            duration = first release-burst edge time - press_start
            emit one short_press or long_press only when duration >= 30 ms

    if a presence alert is active and a press event exists:
        short press acknowledges the alert
        long press requests 5 min snooze; reject when snooze_count == MAX_SNOOZE
    else if ui_page == STATUS and long_press:
        ui_page = LUX_SETTING
    else if ui_page == LUX_SETTING and short_press:
        user_lux_target += LUX_STEP
        if user_lux_target > MAX_LUX_TARGET:
            user_lux_target = DEFAULT_LUX_TARGET
        publish user_lux_target only when changed
    else if ui_page == LUX_SETTING and long_press:
        ui_page = LATE_MODE_SETTING
    else if ui_page == LATE_MODE_SETTING and short_press:
        late_mode = not late_mode
        publish late_mode only when changed
    else if ui_page == LATE_MODE_SETTING and long_press:
        ui_page = STATUS

    if a new alert arrives or now - last_button_event >= SETTINGS_TIMEOUT:
        ui_page = STATUS

display_alert_task every 500 ms or when alert queue wakes it:
    air = snapshot air_status under the shared-data mutex

    if air.age_s > 15:
        sync_alert(SRC_AIR, ALERT_CO2_HIGH, false)
        sync_alert(SRC_AIR, ALERT_TEMP, false)
        sync_alert(SRC_AIR, ALERT_SENSOR_FAULT, false)
    else if air.sensor_ok == false:
        sync_alert(SRC_AIR, ALERT_CO2_HIGH, false)
        sync_alert(SRC_AIR, ALERT_TEMP, false)
        sync_alert(SRC_AIR, ALERT_SENSOR_FAULT, true, priority=2)
    else:
        sync_alert(SRC_AIR, ALERT_SENSOR_FAULT, false)
        sync_alert(SRC_AIR, ALERT_CO2_HIGH, air.co2_level == 2, priority=1)
        sync_alert(SRC_AIR, ALERT_TEMP, air.temp_flag != 0, priority=1,
                   detail=air.temp_flag)

    consume all ALERT_SHOW and ALERT_CLEAR requests
    discard cleared alerts
    choose highest priority active alert; if tied choose oldest

    if air.age_s > 15 or air.sensor_ok == false:
        show "ไม่มีข้อมูลอากาศ" on normal status page
    else:
        show air.co2_ppm, air.temp_c_x10 / 10, air.rh_pct,
             air.co2_level and air.fan_pct

    if active alert exists:
        render `SENSOR ERR` for sensor faults so the title fits 128 px
        keep presence alerts and every priority 2 alert full-screen until cleared
        keep other alerts full-screen for 10 s, then show a one-line banner
        on the status page so sit time remains visible
        priority 0: screen only
        priority 1: one short beep when alert becomes active
        priority 2: repeat short beeps with gaps, never block the task
    else if ui_page == LUX_SETTING:
        render user_lux_target and instructions "กดสั้น: เพิ่ม / กดยาว: ถัดไป"
    else if ui_page == LATE_MODE_SETTING:
        render late_mode and instructions "กดสั้น: สลับ / กดยาว: บันทึก"
    else:
        render normal status page with sit_minutes, air.co2_level,
               air.fan_pct and link_ok

    if take i2c_lock succeeds within 50 ms:
        send one OLED transaction
        release i2c_lock immediately
    else:
        skip this frame and increment display_error_count
```

## 5. ค่าที่ใช้และที่มา

| ค่า | ใช้ในระบบ | ที่มา/เหตุผล |
|---|---:|---|
| เตือนพักสายตา | 20 min | กฎ 20-20-20 และงานวิจัยที่ทีมอ้างอิง [B2-1]; เป็นค่ากลางของ D ห้ามเปลี่ยนเอง |
| เตือนลุก | 30 min | งานวิจัยใช้การลุกเดิน 3 min ทุก 30 min [B2-2]; เป็นค่ากลางของ D |
| ถือว่าพักสำเร็จ | ไม่พบคนต่อเนื่อง 3 min | ใช้ช่วงการลุกเดิน 3 min จาก [B2-2] เพื่อไม่รีเซ็ตเมื่อเพียงลุกหยิบของ |
| ปิดไฟ/ประหยัดพลังงาน | ไม่พบคน 5 min | FR-08 และ NFR-08; แยกจาก 3 min เพื่อไม่ปิดไฟทันทีเมื่อพักสั้น |
| snooze | 5 min | FR-05 |
| จำนวน snooze สูงสุด | 2 ครั้งต่อ alert | FR-05; ไม่มีคำสั่งปิดการเตือนถาวร |
| ความสว่างที่ผู้ใช้ตั้ง | 500–1,000 lux, เพิ่มทีละ 100 lux | FR-07 และ NFR-15; ค่า step เป็น design parameter สำหรับปุ่มเดียว |
| โหมดทำงานดึก | ปิดเป็นค่าเริ่มต้น | FR-10; เปิดได้จากเมนูบน OLED |
| รีเซ็ตค่าผู้ใช้ | เมื่อไม่มีคนครบ 5 min | FR-11; กลับเป็น 500 lux และ `late_mode=false` |
| sensor timeout | 500 ms | เท่ากับพลาด 5 รอบของ task 100 ms เป็น design parameter เพื่อตรวจ fault เร็วและไม่ตัดสินจาก frame เดียว |
| presence stable | 1 s | ต้องเห็นผลเดิม 10 รอบของ task 100 ms เป็น design parameter เพื่อลดการสลับสถานะจาก noise |
| debounce / long press | 30 ms / 1.5 s | design parameter สำหรับแยก bounce และ intent ของผู้ใช้ ต้องยืนยันจาก usability test |

## 6. ข้อมูลรับเข้าและส่งออกตาม A1

| ทิศทาง | ข้อมูล | การใช้ |
|---|---|---|
| สร้าง | `occupied` (`bool`) | เปลี่ยนเมื่อสถานะคนเปลี่ยน; sensor fault ใช้ `true`; A รวมค่าจากทุกโต๊ะเป็น `room_occupied_count` ให้ C |
| สร้าง | `sit_minutes` (`uint16_t`) | อัปเดตทุก 1 s จากเวลานั่ง ไม่รวมช่วง `AWAY_PENDING` |
| สร้าง | `away_timeout` (`bool`) | `true` เมื่อไม่พบคน 5 min; fault ใช้ `false` |
| สร้าง | `user_lux_target` (`uint16_t`) | 500–1,000 lux; B สร้างจากปุ่ม ส่งเมื่อเปลี่ยน และ D ใช้เป็น setpoint |
| สร้าง | `late_mode` (`bool`) | B สร้างจากปุ่ม ส่งเมื่อเปลี่ยน และ D ใช้เลือกแสงแบบกลางวันหลัง 19:00 |
| รับ | `air_status` (`air_status_t`) | A เขียนสำเนาจาก C ทุก 5 s; B อ่าน `co2_ppm`, `temp_c_x10`, `rh_pct`, `co2_level`, `temp_flag`, `sensor_ok`, `fan_pct`, `age_s` เพื่อแสดงผลและสร้าง alert อากาศ |
| รับ | `link_ok` (`bool`) | แสดงสถานะ Wi-Fi |
| สร้าง/รับ | `alert_request` (`alert_request_t`) | B สร้าง `ALERT_EYE_BREAK`, `ALERT_STAND_BREAK` และแปลง `air_status` เป็น `ALERT_CO2_HIGH`, `ALERT_TEMP`, `ALERT_SENSOR_FAULT`; รับ `ALERT_LINK_LOST` จาก A และ alert แสงจาก D ภายในโหนดเท่านั้น |
| ใช้ร่วม | `i2c_lock` | ล็อกเฉพาะ OLED transaction และรอไม่เกิน 50 ms |

## 7. กรณีผิดพลาด

| ปัญหา | ตรวจจับ | การตอบสนองแบบปลอดภัย |
|---|---|---|
| LD2410 ไม่ส่งข้อมูล/ข้อมูลเสีย | ไม่มี valid frame เกิน 500 ms | `occupied=true`, `away_timeout=false`, แจ้ง fault priority 2 และไม่อนุญาตให้ logic นี้ปิดไฟ |
| OLED หรือ I²C ถูกใช้งานอยู่ | ขอ `i2c_lock` ไม่ได้ใน 50 ms | ข้าม frame ปล่อยให้ task อื่นทำงาน บันทึก error และลองใหม่รอบถัดไป |
| Wi-Fi/ข้อมูลอากาศขาด | `link_ok=false` หรือ `air_status.age_s>15` | การเตือนพักยังทำงานต่อ; clear alert อากาศที่ค้างเพื่อไม่แสดงค่าล้าสมัย, แสดง “ไม่มีข้อมูลอากาศ” และรับ `ALERT_LINK_LOST` จาก A |
| alert queue เต็ม | enqueue ล้มเหลว | เก็บ counter; fault/priority 2 แทนที่ alert priority ต่ำสุด ส่วน priority 0 อาจถูกทิ้ง |
| ปุ่มค้าง | active-low ต่อเนื่องเกิน 10 s | รับ long press เพียงครั้งเดียว รอปล่อยปุ่มก่อนรับ event ใหม่ และแสดงสัญลักษณ์ปุ่มผิดปกติ |

## ลำดับการแสดงเมื่อมีหลาย alert

1. เลือก `priority` 2 ก่อน 1 ก่อน 0 ตาม A1 v1.0
2. priority เท่ากันให้รายการที่มาถึงก่อนแสดงก่อน; `ALERT_STAND_BREAK` แทน `ALERT_EYE_BREAK` ของ session เดียวกันเพราะครอบคลุมการพักที่มากกว่า
3. `ALERT_CLEAR` ลบเฉพาะรายการที่ `source` และ `type` ตรงกัน
4. alert ที่ถูกแทรกยังคงอยู่ใน active set และกลับมาแสดงเมื่อรายการสำคัญกว่าถูก clear
5. buzzer ส่งเสียงเฉพาะตอน alert กลายเป็นรายการที่แสดง เพื่อลดเสียงซ้ำจากการวาดจอทุก 500 ms

## เช็กลิสต์ก่อนส่ง

- [x] ทุกสถานะมีทางเข้าและทางออก รวม recovery จาก sensor fault
- [x] ค่า 20 min, 30 min, 3 min และ 5 min มีที่มา/สถานะการยืนยัน
- [x] ชื่อข้อมูลและเจ้าของตรงกับ `02_interface/A1_interface.md` v1.0 (`A1-v1.0`)
- [x] มี fail-safe ที่ไม่ปิดไฟใส่คน
- [x] ระบุนโยบาย alert ซ้อน, ปุ่ม, stale data และ I²C mutex
- [x] จำกัด snooze ไม่เกิน 2 ครั้งต่อ alert และไม่มีคำสั่งปิดการเตือนถาวร
- [x] รองรับ FR-07, FR-10 และ FR-11 ด้วยปุ่มเดียว โดยไม่เปิดให้ผู้ใช้เปลี่ยนเกณฑ์อากาศ/พัดลมตาม FR-16
- [x] B แปลง `air_status` เป็น alert ภายในโหนด; ไม่มี `alert_request` ส่งข้าม Wi-Fi

## อ้างอิง

[B2-1] C. Talens-Estarelles et al., “The effects of breaks on digital eye strain, dry eye and binocular vision: Testing the 20-20-20 rule,” *Contact Lens and Anterior Eye*, 2022, Art. no. 101744, doi: 10.1016/j.clae.2022.101744.

[B2-2] P. Wennberg et al., “Acute effects of breaking up prolonged sitting on fatigue and cognition: A pilot study,” *BMJ Open*, vol. 6, no. 2, Art. no. e009630, 2016, doi: 10.1136/bmjopen-2015-009630.

[B2-3] เอกสารประกอบวิชา 010123119, บท 3 หัวข้อ 3.5 หน้า 30–38 (Timed FSM), บท 16 หน้า 9 (mutex), หน้า 15–21 (OS), และหน้า 30–34 (real-time).
