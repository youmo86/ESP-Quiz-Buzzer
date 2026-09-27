#include "led_controller.h"
#include "mqtt_server.h"
#include <math.h>

LEDController* ledController = nullptr;

LEDController::LEDController(Adafruit_NeoPixel& ledStrip) : strip(ledStrip) {}

void LEDController::setPixelColor(uint16_t pixel, const Rgb& color) {
  if (pixel < LED_COUNT) strip.setPixelColor(pixel, strip.Color(color.r, color.g, color.b));
}

void LEDController::clearAllLEDs() {
  strip.clear();
  strip.show();
}

void LEDController::showLEDs() {
  strip.show();
}

void LEDController::showRGBTest() {
  // Short startup hardware test. This is intentionally the only blocking LED sequence.
  const Rgb testColors[] = { Rgb(255, 0, 0), Rgb(0, 255, 0), Rgb(0, 0, 255) };
  Serial.printf("Testing RGB on %u master strip LEDs...\n", LED_COUNT);
  for (const Rgb& color : testColors) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) setPixelColor(i, color);
    strip.show();
    delay(350);
  }
  clearAllLEDs();
}

void LEDController::setClientLED(uint8_t slotIndex, const Rgb& color) {
  if (slotIndex < LED_COUNT) setPixelColor(slotIndex, color);
}

void LEDController::showSolid(const Rgb& color) {
  for (uint8_t i = 0; i < LED_COUNT; ++i) setPixelColor(i, color);
  strip.show();
}

void LEDController::showConnectedClients() {
  strip.clear();
  for (uint8_t i = 0; i < gameClientCount && i < LED_COUNT; ++i) {
    if (gameClients[i].connected) setClientLED(i, gameClients[i].color);
  }
  strip.show();
}

void LEDController::updateServerLEDs() {
  // During ANSWER, the whole strip identifies the currently active player.
  if (activeClientIndex >= 0 && activeClientIndex < queueLength) {
    const String& activeId = buzzQueue[activeClientIndex];
    for (uint8_t i = 0; i < gameClientCount; ++i) {
      if (gameClients[i].id == activeId) {
        showSolid(gameClients[i].color);
        return;
      }
    }
  }
  clearAllLEDs();
}

void LEDController::flashWrong() {
  // Immediate short visual feedback. Game state animation takes over afterwards.
  showSolid(Rgb(255, 0, 0));
}

void LEDController::animateLobby() {
  // Connected clients keep their own slot/color. A soft white cursor moves across empty slots.
  static uint32_t lastUpdate = 0;
  static uint8_t cursor = 0;
  if (millis() - lastUpdate < 180) return;

  strip.clear();
  for (uint8_t i = 0; i < gameClientCount && i < LED_COUNT; ++i) {
    if (gameClients[i].connected) {
      Rgb c = gameClients[i].color;
      setClientLED(i, Rgb(c.r / 3, c.g / 3, c.b / 3));
    }
  }

  // Cursor is only an ambience effect and never hides a connected player's colour.
  if (cursor >= gameClientCount || !gameClients[cursor].connected) setClientLED(cursor, Rgb(80, 80, 80));
  strip.show();
  cursor = (cursor + 1) % LED_COUNT;
  lastUpdate = millis();
}

void LEDController::animateReadyPingPong() {
  // READY: connected player colours breathe together.
  animateReady();
}

void LEDController::animateReady() {
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < PULSE_SPEED_MS) return;

  float breath = (sinf(millis() * 0.003f) + 1.0f) * 0.5f;
  uint8_t level = 45 + (uint8_t)(breath * 210.0f);

  strip.clear();
  for (uint8_t i = 0; i < gameClientCount && i < LED_COUNT; ++i) {
    if (!gameClients[i].connected) continue;
    const Rgb& c = gameClients[i].color;
    setClientLED(i, Rgb(
      (uint8_t)((uint16_t)c.r * level / 255),
      (uint8_t)((uint16_t)c.g * level / 255),
      (uint8_t)((uint16_t)c.b * level / 255)));
  }
  strip.show();
  lastUpdate = millis();
}

void LEDController::animateOpen() {
  // Question open: a green comet travels along the complete 10-LED strip.
  static uint32_t lastUpdate = 0;
  static uint8_t head = 0;
  if (millis() - lastUpdate < 70) return;

  strip.clear();
  for (uint8_t trail = 0; trail < 4; ++trail) {
    uint8_t pos = (head + LED_COUNT - trail) % LED_COUNT;
    uint8_t green = 255 >> trail;
    setPixelColor(pos, Rgb(0, green, 0));
  }
  strip.show();
  head = (head + 1) % LED_COUNT;
  lastUpdate = millis();
}
