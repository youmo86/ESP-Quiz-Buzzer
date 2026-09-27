#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "protocol.h"

class ClientMQTT {
private:
  WiFiClient wifiClient;
  PubSubClient mqttClient;
  String clientId;
  bool connected;
  uint32_t lastConnectionAttempt;
  uint32_t lastPing;
public:
  ClientMQTT();
  void begin(); void loop(); bool isConnected();
  bool connectWiFi(); void disconnectWiFi();
  bool connectMQTT(); void disconnectMQTT(); void onMessage(char* topic, byte* payload, unsigned int length);
  void sendJoinRequest(); void sendBuzz(); void sendPing();
  const String& getClientId() const;
};

void handleAssignment(const String& payload);
void handleGameState(const String& payload);
void handleQueue(const String& payload);
void handleCommand(const String& payload);

extern ClientMQTT* clientMqtt;
extern bool clientTestMode;
