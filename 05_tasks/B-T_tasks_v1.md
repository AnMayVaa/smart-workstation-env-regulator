# B-T ข้อมูลงานย่อยของโมดูลตรวจจับคนและการเตือนพัก

| ชื่องานย่อย | ทำอะไร | ทำทุกกี่ ms | ต้องเสร็จภายในกี่ ms | ใช้เวลาทำประมาณกี่ ms | ใช้ของร่วมกับงานอื่นไหม |
|---|---|---:|---:|---:|---|
| `presence_task` | อ่าน UART/สถานะ LD2410, debounce และอัปเดต `occupied` | 100 | 100 | 1 (ประมาณ) | UART เฉพาะงานนี้; เขียนข้อมูลกลางผ่าน critical section สั้น |
| `display_alert_task` | รับ `alert_request`, เลือกข้อความตาม priority, วาด OLED และกำหนดรูปแบบ buzzer | 500 และปลุกทันทีเมื่อ queue มีข้อมูล | 500; เหตุการณ์ priority 2 ควรเริ่มภายใน 500 | 8 (ประมาณ) | OLED ใช้ `i2c_lock`; queue เตือนใช้ร่วมกับทุกโมดูล |
| `break_timer_task` | นับ `sit_minutes`, ตรวจ 20/30 นาที, 3 นาทีที่ถือว่าพัก และ 5 นาทีสำหรับ `away_timeout` | 1000 | 1000 | 1 (ประมาณ) | อ่าน `occupied`; ส่ง `alert_request` เข้า queue |
| `button_event` | ISR บันทึกเวลาและส่ง event; task แยก debounce/แปล short-long press | เมื่อกด; ตรวจ debounce 30 | 100 | 0.2 (ประมาณ) | button event queue |

> เวลา execute เป็นค่าประมาณสำหรับเอกสารออกแบบ ยังไม่ใช่ผลวัดจากโปรแกรมจริง A ต้องวัด worst-case execution time (WCET) ใน A4 แล้วส่งค่ากลับมาแก้ B3 ก่อนรวมเล่ม
