# B3 ตารางงานย่อยรวมและการวิเคราะห์ว่าทำทัน

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.3 |
| วันที่ | 2026-10-15 |
| เจ้าของ | B |
| สถานะ | รวม task specification และผลรัน Wokwi ต่อเนื่อง 22.8 นาทีจาก A5 แล้ว; ยังรอวัดบนฮาร์ดแวร์จริง |

## ขอบเขตและสมมติฐาน

ตารางนี้ใช้ `05_tasks/A-T_tasks.md` v1.1, `05_tasks/C-T_tasks.md` และ `05_tasks/D-T_tasks.md` จาก branch ของเจ้าของงาน ค่า “ใช้เวลาทำ” ในส่วนออกแบบยังเป็น **estimated WCET** สำหรับตรวจความเป็นไปได้ ไม่ใช่ผลวัด ออกแบบให้ periodic task ใช้ `vTaskDelayUntil`, I/O แบบไม่ busy-wait และ Wi-Fi/NTP รอแบบ block เพื่อไม่กิน CPU ระหว่างรอเครือข่าย

กำหนด minimum inter-arrival ของปุ่มเป็น 100 ms และใช้ T = 1,000 ms สำหรับ `desk_communication_task` เพราะตื่นทันทีเมื่อ `occupied` เปลี่ยน งานควบคุมของ B/C/D pin บน core 1 ส่วนงานสื่อสาร A, NTP, Wi-Fi driver และ TCP/IP/lwIP อยู่ core 0 ตาม A-T v1.1 และโค้ด A4 จึงต้องคำนวณ utilization แยก core

## 1. ตารางงานย่อยโหนดโต๊ะ

| RMS priority | งานย่อย | เจ้าของ | Core | ทำอะไร | Period / min inter-arrival T (ms) | Deadline D (ms) | Estimated WCET C (ms) | C/T | ของร่วม/การป้องกัน |
|---:|---|---|---:|---|---:|---:|---:|---:|---|
| 1 | `presence_task` | B | 1 | อ่าน LD2410, filter และอัปเดต `occupied` | 100 | 100 | 1.0 | 0.010000 | UART เฉพาะ task; critical section สั้นกับข้อมูลกลาง |
| 2 | `button_event_task` | B | 1 | debounce และแปล short/long press สำหรับ alert และเมนูตั้งค่าแสง | 100 | 100 | 0.2 | 0.002000 | ISR ส่ง queue เท่านั้น; publish `user_lux_target`/`late_mode` เมื่อเปลี่ยน |
| 3 | `light_control_task` | D | 1 | อ่าน BH1750 และปรับ PWM | 200 | 200 | 2.0 | 0.010000 | I²C ผ่าน `i2c_lock`; PWM เป็น hardware peripheral |
| 4 | `display_alert_task` | B | 1 | แปลง `air_status` เป็น alert, จัดลำดับ, วาด OLED และกำหนด buzzer | 500 | 500 | 8.0 | 0.016000 | snapshot ข้อมูลกลาง; I²C ผ่าน `i2c_lock`; alert ผ่าน queue ภายในโหนด |
| 5 | `break_timer_task` | B | 1 | นับเวลานั่งและตัดสิน 20/30/3/5 นาที | 1,000 | 1,000 | 1.0 | 0.001000 | อ่าน snapshot ของ `occupied`; ส่ง queue |
| — | `desk_communication_task` | A | 0 | ส่ง `desk_report`, รับ `room_report`, เขียนสำเนา `air_status`/`link_ok` และอัปเดตเวลา | 1,000 (ช่วงห่างต่ำสุด) | 1,000 | 15.0 | 0.015000 | รอบปกติ 5,000 / ว่าง 15,000 ms และตื่นเมื่อ `occupied` เปลี่ยน; network wait ต้อง block |
| — | `ntp_sync_task` | A | 0 | ขอเวลา NTP และอัปเดต `time_of_day` | 3,600,000 | 3,600,000 | 5.0 | 0.0000014 | network wait ต้อง block; ไม่ถือ mutex ระหว่างรอ |

