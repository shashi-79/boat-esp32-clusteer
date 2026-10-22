# Rescue Boat — Complete Circuit Diagram & Hardware Wiring Reference

This document details every hardware connection, pinout configuration, power distribution circuit, and resistor divider schematic for the Dual-ESP32 Rescue Boat system.

---

## 1. System Block Diagram & Power Architecture

```
                                  ┌───────────────────────────────┐
                                  │      3S/4S Motor LiPo         │
                                  │   (11.1V - 16.8V, High-C)     │
                                  └───────┬───────────────┬───────┘
                                          │ (+)           │ (-)
                                          ▼               │
                                  ┌───────────────┐       │
                                  │  INA219 VIN+  │       │
                                  │ (Current Shunt│       │
                                  │  0.1Ω Resistor│       │
                                  │  INA219 VIN-  │       │
                                  └───────┬───────┘       │
                                          │               │
                 ┌────────────────────────┴─────────┐     │
                 │ (+) Load                         │     │
                 ▼                                  ▼     ▼
    ┌─────────────────────────┐         ┌─────────────────────────┐
    │  Dual Brushless ESCs    │         │     5V 3A Step-Down     │
    │  (Left & Right Motors)  │         │     BEC / Buck Reg      │
    └─────────────────────────┘         └────────────┬────────────┘
                                                     │ 5V Rail
                         ┌───────────────────────────┼───────────────────────────┐
                         ▼                           ▼                           ▼
                 ┌───────────────┐           ┌───────────────┐           ┌───────────────┐
                 │  Pan / Tilt   │           │   ESP32-CAM   │           │  HLK-LD2450   │
                 │ 5x PWM Servos │           │ Camera Node   │           │ mmWave Radar  │
                 └───────────────┘           └───────────────┘           └───────────────┘

                 ┌───────────────────────────────┐
                 │    2S Logic LiPo / 5V Bank    │
                 │      (7.4V - 8.4V Logic)      │
                 └───────┬───────────────┬───────┘
                         │ (+)           │ (-)
                         ▼               │
                 ┌───────────────┐       │
                 │ 5V LDO / BEC  │       │
                 └───────┬───────┘       │
                         │ 5V            │
                         ▼               ▼
                 ┌───────────────────────────────┐
                 │     ESP32-S3 DevKitC-1        │
                 │    (Master Controller)        │
                 └───────┬───────────────┬───────┘
                         │ 3.3V Out      │
                         ▼               │
                 ┌───────────────┐       │
                 │ BNO055 9-DOF  │       │
                 │   IMU / Mag   │       │
                 └───────────────┘       │
                                         ▼
                 ═════════════════════════════════
                 COMMON GROUND BUS (ALL GNDs TIED)
                 ═════════════════════════════════
```

> [!CAUTION]
> **Common Ground Requirement:** All ground lines (LiPo Battery (-), Logic Battery (-), BEC GND, ESP32-S3 GND, ESP32-CAM GND, ESC GND, Servos GND, and Sensor GND) **MUST** be connected to a shared central ground bus. If grounds are split or floating, PWM signals will jitter and serial communication will fail.

> [!WARNING]
> **Servo Power Isolation:** Never power servos or the ESP32-CAM from the ESP32-S3 3.3V or onboard 5V pin. Servos create high inductive spikes and stall current draws (up to 2A) that will brown out the ESP32-S3 MCU and trigger immediate reboots. Use the external 5V 3A BEC for servos and camera.

---

## 2. Complete ESP32-S3 Pinout & Wiring Table

