#define SERVER 1
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include "config.h"
#include "protocol.h"
#include "mqtt_server.h"
#include "led_controller.h"
#include "game_manager.h"

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

Bounce btnBack;
Bounce btnNext;
Bounce btnWrong;
Bounce btnCorrect;

static uint32_t backPressedAt = 0;
static bool backResetTriggered = false;

static void setupButton(Bounce& button, uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  button.attach(pin);
  button.interval(DEBOUNCE_MS);
}

static void handleMasterButtons() {
  btnBack.update();
  btnNext.update();
  btnWrong.update();
  btnCorrect.update();

  // ◀ : deliberate long hold to reset the whole quiz to the beginning.
  if (btnBack.fell()) {
    backPressedAt = millis();
    backResetTriggered = false;
  }
  if (btnBack.read() == LOW && backPressedAt && !backResetTriggered &&
      millis() - backPressedAt >= MASTER_RESET_HOLD_MS) {
    if (gameManager) gameManager->resetGame();
    backResetTriggered = true;
  }
  if (btnBack.rose()) {
    backPressedAt = 0;
    backResetTriggered = false;
  }

  // ▶ : start the quiz/question, or force the next question from any game state.
  if (btnNext.fell() && gameManager) {
    if (currentPhase == Phase::LOBBY) {
      if (gameClientCount >= MIN_CLIENTS_TO_START) {
        gameLocked = true;
        gameManager->startQuestion();
      }
    } else if (currentPhase != Phase::BOOT) {
      gameManager->startQuestion();
    }
  }

  // ✕ : current answer is wrong; advance to the next queued buzzer.
  if (btnWrong.fell() && gameManager && currentPhase == Phase::ANSWER) {
    gameManager->nextClient();
  }

  // ✓ : current answer is correct; finish the question.
  if (btnCorrect.fell() && gameManager && currentPhase == Phase::ANSWER) {
    gameManager->correctAnswer();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Quiz-Buzzer Master Starting...");
  Serial.printf("Master: WS2812 GPIO %d (%d LEDs) | TFT SPI SCK=%d MISO=%d MOSI=%d CS=%d DC=%d RST=%d\n",
                LED_PIN, LED_COUNT, TFT_SCK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN,
                TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.clear();
  strip.show();
  ledController = new LEDController(strip);

  setupButton(btnBack, BTN_BACK_PIN);
  setupButton(btnNext, BTN_NEXT_PIN);
  setupButton(btnWrong, BTN_WRONG_PIN);
  setupButton(btnCorrect, BTN_CORRECT_PIN);

  gameManager = new GameManager();

  Serial.println("Setting up WiFi Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PSK, 1, 0, MAX_CLIENTS);
  WiFi.softAPConfig(IPAddress(AP_IP_ADDR), IPAddress(AP_GATEWAY_ADDR), IPAddress(AP_SUBNET_ADDR));

  Serial.printf("AP SSID: %s\n", WIFI_SSID);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

  mqttBroker.subscribe(Topic::JOIN, [](const char * payload) { handleClientJoin(String(payload)); });
  mqttBroker.subscribe(Topic::BUZZ, [](const char * payload) { handleClientBuzz(String(payload)); });
  mqttBroker.subscribe(Topic::PING, [](const char * payload) { handleClientPing(String(payload)); });
  mqttBroker.begin();

  publishAnnounce();
  gameManager->publishGameState();

  ledController->showRGBTest();

  Serial.println("Master controls:");
  Serial.println("  NEXT    (>) : start / next question");
  Serial.println("  BACK    (<) : hold 2s to reset to lobby");
  Serial.println("  WRONG   (X) : wrong answer / next queued buzzer");
  Serial.println("  CORRECT (OK): correct answer / finish question");
}

void loop() {
  mqttBroker.loop();
  handleMasterButtons();

  if (gameManager) {
    gameManager->handlePhase();
    gameManager->sendPingToAllClients();
  }

  static uint32_t lastClientCheck = 0;
  if (millis() - lastClientCheck > 5000) {
    checkClientTimeouts();
    lastClientCheck = millis();
  }

  delay(10);
}
