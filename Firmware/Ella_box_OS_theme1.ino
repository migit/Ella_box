#include <dummy.h>

// ELLA-Box V1.0 by Mikrollere@Hackster.io
//August 20, 2025
#include <Wire.h>
#include <U8g2lib.h>
#include <esp_sleep.h>
#include <EEPROM.h>

// Hardware config
#define ENCODER_A 8     // GPIO8
#define ENCODER_B 9     // GPIO9
#define ENCODER_SW 10   // GPIO10
#define BUZZER_PIN 6    // GPIO6
#define VIBRATION_PIN 7 // GPIO7
#define BACKLIGHT_PIN 5 // GPIO5 for red LED backlight
#define BATTERY_ADC_PIN 2 // GPIO2 (ADC1_1) for battery voltage
#define VBUS_SENSE_PIN 3  // GPIO3 (ADC1_2) for USB VBUS detection
// I2C: SDA=GPIO20, SCL=GPIO21

// Define ACTIVE_BUZZER for active buzzer (DC on/off)
// #define ACTIVE_BUZZER

// OLED setup
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// PWM config
const int buzzerPwmChannel = 0;
const int vibPwmChannel = 1;
const int backlightPwmChannel = 2;
const int pwmResolution = 8;
const int buzzerFreqLow = 800;
const int buzzerFreqMid = 1200;
const int buzzerFreqHigh = 1600;
const int buzzerFreqExtra = 2000;
const int vibFreq = 500;
const int backlightFreq = 5000;
const unsigned long backlightTimeout = 5000;
const int backlightStep = 5;

// Battery config
const float MAX_BATTERY_VOLTAGE = 4.2; // LiPo max
const float MIN_BATTERY_VOLTAGE = 3.0; // LiPo min
const unsigned long batteryUpdateInterval = 5000; // Update every 5s
const int batteryAdcSamples = 10; // Average 10 samples

// EEPROM config
#define EEPROM_SIZE 6 // No need to store charging status
#define EEPROM_SOUND_ADDR 0
#define EEPROM_VIBRATION_ADDR 1
#define EEPROM_SLEEPTIME_ADDR 2
#define EEPROM_VIBINTENSITY_ADDR 3
#define EEPROM_SOUND_VOLUME_ADDR 4
#define EEPROM_BACKLIGHT_ADDR 5

// Settings
const uint32_t sleepTimes[] = {15000, 30000, 60000, 0};
const uint8_t vibIntensities[] = {96, 160, 255};
#ifdef ACTIVE_BUZZER
const uint8_t soundVolumes[] = {6, 6, 6};
#else
const uint8_t soundVolumes[] = {128, 192, 255};
#endif

// Animation config
const int32_t animFrameInterval = 16;
const float scrollSpeed = 0.3f;
const int32_t fadeInterval = 25; // Faster blinking (was 50)
const int32_t fadeSteps = 5;

// Forward declarations
class Menu;
class MenuManager;
void enterSettings();
void enterTools();
void enterSystem();
void startScan();
void showAbout();
void enterBacklightAdjustment();
void showBatteryStatus();

// BatteryManager class
class BatteryManager {
public:
  float batteryVoltage = 0;
  uint8_t batteryPercentage = 0;
  enum ChargingStatus { NOT_CHARGING, CHARGING, FULL };
  ChargingStatus chargingStatus = NOT_CHARGING;
  unsigned long lastBatteryUpdate = 0;

  void setup() {
    pinMode(BATTERY_ADC_PIN, INPUT);
    pinMode(VBUS_SENSE_PIN, INPUT);
    update();
    Serial.println("BatteryManager initialized");
  }

  void update() {
    unsigned long now = millis();
    if (now - lastBatteryUpdate < batteryUpdateInterval) return;
    lastBatteryUpdate = now;

    // Read battery voltage
    uint32_t adcSum = 0;
    for (int i = 0; i < batteryAdcSamples; i++) {
      adcSum += analogReadMilliVolts(BATTERY_ADC_PIN);
      delay(1);
    }
    float adcVoltage = (adcSum / batteryAdcSamples) / 1000.0; // mV to V
    batteryVoltage = adcVoltage * 2; // 1:1 voltage divider
    
    // Estimate SoC
    batteryPercentage = constrain(
      (int)(100.0 * (batteryVoltage - MIN_BATTERY_VOLTAGE) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE)),
      0, 100
    );

    // Detect charging status
    bool vbusPresent = digitalRead(VBUS_SENSE_PIN);
    if (!vbusPresent) {
      chargingStatus = NOT_CHARGING;
    } else if (batteryVoltage >= 4.15) {
      chargingStatus = FULL;
    } else {
      chargingStatus = CHARGING;
    }