Master configuration defined in [`boat-controller/src/config.h`](file:///c:/Users/anamika%20sakal/Downloads/shashi/boat-esp32-clusteer/boat-controller/src/config.h).

| GPIO | Pin Name / Function | Target Module | Module Pin | Direction / Mode | Electrical Spec / Notes |
|:----:|:-------------------|:--------------|:----------:|:----------------:|:------------------------|
| **1** | `PIN_BATT_MOTOR` | Motor LiPo Voltage Divider | Divider Midpoint | Input (ADC1_CH0) | 11.0:1 Divider (10kΩ / 1kΩ) |
| **2** | `PIN_BATT_LOGIC` | Logic LiPo Voltage Divider | Divider Midpoint | Input (ADC1_CH1) | 6.0:1 Divider (10kΩ / 2kΩ) |
| **4** | `PIN_ESC_LEFT` | Left Motor ESC | PWM White/Orange | Output (LEDC Ch 0) | 50 Hz PWM, 1000–2000 µs |
| **5** | `PIN_ESC_RIGHT` | Right Motor ESC | PWM White/Orange | Output (LEDC Ch 1) | 50 Hz PWM, 1000–2000 µs |
| **6** | `PIN_SERVO_PAN` | Camera Gimbal Pan Servo | Signal (Yellow/White) | Output (LEDC Ch 2) | 50 Hz PWM, 500–2500 µs |
| **7** | `PIN_SERVO_TILT` | Camera Gimbal Tilt Servo| Signal (Yellow/White) | Output (LEDC Ch 3) | 50 Hz PWM, 500–2500 µs |
| **8** | `PIN_I2C_SDA` | INA219 + BNO055 Bus | SDA | Bidirectional | Shared I²C Data, 3.3V logic |
| **9** | `PIN_I2C_SCL` | INA219 + BNO055 Bus | SCL | Output (Clock) | Shared I²C Clock, 3.3V logic |
| **15**| `PIN_SERVO_AUX1` | Expansion Servo 1 | Signal | Output (LEDC Ch 4) | Reserved (Release mechanism / gripper) |
| **16**| `PIN_SERVO_AUX2` | Expansion Servo 2 | Signal | Output (LEDC Ch 5) | Reserved (Water sampler / searchlight) |
| **17**| `PIN_SERVO_AUX3` | Expansion Servo 3 | Signal | Output (LEDC Ch 6) | Reserved |
| **18**| `PIN_GPS_RX` | NEO-M8N GPS Module | TXD | Input (HW UART1) | 9600 Baud, 8N1 |
| **19**| `PIN_GPS_TX` | NEO-M8N GPS Module | RXD | Output (HW UART1) | 9600 Baud, 8N1 |
| **20**| `PIN_RADAR_RX` | HLK-LD2450 mmWave Radar | TX | Input (HW UART2) | 256,000 Baud, 8N1 |
| **21**| `PIN_RADAR_TX` | HLK-LD2450 mmWave Radar | RX | Output (HW UART2) | 256,000 Baud, 8N1 |
| **3V3**| 3.3V Out | BNO055, INA219 Logic | VCC / VIN | Power Output | 3.3V from S3 onboard regulator |
| **5V** | 5V In / VBUS | 5V BEC (Logic rail) | 5V Out | Power Input | Clean 5.0V supply for S3 board |
| **GND**| Ground | System Common Bus | GND | Power / Reference | Central Ground Bus |

> [!IMPORTANT]
> **Blocked Pins on ESP32-S3 N8R8:**
> GPIO **33, 34, 35, 36, and 37** are hardwired to the high-speed Octal SPI PSRAM inside the ESP32-S3 module. **Never connect any external wires or components to these pins**, or the firmware will crash on boot with cache errors.

---

## 3. Sensor & Peripheral Wiring Diagrams

### 3.1 I²C Bus Wiring (INA219 & BNO055)

The I²C bus operates at 3.3V logic on GPIO 8 (SDA) and GPIO 9 (SCL). Both sensors share these two lines in parallel.

```
       ESP32-S3                       INA219 Sensor                  BNO055 9-DOF IMU
    ┌─────────────┐                  ┌─────────────┐                 ┌─────────────┐
    │     3V3 ────┼──────────────────┼─ VCC        │   ┌─────────────┼─ VIN        │
    │     GND ────┼──────────────────┼─ GND        │   │ ┌───────────┼─ GND        │
    │  GPIO 8 ────┼──[ I2C SDA ]─────┼─ SDA        └───┼─┼───────────┼─ SDA        │
    │  GPIO 9 ────┼──[ I2C SCL ]─────┼─ SCL            │ │           │  SCL        │
    └─────────────┘                  └─────────────┘   │ │           └─────────────┘
                                                       │ │
                                          3V3 ─────────┘ │
                                          GND ───────────┘
```

* **INA219 Settings:** Address `0x40`. Measures main battery pack voltage and current consumption.
* **BNO055 Settings:** Address `0x28` (ADR pin tied to GND). Provides 9-DOF fused magnetic compass heading.

---

### 3.2 Battery Monitoring & INA219 Shunt Connection

The INA219 measures high-side current by placing its 0.1Ω onboard shunt resistor directly between the Motor LiPo (+) lead and the ESC (+) supply rail:

```
  Motor LiPo (+) ───────► [ INA219 VIN+ ]
                             │
                        [0.1Ω Shunt]
                             │
  ESCs & BEC (+) ◄─────── [ INA219 VIN- ]

  Motor LiPo (-) ──────────────────────────────────────► Common Ground Bus
```

#### Dual Battery ADC Resistor Dividers (Fallback & Logic)
Implemented in [`boat-controller/src/battery.h`](file:///c:/Users/anamika%20sakal/Downloads/shashi/boat-esp32-clusteer/boat-controller/src/battery.h):

```
1. Motor Battery Fallback Divider (11:1 Ratio):
   Motor LiPo (+) ───[ 10 kΩ Resistor ]───┬───[ 1 kΩ Resistor ]───► GND
                                          │
                                          └───► ESP32-S3 GPIO 1 (ADC1_CH0)
   * 3S LiPo Max (12.6V) ──► 12.6V / 11 = 1.145V into ADC
   * 4S LiPo Max (16.8V) ──► 16.8V / 11 = 1.527V into ADC

2. Logic Battery Divider (6:1 Ratio):
   Logic LiPo (+) ───[ 10 kΩ Resistor ]───┬───[ 2 kΩ Resistor ]───► GND
                                          │
                                          └───► ESP32-S3 GPIO 2 (ADC1_CH1)
   * 2S LiPo Max (8.4V)  ──►  8.4V / 6  = 1.400V into ADC
```

---

### 3.3 GPS Module Wiring (NEO-M8N)

Connected via Hardware Serial UART1 as defined in [`boat-controller/src/gps.h`](file:///c:/Users/anamika%20sakal/Downloads/shashi/boat-esp32-clusteer/boat-controller/src/gps.h):

```
       ESP32-S3                         NEO-M8N GPS
    ┌──────────────┐                 ┌──────────────┐
    │      5V  ────┼─────────────────┼─ VCC         │ (from clean 5V BEC)
    │     GND  ────┼─────────────────┼─ GND         │
    │ GPIO 18  ────┼──[ RX1 ◄── TX ]─┼─ TXD         │
    │ GPIO 19  ────┼──[ TX1 ──► RX ]─┼─ RXD         │
    └──────────────┘                 └──────────────┘
```

* **Baud Rate:** 9600 Baud
* **Notes:** Antenna must have a clear sky view upwards away from high-current motor wires.

---

### 3.4 24GHz mmWave Radar Wiring (HLK-LD2450)

Connected via Hardware Serial UART2 as defined in [`boat-controller/src/radar.h`](file:///c:/Users/anamika%20sakal/Downloads/shashi/boat-esp32-clusteer/boat-controller/src/radar.h):

```
       ESP32-S3                        HLK-LD2450 Radar
    ┌──────────────┐                 ┌──────────────────┐
    │      5V  ────┼─────────────────┼─ VCC (5V Required│
    │     GND  ────┼─────────────────┼─ GND             │
    │ GPIO 20  ────┼──[ RX2 ◄── TX ]─┼─ TX              │
    │ GPIO 21  ────┼──[ TX2 ──► RX ]─┼─ RX              │
    └──────────────┘                 └──────────────────┘
```

* **Baud Rate:** 256,000 Baud
* **Placement:** Mount facing forward at the bow, above the water line, away from metal obstructions.

---

### 3.5 ESC & Motor Wiring (Differential Thrust)

Bidirectional brushless ESCs (1500µs neutral) controlled via [`boat-controller/src/motors.h`](file:///c:/Users/anamika%20sakal/Downloads/shashi/boat-esp32-clusteer/boat-controller/src/motors.h):

```
                        ┌──────────────────────────────────────────────┐
                        │              ESP32-S3 Master                 │
                        │                                              │
                        │   GPIO 4 (LEDC 0)          GPIO 5 (LEDC 1)   │
                        └──────┬────────────────────────────┬──────────┘
                               │ White (Signal)             │ White (Signal)
                               ▼                            ▼
                    ┌─────────────────────┐      ┌─────────────────────┐
                    │      LEFT ESC       │      │      RIGHT ESC      │
                    │   (Bidirectional)   │      │   (Bidirectional)   │
                    └───┬─────────────┬───┘      └───┬─────────────┬───┘
     LiPo (-) / GND ────┼─────────────│──────────────┼─────────────│──── Common GND
     LiPo (+) (via INA)─┴─────────────│──────────────┴─────────────│
                                      ▼                            ▼
                                3-Phase A/B/C                3-Phase A/B/C
                                      ▼                            ▼
                              ┌───────────────┐            ┌───────────────┐
                              │  Left Motor   │            │  Right Motor  │
                              └───────────────┘            └───────────────┘
```

* **Signal Range:** 1000 µs (Full Reverse) — 1500 µs (Neutral Stop) — 2000 µs (Full Forward).
* **Direction Swap:** Swap any two of the three motor bullet connectors if a propeller spins in reverse.

---

### 3.6 Pan / Tilt Gimbal Servos Wiring

Operated via [`boat-controller/src/servos.h`](file:///c:/Users/anamika%20sakal/Downloads/shashi/boat-esp32-clusteer/boat-controller/src/servos.h):

```
       ESP32-S3                        Pan Servo                 Tilt Servo
    ┌──────────────┐                 ┌───────────┐             ┌───────────┐
    │  GPIO 6 ─────┼───[ Signal ]────┼─ Orange   │             │           │
    │  GPIO 7 ─────┼───[ Signal ]────┼───────────┼─────────────┼─ Orange   │
    │     GND ─────┼───[ Ref GND ]───┼─ Brown    │     ┌───────┼─ Brown    │
    └──────────────┘                 └─────┬─────┘     │       └─────┬─────┘
                                           │           │             │
        5V BEC (+) ────────────────────────┼───────────┼─────────────┘ (Red / +5V)
        5V BEC GND ────────────────────────┴───────────┘               (Brown / GND)
```

---

## 4. Camera Streamer Node (ESP32-CAM) Wiring

The ESP32-CAM operates as an independent Wi-Fi client running [`cam-streamer/src/main.cpp`](file:///c:/Users/anamika%20sakal/Downloads/shashi/boat-esp32-clusteer/cam-streamer/src/main.cpp).

### 4.1 Operational Wiring (Normal Run Mode)

```
       5V 3A BEC                         ESP32-CAM (AI-Thinker)
    ┌──────────────┐                 ┌─────────────────────────────┐
    │      5V  ────┼─────────────────┼─ 5V (Requires 2A Peak)      │
    │     GND  ────┼─────────────────┼─ GND                        │
    └──────────────┘                 │                             │
                                     │ GPIO 0: UNCONNECTED (FLOAT) │
                                     │ U0TXD:  UNCONNECTED         │
                                     │ U0RXD:  UNCONNECTED         │
                                     │ OV2640 Ribbon: Firmly Seated│
                                     └─────────────────────────────┘
```

### 4.2 Flashing / Programming Wiring (FTDI / USB-UART)

To flash firmware using [`deploy/flash_cam.ps1`](file:///c:/Users/anamika%20sakal/Downloads/shashi/boat-esp32-clusteer/deploy/flash_cam.ps1):

```
       USB-to-UART Adapter (FTDI)               ESP32-CAM
    ┌──────────────────────────────┐         ┌─────────────┐
    │ 5V Out (or External 5V BEC) ─┼─────────┼─ 5V         │
    │ GND                          ┼────┬────┼─ GND        │
    │ TXD                          ┼──┐ │ ┌──┼─ U0RXD      │
    │ RXD                          ┼──│─┼─┘  │ U0TXD      │
    └──────────────────────────────┘  │ │    │             │
                                      │ └───►│ GPIO 0      │
                                      │      └─────────────┘
                                      └──────── (Bridge GPIO 0 to GND during boot)
```

> [!NOTE]
> * **Boot into Flash Mode:** Connect `GPIO 0` to `GND`, then press the `RST` button.
> * **Run Mode:** Disconnect `GPIO 0` from `GND`, then press `RST` to start streaming video.

---

## 5. Wire Gauge & Fuse Ratings

| Wire Run | Minimum Gauge | Typical Current | Peak Current | Recommended Protection |
|:---|:---:|:---:|:---:|:---|
| **Motor LiPo Main Leads** | 12 AWG – 14 AWG | 20A – 30A | 60A | 40A–50A Inline Fuse / XT60 |
| **ESC Motor Power Wires** | 14 AWG – 16 AWG | 10A – 15A | 30A | Connected downstream of INA219 |
| **5V Step-Down BEC Input** | 20 AWG – 22 AWG | 1A – 2A | 3A | 3A Fast Fuse / JST-RCY |
| **Servos 5V Power Rail** | 22 AWG | 1.5A | 3A | Directly on 5V BEC output |
| **ESP32-CAM 5V Power** | 22 AWG | 350mA | 1.8A (Wi-Fi Tx) | Decoupling 470µF Capacitor at pin |
| **Sensor & Signal Wires** | 26 AWG – 28 AWG | < 50mA | < 100mA | DuPont / JST-SH / Silicone wire |

---

## 6. Noise Suppression & Hardware Best Practices

1. **Large Low-ESR Capacitor on 5V BEC Output:**
   Place a **470µF to 1000µF 10V electrolytic capacitor** across the 5V and GND rails feeding the servos and ESP32-CAM to absorb servo inductive kickback and prevent camera brownouts.
2. **Motor EMI Suppression:**
   Keep the high-frequency ESC and motor wires physically separated (at least 5cm) from the I²C bus wires (`GPIO 8`, `GPIO 9`) and GPS antenna cable.
3. **Twisted Pairs:**
   Twist the `SDA` + `GND` and `SCL` + `GND` lines, or run them with ground shielding, to prevent motor EMI from locking up the BNO055 I²C bus.
4. **GPS Antenna Orientation:**
   Mount the GPS ceramic patch antenna horizontally facing straight up with an unobstructed view of the sky, elevated above battery and ESC wiring.
5. **Radar Grounding:**
   Ensure the HLK-LD2450 radar has a direct low-impedance connection to the central ground bus to avoid false ghost blips caused by ground bounce.
