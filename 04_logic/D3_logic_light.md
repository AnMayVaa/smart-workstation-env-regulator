# D3 Logic ของโมดูลแสง

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.1 |
| วันที่ | 2026-10-14 |
| แก้ไขใน v1.1 | ตัด `reset_user_settings()` ออก B เป็นคนคืนค่าที่ผู้ใช้ปรับตาม A1 v1.0 ข้อ 3, อัปเดตตารางข้อมูลตาม A1 v1.0 (Issue #7) |
| เจ้าของ | D |
| ส่งให้ | A (เขียนโค้ด A4) |
| อ้างอิง | `01_requirements/D1_requirements.md` (FR-06–FR-11, NFR-02, NFR-03, NFR-07, NFR-09–NFR-11), `02_interface/A1_interface.md`, `03_hardware/D2_hardware_light.md`, `05_tasks/D-T_tasks.md` |
| ภาพ FSM | `07_visuals/D3_fsm_light.png` (แก้ไขได้ที่ `07_visuals/D3_fsm_light.drawio`) |

> **ภาพรวม:** โมดูลแสงมี 2 ส่วนที่ทำงานพร้อมกัน (แบบ AND super-state บท 3 หน้า 49–78)
> **ส่วน A — สถานะไฟ:** ตัดสินว่าไฟควรเปิด หรี่ หรือปิด และคุมความสว่างรวม (B) แบบป้อนกลับให้ได้ค่าเป้าหมาย
> **ส่วน B — สีแสง:** ตัดสินสัดส่วนแสงขาวเย็น (r) ตามเวลา และโหมดทำงานดึก
> สุดท้ายรวมเป็น duty ของ 2 ช่อง: **duty_CW = B × r** และ **duty_WW = B × (1 − r)**
> ทั้งหมดทำใน `light_control_task` งานเดียว ทุก 200 ms

---

## 1. สถานะทั้งหมด

### ส่วน A — สถานะไฟ

| สถานะ | ความหมาย | ไฟทำอะไร |
|---|---|---|
| **S0 OFF** | ไม่มีคน (หรือเพิ่งเปิดเครื่อง) | ปิด (B = 0%) |
| **S1 REGULATING** | มีคนนั่ง เซนเซอร์ปกติ | วงควบคุมแบบป้อนกลับปรับ B ให้แสงบนโต๊ะได้ค่าเป้าหมาย |
| **S2 DIMMING** | ไม่มีคนครบ 5 นาที (`away_timeout` = true) | หรี่เหลือ 50% ของ B เดิม ค้างไว้ 60 วินาที แล้วค่อยๆ ปิด |
| **S3 SENSOR_FAULT** | อ่าน BH1750 ไม่ได้ 3 รอบติดกัน | ค้างไฟที่ B = 60% (ไม่ใช้วงป้อนกลับ) และส่งคำเตือนขึ้นจอ |

### ส่วน B — สีแสง (r = สัดส่วนแสงขาวเย็น)

| สถานะ | เงื่อนไข | r เป้าหมาย | อุณหภูมิสีโดยประมาณ |
|---|---|---|---|
| **C0 NEUTRAL** | `time_valid` = false (ยังไม่ได้เวลาจาก NTP) | 0.50 | ~4,000 K |
| **C1 DAY** | 07:00–16:59 | 0.80 | ~5,000–5,500 K |
| **C2 TRANSITION** | 17:00–18:59 | ลดลงเป็นเส้นตรงจาก 0.80 → 0.00 ใน 120 นาที | ลดลงเรื่อยๆ |
| **C3 EVENING** | 19:00–06:59 | 0.00 | ~2,700–3,000 K |
| **C4 LATE_MODE** | `late_mode` = true (ผู้ใช้เปิดเอง) | 0.80 (ใช้สีกลางวันต่อ) | ~5,000–5,500 K |

---

## 2. ตารางเปลี่ยนสถานะ

### ส่วน A — สถานะไฟ

| จากสถานะ | ไปสถานะ | เงื่อนไข | สิ่งที่ระบบทำ |
|---|---|---|---|
| (เปิดเครื่อง) | S0 OFF | — | B = 0, ตั้ง BH1750 เป็นโหมดวัดต่อเนื่อง (H-Resolution), ตั้ง LEDC 5 kHz 13 บิต 2 ช่อง |
| S0 OFF | S1 REGULATING | `occupied` = true | ค่อยๆ เปิดไฟจาก 0 → B_start ใน 2 วินาที (B_start = B ครั้งล่าสุดที่ใช้ ถ้าไม่มีใช้ 50%) |
| S1 REGULATING | S1 REGULATING | ทุก 200 ms | อ่าน `lux_now` → ปรับ B ตามวงป้อนกลับ (ข้อ 4) |
| S1 REGULATING | S2 DIMMING | `away_timeout` = true | จำค่า B ไว้ (B_saved), หรี่เหลือ B_saved × 0.5 ใน 2 วินาที, เริ่มนับ 60 วินาที (ค่าที่ผู้ใช้ปรับ B เป็นคนคืนเป็นค่าเริ่มต้นเองเมื่อ `away_timeout` เป็นจริง ตาม FR-11 และ A1 ข้อ 3 D อ่านอย่างเดียว) |
| S2 DIMMING | S1 REGULATING | `occupied` = true (คนกลับมาก่อนดับ) | ค่อยๆ กลับไปที่ B_saved ใน 2 วินาที แล้วใช้วงป้อนกลับต่อ |
| S2 DIMMING | S0 OFF | ค้างครบ 60 วินาที | ค่อยๆ ปิดเหลือ 0 ใน 5 วินาที |
| S1 REGULATING | S3 SENSOR_FAULT | อ่านค่าไม่ได้ 3 รอบติดกัน (รวมกรณีรอ `i2c_lock` เกิน 50 ms) | ค่อยๆ ไปที่ B = 60% ใน 2 วินาที, ส่ง `alert_request` {SRC_LIGHT, ALERT_SENSOR_FAULT, priority 1, ALERT_SHOW} |
| S3 SENSOR_FAULT | S1 REGULATING | อ่านค่าได้ปกติ 2 รอบติดกัน | ส่ง `alert_request` {…, ALERT_CLEAR}, กลับใช้วงป้อนกลับ |
| S3 SENSOR_FAULT | S2 DIMMING | `away_timeout` = true | เหมือนแถว S1 → S2 (หรี่จาก 60%) |

> **ข้อสังเกต:** ถ้าคนลุกไปไม่ถึง 5 นาที (`occupied` = false แต่ `away_timeout` = false) ไฟ **ยังอยู่ที่ S1** ไม่หรี่ เพราะ B เป็นคนนับเวลาและตัดสิน `away_timeout` (ตกลงใน A1 Q1) และถ้าเซนเซอร์คนของ B เสีย B จะตั้ง `occupied` = true และ `away_timeout` = false ไฟจึงไม่ดับใส่คน

### ส่วน B — สีแสง

| จากสถานะ | ไปสถานะ | เงื่อนไข | สิ่งที่ระบบทำ |
|---|---|---|---|
| (เปิดเครื่อง) | C0 NEUTRAL | — | r = 0.50 |
| C0 NEUTRAL | C1 / C2 / C3 | `time_valid` เปลี่ยนเป็น true | เลือกสถานะตามเวลา และค่อยๆ ขยับ r ไปหาเป้าหมาย (ไม่เกิน 1% ต่อนาที) |
| C1 DAY | C2 TRANSITION | เวลา = 17:00 | r ลดลงตามเส้นตรง (≈ 0.67% ต่อนาที) |
| C2 TRANSITION | C3 EVENING | เวลา = 19:00 | r = 0.00 |
| C3 EVENING | C1 DAY | เวลา = 07:00 | ค่อยๆ ขยับ r ขึ้นไป 0.80 (ไม่เกิน 1% ต่อนาที) |
| C1 / C2 / C3 / C0 | C4 LATE_MODE | `late_mode` = true | ขยับ r ไป 0.80 ใน **10 วินาที** (เป็นคำสั่งผู้ใช้ จึงเปลี่ยนเร็วกว่าแบบอัตโนมัติ แต่ไม่กระชาก ตาม NFR-11) |
| C4 LATE_MODE | สถานะตามเวลา | `late_mode` = false | ขยับ r กลับไปตามเวลาใน 10 วินาที |

---

## 3. ภาพ FSM

ดู `07_visuals/D3_fsm_light.png` (ซ้าย: สถานะไฟ / ขวา: สีแสง ทำงานพร้อมกัน)

---

## 4. Pseudocode

```text
// ===== ค่าคงที่ (ที่มาดูข้อ 5) =====
CONST LUX_DEFAULT      = 500     // lux, FR-06
CONST LUX_USER_MIN     = 500     // lux, FR-07
CONST LUX_USER_MAX     = 1000    // lux, FR-07
CONST LUX_DEADBAND     = 25      // lux, ±25 อยู่ในช่วงยอมรับ ±50
CONST K_P              = 0.02    // % ต่อ lux (A จูนได้ในการจำลอง)
CONST STEP_MAX         = 2.0     // % ต่อรอบ 200 ms
CONST B_START_DEFAULT  = 50.0    // %
CONST B_FAULT          = 60.0    // %
CONST DIM_RATIO        = 0.5
CONST DIM_HOLD_MS      = 60000
CONST R_DAY = 0.80, R_EVENING = 0.00, R_NEUTRAL = 0.50, R_LATE = 0.80
CONST R_RATE_AUTO      = 0.01    // ต่อนาที (NFR-10)
CONST FADE_ON_MS = 2000, FADE_OFF_MS = 5000, FADE_USER_MS = 10000
CONST PWM_FREQ = 5000, PWM_BITS = 13          // NFR-09
CONST FAULT_COUNT = 3, RECOVER_COUNT = 2      // NFR-07

STATE light_state = S0_OFF
FLOAT B = 0, B_saved = 0, B_last = B_START_DEFAULT
FLOAT r = R_NEUTRAL
INT   err_count = 0, ok_count = 0

TASK light_control_task (period = 200 ms):
  LOOP FOREVER:
    // 1) อ่านข้อมูลกลางแบบ snapshot
    snap = read_snapshot(occupied, away_timeout, time_of_day, time_valid,
                         user_lux_target, late_mode)

    // 2) อ่านเซนเซอร์แสง (ห้ามรอ lock เกิน 50 ms)
    IF take(i2c_lock, timeout = 50 ms) THEN
        ok = bh1750_read_lux(&lux)        // อ่านผลล่าสุด 2 ไบต์ ไม่ต้องรอ 120 ms
        give(i2c_lock)
    ELSE
        ok = FALSE                        // ข้ามรอบนี้ นับเป็นความผิดพลาด (A1 ข้อ 7)
    END IF
    IF ok THEN lux_now = lux; err_count = 0; ok_count += 1
    ELSE err_count += 1; ok_count = 0
    END IF

    // 3) ความสว่างเป้าหมาย (ตรวจช่วงค่าที่ผู้ใช้ป้อน NFR-15)
    target = clamp(snap.user_lux_target, LUX_USER_MIN, LUX_USER_MAX)
    IF snap.user_lux_target ไม่มีค่า THEN target = LUX_DEFAULT

    // 4) FSM ส่วน A: สถานะไฟ
    SWITCH light_state:
      CASE S0_OFF:
        IF snap.occupied THEN
            fade_B_to(B_last, FADE_ON_MS); light_state = S1_REGULATING
      CASE S1_REGULATING:
        IF snap.away_timeout THEN enter_dimming()
        ELSE IF err_count >= FAULT_COUNT THEN
            fade_B_to(B_FAULT, FADE_ON_MS)
            send_alert(SRC_LIGHT, ALERT_SENSOR_FAULT, 1, ALERT_SHOW)
            light_state = S3_SENSOR_FAULT
        ELSE IF ok THEN
            error = target - lux_now
            IF abs(error) > LUX_DEADBAND THEN
                B = clamp(B + clamp(K_P * error, -STEP_MAX, +STEP_MAX), 0, 100)
            END IF
            B_last = B
      CASE S2_DIMMING:
        IF snap.occupied THEN
            fade_B_to(B_saved, FADE_ON_MS); light_state = S1_REGULATING
        ELSE IF dim_timer_elapsed() >= DIM_HOLD_MS THEN
            fade_B_to(0, FADE_OFF_MS); light_state = S0_OFF
      CASE S3_SENSOR_FAULT:
        IF snap.away_timeout THEN enter_dimming()
        ELSE IF ok_count >= RECOVER_COUNT THEN
            send_alert(SRC_LIGHT, ALERT_SENSOR_FAULT, 1, ALERT_CLEAR)
            light_state = S1_REGULATING
    END SWITCH

    // 5) FSM ส่วน B: สีแสง
    IF snap.late_mode THEN r_target = R_LATE;   rate = per_fade(FADE_USER_MS)
    ELSE IF NOT snap.time_valid THEN r_target = R_NEUTRAL; rate = R_RATE_AUTO
    ELSE r_target = schedule(snap.time_of_day); rate = R_RATE_AUTO
    r = move_toward(r, r_target, rate)          // จำกัดความเร็วการเปลี่ยน

    // 6) รวมเป็น duty แล้วสั่ง LEDC (ใช้ hardware fade 200 ms ให้เนียน)
    ledc_fade_to(CH_CW, to_counts(B * r),       200 ms)
    ledc_fade_to(CH_WW, to_counts(B * (1 - r)), 200 ms)

    wait_until_next_period(200 ms)
  END LOOP

FUNCTION enter_dimming():
  B_saved = B
  fade_B_to(B * DIM_RATIO, FADE_ON_MS)
  // ไม่ล้างค่าที่ผู้ใช้ปรับในฝั่ง D: B คืนค่า user_lux_target และ late_mode เอง (A1 ข้อ 3, FR-11)
  start_dim_timer()
  light_state = S2_DIMMING

FUNCTION schedule(t):            // ตารางสีแสงใน D1
  IF 07:00 <= t < 17:00 THEN RETURN R_DAY
  IF 17:00 <= t < 19:00 THEN RETURN R_DAY * (1 - minutes_since(17:00, t) / 120)
  RETURN R_EVENING

FUNCTION to_counts(pct):         // 13 บิต: 0–8191
  RETURN round(pct / 100 * 8191)
```

**เรื่องที่ A ต้องรู้ตอนเขียนโค้ด**
- `fade_B_to()` คือการเปลี่ยน B แบบค่อยเป็นค่อยไปในเวลาที่กำหนด ใช้ hardware fade ของ LEDC ได้ (`ledc_set_fade_with_time()`) ไม่ต้องสร้าง task เพิ่ม
- **ค่าที่ผู้ใช้ปรับ (`user_lux_target`, `late_mode`):** ตัดสินแล้วใน A1 v1.0 ข้อ 3 ว่า B เป็นเจ้าของคนเดียวและเป็นคนคืนค่าเมื่อ `away_timeout` เป็นจริง D อ่านอย่างเดียว ไม่เขียนกลับ (เดิม D3 v1 มี `reset_user_settings()` ในฝั่ง D ตัดออกแล้ว)
- **ในการจำลอง Wokwi:** photoresistor มองไม่เห็นแสงจาก LED ให้คิด `lux_now = lux_จากตัวเลื่อน + 6 × B` (ไฟเต็ม 100% ให้แสงเพิ่ม 600 lux ตามที่ประมาณใน D2) เพื่อให้วงป้อนกลับทำงานได้

---

## 5. ค่าที่ใช้และที่มา

| ค่า | ค่าที่ใช้ | ที่มา / เหตุผล |
|---|---|---|
| ความสว่างเป้าหมาย | 500 lux (ผู้ใช้ปรับได้ 500–1,000) | D1 FR-06, FR-07, EN 12464-1:2021 |
| ช่วงยอมรับ (deadband) | ±25 lux | อยู่ภายในเกณฑ์ ±50 lux ของ D1 กันไฟปรับขึ้นลงไม่หยุดจากค่าที่แกว่งเล็กน้อย และกว้างกว่าความละเอียดเซนเซอร์ (1 lux) มาก |
| ปรับ B สูงสุดต่อรอบ | 2% ต่อ 200 ms | จาก 0 ถึง 100% ใช้ 10 วินาที ทัน NFR-02 (กลับเข้าเกณฑ์ภายใน 10 วินาที) และไม่ให้ผู้ใช้เห็นไฟกระตุก |
| K_P | 0.02 % ต่อ lux | ค่าเริ่มต้น: error 100 lux → ปรับ 2% (เท่าค่าสูงสุดต่อรอบ) A จูนในการจำลองได้ |
| รอบการทำงาน | 200 ms | ยาวกว่าเวลาวัดสูงสุดของ BH1750 (180 ms) ทุกรอบได้ค่าใหม่ (D-T) |
| หรี่เมื่อไม่มีคน | 50% แล้วค้าง 60 วินาที ก่อนปิดใน 5 วินาที | D1 FR-08 และ ASHRAE 90.1 (หรี่ 50% เมื่อไม่มีคน) / 60 วินาทีเป็นค่าที่ D ตั้ง ให้เวลาคนที่กลับมาทันก่อนไฟดับ |
| ความถี่ PWM | 5 kHz, 13 บิต | D1 NFR-09, IEEE 1789-2015, เอกสาร LEDC ของ ESP32 |
| ไฟตอนเซนเซอร์เสีย | B = 60% | ค่าที่ D ตั้ง ให้แสงพอทำงานได้ (ไม่มืด) โดยไม่จ้าเกินในห้องที่มีแสงอื่นช่วย |
| นับว่าเสีย / หาย | เสีย 3 รอบติดกัน / ปกติ 2 รอบติดกัน | D1 NFR-07, รูปแบบเดียวกับโมดูลอากาศของ C |
| สัดส่วนสี r | กลางวัน 0.80 / ค่ำ 0.00 / ไม่รู้เวลา 0.50 / ทำงานดึก 0.80 | D1 ตารางค่าเกณฑ์, [4], [10] |
| ความเร็วเปลี่ยนสีอัตโนมัติ | ไม่เกิน 1% ต่อนาที (ช่วงเย็นจริงใช้ ≈ 0.67% ต่อนาที) | D1 NFR-10 |
| เวลาเปิด / ปิด / คำสั่งผู้ใช้ | 2 / 5 / 10 วินาที | D1 NFR-11 (ไม่น้อยกว่า 2 วินาที) และ NFR-03 (ไฟเริ่มสว่างภายใน 1 วินาที — เริ่ม fade ทันทีในรอบถัดไป ≤ 200 ms) |

---

## 6. ข้อมูลรับเข้าและส่งออก (ตาม A1)

| ข้อมูล | ทิศทาง | จาก / ถึง | ใช้ทำอะไร |
|---|---|---|---|
| `occupied` | รับ | B | เปิดไฟเมื่อมีคน (S0 → S1, S2 → S1) |
| `away_timeout` | รับ | B | หรี่และปิดไฟ (S1/S3 → S2) |
| `time_of_day` | รับ | A | เลือกสีแสง |
| `time_valid` | รับ | A | ถ้า false ใช้สีกลาง (C0) |
| `user_lux_target` 🆕 | รับ | B | ความสว่างเป้าหมายที่ผู้ใช้ตั้ง (500–1,000) D จำกัดค่าให้อยู่ในช่วงเสมอ (NFR-15) · มีใน A1 v1.0 แล้ว |
| `late_mode` 🆕 | รับ | B | โหมดทำงานดึก · มีใน A1 v1.0 แล้ว |
| `lux_now` | สร้าง | D (ใช้เอง) | ค่าแสงบนโต๊ะ |
| `alert_request` | ส่ง | B (ผ่าน queue, `source` = `SRC_LIGHT`) | เตือนเมื่อเซนเซอร์แสงเสีย / เลิกเตือนเมื่อกลับมาปกติ |
| `i2c_lock` | ใช้ | A กำหนดกติกา | ขอก่อนอ่าน BH1750 รอไม่เกิน 50 ms |

---

## 7. กรณีผิดพลาด

| กรณี | ระบบทำอะไร | เหตุผล |
|---|---|---|
| BH1750 อ่านไม่ได้ / รอ `i2c_lock` ไม่ได้ 1–2 รอบ | ข้ามรอบนั้น ใช้ B เดิมต่อ | ไม่ตัดสินว่าเสียจากสะดุดครั้งเดียว |
| อ่านไม่ได้ 3 รอบติดกัน | S3: ไฟค้าง 60% + เตือนบนจอ | ล้มเหลวอย่างปลอดภัย ผู้ใช้ยังมีแสงทำงาน (บท 2 หน้า 27, บท 6 หน้า 37) และแจ้งให้รู้ (บท 6 หน้า 48) |
| อ่านค่าได้แต่ผิดปกติ (เช่น เกิน 65,535 หรือกระโดดเกินจริง) | ถือเป็นการอ่านไม่ได้ในรอบนั้น | บท 6 หน้า 46 (อย่าเชื่อ input ที่ผิดช่วง) |
| ยังไม่ได้เวลาจาก NTP (`time_valid` = false) | ใช้สีกลาง r = 0.50 | ไม่เสี่ยงทั้งด้านกลางวันและกลางคืน |
| Wi-Fi หลุดหลังเคยได้เวลาแล้ว | ใช้เวลาที่ ESP32 นับต่อเอง สีแสงทำงานปกติ | D1 NFR-06 |
| แสงภายนอกสว่างเกินเป้าหมายแม้ปิดไฟ | B ลดถึง 0% แล้วหยุด | ระบบทำให้มืดลงไม่ได้ (ข้อจำกัด เขียนในหัวข้อ 8) |
| `user_lux_target` นอกช่วง 500–1,000 | ปัดเข้าช่วง | D1 NFR-15 |
| เซนเซอร์คนของ B เสีย | B ตั้ง `occupied` = true, `away_timeout` = false → ไฟเปิดต่อ | ไม่ปิดไฟใส่คน (ตกลงใน A1 Q5) |

---

## ✅ เช็คก่อนส่ง (แบบฟอร์ม LOGIC)

- [x] สถานะทั้งหมด
- [x] ตารางเปลี่ยนสถานะ
- [x] ภาพ FSM (PNG + ไฟล์ draw.io)
- [x] Pseudocode
- [x] ค่าที่ใช้และที่มา
- [x] ข้อมูลรับเข้าและส่งออกตาม A1
- [x] กรณีผิดพลาด

## อ้างอิง

- เอกสารประกอบวิชา 010123119: บท 2 หน้า 27, บท 3 หน้า 30–38 (Timed FSM) และหน้า 49–78 (AND super-state), บท 6 หน้า 37, 46, 48, บท 13 หน้า 91–96
- [3] EN 12464-1:2021 · [4] Viola et al., 2008 · [10] Chang et al., 2015 · [11] IEEE Std 1789-2015 · [12] ASHRAE 90.1-2019 (เลขตาม D1)
- Espressif Systems, "LED Control (LEDC)," *ESP-IDF Programming Guide*.
