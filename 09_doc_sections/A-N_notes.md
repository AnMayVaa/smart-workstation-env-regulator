# A-N บันทึกปัญหา ข้อจำกัด และอ้างอิงของ A

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v0.1 (เติมต่อระหว่าง A4–A5) |
| วันที่ | 2026-10-14 |
| เจ้าของ | A |
| ส่งให้ | D (เล่มหัวข้อ 8 และ 10) |

## 1. ปัญหาที่พบระหว่างรวมระบบ และวิธีแก้ (เล่มหัวข้อ 8)

| # | ปัญหา | พบที่ | ผลถ้าไม่แก้ | วิธีแก้ |
|---|---|---|---|---|
| 1 | IRLZ44N ไม่มีค่ารับรองการเปิดเต็มที่ VGS = 3.3 V (datasheet ระบุต่ำสุด 4 V) | C1 (D ตรวจพบ) | อ้างว่า "เปิดเต็มที่ 3.3 V" โดยไม่มีหลักฐาน | เปลี่ยนเป็น AO3400A ทั้งระบบ (A2 ข้อ 5) |
| 2 | กระแส SCD41 ใน C1 ใช้ค่าของโหมดวัดทุก 5 นาที แต่ระบบวัดทุก 5 วินาที | C1 เทียบ datasheet | ตารางไฟต่ำกว่าจริง ~30 เท่า (0.5 vs 15–18 mA, peak 205 mA) | A2 ใช้ค่าที่ถูก แจ้ง C แก้ C1 |
| 3 | ADC2 ของ ESP32 ใช้ไม่ได้ขณะเปิด Wi-Fi | ตอนวางผังขาใน Wokwi | ค่าแสงและ CO₂ จำลองอ่านไม่ได้เมื่อต่อ Wi-Fi | ย้ายอุปกรณ์ analog ทุกตัวไป ADC1 (GPIO 34, 35) |
| 4 | ถ้านับเฉพาะ `room_occupied_count` เมื่อ Wi-Fi ล่มค่าจะเป็น 0 และพัดลมปิดทั้งที่มีคน | ทบทวน C2 กับ A1 | CO₂ สะสมขณะมีคนอยู่ | เพิ่ม `room_desks_online` แยก "ไม่รู้" กับ "ว่าง" (A1 ข้อ 4) |
| 5 | C2 ร่างให้โหนดห้องส่ง `alert_request` ข้าม Wi-Fi แต่โหนดห้องไม่มีจอ และข้อความเตือนครั้งเดียวหายได้ | ทบทวน C2 กับ A1 | การเตือนเรื่องอากาศหายเมื่อข้อความหาย หรือโต๊ะเพิ่งต่อเข้ามา | ส่งสถานะผ่าน `air_status` อย่างเดียว B ตัดสินการเตือนเอง (A1 ข้อ 5) |
| 6 | ค่าที่ผู้ใช้ปรับ (FR-07, FR-10) ยังไม่มีในตารางข้อมูลกลาง และไม่ชัดว่าใครคืนค่า (FR-11) | D1 | สองโมดูลเขียนข้อมูลเดียวกัน | เพิ่ม `user_lux_target`, `late_mode` ให้ B เป็นเจ้าของคนเดียว |

## 2. ข้อจำกัดของการจำลองใน Wokwi

1. ไม่มีแบบจำลองของ LD2410, BH1750, SCD41 จึงใช้ PIR, photoresistor, potentiometer + DHT22 แทน ทดสอบได้แค่ logic ไม่ได้ทดสอบความแม่นยำของเซนเซอร์จริง
2. photoresistor มองไม่เห็นแสงจาก LED ที่จำลอง ต้องบวกแสงจำลองในโค้ด (`lux_now = lux จากตัวเลื่อน + 6 × duty`) เพื่อทดสอบวงป้อนกลับ
3. 2 โหนดอยู่คนละโปรเจค จึงคุยกันผ่าน MQTT broker สาธารณะทางอินเทอร์เน็ต เวลาส่งข้อมูลจึงช้ากว่าเครือข่ายในห้องจริง และขึ้นกับ broker ภายนอก
4. Wokwi วัดกระแสไฟไม่ได้ ตารางไฟใน A2 เป็นค่าจาก datasheet และค่าประมาณ (OLED, buzzer, แถบ LED) ต้องวัดเมื่อสร้างจริง
5. เวลาทำงานของ task ที่วัดในโปรแกรมจำลองใช้ตรวจแนวโน้มได้ แต่ไม่เท่ากับบนฮาร์ดแวร์จริงทุกประการ

## 3. แนวทางพัฒนาต่อ (ส่งให้ D ใช้ในหัวข้อ 9)

- ใช้ light-sleep อัตโนมัติของ ESP32 ร่วมกับ wake-up จากขา OUT ของ LD2410 เมื่อไม่มีคนนาน เพื่อลดไฟของโหนดโต๊ะลงอีก
- อัปเดตโปรแกรมผ่าน Wi-Fi (OTA) เมื่อมีหลายโต๊ะ ไม่ต้องถอดอุปกรณ์มาต่อสาย
- ส่งค่ารวมของห้อง (CO₂ เฉลี่ย, จำนวนชั่วโมงที่มีคน) ขึ้น cloud dashboard ผ่าน broker เดิม (บท 24) โดยไม่ส่งข้อมูลรายโต๊ะ (NFR-16)

## 4. อ้างอิงที่ A ใช้ (ส่งให้ D จัดเลข IEEE ทั้งเล่ม)

- Espressif Systems, "ESP32 Series Datasheet," v5.3.
- Espressif Systems, "ESP32-WROOM-32E & ESP32-WROOM-32UE Datasheet."
- Espressif Systems, "ESP32-DevKitC V4 Schematic."
- Espressif Systems, *ESP-IDF Programming Guide*: "Wi-Fi Driver," "System Time," "Analog to Digital Converter (ADC) Oneshot Mode Driver." [Online]. Available: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/
- Texas Instruments, "LM2596 SIMPLE SWITCHER Power Converter 150-kHz 3-A Step-Down Voltage Regulator," datasheet.
- Sensirion AG, "SCD4x Datasheet."
- Alpha & Omega Semiconductor, "AO3400A 30V N-Channel MOSFET," datasheet.
- International Rectifier, "IRLZ44N HEXFET Power MOSFET," datasheet.
- OASIS, "MQTT Version 3.1.1," OASIS Standard, Oct. 2014.
- Real Time Engineers Ltd., "Mutexes," *FreeRTOS Documentation*.

## 5. ลิงก์โค้ด

- Repository: https://github.com/AnMayVaa/smart-workstation-env-regulator (โค้ด A4 จะอยู่ใน `08_code/`)
