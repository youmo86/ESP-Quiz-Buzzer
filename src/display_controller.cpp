#include "display_controller.h"
#ifdef SERVER

#include "mqtt_server.h"
#include "config.h"

DisplayController* displayController = nullptr;

DisplayController::DisplayController(Adafruit_ILI9341& display)
  : tft(display), lastPhase(Phase::BOOT), lastClientCount(255), lastQueueLength(255),
    lastActiveClientIndex(-127), lastRefresh(0), firstDraw(true), testMode(false),
    testBuzzSlot(0), testBuzzAt(0) {}

void DisplayController::begin() {
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);
  forceRefresh();
}

void DisplayController::setTestMode(bool enabled) {
  testMode = enabled;
  testBuzzSlot = 0;
  testBuzzAt = 0;
  forceRefresh();
}

void DisplayController::showTestBuzz(uint8_t slot) {
  testBuzzSlot = slot;
  testBuzzAt = millis();
  forceRefresh();
}

void DisplayController::forceRefresh() { firstDraw = true; drawScreen(); }

void DisplayController::update() {
  uint32_t now = millis();
  if (testMode) {
    if (firstDraw || now - lastRefresh >= 500 || (testBuzzSlot && now - testBuzzAt > 1200)) {
      if (testBuzzSlot && now - testBuzzAt > 1200) testBuzzSlot = 0;
      drawScreen();
    }
    return;
  }
  bool changed = firstDraw || currentPhase != lastPhase || gameClientCount != lastClientCount ||
                 queueLength != lastQueueLength || activeClientIndex != lastActiveClientIndex;
  if (changed || now - lastRefresh >= 1000) drawScreen();
}

uint16_t DisplayController::rgb565(uint8_t r, uint8_t g, uint8_t b) const { return tft.color565(r,g,b); }
uint8_t DisplayController::batteryPercent(uint16_t mv) const {
  if (!mv) return 255; if (mv >= 4200) return 100; if (mv <= 3300) return 0;
  return (uint8_t)(((uint32_t)(mv - 3300) * 100U) / 900U);
}

void DisplayController::drawCentered(const String& text, int16_t y, uint8_t size, uint16_t color) {
  int16_t x1,y1; uint16_t w,h; tft.setTextSize(size); tft.setTextColor(color, ILI9341_BLACK);
  tft.getTextBounds(text,0,y,&x1,&y1,&w,&h); tft.setCursor((320-(int16_t)w)/2,y); tft.print(text);
}

void DisplayController::drawHeader() {
  tft.fillRect(0,0,320,30,ILI9341_DARKCYAN);
  tft.setTextColor(ILI9341_WHITE); tft.setTextSize(2); tft.setCursor(9,7); tft.print("QUIZ MASTER");
  uint8_t connected=0; for(uint8_t i=0;i<gameClientCount;i++) if(gameClients[i].connected) connected++;
  tft.setCursor(247,7); tft.printf("%u/10",connected);
}

void DisplayController::drawClientGrid() {
  for(uint8_t slot=0;slot<MAX_CLIENTS;slot++) {
    uint8_t col=slot%5,row=slot/5; int16_t x=4+col*63,y=37+row*57;
    bool exists=slot<gameClientCount; uint16_t border=ILI9341_DARKGREY;
    if(exists){const Rgb& c=gameClients[slot].color;border=rgb565(c.r,c.g,c.b);}
    tft.drawRoundRect(x,y,60,51,6,border);
    if(exists && gameClients[slot].connected) tft.fillCircle(x+50,y+10,3,ILI9341_GREEN);
    tft.setTextSize(2);tft.setTextColor(exists&&gameClients[slot].connected?ILI9341_WHITE:ILI9341_DARKGREY,ILI9341_BLACK);
    tft.setCursor(x+6,y+5);tft.printf("%u",slot+1);
    tft.setTextSize(1);tft.setCursor(x+6,y+31);
    if(!exists)tft.print("LIBRE");
    else if(!gameClients[slot].connected)tft.print("OFF");
    else {uint8_t p=batteryPercent(gameClients[slot].batteryMv); if(p==255)tft.print("BAT --");else tft.printf("BAT %u%%",p);}
  }
}

