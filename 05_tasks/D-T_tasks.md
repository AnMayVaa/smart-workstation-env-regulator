# D-T ข้อมูลงานย่อยของโมดูลแสง

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1 |
| วันที่ | 2026-10-14 |
| เจ้าของ | D |
| ส่งให้ | B (ใช้ใน B3) |

| ชื่องานย่อย | ทำอะไร | ทำทุกกี่ ms | ต้องเสร็จภายในกี่ ms | ใช้เวลาทำประมาณกี่ ms | ใช้ของร่วมกับงานอื่นไหม |
|---|---|---:|---:|---:|---|
| `light_control_task` | อ่านค่า `lux_now` จาก BH1750, ปรับความสว่างรวมแบบป้อนกลับให้ได้ค่าเป้าหมาย (500 lux หรือ `user_lux_target`), คำนวณสัดส่วนแสงขาวเย็น/ขาวอุ่นจาก `time_of_day` และ `late_mode`, ตั้ง duty ของ PWM 2 ช่อง, หรี่และปิดไฟเมื่อ `away_timeout` เป็นจริง | 200 | 200 | 2 (ประมาณ) | สาย I²C ร่วมกับ OLED ของ B ต้องขอ `i2c_lock` (รอไม่เกิน 50 ms ถ้าไม่ได้ให้ข้ามรอบ) / อ่าน `occupied`, `away_timeout`, `time_of_day`, `time_valid`, `user_lux_target`, `late_mode` แบบ snapshot / ส่ง `alert_request` เมื่อ BH1750 อ่านไม่ได้ 3 รอบติดกัน |

## หมายเหตุ

- **มีงานย่อยงานเดียว** การเปลี่ยนสีตามเวลา การรับคำสั่งผู้ใช้ และการหรี่ไฟ ทำอยู่ในรอบเดียวกันของ `light_control_task` ไม่ต้องแยก task เพิ่ม
- **ทำไมใช้เวลาแค่ประมาณ 2 ms ทั้งที่ BH1750 วัดค่า 1 ครั้งใช้ 120 ms (สูงสุด 180 ms):** ตั้ง BH1750 เป็นโหมดวัดต่อเนื่อง (Continuous H-Resolution) เซนเซอร์วัดเองตลอด งานนี้แค่อ่านผลล่าสุด 2 ไบต์ทาง I²C จึงไม่ต้องรอ 120 ms
- **ทำไมเลือกรอบ 200 ms:** ยาวกว่าเวลาวัดสูงสุดของ BH1750 (180 ms) ทุกครั้งที่อ่านจึงได้ค่าใหม่เสมอ และเร็วพอให้ไฟปรับตามแสงภายนอกได้ทันตาม NFR-02 (กลับมา 500 ± 50 lux ภายใน 10 วินาที)
- **การค่อยๆ เปลี่ยนแสง (NFR-10, NFR-11) ไม่กิน CPU:** ใช้ hardware fade ของ LEDC ใน ESP32 ชิปไล่ค่า duty เอง task แค่สั่งค่าปลายทาง
- **PWM ไม่ใช่ task:** ใช้ LEDC ของ ESP32 ที่ 5 kHz (NFR-09) เป็น hardware peripheral จึงไม่มีงานย่อยที่ต้องสลับขา GPIO เอง
- ค่าเวลาเป็น **ค่าประมาณ** สำหรับเอกสารออกแบบ A ต้องวัด WCET จริงใน A4
- ชื่อ task และค่า period / deadline / WCET ตรงกับที่ B ใส่ไว้ใน B3 แล้ว (`light_control_task`, 200 / 200 / 2.0 ms)

## อ้างอิง

- ROHM Semiconductor, "BH1750FVI: Digital 16-bit serial output type ambient light sensor IC," datasheet. (เวลาวัด H-Resolution 120 ms ทั่วไป, 180 ms สูงสุด)
- Espressif Systems, "LED Control (LEDC)," *ESP-IDF Programming Guide*. [Online]. Available: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/ledc.html
- เอกสารประกอบวิชา 010123119 บท 17 หน้า 19–21 (ค่า c, d, p และ utilization)
