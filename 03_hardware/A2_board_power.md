# A2 บอร์ด + ระบบไฟเลี้ยง + ผังขา

| รายการ | ค่า |
|---|---|
| เวอร์ชัน | v1.1 (ตัวแทน LD2410 ใน Wokwi เป็น slide switch) |
| วันที่ | 2026-10-14 |
| เจ้าของ | A |
| ส่งให้ | ทุกคน (ดูว่าอุปกรณ์ของตัวเองต่อขาไหน), D (เล่มหัวข้อ 4) |
| ใช้ข้อมูลจาก | B1 v1.0, C1 v1, D2 v1, ข้อเสนอ MOSFET ของ D (`proposal_A_mosfet.md`) |

> เนื้อหาข้อ 1–6 ใช้ลงเล่มหัวข้อ 4 (Selection of microcontroller, Power requirements) ได้ทันที
> ภาพผังการต่อ: `07_visuals/A2_wiring_desk.png`, `07_visuals/A2_wiring_room.png`

---

## 1. บอร์ดที่เลือก: ESP32-DevKitC V4 (โมดูล ESP32-WROOM-32E) ทั้ง 2 โหนด

ใช้บอร์ดรุ่นเดียวกันทั้งโหนดโต๊ะและโหนดห้อง เพื่อให้ใช้โค้ดพื้นฐานร่วมกัน (Wi-Fi, MQTT, FreeRTOS) และมีอะไหล่ชนิดเดียว

### 1.1 เหตุผลตามเกณฑ์เลือกตัวประมวลผล (บท 10 หัวข้อ 10.3 หน้า 14–28)

| เกณฑ์ในสไลด์ | ESP32 มีอะไร [A2-1] | ระบบเราใช้ทำอะไร |
|---|---|---|
| Interrupt | ทุกขา GPIO ตั้งเป็นแหล่ง interrupt ได้ | ปุ่มของ B ใช้ interrupt แบบขอบขาลง (B1) |
| Timer | timer 64 บิต 4 ตัว (2 กลุ่ม × 2) + watchdog, LED PWM (LEDC) 16 ช่อง | PWM ของ LED 5 kHz (D1 NFR-09), พัดลม 50 Hz, buzzer 4 kHz ใช้ฮาร์ดแวร์ ไม่ต้องสร้าง task ไว้สลับขา (B3) |
| Sleep mode | 5 โหมด: Active, Modem-sleep (20–68 mA), Light-sleep (0.8 mA), Deep-sleep (10 µA), Hibernation (5 µA) | Wi-Fi modem-sleep ระหว่างรอบส่งข้อมูล (A3 ข้อ 8) |
| หน่วยความจำ | SRAM 520 KB, flash 4 MB บนโมดูล | FreeRTOS หลาย task + Wi-Fi + MQTT + buffer จอ OLED 1 KB ได้สบาย |
| ความเร็ว | dual-core Xtensa LX6 สูงสุด 240 MHz | core 0 ทำ Wi-Fi/TCP-IP, core 1 ทำงานควบคุมของ B, C, D แยกกัน (ดู A-T) |
| การเชื่อมต่อ | Wi-Fi 802.11 b/g/n + Bluetooth, I²C 2 ชุด, UART 3 ชุด, ADC 12 บิต 18 ช่อง | Wi-Fi (A3), I²C (OLED, BH1750, SCD41), UART2 (LD2410) |
| RTOS | FreeRTOS มากับ ESP-IDF และ Arduino-ESP32 | ตามแผนงานย่อย B3 |

### 1.2 เทียบกับบอร์ดอื่น

