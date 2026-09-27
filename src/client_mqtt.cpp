#include "client_mqtt.h"
#include "client_manager.h"
#include "client_led_controller.h"

// Global instance
ClientMQTT* clientMqtt = nullptr;
bool gameIsOpen = false; // Track if game is in OPEN state

namespace {
uint16_t readBatteryMillivolts() {
  uint32_t totalMv = 0;

  for (uint8_t i = 0; i < BATTERY_ADC_SAMPLES; ++i) {
    totalMv += analogReadMilliVolts(BATTERY_ADC_PIN);
  }

  const uint32_t adcMv = totalMv / BATTERY_ADC_SAMPLES;
  return static_cast<uint16_t>(adcMv * BATTERY_DIVIDER_RATIO);
}
}

ClientMQTT::ClientMQTT() : mqttClient(wifiClient), connected(false), lastConnectionAttempt(0), lastPing(0) {
  uint64_t mac = ESP.getEfuseMac();
  clientId = "C-" + String((uint32_t)(mac >> 16), HEX);
}

void ClientMQTT::begin() {
  Serial.printf("Client ID: %s\n", clientId.c_str());
  pinMode(BATTERY_ADC_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) { this->onMessage(topic, payload, length); });
  connectWiFi();
}

void ClientMQTT::loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      if (millis() - lastConnectionAttempt > 5000) { connectMQTT(); lastConnectionAttempt = millis(); }
    } else {
      mqttClient.loop();
      if (millis() - lastPing > PING_INTERVAL_MS) { sendPing(); lastPing = millis(); }
    }
  } else if (millis() - lastConnectionAttempt > 10000) {
    connectWiFi(); lastConnectionAttempt = millis();
  }
}

bool ClientMQTT::isConnected() { return WiFi.status() == WL_CONNECTED && mqttClient.connected(); }

bool ClientMQTT::connectWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PSK);
  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) { delay(100); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) { Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str()); return true; }
  Serial.println("\nWiFi connection failed!"); return false;
}

void ClientMQTT::disconnectWiFi() { WiFi.disconnect(); }

bool ClientMQTT::connectMQTT() {
  Serial.printf("Connecting to MQTT broker: %s:%d\n", MQTT_HOST, MQTT_PORT);
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("MQTT connected!");
    String assignTopic = String(Topic::ASSIGN) + clientId;
    mqttClient.subscribe(assignTopic.c_str()); mqttClient.subscribe(Topic::STATE); mqttClient.subscribe(Topic::QUEUE); mqttClient.subscribe(Topic::CMD);
    sendJoinRequest(); return true;
  }
  Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state()); return false;
}

void ClientMQTT::disconnectMQTT() { mqttClient.disconnect(); }

void ClientMQTT::onMessage(char* topic, byte* payload, unsigned int length) {
  String payloadStr = ""; for (unsigned int i=0;i<length;i++) payloadStr += (char)payload[i];
  String topicStr = String(topic);
  Serial.printf("MQTT received - Topic: %s, Payload: %s\n", topicStr.c_str(), payloadStr.c_str());
  if (topicStr.startsWith(Topic::ASSIGN)) handleAssignment(payloadStr);
  else if (topicStr == Topic::STATE) handleGameState(payloadStr);
  else if (topicStr == Topic::QUEUE) handleQueue(payloadStr);
  else if (topicStr == Topic::CMD) handleCommand(payloadStr);
}

void ClientMQTT::sendJoinRequest() {
  StaticJsonDocument<200> doc; doc[JsonKey::ID]=clientId; doc[JsonKey::CAPABILITY]=LED_COUNT; doc[JsonKey::FIRMWARE]="1.0";
  String message; serializeJson(doc,message); mqttClient.publish(Topic::JOIN,message.c_str()); Serial.printf("Sent join request: %s\n",message.c_str());
}

void ClientMQTT::sendBuzz() {
  if(!isConnected())return; StaticJsonDocument<200> doc; doc[JsonKey::ID]=clientId; doc[JsonKey::TIMESTAMP]=millis();
  String message;serializeJson(doc,message);mqttClient.publish(Topic::BUZZ,message.c_str());Serial.printf("Sent buzz: %s\n",message.c_str());
}

void ClientMQTT::sendPing() {
  if(!isConnected())return; const uint16_t batteryMv=readBatteryMillivolts(); StaticJsonDocument<128> doc;
  doc[JsonKey::ID]=clientId;doc[JsonKey::BATTERY_MV]=batteryMv;String message;serializeJson(doc,message);
  mqttClient.publish(Topic::PING,message.c_str());Serial.printf("Sent heartbeat: %s\n",message.c_str());
}

