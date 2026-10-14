# ทดสอบอัตโนมัติด้วย Wokwi CLI

ชุดนี้รันการทดสอบใน `06_testing` แบบไม่ต้องคลิกเอง: สั่ง Serial, กดปุ่ม, หมุน LDR, ถ่ายภาพจอ OLED และเก็บ VCD ของ logic analyzer ทุกครั้ง
ผลล่าสุดอยู่ที่ `06_testing/evidence/2026-10-14_wokwi_ci/`

## ไฟล์

| ไฟล์ | หน้าที่ |
|---|---|
| `build_ci.sh` | build firmware 7 แบบจาก `../desk_node` และ `../room_node` (แต่ละกลุ่มใช้ MQTT prefix ของตัวเอง) |
| `gen.py` | สร้าง scenario `.yaml` ทุกไฟล์ (แก้ขั้นตอนทดสอบที่นี่ แล้วรันใหม่) |
| `run_ci.py` | รัน 1–3 โหนดพร้อมกัน เก็บ `console.log` (มีเวลา wall), `serial.log`, `logic_analyzer.vcd` และภาพ |
| `vcdsum.py`, `beeps.py` | สรุปความถี่/duty ของ PWM และเสียง buzzer จาก VCD |

## วิธีรัน

```bash
NONCE=r2 ./build_ci.sh                  # เปลี่ยน NONCE ทุกรอบ กัน retained/LWT ของรอบก่อน
python3 gen.py
export WOKWI_TOKEN_FILE=~/Documents/GitHub/.wokwi-token   # token อยู่นอก repo เสมอ
python3 run_ci.py room_a room_node:room_a.yaml:120000
python3 run_ci.py desk_a desk_node:desk_a.yaml:120000
python3 run_ci.py int room_int:int_room.yaml:150000 desk_node_int:int_desk.yaml:150000
STAGGER=6 python3 run_ci.py multi room_multi:multi_room.yaml:90000 desk_multi1:multi_desk.yaml:90000 desk_node_2:multi_desk.yaml:90000
python3 vcdsum.py desk_node/out/desk_a/logic_analyzer.vcd D0=CW D1=WW D2=BUZ
```

## ข้อควรรู้

- `desk_node` (unit test) ใช้ `TIME_SCALE 120`: 1 วินาทีจำลอง = 2 นาทีระบบ (EYE 10 s, STAND 15 s, snooze 2.5 s, away 2.5 s) แต่ DIM hold 60 s และ fade ยังเป็นเวลาจริง
- ชุด int/multi ใช้ `TIME_SCALE 1` เพื่อไม่ให้เตือนพักแทรกตอนทดสอบการสื่อสาร
- Wokwi จำลองช้ากว่าเวลาจริง (~0.3–0.7×) จึงตั้ง MQTT keepalive 60 s ใน build จำลอง
- แผนฟรีมีโควตา CI รายเดือน ชุดเต็มใช้ราว 10 นาทีจำลอง และรันพร้อมกันได้ 2 โหนดอย่างเสถียร (3 โหนดให้เว้น `STAGGER`)
- `wait-serial` จับเฉพาะบรรทัดที่พิมพ์หลังเริ่ม step นั้น จึงต้องรอ log ทันทีหลังปล่อยปุ่ม ห้ามคั่น delay
