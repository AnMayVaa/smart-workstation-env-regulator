# C-N: บันทึกปัญหา ข้อจำกัด แนวทางพัฒนาต่อยอด และแหล่งอ้างอิง โมดูลอากาศ (v1)

> **รหัสเอกสาร:** C-N_notes_v1  
> **ผู้จัดทำ:** C (ซัน - ทิวากร ฉับพลัน 6701012610112)  
> **ส่งให้:** D (กล้อง - ใช้รวบรวมลงหัวข้อ 8, 9 และ 10 ของรายงานฉบับสมบูรณ์)  
> **สถานะความสำคัญ:** 🟢 ใช้ตอนรวมเล่ม

---

## 1. ปัญหาทางเทคนิคและข้อจำกัดของโมดูลอากาศ (สำหรับหัวข้อ 8: Challenges & Limitations)

### 1.1 ความท้าทายทางเทคนิคและการจำลอง (Technical & Simulation Challenges)
1. **ข้อจำกัดของโปรแกรมจำลอง Wokwi ต่อเซนเซอร์ก๊าซ $\text{CO}_2$:**
   - ในโปรแกรม Wokwi ไม่มีโมเดลจำลองเซนเซอร์ Sensirion SCD41 ที่วัดก๊าซ $\text{CO}_2$ แบบ Photoacoustic NDIR โดยตรง จึงต้องออกแบบการจำลองทดแทน (Hardware Abstraction / Simulation Mocking) โดยใช้ตัวต้านทานปรับค่าได้ (Potentiometer) ต่อเข้าขา ADC เพื่อจำลองระดับความเข้มข้น $\text{CO}_2$ ในช่วง $400 - 2000\ \text{ppm}$ คู่กับเซนเซอร์ DHT22 สำหรับจำลองค่าอุณหภูมิและความชื้น
2. **ความเฉื่อยทางกายภาพของก๊าซในห้อง (Physical Thermal & Gas Inertia):**
   - การระบายก๊าซ $\text{CO}_2$ ออกจากห้องจริงไม่ได้ลดลงทันทีที่พัดลมหมุน แต่มีช่วงเวลาหน่วง (Transport Delay) ตามปริมาตรห้องและอัตราการไหลของอากาศ หากใช้การควบคุมแบบตั้งเกณฑ์จุดเดียว (Single Threshold) ที่ $1000\ \text{ppm}$ จะเกิดปัญหาพัดลมเปิด-ปิดสลับกันถี่ๆ (Chattering) จึงต้องแก้ปัญหาด้วยการออกแบบแถบ **Hysteresis ($800 - 1000\ \text{ppm}$)** และการปรับความเร็วรอบแบบหลายระดับ

### 1.2 ข้อจำกัดของการออกแบบปัจจุบัน (Limitations of Current Design)
1. **พัดลมดูดอากาศช่วยลด $\text{CO}_2$ แต่ดึงอากาศร้อน/ชื้นจากภายนอกเข้ามา:**
   - เมื่อพัดลมดูดอากาศเสียออกจากห้องแอร์ อากาศจากโถงทางเดินหรือภายนอกจะไหลเข้ามาแทนที่ หากอากาศภายนอกมีอุณหภูมิสูง อาจทำให้เครื่องปรับอากาศในห้องต้องทำงานหนักขึ้น
2. **ระบบทำได้เพียงแจ้งเตือนอุณหภูมิ แต่ไม่ได้สั่งการเครื่องปรับอากาศโดยตรง:**
   - เพื่อไม่ให้ระบบซับซ้อนเกินขอบเขตและรองรับห้องทำงานทุกแบบ ระบบจึงใช้การแจ้งเตือนให้ผู้ใช้ปรับรีโมทแอร์เองเมื่ออุณหภูมิออกนอกช่วง $20-26^\circ\text{C}$

---

## 2. แนวทางการพัฒนาต่อยอดในอนาคต (สำหรับหัวข้อ 9: Future Improvements)

1. **การควบคุมพัดลมแบบ Closed-Loop PID หรือการทำระบบแลกเปลี่ยนความร้อน (ERV - Energy Recovery Ventilator):**
   - เปลี่ยนจากการปรับความเร็วพัดลมแบบขั้นบันได (0%, 60%, 100%) เป็นการควบคุมแบบ Proportional-Integral (PI Controller) ที่ปรับรอบพัดลมอย่างละเอียดตามอัตราการเพิ่มขึ้นของ $\text{CO}_2$ ($\frac{d(\text{CO}_2)}{dt}$) และติดตั้งชุดแลกเปลี่ยนความร้อนก่อนปล่อยอากาศออก เพื่อรักษาความเย็นในห้องแอร์
