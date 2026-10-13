# B2 Logic โมดูลตรวจจับคนและการเตือนพัก

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.0 |
| วันที่ | 2026-10-13 |
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
| `EYE_ALERT`/`STAND_ALERT` | `WORKING` | กดยาวอย่างน้อย 1.5 s | เลื่อนเฉพาะ alert ปัจจุบัน 5 min, หยุดเสียง และแสดงเวลาเตือนใหม่ |
| `WORKING`/alert | `AWAY_PENDING` | ไม่พบคนต่อเนื่อง 1 s | ตั้ง `occupied=false`, จำ `sit_elapsed`, เริ่ม `away_elapsed`; clear alert ของ B บนจอแต่เก็บ due time |
| `AWAY_PENDING` | `WORKING` | พบคนก่อนครบ 3 min | ตั้ง `occupied=true`, `away_timeout=false`, ทำ session เดิมต่อโดยไม่นับช่วงที่ลุกออกไป |
| `AWAY_PENDING` | `UNOCCUPIED` | ไม่พบคนครบ 3 min | ถือว่าพักสำเร็จตามช่วงลุกเดิน 3 นาที [B2-2], รีเซ็ต session และ due time; ถ้าเวลาว่างรวมถึง 5 min ให้ตั้ง `away_timeout=true` |

## 3. ภาพ FSM

- ภาพพร้อมใช้: `07_visuals/B2_fsm_presence_v1.png`
- ภาพเวกเตอร์: `07_visuals/B2_fsm_presence_v1.svg`
- ต้นฉบับแก้ไขใน diagrams.net: `07_visuals/B2_fsm_presence_v1.drawio`

## 4. Pseudocode

