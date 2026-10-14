# B4 เนื้อหาเอกสารส่วนของบทบาท B

## 4. Hardware Design: โมดูลตรวจจับคนและส่วนติดต่อผู้ใช้

โหนดโต๊ะใช้ HLK-LD2410 ซึ่งเป็นเรดาร์ mmWave 24 GHz ตรวจการมีอยู่ของบุคคลแทน PIR เหตุผลสำคัญคือผู้ใช้โต๊ะอาจนั่งนิ่งเป็นเวลานาน แต่ระบบยังต้องรักษาไฟและตัวนับ session ไว้ LD2410 ระบุว่าสามารถตรวจทั้งเป้าหมายเคลื่อนไหวและนิ่ง มีระยะปรับได้ 0.75–6 m ใช้ไฟ 5 V กระแสเฉลี่ย 80 mA และสื่อสารกับ ESP32 ผ่าน UART ระดับลอจิก 3.3 V [B4-1] ใน Wokwi ใช้ slide switch ที่ GPIO 13 แทนเฉพาะการสร้างเหตุการณ์ `occupied` จึงไม่ใช้ผลจำลองเพื่อสรุปความแม่นยำของ mmWave

ส่วนแสดงผลเลือก OLED 128×64 ที่ใช้ SSD1306 เพราะรองรับ I²C แสดงข้อความและไอคอนได้ในพื้นที่เล็ก และแชร์บัสกับ BH1750 ได้โดยใช้ `i2c_lock` [B4-2] เสียงเตือนใช้ TDK PS1240P02BT ซึ่งขับด้วยสัญญาณสี่เหลี่ยม 3 V ที่ 4 kHz และให้ระดับเสียงอย่างน้อย 70 dB(A) ที่ระยะ 10 cm [B4-3] ปุ่ม B3F-1000 ต่อแบบ active-low ใช้รับทราบหรือเลื่อนการเตือน และใช้ตั้งค่าความสว่างหรือโหมดทำงานดึกผ่านเมนูตามบริบทบน OLED มี tactile feedback และอายุทางกล 1,000,000 ครั้ง [B4-4]

การคำนวณกำลังไฟต้องใช้ 200 mA เป็นความสามารถขั้นต่ำของรางที่จ่าย LD2410 ตามคู่มือ แม้กระแสเฉลี่ยเป็น 80 mA งบกระแสของ OLED 30 mA และ buzzer 5 mA เป็นค่าประมาณที่ต้องวัดยืนยันกับโมดูลจริงก่อนออกแบบแหล่งจ่ายขั้นสุดท้าย รายละเอียดและ trade-off อยู่ใน `03_hardware/B1_hardware_presence.md`

A2 ยืนยันผังขาของ B เป็น OLED SDA/SCL ที่ GPIO 21/22, LD2410 UART2 ที่ GPIO 16/17 และ OUT สำรองที่ GPIO 13, ปุ่มหลักที่ GPIO 27 และ buzzer ที่ GPIO 26 การออกแบบเลือกใช้ปุ่มเดียวร่วมกับเมนูตามบริบท จึงเก็บ GPIO 33 ที่ A2 สำรองไว้โดยยังไม่ติดตั้งปุ่มที่ 2

## 5. Software Design: ตรวจคน นับเวลา และเตือนพัก

ซอฟต์แวร์ใช้ Timed FSM แบ่งเป็น `UNOCCUPIED`, `WORKING`, `EYE_ALERT`, `STAND_ALERT`, `AWAY_PENDING` และ `SENSOR_FAULT` เมื่อพบคนต่อเนื่อง 1 s ระบบเริ่ม session และอัปเดต `sit_minutes` ทุกวินาที เมื่อครบ 20 min ส่ง `ALERT_EYE_BREAK`; เมื่อครบ 30 min ส่ง `ALERT_STAND_BREAK` การเตือนลุกมีลำดับเหนือการเตือนพักสายตาใน session เดียวกันเพราะครอบคลุมการพักที่มากกว่า

