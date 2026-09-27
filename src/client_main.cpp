#define CLIENT 1
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include "config.h"
#include "protocol.h"
#include "client_led_controller.h"
#include "client_mqtt.h"
#include "client_manager.h"

Adafruit_NeoPixel strip(LED_COUNT,LED_PIN,NEO_GRB+NEO_KHZ800);
Bounce button=Bounce();

void setup(){
  Serial.begin(115200);Serial.println("ESP32 Quiz-Buzzer Client Starting...");
  Serial.printf("Hardware Config - LED Pin: %d, Button Pin: %d, LED Count: %d\n",LED_PIN,BUTTON_PIN,LED_COUNT);
  strip.begin();strip.setBrightness(LED_BRIGHTNESS);strip.clear();strip.show();clientLedController=new ClientLEDController(strip);
  pinMode(BUTTON_PIN,INPUT_PULLUP);button.attach(BUTTON_PIN);button.interval(DEBOUNCE_MS);clientButtonHandler=new ClientButtonHandler(button);
  clientMqtt=new ClientMQTT();clientManager=new ClientManager();clientLedController->showRGBTest();clientMqtt->begin();
  Serial.println("Client ready!");
}

void loop(){
  if(clientMqtt){
    clientMqtt->loop();
    if(clientManager){
      if(clientMqtt->isConnected()){
        if(clientManager->getState()==ClientState::DISCONNECTED){if(clientManager->isAssigned())clientManager->setState(ClientState::IDLE);else clientManager->setState(ClientState::CONNECTING);}
      }else if(clientManager->getState()!=ClientState::DISCONNECTED)clientManager->setState(ClientState::DISCONNECTED);
    }
  }

  if(clientButtonHandler){
    clientButtonHandler->update();
    if(clientButtonHandler->wasPressed()){
      Serial.println("Button pressed!");
      if(clientManager){
        if(clientTestMode){
          if(clientMqtt&&clientMqtt->isConnected()&&clientManager->isAssigned()){
            // Diagnostic mode deliberately bypasses hasBuzzed/state locking.
            // Every physical press is sent so the same buzzer can be tested repeatedly.
            clientMqtt->sendBuzz();
            Serial.println("TEST buzz sent - client remains immediately reusable");
          }else Serial.println("TEST buzz unavailable - client not assigned/connected");
        }else if(clientManager->canBuzz())clientManager->buzz();
        else{
          Serial.println("Cannot buzz right now");
          Serial.printf("State: %d, Assigned: %s, Buzzed: %s, Connected: %s\n",(int)clientManager->getState(),clientManager->isAssigned()?"yes":"no",clientManager->getData().hasBuzzed?"yes":"no",(clientMqtt&&clientMqtt->isConnected())?"yes":"no");
        }
      }
    }
  }
  if(clientManager)clientManager->handleStateAnimations();
  delay(10);
}