    Serial.printf("Battery: %.2fV, %d%%, Charging: %s\n",
                  batteryVoltage, batteryPercentage,
                  chargingStatus == NOT_CHARGING ? "OFF" :
                  chargingStatus == CHARGING ? "ON" : "FULL");
  }
};
BatteryManager battery;

// SettingsManager class
class SettingsManager {
public:
  bool soundEnabled = true;
  bool vibrationEnabled = true;
  uint8_t backlightIntensity = 255;
  uint8_t sleepTimeIndex = 1;
  uint8_t vibIntensityIndex = 1;
  uint8_t soundVolumeIndex = 1;
  uint32_t inactivityTimeout = sleepTimes[1];
  uint8_t batteryPercentage = 0;
  BatteryManager::ChargingStatus chargingStatus = BatteryManager::NOT_CHARGING;

  void setup() {
    EEPROM.begin(EEPROM_SIZE);
    load();
    Serial.println("SettingsManager initialized");
  }

  void load() {
    uint8_t val;
    val = EEPROM.read(EEPROM_SOUND_ADDR);
    soundEnabled = (val > 1) ? true : val;
    if (val > 1) saveSound();
    val = EEPROM.read(EEPROM_VIBRATION_ADDR);
    vibrationEnabled = (val > 1) ? true : val;
    if (val > 1) saveVibration();
    val = EEPROM.read(EEPROM_SLEEPTIME_ADDR);
    sleepTimeIndex = (val > 3) ? 1 : val;
    if (val > 3) saveSleepTime();
    val = EEPROM.read(EEPROM_VIBINTENSITY_ADDR);
    vibIntensityIndex = (val > 2) ? 1 : val;
    if (val > 2) saveVibIntensity();
    val = EEPROM.read(EEPROM_SOUND_VOLUME_ADDR);
    soundVolumeIndex = (val > 2) ? 1 : val;
    if (val > 2) saveSoundVolume();
    val = EEPROM.read(EEPROM_BACKLIGHT_ADDR);
    backlightIntensity = val;
    if (val > 255) saveBacklight();
    inactivityTimeout = sleepTimes[sleepTimeIndex];
    Serial.printf("Settings loaded: Sound=%d, Vib=%d, Backlight=%d, Sleep=%s, VibInt=%d, SndVol=%d\n",
                  soundEnabled, vibrationEnabled, backlightIntensity,
                  sleepTimeIndex == 3 ? "Off" : String(sleepTimes[sleepTimeIndex]/1000).c_str(),
                  vibIntensityIndex, soundVolumeIndex);
  }

  void saveSound() { EEPROM.write(EEPROM_SOUND_ADDR, soundEnabled); EEPROM.commit(); Serial.println("Sound setting saved"); }
  void saveVibration() { EEPROM.write(EEPROM_VIBRATION_ADDR, vibrationEnabled); EEPROM.commit(); Serial.println("Vibration setting saved"); }
  void saveSleepTime() { EEPROM.write(EEPROM_SLEEPTIME_ADDR, sleepTimeIndex); EEPROM.commit(); inactivityTimeout = sleepTimes[sleepTimeIndex]; Serial.println("Sleep time saved"); }
  void saveVibIntensity() { EEPROM.write(EEPROM_VIBINTENSITY_ADDR, vibIntensityIndex); EEPROM.commit(); Serial.println("Vib intensity saved"); }
  void saveSoundVolume() { EEPROM.write(EEPROM_SOUND_VOLUME_ADDR, soundVolumeIndex); EEPROM.commit(); Serial.println("Sound volume saved"); }
  void saveBacklight() { EEPROM.write(EEPROM_BACKLIGHT_ADDR, backlightIntensity); EEPROM.commit(); Serial.println("Backlight intensity saved"); }
};
SettingsManager settings;

// BacklightManager class
class BacklightManager {
public:
  unsigned long lastBacklightTime = 0;

  void setup() {
    ledcSetup(backlightPwmChannel, backlightFreq, pwmResolution);
    ledcAttachPin(BACKLIGHT_PIN, backlightPwmChannel);
    ledcWrite(backlightPwmChannel, settings.backlightIntensity);
    lastBacklightTime = millis();
    Serial.println("BacklightManager initialized");
  }

  void turnOn() {
    ledcWrite(backlightPwmChannel, settings.backlightIntensity);
    lastBacklightTime = millis();
    Serial.println("Backlight ON");
  }

  void turnOff() {
    ledcWrite(backlightPwmChannel, 0);
    Serial.println("Backlight OFF");
  }

