#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <StreamString.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

#define API_KEY "" // TODO: Change to your sinric API Key. Your API Key is displayed on sinric.com dashboard
#define SSID_NAME "" // TODO: Change to your Wifi network SSID
#define WIFI_PASSWORD "" // TODO: Change to your Wifi network password
#define SERVER_URL "iot.sinric.com"
#define SERVER_PORT 80

#define TV_API ""  // TV API
#define AC_API ""  // AC API
#define TEST_FAN_API "" // some switch api used to test code

#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 
#define DELAY 100
int kIrLed = D2; // IR Pin

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);

IRsend irsend(kIrLed);

int DEFAULT_VOL=2;

// These are the IR codes I used

int tvPower=0x20DF10EF; // POWER ON OFF NEC
int tvVolDown=0x20DFC03F; // SHARP TV VOL DOWN NEC
int tvVolUp=0x20DF40BF; // SHARP TV VOL UP NEC
int tvMute=0x20DF906F; // SHARP TV MUTE NEC
int tvPlay=0x20DF42BD; // SHARP PLAY NEC
int tvPause=0x20DF827D; // SHARP PAUSE NEC
int tvBack=0x20DF20DF; // SHARP BACK NEC
int tvOk=0x20DF5AA5; // SHARP OK NEC

int acPower=0x10AF8877; // POWER ON OFF NEC
int acTempUp=0x10AF708F; // AC TEMP UP NEC
int acTempDown=0x10AFB04F; // AC TEMP DOWN NEC
int acTimer=0x10AF609F; // AC TIMER BUTTON NEC
int acCool=0x10AF906F; // AC COOL BUTTON NEC
int acEcon=0x10AF40BF; // AC ECON BUTTON NEC

int testfanPower=0xD81; // FAN ON OFF 0xD81 sendSymphony


void setup() {
  irsend.begin();
  Serial.begin(115200);
  
  WiFiMulti.addAP(SSID_NAME, WIFI_PASSWORD);
  Serial.println();
  Serial.print("Connecting to Wifi: ");
  Serial.println(SSID_NAME);  

  // Waiting for Wifi connect
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if(WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // server address, port and URL
  webSocket.begin(SERVER_URL, SERVER_PORT, "/");

  // event handler
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", API_KEY);
  
  // try again every 5000ms if connection has failed
  webSocket.setReconnectInterval(5000);   // If you see 'class WebSocketsClient' has no member named 'setReconnectInterval' error update arduinoWebSockets
}

void loop() {
  webSocket.loop();
  
  if(isConnected) {
      uint64_t now = millis();
      
      // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
      if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
          heartbeatTimestamp = now;
          webSocket.sendTXT("H");          
      }
  }   
}

// deviceId is the ID assgined to your smart-home-device in sinric.com dashboard. Copy it from dashboard and paste it here

void adjustLoop(int adjustCount, int irCode, int extra) {
  for (int i=0; i < abs(adjustCount) + extra; i++)
       irsend.sendNEC(irCode);
  delay(DELAY);
}
void adjustIfElse(int adjustCount, int irUP, int irDOWN, int extra=0) {
 if (adjustCount > 0)
      adjustLoop(adjustCount, irUP, extra);
  else if (adjustCount < 0) 
      adjustLoop(adjustCount, irDOWN, extra);
}

void adjustVolume(int adjustVolume, String deviceId) {
   Serial.print("adjustVolume: ");
   Serial.println(adjustVolume);
   Serial.println(deviceId);
   
  if (deviceId == TV_API) {
    adjustIfElse(adjustVolume, tvVolUp, tvVolDown, 1);
  }
}

void Play(String deviceId) {
  Serial.print("Play: ");
  Serial.println(deviceId);
   
  if (deviceId == TV_API) {
     irsend.sendNEC(tvPlay);
  }
}

void Pause(String deviceId) {
  Serial.print("Pause: ");
  Serial.println(deviceId);
   
  if (deviceId == TV_API) {
     irsend.sendNEC(tvPause);
  }
}

void SkipAd(String deviceId) {
  Serial.print("SkipAd: ");
  Serial.println(deviceId);
   
  if (deviceId == TV_API) {
     irsend.sendNEC(tvOk);
  }
}


void adjustTemp(int adjustTemp, String deviceId) {
  Serial.print("adjustTemp: ");
  Serial.println(adjustTemp);
  Serial.println(deviceId);

  irsend.sendNEC(acCool);
  delay(DELAY);
  irsend.sendNEC(acEcon);
  delay(DELAY);
   
  if (deviceId == AC_API) {
    adjustIfElse(adjustTemp, acTempUp, acTempDown);
  }
}

