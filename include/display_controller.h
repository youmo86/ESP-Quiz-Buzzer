#pragma once
#ifdef SERVER

#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include "protocol.h"

class DisplayController {
public:
  explicit DisplayController(Adafruit_ILI9341& display);
  void begin();
  void update();
  void forceRefresh();
  void setTestMode(bool enabled);
  void showTestBuzz(uint8_t slot);

private:
  Adafruit_ILI9341& tft;
  Phase lastPhase;
  uint8_t lastClientCount;
  uint8_t lastQueueLength;
  int8_t lastActiveClientIndex;
  uint32_t lastRefresh;
  bool firstDraw;
  bool testMode;
  uint8_t testBuzzSlot;
  uint32_t testBuzzAt;

  void drawScreen();
  void drawHeader();
  void drawClientGrid();
  void drawStatus();
  void drawFooter();
  void drawTestScreen();
  void drawCentered(const String& text, int16_t y, uint8_t size, uint16_t color);
  uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) const;
  uint8_t batteryPercent(uint16_t millivolts) const;
};

extern DisplayController* displayController;

#endif