| บอร์ด | จุดเด่น | ทำไมไม่เลือก |
|---|---|---|
| **ESP32-DevKitC V4** | ครบทุกเกณฑ์ข้างบน, ราคาถูก, จำลองใน Wokwi ได้ | ✅ เลือก |
| Arduino Uno R3 (ATmega328P) | ใช้ง่าย บัดกรีง่าย | ไม่มี Wi-Fi (ต้องเพิ่มโมดูล), SRAM 2 KB ไม่พอสำหรับ RTOS + จอ OLED (buffer 1 KB) + เครือข่าย, CPU 8 บิต 16 MHz |
| ESP8266 (NodeMCU) | มี Wi-Fi ราคาถูก | core เดียว งาน Wi-Fi แย่งเวลากับงานควบคุม, มี ADC ช่องเดียว, ขา GPIO น้อย ไม่พอสำหรับโหนดโต๊ะ (I²C + UART + PWM 3 ช่อง + ปุ่ม) |
| Raspberry Pi Pico W | dual-core, มี Wi-Fi | FreeRTOS ไม่ได้มากับ SDK โดยตรง, ไม่มีให้จำลองแบบมี Wi-Fi ใน Wokwi เท่า ESP32 |

> ทีมเลือกจำลองด้วย Wokwi เพราะ Tinkercad ไม่มี ESP32 (ไฟล์ภาพรวมข้อ 1)

---

## 2. ระบบไฟเลี้ยง

### 2.1 โครงสร้าง (เหมือนกันทั้ง 2 โหนด)

```text
Adapter 12 V ─┬─ ฟิวส์ ─┬──────────────────────────────► โหลด 12 V (แถบ LED / พัดลม) ผ่าน AO3400A
              │         │
              │         └─► LM2596-5.0 (step-down) ─► ราง 5 V ─┬─► ขา 5V ของ ESP32-DevKitC
              │                                                 │      └─► AMS1117-3.3 บนบอร์ด ─► ราง 3.3 V (ESP32, I²C, ปุ่ม, buzzer, RGB)
              │                                                 └─► LD2410 (โหนดโต๊ะเท่านั้น)
              └─ GND ร่วมทุกส่วน (จุดเดียวใกล้ adapter)
```

| ส่วน | เลือก | เหตุผล |
|---|---|---|
| แหล่งจ่าย | adapter 12 V DC ตัวเดียวต่อโหนด | แถบ LED (D2) และพัดลม (C1) ใช้ 12 V อยู่แล้ว ไม่ต้องมี adapter หลายตัว |
| 12 V → 5 V | **LM2596-5.0** (step-down แบบสวิตชิ่ง รับเข้าได้ถึง 40 V จ่ายได้ 3 A) [A2-3] | ถ้าใช้ regulator แบบเชิงเส้นลดจาก 12 V เป็น 5 V ที่ 0.5 A จะร้อน (12 − 5) × 0.5 = 3.5 W ต้องมีแผ่นระบายความร้อนใหญ่ แบบสวิตชิ่งเสียพลังงานน้อยกว่ามาก (คิดประสิทธิภาพ 80% ในการคำนวณ) |
| 5 V → 3.3 V | AMS1117-3.3 ที่มากับบอร์ด DevKitC [A2-4] | จ่ายได้ 1 A พอกับโหลด 3.3 V สูงสุดที่คำนวณได้ (ข้อ 2.2–2.3) ไม่ต้องเพิ่มอุปกรณ์ |
| ฟิวส์ | polyfuse ขนาดตามข้อ 2.4 | ตัดไฟเมื่อลัดวงจร (เช่น สายแถบ LED แตะกัน) ป้องกันสายไหม้ |
| ตัวเก็บประจุ | 470 µF/25 V ที่ทางเข้า 12 V, 100 µF ที่ราง 5 V ใกล้ LD2410 | ลดแรงดันตกตอนพัดลมสตาร์ทหรือ Wi-Fi ส่งข้อมูล ESP32 จะได้ไม่รีเซ็ต |

### 2.2 ตารางไฟ: โหนดโต๊ะ

