# 🌀 TCL Air Conditioner Integration for Home Assistant

> 🇬🇧 English fork of [sorz2122/tclac](https://github.com/sorz2122/tclac) — tested with **TCL TAC-12CHDA**
> Simple DIY project with ESP32 + USB cable. No cloud required.

---

## 🛠️ What you need

- **ESP32** (e.g. ESP32-C3, WROOM32, NodeMCU)
- **USB-A plug or cable**
  👉 I used this one: [AliExpress link](https://www.aliexpress.com/item/1005005776162012.html)
- **Home Assistant with ESPHome (version 2023.3.0 or later)**

---

## 🔌 Wiring

| USB-A Pin | Wire colour | → ESP32 Pin |
|-----------|-------------|-------------|
| GND       | Black       | VIN/VCC     |
| D+        | Green       | GND         |
| D-        | Grey        | RXD         |
| VBUS      | Red         | TXD         |

### 🔍 Example photos
(Note that I didn't pay attention to wire colours here. The colours in the table above correspond to typical USB-A cables that you can simply cut.)

<img src="https://github.com/user-attachments/assets/9b674e06-41ca-4bcf-b09b-691a5fbd8545" width="400"/>
<br/>

![Wiring Example 2](https://github.com/user-attachments/assets/e30fadd9-19cd-47ec-baab-86f8a80410f6)

![7480a856c7839044d7a04292d352b709a2155c07_2_296x500](https://github.com/user-attachments/assets/5b3ccbb8-eb62-4743-8d05-f88a9b986743)

---

## 🧠 Setup in Home Assistant

> This solution is based on **ESPHome** and only works with Home Assistant.

### 1. Install ESPHome

- In Home Assistant go to **Settings → Add-ons → ESPHome** and install

### 2. Create a new device

- In the ESPHome dashboard → "New Device"
- Select your ESP32 type, e.g. `esp32-c3-devkitm-1` or `nodemcu-32s`

### 3. Paste configuration

#### Option A: Simple configuration
[📄 Sample_conf.yaml](https://github.com/jonathanendersby/tclac-en/blob/master/Sample_conf.yaml)

#### Option B: Full configuration
[📄 TCL-Conditioner.yaml](https://github.com/jonathanendersby/tclac-en/blob/master/TCL-Conditioner.yaml)

📝 **Important:**
- Update WiFi credentials, device name, etc.
- Comments in the YAML help with setup

### 4. Flash to ESP32

- Connect via USB cable or use OTA (Over-the-Air)

---

## ✅ Compatible air conditioners

These models have been successfully tested:

- **TCL:** TAC-07CHSA / TAC-09CHSA / TAC-12CHSA / TAC-12CHDA
- **Daichi:** AIR20AVQ1, AIR25AVQS1R-1, DA35EVQ1-1
- **Axioma:** ASX09H1 / ASB09H1
- **Dantex:** RK-12SATI / RK-12SATIE
- ...and similar models

⚠️ **Note:**
Even if the model name matches, there may be differences (no USB port, no UART on the board, etc.).

---

## ☕ Support

https://buymeacoffee.com/sorz2122

<img src="https://github.com/user-attachments/assets/87d5d62f-ba5c-4a7e-a4b8-4cf1fd3018af" width="400"/>
<br/>

---

## 🔧 Advanced configuration via remote package

You can load the configuration modularly:

```yaml
packages:
  remote_package:
    url: https://github.com/jonathanendersby/tclac-en.git
    ref: master
    files:
      - packages/core.yaml   # Main module
      # - packages/leds.yaml # Optional
    refresh: 30s
```