```text
CONSTANT EYE_INTERVAL       = 20 min
CONSTANT STAND_INTERVAL     = 30 min
CONSTANT VALID_BREAK        = 3 min
CONSTANT AWAY_TIMEOUT       = 5 min
CONSTANT SNOOZE_INTERVAL    = 5 min
CONSTANT SENSOR_TIMEOUT     = 500 ms
CONSTANT PRESENCE_STABLE    = 1 s
CONSTANT BUTTON_DEBOUNCE    = 30 ms
CONSTANT LONG_PRESS         = 1.5 s

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
                away_timeout = true

        WORKING:
            sit_elapsed += 1 s
            sit_minutes = floor(sit_elapsed / 60 s)

            if not stable_presence:
                begin_away_without_resetting_session()
                occupied = false
                state = AWAY_PENDING
            else if now >= next_stand_due:
                enqueue ALERT_STAND_BREAK with priority 1
                next_stand_due += STAND_INTERVAL
                state = STAND_ALERT
            else if now >= next_eye_due:
                enqueue ALERT_EYE_BREAK with priority 1
                next_eye_due += EYE_INTERVAL
                state = EYE_ALERT

        EYE_ALERT or STAND_ALERT:
            sit_elapsed += 1 s
            if not stable_presence:
                clear_current_presence_alert()
                begin_away_without_resetting_session()
                state = AWAY_PENDING
            else if stand alert becomes due:
                replace current eye alert with ALERT_STAND_BREAK
                next_stand_due += STAND_INTERVAL
                state = STAND_ALERT
            else if short_press:
                clear_current_presence_alert()
                state = WORKING
            else if long_press:
                move due time of current alert to now + SNOOZE_INTERVAL
                clear_current_presence_alert()
                state = WORKING

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
                away_timeout = true

display_alert_task every 500 ms or when alert queue wakes it:
    consume all ALERT_SHOW and ALERT_CLEAR requests
    discard cleared alerts
    choose highest priority active alert; if tied choose oldest

    if air_status.age_s > 15 or air_status.sensor_ok == false:
        show "ไม่มีข้อมูลอากาศ" on normal status page
    else:
        show CO2, temperature and humidity from air_status

    if active alert exists:
        render alert message
        priority 0: screen only
        priority 1: one short beep when alert becomes active
        priority 2: repeat short beeps with gaps, never block the task
    else:
        render normal status page with sit_minutes and link_ok

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
| ปิดไฟ/ประหยัดพลังงาน | ไม่พบคน 5 min | ค่าร่างของ D ในภาพรวม; แยกจาก 3 min เพื่อไม่ปิดไฟทันทีเมื่อพักสั้นและต้องยืนยันใน D1 |
| snooze | 5 min | ค่าแนะนำในเอกสารบทบาท B; เป็น design parameter ไม่ใช่ค่าจากงานวิจัย |
| sensor timeout | 500 ms | เท่ากับพลาด 5 รอบของ task 100 ms เป็น design parameter เพื่อตรวจ fault เร็วและไม่ตัดสินจาก frame เดียว |
| presence stable | 1 s | ต้องเห็นผลเดิม 10 รอบของ task 100 ms เป็น design parameter เพื่อลดการสลับสถานะจาก noise |
| debounce / long press | 30 ms / 1.5 s | design parameter สำหรับแยก bounce และ intent ของผู้ใช้ ต้องยืนยันจาก usability test |

## 6. ข้อมูลรับเข้าและส่งออกตาม A1

| ทิศทาง | ข้อมูล | การใช้ |
|---|---|---|
| สร้าง | `occupied` (`bool`) | เปลี่ยนเมื่อสถานะคนเปลี่ยน; sensor fault ใช้ `true` |
| สร้าง | `sit_minutes` (`uint16_t`) | อัปเดตทุก 1 s จากเวลานั่ง ไม่รวมช่วง `AWAY_PENDING` |
| สร้าง | `away_timeout` (`bool`) | `true` เมื่อไม่พบคน 5 min; fault ใช้ `false` |
| รับ | `air_status` (`air_status_t`) | แสดงค่า; ถ้า `age_s > 15` หรือ `sensor_ok=false` แสดงว่าไม่มีข้อมูลอากาศ โดยไม่ตั้งเกณฑ์อากาศเอง |
| รับ | `link_ok` (`bool`) | แสดงสถานะ Wi-Fi |
| รับ | `alert_request` (`alert_request_t`) | จัดคิวตาม `priority`, `action` และเวลามาถึง |
| ใช้ร่วม | `i2c_lock` | ล็อกเฉพาะ OLED transaction และรอไม่เกิน 50 ms |

## 7. กรณีผิดพลาด

| ปัญหา | ตรวจจับ | การตอบสนองแบบปลอดภัย |
|---|---|---|
| LD2410 ไม่ส่งข้อมูล/ข้อมูลเสีย | ไม่มี valid frame เกิน 500 ms | `occupied=true`, `away_timeout=false`, แจ้ง fault priority 2 และไม่อนุญาตให้ logic นี้ปิดไฟ |
| OLED หรือ I²C ถูกใช้งานอยู่ | ขอ `i2c_lock` ไม่ได้ใน 50 ms | ข้าม frame ปล่อยให้ task อื่นทำงาน บันทึก error และลองใหม่รอบถัดไป |
| Wi-Fi/ข้อมูลอากาศขาด | `link_ok=false` หรือ `air_status.age_s>15` | การเตือนพักยังทำงานต่อ; แสดง “ไม่มีข้อมูลอากาศ” และไอคอนลิงก์ขาด |
| alert queue เต็ม | enqueue ล้มเหลว | เก็บ counter; fault/priority 2 แทนที่ alert priority ต่ำสุด ส่วน priority 0 อาจถูกทิ้ง |
| ปุ่มค้าง | active-low ต่อเนื่องเกิน 10 s | รับ long press เพียงครั้งเดียว รอปล่อยปุ่มก่อนรับ event ใหม่ และแสดงสัญลักษณ์ปุ่มผิดปกติ |

## ลำดับการแสดงเมื่อมีหลาย alert

1. เลือก `priority` 2 ก่อน 1 ก่อน 0 ตาม A1
2. priority เท่ากันให้รายการที่มาถึงก่อนแสดงก่อน; `ALERT_STAND_BREAK` แทน `ALERT_EYE_BREAK` ของ session เดียวกันเพราะครอบคลุมการพักที่มากกว่า
3. `ALERT_CLEAR` ลบเฉพาะรายการที่ `source` และ `type` ตรงกัน
4. alert ที่ถูกแทรกยังคงอยู่ใน active set และกลับมาแสดงเมื่อรายการสำคัญกว่าถูก clear
5. buzzer ส่งเสียงเฉพาะตอน alert กลายเป็นรายการที่แสดง เพื่อลดเสียงซ้ำจากการวาดจอทุก 500 ms

## เช็กลิสต์ก่อนส่ง

- [x] ทุกสถานะมีทางเข้าและทางออก รวม recovery จาก sensor fault
- [x] ค่า 20 min, 30 min, 3 min และ 5 min มีที่มา/สถานะการยืนยัน
- [x] ชื่อข้อมูลตรงกับ A1 v0.1 โดยไม่แก้ contract
- [x] มี fail-safe ที่ไม่ปิดไฟใส่คน
- [x] ระบุนโยบาย alert ซ้อน, ปุ่ม, stale data และ I²C mutex

## อ้างอิง

[B2-1] C. Talens-Estarelles et al., “The effects of breaks on digital eye strain, dry eye and binocular vision: Testing the 20-20-20 rule,” *Contact Lens and Anterior Eye*, 2022, Art. no. 101744, doi: 10.1016/j.clae.2022.101744.

[B2-2] P. Wennberg et al., “Acute effects of breaking up prolonged sitting on fatigue and cognition: A pilot study,” *BMJ Open*, vol. 6, no. 2, Art. no. e009630, 2016, doi: 10.1136/bmjopen-2015-009630.

[B2-3] เอกสารประกอบวิชา 010123119, บท 3 หัวข้อ 3.5 หน้า 30–38 (Timed FSM), บท 16 หน้า 9 (mutex), หน้า 15–21 (OS), และหน้า 30–34 (real-time).