| อุปกรณ์ | ราง | ปกติ (mA) | สูงสุด (mA) | ที่มา |
|---|---|---:|---:|---|
| ESP32-WROOM-32E | 3.3 V | 100 | 240 | รับ Wi-Fi 95–100 mA, ส่ง +19.5 dBm 240 mA [A2-1] |
| OLED SSD1306 | 3.3 V | 30 | 30 | ค่าประมาณจาก B1 (ต้องวัดจริง) |
| BH1750 | 3.3 V | 0.12 | 0.19 | D2 |
| Buzzer PS1240 | 3.3 V | 0 | 5 | B1 (ดังเฉพาะตอนเตือน) |
| ปุ่ม B3F + pull-up 10 kΩ | 3.3 V | 0 | 0.33 | B1 |
| **รวมราง 3.3 V** | | **130** | **276** | AMS1117 จ่ายได้ 1,000 mA ✅ |
| LD2410 | 5 V | 80 | 200 | B1 (คู่มือให้แหล่งจ่ายรองรับ > 200 mA) |
| **รวมราง 5 V** | | **210** | **476** | LM2596 จ่ายได้ 3,000 mA ✅ |
| ราง 5 V คิดกลับเป็นกระแสที่ 12 V | 12 V | 109 | 248 | = 5 V × I ÷ 0.8 ÷ 12 V |
| แถบ LED CCT (2 ช่องรวม) | 12 V | 500–700 (เปิดเต็ม) | 1,000 | D2 (ผลรวม 2 ช่องไม่เกิน 1 ช่องเต็ม) |
| **รวมที่ adapter** | **12 V** | **≈ 0.81 A** | **≈ 1.25 A** | |

**เลือก adapter 12 V 2 A (24 W)** มีส่วนเผื่อ (2 − 1.25) ÷ 1.25 ≈ 60% สำหรับความคลาดเคลื่อนของค่าประมาณ (OLED, แถบ LED) และการเสื่อมของ adapter

### 2.3 ตารางไฟ: โหนดห้อง

| อุปกรณ์ | ราง | ปกติ (mA) | สูงสุด (mA) | ที่มา |
|---|---|---:|---:|---|
| ESP32-WROOM-32E | 3.3 V | 100 | 240 | [A2-1] |
| SCD41 (วัดทุก 5 วินาที) | 3.3 V | 15–18 | 205 | datasheet: เฉลี่ย 15 mA (สูงสุด 18), peak สูงสุด 205 mA ที่ 3.3 V [A2-5] |
| ไฟสถานะ RGB | 3.3 V | 10 | 30 | C1 (ติดทีละสี ~10 mA, กันกรณี 3 สีพร้อมกัน) |
| **รวมราง 3.3 V** | | **≈ 128** | **475** | AMS1117 1,000 mA ✅ |
| ราง 5 V คิดกลับเป็นกระแสที่ 12 V | 12 V | 67 | 247 | = 5 V × I ÷ 0.8 ÷ 12 V |
| พัดลมดูดอากาศ DC 12 V | 12 V | 250 | 400 (สตาร์ท) | C1 |
| **รวมที่ adapter** | **12 V** | **≈ 0.32 A** | **≈ 0.65 A** | |

**เลือก adapter 12 V 1.5 A (18 W)** ตามที่ C1 เสนอ มีส่วนเผื่อมากกว่า 2 เท่าของค่าสูงสุด รองรับพัดลมรุ่นที่แรงขึ้นได้ถ้าห้องใหญ่กว่าที่ออกแบบ

> ⚠️ **แก้ตัวเลขใน C1:** C1 ใช้ค่าเฉลี่ยของ SCD41 = 0.5 mA ซึ่งเป็นค่าของโหมด single shot วัด **ทุก 5 นาที** แต่ C2 วัด **ทุก 5 วินาที** (periodic measurement) ค่าที่ถูกคือเฉลี่ย 15–18 mA และ peak สูงสุด 205 mA [A2-5] A ใช้ค่าที่ถูกในตารางนี้แล้ว (แจ้ง C ผ่าน Issue)

### 2.4 พลังงานตามสถานะ (การจัดการพลังงาน)

