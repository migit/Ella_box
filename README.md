# ELLA-Box — The box you can hack!
![Screencastfrom2025-08-2110-57-51-ezgif com-added-text](https://github.com/user-attachments/assets/c1ef5312-3129-4777-a409-329eb6b0a6af)



ELLA-Box is a small, pocket-sized, hackable IoT gadget in a 3D-printed case — designed to control and monitor almost anything. Use it for smart home control, sensors, notifications, remote control, wearables, or experimental gadgets.  

It features a futuristic, cyberpunk-style interface (glowing OLED, tactile feedback, pulsing menus) driven by an ESP32-C3 microcontroller — yet remains low-cost, compact, and extensible.

---

## ⚙️ Features

- **Display**: 1.3″ SH1106 OLED (128×64)  
- **Input**: Rotary encoder (for scrolling) + push-button (for selection)  
- **Feedback**: Vibration motor (mini coin type) + buzzer — both with adjustable intensity/volume for haptic/audio cues  
- **UI**: Cyberpunk-themed interface — large fonts, glowing borders, scanline effects, pulsing animations  
- **Settings**: Persistent user settings (sound, vibration, sleep-timer, intensity, volume) stored in EEPROM  
- **Power**: Low-power deep-sleep mode (≈ 5 µA), device wakes on encoder or button press — perfect for battery use  
- **Extensible menus**: Modular menu system (Main, Settings, Tools, System, Scan, About) — you can customize or add new actions  
- **Compact binary**: 310–510 KB — comfortably fits the 1.2 MB flash memory of the ESP32-C3  

Potential uses: smart-home remote, environmental sensor hub, wearable notifier, debugging tool, interactive art, or custom IoT gadget.

---

## 📦 Hardware Components

- ESP32-C3 Dev Board × 1  
- 1.3″ SH1106 OLED Display (128×64) with Rotary Encoder + Push-button × 1  
- Mini coin-type Vibration Motor × 1  
- Passive Buzzer × 1  
- TP4056 LiPo charger × 1  
- 500 mAh LiPo battery × 1  

Optional: 3D-printed enclosure / case.

---

## 🛠️ Build Instructions

1. **Hardware wiring**  
   - Connect OLED to ESP32-C3 SDA → GPIO20, SCL → GPIO21  
   - Rotary encoder → GPIO8 (A), GPIO9 (B), GPIO10 (SW)  
   - Vibration motor → GPIO7  
   - Buzzer (via PWM) → GPIO6  
   - (Optional) Add LiPo battery + TP4056 charger for portable use  

2. **Software setup**  
   - Install Arduino IDE + ESP32 core (select “ESP32-C3 Dev Module”)  
   - Add required libraries: `U8g2` (for OLED), `EEPROM` (comes with ESP32 core)  
   - Download/clone this repository (firmware: `ella-box_OS`)  
   - Adjust compiler settings: optimize for size, enable LTO (Link Time Optimization)  

3. **Compile & flash**  
   - Compile firmware (should result in 310–510 KB binary)  
   - Flash to ESP32-C3 — ensure total size remains under 1.2 MB  

4. **Test & customize**  
   - Use encoder + button to navigate menus  
   - Adjust settings (sound/vibration intensity, sleep timer, UI)  
   - Extend functionality: add sensors, BLE/WiFi modules, custom “Tools”, sensor reading, notifications, etc.  

---

## 🧩 Possible Extensions & Ideas

- Design a proper PCB + revised 3D-printed enclosure  
- Add BLE / WiFi for smart-home integration or remote control  
- Attach environmental sensors (e.g. temperature/humidity — BME280) and use as handheld sensor hub  
- Turn it into a wearable notification badge (e.g. phone notifications, alerts)  
- Create fun utilities or games (e.g. “Snake”, basic menu-based apps)  
- Implement OTA (Over-The-Air) firmware updates via WiFi  
- Experiment with tiny AI/ML models (on external modules) and use ELLA-Box as an interface/controller  

---



