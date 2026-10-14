# B-TC Test case โมดูลตรวจจับคนและการเตือนพัก

| รหัส | ประเภท | ทดสอบอะไร | สภาพก่อนเริ่ม | ใส่อะไรเข้าไป / ทำอะไร | ควรได้ผลอะไร | ตรวจ requirement ข้อไหน | ทดสอบใน Wokwi ได้ไหม |
|---|---|---|---|---|---|---|---|
| TC-B-01 | unit | เตือนพักสายตาเมื่อทำงานต่อเนื่องครบ 20 นาที | `occupied=true`, ไม่มี alert, ตัวนับเริ่มที่ 0 | เร่งเวลาจำลองจน `sit_elapsed=20 นาที` | queue มี `ALERT_EYE_BREAK`; OLED แสดงให้มองไกล 20 วินาทีและ buzzer ดังสั้นภายใน 1 s | FR-03, NFR-04, NFR-14 | ได้ โดยใช้ PIR และ time acceleration |
| TC-B-02 | unit | เตือนลุกเมื่อทำงานต่อเนื่องครบ 30 นาทีและรีเซ็ตหลังพักจริง | `occupied=true`, ตัวนับเริ่มที่ 0 | เร่งถึง 30 นาที แล้วตั้ง `occupied=false` ต่อเนื่อง 3 นาที ก่อนกลับเป็น `true` | เกิด `ALERT_STAND_BREAK` ที่ 30 นาที; หลังไม่พบคนครบ 3 นาที ตัวนับและ due time เริ่มใหม่เมื่อกลับมานั่ง | FR-01, FR-02, FR-04 | ได้ |
| TC-B-03 | system/fault | เซนเซอร์ตรวจคนเสียแล้วระบบไม่ปิดไฟใส่ผู้ใช้ | ระบบกำลังทำงานและเคยมีคน | หยุดส่ง frame ของ LD2410 เกิน 500 ms | ตั้งค่า fail-safe `occupied=true`, `away_timeout=false`, ส่ง `ALERT_SENSOR_FAULT` priority 2; ต้องไม่ส่งเงื่อนไขปิดไฟ | NFR-07 | ได้ โดยเพิ่มสวิตช์จำลอง sensor fault |
| TC-B-04 | unit/UI | จำกัดการเลื่อนเตือนไม่เกิน 2 ครั้งต่อ alert | อยู่ใน `EYE_ALERT` หรือ `STAND_ALERT`, `snooze_count=0` | กดยาว ≥1.5 s สามครั้ง โดยเร่งเวลาให้ alert กลับมาหลังครั้งที่ 1 และ 2 | ครั้งที่ 1–2 เลื่อน 5 นาทีและเพิ่มตัวนับ; ครั้งที่ 3 ไม่เลื่อน ไม่ clear alert และ OLED แสดงว่าเลื่อนได้สูงสุด 2 ครั้ง; ไม่มีสถานะปิดการเตือนถาวร | FR-05, NFR-14 | ได้ โดยใช้ปุ่มและ time acceleration |
| TC-B-05 | unit/UI | ตั้งค่าความสว่างด้วยปุ่มเดียวภายในช่วงที่อนุญาต | ไม่มี alert, หน้า `STATUS`, `user_lux_target=500` | กดยาวเข้าเมนู แล้วกดสั้นซ้ำจนผ่าน 1,000 lux | OLED แสดง 500, 600, …, 1,000 lux และวนกลับ 500; publish เฉพาะเมื่อค่าเปลี่ยนและไม่มีค่านอกช่วง | FR-07, NFR-15 | ได้ |
| TC-B-06 | integration/UI | เปิดโหมดทำงานดึกและรีเซ็ตค่าผู้ใช้เมื่อโต๊ะว่าง | ไม่มี alert และยังมีคน | ตั้ง 800 lux, เปิด `late_mode`, จากนั้นจำลองไม่พบคนครบ 5 นาที | ก่อน timeout ส่ง 800/true ให้ D; เมื่อครบ 5 นาทีตั้ง `away_timeout=true`, รีเซ็ตและส่ง 500/false | FR-08, FR-10, FR-11 | ได้ โดยใช้ time acceleration |
| TC-B-07 | integration | B แสดง `air_status` และสร้าง alert อากาศเอง | A เขียนสำเนา `air_status` ใหม่, `age_s<=15` | ป้อน `co2_level=2`; `temp_flag=1/2`; `sensor_ok=false` ทีละกรณี พร้อมค่า `fan_pct` | B สร้าง/clear `ALERT_CO2_HIGH`, `ALERT_TEMP`, `ALERT_SENSOR_FAULT` จาก `SRC_AIR` เมื่อสถานะเปลี่ยน; OLED แสดง CO₂ อุณหภูมิ ความชื้น ระดับ และ `fan_pct`; ไม่มี alert ข้าม Wi-Fi | FR-14, FR-16, FR-17, FR-18, FR-19, NFR-14 | ได้ โดย mock shared data/queue |
| TC-B-08 | integration/fault | ไม่ใช้ข้อมูลอากาศล้าสมัย | มี alert อากาศ active และ `link_ok=true` | เพิ่ม `air_status.age_s` ให้เกิน 15 หรือจำลองขาด `room_report` | clear alert อากาศที่ค้าง, แสดง “ไม่มีข้อมูลอากาศ”; A ส่ง `ALERT_LINK_LOST` เมื่อ link ขาด ขณะที่การเตือนพักของ B ยังทำงาน | FR-19, NFR-06, NFR-07 | ได้ โดย time acceleration |

## หมายเหตุส่งให้ A และ C

- ให้แผนทดสอบรวมอ้างรหัส TC-B-01 ถึง TC-B-08 จากไฟล์นี้; branch C ยังใช้ความหมาย TC-B-01 ถึง TC-B-04 คนละลำดับและต้อง sync ก่อนรวม
- A ต้อง mock/route `user_lux_target`, `late_mode` และ `air_status`; ห้าม route `alert_request` ข้าม Wi-Fi ตาม A1 v1.0
- ใน Wokwi ห้ามเร่ง period ของ task 100/200/500/1,000/5,000 ms เพราะใช้วิเคราะห์ real-time; ให้ `TIME_SCALE` เปลี่ยนเฉพาะช่วง 20/30/3/5 นาที และรัน TC-B-01 ที่ `TIME_SCALE=1` อย่างน้อยหนึ่งครั้ง