| สถานะโหนดโต๊ะ | สิ่งที่ทำงาน | กำลังไฟโดยประมาณ |
|---|---|---|
| มีคน ไฟเปิดเต็ม | ทุกอย่าง | 12 V × 0.81 A ≈ **9.7 W** |
| ไม่มีคนครบ 5 นาที (`away_timeout`) | ปิดแถบ LED, ส่งข้อมูลทุก 15 วินาที, Wi-Fi modem-sleep, LD2410 ยังตรวจคน | ราง 5 V ≈ (ESP32 modem-sleep ≤ 68 mA + OLED 30 mA + LD2410 80 mA) × 5 V ÷ 0.8 ≈ **1.1 W** |

ลดลงประมาณ **89%** เมื่อไม่มีคน แสดงว่าตัวกินไฟหลักคือแถบ LED ฟีเจอร์ปิดไฟเมื่อไม่มีคน (FR-08) จึงมีผลด้านพลังงานมากที่สุด ส่วนการลดรอบ Wi-Fi ช่วยเพิ่มอีกเล็กน้อย

ฟิวส์: โหนดโต๊ะ polyfuse 2.5 A, โหนดห้อง 1.1 A (สูงกว่ากระแสสูงสุดที่คำนวณ แต่ไม่เกินพิกัด adapter)

---

## 3. ผังขา: โหนดโต๊ะ

| GPIO | หน้าที่ | อุปกรณ์ / ต่อผ่าน | เจ้าของ | ใน Wokwi |
|---|---|---|---|---|
| 21 | I²C SDA | OLED (0x3C) + BH1750 (0x23) บัสเดียวกัน | B, D | เหมือนจริง (OLED) |
| 22 | I²C SCL | เหมือนบรรทัดบน | B, D | เหมือนจริง |
| 16 | UART2 RX | ← TX ของ LD2410 (256,000 baud) | B | ไม่ใช้ |
| 17 | UART2 TX | → RX ของ LD2410 | B | ไม่ใช้ |
| 13 | Digital in | ขา OUT ของ LD2410 (สัญญาณมีคน สำรอง) | B | **slide switch** |
| 27 | Digital in + interrupt ขอบขาลง | ปุ่ม B3F active-low, pull-up 10 kΩ ไป 3.3 V | B | pushbutton |
| 33 | Digital in (สำรอง) | ปุ่มที่ 2 ถ้า B ต้องการแยกปุ่มตั้งค่า (FR-07, FR-10) | B | pushbutton (ถ้าใช้) |
| 26 | LEDC PWM 4 kHz | buzzer PS1240 ผ่าน R 1 kΩ | B | buzzer |
| 18 | LEDC PWM 5 kHz | Gate AO3400A ช่องขาวเย็น (CW) ผ่าน R 100 Ω, pull-down 10 kΩ | D | LED สีขาว + R 220 Ω |
| 19 | LEDC PWM 5 kHz | Gate AO3400A ช่องขาวอุ่น (WW) ผ่าน R 100 Ω, pull-down 10 kΩ | D | LED สีเหลือง + R 220 Ω |
| 34 | ADC1_CH6 (รับเข้าอย่างเดียว) | — (จริงใช้ BH1750 ทาง I²C) | D | **photoresistor** ขา AO |
| 5V / GND | ไฟเลี้ยง | จาก LM2596 / GND ร่วม | A | — |

**การตั้ง LEDC:** timer 0 = 5 kHz ความละเอียด 13 บิต (ช่อง 0 = CW, ช่อง 1 = WW) ตาม D1 NFR-09 · timer 1 = 4 kHz (ช่อง 2 = buzzer) ช่องที่ใช้ timer เดียวกันต้องมีความถี่เดียวกัน จึงแยก timer ตามความถี่

## 4. ผังขา: โหนดห้อง

