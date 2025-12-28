/*
 * Project: ELLA-BOX
 * Description:   ELLA-Box is a pocket-sized, hackable IoT gadget in a 3D-printed case — designed to control and monitor almost anything. Use it for smart home control, sensors, notifications, remote control, wearables, or experimental gadgets.
 * Author: Michael Seyoum (https://www.hackster.io/mikroller)
 * Created: August 22, 2025 
 * License: GPL-3.0-only - See LICENSE file for details
 * Repository: https://github.com/migit/Ella_box
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>

// Hardware config
#define ENCODER_A 8     // GPIO8
#define ENCODER_B 9     // GPIO9
#define ENCODER_SW 10   // GPIO10
#define BUZZER_PIN 6    // GPIO6
#define VIBRATION_PIN 7 // GPIO7
#define BACKLIGHT_PIN 5 // GPIO5 (PWM)

// OLED setup
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// UI Constants
const uint8_t TITLE_HEIGHT = 12;
const uint8_t ITEM_HEIGHT = 12;
const uint8_t VISIBLE_ITEMS = 5;
const uint8_t MENU_TOP_OFFSET = 10;

// Icon definitions
#define ICON_NONE 0
#define ICON_ARROW 1
#define ICON_TOGGLE_ON 4
#define ICON_TOGGLE_OFF 5

// PWM config
const int buzzerPwmChannel = 0;
const int vibPwmChannel = 1;
const int backlightPwmChannel = 2;
const int pwmResolution = 8;
const int buzzerFreq = 500;
const int vibFreq = 500;
const int backlightFreq = 5000;

// EEPROM config
#define EEPROM_SIZE 7
#define EEPROM_SOUND_ADDR 0
#define EEPROM_VIBRATION_ADDR 1
#define EEPROM_SLEEPTIME_ADDR 2
#define EEPROM_VIBINTENSITY_ADDR 3
#define EEPROM_SOUND_VOLUME_ADDR 4
#define EEPROM_BACKLIGHT_TIMEOUT_ADDR 5
#define EEPROM_DIM_PERCENTAGE_ADDR 6

// Settings
const uint32_t sleepTimes[] = {15000, 30000, 60000, 0};
const uint32_t backlightTimeouts[] = {3000, 5000, 10000, 0};
const uint8_t dimPercentages[] = {10, 25, 50};
const uint8_t vibIntensities[] = {128, 192, 255};
const uint8_t soundVolumes[] = {64, 128, 192};

// ========== SNAKE GAME CONSTANTS (ENHANCED) ==========
#define GAME_WIDTH 128
#define GAME_HEIGHT 48
#define GRID_SIZE 6           // Increased from 4 for bigger snake
#define GRID_WIDTH (GAME_WIDTH / GRID_SIZE)
#define GRID_HEIGHT (GAME_HEIGHT / GRID_SIZE)
#define INITIAL_SNAKE_LENGTH 3
#define GAME_SPEED_START 120  // Faster start
#define GAME_SPEED_MIN 50     // Minimum speed (fastest)
#define SPEED_INCREASE 2      // Speed increase per food
#define SNAKE_HIGHSCORE_ADDR 100

// Game states
enum SnakeGameState {
  SNAKE_MENU,
  SNAKE_PLAYING,
  SNAKE_GAME_OVER,
  SNAKE_PAUSED
};

// Direction enum
enum Direction {
  UP,
  DOWN,
  LEFT,
  RIGHT
};

// Snake segment struct
struct Point {
  int x;
  int y;
};

// Game variables
SnakeGameState snakeGameState = SNAKE_MENU;
Point snake[GRID_WIDTH * GRID_HEIGHT];
int snakeLength = INITIAL_SNAKE_LENGTH;
Direction snakeDirection = RIGHT;
Direction nextDirection = RIGHT;  // Buffer for responsive controls
Point food;
int snakeScore = 0;
int snakeHighScore = 0;
int gameSpeed = GAME_SPEED_START;
unsigned long lastGameUpdate = 0;
unsigned long lastInputTime = 0;
bool inGameMode = false;
bool growthEffectActive = false;
unsigned long growthEffectStart = 0;
const unsigned long GROWTH_EFFECT_DURATION = 200;

// Forward declarations
class Menu;
class MenuManager;
class DisplayManager;

// Function declarations
void toggleSound();
void toggleVibration();
void cycleSleepTime();
void cycleVibIntensity();
void cycleSoundVolume();
void cycleBacklightTimeout();
void cycleDimPercentage();

// Snake game functions
void startSnakeGame();
void snakeGameAction();
void runSnakeGameLoop();
void initSnakeGame();
void updateSnakeGame();
void drawSnakeGame();
void drawSnakeMenu();
void drawSnakeGameOver();
void drawSnakePaused();
void handleSnakeInput();

// MenuItem struct
struct MenuItem {
  const char* name;
  void (*action)();
  Menu* submenu;
  uint8_t iconType;
  bool showToggle;
  bool* toggleState;
  const char** valueLabels;
  uint8_t* valueIndex;
};

// Menu class
class Menu {
public:
  MenuItem* items;
  uint8_t itemCount;
  uint8_t selectedIndex = 0;
  uint8_t firstVisibleIndex = 0;
  Menu* parent = nullptr;
  const char* title;

  Menu(MenuItem* _items, uint8_t _count, Menu* _parent, const char* _title)
    : items(_items), itemCount(_count), parent(_parent), title(_title) {}

  void selectNext() {
    if (itemCount == 0) return;
    selectedIndex = min(selectedIndex + 1, itemCount - 1);
    if (selectedIndex >= firstVisibleIndex + VISIBLE_ITEMS) {
      firstVisibleIndex = selectedIndex - VISIBLE_ITEMS + 1;
    }
    firstVisibleIndex = min(firstVisibleIndex, (uint8_t)(itemCount > VISIBLE_ITEMS ? itemCount - VISIBLE_ITEMS : 0));
  }

  void selectPrev() {
    if (itemCount == 0) return;
    selectedIndex = max(selectedIndex - 1, 0);
    if (selectedIndex < firstVisibleIndex) {
      firstVisibleIndex = selectedIndex;
    }
    firstVisibleIndex = min(firstVisibleIndex, (uint8_t)(itemCount > VISIBLE_ITEMS ? itemCount - VISIBLE_ITEMS : 0));
  }
};

// SettingsManager class
class SettingsManager {
public:
  bool soundEnabled = true;
  bool vibrationEnabled = true;
  uint8_t sleepTimeIndex = 1;
  uint8_t vibIntensityIndex = 1;
  uint8_t soundVolumeIndex = 0;
  uint8_t backlightTimeoutIndex = 0;
  uint8_t dimPercentageIndex = 1;
  uint32_t inactivityTimeout = sleepTimes[1];
  uint32_t backlightTimeout = backlightTimeouts[0];
  bool settingsChanged = false;

  void setup() {
    EEPROM.begin(EEPROM_SIZE + 10);
    load();
    Serial.println("Settings initialized");
  }

  void load() {
    soundEnabled = EEPROM.read(EEPROM_SOUND_ADDR) <= 1 ? EEPROM.read(EEPROM_SOUND_ADDR) : true;
    vibrationEnabled = EEPROM.read(EEPROM_VIBRATION_ADDR) <= 1 ? EEPROM.read(EEPROM_VIBRATION_ADDR) : true;
    sleepTimeIndex = EEPROM.read(EEPROM_SLEEPTIME_ADDR) <= 3 ? EEPROM.read(EEPROM_SLEEPTIME_ADDR) : 1;
    vibIntensityIndex = EEPROM.read(EEPROM_VIBINTENSITY_ADDR) <= 2 ? EEPROM.read(EEPROM_VIBINTENSITY_ADDR) : 1;
    soundVolumeIndex = EEPROM.read(EEPROM_SOUND_VOLUME_ADDR) <= 2 ? EEPROM.read(EEPROM_SOUND_VOLUME_ADDR) : 0;
    backlightTimeoutIndex = EEPROM.read(EEPROM_BACKLIGHT_TIMEOUT_ADDR) <= 3 ? EEPROM.read(EEPROM_BACKLIGHT_TIMEOUT_ADDR) : 0;
    dimPercentageIndex = EEPROM.read(EEPROM_DIM_PERCENTAGE_ADDR) <= 2 ? EEPROM.read(EEPROM_DIM_PERCENTAGE_ADDR) : 1;
    inactivityTimeout = sleepTimes[sleepTimeIndex];
    backlightTimeout = backlightTimeouts[backlightTimeoutIndex];
    
    // Load snake high score
    snakeHighScore = EEPROM.read(SNAKE_HIGHSCORE_ADDR) | (EEPROM.read(SNAKE_HIGHSCORE_ADDR + 1) << 8);
    if (snakeHighScore > 10000) snakeHighScore = 0;
    
    EEPROM.end();
    Serial.printf("Settings loaded. Snake high score: %d\n", snakeHighScore);
  }

  void save(uint8_t addr, uint8_t value) {
    if (EEPROM.read(addr) != value) {
      EEPROM.write(addr, value);
      settingsChanged = true;
    }
  }

  void commit() {
    if (settingsChanged) {
      EEPROM.begin(EEPROM_SIZE + 10);
      EEPROM.commit();
      settingsChanged = false;
      Serial.println("Settings saved");
      EEPROM.end();
    }
  }
  
  void saveSnakeHighScore() {
    if (snakeScore > snakeHighScore) {
      snakeHighScore = snakeScore;
      EEPROM.begin(EEPROM_SIZE + 10);
      EEPROM.write(SNAKE_HIGHSCORE_ADDR, snakeHighScore & 0xFF);
      EEPROM.write(SNAKE_HIGHSCORE_ADDR + 1, (snakeHighScore >> 8) & 0xFF);
      EEPROM.commit();
      EEPROM.end();
      Serial.printf("New snake high score saved: %d\n", snakeHighScore);
    }
  }
};
SettingsManager settings;

// FeedbackManager class
class FeedbackManager {
public:
  void setup() {
    ledcSetup(buzzerPwmChannel, buzzerFreq, pwmResolution);
    ledcAttachPin(BUZZER_PIN, buzzerPwmChannel);
    ledcWrite(buzzerPwmChannel, 0);
    
    ledcSetup(vibPwmChannel, vibFreq, pwmResolution);
    ledcAttachPin(VIBRATION_PIN, vibPwmChannel);
    ledcWrite(vibPwmChannel, 0);
    
    Serial.println("Feedback initialized");
  }

  void trigger(bool isButton, bool isLowBattery = false, bool isLongPress = false) {
    if (settings.soundEnabled) {
      ledcWrite(buzzerPwmChannel, isLongPress ? soundVolumes[settings.soundVolumeIndex] / 2 : soundVolumes[settings.soundVolumeIndex]);
      delay(isLongPress ? 50 : 20);
      ledcWrite(buzzerPwmChannel, 0);
    }

    if (settings.vibrationEnabled && !isLowBattery) {
      ledcWrite(vibPwmChannel, isLongPress ? vibIntensities[settings.vibIntensityIndex] / 2 : vibIntensities[settings.vibIntensityIndex]);
      delay(isLongPress ? 100 : 50);
      ledcWrite(vibPwmChannel, 0);
    }
  }
  
  void snakeEatSound() {
    if (settings.soundEnabled) {
      // Two-tone eat sound
      ledcWrite(buzzerPwmChannel, soundVolumes[settings.soundVolumeIndex]);
      delay(15);
      ledcWrite(buzzerPwmChannel, soundVolumes[settings.soundVolumeIndex] / 2);
      delay(15);
      ledcWrite(buzzerPwmChannel, 0);
    }
  }
  
  void gameOverSound() {
    if (settings.soundEnabled) {
      // Sad descending tone
      for (int i = 100; i > 0; i -= 10) {
        ledcWrite(buzzerPwmChannel, i);
        delay(5);
      }
      ledcWrite(buzzerPwmChannel, 0);
    }
  }
};
FeedbackManager feedback;

// BacklightManager class
class BacklightManager {
public:
  void setup() {
    ledcSetup(backlightPwmChannel, backlightFreq, pwmResolution);
    ledcAttachPin(BACKLIGHT_PIN, backlightPwmChannel);
    setIntensity(255);
    lastActive = millis();
    Serial.println("Backlight initialized");
  }

  void turnOn() {
    ledcWrite(backlightPwmChannel, 255);
    lastActive = millis();
  }

  void turnOff() {
    ledcWrite(backlightPwmChannel, 0);
  }

  void update() {
    if (settings.backlightTimeout > 0 && millis() - lastActive >= settings.backlightTimeout) {
      turnOff();
    }
  }

  void setIntensity(uint8_t intensity) {
    ledcWrite(backlightPwmChannel, intensity);
  }
  
  void growthFlash() {
    // Flash backlight for growth effect
    for (int i = 0; i < 3; i++) {
      ledcWrite(backlightPwmChannel, 255);
      delay(30);
      ledcWrite(backlightPwmChannel, 128);
      delay(30);
    }
    ledcWrite(backlightPwmChannel, 255);
  }

private:
  unsigned long lastActive = 0;
};
BacklightManager backlight;

// InputManager class
class InputManager {
public:
  void setup() {
    instance = this;
    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_A), handleEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_B), handleEncoderISR, CHANGE);
    Serial.println("Input initialized");
  }

  void update() {
    bool buttonState = digitalRead(ENCODER_SW);
    unsigned long now = millis();

    // Ultra-responsive debouncing
    if (buttonState == LOW && lastButtonState == HIGH && now - lastDebounceTime > 10) {
      buttonPressed = true;
      lastDebounceTime = now;
    }

    // Long press detection
    if (buttonState == LOW && now - lastDebounceTime >= 300) {
      longPress = true;
      buttonPressed = false;
    }

    if (buttonState == HIGH && lastButtonState == LOW) {
      lastDebounceTime = now;
    }

    lastButtonState = buttonState;
  }

  int getEncoderDelta() {
    noInterrupts();
    int delta = encoderDelta;
    encoderDelta = 0;
    interrupts();
    return delta;
  }

  bool getButtonPressed() {
    bool pressed = buttonPressed;
    buttonPressed = false;
    return pressed;
  }

  bool getLongPress() {
    bool press = longPress;
    longPress = false;
    return press;
  }

private:
  static InputManager* instance;
  volatile int encoderDelta = 0;
  bool buttonPressed = false;
  bool longPress = false;
  bool lastButtonState = HIGH;
  unsigned long lastDebounceTime = 0;

  static void handleEncoderISR() {
    static uint8_t lastEncoded = 0;
    uint8_t encoded = (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);
    uint8_t sum = (lastEncoded << 2) | encoded;

    // Accumulate encoder delta for ultra-responsive control
    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
      instance->encoderDelta += -1;
    } 
    else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
      instance->encoderDelta += 1;
    }

    lastEncoded = encoded;
  }
};
InputManager* InputManager::instance = nullptr;
InputManager input;

// Menu definitions
const char* sleepTimeLabels[] = {"15s", "30s", "60s", "Off"};
const char* vibIntensityLabels[] = {"Low", "Med", "High"};
const char* soundVolumeLabels[] = {"Low", "Med", "High"};
const char* backlightTimeoutLabels[] = {"3s", "5s", "10s", "Off"};
const char* dimPercentageLabels[] = {"10%", "25%", "50%"};

// SIMPLIFIED MENUS: Only Game and Settings
MenuItem mainItems[] = {
  {"Snake Game", snakeGameAction, nullptr, ICON_NONE, false, nullptr, nullptr, nullptr},
  {"Settings", nullptr, nullptr, ICON_ARROW, false, nullptr, nullptr, nullptr}
};

MenuItem settingsItems[] = {
  {"Sound", toggleSound, nullptr, ICON_NONE, true, &settings.soundEnabled, nullptr, nullptr},
  {"Vibration", toggleVibration, nullptr, ICON_NONE, true, &settings.vibrationEnabled, nullptr, nullptr},
  {"Sleep Time", cycleSleepTime, nullptr, ICON_NONE, false, nullptr, sleepTimeLabels, &settings.sleepTimeIndex},
  {"Vib. Int.", cycleVibIntensity, nullptr, ICON_NONE, false, nullptr, vibIntensityLabels, &settings.vibIntensityIndex},
  {"Sound Vol.", cycleSoundVolume, nullptr, ICON_NONE, false, nullptr, soundVolumeLabels, &settings.soundVolumeIndex},
  {"BL Timeout", cycleBacklightTimeout, nullptr, ICON_NONE, false, nullptr, backlightTimeoutLabels, &settings.backlightTimeoutIndex},
  {"Dim %", cycleDimPercentage, nullptr, ICON_NONE, false, nullptr, dimPercentageLabels, &settings.dimPercentageIndex},
  {"Back", nullptr, nullptr, ICON_NONE, false, nullptr, nullptr, nullptr}
};

// Create menu objects
Menu mainMenu(mainItems, sizeof(mainItems)/sizeof(MenuItem), nullptr, "Main Menu");
Menu settingsMenu(settingsItems, sizeof(settingsItems)/sizeof(MenuItem), &mainMenu, "Settings");

// Update main menu items with correct submenu pointers
void initMenus() {
  mainMenu.items[1].submenu = &settingsMenu;  // Index 1 is Settings
}

// ========== ENHANCED SNAKE GAME IMPLEMENTATION ==========
void startSnakeGame() {
  inGameMode = true;
  initSnakeGame();
  snakeGameState = SNAKE_MENU;
  Serial.println("Starting Snake game");
}

void snakeGameAction() {
  startSnakeGame();
}

void initSnakeGame() {
  // Initialize snake in the middle
  for (int i = 0; i < snakeLength; i++) {
    snake[i].x = GRID_WIDTH / 2 - i;
    snake[i].y = GRID_HEIGHT / 2;
  }
  
  // Generate first food
  generateFood();
  
  snakeDirection = RIGHT;
  nextDirection = RIGHT;
  snakeScore = 0;
  gameSpeed = GAME_SPEED_START;
  snakeGameState = SNAKE_MENU;
  lastGameUpdate = millis();
  lastInputTime = millis();
  growthEffectActive = false;
  
  Serial.println("Snake game initialized");
}

void generateFood() {
  bool onSnake;
  do {
    onSnake = false;
    food.x = random(0, GRID_WIDTH);
    food.y = random(0, GRID_HEIGHT);
    
    // Check if food overlaps with snake
    for (int i = 0; i < snakeLength; i++) {
      if (snake[i].x == food.x && snake[i].y == food.y) {
        onSnake = true;
        break;
      }
    }
  } while (onSnake);
}

void updateSnakeGame() {
  if (snakeGameState != SNAKE_PLAYING) return;
  
  unsigned long currentTime = millis();
  if (currentTime - lastGameUpdate < gameSpeed) return;
  
 // Apply buffered direction with EXTRA safety check
  // Don't allow 180-degree turns when snake is longer than 2 segments
  if (snakeLength > 2) {
    bool isOppositeDirection = 
      (snakeDirection == UP && nextDirection == DOWN) ||
      (snakeDirection == DOWN && nextDirection == UP) ||
      (snakeDirection == LEFT && nextDirection == RIGHT) ||
      (snakeDirection == RIGHT && nextDirection == LEFT);
    
    if (!isOppositeDirection) {
      snakeDirection = nextDirection;
    }
  } else {
    // Allow any direction for very short snakes
    snakeDirection = nextDirection;
  }
  
  // Move snake
  Point newHead = snake[0];
  
switch (snakeDirection) {
    case UP:
      newHead.y = (newHead.y - 1 + GRID_HEIGHT) % GRID_HEIGHT;
      break;
    case DOWN:
      newHead.y = (newHead.y + 1) % GRID_HEIGHT;
      break;
    case LEFT:
      newHead.x = (newHead.x - 1 + GRID_WIDTH) % GRID_WIDTH;
      break;
    case RIGHT:
      newHead.x = (newHead.x + 1) % GRID_WIDTH;
      break;
  }
  
  // Check collision with self (skip the head itself)
  for (int i = 1; i < snakeLength; i++) {
    if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
      snakeGameState = SNAKE_GAME_OVER;
      settings.saveSnakeHighScore();
      feedback.gameOverSound();
      
      // Vibrate on game over
      if (settings.vibrationEnabled) {
        ledcWrite(vibPwmChannel, vibIntensities[settings.vibIntensityIndex]);
        delay(300);
        ledcWrite(vibPwmChannel, 0);
      }
      return;
    }
  }
  
  
  // Shift snake body
  for (int i = snakeLength; i > 0; i--) {
    snake[i] = snake[i - 1];
  }
  snake[0] = newHead;
  lastGameUpdate = currentTime;
  
  // Check if snake ate food
  if (newHead.x == food.x && newHead.y == food.y) {
    snakeLength++;
    snakeScore += 10;
    
    // Speed up game
    gameSpeed = max(GAME_SPEED_MIN, gameSpeed - SPEED_INCREASE);
    
    // Generate new food
    generateFood();
    
    // Play eat sound
    feedback.snakeEatSound();
    
    // Activate growth effect
    growthEffectActive = true;
    growthEffectStart = currentTime;
    
    // Flash backlight
    backlight.growthFlash();
  }
  
  // Update growth effect
  if (growthEffectActive && currentTime - growthEffectStart > GROWTH_EFFECT_DURATION) {
    growthEffectActive = false;
  }
}

void drawSnakeGame() {
  u8g2.clearBuffer();
  
  // Draw game area border
  u8g2.drawFrame(0, 0, GAME_WIDTH, GAME_HEIGHT);
  
  // Draw snake with enhanced visuals
  for (int i = 0; i < snakeLength; i++) {
    int x = snake[i].x * GRID_SIZE;
    int y = snake[i].y * GRID_SIZE;
    
    if (i == 0) {
      // Head - draw filled with eyes
      u8g2.drawBox(x, y, GRID_SIZE, GRID_SIZE);
      
      // Draw eyes based on direction
      u8g2.setDrawColor(0);
      if (snakeDirection == RIGHT) {
        u8g2.drawBox(x + GRID_SIZE - 2, y + 1, 1, 1);
        u8g2.drawBox(x + GRID_SIZE - 2, y + GRID_SIZE - 2, 1, 1);
      } else if (snakeDirection == LEFT) {
        u8g2.drawBox(x + 1, y + 1, 1, 1);
        u8g2.drawBox(x + 1, y + GRID_SIZE - 2, 1, 1);
      } else if (snakeDirection == UP) {
        u8g2.drawBox(x + 1, y + 1, 1, 1);
        u8g2.drawBox(x + GRID_SIZE - 2, y + 1, 1, 1);
      } else if (snakeDirection == DOWN) {
        u8g2.drawBox(x + 1, y + GRID_SIZE - 2, 1, 1);
        u8g2.drawBox(x + GRID_SIZE - 2, y + GRID_SIZE - 2, 1, 1);
      }
      u8g2.setDrawColor(1);
    } else {
      // Body - draw frame with rounded corners effect
      if (growthEffectActive && i == snakeLength - 1) {
        // Highlight newest segment during growth effect
        u8g2.drawBox(x, y, GRID_SIZE, GRID_SIZE);
      } else {
        u8g2.drawFrame(x, y, GRID_SIZE, GRID_SIZE);
        // Add some texture to body segments
        u8g2.drawPixel(x + 1, y + 1);
        u8g2.drawPixel(x + GRID_SIZE - 2, y + GRID_SIZE - 2);
      }
    }
  }
  
  // Draw food with enhanced visuals
  int foodX = food.x * GRID_SIZE + GRID_SIZE/2;
  int foodY = food.y * GRID_SIZE + GRID_SIZE/2;
  u8g2.drawDisc(foodX, foodY, GRID_SIZE/2);
  u8g2.setDrawColor(0);
  u8g2.drawPixel(foodX - 1, foodY - 1);
  u8g2.setDrawColor(1);
  
  // Draw score and high score at bottom with better layout
  u8g2.setFont(u8g2_font_6x10_tf);
  char scoreStr[20];
  snprintf(scoreStr, sizeof(scoreStr), "SCORE: %d", snakeScore);
  u8g2.drawStr(2, GAME_HEIGHT + 12, scoreStr);
  
  char highStr[20];
  snprintf(highStr, sizeof(highStr), "HIGH: %d", snakeHighScore);
  u8g2.drawStr(GAME_WIDTH - u8g2.getStrWidth(highStr) - 2, GAME_HEIGHT + 12, highStr);
  
  // Draw speed indicator
  char speedStr[10];
  int speedPercent = map(gameSpeed, GAME_SPEED_START, GAME_SPEED_MIN, 0, 100);
  snprintf(speedStr, sizeof(speedStr), "SPD: %d%%", speedPercent);
  u8g2.drawStr(GAME_WIDTH/2 - u8g2.getStrWidth(speedStr)/2, GAME_HEIGHT + 12, speedStr);
  
  // Draw length indicator
  u8g2.setFont(u8g2_font_5x7_tf);
  char lenStr[10];
  snprintf(lenStr, sizeof(lenStr), "LEN: %d", snakeLength);
  u8g2.drawStr(GAME_WIDTH/2 - u8g2.getStrWidth(lenStr)/2, GAME_HEIGHT + 22, lenStr);
  
  // Draw controls hint (only show briefly at start)
  static unsigned long controlsHideTime = 0;
  if (millis() - lastGameUpdate < 5000 || snakeScore < 30) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(2, GAME_HEIGHT + 22, "ROTATE:DIR");
    u8g2.drawStr(GAME_WIDTH - 40, GAME_HEIGHT + 22, "PRESS:PAUSE");
    controlsHideTime = millis();
  }
  
  u8g2.sendBuffer();
}

void drawSnakeMenu() {
  u8g2.clearBuffer();
  
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(40, 15, "SNAKE");
  
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Show high score prominently
  char highStr[30];
  snprintf(highStr, sizeof(highStr), "HIGH SCORE: %d", snakeHighScore);
  u8g2.drawStr((128 - u8g2.getStrWidth(highStr)) / 2, 30, highStr);
  
  // Game instructions
  u8g2.drawStr(20, 45, "Rotate: Change direction");
  u8g2.drawStr(30, 55, "Press: Start/Pause");
  u8g2.drawStr(20, 62, "Long press: Exit");
  
  u8g2.sendBuffer();
}

void drawSnakeGameOver() {
  u8g2.clearBuffer();
  
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(20, 20, "GAME OVER");
  
  u8g2.setFont(u8g2_font_6x10_tf);
  
  char scoreStr[20];
  snprintf(scoreStr, sizeof(scoreStr), "Score: %d", snakeScore);
  u8g2.drawStr((128 - u8g2.getStrWidth(scoreStr)) / 2, 35, scoreStr);
  
  char lengthStr[20];
  snprintf(lengthStr, sizeof(lengthStr), "Length: %d", snakeLength);
  u8g2.drawStr((128 - u8g2.getStrWidth(lengthStr)) / 2, 45, lengthStr);
  
  if (snakeScore == snakeHighScore && snakeHighScore > 0) {
    u8g2.drawStr(25, 55, "NEW HIGH SCORE!");
  } else {
    char highStr[20];
    snprintf(highStr, sizeof(highStr), "High: %d", snakeHighScore);
    u8g2.drawStr((128 - u8g2.getStrWidth(highStr)) / 2, 55, highStr);
  }
  
  u8g2.drawStr(15, 62, "Press: Play again");
  
  u8g2.sendBuffer();
}

void drawSnakePaused() {
  drawSnakeGame(); // Draw game in background
  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.setDrawColor(1);
  u8g2.drawBox(30, 15, 68, 20);
  u8g2.setDrawColor(0);
  u8g2.drawStr(40, 30, "PAUSED");
  u8g2.sendBuffer();
}

void handleSnakeInput() {
  static bool lastButtonState = HIGH;
  static unsigned long lastButtonPress = 0;
  static unsigned long lastDirectionChange = 0;
  const unsigned long DIRECTION_CHANGE_COOLDOWN = 80; // Increased cooldown
  unsigned long now = millis();
  
  // Get encoder delta
  int encoderDelta = input.getEncoderDelta();
  
  // Handle direction changes
  if (encoderDelta != 0 && snakeGameState == SNAKE_PLAYING) {
    // Only process direction changes at certain intervals
    if (now - lastDirectionChange >= DIRECTION_CHANGE_COOLDOWN) {
      
      // Calculate new direction based on encoder rotation
      Direction newDirection = nextDirection;
      
      // Process multiple encoder steps at once
      int steps = abs(encoderDelta);
      for (int i = 0; i < steps; i++) {
        if (encoderDelta > 0) { // Clockwise
          switch (newDirection) {
            case UP: newDirection = RIGHT; break;
            case RIGHT: newDirection = DOWN; break;
            case DOWN: newDirection = LEFT; break;
            case LEFT: newDirection = UP; break;
          }
        } else { // Counter-clockwise
          switch (newDirection) {
            case UP: newDirection = LEFT; break;
            case LEFT: newDirection = DOWN; break;
            case DOWN: newDirection = RIGHT; break;
            case RIGHT: newDirection = UP; break;
          }
        }
      }
      
      // IMPORTANT: Don't allow 180-degree turns
      // This prevents immediate self-collision when rotating quickly
      bool isOppositeDirection = 
        (snakeDirection == UP && newDirection == DOWN) ||
        (snakeDirection == DOWN && newDirection == UP) ||
        (snakeDirection == LEFT && newDirection == RIGHT) ||
        (snakeDirection == RIGHT && newDirection == LEFT);
      
      // Only update direction if it's not opposite AND if snake length > 1
      // For very short snakes, we can be more lenient
      if (!isOppositeDirection || snakeLength <= 2) {
        nextDirection = newDirection;
        lastDirectionChange = now;
        
        // Quick haptic feedback
        if (settings.vibrationEnabled) {
          ledcWrite(vibPwmChannel, 32); // Very light feedback
          delay(2);
          ledcWrite(vibPwmChannel, 0);
        }
      }
    }
  }
  
  // Handle button
  bool buttonState = digitalRead(ENCODER_SW);
  
  if (buttonState == LOW && lastButtonState == HIGH) {
    lastButtonPress = now;
  }
  
  if (buttonState == HIGH && lastButtonState == LOW) {
    if (now - lastButtonPress > 300) {
      // Long press - exit to main menu
      inGameMode = false;
      snakeGameState = SNAKE_MENU;
      feedback.trigger(false, false, true);
    } else {
      // Short press
      switch (snakeGameState) {
        case SNAKE_MENU:
          snakeGameState = SNAKE_PLAYING;
          feedback.trigger(true);
          break;
        case SNAKE_PLAYING:
          snakeGameState = SNAKE_PAUSED;
          feedback.trigger(true);
          break;
        case SNAKE_PAUSED:
          snakeGameState = SNAKE_PLAYING;
          feedback.trigger(true);
          break;
        case SNAKE_GAME_OVER:
          initSnakeGame();
          snakeGameState = SNAKE_PLAYING;
          feedback.trigger(true);
          break;
      }
    }
  }
  
  lastButtonState = buttonState;
}

void runSnakeGameLoop() {
  handleSnakeInput();
  
  switch (snakeGameState) {
    case SNAKE_MENU:
      drawSnakeMenu();
      break;
    case SNAKE_PLAYING:
      updateSnakeGame();
      drawSnakeGame();
      break;
    case SNAKE_PAUSED:
      drawSnakePaused();
      break;
    case SNAKE_GAME_OVER:
      drawSnakeGameOver();
      break;
  }
}

// DisplayManager class
class DisplayManager {
public:
  Menu* currentMenu = nullptr;
  bool showingFullScreen = false;
  uint8_t lastSelectedIndex = 0;

  void setup() {
    Wire.begin(20, 21);
    u8g2.begin();
    u8g2.setFont(u8g2_font_helvB08_tf);
    u8g2.setFontMode(1);
    u8g2.setContrast(200);
    
    // Show splash screen
    u8g2.clearBuffer();
    u8g2.drawStr(10, 30, "SNAKE GAME");
    u8g2.drawStr(20, 50, "LOADING...");
    u8g2.sendBuffer();
    delay(500);
    
    currentMenu = &mainMenu;
    drawMenu(*currentMenu);
    Serial.println("Display initialized");
  }

  void drawMenu(Menu& menu) {
    u8g2.clearBuffer();
    u8g2.drawFrame(1, 1, 126, 62);
    
    drawTitleBar(menu.title);
    
    uint8_t firstVisible = menu.firstVisibleIndex;
    uint8_t itemsToShow = min((uint8_t)VISIBLE_ITEMS, (uint8_t)(menu.itemCount - firstVisible));
    if (menu.itemCount > VISIBLE_ITEMS) firstVisible = min(firstVisible, (uint8_t)(menu.itemCount - VISIBLE_ITEMS));
    else firstVisible = 0;

    for (uint8_t i = 0; i < itemsToShow; i++) {
        uint8_t itemIndex = firstVisible + i;
        int16_t yPos = TITLE_HEIGHT + MENU_TOP_OFFSET + (i * ITEM_HEIGHT);
        drawMenuItem(menu.items[itemIndex], 0, yPos, itemIndex == menu.selectedIndex);
    }

    lastSelectedIndex = menu.selectedIndex;
    if (menu.itemCount > VISIBLE_ITEMS) drawScrollIndicator(firstVisible, menu.itemCount);
    u8g2.sendBuffer();
  }

  void update() {
    // Simple update for menu cursor
    if (!showingFullScreen && currentMenu && !inGameMode) {
      if (lastSelectedIndex != currentMenu->selectedIndex) {
        drawMenu(*currentMenu);
        lastSelectedIndex = currentMenu->selectedIndex;
      }
    }
  }

  bool isShowingFullScreen() { return showingFullScreen; }

  void setCurrentMenu(Menu* menu) {
    currentMenu = menu;
    showingFullScreen = false;
    if (menu) {
      menu->selectedIndex = constrain(menu->selectedIndex, 0, menu->itemCount - 1);
      menu->firstVisibleIndex = constrain(menu->firstVisibleIndex, 0, (uint8_t)(menu->itemCount > VISIBLE_ITEMS ? menu->itemCount - VISIBLE_ITEMS : 0));
      drawMenu(*menu);
    }
  }

  Menu* getCurrentMenu() { return currentMenu; }

private:
  void drawTitleBar(const char* title) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, TITLE_HEIGHT);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_helvB08_tf);
    
    uint8_t titleWidth = u8g2.getStrWidth(title);
    u8g2.drawStr((128 - titleWidth)/2, TITLE_HEIGHT - 3, title);
  }

  void drawMenuItem(MenuItem& item, int16_t x, int16_t y, bool isSelected) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(x, y - ITEM_HEIGHT/2, 128, ITEM_HEIGHT);
    
    if (isSelected) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(x, y - ITEM_HEIGHT/2, 128, ITEM_HEIGHT);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }
    
    // Draw text
    u8g2.setFont(u8g2_font_helvR08_tf);
    u8g2.drawStr(x + 8, y, item.name);
    
    // Draw value or indicator
    if (item.valueLabels && item.valueIndex && *item.valueIndex < 4) {
      u8g2.setCursor(80, y);
      u8g2.print(item.valueLabels[*item.valueIndex]);
    } 
    else if (item.showToggle && item.toggleState) {
      drawIcon(90, y, *item.toggleState ? ICON_TOGGLE_ON : ICON_TOGGLE_OFF, isSelected);
    } 
    else if (item.submenu) {
      drawIcon(100, y, ICON_ARROW, isSelected);
    }
    
    // Draw selection cursor
    if (isSelected) {
      u8g2.setDrawColor(1);
      u8g2.drawTriangle(x + 2, y, x + 8, y - 4, x + 8, y + 4);
    }
  }

  void drawIcon(int16_t x, int16_t y, uint8_t iconType, bool inverted = false) {
    u8g2.setDrawColor(inverted ? 0 : 1);
    
    switch (iconType) {
      case ICON_ARROW:
        u8g2.drawLine(x, y, x + 4, y);
        u8g2.drawLine(x + 1, y - 1, x + 3, y - 1);
        u8g2.drawLine(x + 2, y - 2, x + 2, y - 2);
        u8g2.drawLine(x + 1, y + 1, x + 3, y + 1);
        u8g2.drawLine(x + 2, y + 2, x + 2, y + 2);
        break;
        
      case ICON_TOGGLE_ON:
        u8g2.drawDisc(x, y, 3);
        break;
        
      case ICON_TOGGLE_OFF:
        u8g2.drawCircle(x, y, 3);
        break;
    }
  }

  void drawScrollIndicator(uint8_t firstItem, uint8_t totalItems) {
    uint8_t indicatorHeight = (VISIBLE_ITEMS * (64 - TITLE_HEIGHT - MENU_TOP_OFFSET)) / totalItems;
    uint8_t indicatorPos = (firstItem * (64 - TITLE_HEIGHT - MENU_TOP_OFFSET)) / totalItems + TITLE_HEIGHT + MENU_TOP_OFFSET;
    u8g2.drawFrame(125, indicatorPos, 3, indicatorHeight);
  }
};
DisplayManager display;

// MenuManager class
class MenuManager {
public:
  Menu* currentMenu = &mainMenu;

  void setCurrent(Menu* menu) {
    currentMenu = menu;
    display.setCurrentMenu(menu);
  }

  void handleInput() {
    int16_t delta = input.getEncoderDelta();
    
    if (delta != 0 && currentMenu && !display.isShowingFullScreen()) {
      if (delta > 0) {
        currentMenu->selectNext();
      } else if (delta < 0) {
        currentMenu->selectPrev();
      }
      feedback.trigger(false);
      display.drawMenu(*currentMenu);
    }
    
    if (input.getButtonPressed()) {
      if (display.isShowingFullScreen()) {
        display.setCurrentMenu(currentMenu);
        display.drawMenu(*currentMenu);
        feedback.trigger(true);
      }
      else if (currentMenu) {
        MenuItem& item = currentMenu->items[currentMenu->selectedIndex];
        
        if (item.submenu) {
          setCurrent(item.submenu);
        } 
        else if (strcmp(item.name, "Back") == 0 && currentMenu->parent) {
          setCurrent(currentMenu->parent);
        }
        else if (item.action) {
          item.action();
          if (!display.isShowingFullScreen()) {
            display.drawMenu(*currentMenu);
          }
        }
        feedback.trigger(true);
      }
    }
    
    if (input.getLongPress()) {
      if (currentMenu && currentMenu->parent) {
        setCurrent(currentMenu->parent);
        feedback.trigger(false, false, true);
      }
    }
  }
};
MenuManager menuManager;

// Menu actions implementation
void toggleSound() {
  settings.soundEnabled = !settings.soundEnabled;
  settings.save(EEPROM_SOUND_ADDR, settings.soundEnabled);
  settings.commit();
  feedback.trigger(true);
}

void toggleVibration() {
  settings.vibrationEnabled = !settings.vibrationEnabled;
  settings.save(EEPROM_VIBRATION_ADDR, settings.vibrationEnabled);
  settings.commit();
  feedback.trigger(true);
}

void cycleSleepTime() {
  settings.sleepTimeIndex = (settings.sleepTimeIndex + 1) % 4;
  settings.inactivityTimeout = sleepTimes[settings.sleepTimeIndex];
  settings.save(EEPROM_SLEEPTIME_ADDR, settings.sleepTimeIndex);
  settings.commit();
  feedback.trigger(true);
}

void cycleVibIntensity() {
  settings.vibIntensityIndex = (settings.vibIntensityIndex + 1) % 3;
  settings.save(EEPROM_VIBINTENSITY_ADDR, settings.vibIntensityIndex);
  settings.commit();
  feedback.trigger(true);
}

void cycleSoundVolume() {
  settings.soundVolumeIndex = (settings.soundVolumeIndex + 1) % 3;
  settings.save(EEPROM_SOUND_VOLUME_ADDR, settings.soundVolumeIndex);
  settings.commit();
  feedback.trigger(true);
}

void cycleBacklightTimeout() {
  settings.backlightTimeoutIndex = (settings.backlightTimeoutIndex + 1) % 4;
  settings.backlightTimeout = backlightTimeouts[settings.backlightTimeoutIndex];
  settings.save(EEPROM_BACKLIGHT_TIMEOUT_ADDR, settings.backlightTimeoutIndex);
  settings.commit();
  feedback.trigger(true);
}

void cycleDimPercentage() {
  settings.dimPercentageIndex = (settings.dimPercentageIndex + 1) % 3;
  settings.save(EEPROM_DIM_PERCENTAGE_ADDR, settings.dimPercentageIndex);
  settings.commit();
  feedback.trigger(true);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting Snake Game System");

  randomSeed(analogRead(0));
  initMenus();
  settings.setup();
  feedback.setup();
  input.setup();
  display.setup();
  backlight.setup();

  menuManager.setCurrent(&mainMenu);
  Serial.println("Setup complete");
}

void loop() {
  input.update();
  backlight.update();
  display.update();
  
  if (inGameMode) {
    // Run snake game loop
    runSnakeGameLoop();
    
    // Keep backlight on during game
    backlight.turnOn();
  } else {
    // Normal menu mode
    menuManager.handleInput();
    
    // Reset backlight timer on any input
    if (input.getEncoderDelta() != 0 || input.getButtonPressed() || input.getLongPress()) {
      backlight.turnOn();
    }
  }
  
  delay(10); // Faster loop for better responsiveness
}
