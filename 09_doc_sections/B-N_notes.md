# B-N บันทึกปัญหา ข้อจำกัด และแหล่งอ้างอิง

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.3 |
| วันที่ | 2026-10-15 |
| เจ้าของ | B |

## ปัญหาและการตัดสินใจ

| # | ปัญหา/ข้อจำกัด | ผลกระทบ | วิธีรับมือปัจจุบัน | งานติดตาม |
|---:|---|---|---|---|
| 1 | A1 v1.0 ยืนยันว่า C ส่ง `air_status` ผ่าน A แต่ไม่ส่ง `alert_request` ข้าม Wi-Fi | logic ร่างเดิมของ B รับ alert จาก C ผิดทิศทาง | B อ่าน `co2_level`, `temp_flag`, `sensor_ok` แล้วสร้าง/clear alert `SRC_AIR` ภายในโหนด; A5 S1/S1b/S3 ผ่าน | รันต่อเนื่อง 10 นาทีเพื่อยืนยันการ clear/reconnect ซ้ำ |
| 2 | A1 v1.0 ให้ B เป็นเจ้าของ `user_lux_target`/`late_mode` และ reset ตาม FR-11 | ต้องมี UI ที่ไม่ชนกับปุ่ม alert | ใช้เมนูตามบริบทบน OLED; A5 D4 ยืนยัน 800 lux/late ON และ reset เป็น 500/false หลัง away | ไม่มีงานค้างฝั่ง B |
| 3 | WCET ขั้นออกแบบเป็นค่าประมาณ | อาจประเมิน blocking และ utilization ต่ำกว่าจริง | B3 เพิ่มผล A5 แยก core: งานควบคุมโต๊ะ core 1 = 26.88%, communication core 0 = 0.56%, deadline miss = 0 ในช่วงประมาณ 45 s | รัน 10 นาทีและวัดบน ESP32 จริงก่อนสรุป WCET ขั้นสุดท้าย |
| 4 | Wokwi ไม่มี LD2410 | พิสูจน์ความแม่นยำของ presence sensor ไม่ได้ | ใช้ slide switch ที่ GPIO 13 ทดสอบ FSM และ fault injection เท่านั้น | ทดสอบกับ LD2410 จริงหรือ HIL simulator ในงานต่อยอด |
| 5 | กระแสรวมของ OLED module และ buzzer ไม่ได้ระบุครบใน datasheet ระดับระบบ | การคำนวณแหล่งจ่ายอาจคลาดเคลื่อน | กันงบ 30 mA และ 5 mA แยกเป็นค่าประมาณ | A วัดกระแสจริงและใส่ margin ใน A2/A4 |
| 6 | mmWave อาจตรวจคนด้านหลังผนังบางหรือคนที่เดินผ่าน | เกิด false occupied และไม่ปิดไฟ | ปรับ gate/sensitivity ให้ครอบคลุมเฉพาะเก้าอี้และใช้ stable time 1 s | ทดสอบตำแหน่งติดตั้งหลายแบบ; บันทึก false positive/negative |
| 7 | OLED และ BH1750 ใช้ I²C ร่วมกัน | เกิด race หรือ priority inversion ได้ | ใช้ `i2c_lock`, timeout 50 ms; A5 วัด display 43.186 ms และ light 28.534 ms โดยไม่พลาด deadline | ยืนยันซ้ำในการรัน 10 นาทีและบอร์ดจริง |
| 8 | ค่าตั้ง 500 ms, 1 s, 30 ms และ 1.5 s เป็น design parameters | อาจไม่เหมาะกับผู้ใช้หรือ hardware จริง | ระบุแหล่งว่าเป็นค่าตั้งเพื่อทดสอบ ไม่อ้างเป็นงานวิจัย | ปรับจาก fault-injection และ usability test พร้อมขึ้นเวอร์ชัน |
| 9 | **ปิดแล้ว:** แผนทดสอบรวมเคยใช้รหัส TC-B ไม่ตรงกับไฟล์ B | รวมแผนแล้วอาจอ้างผลผิดกรณี | ตาราง 7.1 ในเล่ม pre-final ใช้ความหมาย TC-B-01 ถึง TC-B-08 ตรงกับ B-TC แล้ว | ตรวจ mapping ของตารางผล 7.3 แยกในข้อ 13 |
| 10 | **ปิดแล้ว:** ชื่อ alert อากาศและ ownership เคยไม่ตรงกันระหว่างเอกสาร | B อาจสร้างหรือ clear alert ผิดชนิด | ยึด A1 v1.0 เพียงชุดเดียว: `ALERT_CO2_HIGH`, `ALERT_TEMP`, `ALERT_SENSOR_FAULT` จาก `SRC_AIR`; B แปลงจาก `air_status` และไม่มี alert ข้าม Wi-Fi | ไม่มีงานค้างฝั่ง B |
| 11 | `08_code/SIMULATION_GUIDE.md` เรียกกรณีเตือน 20 นาทีว่า TC-B-02 แต่ไฟล์ B กำหนดเป็น TC-B-01 | หลักฐาน Wokwi อาจติดรหัสผิด | B-TC เป็น source of truth และเพิ่มกติกา `TIME_SCALE` ให้ตรง simulation guide | A แก้ชื่อเป็น TC-B-01 ก่อนรวมหลักฐาน A5 |
| 12 | **ปิดแล้ว:** C-T เคยเขียนว่า `Task_Control_Fan` สร้าง `alert_request` | implementation ฝั่ง C อาจส่งข้อมูลซ้ำหรือผิด ownership | C-T ล่าสุดระบุว่าอัปเดตเฉพาะ field ใน `air_status` และไม่มี `alert_request` ข้าม Wi-Fi | ไม่มีงานค้าง |
| 13 | ตารางผล 7.3 ใน `D5_design_document.docx`/A5 จับ `TC-B-01` กับการเข้า WORKING และรวม EYE/STAND ไว้ใน `TC-B-02` | รหัสผลทดสอบไม่ตรงกับ test plan ตาราง 7.1 | B-TC ยังคง TC-B-01 = EYE 20 นาที และ TC-B-02 = STAND 30 นาทีพร้อม reset หลังพักจริง | D/A แยกผลและแก้ label ตอนสร้างเล่มรอบถัดไป |
| 14 | หลัง LD2410 ฟื้นจาก fault ระบบเริ่ม session ใหม่ | เวลานั่งก่อน fault ไม่ถูกนำมาต่อ | ยืนยันให้คงพฤติกรรมนี้ เพราะช่วง fault ทำให้เวลานั่งไม่น่าเชื่อถือและ B2 ระบุไว้แล้ว | เพิ่มคำอธิบายในผล/ข้อจำกัด ไม่ต้องแก้โค้ด |
| 15 | A5 เคยแนะนำเพิ่ม stack ของ `button_event_task` เป็น 3,072 B | stack margin เดิมต่ำสุด | โค้ดล่าสุดกำหนด stack 3,072 B แล้ว | รัน `stats` 10 นาทีเพื่อยืนยัน high-water mark หลังแก้ |
| 16 | A5/เล่มรวมบวก utilization ของ task บน core 0 และ core 1 แล้วเทียบ RMS bound ชุดเดียว | การสรุป schedulability ไม่ตรงกับการ pin task จริง | B3 แยก core 1 control ออกจาก core 0 communication ตาม A-T v1.1 และ A4 | A/D แก้ตาราง 5.5 และข้อความหัวข้อ 6.3 ในเล่มรอบถัดไป |

## ข้อจำกัดของแบบจำลองเวลา

ขั้นออกแบบใช้ estimated WCET ได้ 3.90% สำหรับงานควบคุมโต๊ะ core 1 และ 1.50% สำหรับ application task core 0 ส่วน A5 วัด implementation จำลองประมาณ 45 s ได้ 26.88% บน core 1 และ 0.56% ของ communication บน core 0 โดยไม่พบ deadline miss โหนดห้องวัดได้ 4.36% บน core 1 และ 0.11% บน core 0 ความต่างหลักมาจากเวลารอ I²C/OLED ผลนี้ยังไม่ใช่ WCET ของฮาร์ดแวร์จริงและไม่รวมทุก system task จึงต้องรัน 10 นาที วัด stack high-water mark และทดสอบบน ESP32 จริงก่อนสรุป

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
- [ ] A/D แก้ mapping ผล TC-B-01/02 ใน A5 และเล่มตาราง 7.3
- [ ] A รัน 10 นาที, `TIME_SCALE=1` 20 นาทีจริง และวัดกระแส/เวลาเมื่อใช้ฮาร์ดแวร์จริง
- [ ] D รวมหมายเลขอ้างอิงกับ bibliography กลางโดยไม่ให้เลขซ้ำ