| GPIO | หน้าที่ | อุปกรณ์ / ต่อผ่าน | เจ้าของ | ใน Wokwi |
|---|---|---|---|---|
| 21 | I²C SDA | SCD41 (0x62) | C | ไม่ใช้ |
| 22 | I²C SCL | SCD41 | C | ไม่ใช้ |
| 25 | LEDC PWM 50 Hz | Gate AO3400A ของพัดลม ผ่าน R 100 Ω, pull-down 10 kΩ | C | LED สีน้ำเงิน + R 220 Ω (แทนพัดลม ความสว่าง = ความแรง) |
| 18 | LEDC PWM | RGB สีแดง ผ่าน R 220 Ω | C | RGB LED (common cathode) |
| 19 | LEDC PWM | RGB สีเขียว ผ่าน R 220 Ω | C | RGB LED |
| 23 | LEDC PWM | RGB สีน้ำเงิน ผ่าน R 220 Ω | C | RGB LED |
| 35 | ADC1_CH7 (รับเข้าอย่างเดียว) | — | C | **potentiometer** แทนค่า CO₂ |
| 4 | Digital I/O | — | C | **DHT22** แทนอุณหภูมิและความชื้น |
| 5V / GND | ไฟเลี้ยง | จาก LM2596 / GND ร่วม | A | — |

ขา 18, 19, 21, 22, 23, 25 ตรงกับที่ C1 เสนอ A รับตามนั้น

**การตั้ง LEDC:** timer 0 = 50 Hz (คาบ 20 ms ตามบท 13 หน้า 95) ช่อง 0 = พัดลม · timer 1 = 5 kHz ช่อง 1–3 = RGB

## 5. ผลการตัดสินเรื่องอุปกรณ์

| เรื่อง | ผล | เหตุผล |
|---|---|---|
| MOSFET ทั้งระบบ | **AO3400A ทุกจุด** (พัดลม 1 ตัว + LED 2 ตัว) | datasheet รับรอง RDS(on) ≤ 48 mΩ ที่ VGS = 2.5 V ซึ่งต่ำกว่า 3.3 V ของ GPIO ส่วน IRLZ44N รับรองถึง VGS = 4 V เท่านั้น [A2-6], [A2-7] รายละเอียดใน `proposal_A_mosfet.md` |
| Flyback diode ของพัดลม | 1N4007 ตาม C1 ใช้ได้ | ที่ PWM 50 Hz เวลาฟื้นตัวของไดโอดไม่เป็นปัญหา ถ้าต้องการลดความร้อนตอนสลับ เปลี่ยนเป็นไดโอด Schottky (เช่น 1N5819) ได้โดยไม่ต้องแก้วงจร |
| Pull-up ของ I²C | ใช้ตัวที่มากับโมดูล (OLED, GY-302, SCD41 breakout) ถ้าไม่มี ใส่ 4.7 kΩ 1 ชุดต่อบัส | pull-up หลายชุดต่อขนานกันจะได้ค่าต่ำลง ควรเหลือไม่ต่ำกว่าประมาณ 2 kΩ เพื่อไม่ให้ขาขับกระแสเกิน |

## 6. ข้อควรระวัง

