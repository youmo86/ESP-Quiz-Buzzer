#include "game_manager.h"
#include "mqtt_server.h"
#include "led_controller.h"
#include <ArduinoJson.h>

// Global instances
ButtonHandler* buttonHandler = nullptr;
GameManager* gameManager = nullptr;

// ButtonHandler Implementation (kept for compatibility; master now uses four dedicated buttons)
ButtonHandler::ButtonHandler(Bounce& btn) : button(btn), lastButtonPress(0), buttonHeld(false) {}

ButtonPress ButtonHandler::checkButtonPress() {
  button.update();
  if (button.fell()) {
    lastButtonPress = millis();
    buttonHeld = false;
    return ButtonPress::NONE;
  }
  if (button.read() == LOW && lastButtonPress && !buttonHeld) {
    uint32_t holdTime = millis() - lastButtonPress;
    if (holdTime >= VERY_LONG_PRESS_MS) {
      buttonHeld = true;
      return ButtonPress::VERY_LONG;
    } else if (holdTime >= LONG_PRESS_MS) {
      buttonHeld = true;
      return ButtonPress::LONG;
    }
  }
  if (button.rose()) {
    if (!buttonHeld && lastButtonPress) {
      uint32_t pressTime = millis() - lastButtonPress;
      lastButtonPress = 0;
      buttonHeld = false;
      if (pressTime < SHORT_PRESS_MAX_MS) return ButtonPress::SHORT;
    }
    lastButtonPress = 0;
    buttonHeld = false;
  }
  return ButtonPress::NONE;
}

void GameManager::handleButtonPress(ButtonPress press) {
  switch(press) {
    case ButtonPress::SHORT:
      if (currentPhase == Phase::LOBBY && gameClientCount >= MIN_CLIENTS_TO_START) {
        currentPhase = Phase::READY;
        gameLocked = true;
        publishGameState();
      } else if (currentPhase == Phase::READY) {
        startQuestion();
      } else if (currentPhase == Phase::ANSWER) {
        nextClient();
      }
      break;
    case ButtonPress::LONG:
      if (currentPhase == Phase::ANSWER) correctAnswer();
      else resetGame();
      break;
    case ButtonPress::VERY_LONG:
      gameLocked = false;
      publishGameState();
      break;
    default:
      break;
  }
}

void GameManager::handlePhase() {
  if (!ledController) return;

  switch(currentPhase) {
    case Phase::BOOT: {
      static uint32_t bootStart = millis();
      static bool bootClearDone = false;
      if (!bootClearDone && millis() - bootStart > 3500) {
        ledController->clearAllLEDs();
        bootClearDone = true;
      }
      if (millis() - bootStart > 15000) {
        currentPhase = Phase::LOBBY;
        ledController->clearAllLEDs();
        Serial.println("=== PHASE: LOBBY ===");
        publishGameState();
        bootClearDone = false;
      }
      break;
    }
    case Phase::LOBBY:
      ledController->animateLobby();
      break;
    case Phase::READY:
      ledController->animateReadyPingPong();
      break;
    case Phase::OPEN:
      ledController->animateOpen();
      break;
    case Phase::ANSWER:
      ledController->updateServerLEDs();
      break;
    case Phase::RESET: {
      static uint32_t lastUpdate = 0;
      static uint16_t animStep = 0;
      if (millis() - lastUpdate > 50) {
        for (uint16_t i = 0; i < LED_COUNT; i++) {
          uint8_t hue = (animStep + i * (256 / LED_COUNT)) & 255;
          uint8_t sector = hue / 43;
          uint8_t remainder = (hue - (sector * 43)) * 6;
          uint8_t p = 0, q = 255 - remainder, t = remainder;
          Rgb rainbowColor;
          switch (sector) {
            case 0: rainbowColor = Rgb(255, t, p); break;
            case 1: rainbowColor = Rgb(q, 255, p); break;
            case 2: rainbowColor = Rgb(p, 255, t); break;
            case 3: rainbowColor = Rgb(p, q, 255); break;
            case 4: rainbowColor = Rgb(t, p, 255); break;
            default: rainbowColor = Rgb(255, p, q); break;
          }
          ledController->setPixelColor(i, rainbowColor);
        }
        ledController->showLEDs();
        animStep += 5;
        lastUpdate = millis();
      }
      // RESET is the finished-question state. Keep it until the operator presses NEXT.
      break;
    }
    default:
      ledController->clearAllLEDs();
      break;
  }
}

void GameManager::resetGame() {
  currentPhase = Phase::LOBBY;
  gameLocked = false;
  memset(buzzQueue, 0, sizeof(buzzQueue));
  queueLength = 0;
  activeClientIndex = -1;
  for (uint8_t i = 0; i < gameClientCount; i++) gameClients[i].buzzed = false;
  if (ledController) ledController->clearAllLEDs();

  // Full reset: restore every connected buzzer to its idle colour.
  StaticJsonDocument<100> doc;
  doc[JsonKey::CMD] = Command::RESET;
  String message;
  serializeJson(doc, message);
  mqttBroker.publish(Topic::CMD, message.c_str());

  Serial.println("=== RESET to LOBBY ===");
  publishBuzzQueue();
  publishGameState();
}

