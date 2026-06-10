# SISTEL-HL7-Monitor

Real‑time HL7 telemonitoring system with bidirectional communication between biomedical sensors (BITalino) and a PC gateway. Implements HL7 v2.6 standard messages (ORU_R01, ADT_A05, OMD_O03) with full syntactic validation, automatic ACK generation, and graphical visualization of vital signs. Developed as part of the *Telemonitoring Systems* course at UPNA (Biomedical Engineering, Autumn 2025).

---

## Features

**Complete HL7 v2.6 Workflow** — Bidirectional communication between sensor node (ESP32) and PC gateway using MLLP encoding:
- ORU_R01 — Unsolicited observation messages (vital signs)
- ADT_A05 — Pre‑admit patient administration messages
- OMD_O03 — Dietary order messages
- Z‑control messages — Start/stop communication and configure sensor requests
- ACK messages — Positive (AA) or error (AR) acknowledgments

**Multi‑Sensor Support** — Up to 4 simultaneous biomedical sensors via BITalino:
- Accelerometer (LOINC: 80493-0) — units: m/s²
- Pulse oximeter (LOINC: 20564-1) — units: % SpO₂
- Thermometer (LOINC: 8310-5) — units: °C
- Blood pressure monitor (LOINC: 55284-4) — units: mmHg (systolic/diastolic)

**Error Detection & Handling** — Automatic syntactic validation of all HL7 messages with configurable error injection for testing:
- **200** — Unsupported message type (ORX, OMX, ADX)
- **201** — Unsupported event code (R0X, A0X, O0X)
- **202** — Unsupported processing ID (X instead of T/D/P)
- **203** — Unsupported HL7 version (3.0 instead of 2.6)
- **101** — Required field missing (per segment: MSH, PID, OBR, OBX, PV1, AL1, ORC, ODS, NTE)

**Real‑time Visualization** — PyQt5‑based GUI with:
- Configurable message Z generation (type, sensors, errors)
- Live preview of HL7 messages before transmission
- Separate sensor windows with real‑time graphs (Matplotlib)
- Adjustable min/max threshold alerts (visual warnings)
- Gateway log console (MLLP traffic, errors, ACKs)

**Hardware Integration** — ESP32‑based architecture:
- **Sensor Node** — Reads BITalino sensors, generates HL7 messages, handles Z commands and ACKs
- **Gateway Node** — Bidirectional Bluetooth bridge between sensor and PC (no processing, pure MLLP relay)

---

## Repository Structure

```
├── Anexo/
│    ├── arduino/
      ├── SENSOR/
        └── SENSOR.ino # Sensor node firmware (ESP32)
│     └── GATEWAY/ 
│       └── GATEWAY.ino # Gateway node firmware (ESP32)
│    
│    ├── interfaz/
│     ├── Interfaz.py # Main PyQt5 application
│     ├── UI_Interfaz.py # Generated UI (from Qt Designer)
│     ├── SensorWindow.py # Per‑sensor real‑time graph window
│     └── NonORUWindow.py # Non‑ORU message display window
│    
│    ├── libraries/ # Required Arduino libraries
│     ├── BitaHealth/ # BITalino sensor library
│     ├── WCommMaster/ # Bluetooth master communication
│     ├── WCommSlave/ # Bluetooth slave communication
│     ├── HL7_Sender/ # HL7 message builder
│     └── HL7_Receiver_Master/ # HL7 message parser
│
├── Grupo05SISTEL.pdf # Project report
├── Anexo-Grupo05SISTEL.pdf # Appendix with HL7 message examples
│
├── .gitignore
└── README.md
```

---

## Dependencies

### Python (PC side)
- **Python 3.11 or higher**
- **PyQt5** — GUI framework
- **PySerial** — Serial communication with gateway
- **Matplotlib** — Real‑time graphing
- **NumPy** — Numerical operations

### Arduino (ESP32 side)
- **ESP32 board support** (Arduino ESP32 core v2.0+)
- **Bluetooth Classic** (Built‑in, no external libraries)
- Custom libraries (provided in `/libraries`):
  - `BitaHealth` — BITalino sensor communication
  - `WCommMaster` / `WCommSlave` — Bluetooth wrapper
  - `HL7_Sender` / `HL7_Receiver_Master` — HL7 v2.6 message builder/parser

### Hardware
- 2 × ESP32 development boards
- 1 × BITalino (r)evolution board
- Sensors: Accelerometer, pulse oximeter, temperature, blood pressure (optional, software‑emulated fallback)

---

## HL7 Message Flow
```
PC GUI                          GATEWAY                         SENSOR
   |                               |                               |
   |--- Z (INICIO, ORU_R01) ---→   |--- (Bluetooth) Z (INICIO) ---→|
   |                               |                               |
   |                               |           (process Z, start sensors)
   |                               |                               |
   |                               |←--- ORU_R01 (VT...FS+CR) ----|
   |←--- (MLLP) ORU_R01 -----------|                               |
   |                               |                               |
   | (validate, plot, send ACK)    |                               |
   |                               |                               |
   |--- ACK (AA) ----------------→ |--- (Bluetooth) ACK (AA) ----→|
   |                               |                               |
   |                               |           (continue ORU every 5s)
   |                               |                               |
   | (repeat ORU/ACK cycle)        |                               |
   |                               |                               |
   |--- Z (FINAL) ----------------→ |--- (Bluetooth) Z (FINAL) ---→|
   |                               |                       (stop)
```
---

## Error injection Examples

| Error | Effect on ORU_R01 | MSH Field Modified |
|---|---|---|
| 200 | Message type becomes `ORX^R01^ORX_R01` | MSH-9 |
| 201 | Event code becomes `ORU^R0X^ORU_R0X` | MSH-9 |
| 202 | Processing ID becomes `X` | MSH-11 |
| 203 | Version becomes `3.0` | MSH-12 |
| 101 | PID-3 (patient ID) removed | PID segment |

When an error is injected, the sensor sends a malformed message. The PC:

1. Detects the error (syntactic validation)

2. Opens sensor windows with error highlighted in red

3. Sends ACK with AR + ERR segment

4. Stops the communication automatically

---

## GUI Features

**Main Window**

- **Control panel** — INICIO / FIN buttons

- **Message configuration** — Type, sensors, error injection

- **Preview panel** — Shows the Z message before sending

- **Gateway log** — Real‑time MLLP traffic, errors, ACKs

**Sensor Window (ORU_R01)**

- Sensor information panel (ID, type, status)

- Received message display (full HL7 segment)

- Requested error display (what was configured)

- Detection error display (what was actually found)

- Real‑time graph with:

    - Dynamic Y‑axis scaling based on sensor type

    - Threshold alerts (red toast notification when min/max exceeded)

    - Last value annotation

- Color coding: Green = no error, Red = error detected

**Non‑ORU Window (ADT_A05, OMD_O03)**

- Full message display

- Error detection panel (green/red)

---

## Authors

- **Paula Crespo**
- **Cristina García**

*Students of Biomedical Engineering — Universidad Pública de Navarra (UPNA)*