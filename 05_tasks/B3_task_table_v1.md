# B3 ตารางงานย่อยรวมและการวิเคราะห์ว่าทำทัน

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.0 (ร่างจากเอกสารกลาง) |
| วันที่ | 2026-10-13 |
| เจ้าของ | B |
| สถานะ | ผลเบื้องต้นผ่านเกณฑ์ RMS แต่ต้องแทน WCET ด้วยค่าที่ A วัด และเทียบ A-T/C-T/D-T ฉบับยืนยันก่อนรวมเล่ม |

## ขอบเขตและสมมติฐาน

ตารางนี้รวบรวมงานจาก `00_team_guide/00_overview.md` ข้อ 6, A1 และเอกสารบทบาทของ A/C/D เพราะใน branch นี้ยังไม่มีไฟล์ A-T, C-T และ D-T ฉบับส่งจริง ค่า “ใช้เวลาทำ” จึงเป็น **estimated WCET** สำหรับตรวจความเป็นไปได้ ไม่ใช่ผลวัด ออกแบบให้ทุก task ใช้ `vTaskDelayUntil`, I/O แบบไม่ busy-wait และ Wi-Fi/NTP รอแบบ block เพื่อไม่กิน CPU ระหว่างรอเครือข่าย

กำหนด deadline เท่ากับ period ยกเว้น event จากปุ่มซึ่งกำหนด minimum inter-arrival 100 ms สำหรับการวิเคราะห์ งานที่ period เท่ากันอยู่ใน RMS class เดียวกันและใช้ความปลอดภัยเป็นตัวตัดสินลำดับภายใน class

## 1. ตารางงานย่อยโหนดโต๊ะ

| RMS priority | งานย่อย | เจ้าของ | ทำอะไร | Period / min inter-arrival T (ms) | Deadline D (ms) | Estimated WCET C (ms) | C/T | ของร่วม/การป้องกัน |
|---:|---|---|---|---:|---:|---:|---:|---|
| 1 | `presence_task` | B | อ่าน LD2410, filter และอัปเดต `occupied` | 100 | 100 | 1.0 | 0.010000 | UART เฉพาะ task; critical section สั้นกับข้อมูลกลาง |
| 2 | `button_event_task` | B | debounce และแปล short/long press สำหรับ alert และเมนูตั้งค่าแสง | 100 | 100 | 0.2 | 0.002000 | ISR ส่ง queue เท่านั้น; publish `user_lux_target`/`late_mode` เมื่อเปลี่ยน |
| 3 | `light_control_task` | D | อ่าน BH1750 และปรับ PWM | 200 | 200 | 2.0 | 0.010000 | I²C ผ่าน `i2c_lock`; PWM เป็น hardware peripheral |
| 4 | `display_alert_task` | B | จัด alert, วาด OLED และกำหนด buzzer | 500 | 500 | 8.0 | 0.016000 | I²C ผ่าน `i2c_lock`; alert ผ่าน queue |
| 5 | `break_timer_task` | B | นับเวลานั่งและตัดสิน 20/30/3/5 นาที | 1,000 | 1,000 | 1.0 | 0.001000 | อ่าน snapshot ของ `occupied`; ส่ง queue |
| 6 | `desk_communication_task` | A | ส่ง `desk_report` และรับ `room_report` | 5,000 | 5,000 | 15.0 | 0.003000 | สำเนาข้อมูลภายใต้ mutex; network wait ต้อง block |
| 7 | `ntp_sync_task` | A | ขอเวลา NTP และอัปเดต `time_of_day` | 3,600,000 | 3,600,000 | 5.0 | 0.0000014 | network wait ต้อง block; ไม่ถือ mutex ระหว่างรอ |

### คำนวณ utilization โหนดโต๊ะ

ใช้สูตรจาก Rate Monotonic Scheduling

```text
U = Σ(Ci / Ti)
  = 1/100 + 0.2/100 + 2/200 + 8/500 + 1/1000 + 15/5000 + 5/3600000
  = 0.0420014
  = 4.20014%

U_bound(n=7) = n(2^(1/n) - 1)
             = 7(2^(1/7) - 1)
             ≈ 0.7286
             = 72.86%
```