void GameManager::startQuestion() {
  // NEXT always starts a clean question: all exclusions from the previous question are cleared here.
  memset(buzzQueue, 0, sizeof(buzzQueue));
  queueLength = 0;
  activeClientIndex = -1;
  for (uint8_t i = 0; i < gameClientCount; i++) gameClients[i].buzzed = false;

  StaticJsonDocument<100> doc;
  doc[JsonKey::CMD] = Command::RESET;
  String message;
  serializeJson(doc, message);
  mqttBroker.publish(Topic::CMD, message.c_str());

  currentPhase = Phase::OPEN;
  Serial.println("=== PHASE: OPEN (new question, all buzzers rearmed) ===");
  publishBuzzQueue();
  publishGameState();
}

void GameManager::nextClient() {
  if (activeClientIndex < 0 || activeClientIndex >= queueLength) return;

  String wrongClientId = buzzQueue[activeClientIndex];

  // Wrong-answer feedback only. IMPORTANT: do not RESET this client and do not clear buzzed.
  // gameClients[].buzzed therefore acts as the per-question exclusion flag.
  StaticJsonDocument<200> doc;
  doc[JsonKey::CMD] = Command::WRONG_FLASH;
  doc[JsonKey::TARGET] = wrongClientId;
  String message;
  serializeJson(doc, message);
  mqttBroker.publish(Topic::CMD, message.c_str());
  Serial.printf("Sent WRONG_FLASH to %s - locked until next question\n", wrongClientId.c_str());

  // Remove the answered client from the active queue. They remain buzzed=true, so any further buzz is ignored.
  for (uint8_t i = activeClientIndex; i + 1 < queueLength; i++) buzzQueue[i] = buzzQueue[i + 1];
  if (queueLength > 0) {
    buzzQueue[queueLength - 1] = "";
    queueLength--;
  }

  if (activeClientIndex < queueLength) {
    String nextClientId = buzzQueue[activeClientIndex];
    StaticJsonDocument<200> nextDoc;
    nextDoc[JsonKey::CMD] = Command::ANIM_ACTIVE;
    nextDoc[JsonKey::TARGET] = nextClientId;
    String nextMessage;
    serializeJson(nextDoc, nextMessage);
    mqttBroker.publish(Topic::CMD, nextMessage.c_str());
    Serial.printf("Next queued client active: %s\n", nextClientId.c_str());
    if (ledController) ledController->updateServerLEDs();
    publishBuzzQueue();
  } else {
    // Nobody is waiting. Re-open buzzing for everyone except clients already marked buzzed.
    activeClientIndex = -1;
    currentPhase = Phase::OPEN;
    Serial.println("=== Queue empty - question remains OPEN; wrong clients stay excluded ===");
    publishBuzzQueue();
    publishGameState();
  }
}

void GameManager::correctAnswer() {
  if (activeClientIndex >= 0 && activeClientIndex < queueLength) {
    String activeClientId = buzzQueue[activeClientIndex];
    StaticJsonDocument<200> doc;
    doc[JsonKey::CMD] = Command::CELEBRATE;
    doc[JsonKey::TARGET] = activeClientId;
    String message;
    serializeJson(doc, message);
    mqttBroker.publish(Topic::CMD, message.c_str());
    Serial.printf("Sent celebrate command to %s\n", activeClientId.c_str());
  }

  // Correct answer closes the question. Do NOT auto-start or auto-ready the next question.
  currentPhase = Phase::RESET;
  celebrationStart = millis();
  Serial.println("=== CORRECT ANSWER - question finished, waiting for NEXT ===");
  publishGameState();
}

void GameManager::resetToReady() {
  // Legacy helper: keep a closed question waiting for the explicit NEXT button.
  currentPhase = Phase::RESET;
  publishGameState();
}

void GameManager::sendPingToAllClients() {
  if (millis() - lastPingTime < PING_INTERVAL_MS) return;
  lastPingTime = millis();
  StaticJsonDocument<100> doc;
  doc[JsonKey::CMD] = "PING_REQUEST";
  doc[JsonKey::TIMESTAMP] = millis();
  String message;
  serializeJson(doc, message);
  mqttBroker.publish(Topic::CMD, message.c_str());
  Serial.printf("Sent ping to all clients (count: %d)\n", gameClientCount);
}

void GameManager::publishGameState() {
  StaticJsonDocument<200> doc;
  doc[JsonKey::PHASE] = phaseToString(currentPhase);
  doc[JsonKey::LOCKED] = gameLocked;
  doc["gameClientCount"] = gameClientCount;
  String message;
  serializeJson(doc, message);
  mqttBroker.publish(Topic::STATE, message.c_str(), true);
  Serial.printf("Published game state: %s\n", message.c_str());
}

void GameManager::publishBuzzQueue() {
  StaticJsonDocument<300> doc;
  JsonArray order = doc.createNestedArray(JsonKey::ORDER);
  for (uint8_t i = 0; i < queueLength; i++) order.add(buzzQueue[i]);
  if (activeClientIndex >= 0 && activeClientIndex < queueLength) doc[JsonKey::ACTIVE] = buzzQueue[activeClientIndex];
  String message;
  serializeJson(doc, message);
  mqttBroker.publish(Topic::QUEUE, message.c_str());
  Serial.printf("Published buzz queue: %s\n", message.c_str());
}
