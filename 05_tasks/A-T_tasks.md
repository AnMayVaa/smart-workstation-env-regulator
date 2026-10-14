# A-T ข้อมูลงานย่อยของ A (สื่อสาร + เวลา)

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.1 (ใส่ WCET ที่วัดได้จาก A5 และย้ายงานสื่อสารไป core 0) |
| วันที่ | 2026-10-14 |
| เจ้าของ | A |
| ส่งให้ | B (ใช้แทนค่าประมาณใน B3) |
| อ้างอิง | `04_logic/A3_communication.md`, `02_interface/A1_interface.md` v1.0 |

## แบบฟอร์ม T

| ชื่องานย่อย | โหนด | ทำอะไร | ทำทุกกี่ ms | ต้องเสร็จภายในกี่ ms | ใช้เวลาทำประมาณกี่ ms | ใช้ของร่วมกับงานอื่นไหม |
|---|---|---|---|---|---|---|
| `desk_communication_task` | โต๊ะ | ส่ง `desk_report`, รับ `room/air` เขียน `air_status` (สำเนา) + `link_ok`, อัปเดต `time_of_day` ทุกนาที | 5,000 (ปกติ) / 15,000 (`away_timeout`) **และ** ตื่นทันทีเมื่อ `occupied` เปลี่ยน: ช่วงห่างต่ำสุด **1,000** | 1,000 | **5.6** (วัดได้ใน A5, เฉลี่ย 5.0 · เดิมประมาณ 15) | mutex ข้อมูลกลาง (อ่าน `occupied`, `away_timeout` / เขียน `air_status`, `link_ok`, `time_of_day`), queue `alert_request` · **ไม่ใช้** `i2c_lock` |
| `ntp_sync_task` | โต๊ะ | ขอเวลา SNTP และตั้ง `time_valid` | 3,600,000 | 3,600,000 | 5 (ค่าประมาณ; การรอเครือข่ายเป็นการ block ไม่กิน CPU) | เขียน `time_valid` ภายใต้ mutex ข้อมูลกลาง |
| `room_communication_task` | ห้อง | ส่ง `room/air`, รับ `desk/+/report` และ `online`, นับ `room_occupied_count` และ `room_desks_online` | 5,000 (ตื่นหลัง `air_sensor_task` อ่านเสร็จ) | 5,000 | **5.5** (วัดได้ใน A5, เฉลี่ย 4.8 · เดิมประมาณ 15) | mutex ข้อมูลกลาง (อ่าน `air_status` / เขียนตัวนับ) · **ไม่ใช้** `i2c_lock` |

## หมายเหตุสำหรับ B3

1. **`desk_communication_task` เป็นงานแบบผสม** (ทั้งตามรอบและตามเหตุการณ์) สำหรับ RMS ให้ใช้ช่วงห่างต่ำสุด T = 1,000 ms เป็นกรณีแย่สุด เพราะ `occupied` เปลี่ยนได้ไม่ถี่กว่า 1 ครั้งต่อ 1 วินาที (B2 ต้องเห็นผลเดิม 1 วินาทีก่อนเปลี่ยน)
   - utilization = 5.6 / 1,000 = **0.0056** (ค่าวัดจริง A5 · เดิมคิดจากค่าประมาณ 15 / 1,000 = 0.015)
   - งานนี้อยู่ core 0 (ข้อ 2) จึงไม่รวมใน utilization ของ core 1 · ผล `stats` ทั้งโหนดโต๊ะ (ทุก task) = 0.274 ยังต่ำกว่า bound 73.5% (n = 6)
   - ลำดับ RMS: T = 1,000 เท่ากับ `break_timer_task` ให้ `break_timer_task` สำคัญกว่า (ความปลอดภัยของจุดขายหลัก) ส่วน `desk_communication_task` อยู่ถัดไป
   - deadline 1,000 ms มาจาก: ข้อความ `occupied` ควรถึงโหนดห้องภายในรอบนับของ C (5 วินาที) โดยมีเวลาเหลือสำหรับเครือข่าย
2. **งานของระบบที่ไม่ได้อยู่ในตาราง:** Wi-Fi driver และ TCP/IP (lwIP) ของ ESP32 เป็น task ของระบบที่ทำงานบน **core 0** [A-T-1] ในโค้ดจริง (A4) งานสื่อสารของ A ก็ pin ไว้ที่ **core 0** ด้วย เพราะต้องรอ socket ของ MQTT และตื่นทุก 100 ms เพื่อบริการ socket ส่วนงานของ B, C, D อยู่ **core 1** ทำให้การวิเคราะห์ RMS ของ core 1 ไม่ถูกรบกวนโดยงานเครือข่าย ควรเขียนข้อนี้เป็นสมมติฐานในหัวข้อ 6
3. **รอบ 1 ชั่วโมงของ NTP:** SNTP ของ ESP32 ซิงก์ซ้ำเองทุก 1 ชั่วโมง (ค่าเริ่มต้น) `ntp_sync_task` จึงทำหน้าที่ตรวจผลและตั้ง `time_valid` ใช้ CPU น้อยมาก
4. **WCET ที่วัดได้:** วัดด้วย `esp_timer_get_time()` รอบงานจริงทุกครั้งในโค้ด A4 (คำสั่ง `stats`) ระหว่างการทดสอบ A5 ใน Wokwi: โต๊ะ 5.6 ms, ห้อง 5.5 ms ต่ำกว่าค่าประมาณเดิม 15 ms (สร้าง JSON < 1 ms + ส่งเข้า buffer ของ TCP/IP) · เป็นค่าจากระบบจำลอง ใช้ดูแนวโน้ม (A-N ข้อ 2.5) ตารางเต็มทุก task อยู่ใน `06_testing/A5_results.md` ข้อ 4

## อ้างอิง

[A-T-1] Espressif Systems, "Wi-Fi Driver," *ESP-IDF Programming Guide* (Wi-Fi task pinned to core 0 by default, `CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0`). [Online]. Available: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html
