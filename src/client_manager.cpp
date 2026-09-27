#include "client_manager.h"
#include "client_led_controller.h"
#include "client_mqtt.h"

ClientManager* clientManager = nullptr;
ClientButtonHandler* clientButtonHandler = nullptr;

ClientManager::ClientManager() {
  data.currentState = ClientState::DISCONNECTED;
  data.slot = 0;
  data.assignedColor = Rgb(255, 0, 0);
  data.hasBuzzed = false;
  data.isActive = false;
  if (clientMqtt) data.id = clientMqtt->getClientId();
}

void ClientManager::setState(ClientState newState) {
  if (data.currentState != newState) {
    Serial.printf("State change: %d -> %d\n", (int)data.currentState, (int)newState);
    data.currentState = newState;
  }
}

ClientState ClientManager::getState() const { return data.currentState; }

void ClientManager::handleStateAnimations() {
  if (!clientLedController) return;
  extern bool gameIsOpen;

  switch (data.currentState) {
    case ClientState::DISCONNECTED:
      clientLedController->animateDisconnected();
      break;
    case ClientState::ASSIGNED: {
      clientLedController->setAllLEDs(data.assignedColor);
      static uint32_t assignmentStart = millis();
      if (millis() - assignmentStart > 2000) {
        setState(ClientState::IDLE);
        assignmentStart = millis() + 60000;
      }
      break;
    }
    case ClientState::IDLE:
      if (isAssigned()) {
        if (gameIsOpen) clientLedController->animateIdle(data.assignedColor);
        else clientLedController->showSolidColor(data.assignedColor);
      } else clientLedController->animateDisconnected();
      break;
    case ClientState::LOCKED_AFTER_BUZZ:
      if (isAssigned()) clientLedController->showLocked(data.assignedColor);
      else clientLedController->animateDisconnected();
      break;
    case ClientState::ACTIVE_TURN:
      if (isAssigned()) clientLedController->animateActiveSpin(data.assignedColor);
      break;
    case ClientState::CELEBRATE: {
      clientLedController->animateCelebration();
      static uint32_t celebrationStart = 0;
      if (!celebrationStart) celebrationStart = millis();
      if (millis() - celebrationStart > CELEBRATION_DURATION_MS) {
        // Keep the winner locked until NEXT sends RESET for the new question.
        setState(ClientState::LOCKED_AFTER_BUZZ);
        celebrationStart = 0;
      }
      break;
    }
    case ClientState::WRONG_FLASH: {
      clientLedController->animateFlash(COLOR_ERROR);
      static uint32_t flashStart = 0;
      if (!flashStart) flashStart = millis();
      if (millis() - flashStart > FLASH_DURATION_MS * 4) {
        // A wrong answer remains excluded for the current question.
        // Do not clear hasBuzzed here. NEXT/RESET is the only re-arm path.
        setState(ClientState::LOCKED_AFTER_BUZZ);
        flashStart = 0;
        Serial.println("WRONG_FLASH complete - locked until next question");
      }
      break;
    }
    default:
      clientLedController->clearAllLEDs();
      break;
  }
}

void ClientManager::setAssignment(uint8_t slot, const Rgb& color) {
  data.slot = slot; data.assignedColor = color; setState(ClientState::ASSIGNED);
  Serial.printf("Assigned slot %d, color R:%d G:%d B:%d\n",slot,color.r,color.g,color.b);
}

bool ClientManager::isAssigned() const { return data.slot > 0; }

void ClientManager::buzz() {
  if (!canBuzz()) return;
  data.hasBuzzed = true; setState(ClientState::LOCKED_AFTER_BUZZ);
  if (clientMqtt && clientMqtt->isConnected()) clientMqtt->sendBuzz();
  Serial.println("BUZZED!");
}

bool ClientManager::canBuzz() const {
  return isAssigned() && !data.hasBuzzed && data.currentState == ClientState::IDLE && clientMqtt && clientMqtt->isConnected();
}

void ClientManager::resetBuzzState() { data.hasBuzzed=false; data.isActive=false; }
const ClientManager::ClientData& ClientManager::getData() const { return data; }
const String& ClientManager::getClientId() const { return data.id; }
const Rgb& ClientManager::getAssignedColor() const { return data.assignedColor; }

ClientButtonHandler::ClientButtonHandler(Bounce& btn) : button(btn), lastButtonPress(0), buttonPressed(false) {}
void ClientButtonHandler::update() { button.update(); if(button.fell()&&!buttonPressed){buttonPressed=true;lastButtonPress=millis();} }
bool ClientButtonHandler::wasPressed() { if(buttonPressed){buttonPressed=false;return true;} return false; }