  void update() {
    unsigned long now = millis();
    if (settings.backlightIntensity > 0 && now - lastBacklightTime >= backlightTimeout) {
      turnOff();
    }
  }
};
BacklightManager backlight;

// FeedbackManager class
class FeedbackManager {
public:
  bool active = false;
  bool isButton = false;
  unsigned long startTime = 0;
  int vibeDuration = 0;
  int buzzerTicks = 0;
  int buzzerTickIndex = 0;
  unsigned long buzzerTickTime = 0;
  int toneIndex = 0;

  void setup() {
    #ifdef ACTIVE_BUZZER
      pinMode(BUZZER_PIN, OUTPUT);
      digitalWrite(BUZZER_PIN, LOW);
    #else
      ledcSetup(buzzerPwmChannel, buzzerFreqLow, pwmResolution);
      ledcAttachPin(BUZZER_PIN, buzzerPwmChannel);
      ledcWrite(buzzerPwmChannel, 0);
    #endif
    ledcSetup(vibPwmChannel, vibFreq, pwmResolution);
    ledcAttachPin(VIBRATION_PIN, vibPwmChannel);
    ledcWrite(vibPwmChannel, 0);
    Serial.println("FeedbackManager initialized");
  }

  void trigger(bool button) {
    if (active && !button) {
      Serial.println("Skipping encoder trigger: already active");
      return;
    }
    if (active && button) {
      stop();
      Serial.println("Feedback cleared for button");
    }
    isButton = button;
    startTime = millis();
    vibeDuration = button ? 200 : 40;
    buzzerTicks = 4;
    buzzerTickIndex = 0;
    buzzerTickTime = startTime;
    toneIndex = 0;
    active = true;

    if (settings.vibrationEnabled) {
      ledcWrite(vibPwmChannel, vibIntensities[settings.vibIntensityIndex]);
      Serial.println("Vibration started");
    }
    if (settings.soundEnabled) {
      #ifndef ACTIVE_BUZZER
        ledcChangeFrequency(buzzerPwmChannel, buzzerFreqLow, pwmResolution);
        ledcWrite(buzzerPwmChannel, soundVolumes[settings.soundVolumeIndex]);
      #else
        digitalWrite(BUZZER_PIN, HIGH);
      #endif
      Serial.println("Buzzer started");
    }
  }

  void update() {
    unsigned long now = millis();
    if (!active) return;
    if (now - startTime >= (isButton ? 200 : 28)) {
      stop();
      Serial.println("Feedback timeout");
      return;
    }
    if (isButton) {
      int tickDuration = settings.soundEnabled ? (
        #ifdef ACTIVE_BUZZER
          soundVolumes[settings.soundVolumeIndex]
        #else
          6
        #endif
      ) : 0;
      if (buzzerTicks > 0 && settings.soundEnabled) {
        if (now - buzzerTickTime >= tickDuration) {
          if (buzzerTickIndex % 2 == 0 && buzzerTickIndex < buzzerTicks * 2 - 1) {
            #ifndef ACTIVE_BUZZER
              ledcWrite(buzzerPwmChannel, soundVolumes[settings.soundVolumeIndex]);
              int freq = toneIndex == 0 ? buzzerFreqLow : toneIndex == 1 ? buzzerFreqMid : toneIndex == 2 ? buzzerFreqHigh : buzzerFreqExtra;
              ledcChangeFrequency(buzzerPwmChannel, freq, pwmResolution);
            #else
              digitalWrite(BUZZER_PIN, HIGH);
            #endif
            toneIndex = (toneIndex + 1) % 4;
            Serial.printf("Buzzer tone %d: %d Hz\n", toneIndex, freq);
          } else {
            #ifndef ACTIVE_BUZZER
              ledcWrite(buzzerPwmChannel, 0);
            #else
              digitalWrite(BUZZER_PIN, LOW);
            #endif
            Serial.println("Buzzer off");
          }
          buzzerTickIndex++;
          buzzerTickTime = now;
          if (buzzerTickIndex >= buzzerTicks * 2) {
            buzzerTicks = 0;
            stop();
            Serial.println("Button beep complete");
          }
        }
      } else if (buzzerTicks > 0) {
        buzzerTicks = 0;
        stop();
        Serial.println("Button feedback done (sound off)");
      }
      if (now - startTime >= vibeDuration) {
        stop();
        Serial.println("Button feedback done");
      }
    } else {
      if (now - startTime >= vibeDuration) {
        ledcWrite(vibPwmChannel, 0);
        Serial.println("Vibration stopped");
      }
      if (buzzerTicks > 0 && settings.soundEnabled) {
        int tickDuration = (
          #ifdef ACTIVE_BUZZER
            soundVolumes[settings.soundVolumeIndex]
          #else
            6
          #endif
        );
        if (now - buzzerTickTime >= tickDuration) {
          if (buzzerTickIndex % 2 == 0 && buzzerTickIndex < buzzerTicks * 2 - 1) {
            #ifndef ACTIVE_BUZZER
              ledcWrite(buzzerPwmChannel, soundVolumes[settings.soundVolumeIndex]);
              int freq = toneIndex == 0 ? buzzerFreqLow : toneIndex == 1 ? buzzerFreqMid : toneIndex == 2 ? buzzerFreqHigh : buzzerFreqExtra;
              ledcChangeFrequency(buzzerPwmChannel, freq, pwmResolution);
            #else
              digitalWrite(BUZZER_PIN, HIGH);
            #endif
            toneIndex = (toneIndex + 1) % 4;
            Serial.printf("Buzzer tone %d: %d Hz\n", toneIndex, freq);
          } else {
            #ifndef ACTIVE_BUZZER
              ledcWrite(buzzerPwmChannel, 0);
            #else
              digitalWrite(BUZZER_PIN, LOW);
            #endif
            Serial.println("Buzzer off");
          }
          buzzerTickIndex++;
          buzzerTickTime = now;
          if (buzzerTickIndex >= buzzerTicks * 2) {
            buzzerTicks = 0;
            stop();
            Serial.println("Encoder beep complete");
          }
        }
      } else if (buzzerTicks > 0) {
        buzzerTicks = 0;
        stop();
        Serial.println("Encoder feedback done (sound off)");
      } else if (now - startTime >= vibeDuration) {
        stop();
        Serial.println("Encoder feedback done (no buzzer)");
      }
    }
  }

