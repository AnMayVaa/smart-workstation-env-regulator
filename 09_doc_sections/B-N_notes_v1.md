# B-N บันทึกปัญหา ข้อจำกัด และแหล่งอ้างอิง

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.1 |
| วันที่ | 2026-10-14 |
| เจ้าของ | B |

## ปัญหาและการตัดสินใจ

| # | ปัญหา/ข้อจำกัด | ผลกระทบ | วิธีรับมือปัจจุบัน | งานติดตาม |
|---:|---|---|---|---|
| 1 | A1 ยังเป็น v0.1 แต่ branch C ยืนยันว่า C สร้าง `air_status` และ `alert_request` | contract กลางอาจยังไม่มี field/enum ล่าสุด | B แสดงข้อมูลและรับ alert จาก C โดยไม่ตัดสินเกณฑ์อากาศซ้ำ | A เพิ่ม field C และ enum `CO2_HIGH`, `TEMP_TOO_HIGH`, `TEMP_TOO_LOW`, `AIR_SENSOR_ERROR` ใน contract กลาง |
| 2 | D1 กำหนดให้ B รับ UI ของ FR-07/FR-10 และรีเซ็ตตาม FR-11 | ปุ่มเดิมต้องรองรับทั้ง alert และค่าผู้ใช้ | ใช้เมนูตามบริบทบน OLED; alert มีสิทธิ์เหนือเมนู; ส่งค่าเฉพาะเมื่อเปลี่ยน | A เพิ่ม `user_lux_target`/`late_mode`; D ตรวจการรับค่าและ reset integration |
| 3 | A-T ยังไม่มีฉบับส่งจริง ส่วน C/D มีข้อมูลใน branch แยก | B3 ยังใช้ estimated WCET และอาจไม่ตรง implementation สุดท้าย | อัปเดต period/interface จาก branch C/D แต่ไม่คัดลอก ownership | A วัด WCET ใน A4 และทุกฝ่าย sync task table ก่อนรวม |
| 4 | Wokwi ไม่มี LD2410 | พิสูจน์ความแม่นยำของ presence sensor ไม่ได้ | ใช้ PIR ทดสอบ FSM และ fault injection เท่านั้น | ทดสอบกับ LD2410 จริงหรือ HIL simulator ในงานต่อยอด |
| 5 | กระแสรวมของ OLED module และ buzzer ไม่ได้ระบุครบใน datasheet ระดับระบบ | การคำนวณแหล่งจ่ายอาจคลาดเคลื่อน | กันงบ 30 mA และ 5 mA แยกเป็นค่าประมาณ | A วัดกระแสจริงและใส่ margin ใน A2/A4 |
| 6 | mmWave อาจตรวจคนด้านหลังผนังบางหรือคนที่เดินผ่าน | เกิด false occupied และไม่ปิดไฟ | ปรับ gate/sensitivity ให้ครอบคลุมเฉพาะเก้าอี้และใช้ stable time 1 s | ทดสอบตำแหน่งติดตั้งหลายแบบ; บันทึก false positive/negative |
| 7 | OLED และ BH1750 ใช้ I²C ร่วมกัน | เกิด race หรือ priority inversion ได้ | ใช้ `i2c_lock`, timeout 50 ms และปล่อยหลัง transaction | A ตรวจ critical-section time และ error counter |
| 8 | ค่าตั้ง 500 ms, 1 s, 30 ms และ 1.5 s เป็น design parameters | อาจไม่เหมาะกับผู้ใช้หรือ hardware จริง | ระบุแหล่งว่าเป็นค่าตั้งเพื่อทดสอบ ไม่อ้างเป็นงานวิจัย | ปรับจาก fault-injection และ usability test พร้อมขึ้นเวอร์ชัน |
| 9 | แผนทดสอบใน branch C ใช้รหัส TC-B-01 ถึง TC-B-04 คนละความหมายกับไฟล์ของ B | รวมแผนแล้วอาจอ้างผลทดสอบผิดกรณี | ถือ `06_testing/B-TC_testcases_v1.md` เป็น source of truth ของรหัส B | C เปลี่ยน reference ให้ตรง TC-B-01 ถึง TC-B-07 ก่อน merge |
| 10 | branch C ใช้ชื่อ alert บางจุดไม่ตรงกัน เช่น `AIR_SENSOR_ERROR`/`AIR_SENSOR_ERR` และ `CO2_HIGH`/`CO2_HIGH_VENTILATING` | B อาจ map ข้อความหรือ clear alert ผิดชนิด | เอกสาร B ยึดชื่อตาราง interface ของ C คือ `AIR_SENSOR_ERROR` และ `CO2_HIGH` ชั่วคราว | A และ C กำหนด enum กลางเพียงชื่อเดียวก่อนเขียน integration code |