### คำนวณ utilization โหนดโต๊ะ

ใช้สูตรจาก Rate Monotonic Scheduling

```text
U_core1 = 1/100 + 0.2/100 + 2/200 + 8/500 + 1/1000
        = 0.039
        = 3.90%

U_bound(n=5) = 5(2^(1/5) - 1)
             ≈ 0.7435
             = 74.35%

U_core0_app = 15/1000 + 5/3600000
            ≈ 0.0150014
            = 1.50014%
```

งานควบคุม core 1 มี **3.90% < 74.35%** จึงผ่าน sufficient utilization bound ของ RMS ตามค่าประมาณ ส่วน 1.50% ของ core 0 เป็นเฉพาะ application task ของ A ห้ามเทียบกับ bound โดยลำพังเพราะยังมี Wi-Fi/TCP-IP system tasks บน core เดียวกัน แม้เผื่อ blocking จาก I²C critical section สูงสุดประมาณ 8 ms งานคุมแสงยังมีเวลาตอบสนองประมาณ 11.2 ms ซึ่งต่ำกว่า deadline 200 ms มาก

## 2. ตารางงานย่อยโหนดห้อง

| RMS class / tie priority | งานย่อย | เจ้าของ | Core | ทำอะไร | Period T (ms) | Deadline D (ms) | Estimated WCET C (ms) | C/T | ของร่วม/การป้องกัน |
|---:|---|---|---:|---|---:|---:|---:|---:|---|
| 1 | `Task_Read_Air` | C | 1 | อ่าน SCD41, ตรวจ CRC/ช่วงค่า และเขียน `air_status` | 5,000 | 500 | 5.0 | 0.001000 | I²C ผ่าน `i2c_lock`; เขียน snapshot ให้ C/A ใช้ |
| 2 | `Task_Control_Fan` | C | 1 | ตัดสิน FSM พัดลม/ไฟสถานะจาก `air_status` และ `room_occupied_count` | 5,000 | 1,000 | 1.0 | 0.000200 | PWM/GPIO hardware; A1 v1.0 ไม่ให้ส่ง `alert_request` ข้าม Wi-Fi |
| — | `room_communication_task` | A | 0 | รับ `desk_report`, นับโต๊ะ และส่ง `room_report` | 5,000 | 5,000 | 15.0 | 0.003000 | snapshot/mutex; network wait ต้อง block |

### คำนวณ utilization โหนดห้อง

```text
U_core1 = 5/5000 + 1/5000
        = 0.0012
        = 0.12%

U_core0_app = 15/5000
            = 0.003
            = 0.30%
```

งานควบคุม core 1 มี utilization เพียง 0.12% งานสองงานมี period เท่ากันแต่ deadline ต่างกัน จึงให้ `Task_Read_Air` ที่ deadline 500 ms มี priority สูงกว่า `Task_Control_Fan` ที่ deadline 1,000 ms เวลารวม 6 ms ต่ำกว่า deadline ที่สั้นที่สุดมาก ส่วน core 0 มี application communication 0.30% โดยยังไม่รวม system tasks

## 3. ผลวัดต่อเนื่องจาก A5

A5 รันโหนดโต๊ะใน Wokwi ต่อเนื่อง 22.8 นาทีพร้อมเหตุการณ์ใช้งาน และรันโหนดห้อง 10.5 นาที ไม่พบ deadline miss หรือ reset ค่าในตารางเป็น WCET ของรอบต่อเนื่องตาม `06_testing/A5_results.md` ข้อ 7 และยังไม่แทนการวัดบน ESP32 จริง

| โหนด | Core | งานย่อย | T (ms) | WCET ที่วัดได้ (µs) | C/T |
|---|---:|---|---:|---:|---:|
| โต๊ะ | 1 | `presence_task` | 100 | 948 | 0.009480 |
| โต๊ะ | 1 | `button_event_task` | 100 | 68 | 0.000680 |
| โต๊ะ | 1 | `light_control_task` | 200 | 31,797 | 0.158985 |
| โต๊ะ | 1 | `display_alert_task` | 500 | 53,110 | 0.106220 |
| โต๊ะ | 1 | `break_timer_task` | 1,000 | 14,394 | 0.014394 |
| โต๊ะ | 0 | `desk_communication_task` | 1,000 | 5,207 | 0.005207 |
| ห้อง | 1 | `air_sensor_task` | 5,000 | 218,015 | 0.043603 |
| ห้อง | 0 | `room_communication_task` | 5,000 | 8,216 | 0.001643 |

