#define SERVER 1
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Bounce2.h>
#include "config.h"
#include "protocol.h"
#include "mqtt_server.h"
#include "led_controller.h"
#include "game_manager.h"
#include "display_controller.h"

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_ILI9341 tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
Bounce btnBack,btnNext,btnWrong,btnCorrect;
static uint32_t backPressedAt=0;static bool backResetTriggered=false;static bool diagnosticMode=false;

static void setupButton(Bounce& b,uint8_t pin){pinMode(pin,INPUT_PULLUP);b.attach(pin);b.interval(DEBOUNCE_MS);}

static void enterDiagnosticMode(){
  diagnosticMode=true;gameLocked=false;
  if(displayController)displayController->setTestMode(true);
  if(ledController)ledController->showConnectedClients();
  Serial.println("=== ENTER DIAGNOSTIC MODE FROM LOBBY ===");
}

static void exitDiagnosticMode(){
  diagnosticMode=false;
  if(displayController)displayController->setTestMode(false);
  if(gameManager)gameManager->resetGame();
  Serial.println("=== EXIT DIAGNOSTIC MODE ===");
}

static void handleDiagnosticBuzz(const String& payload){
  if(!diagnosticMode)return;
  StaticJsonDocument<200> doc;if(deserializeJson(doc,payload))return;String id=doc[JsonKey::ID];
  for(uint8_t i=0;i<gameClientCount;i++)if(gameClients[i].id==id){
    Serial.printf("TEST: buzz received from slot %u (%s)\n",gameClients[i].slot,id.c_str());
    if(displayController)displayController->showTestBuzz(gameClients[i].slot);
    if(ledController)ledController->showSolid(gameClients[i].color);
    return;
  }
}

static void handleMasterButtons(){
  btnBack.update();btnNext.update();btnWrong.update();btnCorrect.update();

  if(diagnosticMode){
    if(btnBack.fell())exitDiagnosticMode();
    return;
  }

  // On the first/home screen (LOBBY), a normal BACK press opens the buzzer diagnostic screen.
  // No button needs to be held during power-up.
  if(currentPhase==Phase::LOBBY && btnBack.fell()){
    enterDiagnosticMode();
    backPressedAt=0;backResetTriggered=false;
    return;
  }

  if(btnBack.fell()){backPressedAt=millis();backResetTriggered=false;}
  if(btnBack.read()==LOW&&backPressedAt&&!backResetTriggered&&millis()-backPressedAt>=MASTER_RESET_HOLD_MS){
    if(gameManager)gameManager->resetGame();backResetTriggered=true;
  }
  if(btnBack.rose()){backPressedAt=0;backResetTriggered=false;}

  if(btnNext.fell()&&gameManager){
    if(currentPhase==Phase::LOBBY){if(gameClientCount>=MIN_CLIENTS_TO_START){gameLocked=true;gameManager->startQuestion();}}
    else if(currentPhase!=Phase::BOOT)gameManager->startQuestion();
    if(displayController)displayController->forceRefresh();
  }
  if(btnWrong.fell()&&gameManager&&currentPhase==Phase::ANSWER){gameManager->nextClient();if(displayController)displayController->forceRefresh();}
  if(btnCorrect.fell()&&gameManager&&currentPhase==Phase::ANSWER){gameManager->correctAnswer();if(displayController)displayController->forceRefresh();}
}

void setup(){
  Serial.begin(115200);Serial.println("ESP32 Quiz-Buzzer Master Starting...");

  strip.begin();strip.setBrightness(LED_BRIGHTNESS);strip.clear();strip.show();ledController=new LEDController(strip);
  setupButton(btnBack,BTN_BACK_PIN);setupButton(btnNext,BTN_NEXT_PIN);setupButton(btnWrong,BTN_WRONG_PIN);setupButton(btnCorrect,BTN_CORRECT_PIN);
  SPI.begin(TFT_SCK_PIN,TFT_MISO_PIN,TFT_MOSI_PIN,TFT_CS_PIN);displayController=new DisplayController(tft);displayController->begin();
  gameManager=new GameManager();

  WiFi.mode(WIFI_AP);WiFi.softAP(WIFI_SSID,WIFI_PSK,1,0,MAX_CLIENTS);WiFi.softAPConfig(IPAddress(AP_IP_ADDR),IPAddress(AP_GATEWAY_ADDR),IPAddress(AP_SUBNET_ADDR));
  mqttBroker.subscribe(Topic::JOIN,[](const char* p){handleClientJoin(String(p));});
  mqttBroker.subscribe(Topic::BUZZ,[](const char* p){if(diagnosticMode)handleDiagnosticBuzz(String(p));else handleClientBuzz(String(p));});
  mqttBroker.subscribe(Topic::PING,[](const char* p){handleClientPing(String(p));});mqttBroker.begin();
  publishAnnounce();gameManager->publishGameState();

  // Normal boot every time. Diagnostic mode is entered later from the lobby/home screen with BACK.
  ledController->showRGBTest();displayController->forceRefresh();
}

void loop(){
  mqttBroker.loop();handleMasterButtons();
  if(gameManager){
    if(!diagnosticMode)gameManager->handlePhase();
    gameManager->sendPingToAllClients();
  }
  if(displayController)displayController->update();
  static uint32_t lastClientCheck=0;if(millis()-lastClientCheck>5000){checkClientTimeouts();lastClientCheck=millis();}
  delay(10);
}