## ข้อจำกัดของแบบจำลองเวลา

การทดสอบ utilization ใน B3 ใช้ periodic/sporadic task, deadline เท่ากับ period และ estimated WCET โดยถือว่า network wait ไม่ busy-wait ผล 4.20% และ 0.74% จึงเป็น feasibility estimate ไม่ใช่หลักฐานรับรองเวลาแบบฮาร์ดเรียลไทม์ เมื่อมีโค้ดจริงต้องวัด execution time, queue latency, I²C blocking และ stack high-water mark ภายใต้กรณี alert กับ Wi-Fi เกิดพร้อมกัน

## แหล่งอ้างอิงแบบ IEEE

[B-N-1] Shenzhen Hi-Link Electronic Co., Ltd., “HLK-LD2410 Human Presence Sensing Module User Manual,” ver. 1.02, Jun. 8, 2022. [Online]. Available: https://www.hlktech.net/index.php?cate=cate-63fdaeb91b37b&id=download-center&pageid=2. [Accessed: Oct. 13, 2026].

[B-N-2] Solomon Systech Ltd., “SSD1306: 128 × 64 Dot Matrix OLED/PLED Segment/Common Driver with Controller,” rev. 1.1, Apr. 2008. [Online]. Available: https://www.solomon-systech.com/product/SSD1306. [Accessed: Oct. 13, 2026].

[B-N-3] TDK Corp., “Piezoelectronic Buzzers: PS Series,” Feb. 2022. [Online]. Available: https://product.tdk.com/info/en/catalog/datasheets/piezoelectronic_buzzer_ps_en.pdf. [Accessed: Oct. 13, 2026].

[B-N-4] OMRON Corp., “B3F Tactile Switch,” cat. no. A070-E1. [Online]. Available: https://components.omron.com/us-en/datasheet_pdf/A070-E1.pdf. [Accessed: Oct. 13, 2026].

[B-N-5] C. Talens-Estarelles et al., “The effects of breaks on digital eye strain, dry eye and binocular vision: Testing the 20-20-20 rule,” *Contact Lens and Anterior Eye*, 2022, Art. no. 101744, doi: 10.1016/j.clae.2022.101744.

[B-N-6] P. Wennberg et al., “Acute effects of breaking up prolonged sitting on fatigue and cognition: A pilot study,” *BMJ Open*, vol. 6, no. 2, Art. no. e009630, 2016, doi: 10.1136/bmjopen-2015-009630.

[B-N-7] C. L. Liu and J. W. Layland, “Scheduling algorithms for multiprogramming in a hard-real-time environment,” *J. ACM*, vol. 20, no. 1, pp. 46–61, Jan. 1973, doi: 10.1145/321738.321743.

[B-N-8] Real Time Engineers Ltd., “Mutexes,” *FreeRTOS Documentation*. [Online]. Available: https://www.freertos.org/Real-time-embedded-RTOS-mutexes.html. [Accessed: Oct. 13, 2026].

## ไฟล์อ้างอิงที่แนบ

- `03_hardware/datasheets/HLK-LD2410_user_manual_v1.02.pdf`
- `03_hardware/datasheets/SSD1306_datasheet_rev1.1.pdf`
- `03_hardware/datasheets/TDK_PS1240P02BT_datasheet.pdf`
- `03_hardware/datasheets/OMRON_B3F_datasheet.pdf`

## สิ่งที่ต้องตรวจซ้ำก่อนรวมเล่ม

- [ ] D ตรวจเลขหน้าแหล่งอ้างอิง [1] และ [6] และปีของ EN 12464-1 ตามหมายเหตุใน overview
- [ ] A/C/D ยืนยันข้อมูลที่ยังรอในตารางปัญหาด้านบน
- [ ] D รวมหมายเลขอ้างอิงกับ bibliography กลางโดยไม่ให้เลขซ้ำ