const String& ClientMQTT::getClientId() const { return clientId; }

void handleAssignment(const String& payload) {
  StaticJsonDocument<200> doc; DeserializationError error=deserializeJson(doc,payload);
  if(error){Serial.printf("Assignment JSON parse error: %s\n",error.c_str());return;}
  uint8_t slot=doc[JsonKey::SLOT];String colorHex=doc[JsonKey::COLOR];
  if(colorHex.length()==7&&colorHex[0]=='#'){
    uint32_t colorValue=strtol(colorHex.substring(1).c_str(),NULL,16);
    Rgb color((colorValue>>16)&0xFF,(colorValue>>8)&0xFF,colorValue&0xFF);
    if(clientManager)clientManager->setAssignment(slot,color);
    Serial.printf("Assignment received: slot %d, color %s\n",slot,colorHex.c_str());
  }
}

void handleGameState(const String& payload) {
  StaticJsonDocument<200> doc;DeserializationError error=deserializeJson(doc,payload);if(error)return;
  String phaseStr=doc[JsonKey::PHASE];bool locked=doc[JsonKey::LOCKED];
  Serial.printf("Game state: %s, locked: %s\n",phaseStr.c_str(),locked?"true":"false");
  Phase phase=stringToPhase(phaseStr.c_str());gameIsOpen=(phase==Phase::OPEN||phase==Phase::ANSWER);
  if(phase==Phase::OPEN&&clientManager&&clientManager->canBuzz()){
    if(clientManager->getState()==ClientState::IDLE){}
  }else if(phase==Phase::READY){
    if(clientManager){clientManager->resetBuzzState();clientManager->setState(ClientState::IDLE);}
  }
}

void handleQueue(const String& payload) {
  StaticJsonDocument<300> doc;DeserializationError error=deserializeJson(doc,payload);if(error)return;
  String activeClient=doc[JsonKey::ACTIVE];
  if(clientManager&&activeClient==clientMqtt->getClientId()){clientManager->setState(ClientState::ACTIVE_TURN);Serial.println("I'm now active!");}
}

void handleCommand(const String& payload) {
  StaticJsonDocument<200> doc;
  DeserializationError error=deserializeJson(doc,payload);
  if(error){Serial.printf("CMD JSON parse error: %s | payload='%s'\n",error.c_str(),payload.c_str());return;}

  String cmd=doc[JsonKey::CMD] | "";
  String target=doc[JsonKey::TARGET] | "";
  Serial.printf("CMD parsed: '%s' len=%u target='%s'\n",cmd.c_str(),(unsigned)cmd.length(),target.c_str());

  if(target.length()>0&&target!=clientMqtt->getClientId()){
    Serial.printf("CMD ignored: target '%s' is not this client '%s'\n",target.c_str(),clientMqtt->getClientId().c_str());
    return;
  }

  if(!clientManager){Serial.println("CMD ignored: clientManager is null");return;}

  if(cmd==Command::CELEBRATE) clientManager->setState(ClientState::CELEBRATE);
  else if(cmd==Command::WRONG_FLASH) clientManager->setState(ClientState::WRONG_FLASH);
  else if(cmd==Command::ANIM_ACTIVE) clientManager->setState(ClientState::ACTIVE_TURN);
  else if(cmd==Command::LIGHT_WHITE) clientManager->setState(ClientState::LOCKED_AFTER_BUZZ);
  else if(cmd==Command::IDLE_COLOR) clientManager->setState(ClientState::IDLE);
  else if(cmd==Command::RESET){
    const auto& before=clientManager->getData();
    Serial.printf("RESET begin: state=%d buzzed=%s active=%s\n",(int)before.currentState,before.hasBuzzed?"yes":"no",before.isActive?"yes":"no");
    clientManager->resetBuzzState();
    clientManager->setState(ClientState::IDLE);
    const auto& after=clientManager->getData();
    Serial.printf("RESET done: state=%d buzzed=%s active=%s gameIsOpen=%s\n",(int)after.currentState,after.hasBuzzed?"yes":"no",after.isActive?"yes":"no",gameIsOpen?"true":"false");
  }else if(cmd=="PING_REQUEST"){
    if(clientMqtt&&clientMqtt->isConnected()){clientMqtt->sendPing();Serial.println("Responded to server ping");}
  }else Serial.printf("CMD unknown: '%s'\n",cmd.c_str());

  Serial.printf("Command handled: %s\n",cmd.c_str());
}