1. **GND ร่วม:** GND ของ adapter 12 V, LM2596, ESP32 และ Source ของ MOSFET ทุกตัวต้องต่อถึงกัน ไม่อย่างนั้นสัญญาณ Gate ไม่มีจุดอ้างอิง (D2, C1) ให้กระแสโหลด 12 V วิ่งกลับ adapter ตรงๆ ไม่ผ่านสาย GND ของ ESP32
2. **Flyback diode ที่พัดลม:** ต่อคร่อมขั้วพัดลมแบบย้อนขั้ว (cathode ไป +12 V) กันแรงดันเหนี่ยวนำย้อนกลับตอน MOSFET ตัดวงจร (บท 13 หน้า 35) แถบ LED ไม่ต้องมีเพราะไม่ใช่ขดลวด
3. **MOSFET เปิดเต็มที่ 3.3 V:** ใช้ AO3400A (ข้อ 5) และมี R pull-down 10 kΩ ที่ Gate กันไฟติดเองตอนเปิดเครื่องก่อนโปรแกรมตั้งค่าขา
4. **ขาที่ห้ามใช้:** GPIO 0, 2, 5, 12, 15 เป็นขา strapping กำหนดโหมดตอนบูต [A2-1] · GPIO 6–11 ต่อกับ flash ในโมดูล · GPIO 1, 3 เป็น UART0 ใช้ดู log ผ่าน USB · GPIO 34–39 รับเข้าอย่างเดียว ไม่มี pull-up ภายใน
5. **ADC ตอนเปิด Wi-Fi:** ADC2 (GPIO 0, 2, 4, 12–15, 25–27) ใช้ไม่ได้ขณะ Wi-Fi ทำงาน เพราะ Wi-Fi driver ใช้ ADC2 อยู่ [A2-8] อุปกรณ์แทนแบบ analog ใน Wokwi (photoresistor, potentiometer) จึงต้องอยู่บน ADC1 (GPIO 32–39) ทุกตัว
6. **LD2410 ต้องการไฟ 5 V แต่สัญญาณ UART เป็น 3.3 V** ต่อตรงกับ ESP32 ได้ ไม่ต้องแปลงระดับ (B1)
7. **บัส I²C:** ความเร็ว 100 kHz (standard mode) ทุกอุปกรณ์รองรับ address ไม่ชนกัน (0x3C, 0x23 / 0x62)

## 7. อุปกรณ์แทนในโปรแกรมจำลอง (Wokwi)

| ของจริง | ใน Wokwi | ข้อจำกัดที่ต้องเขียนในหัวข้อ 8 |
|---|---|---|
| ESP32-DevKitC V4 | `board-esp32-devkit-c-v4` | ตรงรุ่น |
| LD2410 (UART) | slide switch (→ GPIO 13) | ทดสอบได้แค่ logic มี/ไม่มีคน ไม่ได้ทดสอบความแม่นยำของ mmWave · ไม่ใช้ PIR เพราะ PIR ใน Wokwi ให้ HIGH แค่ช่วงสั้น จำลอง "นั่งนิ่ง 20 นาที" ไม่ได้ แต่ LD2410 จริงให้ระดับค้างตามการมีคน |
| BH1750 (I²C) | photoresistor (AO → GPIO 34) | อ่านผ่าน ADC แทน I²C และมองไม่เห็นแสงจาก LED ต้องบวกแสงจำลองในโค้ด (D2 ข้อ 5) |
| แถบ LED + AO3400A | LED 2 ดวง | ดูความสว่างแทน duty |
| SCD41 (I²C) | potentiometer (GPIO 35) + DHT22 (GPIO 4) | ไม่ได้ทดสอบเวลาอ่าน I²C ของ SCD41 จริง |
| พัดลม + AO3400A | LED สีน้ำเงิน (GPIO 25) | ดูความสว่างแทนรอบพัดลม |
| 2 โหนดคุยกัน | 2 โปรเจค Wokwi ผ่าน MQTT broker สาธารณะ | เวลาส่งข้อมูลรวมเวลาผ่านอินเทอร์เน็ต ช้ากว่าเครือข่ายในห้องจริง |

---

## 8. รายการอุปกรณ์รวมทั้งระบบ (Bill of Materials, เล่มหัวข้อ 2)

