#include "client_mqtt.h"
#include "client_manager.h"
#include "client_led_controller.h"

ClientMQTT* clientMqtt=nullptr;
bool gameIsOpen=false;
bool clientTestMode=false;

namespace { uint16_t readBatteryMillivolts(){uint32_t totalMv=0;for(uint8_t i=0;i<BATTERY_ADC_SAMPLES;++i)totalMv+=analogReadMilliVolts(BATTERY_ADC_PIN);return static_cast<uint16_t>((totalMv/BATTERY_ADC_SAMPLES)*BATTERY_DIVIDER_RATIO);} }

ClientMQTT::ClientMQTT():mqttClient(wifiClient),connected(false),lastConnectionAttempt(0),lastPing(0){uint64_t mac=ESP.getEfuseMac();clientId="C-"+String((uint32_t)(mac>>16),HEX);}
void ClientMQTT::begin(){Serial.printf("Client ID: %s\n",clientId.c_str());pinMode(BATTERY_ADC_PIN,INPUT);analogSetPinAttenuation(BATTERY_ADC_PIN,ADC_11db);mqttClient.setServer(MQTT_HOST,MQTT_PORT);mqttClient.setCallback([this](char* t,byte* p,unsigned int l){this->onMessage(t,p,l);});connectWiFi();}
void ClientMQTT::loop(){if(WiFi.status()==WL_CONNECTED){if(!mqttClient.connected()){if(millis()-lastConnectionAttempt>5000){connectMQTT();lastConnectionAttempt=millis();}}else{mqttClient.loop();if(millis()-lastPing>PING_INTERVAL_MS){sendPing();lastPing=millis();}}}else if(millis()-lastConnectionAttempt>10000){connectWiFi();lastConnectionAttempt=millis();}}
bool ClientMQTT::isConnected(){return WiFi.status()==WL_CONNECTED&&mqttClient.connected();}
bool ClientMQTT::connectWiFi(){Serial.printf("Connecting to WiFi: %s\n",WIFI_SSID);WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PSK);uint32_t s=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-s<10000){delay(100);Serial.print(".");}if(WiFi.status()==WL_CONNECTED){Serial.printf("\nWiFi connected! IP: %s\n",WiFi.localIP().toString().c_str());return true;}Serial.println("\nWiFi connection failed!");return false;}
void ClientMQTT::disconnectWiFi(){WiFi.disconnect();}
bool ClientMQTT::connectMQTT(){Serial.printf("Connecting to MQTT broker: %s:%d\n",MQTT_HOST,MQTT_PORT);if(mqttClient.connect(clientId.c_str())){Serial.println("MQTT connected!");String a=String(Topic::ASSIGN)+clientId;mqttClient.subscribe(a.c_str());mqttClient.subscribe(Topic::STATE);mqttClient.subscribe(Topic::QUEUE);mqttClient.subscribe(Topic::CMD);sendJoinRequest();return true;}Serial.printf("MQTT connection failed, rc=%d\n",mqttClient.state());return false;}
void ClientMQTT::disconnectMQTT(){mqttClient.disconnect();}
void ClientMQTT::onMessage(char* topic,byte* payload,unsigned int length){String p="";for(unsigned int i=0;i<length;i++)p+=(char)payload[i];String t=String(topic);Serial.printf("MQTT received - Topic: %s, Payload: %s\n",t.c_str(),p.c_str());if(t.startsWith(Topic::ASSIGN))handleAssignment(p);else if(t==Topic::STATE)handleGameState(p);else if(t==Topic::QUEUE)handleQueue(p);else if(t==Topic::CMD)handleCommand(p);}
void ClientMQTT::sendJoinRequest(){StaticJsonDocument<200>d;d[JsonKey::ID]=clientId;d[JsonKey::CAPABILITY]=LED_COUNT;d[JsonKey::FIRMWARE]="1.0";String m;serializeJson(d,m);mqttClient.publish(Topic::JOIN,m.c_str());Serial.printf("Sent join request: %s\n",m.c_str());}
void ClientMQTT::sendBuzz(){if(!isConnected())return;StaticJsonDocument<200>d;d[JsonKey::ID]=clientId;d[JsonKey::TIMESTAMP]=millis();String m;serializeJson(d,m);mqttClient.publish(Topic::BUZZ,m.c_str());Serial.printf("Sent buzz: %s%s\n",m.c_str(),clientTestMode?" [TEST]":"");}
void ClientMQTT::sendPing(){if(!isConnected())return;StaticJsonDocument<128>d;d[JsonKey::ID]=clientId;d[JsonKey::BATTERY_MV]=readBatteryMillivolts();String m;serializeJson(d,m);mqttClient.publish(Topic::PING,m.c_str());Serial.printf("Sent heartbeat: %s\n",m.c_str());}
const String& ClientMQTT::getClientId()const{return clientId;}