ถ้าผู้ใช้ลุกไม่ถึง 3 min ระบบทำ session เดิมต่อโดยไม่ถือว่าได้พัก ถ้าไม่พบคนครบ 3 min จึงรีเซ็ตตัวนับตามช่วงการลุกเดินที่ใช้อ้างอิง [B4-6] และถ้าไม่พบคนรวมครบ 5 min ตั้ง `away_timeout=true` ให้โมดูลแสงลดพลังงาน ปุ่มกดสั้นใช้รับทราบโดยไม่รีเซ็ต session ส่วนการกดยาวตั้งแต่ 1.5 s เลื่อน alert ปัจจุบัน 5 min ได้ไม่เกิน 2 ครั้งต่อ alert เมื่อครบโควตา OLED จะแจ้งว่าไม่สามารถเลื่อนเพิ่มและคง alert ไว้ ระบบไม่มีคำสั่งปิดการเตือนถาวร

เมื่อไม่มี alert การกดยาวเปิดเมนูตั้งค่า ปุ่มกดสั้นเพิ่ม `user_lux_target` ทีละ 100 lux ภายใน 500–1,000 lux และสลับ `late_mode` ส่วนการกดยาวใช้เปลี่ยนหัวข้อหรือยืนยัน หากมี alert เข้ามาระบบออกจากเมนูเพื่อไม่ให้การกดปุ่มมีความหมายกำกวม B ส่งค่าทั้งสองเมื่อเปลี่ยนให้ D ใช้ควบคุมแสง และรีเซ็ตเป็น 500 lux กับ `late_mode=false` เมื่อ `away_timeout=true` ตาม FR-11 โดยไม่เปิดคำสั่งเปลี่ยนเกณฑ์คุณภาพอากาศหรือพัดลมแก่ผู้ใช้โต๊ะ

จอและ buzzer มี task เจ้าของเพียงตัวเดียว ทุกโมดูลในโหนดโต๊ะส่ง `alert_request` ผ่าน queue ระบบเลือก priority 2 ก่อน 1 ก่อน 0 และใช้รายการที่มาถึงก่อนเมื่อลำดับเท่ากัน C ส่งเฉพาะ `air_status` ผ่าน A ทุก 5 s โดยโหนดห้องไม่ส่ง `alert_request` ข้าม Wi-Fi B อ่าน `co2_level`, `temp_flag` และ `sensor_ok` เพื่อสร้าง `ALERT_CO2_HIGH`, `ALERT_TEMP` และ `ALERT_SENSOR_FAULT` ภายในโหนด พร้อมแสดง CO₂ อุณหภูมิ ความชื้น ระดับอากาศ และ `fan_pct` ค่าที่เก่ากว่า 15 s จะแสดงว่าไม่มีข้อมูลอากาศและ clear alert อากาศที่ล้าสมัย เมื่อ LD2410 ไม่ส่ง valid frame เกิน 500 ms ระบบใช้ fail-safe `occupied=true`, `away_timeout=false` เพื่อไม่ปิดไฟใส่คน พร้อมส่ง sensor fault priority 2 แผนภาพและ pseudocode ฉบับเต็มอยู่ใน `04_logic/B2_logic_presence.md`

การทดสอบ A5 พบและแก้ปัญหาของ B สองจุด ได้แก่ debounce เดิมอาจทิ้งการกดจริงเมื่อขอบแรกเกิดระหว่าง contact bounce จึงเปลี่ยนเป็นให้ทุกขอบเริ่มช่วงเงียบ 30 ms ใหม่ แล้วอ่านระดับ GPIO ที่นิ่งเพียงครั้งเดียว และ eye alert รอบถัดไปอาจล้าง stand alert ที่กำลัง snooze จึงกำหนดให้ stand alert ที่ค้างอยู่ครอบ eye alert จนกว่าจะกลับมาเตือน นอกจากนี้ลดชื่อ `SENSOR FAULT` บน OLED เป็น `SENSOR ERR` เพื่อไม่ให้ข้อความกว้างเกินจอ 128 พิกเซล เตือนการพักและ fault priority 2 คงเต็มจอจนถูก clear ส่วนเตือนอากาศ ลิงก์ และเซนเซอร์แสงแสดงเต็มจอ 10 s แล้วลดเป็นแถบล่างเพื่อให้ยังเห็นเวลานั่ง

### เหตุผลที่ใช้ RTOS