| # | อุปกรณ์ | โหนดโต๊ะ | โหนดห้อง | เจ้าของ |
|---|---|:---:|:---:|---|
| 1 | ESP32-DevKitC V4 (ESP32-WROOM-32E) | 1 | 1 | A |
| 2 | Adapter 12 V DC | 1 (2 A) | 1 (1.5 A) | A |
| 3 | โมดูล step-down LM2596-5.0 | 1 | 1 | A |
| 4 | Polyfuse | 1 (2.5 A) | 1 (1.1 A) | A |
| 5 | ตัวเก็บประจุ 470 µF/25 V, 100 µF/10 V | 1 + 1 | 1 + 1 | A |
| 6 | HLK-LD2410 | 1 | — | B |
| 7 | OLED SSD1306 128×64 I²C | 1 | — | B |
| 8 | Buzzer TDK PS1240P02BT + R 1 kΩ | 1 | — | B |
| 9 | ปุ่ม OMRON B3F-1000 + R 10 kΩ | 1 (+1 สำรอง) | — | B |
| 10 | BH1750 (GY-302) | 1 | — | D |
| 11 | แถบ LED CCT 12 V ~1 m | 1 | — | D |
| 12 | MOSFET AO3400A + R 100 Ω + R 10 kΩ | 2 ชุด | 1 ชุด | D / C |
| 13 | Sensirion SCD41 | — | 1 | C |
| 14 | พัดลมดูดอากาศ DC 12 V + ไดโอด 1N4007 | — | 1 | C |
| 15 | RGB LED common cathode + R 220 Ω × 3 | — | 1 | C |

**โปรแกรมที่ใช้:** Arduino-ESP32 core (FreeRTOS ในตัว), ไลบรารี PubSubClient (MQTT), ArduinoJson, Adafruit SSD1306, Wokwi (จำลอง), draw.io (แผนภาพ), GitHub (เก็บงาน)

## ✅ เช็คก่อนส่ง

- [x] มีเหตุผลการเลือกบอร์ดที่อ้างอิงสไลด์บท 10 และเทียบบอร์ดอื่น 3 ตัว
- [x] ตารางไฟรวมมีอุปกรณ์ครบทุกตัวของ B1, C1, D2
- [x] ผังขาไม่มีขาชนกัน ไม่ใช้ขา strapping/flash และอุปกรณ์ I²C ของแต่ละโหนดใช้สายเดียวกัน
- [x] แนบ datasheet บอร์ดแล้ว (`ESP32_series_datasheet_v5.3.pdf`, `ESP32-WROOM-32E_datasheet.pdf`, `ESP32-DevKitC-V4_schematic.pdf`, `LM2596_datasheet.pdf`)
- [ ] วัดกระแสจริงของ OLED และ buzzer (ทำได้เมื่อสร้างจริงเท่านั้น บันทึกเป็นข้อจำกัด)

## อ้างอิง

[A2-1] Espressif Systems, "ESP32 Series Datasheet," v5.3. (`03_hardware/datasheets/ESP32_series_datasheet_v5.3.pdf`)

[A2-2] Espressif Systems, "ESP32-WROOM-32E & ESP32-WROOM-32UE Datasheet." (`03_hardware/datasheets/ESP32-WROOM-32E_datasheet.pdf`)

[A2-3] Texas Instruments, "LM2596 SIMPLE SWITCHER Power Converter 150-kHz 3-A Step-Down Voltage Regulator," datasheet. (`03_hardware/datasheets/LM2596_datasheet.pdf`)

[A2-4] Espressif Systems, "ESP32-DevKitC V4 Schematic." (`03_hardware/datasheets/ESP32-DevKitC-V4_schematic.pdf`)

[A2-5] Sensirion AG, "SCD4x Datasheet," Table 4. (`03_hardware/datasheets/Sensirion_SCD41_Datasheet.pdf`)

[A2-6] Alpha & Omega Semiconductor, "AO3400A 30V N-Channel MOSFET," datasheet. (`03_hardware/datasheets/AO3400A_datasheet.pdf`)

[A2-7] International Rectifier, "IRLZ44N HEXFET Power MOSFET," datasheet. (`03_hardware/datasheets/IRLZ44N_datasheet.pdf`)

[A2-8] Espressif Systems, "Analog to Digital Converter (ADC) Oneshot Mode Driver," *ESP-IDF Programming Guide*. [Online]. Available: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/adc_oneshot.html