ผลเบื้องต้นคือ **4.20% < 72.86% จึงผ่าน sufficient utilization bound ของ RMS** ภายใต้สมมติฐานของแบบจำลอง แม้เผื่อ blocking จาก I²C critical section สูงสุดประมาณ 8 ms งานคุมแสงยังมีเวลาตอบสนองประมาณ 11.2 ms ซึ่งต่ำกว่า deadline 200 ms มาก งาน alert มี design target ให้เริ่มแสดงภายใน 500 ms หลัง event เข้า queue

## 2. ตารางงานย่อยโหนดห้อง

| RMS class / tie priority | งานย่อย | เจ้าของ | ทำอะไร | Period T (ms) | Deadline D (ms) | Estimated WCET C (ms) | C/T | ของร่วม/การป้องกัน |
|---:|---|---|---|---:|---:|---:|---:|---|
| 1 | `air_sensor_task` | C | สั่ง/อ่าน SCD41 และสร้าง `air_status` | 5,000 | 5,000 | 20.0 | 0.004000 | I²C ผ่าน `i2c_lock`; ไม่หน่วงขณะถือ lock |
| 2 | `fan_control_task` | C | ตัดสินระดับพัดลม/ไฟสถานะจาก snapshot | 5,000 | 5,000 | 2.0 | 0.000400 | PWM/GPIO hardware; safety tie-break สูงกว่า comm |
| 3 | `room_communication_task` | A | รับ `desk_report`, นับโต๊ะ และส่ง `room_report` | 5,000 | 5,000 | 15.0 | 0.003000 | snapshot/mutex; network wait ต้อง block |

### คำนวณ utilization โหนดห้อง

```text
U = 20/5000 + 2/5000 + 15/5000
  = 0.0074
  = 0.74%

U_bound(n=3) = 3(2^(1/3) - 1)
             ≈ 0.7798
             = 77.98%
```

ผลเบื้องต้นคือ **0.74% < 77.98% จึงผ่านเกณฑ์ RMS** งานทั้งสามมี period เท่ากันจึงไม่มีลำดับจาก RMS โดยตรง ให้เริ่มแต่ละงานเหลื่อมเวลากันและใช้ tie priority ตามตารางเพื่อไม่ให้เกิดโหลดพร้อมกันทุก 5 s; เวลารวมต่อรอบประมาณ 37 ms ต่ำกว่า deadline 5,000 ms

## 3. ทรัพยากรที่ใช้ร่วมและ concurrency

| ทรัพยากร | ผู้ใช้ | วิธีป้องกัน | ข้อจำกัดเวลา |
|---|---|---|---|
| I²C โหนดโต๊ะ | BH1750 ของ D, OLED ของ B | FreeRTOS mutex `i2c_lock` พร้อม priority inheritance | รอ lock ไม่เกิน 50 ms; ถือเฉพาะ transaction; ห้าม `vTaskDelay`/รอ Wi-Fi ขณะถือ lock |
| I²C โหนดห้อง | SCD41 ของ C | `i2c_lock` แม้ปัจจุบันมีผู้ใช้หลักรายเดียว เพื่อรองรับการเพิ่มอุปกรณ์ | กติกาเดียวกับโหนดโต๊ะ |
| จอและ buzzer | ทุกโมดูลร้องขอ, B เป็นเจ้าของ | FreeRTOS queue ของ `alert_request`; display task เป็นผู้เขียน hardware รายเดียว | priority 2 > 1 > 0; alert แทรกเมนูตั้งค่าได้; ไม่ block task ด้วยการเปิดเสียง |
| ข้อมูลกลาง | task สร้าง/อ่านต่างกัน | single writer + snapshot ภายใต้ mutex/critical section สั้น | ห้ามถือ lock ระหว่าง network I/O |
| GPIO PWM | งานแสง/พัดลม/buzzer | hardware timer/channel แยกกัน; A ตรวจ channel conflict ใน A2 | ไม่สร้าง RTOS task ที่คาบ 1–20 ms เพียงเพื่อ toggle ขา |