ผลรวมตามตาราง A5 ข้อ 7 คือ `U = 0.294966` หรือ **29.50%** และต่ำกว่า bound `n=6` ที่ 73.48% แต่ตัวเลขนั้นรวม task คนละ core จึงใช้สรุป schedulability ไม่ได้ เมื่อแยกตามการ pin จริง งานควบคุมโต๊ะ core 1 มี `U = 0.289759` หรือ **28.98%** ต่ำกว่า RMS bound สำหรับ `n=5` ที่ **74.35%** ส่วน `desk_communication_task` บน core 0 ใช้ **0.52%** เฉพาะ application task และยังไม่รวม Wi-Fi/TCP-IP system tasks

โหนดห้องใช้ค่า `air_sensor_task` 218.015 ms จากรอบสั้นเป็นกรณีแย่สุด เพราะสูงกว่ารอบต่อเนื่อง 7.795 ms ที่ไม่มีการอ่าน DHT22 ครั้งแรก ส่วน `room_communication_task` ใช้ค่ารอบต่อเนื่อง 8.216 ms ดังนั้น application utilization ที่วัดได้คือ 4.36% บน core 1 และ 0.16% บน core 0

WCET ของ `light_control_task` 31.797 ms และ `display_alert_task` 53.110 ms สะท้อนการรอ `i2c_lock` ระหว่าง OLED วาดเต็มเฟรม แต่ยังต่ำกว่า deadline 200 และ 500 ms ตามลำดับ `button_event_task` เหลือ stack 2,196 B หลังเพิ่ม allocation เป็น 3,072 B และรัน 22.8 นาที จึงยืนยัน margin ได้ดีกว่ารอบสั้นที่เคยเหลือ 876 B

## 4. ทรัพยากรที่ใช้ร่วมและ concurrency

| ทรัพยากร | ผู้ใช้ | วิธีป้องกัน | ข้อจำกัดเวลา |
|---|---|---|---|
| I²C โหนดโต๊ะ | BH1750 ของ D, OLED ของ B | FreeRTOS mutex `i2c_lock` พร้อม priority inheritance | รอ lock ไม่เกิน 50 ms; ถือเฉพาะ transaction; ห้าม `vTaskDelay`/รอ Wi-Fi ขณะถือ lock |
| I²C โหนดห้อง | SCD41 ของ C | `i2c_lock` แม้ปัจจุบันมีผู้ใช้หลักรายเดียว เพื่อรองรับการเพิ่มอุปกรณ์ | กติกาเดียวกับโหนดโต๊ะ |
| จอและ buzzer | ทุกโมดูลร้องขอ, B เป็นเจ้าของ | FreeRTOS queue ของ `alert_request`; display task เป็นผู้เขียน hardware รายเดียว | priority 2 > 1 > 0; alert แทรกเมนูตั้งค่าได้; ไม่ block task ด้วยการเปิดเสียง |
| ข้อมูลกลาง | task สร้าง/อ่านต่างกัน | single writer + snapshot ภายใต้ mutex/critical section สั้น | ห้ามถือ lock ระหว่าง network I/O |
| GPIO PWM | งานแสง/พัดลม/buzzer | hardware timer/channel แยกกัน; A ตรวจ channel conflict ใน A2 | ไม่สร้าง RTOS task ที่คาบ 1–20 ms เพียงเพื่อ toggle ขา |

## 5. เหตุผลที่เป็น soft real-time