2. **การเพิ่มตัวส่งสัญญาณอินฟราเรด (IR Blaster) ควบคุมเครื่องปรับอากาศอัตโนมัติ:**
   - ต่อยอดจากฟีเจอร์แจ้งเตือนอุณหภูมิ โดยเพิ่มวงจร IR LED บนโหนดห้องเพื่อส่งคำสั่งปรับอุณหภูมิแอร์ให้อยู่ในช่วง $21-22^\circ\text{C}$ โดยอัตโนมัติ
3. **การตรวจวัดฝุ่นละอองขนาดเล็ก ($\text{PM}_{2.5}$) และสารประกอบอินทรีย์ระเหยง่าย (VOCs):**
   - เพิ่มเซนเซอร์วัดฝุ่น (เช่น PMS5003 หรือ SEN55) เพื่อตรวจสอบว่าอากาศภายนอกที่จะดึงเข้ามามีฝุ่น $\text{PM}_{2.5}$ สูงหรือไม่ หากภายนอกฝุ่นสูง ระบบจะสลับไปสั่งเปิดเครื่องฟอกอากาศภายในห้องแทนการดูดอากาศจากภายนอก

---

## 3. รายการอ้างอิงและ Datasheet ของโมดูล C (รูปแบบ IEEE สำหรับหัวข้อ 10: References)

### 3.1 งานวิจัยและมาตรฐานที่ใช้ในโมดูลอากาศ (ตรงกับรายการกลางของทีม + เพิ่มเติมมาตรฐาน)
- **[1]** J. G. Allen, P. MacNaughton, U. Satish, S. Santanam, J. Vallarino, and J. D. Spengler, "Associations of cognitive function scores with carbon dioxide, ventilation, and volatile organic compound exposures in office workers: A controlled exposure study of green and conventional office environments," *Environ. Health Perspect.*, vol. 124, no. 6, pp. 805–812, Jun. 2016, doi: 10.1289/ehp.1510037.
- **[2]** U. Satish et al., "Is CO2 an indoor pollutant? Direct effects of low-to-moderate CO2 concentrations on human decision-making performance," *Environ. Health Perspect.*, vol. 120, no. 12, pp. 1671–1677, Dec. 2012, doi: 10.1289/ehp.1104789.
- **[5]** O. Seppänen, W. J. Fisk, and Q. H. Lei, "Effect of temperature on task performance in office environment," Lawrence Berkeley National Laboratory, Berkeley, CA, USA, Tech. Rep. LBNL-60946, Jul. 2006.
- **[6]** A. A. Abusharha and E. I. Pearce, "The effect of low humidity on the human tear film," *Cornea*, vol. 32, no. 4, pp. 429–434, Apr. 2013, doi: 10.1097/ICO.0b013e31826671ab.
- **[C-Ref-1]** *Ventilation for Acceptable Indoor Air Quality*, ANSI/ASHRAE Standard 62.1-2022, American Society of Heating, Refrigerating and Air-Conditioning Engineers, Atlanta, GA, USA, 2022.

### 3.2 เอกสารข้อมูลจำเพาะของอุปกรณ์ (Hardware Datasheets)
- **[C-DS-1]** Sensirion AG, *SCD4x – Breaking the Size Barrier in CO2 Sensing (SCD40/SCD41 Datasheet)*, Version 1.7, Apr. 2025. (ตรงกับไฟล์ `Sensirion_SCD41_Datasheet.pdf` ใน `03_hardware/datasheets/`)
- **[C-DS-2]** Alpha & Omega Semiconductor, *AO3400A 30V N-Channel MOSFET Datasheet*, Rev. 5.0, Dec. 2020. (ตรงกับไฟล์ `AO3400A_datasheet.pdf` ใน `03_hardware/datasheets/`)
- **[C-DS-3]** Vishay General Semiconductor, *1N4001 through 1N4007 General Purpose Plastic Rectifier Datasheet*, Rev. 1.8, Feb. 2018. (ตรงกับไฟล์ `1N4007_Datasheet.pdf` ใน `03_hardware/datasheets/`)
- **[C-DS-4]** Infineon Technologies / International Rectifier, *IRLZ44NPbF HEXFET Power MOSFET Logic-Level Gate Drive Datasheet*, Oct. 2012. (สำหรับอ้างอิงเปรียบเทียบใน `03_hardware/datasheets/IRLZ44N_datasheet.pdf`)
- **[C-DS-5]** Hanwei Electronics Co., Ltd., *MQ-135 Semiconductor Sensor for Air Quality Datasheet*, 2015. (สำหรับอ้างอิงเปรียบเทียบคุณลักษณะเซนเซอร์เคมี)