void setACMode(String modeVal, String deviceId) {
  if (deviceId == AC_API) {
    if (modeVal == "COOL") {
      Serial.println("Setting AC to COOL");
      irsend.sendNEC(acCool);
    }
    else if (modeVal == "ECO") {
      Serial.println("Setting AC to ECO");
      irsend.sendNEC(acEcon);
    }
  }
}

void setMute(String deviceId) {
  Serial.print("mute: ");
  Serial.println(deviceId);
  if (deviceId == TV_API) {
      irsend.sendNEC(tvMute);
  }
}

void turnOnOff(String deviceId) {
  Serial.print("Turn on/off device id: ");
  Serial.println(deviceId);
    
  if (deviceId == TV_API) // Device ID of first device
  {  
    Serial.println("Sharp TV");
    irsend.sendNEC(tvBack);
    delay(1000);
    irsend.sendNEC(tvPower);
  }
  else if (deviceId == AC_API) {
    Serial.println("Living Room AC");
    irsend.sendNEC(acPower);
  } 
  else if (deviceId == TEST_FAN_API) {
    Serial.println("test fan");
    irsend.sendSymphony(testfanPower);
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      isConnected = false;    
      Serial.printf("[webSocketEvent] Webservice disconnected from server!\n");
      break;
    case WStype_CONNECTED: {
      isConnected = true;
      Serial.printf("[webSocketEvent] Service connected to server at url: %s\n", payload);
      Serial.printf("[webSocketEvent] Waiting for commands from server ...\n");        
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[webSocketEvent] get text: %s\n", payload);
#if ARDUINOJSON_VERSION_MAJOR == 5
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
        DynamicJsonDocument json(1024);
        deserializeJson(json, (char*) payload);      
#endif        
        String deviceId = json ["deviceId"];     
        String action = json ["action"];

        
        if (action == "setPowerState") {
            // alexa, turn on tv ==> {"deviceId":"xx","action":"setPowerState","value":"ON"}
            turnOnOff(deviceId);
            
        } else if (action == "SetMute") { 
            // alexa, mute tv ==> {"deviceId":"xxx","action":"SetMute","value":{"mute":true}}
            setMute(deviceId);
            
        } else if (action == "AdjustVolume") { 
            // alexa, turn the volume down on tv by 20 ==> {"deviceId":"xxx","action":"AdjustVolume","value":{"volume":-20,"volumeDefault":false}}
            // alexa, lower the volume on tv ==> {"deviceId":"xx","action":"AdjustVolume","value":{"volume":-10,"volumeDefault":true}}
            
            int pre_volume = String(json ["value"]["volume"]).toInt();
            //int pre_volume = kcvolume.toInt(); 
            int volume;
            
            if (json ["value"]["volumeDefault"] == true) {
              volume = DEFAULT_VOL * (pre_volume / abs(pre_volume)); // set default value to DEFAULT_VOL
            }
            else {
              volume = pre_volume;
            }
            adjustVolume(volume, deviceId);
        } else if (action == "Play") {
            Play(deviceId);
        } else if (action == "Pause") {
            Pause(deviceId);
        } else if (action == "Next") {
            SkipAd(deviceId);
        } else if(action == "AdjustTargetTemperature") {
            //Alexa, make it warmer in here
            //Alexa, make it cooler in here
            //String scale = json["value"]["targetSetpointDelta"]["scale"]; 
            int value = String(json["value"]["targetSetpointDelta"]["value"]).toInt(); 
  
            Serial.println("[WSc] AdjustTargetTemperature value: ");
            Serial.println(value);
            //Serial.println("[WSc] AdjustTargetTemperature scale: " + scale);  
            
            //int value = pre_value.toInt();
             adjustTemp(value, deviceId);
                
        } else if(action == "SetThermostatMode") { 
            //Alexa, set thermostat name to mode
            //Alexa, set thermostat to automatic
            //Alexa, set kitchen to off
            String modeVal = String(json["value"]["thermostatMode"]["value"]);
            
            Serial.println("[WSc] SetThermostatMode value: " + modeVal);
            setACMode(modeVal, deviceId);
            
        } else if (action == "test") {
            Serial.println("[WSc] received test command from sinric.com");
        }
      }
      break;
      delay(DELAY);
    case WStype_BIN:
      Serial.printf("[webSocketEvent] get binary length: %u\n", length);
      break;
    default: break;
  }
}
 