void handleAssignment(const String& p){StaticJsonDocument<200>d;if(deserializeJson(d,p))return;uint8_t slot=d[JsonKey::SLOT];String h=d[JsonKey::COLOR];if(h.length()==7&&h[0]=='#'){uint32_t v=strtol(h.substring(1).c_str(),NULL,16);Rgb c((v>>16)&255,(v>>8)&255,v&255);if(clientManager)clientManager->setAssignment(slot,c);Serial.printf("Assignment received: slot %d, color %s\n",slot,h.c_str());}}
void handleGameState(const String& p){StaticJsonDocument<200>d;if(deserializeJson(d,p))return;String s=d[JsonKey::PHASE];bool locked=d[JsonKey::LOCKED];Serial.printf("Game state: %s, locked: %s\n",s.c_str(),locked?"true":"false");Phase ph=stringToPhase(s.c_str());gameIsOpen=(ph==Phase::OPEN||ph==Phase::ANSWER);if(ph==Phase::READY&&clientManager&&!clientTestMode){clientManager->resetBuzzState();clientManager->setState(ClientState::IDLE);}}
void handleQueue(const String& p){if(clientTestMode)return;StaticJsonDocument<300>d;if(deserializeJson(d,p))return;String a=d[JsonKey::ACTIVE];if(clientManager&&a==clientMqtt->getClientId()){clientManager->setState(ClientState::ACTIVE_TURN);Serial.println("I'm now active!");}}
void handleCommand(const String& p){StaticJsonDocument<200>d;DeserializationError e=deserializeJson(d,p);if(e){Serial.printf("CMD JSON parse error: %s | payload='%s'\n",e.c_str(),p.c_str());return;}String cmd=d[JsonKey::CMD]|"";String target=d[JsonKey::TARGET]|"";Serial.printf("CMD parsed: '%s' len=%u target='%s'\n",cmd.c_str(),(unsigned)cmd.length(),target.c_str());if(target.length()>0&&target!=clientMqtt->getClientId())return;if(!clientManager)return;
if(cmd==Command::TEST_MODE_ON){clientTestMode=true;clientManager->resetBuzzState();clientManager->setState(ClientState::IDLE);Serial.println("TEST MODE ON - repeated buzz enabled");}
else if(cmd==Command::TEST_MODE_OFF){clientTestMode=false;clientManager->resetBuzzState();clientManager->setState(ClientState::IDLE);Serial.println("TEST MODE OFF - normal lock restored");}
else if(cmd==Command::CELEBRATE)clientManager->setState(ClientState::CELEBRATE);
else if(cmd==Command::WRONG_FLASH)clientManager->setState(ClientState::WRONG_FLASH);
else if(cmd==Command::ANIM_ACTIVE)clientManager->setState(ClientState::ACTIVE_TURN);
else if(cmd==Command::LIGHT_WHITE)clientManager->setState(ClientState::LOCKED_AFTER_BUZZ);
else if(cmd==Command::IDLE_COLOR)clientManager->setState(ClientState::IDLE);
else if(cmd==Command::RESET){const auto& b=clientManager->getData();Serial.printf("RESET begin: state=%d buzzed=%s active=%s\n",(int)b.currentState,b.hasBuzzed?"yes":"no",b.isActive?"yes":"no");clientManager->resetBuzzState();clientManager->setState(ClientState::IDLE);const auto& a=clientManager->getData();Serial.printf("RESET done: state=%d buzzed=%s active=%s gameIsOpen=%s\n",(int)a.currentState,a.hasBuzzed?"yes":"no",a.isActive?"yes":"no",gameIsOpen?"true":"false");}
else if(cmd=="PING_REQUEST"){if(clientMqtt&&clientMqtt->isConnected())clientMqtt->sendPing();}
else Serial.printf("CMD unknown: '%s'\n",cmd.c_str());Serial.printf("Command handled: %s\n",cmd.c_str());}
