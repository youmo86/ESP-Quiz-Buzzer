#include "display_controller.h"
#ifdef SERVER

#include "mqtt_server.h"
#include "config.h"

DisplayController* displayController = nullptr;

DisplayController::DisplayController(Adafruit_ILI9341& display)
  : tft(display), lastPhase(Phase::BOOT), lastClientCount(255), lastQueueLength(255),
    lastActiveClientIndex(-127), lastRefresh(0), firstDraw(true) {}

void DisplayController::begin() {
  tft.begin();
  tft.setRotation(1); // 320x240 landscape
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);
  forceRefresh();
}

void DisplayController::forceRefresh() {
  firstDraw = true;
  drawScreen();
}

void DisplayController::update() {
  const uint32_t now = millis();
  bool changed = firstDraw || currentPhase != lastPhase || gameClientCount != lastClientCount ||
                 queueLength != lastQueueLength || activeClientIndex != lastActiveClientIndex;

  // Refresh once per second as well so connection/battery telemetry becomes visible.
  if (changed || now - lastRefresh >= 1000) drawScreen();
}

uint16_t DisplayController::rgb565(uint8_t r, uint8_t g, uint8_t b) const {
  return tft.color565(r, g, b);
}

uint8_t DisplayController::batteryPercent(uint16_t mv) const {
  if (mv == 0) return 255; // unknown
  if (mv >= 4200) return 100;
  if (mv <= 3300) return 0;
  return (uint8_t)(((uint32_t)(mv - 3300) * 100U) / 900U);
}

void DisplayController::drawCentered(const String& text, int16_t y, uint8_t size, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(size);
  tft.setTextColor(color, ILI9341_BLACK);
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((320 - (int16_t)w) / 2, y);
  tft.print(text);
}

void DisplayController::drawHeader() {
  tft.fillRect(0, 0, 320, 28, ILI9341_DARKCYAN);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(8, 7);
  tft.print("QUIZ MASTER");

  uint8_t connected = 0;
  for (uint8_t i = 0; i < gameClientCount; ++i) if (gameClients[i].connected) ++connected;
  tft.setCursor(244, 7);
  tft.printf("%u/%u", connected, gameClientCount);
}

void DisplayController::drawClientGrid() {
  const int16_t top = 34;
  const int16_t cellW = 62;
  const int16_t cellH = 54;

  for (uint8_t slot = 0; slot < MAX_CLIENTS; ++slot) {
    const uint8_t col = slot % 5;
    const uint8_t row = slot / 5;
    const int16_t x = 3 + col * 63;
    const int16_t y = top + row * 57;

    bool exists = slot < gameClientCount;
    uint16_t border = ILI9341_DARKGREY;
    if (exists) {
      const Rgb& c = gameClients[slot].color;
      border = rgb565(c.r, c.g, c.b);
    }

    tft.drawRoundRect(x, y, cellW, cellH, 5, border);
    tft.setTextSize(2);
    tft.setTextColor(exists && gameClients[slot].connected ? ILI9341_WHITE : ILI9341_DARKGREY, ILI9341_BLACK);
    tft.setCursor(x + 5, y + 5);
    tft.printf("#%u", slot + 1);

    if (!exists) {
      tft.setTextSize(1);
      tft.setCursor(x + 5, y + 32);
      tft.print("---");
      continue;
    }

    uint8_t pct = batteryPercent(gameClients[slot].batteryMv);
    tft.setTextSize(1);
    tft.setCursor(x + 5, y + 31);
    if (!gameClients[slot].connected) {
      tft.print("OFFLINE");
    } else if (pct == 255) {
      tft.print("BAT --");
    } else {
      tft.printf("BAT %u%%", pct);
    }
  }
}

void DisplayController::drawStatus() {
  tft.fillRect(0, 151, 320, 51, ILI9341_BLACK);

  switch (currentPhase) {
    case Phase::BOOT:
      drawCentered("DEMARRAGE...", 166, 2, ILI9341_YELLOW);
      break;
    case Phase::LOBBY:
      drawCentered(gameClientCount >= MIN_CLIENTS_TO_START ? ">  DEMARRER" : "ATTENTE BUZZERS", 166, 2, ILI9341_CYAN);
      break;
    case Phase::READY:
      drawCentered(">  OUVRIR QUESTION", 166, 2, ILI9341_CYAN);
      break;
    case Phase::OPEN:
      drawCentered("BUZZEZ !", 160, 3, ILI9341_GREEN);
      break;
    case Phase::ANSWER:
      if (activeClientIndex >= 0 && activeClientIndex < queueLength) {
        String activeId = buzzQueue[activeClientIndex];
        uint8_t slot = 0;
        Rgb c(255, 255, 255);
        for (uint8_t i = 0; i < gameClientCount; ++i) {
          if (gameClients[i].id == activeId) { slot = gameClients[i].slot; c = gameClients[i].color; break; }
        }
        drawCentered("BUZZER #" + String(slot), 158, 3, rgb565(c.r, c.g, c.b));
        drawCentered("X MAUVAIS     OK BON", 187, 1, ILI9341_WHITE);
      }
      break;
    case Phase::RESET:
      drawCentered("BONNE REPONSE !", 160, 2, ILI9341_GREEN);
      drawCentered(">  QUESTION SUIVANTE", 185, 1, ILI9341_WHITE);
      break;
    default:
      break;
  }
}

void DisplayController::drawFooter() {
  tft.fillRect(0, 211, 320, 29, ILI9341_NAVY);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(7, 221);
  tft.print("< RESET(2s)");
  tft.setCursor(228, 221);
  tft.print("> SUIVANT");
}

void DisplayController::drawScreen() {
  tft.fillScreen(ILI9341_BLACK);
  drawHeader();
  drawClientGrid();
  drawStatus();
  drawFooter();

  lastPhase = currentPhase;
  lastClientCount = gameClientCount;
  lastQueueLength = queueLength;
  lastActiveClientIndex = activeClientIndex;
  lastRefresh = millis();
  firstDraw = false;
}

#endif