void DisplayController::drawStatus() {
  tft.fillRect(0,151,320,54,ILI9341_BLACK);
  switch(currentPhase){
    case Phase::BOOT: drawCentered("DEMARRAGE",164,2,ILI9341_YELLOW);break;
    case Phase::LOBBY: drawCentered(gameClientCount>=MIN_CLIENTS_TO_START?">  DEMARRER":"ATTENTE BUZZERS",164,2,ILI9341_CYAN);break;
    case Phase::READY: drawCentered(">  OUVRIR QUESTION",164,2,ILI9341_CYAN);break;
    case Phase::OPEN: drawCentered("BUZZEZ !",159,3,ILI9341_GREEN);break;
    case Phase::ANSWER:
      if(activeClientIndex>=0&&activeClientIndex<queueLength){String id=buzzQueue[activeClientIndex];uint8_t slot=0;Rgb c(255,255,255);
        for(uint8_t i=0;i<gameClientCount;i++)if(gameClients[i].id==id){slot=gameClients[i].slot;c=gameClients[i].color;break;}
        drawCentered("BUZZER "+String(slot),157,3,rgb565(c.r,c.g,c.b));drawCentered("X MAUVAIS        OK BON",187,1,ILI9341_WHITE);}break;
    case Phase::RESET: drawCentered("BONNE REPONSE !",159,2,ILI9341_GREEN);drawCentered("> QUESTION SUIVANTE",186,1,ILI9341_WHITE);break;
    default:break;
  }
}

void DisplayController::drawFooter(){
  tft.fillRect(0,211,320,29,ILI9341_NAVY);tft.setTextSize(1);tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(7,221);tft.print("< RESET 2s");tft.setCursor(235,221);tft.print("> SUIVANT");
}

void DisplayController::drawTestScreen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0,0,320,31,ILI9341_ORANGE);tft.setTextColor(ILI9341_BLACK);tft.setTextSize(2);tft.setCursor(12,8);tft.print("MODE TEST BUZZERS");
  for(uint8_t slot=0;slot<MAX_CLIENTS;slot++){
    uint8_t col=slot%5,row=slot/5;int16_t x=4+col*63,y=43+row*63;bool exists=slot<gameClientCount;
    uint16_t border=ILI9341_DARKGREY;if(exists){const Rgb& c=gameClients[slot].color;border=rgb565(c.r,c.g,c.b);}
    if(testBuzzSlot==slot+1)tft.fillRoundRect(x,y,60,55,6,border);else tft.drawRoundRect(x,y,60,55,6,border);
    uint16_t text=(testBuzzSlot==slot+1)?ILI9341_BLACK:(exists&&gameClients[slot].connected?ILI9341_WHITE:ILI9341_DARKGREY);
    tft.setTextColor(text);tft.setTextSize(2);tft.setCursor(x+6,y+6);tft.printf("#%u",slot+1);
    tft.setTextSize(1);tft.setCursor(x+6,y+34);
    if(!exists)tft.print("ABSENT");else if(!gameClients[slot].connected)tft.print("OFFLINE");else {uint8_t p=batteryPercent(gameClients[slot].batteryMv);if(p==255)tft.print("OK BAT--");else tft.printf("OK %u%%",p);}
  }
  tft.fillRect(0,174,320,66,ILI9341_NAVY);
  if(testBuzzSlot){drawCentered("BUZZ RECU : #"+String(testBuzzSlot),183,2,ILI9341_GREEN);}
  else drawCentered("APPUYEZ SUR CHAQUE BUZZER",181,1,ILI9341_WHITE);
  drawCentered("< QUITTER LE MODE TEST",216,1,ILI9341_WHITE);
}

void DisplayController::drawScreen(){
  if(testMode)drawTestScreen();else{tft.fillScreen(ILI9341_BLACK);drawHeader();drawClientGrid();drawStatus();drawFooter();}
  lastPhase=currentPhase;lastClientCount=gameClientCount;lastQueueLength=queueLength;lastActiveClientIndex=activeClientIndex;lastRefresh=millis();firstDraw=false;
}

#endif
