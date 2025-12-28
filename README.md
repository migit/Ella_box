# ELLA-Box — The Box You Can Hack!


[![Languages: C++ | Arduino](https://img.shields.io/badge/languages-C%2B%2B%20%7C%20Arduino-blue.svg)](#tech-stack)
[![GitHub repo size](https://img.shields.io/github/repo-size/migit/Ella_box)](#)


![ELLA-Box Demo](https://github.com/user-attachments/assets/c1ef5312-3129-4777-a409-329eb6b0a6af)


---

**ELLA-Box** is a pocket-sized, hackable IoT gadget in a 3D-printed case — designed to control and monitor almost anything. Use it for smart home control, sensors, notifications, remote control, wearables, or experimental gadgets and even retro game console.

It features a futuristic, cyberpunk-style interface (glowing OLED, tactile feedback, pulsing menus) driven by an ESP32-C3 microcontroller — low-cost, compact, and extensible.

---

## Features

* **Display**: 1.3″ SH1106 OLED (128×64)
* **Input**: Rotary encoder (scroll) + push-button (select)
* **Feedback**: Mini vibration motor + buzzer, adjustable intensity/volume
* **UI**: Cyberpunk-themed, large fonts, glowing borders, scanlines, pulsing animations
* **Settings**: Persistent user preferences stored in EEPROM
* **Power**: Deep-sleep mode (~5 µA), wakes on encoder/button press
* **Menus**: Modular system (Main, Settings, Tools, System, Scan, About)
* **Binary size**: 310–510 KB, fits ESP32-C3 flash (1.2 MB)

Potential uses: smart-home remote, environmental sensor hub, wearable notifier, debugging tool, interactive art, or custom IoT gadget.

---

## Hardware Components

* ESP32-C3 Dev Board × 1
* 1.3″ SH1106 OLED Display (128×64) with Rotary Encoder + Push-button × 1
* Mini coin-type Vibration Motor × 1
* Passive Buzzer × 1
* TP4056 LiPo charger × 1
* 500 mAh LiPo battery × 1
* 3D-printed enclosure ![here]](https://www.thingiverse.com/thing:7124618)


---

## Build Instructions

### Hardware Wiring

* OLED: SDA → GPIO20, SCL → GPIO21
* Rotary encoder: A → GPIO8, B → GPIO9, SW → GPIO10
* Vibration motor → GPIO7
* Buzzer (PWM) → GPIO6
* Optional: LiPo + TP4056 charger for portable use

### Software Setup

* Install Arduino IDE + ESP32 core (select “ESP32-C3 Dev Module”)
* Add libraries: `U8g2` (OLED), `EEPROM` (comes with core)
* Clone this repository (`ella-box_OS` firmware)
* Optimize compiler settings: size optimization, enable LTO

### Compile & Flash

* Compile firmware (310–510 KB)
* Flash to ESP32-C3 (ensure total < 1.2 MB)

### Test & Customize

* Navigate menus with encoder + button
* Adjust settings: sound/vibration intensity, sleep timer, UI
* Extend: add sensors, BLE/WiFi modules, custom Tools, notifications

---

## Possible Extensions & Ideas

* Custom PCB + 3D-printed enclosure
* BLE / WiFi smart-home integration
* Attach sensors (BME280, etc.) for handheld sensor hub
* Wearable notification badge
* Games/utilities (“Snake”, menu apps)
* OTA firmware updates via WiFi
* Tiny AI/ML models interface/controller



