# B5 ตรวจเล่มก่อนส่งสำหรับบทบาท B

| รายการ | ค่า |
|---|---|
| วันที่ตรวจ | 2026-10-15 |
| ผู้ตรวจ | B |
| เล่มที่ตรวจ | `10_final/D5_design_document.docx` |
| branch | `B5_prefinal_review` |
| สถานะ | ตรวจส่วน B แล้ว พบ 2 จุดที่ต้องแก้ในเล่มและงานยืนยันก่อนส่ง |

## ขอบเขตการตรวจ

ตรวจหัวข้อที่ D5 มอบหมายให้ B ได้แก่ 4.2.2, 5.1.1, 5.2.1, 5.3 และ 6.3 รวมถึงตารางแผน/ผลทดสอบในหัวข้อ 7 ที่อ้าง TC-B โดยเทียบกับ source ล่าสุดต่อไปนี้

- `03_hardware/B1_hardware_presence.md`
- `04_logic/B2_logic_presence.md`
- `05_tasks/B3_task_table.md`
- `06_testing/B-TC_testcases.md`
- `06_testing/A5_results.md`
- `02_interface/A1_interface.md`
- `03_hardware/A2_board_power.md`

## ผลตรวจ branch

ตรวจ remote branch ทุก branch ยกเว้น `main` ณ วันที่ 2026-10-15 พบว่า `A_code_tests`, `A_integration`, `A_sim_guide`, `B_presence_tasks`, `D_pm_light` และ `feat/role-c-air-module` ไม่มี commit ที่ยังไม่อยู่ใน `main` ส่วน `D5_prefinal_review` มี commit เพิ่มเล่ม pre-final และ README จำนวน 1 commit ดังนั้น branch นี้จึงสร้างต่อจาก `origin/D5_prefinal_review` เพื่อให้มีทั้ง source ที่ merge แล้วและเล่มที่ต้องตรวจ

## รายการที่ถูกต้องแล้ว

| หัวข้อ | ผลตรวจ |
|---|---|
| 4.2.2 Hardware | ชนิดอุปกรณ์ ขา GPIO 13/16/17/21/22/26/27 และการใช้ slide switch แทน LD2410 ใน Wokwi ตรงกับ A2 v1.1 |
| 5.1.1 FSM | เกณฑ์ 1 s, 3 min, 5 min, fail-safe `occupied=true` และ reset ค่าผู้ใช้ตรงกับ B2 |
| 5.2.1 Pseudocode | การเตือน 20/30 min, snooze 5 min สูงสุด 2 ครั้ง และ FR-11 ตรงกับ B2 |
| 5.3 RTOS | คาบและ WCET รายงานตรงกับ A5 แต่การรวม utilization ข้าม core ต้องแก้ตามหัวข้อถัดไป |
| 6.3 Concurrency | `i2c_lock` 50 ms, alert queue, single writer/snapshot และผล latency 0.9 s ตรงกับ implementation/test; ตัวเลข utilization ต้องแยก core |
| ตาราง 7.1 | ความหมาย TC-B-01 ถึง TC-B-08 ตรงกับ `B-TC_testcases.md` |

## จุดที่ต้องแก้ในเล่ม

### ตาราง 7.3 ผลการทดสอบโหนดโต๊ะ

ตารางผลจับรหัส TC-B-01 และ TC-B-02 ไม่ตรงกับตารางแผน 7.1 และไฟล์ B-TC

| รหัส | ข้อความปัจจุบัน | ข้อความที่ควรใช้ |
|---|---|---|
| TC-B-01 | `presence on` แล้วเข้า WORKING | ทดสอบนั่งต่อเนื่องครบ 20 นาที แล้วเกิด `ALERT_EYE_BREAK`; OLED และ buzzer เริ่มเตือนภายใน 1 s |
| TC-B-02 | รวม EYE 20 นาทีและ STAND 30 นาที | ทดสอบนั่งครบ 30 นาทีแล้วเกิด `ALERT_STAND_BREAK`; หลังไม่พบคนต่อเนื่อง 3 นาที session และ due time เริ่มใหม่ |

ผล `UNOCCUPIED -> WORKING` หลัง 1.1 s เป็นหลักฐานของ FR-01 และ precondition ของชุดทดสอบ แต่ไม่ใช่ความหมายของ TC-B-01 ใน test plan จึงควรย้ายไปข้อความสังเกตหรือเพิ่มกรณีทดสอบใหม่ ไม่ควรใช้รหัส TC-B-01 ซ้ำ

