#pragma once
#include <Arduino.h>

// Shared WS2812B data pin
constexpr uint8_t LED_PIN = 5;

#ifdef SERVER
  // Master: 10-LED WS2812B strip only (the old 8-LED ring is removed)
  constexpr uint16_t LED_COUNT = 10;

  // ILI9341 2.8" TFT - hardware VSPI, landscape 320x240
  // SPI bus kept on the ESP32 standard VSPI pins.
  constexpr uint8_t TFT_SCK_PIN  = 18;
  constexpr uint8_t TFT_MISO_PIN = 19;
  constexpr uint8_t TFT_MOSI_PIN = 23;
  constexpr uint8_t TFT_CS_PIN   = 17;
  constexpr uint8_t TFT_DC_PIN   = 16;
  constexpr uint8_t TFT_RST_PIN  = 4;

  // Master controls - grouped GPIOs, active LOW with INPUT_PULLUP
  constexpr uint8_t BTN_BACK_PIN    = 25; // ◀ hold = reset to beginning
  constexpr uint8_t BTN_NEXT_PIN    = 26; // ▶ start / next question
  constexpr uint8_t BTN_WRONG_PIN   = 27; // ✕ wrong answer
  constexpr uint8_t BTN_CORRECT_PIN = 32; // ✓ correct answer
#else
  // Client: 8-LED ring + main buzzer button
  constexpr uint16_t LED_COUNT = 8;
  constexpr uint8_t BUTTON_PIN = 18;

  // Client battery monitoring
  constexpr uint8_t BATTERY_ADC_PIN = 34;       // ADC1 input; usable while WiFi is active
  constexpr uint8_t BATTERY_ADC_SAMPLES = 8;    // Average samples for a stable reading
  constexpr float BATTERY_DIVIDER_RATIO = 2.0f; // 100k / 100k voltage divider
#endif

// LED Configuration
constexpr uint8_t LED_BRIGHTNESS = 64; // 0..255

// Button Timing Configuration (ms)
constexpr uint16_t DEBOUNCE_MS = 20;
constexpr uint16_t SHORT_PRESS_MAX_MS = 600;
constexpr uint16_t LONG_PRESS_MS = 1200;
constexpr uint16_t VERY_LONG_PRESS_MS = 4000;
constexpr uint16_t MASTER_RESET_HOLD_MS = 2000;

// WiFi/Network Configuration
constexpr char WIFI_SSID[] = "QUIZ-HUB";
constexpr char WIFI_PSK[] = "quiz12345"; // TODO: Change in production

#define AP_IP_ADDR 192, 168, 4, 1
#define AP_GATEWAY_ADDR 192, 168, 4, 1
#define AP_SUBNET_ADDR 255, 255, 255, 0

// MQTT Configuration
constexpr char MQTT_HOST[] = "192.168.4.1";
constexpr uint16_t MQTT_PORT = 1883;
constexpr uint16_t MQTT_KEEPALIVE_INTERVAL = 60;

// Game Configuration
constexpr uint8_t MAX_CLIENTS = 10;
constexpr uint8_t MIN_CLIENTS_TO_START = 1;

// Ping Configuration
constexpr uint16_t PING_INTERVAL_MS = 5000;
constexpr uint16_t CLIENT_TIMEOUT_MS = 15000;

struct Rgb {
  uint8_t r, g, b;
  constexpr Rgb(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : r(red), g(green), b(blue) {}
};

constexpr Rgb PLAYER_COLORS[MAX_CLIENTS] = {
  Rgb(255, 0, 0),
  Rgb(0, 0, 255),
  Rgb(0, 255, 0),
  Rgb(255, 255, 0),
  Rgb(255, 0, 255),
  Rgb(0, 255, 255),
  Rgb(255, 128, 0),
  Rgb(128, 0, 255),
  Rgb(255, 192, 203),
  Rgb(255, 255, 255)
};

constexpr Rgb COLOR_WHITE(255, 255, 255);
constexpr Rgb COLOR_BLACK(0, 0, 0);
constexpr Rgb COLOR_ERROR(255, 0, 0);

constexpr uint16_t PULSE_SPEED_MS = 50;
constexpr uint16_t SPIN_SPEED_MS = 60;
constexpr uint16_t CELEBRATION_DURATION_MS = 5000;
constexpr uint16_t FLASH_DURATION_MS = 200;