  void stop() {
    ledcWrite(vibPwmChannel, 0);
    #ifndef ACTIVE_BUZZER
      ledcWrite(buzzerPwmChannel, 0);
    #else
      digitalWrite(BUZZER_PIN, LOW);
    #endif
    active = false;
    buzzerTicks = 0;
    toneIndex = 0;
    Serial.println("Feedback stopped");
  }
};
FeedbackManager feedback;

// InputManager class
class InputManager {
public:
  volatile bool encoderChanged = false;
  volatile int encoderDirection = 0;
  bool buttonPressed = false;
  unsigned long lastButtonTime = 0;
  unsigned long lastEncoderTime = 0;
  int encoderPos = 0;
  int lastEncoded = 0;
  bool lastButtonState = HIGH;
  const int debounceDelay = 30;
  static InputManager* instance;

  void setup() {
    instance = this;
    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_A), handleEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_B), handleEncoderISR, CHANGE);
    Serial.println("InputManager initialized");
  }

  void update() {
    unsigned long now = millis();
    bool currentButtonState = digitalRead(ENCODER_SW);
    if (currentButtonState == LOW && lastButtonState == HIGH && now - lastButtonTime > debounceDelay) {
      buttonPressed = true;
      lastButtonTime = now;
      backlight.turnOn();
      Serial.println("Button pressed");
    }
    lastButtonState = currentButtonState;

    if (encoderChanged && now - lastEncoderTime > debounceDelay) {
      encoderPos += encoderDirection;
      lastEncoderTime = now;
      backlight.turnOn();
      Serial.printf("Encoder: dir=%d, pos=%d\n", encoderDirection, encoderPos);
      encoderChanged = false;
    }
  }

  static void handleEncoderISR() {
    static int lastEncoded = 0;
    int MSB = digitalRead(ENCODER_A);
    int LSB = digitalRead(ENCODER_B);
    int encoded = (MSB << 1) | LSB;
    int sum = (lastEncoded << 2) | encoded;
    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
      instance->encoderDirection = -1;
      instance->encoderChanged = true;
    } else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
      instance->encoderDirection = 1;
      instance->encoderChanged = true;
    }
    lastEncoded = encoded;
  }

  bool getButtonPressed() {
    bool pressed = buttonPressed;
    buttonPressed = false;
    return pressed;
  }

  int getEncoderDelta() {
    int delta = encoderPos;
    encoderPos = 0;
    return delta;
  }
};
InputManager* InputManager::instance = nullptr;
InputManager input;

// DisplayManager class
class DisplayManager {
public:
  float scrollOffset = 0;
  float targetScrollOffset = 0;
  unsigned long lastAnimTime = 0;
  unsigned long lastFadeTime = 0;
  unsigned long lastScanlineTime = 0;
  int fadeLevel = fadeSteps;
  bool fadeDirection = true;
  bool aboutDisplayed = false;