ให้แก้ mapping เดียวกันใน `06_testing/A5_results.md` แถว D1–D2 ก่อน D สร้างเล่มรอบถัดไป หากยังไม่มีหลักฐานส่วน “reset หลังพัก 3 นาที” ของ TC-B-02 ให้ระบุว่าผ่านบางส่วน/รอทดสอบ แทนการสรุปว่าผ่านครบ

### ตาราง 5.5 และข้อความหัวข้อ 6.3

โค้ด A4 pin งานควบคุม B/C/D บน core 1 แต่ pin `desk_communication_task`, `room_communication_task` และ NTP บน core 0 ตาราง 5.5/A5 นำค่า C/T ของสอง core มาบวกเป็น 0.274 แล้วเทียบ RMS bound `n=6` ชุดเดียว จึงไม่ใช่การวิเคราะห์ schedulability ตาม core ที่รันจริง

ให้แก้ข้อความเป็นดังนี้

- งานควบคุมโต๊ะ core 1 จำนวน 5 งาน: `U = 0.268796` หรือ **26.88%**, RMS bound `n=5` = **74.35%**
- งานสื่อสารโต๊ะ core 0 ที่ instrument: `U = 0.005605` หรือ **0.56%**; ห้ามสรุปผ่าน RMS จากค่านี้เพียงค่าเดียวเพราะยังมี Wi-Fi/TCP-IP system tasks
- โหนดห้อง: `air_sensor_task` core 1 = **4.36%**, `room_communication_task` core 0 = **0.11%**
- คงข้อสรุปว่า deadline miss = 0 ในช่วงทดสอบประมาณ 45 s แต่ระบุว่ายังรอรัน 10 นาทีและฮาร์ดแวร์จริง

## Source ที่อัปเดตใน B5

- B1 เปลี่ยนตัวแทน LD2410 ใน Wokwi จาก PIR เป็น slide switch ให้ตรง A2 v1.1 และวงจรจริง
- B2 เพิ่มหมายเหตุแยก input จำลอง GPIO 13 ออกจาก UART2 ของฮาร์ดแวร์จริง
- B3 เพิ่ม WCET ที่วัดจาก A5 แยกตาม core, I²C blocking และขอบเขตของการวัด 45 s
- B4 เพิ่ม bug/fix ของ debounce, snooze collision และข้อความ OLED พร้อมผล timing จริง
- B-N ปิดประเด็น A1/C-T ที่แก้แล้ว บันทึก decision หลัง sensor fault และรายการแก้เล่ม TC-B

## การตัดสินใจของ B

1. เมื่อ LD2410 ฟื้นจาก fault ให้เริ่ม session ใหม่ตาม B2 เพราะช่วง fault ทำให้เวลานั่งไม่น่าเชื่อถือ ไม่แก้ให้ต่อเวลาเดิม
2. `button_event_task` ใช้ stack 3,072 B ในโค้ดล่าสุดตามข้อเสนอ A5 แต่ต้องรัน `stats` ซ้ำเพื่อยืนยัน high-water mark
3. B-TC เป็น source of truth ของความหมาย TC-B-01 ถึง TC-B-08

## งานที่ยังรอก่อนส่งจริง

- [ ] A/D แก้ mapping TC-B-01 และ TC-B-02 ใน A5 และตาราง 7.3
- [ ] A/D แยก utilization core 0/core 1 ใน A5 ตาราง 5.5 และหัวข้อ 6.3
- [ ] รันระบบต่อเนื่อง 10 นาทีและยืนยัน deadline miss = 0 กับ stack high-water mark
- [ ] รัน TC-B-01 ที่ `TIME_SCALE=1` ครบ 20 นาทีจริง
- [ ] ยืนยัน TC-B-02 ส่วน reset session หลังพักจริง 3 นาที หรือระบุผลเป็นผ่านบางส่วน
- [ ] วัดกระแส OLED/buzzer และทดสอบ LD2410 บนฮาร์ดแวร์จริง

## ข้อสรุป

ส่วน Hardware, FSM และ pseudocode ของ B ในเล่ม pre-final สอดคล้องกับ source ล่าสุด จุดที่ต้องแก้มี 2 เรื่องคือ mapping ผล TC-B-01/02 และการรวม utilization ข้าม core ส่วนงานที่เหลือเป็นการเก็บหลักฐานระยะยาวและฮาร์ดแวร์จริง ไม่ใช่การเปลี่ยน logic หลักของ B
