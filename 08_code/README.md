# 08_code — โค้ดจริงและวงจรจำลอง Wokwi (A4)

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.0 (ทดสอบแล้ว ผลใน `06_testing/A5_results.md`) |
| วันที่ | 2026-10-14 |
| เจ้าของ | A |
| อิงจาก | A1 v1.0, A2, A3, B2 v1.2 (PR #10), C2 v2 (PR #11), D3 v1, B3 |
| คอมไพล์ผ่าน | Arduino-ESP32 core 3.0.7 (ทดสอบแล้ว) · โค้ดรองรับ 2.0.17 ผ่าน wrapper PWM ใน `config.h` |

```text
08_code/
├── README.md              ← ไฟล์นี้
├── SIMULATION_GUIDE.md    ← แผนการจำลอง
├── desk_node/             ← โหนดโต๊ะ
│   ├── sketch.ino         ← สร้าง task, ข้อมูลกลาง, คำสั่งทดสอบ, สถิติ (A)
│   ├── config.h           ← ขา, ค่าเกณฑ์ D1, รอบ/priority ของ task
│   ├── shared.h           ← ตารางข้อมูลกลาง A1 (air_status_t, alert_request_t)
│   ├── presence.cpp/.h    ← B2: ตรวจคน, เตือนพัก, ปุ่ม + เมนูตั้งค่า (B)
│   ├── display.cpp/.h     ← B2: จอ OLED, buzzer, คิวการเตือน (B)
│   ├── light.cpp/.h       ← D3: คุมความสว่าง + สีแสง (D)
│   ├── comm.cpp/.h        ← A3: Wi-Fi, MQTT, NTP (A)
│   ├── diagram.json       ← วงจร Wokwi
│   ├── wokwi.toml, build/ ← สำหรับ Wokwi for VS Code
│   └── libraries.txt      ← สำหรับเว็บ wokwi.com
├── desk_node_2/           ← โต๊ะตัวที่ 2 (DESK_ID 2) ใช้กับ TC-A-06
├── ci/                    ← ทดสอบอัตโนมัติด้วย Wokwi CLI (ดู ci/README.md)
├── open_wokwi.bat         ← เปิด VS Code ทุกโหนด
└── room_node/             ← โหนดห้อง
    ├── sketch.ino, config.h, shared.h
    ├── air.cpp/.h         ← C2: อากาศ, พัดลม, ไฟสถานะ (C)
    ├── comm.cpp/.h        ← A3: Wi-Fi, MQTT, ตารางโต๊ะ (A)
    ├── diagram.json
    └── libraries.txt
```

---

## 1. เปิดใน Wokwi

### วิธีที่ 1 (แนะนำ ไม่ต้องคัดลอกไฟล์): Wokwi for VS Code

1. ติดตั้ง VS Code และส่วนขยาย **Wokwi for VS Code** (`wokwi.wokwi-vscode`)
2. ใน VS Code กด **F1 → Wokwi: Request a new License** แล้วกดรับ license ฟรีในเว็บ (ทำครั้งเดียว ต่ออายุทุก 30 วัน)
3. ดับเบิลคลิก `08_code/open_wokwi.bat` → เปิด VS Code 2 หน้าต่าง (โหนดห้อง + โหนดโต๊ะ) · ถ้าต้องการโต๊ะตัวที่ 2 ให้รัน `open_wokwi.bat 2`
4. ในแต่ละหน้าต่างกด **F1 → Wokwi: Start Simulator** · Serial Monitor อยู่แท็บ Terminal พิมพ์คำสั่งได้เลย

แต่ละโฟลเดอร์มี `wokwi.toml` + `diagram.json` + โปรแกรมที่คอมไพล์แล้วใน `build/` (ESP32 Arduino core 3.0.7) ถ้าแก้โค้ดต้องคอมไพล์ใหม่ (`arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir build .`) · `build/` ไม่ถูก commit (อยู่ใน `.gitignore`)

### วิธีที่ 2: เว็บ wokwi.com (คัดลอกทีละไฟล์)


1. https://wokwi.com → **New Project → ESP32 → Arduino (ESP32)**
2. แท็บ `diagram.json` → ลบของเดิม วางเนื้อหา `diagram.json` ของโหนดนั้นทั้งไฟล์
3. แท็บ `sketch.ino` → วางเนื้อหา `sketch.ino`
4. ปุ่ม **▾ ข้างแท็บไฟล์ → New file** สร้างไฟล์ที่เหลือ **ชื่อตรงตัว** แล้ววางเนื้อหา (`.h` และ `.cpp` ทุกไฟล์ + `libraries.txt`)
5. กด ▶ ครั้งแรกจะดาวน์โหลดไลบรารีจาก `libraries.txt` เอง
6. **Save** แล้วตั้งชื่อ `swer-desk-node` / `swer-room-node` และตั้งเป็น public เพื่อใส่ลิงก์ในเล่มหัวข้อ 10

> รัน 2 โหนดพร้อมกัน: เปิด **2 หน้าต่างเบราว์เซอร์วางข้างกัน** (ไม่ใช่ 2 แท็บ) เบราว์เซอร์ลดความเร็วแท็บที่ซ่อนอยู่
> ทั้ง 2 โหนดต้องใช้ `TOPIC_PREFIX` เดียวกันใน `config.h` (ตอนนี้ `swer-7k3q/r1/`)

### ค่าที่ปรับได้ใน `config.h`

| ค่า | ค่าเริ่มต้น | ใช้ทำอะไร |
|---|---|---|
| `TIME_SCALE` | 60 | 1 นาทีของระบบ = 1 วินาทีจริง (เตือนพักสายตาที่ 20 วินาที) · ตั้ง 1 เพื่อรันเวลาจริง · **ไม่เร่ง** รอบของ task (100 ms, 200 ms, 5 s) |
| `DESK_ID` | 1 | เปลี่ยนเป็น 2 สำหรับโต๊ะตัวที่ 2 (TC-A-06) |
| `SIMULATION` | 1 | 0 = โค้ดสำหรับฮาร์ดแวร์จริง (BH1750, SCD41 ทาง I²C) |

### อุปกรณ์ในวงจรจำลอง

| โหนด | ชิ้นส่วน | ใช้ทำอะไร |
|---|---|---|
| โต๊ะ | **slide switch** (GPIO 13) | แทนขา OUT ของ LD2410: ขวา = มีคน, ซ้าย = ไม่มีคน · ใช้สวิตช์แทน PIR เพราะ PIR ใน Wokwi ให้ HIGH แค่ช่วงสั้นหลังคลิก จำลอง "นั่งนิ่ง 20 นาที" ไม่ได้ ส่วน LD2410 จริงให้ระดับค้างตามการมีคน |
| โต๊ะ | photoresistor (GPIO 34) | แสงภายนอก (คลิกแล้วเลื่อน lux) ค่าเริ่ม 200 lux · โค้ดบวกแสงจาก LED ให้เอง 6 lux ต่อ 1% |
| โต๊ะ | ปุ่มสีน้ำเงิน (GPIO 27, คีย์บอร์ด **B**) | สั้น = รับทราบ / เพิ่มค่า · ค้าง ≥ 1.5 s = เลื่อนเตือน / เข้าเมนู / ถัดไป |
| โต๊ะ | LED ขาว (18) / เหลือง (19) | แถบ LED ขาวเย็น / ขาวอุ่น |
| ห้อง | potentiometer (GPIO 35) | CO₂ 400–2,000 ppm (หรือพิมพ์ `co2 <ppm>` ให้ค่าตรงตัว) |
| ห้อง | DHT22 (GPIO 4) | อุณหภูมิ/ความชื้น (คลิกแล้วเลื่อน) |
| ห้อง | LED น้ำเงิน (25) / RGB | พัดลม (ความสว่าง = ความแรง) / ไฟสถานะ |
| ทั้งคู่ | logic analyzer | บันทึก PWM → หยุดการจำลองแล้วได้ไฟล์ `.vcd` เปิดใน PulseView |

### คำสั่งใน Serial Monitor

| โหนดโต๊ะ | โหนดห้อง |
|---|---|
| `help`, `status`, `stats` | `help`, `status`, `stats` |
| `wifi off` / `wifi on` | `wifi off` / `wifi on` |
| `fault ld2410` / `ok ld2410` | `fault scd41` / `ok scd41` |
| `fault bh1750` / `ok bh1750` | `desks <online> <occupied>` / `desks auto` (ทดสอบ C เดี่ยวๆ) |
| | `co2 <ppm>` / `co2 pot` (ตั้งค่า CO₂ ตรงตัว / กลับไปใช้ pot) |
| `away` (เลื่อนสวิตช์ไปซ้ายก่อน) | |
| `time 16:55` / `time auto` | |

---

## 2. ขั้นตอนทดสอบด้วยมือ (ตาม C5 v2)

ผลการทดสอบอัตโนมัติทุกขั้นอยู่ที่ `06_testing/A5_results.md` (หลักฐานใน `06_testing/evidence/`) · ตารางนี้ใช้ทดสอบซ้ำเองใน VS Code หรือตอนอัดวิดีโอ
ค่าเวลาในตารางเป็นเวลาจริงที่ `TIME_SCALE 60`

| ขั้น | test case | ทำอะไร | ดูอะไร |
|---|---|---|---|
| 1 | — | เปิดโหนดห้อง แล้วโหนดโต๊ะ | log `MQTT connected`, จอโต๊ะขึ้น CO₂ ภายใน 10 s, มุมขวาบน `LINK` |
| 2 | TC-B-01..02, NFR-04 | เลื่อนสวิตช์ไปขวา (มีคน) แล้วรอ | 20 s → `EYE BREAK` + เสียง, 30 s → `STAND UP` |
| 3 | TC-B-04, FR-05 | ตอนมีเตือน กดปุ่มค้าง 3 ครั้ง (รอเตือนกลับทุกครั้ง ~5 s) | ครั้งที่ 3 จอขึ้น `max 2 snoozes` |
| 4 | TC-B-05..06, FR-07/10/11 | ไม่มีเตือน: ค้างปุ่ม → กดสั้น 3 ครั้ง (800 lux) → ค้าง → กดสั้น (Late ON) → ค้าง · แล้วเลื่อนสวิตช์ไปซ้าย รอ 5 s | ค่าเปลี่ยนบนจอ · หลังไม่มีคน 5 นาทีระบบ ค่ากลับ 500 / OFF, log `away_timeout=1` |
| 5 | TC-D-01, NFR-02 | มีคน, ลด photoresistor จาก 200 → 50 lux | `status` → `lux_now` กลับเข้า 500 ± 50 ภายใน 10 s, LED สว่างขึ้น |
| 6 | TC-D-02, NFR-10 | `time 16:55` แล้วรอ ~2 นาที | `status` → `r` ลดจาก 0.80 ทีละน้อย (ไม่เกิน 1%/นาทีระบบ) |
| 7 | TC-D-03, FR-08 | สวิตช์ไปซ้าย รอ 5 s | log `D: REGULATING -> DIMMING` (หรี่ 50%) ค้าง 60 s แล้ว `-> OFF` · เลื่อนกลับขวาระหว่างหรี่ → กลับ REGULATING |
| 8 | TC-D-06, NFR-07 | `fault bh1750` / `ok bh1750` | ไฟค้าง 60% + เตือน 10 s แล้วเป็นแถบล่าง · กลับปกติหลัง 2 รอบ |
| 9 | TC-C-01..03, FR-13 | ห้อง: `co2 1050` → รอ 5 s → `co2 920` → รอ 5 s → `co2 750` | พัดลม 100% แดง → 60% เหลือง → 0% เขียว · จอโต๊ะตาม |
| 10 | TC-C-04, FR-15 | สวิตช์โต๊ะไปซ้าย | โหนดห้อง `room_occupied_count=0` → `S0_ROOM_EMPTY` พัดลมดับ |
| 11 | TC-C-05 | ห้อง: `fault scd41` | 15 s → `S4_SENSOR_FAULT` พัดลม 80% แดงกะพริบ · จอโต๊ะ `SENSOR ERR` |
| 12 | TC-C-06, FR-17 | DHT22 ตั้ง 27°C | จอโต๊ะ `ROOM TEMP` |
| 13 | TC-A-03, NFR-06 | โต๊ะ: `wifi off` 30 s แล้ว `wifi on` | `L:--` + `No air data` แต่ไฟ ตรวจคน เตือนพักยังทำงาน · ต่อกลับเอง |
| 14 | TC-A-04 | ปิดหน้าต่างโหนดโต๊ะ (CO₂ ≥ 1,000) | ห้อง: `room_desks_online=0` แต่พัดลม**ยังเปิด** |
| 15 | TC-A-06, NFR-13 | เปิดโหนดโต๊ะตัวที่ 2 (`DESK_ID 2`) | ห้อง: `room_desks_online=2` โดยไม่แก้โค้ดห้อง |
| 16 | TC-A-07, NFR-05 | ปล่อยรัน 10 นาทีแล้ว `stats` | WCET, utilization, `late = 0`, stack ที่เหลือ → ส่งให้ B แทนค่าประมาณใน B3 |

---

## 3. หมายเหตุถึงเจ้าของโมดูล (ตรวจว่าโค้ดตรงกับที่ออกแบบ)

| โมดูล | ที่ A ทำ | ต่างจากเอกสารไหม |
|---|---|---|
| B (presence.cpp) | การเปลี่ยน `UNOCCUPIED ↔ WORKING ↔ AWAY_PENDING` ทำใน `presence_task` (100 ms) ทันทีหลังกรอง 1 s | B2 วางไว้ใน `break_timer_task` (1 s) · ย้ายเพื่อให้ `occupied` ถึง D ภายใน ≤ 1.1 s ทัน NFR-03 · FSM เหมือนเดิม |
| B (display.cpp) | เตือนจากคน (พักสายตา/ลุก) และ priority 2 เต็มจอจนกว่าจะหาย · เตือนอื่น (อากาศ, ลิงก์, เซนเซอร์แสง) เต็มจอ 10 s แล้วเป็นแถบล่าง | B2 ไม่ได้กำหนด · เสนอเพื่อให้เวลานั่งยังเห็นได้ · B ยืนยัน |
| B | ข้อความบน OLED เป็นภาษาอังกฤษ | ฟอนต์ของ Adafruit GFX ไม่มีอักษรไทย |
| C (air.cpp) | ออกจาก SENSOR_FAULT เมื่ออ่านสำเร็จ **2 ครั้งติดกัน** | ตามตารางเปลี่ยนสถานะ C2 (pseudocode ของ C2 ออกเมื่อสำเร็จ 1 ครั้ง) · C เลือกให้ตรงกันใน C2 |
| D (light.cpp) | ไม่ล้าง `user_lux_target`/`late_mode` เอง (B ทำ) · fade เป็น ramp ในซอฟต์แวร์ทุก 200 ms | ตาม A1 v1.0 Q6 · ผลเท่ากับ LEDC hardware fade |
| B (presence.cpp) | debounce ปุ่ม: ทุกขอบเริ่มนับเงียบ 30 ms ใหม่ แล้วอ่านระดับขาที่นิ่ง (แก้จาก A5 bug 1) | B2 กำหนด debounce 30 ms แต่ไม่ได้ระบุวิธี · วิธีเดิมทิ้งการกดจริงเมื่อปุ่มเด้ง |
| B (presence.cpp) | ถ้ามี STAND ที่ snooze รออยู่ EYE ที่ครบกำหนดระหว่างนั้นถูกครอบไป (แก้จาก A5 bug 2) | ขยายกฎ "STAND ครอบ EYE" ของ B2 ให้รวมช่วง snooze · B ยืนยัน |
| B (display.cpp) | หัวข้อเตือนเซนเซอร์เสียแสดง `SENSOR ERR` | `SENSOR FAULT` ยาวเกินจอ 128 px (A5 bug 4) · ชื่อใน log ยังเป็น `SENSOR FAULT` |
| D (light.cpp) | สูตรแปลง LDR จำลองใช้ไฟ 3.3 V: `R = 10k·V/(3.3−V)` | เฉพาะ `SIMULATION 1` (A5 bug 5) ฮาร์ดแวร์จริงอ่าน BH1750 |
| C (air.cpp) | เรียก `comm_update_counts()` ก่อนตัดสิน FSM ทุกรอบ | ตัวนับโต๊ะสดในรอบเดียวกัน ไม่ช้า 5 s (A5 bug 8) · FSM เหมือนเดิม |
| A (comm.cpp) | งานสื่อสารอยู่ **core 0** กับ Wi-Fi และตื่นทุก 100 ms เพื่อบริการ socket MQTT (งานจริงเกิดทุก 5 s หรือเมื่อ `occupied` เปลี่ยน) | A-T v1.1 อัปเดตแล้ว พร้อม WCET ที่วัดได้ · core 1 เหลือเฉพาะงานควบคุม |
| A (config.h) | PWM ทุกขาใช้ LEDC channel คู่ (0, 2, 4, …) = มี timer ของตัวเอง · MQTT keepalive 60 s เมื่อ `SIMULATION 1` (จริง 15 s) | channel ติดกันใช้ timer ร่วมกัน ทำให้ duty พัดลมและไฟผิด (A5 bug 3) · การจำลองช้ากว่าเวลาจริง (A5 bug 9) |

## 4. ข้อจำกัด

ดู `09_doc_sections/A-N_notes.md` ข้อ 2 · เพิ่มเติม: สวิตช์แทน LD2410 ทดสอบได้แค่ logic, MQTT ผ่าน broker สาธารณะ `broker.hivemq.com` (ถ้า broker ล่มให้เปลี่ยน `MQTT_HOST` เป็น `test.mosquitto.org` ทั้ง 2 โหนด)