ระบบมี deadline เพื่อให้การอ่านเซนเซอร์ การเตือน และการควบคุมตอบสนองสม่ำเสมอ แต่การพลาด deadline หนึ่งรอบทำให้ประสบการณ์หรือคุณภาพการควบคุมลดลง ไม่ได้ทำให้เกิดอันตรายทันทีแบบ hard real-time ตัวอย่างเช่น OLED อัปเดตช้า 1 รอบยังแสดงในรอบถัดไป และการอ่านอากาศช้าเล็กน้อยยังใช้ค่าก่อนหน้าได้ ระบบจึงเป็น **soft real-time** พร้อมใช้ fail-safe เมื่อข้อมูลสำคัญหาย

## 6. เหตุผลที่ใช้ FreeRTOS

1. งานสองโหนดมี period ตั้งแต่ 100 ms ถึง 1 ชั่วโมง การใช้ task แยกทำให้กำหนดเวลาและ priority ตาม RMS ได้ตรงกว่า loop ที่มี network I/O ปะปน โดยแยก network stack ไว้ core 0 และงานที่วิเคราะห์ไว้ core 1
2. queue ทำให้หลายโมดูลส่ง `alert_request` ไปยังเจ้าของจอ/buzzer ได้โดยไม่เขียน hardware พร้อมกัน
3. mutex พร้อม priority inheritance ลดปัญหา priority inversion บนบัส I²C ที่ OLED และ BH1750 ใช้ร่วมกัน
4. task ที่รอ Wi-Fi/NTP สามารถ block ได้โดยไม่หยุดงานตรวจคน คุมแสง และเตือนพัก
5. FreeRTOS มีอยู่ใน ESP32/Arduino framework อยู่แล้ว จึงไม่เพิ่ม OS ภายนอก

## 7. ข้อจำกัดของผลคำนวณและงานที่ต้องยืนยัน

- [x] A5 รัน Wokwi ต่อเนื่อง 22.8 นาทีสำหรับโต๊ะและ 10.5 นาทีสำหรับห้อง โดย `late = 0` ทุก task
- [ ] วัด WCET และ stack high-water mark บน ESP32 จริงก่อนใช้เป็นค่าขั้นสุดท้าย
- [x] รวมชื่อ, period, deadline และ estimated WCET จาก A-T/C-T/D-T ฉบับเจ้าของงานแล้ว
- [x] รับ requirement จาก D1 แล้ว: NFR-04 กำหนดให้แสดงการเตือนพักภายใน 1 s และ NFR-14 กำหนดอย่างน้อย 2 ช่องทาง
- [x] A1 v1.0 ยืนยัน `user_lux_target`, `late_mode`, `room_occupied_count` และเจ้าของข้อมูลแล้ว
- [x] C-T ฉบับล่าสุดอัปเดตแล้วว่าไม่มี `alert_request` ข้าม Wi-Fi; B แปลงจาก `air_status` ตาม A1 v1.0
- [ ] A ตรวจว่าการเรียก Wi-Fi/NTP ไม่ busy-wait และไม่ถือ mutex ขณะ block
- [ ] ถ้า task set หรือ period เปลี่ยน ต้องคำนวณ `U`, RMS bound และ blocking ใหม่

## เช็กลิสต์ก่อนส่ง

- [x] มีงานย่อยครบทั้งโหนดโต๊ะและโหนดห้องตามข้อมูลที่มีใน repo
- [x] เรียง priority ตาม RMS และอธิบายกรณี period เท่ากัน
- [x] แสดงวิธีคำนวณรายโหนดและเทียบ bound
- [x] ระบุ I²C mutex, queue, blocking และข้อห้ามถือ lock
- [x] แยกค่าประมาณออกจากผลวัดต่อเนื่องของ A5 และแยก utilization ตาม core

## อ้างอิง

[B3-1] เอกสารประกอบวิชา 010123119, บท 16 หน้า 9, 15–21, 30–34 และบท 17 หน้า 17–31.

[B3-2] C. L. Liu and J. W. Layland, “Scheduling algorithms for multiprogramming in a hard-real-time environment,” *J. ACM*, vol. 20, no. 1, pp. 46–61, Jan. 1973, doi: 10.1145/321738.321743.