## 4. เหตุผลที่เป็น soft real-time

ระบบมี deadline เพื่อให้การอ่านเซนเซอร์ การเตือน และการควบคุมตอบสนองสม่ำเสมอ แต่การพลาด deadline หนึ่งรอบทำให้ประสบการณ์หรือคุณภาพการควบคุมลดลง ไม่ได้ทำให้เกิดอันตรายทันทีแบบ hard real-time ตัวอย่างเช่น OLED อัปเดตช้า 1 รอบยังแสดงในรอบถัดไป และการอ่านอากาศช้าเล็กน้อยยังใช้ค่าก่อนหน้าได้ ระบบจึงเป็น **soft real-time** พร้อมใช้ fail-safe เมื่อข้อมูลสำคัญหาย

## 5. เหตุผลที่ใช้ FreeRTOS

1. งานสองโหนดมี period ตั้งแต่ 100 ms ถึง 1 ชั่วโมง การใช้ task แยกทำให้กำหนดเวลาและ priority ตาม RMS ได้ตรงกว่า loop ที่มี network I/O ปะปน
2. queue ทำให้หลายโมดูลส่ง `alert_request` ไปยังเจ้าของจอ/buzzer ได้โดยไม่เขียน hardware พร้อมกัน
3. mutex พร้อม priority inheritance ลดปัญหา priority inversion บนบัส I²C ที่ OLED และ BH1750 ใช้ร่วมกัน
4. task ที่รอ Wi-Fi/NTP สามารถ block ได้โดยไม่หยุดงานตรวจคน คุมแสง และเตือนพัก
5. FreeRTOS มีอยู่ใน ESP32/Arduino framework อยู่แล้ว จึงไม่เพิ่ม OS ภายนอก

## 6. ข้อจำกัดของผลคำนวณและงานที่ต้องยืนยัน

- [ ] A วัด WCET/response time ของทุก task จาก A4 อย่างน้อย 100 รอบ และใช้ค่าสูงสุดรวม margin แทน estimated WCET
- [ ] A, C และ D ส่ง A-T/C-T/D-T แล้วตรวจว่าชื่อ, period, deadline และทรัพยากรตรงกับตารางนี้
- [x] รับ requirement จาก D1 แล้ว: NFR-04 กำหนดให้แสดงการเตือนพักภายใน 1 s และ NFR-14 กำหนดอย่างน้อย 2 ช่องทาง
- [ ] A รวม `user_lux_target`, `late_mode` และการ aggregate `occupied` เป็น `room_occupied_count` ใน contract กลาง
- [ ] A ตรวจว่าการเรียก Wi-Fi/NTP ไม่ busy-wait และไม่ถือ mutex ขณะ block
- [ ] ถ้า task set หรือ period เปลี่ยน ต้องคำนวณ `U`, RMS bound และ blocking ใหม่

## เช็กลิสต์ก่อนส่ง

- [x] มีงานย่อยครบทั้งโหนดโต๊ะและโหนดห้องตามข้อมูลที่มีใน repo
- [x] เรียง priority ตาม RMS และอธิบายกรณี period เท่ากัน
- [x] แสดงวิธีคำนวณรายโหนดและเทียบ bound
- [x] ระบุ I²C mutex, queue, blocking และข้อห้ามถือ lock
- [x] แยกค่าประมาณออกจากผลวัดที่ยังรอ A

## อ้างอิง

[B3-1] เอกสารประกอบวิชา 010123119, บท 16 หน้า 9, 15–21, 30–34 และบท 17 หน้า 17–31.

[B3-2] C. L. Liu and J. W. Layland, “Scheduling algorithms for multiprogramming in a hard-real-time environment,” *J. ACM*, vol. 20, no. 1, pp. 46–61, Jan. 1973, doi: 10.1145/321738.321743.
