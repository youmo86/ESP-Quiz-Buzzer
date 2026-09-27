#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// Master WS2812B controller: one 10-LED strip, no legacy 8-LED ring.
class LEDController {
private:
  Adafruit_NeoPixel& strip;

public:
  explicit LEDController(Adafruit_NeoPixel& ledStrip);

  void setPixelColor(uint16_t pixel, const Rgb& color);
  void clearAllLEDs();
  void showRGBTest();
  void showLEDs();

  // Each LED maps naturally to one of the 10 client slots when a per-client view is useful.
  void setClientLED(uint8_t slotIndex, const Rgb& color);
  void showConnectedClients();

  // Whole-strip game feedback.
  void showSolid(const Rgb& color);
  void updateServerLEDs();
  void flashWrong();

  // Non-blocking animations, called repeatedly from GameManager::handlePhase().
  void animateLobby();
  void animateReadyPingPong();
  void animateReady();
  void animateOpen();
};

extern LEDController* ledController;