ระบบมีงานหลายคาบตั้งแต่ตรวจคนทุก 100 ms ไปจนถึง NTP ทุก 1 ชั่วโมง และมี I/O ที่อาจรอ Wi-Fi การใช้ FreeRTOS ช่วยแยก task ตามคาบและ priority, block งานเครือข่ายโดยไม่หยุดการตรวจคน, ส่ง alert ด้วย queue และป้องกัน I²C ด้วย mutex ที่มี priority inheritance (บท 16 หน้า 9 และ 15–21) วิธีนี้ทำให้แสดงการจัดเวลาและวิเคราะห์ตาม RMS ได้ชัดกว่า loop เดียวที่แต่ละฟังก์ชันอาจหน่วงงานถัดไป

## 6. Handling Concurrency, Timing, and Latency

ขั้นออกแบบประเมิน utilization งานควบคุมโหนดโต๊ะบน core 1 ไว้ 3.90% จาก estimated WCET ส่วน A5 รัน Wokwi ต่อเนื่อง 22.8 นาทีได้ 28.98% สำหรับ 5 งานบน core 1 เทียบกับ RMS bound 74.35% และไม่พบ deadline miss งานสื่อสารอยู่ core 0 และใช้ 0.52% เฉพาะ application task จึงไม่นำมารวมกับ RMS ของ core 1 แม้ผลรวมที่ A5 รายงานจะเป็น 29.50% `light_control_task` มี WCET 31.797 ms และ `display_alert_task` มี WCET 53.110 ms ทั้งคู่ยังต่ำกว่า deadline 200 และ 500 ms ตามลำดับ stack ของ `button_event_task` จัดสรร 3,072 B และเหลืออย่างน้อย 2,196 B ในรอบยาว ผลนี้ยังต้องยืนยันบน ESP32 จริง รายละเอียดสูตรและขอบเขตงานที่วัดอยู่ใน `05_tasks/B3_task_table.md`

OLED และ BH1750 ใช้ I²C ร่วมกัน ทุก transaction ต้องขอ `i2c_lock` รอไม่เกิน 50 ms และปล่อยทันที ห้ามหน่วงหรือรอ network ขณะถือ lock Queue ของ alert ป้องกันหลาย producer เขียนจอพร้อมกัน ส่วนข้อมูลกลางใช้ single writer และอ่านเป็น snapshot ภายใต้ critical section สั้น PWM ของไฟ พัดลม และ buzzer ใช้ hardware peripheral จึงไม่สร้าง task ความถี่สูงเพื่อ toggle GPIO

ระบบจัดเป็น soft real-time (บท 16 หน้า 30–34) เพราะการพลาด deadline หนึ่งรอบทำให้การแสดงผลหรือการควบคุมช้าลง แต่ไม่เกิดความเสียหายร้ายแรงทันที ตัวอย่างเช่น OLED ที่ข้ามหนึ่ง frame สามารถวาดใหม่ในรอบ 500 ms ถัดไปได้ ขณะเดียวกันระบบมี fail-safe สำหรับ sensor fault และ stale data เพื่อลดผลกระทบจากการพลาดเวลา

## อ้างอิง

[B4-1] Shenzhen Hi-Link Electronic Co., Ltd., “HLK-LD2410 Human Presence Sensing Module User Manual,” ver. 1.02, Jun. 8, 2022.

[B4-2] Solomon Systech Ltd., “SSD1306: 128 × 64 Dot Matrix OLED/PLED Segment/Common Driver with Controller,” rev. 1.1, Apr. 2008.

[B4-3] TDK Corp., “Piezoelectronic Buzzers: PS Series,” Feb. 2022.

[B4-4] OMRON Corp., “B3F Tactile Switch,” cat. no. A070-E1.

[B4-5] C. Talens-Estarelles et al., “The effects of breaks on digital eye strain, dry eye and binocular vision: Testing the 20-20-20 rule,” *Contact Lens and Anterior Eye*, 2022, Art. no. 101744, doi: 10.1016/j.clae.2022.101744.

[B4-6] P. Wennberg et al., “Acute effects of breaking up prolonged sitting on fatigue and cognition: A pilot study,” *BMJ Open*, vol. 6, no. 2, Art. no. e009630, 2016, doi: 10.1136/bmjopen-2015-009630.

[B4-7] เอกสารประกอบวิชา 010123119, บท 3 หน้า 30–38, บท 16 หน้า 9, 15–21 และ 30–34, บท 17 หน้า 17–31.