  void setup() {
    Wire.begin(20, 21);
    scanI2C();
    u8g2.begin();
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.clearBuffer();
    u8g2.drawStr(4, 20, "ESP32-C3");
    u8g2.drawStr(4, 32, "Ella-Box-OS v1.0");
    u8g2.sendBuffer();
    delay(1000);
    Serial.println("DisplayManager initialized");
  }

  void scanI2C() {
    Serial.println("Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("I2C device found at address 0x%02X\n", addr);
      }
    }
    Serial.println("I2C scan complete");
  }

  void fadeIn() {
    for (int i = 0; i <= 255; i += 51) {
      u8g2.setContrast(i);
      delay(10);
    }
  }

  void updateAnimation() {
    unsigned long now = millis();
    if (now - lastAnimTime < animFrameInterval) return;
    if (!aboutDisplayed) {
      scrollOffset += (targetScrollOffset - scrollOffset) * scrollSpeed;
    } else {
      scrollOffset = 0;
    }
    if (now - lastFadeTime >= fadeInterval) {
      fadeLevel += fadeDirection ? -1 : 1;
      if (fadeLevel <= 0 || fadeLevel >= fadeSteps) fadeDirection = !fadeDirection;
      lastFadeTime = now;
    }
    lastAnimTime = now;
  }

  void drawBackground() {
    u8g2.drawFrame(0, 0, 128, 64); // Outer border
    u8g2.drawHLine(0, 9, 128); // Top bar separator
    drawBatteryStatus();
  }

  void drawBatteryStatus() {
    if (aboutDisplayed) return; // Skip in About screen
    u8g2.setFont(u8g2_font_profont10_tf); // Small font for compact display
    char buf[8];
    if (settings.chargingStatus == BatteryManager::CHARGING) {
      snprintf(buf, sizeof(buf), "%d%% ⚡", settings.batteryPercentage);
    } else if (settings.chargingStatus == BatteryManager::FULL) {
      snprintf(buf, sizeof(buf), "%d%% ✓", settings.batteryPercentage);
    } else {
      snprintf(buf, sizeof(buf), "%d%%", settings.batteryPercentage);
    }
    int width = u8g2.getStrWidth(buf);
    u8g2.drawStr(128 - width - 2, 8, buf);
    u8g2.setFont(u8g2_font_profont12_tf); // Restore default font
    /* Optional graphical battery icon (uncomment to enable)
    int barWidth = (settings.batteryPercentage * 8) / 100; // 8px max fill
    u8g2.drawFrame(108, 2, 12, 6); // Battery outline (10px wide + 2px terminal)
    u8g2.drawBox(110, 4, barWidth, 2); // Fill bar
    u8g2.drawVLine(120, 3, 4); // Battery terminal
    */
  }

  void drawTitle(const char* title) {
    u8g2.setFont(u8g2_font_profont12_tf);
    char upperTitle[16];
    strncpy(upperTitle, title, 15);
    upperTitle[15] = '\0';
    for (int i = 0; upperTitle[i]; i++) {
      upperTitle[i] = toupper(upperTitle[i]);
    }
    int width = u8g2.getStrWidth(upperTitle);
    u8g2.drawStr((128 - width) / 2, 20, upperTitle); // Centered title
  }

  void drawMenu(class Menu& menu);
  void drawAbout() {
    aboutDisplayed = true;
    u8g2.clearBuffer();
    drawBackground();
    u8g2.setFont(u8g2_font_profont12_tf);
    int x1 = (128 - u8g2.getStrWidth("ELLA Box")) / 2;
    int x2 = (128 - u8g2.getStrWidth("FIRMWARE V1.0")) / 2;
    int x3 = (128 - u8g2.getStrWidth("Mikroller@hackster.io")) / 2;
    u8g2.drawStr(x1, 16, "ELLA Box");
    u8g2.drawStr(x2, 32, "IRMWARE V1.0 V1.0");
    u8g2.drawStr(x3, 48, "Mikroller@hackster.io");
    u8g2.sendBuffer();
    Serial.println("Drawing About screen");
  }

  void drawSelection(const char* text) {
    aboutDisplayed = false;
    u8g2.setFont(u8g2_font_profont12_tf);
    drawBackground();
    int x = (128 - u8g2.getStrWidth(text)) / 2;
    u8g2.drawStr(x, 32, text);
  }
};
DisplayManager display;

// MenuItem struct
struct MenuItem {
  const char* name;
  void (*action)();
  class Menu* submenu;
};

// Menu class
class Menu {
public:
  MenuItem* items;
  int itemCount;
  int selectedIndex = 0;
  Menu* parent = nullptr;
  float animPositions[7] = {0};
  const char* title;

  Menu(MenuItem* _items, int _count, Menu* _parent, const char* _title)
    : items(_items), itemCount(_count), parent(_parent), title(_title) {}

  void selectNext() {
    int newIndex = selectedIndex + 1;
    if (newIndex < itemCount) {
      selectedIndex = newIndex;
      feedback.trigger(false);
      updateScroll();
      Serial.printf("Menu: Selected %s\n", items[selectedIndex].name);
    }
  }

  void selectPrev() {
    int newIndex = selectedIndex - 1;
    if (newIndex >= 0) {
      selectedIndex = newIndex;
      feedback.trigger(false);
      updateScroll();
      Serial.printf("Menu: Selected %s\n", items[selectedIndex].name);
    }
  }

  void activate(MenuManager& menuManager);

  void updateScroll() {
    int offset = max(0, min(selectedIndex - 3, itemCount - 4));
    display.targetScrollOffset = offset * 12;
    for (int i = 0; i < min(itemCount, 7); i++) {
      animPositions[i] = 32 + i * 12 - display.scrollOffset; // Start at y=32
    }
  }
};

// Menu definitions
MenuItem mainItems[] = {
  {"SETTINGS", enterSettings, nullptr},
  {"TOOLS", enterTools, nullptr},
  {"SYSTEM", enterSystem, nullptr},
  {"SCAN", startScan, nullptr},
  {"ABOUT", showAbout, nullptr}
};
Menu mainMenu(mainItems, 5, nullptr, "MENU");

MenuItem settingsItems[] = {
  {"SOUND", nullptr, nullptr},
  {"VIBRATION", nullptr, nullptr},
  {"SLEEP TIME", nullptr, nullptr},
  {"VIB INT", nullptr, nullptr},
  {"SND VOL", nullptr, nullptr},
  {"BACKLIGHT", nullptr, nullptr},
  {"BATTERY", nullptr, nullptr},
  {"BACK", nullptr, nullptr}
};
Menu settingsMenu(settingsItems, 8, &mainMenu, "SETTINGS");

MenuItem toolsItems[] = {
  {"TOOL 1", nullptr, nullptr},
  {"TOOL 2", nullptr, nullptr},
  {"BACK", nullptr, nullptr}
};
Menu toolsMenu(toolsItems, 3, &mainMenu, "TOOLS");

MenuItem systemItems[] = {
  {"SYS INFO", nullptr, nullptr},
  {"RESET", nullptr, nullptr},
  {"BACK", nullptr, nullptr}
};
Menu systemMenu(systemItems, 3, &mainMenu, "SYSTEM");

MenuItem scanItems[] = {
  {"START SCAN", nullptr, nullptr},
  {"SCAN LOG", nullptr, nullptr},
  {"BACK", nullptr, nullptr}
};
Menu scanMenu(scanItems, 3, &mainMenu, "SCAN");

// MenuManager class
class MenuManager {
public:
  Menu* currentMenu = nullptr;
  bool adjustingBacklight = false;

  void setCurrent(Menu* menu) {
    currentMenu = menu;
    adjustingBacklight = false;
    if (menu) {
      currentMenu->updateScroll();
      display.aboutDisplayed = false;
      Serial.printf("Menu set: %s\n", menu->items[menu->selectedIndex].name);
    }
  }

  void handleInput() {
    int delta = input.getEncoderDelta();
    if (adjustingBacklight) {
      if (delta != 0) {
        int newIntensity = settings.backlightIntensity + delta * backlightStep;
        newIntensity = max(0, min(255, newIntensity));
        settings.backlightIntensity = newIntensity;
        backlight.turnOn();
        feedback.trigger(false);
        Serial.printf("Backlight Intensity: %d\n", settings.backlightIntensity);
      }
      if (input.getButtonPressed()) {
        settings.saveBacklight();
        adjustingBacklight = false;
        feedback.trigger(true);
        Serial.println("Backlight adjustment saved and exited");
      }
    } else {
      if (delta > 0) {
        if (currentMenu) currentMenu->selectNext();
        else if (display.aboutDisplayed) setCurrent(&mainMenu);
      } else if (delta < 0) {
        if (currentMenu) currentMenu->selectPrev();
        else if (display.aboutDisplayed) setCurrent(&mainMenu);
      }
      if (input.getButtonPressed()) {
        if (currentMenu) {
          if (currentMenu->parent && strcmp(currentMenu->items[currentMenu->selectedIndex].name, "BACK") == 0) {
            setCurrent(currentMenu->parent);
            display.fadeIn();
            feedback.trigger(true);
            Serial.println("Menu: Back");
          } else {
            currentMenu->activate(*this);
          }
        } else if (display.aboutDisplayed) {
          setCurrent(&mainMenu);
          display.fadeIn();
          feedback.trigger(true);
          Serial.println("Menu: Back to Main from About");
        }
      }
    }
  }
};
MenuManager menuManager;

// Menu implementation
void Menu::activate(MenuManager& menuManager) {
  if (items[selectedIndex].submenu) {
    menuManager.setCurrent(items[selectedIndex].submenu);
    display.fadeIn();
  } else if (items[selectedIndex].action) {
    items[selectedIndex].action();
    display.fadeIn();
  }
  feedback.trigger(true);
}

// DeepSleepManager class
class DeepSleepManager {
public:
  unsigned long lastInteractionTime = millis();

  void setup() {
    gpio_wakeup_enable(GPIO_NUM_8, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(GPIO_NUM_9, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(GPIO_NUM_10, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    Serial.println("DeepSleepManager initialized");
  }

  void update() {
    if (settings.inactivityTimeout == 0) return;
    unsigned long now = millis();
    if (now - lastInteractionTime > settings.inactivityTimeout) {
      Serial.printf("Entering deep sleep, free heap: %u bytes\n", esp_get_free_heap_size());
      backlight.turnOff();
      u8g2.setPowerSave(1);
      feedback.stop();
      Serial.flush();
      esp_deep_sleep_start();
    }
  }

  void resetTimer() {
    lastInteractionTime = millis();
    backlight.turnOn();
  }
};
DeepSleepManager sleepManager;

// Settings menu actions
void toggleSound() {
  settings.soundEnabled = !settings.soundEnabled;
  settings.saveSound();
  feedback.trigger(true);
  Serial.printf("Sound toggled: %d\n", settings.soundEnabled);
}

void toggleVibration() {
  settings.vibrationEnabled = !settings.vibrationEnabled;
  settings.saveVibration();
  feedback.trigger(true);
  Serial.printf("Vibration: %d\n", settings.vibrationEnabled);
}

void cycleSleepTime() {
  settings.sleepTimeIndex = (settings.sleepTimeIndex + 1) % 4;
  settings.saveSleepTime();
  feedback.trigger(true);
  Serial.printf("Sleep Time: %s\n", settings.sleepTimeIndex == 3 ? "Off" : String(sleepTimes[settings.sleepTimeIndex]/1000).c_str());
}

void cycleVibIntensity() {
  settings.vibIntensityIndex = (settings.vibIntensityIndex + 1) % 3;
  settings.saveVibIntensity();
  feedback.trigger(true);
  Serial.printf("Vib Intensity: %d\n", settings.vibIntensityIndex);
}

void cycleSoundVolume() {
  settings.soundVolumeIndex = (settings.soundVolumeIndex + 1) % 3;
  settings.saveSoundVolume();
  feedback.trigger(true);
  Serial.printf("Sound Volume: %d\n", settings.soundVolumeIndex);
}

void enterBacklightAdjustment() {
  menuManager.adjustingBacklight = true;
  feedback.trigger(true);
  Serial.println("Entered backlight adjustment mode");
}

void showBatteryStatus() {
  char buf[32];
  snprintf(buf, sizeof(buf), "BATTERY: %d%%", settings.batteryPercentage);
  display.drawSelection(buf);
  feedback.trigger(true);
  Serial.println("Battery status displayed");
}

// Tools menu actions
void tool1Action() {
  display.drawSelection("TOOL 1");
  feedback.trigger(true);
  Serial.println("Tool 1 activated");
}

void tool2Action() {
  display.drawSelection("TOOL 2");
  feedback.trigger(true);
  Serial.println("Tool 2 activated");
}

void sysInfoAction() {
  display.drawSelection("SYS INFO");
  feedback.trigger(true);
  Serial.println("System Info displayed");
}

void resetAction() {
  display.drawSelection("RESET");
  feedback.trigger(true);
  Serial.println("Reset triggered");
  ESP.restart();
}

// Scan menu actions
void startScanAction() {
  display.drawSelection("SCANNING...");
  feedback.trigger(true);
  Serial.println("Scan started");
}

void scanLogAction() {
  display.drawSelection("SCAN LOG");
  feedback.trigger(true);
  Serial.println("Scan log displayed");
}

// Placeholder actions
void enterSettings() { menuManager.setCurrent(&settingsMenu); }
void enterTools() { menuManager.setCurrent(&toolsMenu); }
void enterSystem() { menuManager.setCurrent(&systemMenu); }
void startScan() { menuManager.setCurrent(&scanMenu); }
void showAbout() {
  menuManager.setCurrent(nullptr);
  display.drawAbout();
  Serial.println("Show About triggered");
}

// Settings menu action bindings
void setupSettingsActions() {
  settingsItems[0].action = toggleSound;
  settingsItems[1].action = toggleVibration;
  settingsItems[2].action = cycleSleepTime;
  settingsItems[3].action = cycleVibIntensity;
  settingsItems[4].action = cycleSoundVolume;
  settingsItems[5].action = enterBacklightAdjustment;
  settingsItems[6].action = showBatteryStatus;
  toolsItems[0].action = tool1Action;
  toolsItems[1].action = tool2Action;
  systemItems[0].action = sysInfoAction;
  systemItems[1].action = resetAction;
  scanItems[0].action = startScanAction;
  scanItems[1].action = scanLogAction;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Booting ESP32-C3 Super Mini...");
  Serial.printf("Reset reason: %d\n", esp_reset_reason());
  Serial.printf("Free heap: %u bytes\n", esp_get_free_heap_size());

  settings.setup();
  setupSettingsActions();
  battery.setup();
  feedback.setup();
  input.setup();
  display.setup();
  backlight.setup();
  sleepManager.setup();

  menuManager.setCurrent(&mainMenu);
  Serial.println("Setup complete");
}

void loop() {
  input.update();
  feedback.update();
  backlight.update();
  battery.update();
  display.updateAnimation();
  sleepManager.update();

  menuManager.handleInput();
  if (input.getEncoderDelta() || input.getButtonPressed()) {
    sleepManager.resetTimer();
  }

  settings.batteryPercentage = battery.batteryPercentage;
  settings.chargingStatus = battery.chargingStatus;

  u8g2.firstPage();
  do {
    if (display.aboutDisplayed) {
      display.drawAbout();
    } else if (menuManager.currentMenu) {
      display.drawMenu(*menuManager.currentMenu);
    }
  } while (u8g2.nextPage());

  static unsigned long lastHeapCheck = 0;
  if (millis() - lastHeapCheck > 5000) {
    Serial.printf("Free heap: %u bytes\n", esp_get_free_heap_size());
    lastHeapCheck = millis();
  }
}

// DisplayManager menu rendering
void DisplayManager::drawMenu(Menu& menu) {
  drawBackground();
  drawTitle(menu.title);

  int offset = max(0, min(menu.selectedIndex - 3, menu.itemCount - 4));
  for (int i = offset; i < min(menu.itemCount, offset + 4); i++) {
    float y = menu.animPositions[i - offset];

    char buf[32];
    if (menu.parent == &mainMenu && menu.items == settingsItems) {
      if (i == 0) snprintf(buf, sizeof(buf), "SOUND: %s", settings.soundEnabled ? "ON" : "OFF");
      else if (i == 1) snprintf(buf, sizeof(buf), "VIBRATION: %s", settings.vibrationEnabled ? "ON" : "OFF");
      else if (i == 2) {
        if (settings.sleepTimeIndex == 3) snprintf(buf, sizeof(buf), "SLEEP TIME: OFF");
        else snprintf(buf, sizeof(buf), "SLEEP TIME: %ds", sleepTimes[settings.sleepTimeIndex]/1000);
      }
      else if (i == 3) snprintf(buf, sizeof(buf), "VIB INT: %s", (const char*[]){"LOW", "MEDIUM", "HIGH"}[settings.vibIntensityIndex]);
      else if (i == 4) snprintf(buf, sizeof(buf), "SND VOL: %s", (const char*[]){"MIN", "MEDIUM", "LOW"}[settings.soundVolumeIndex]);
      else if (i == 5) snprintf(buf, sizeof(buf), "BACKLIGHT: %d%%", (settings.backlightIntensity * 100) / 255);
      else if (i == 6) snprintf(buf, sizeof(buf), "BATTERY: %d%% %s", settings.batteryPercentage,
                               settings.chargingStatus == BatteryManager::NOT_CHARGING ? "OFF" :
                               settings.chargingStatus == BatteryManager::CHARGING ? "ON" : "FULL");
      else snprintf(buf, sizeof(buf), "%s", menu.items[i].name);
    } else {
      snprintf(buf, sizeof(buf), "%s", menu.items[i].name);
    }

    if (i == menu.selectedIndex) {
      u8g2.drawStr(2, y, "[");
      if (fadeLevel > 0) {
        int boxWidth = u8g2.getStrWidth(buf) + 4;
        u8g2.setDrawColor(1);
        u8g2.drawBox(2, y - 10, boxWidth, 12);
        u8g2.drawFrame(1, y - 11, boxWidth + 2, 14);
        if (fadeLevel >= 3) {
          u8g2.drawFrame(0, y - 12, boxWidth + 4, 16);
        }
        u8g2.setDrawColor(0);
        u8g2.drawStr(4, y, buf);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(4, y, buf);
      }
    } else {
      u8g2.drawStr(4, y, buf);
    }
  }
}