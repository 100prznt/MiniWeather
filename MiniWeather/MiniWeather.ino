


#include <FS.h>                    //this needs to be first, or it all crashes and burns...
#include "Arduino.h"
#include <ESP8266WiFi.h>
#include "JsonStreamingParser.h"   // Json Streaming Parser  


#include <ESP8266HTTPClient.h>     // Web Download
#include <ESP8266httpUpdate.h>     // Web Updater

#include <ArduinoJson.h>          // ArduinoJSON                 https://github.com/bblanchon/ArduinoJson

#include <DNSServer.h>            // - Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // - Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          // WifiManager 


#include <NTPClient.h>
#include <time.h>

#include <Arduino.h>

#include <ESPStringTemplate.h>

#include <Wire.h>
#include "cactus_io_BME280_I2C.h"

#include "index.h"






//const long interval = 3000 * 1000;  // alle 60 Minuten prüfen
unsigned long previousMillis = millis() - 2980 * 1000;
unsigned long lastPressed = millis();

WiFiClientSecure client;

ESP8266WebServer server(80);
BME280_I2C bme(0x77);

char time_value[20];


float temperature, humidity, pressure;

int interval = 300;
int modetoggle = 0;

// Variables will change:
int buttonPushCounter = 0;   // counter for the number of button presses
int buttonState = 1;         // current state of the button
int lastButtonState = 1;     // previous state of the button

#define VERSION "1.9rc5"
#define USE_SERIAL Serial
#define TOGGLE_PIN 0 // D3




//define your default values here, if there are different values in config.json, they are overwritten.
char deviceName[40];
char intervalBuffer[5];
char maxModules[5];
char htmlBuffer[4096];

// =======================================================================

//flag for saving data
bool shouldSaveConfig = true;

//callback notifying us of the need to save config
void saveConfigCallback() {
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

void handleRoot() {

    ESPStringTemplate webpage(htmlBuffer, sizeof(htmlBuffer));

    TokenStringPair pair[1];
    pair[0].setPair("%INSTAGRAM%", deviceName);

    webpage.add_P(_PAGE_HEAD);
    webpage.add_P(_PAGE_START);

    webpage.add_P(_PAGE_ACTIONS);

    webpage.add_P(_PAGE_CONFIG_NAME, pair, 1);

    /*switch (mode)
    {
    case 1:
        webpage.add_P(_PAGE_CONFIG_MODE1);     break;

    case 2:
        webpage.add_P(_PAGE_CONFIG_MODE2);       break;

    case 3:

        webpage.add_P(_PAGE_CONFIG_MODE3);       break;

        break;

    default:
        webpage.add_P(_PAGE_CONFIG_MODE1);     break;
    }*/

    TokenStringPair intensityPair[1];


    //intensityPair[0].setPair("%INTENSITY%", matrixIntensity);
    webpage.add_P(_PAGE_CONFIG_INTENSITY, intensityPair, 1);
    webpage.add_P(_PAGE_FOOTER);

    server.send(200, "text/html", htmlBuffer);


}

void redirectBack() {
    server.sendHeader("Location", String("/"), true);
    server.send(302, "text/plain", "");
}

void getConfig() {
    // instagramName
    String deviceNameString = server.arg("devicename");
    deviceNameString.toCharArray(deviceName, 40);

    // mode
    String intervalString = server.arg("interval");
    interval = intervalString.toInt();


    saveConfig();

    redirectBack();
}


void getReset() {
    redirectBack();
    restartX();
}

void getUpdate() {
    redirectBack();
    updateFirmware();
}

void getFormat() {
    redirectBack();
    infoReset();
}

void clearBuffer() {

}


void setup() {

    // Serial debugging
    Serial.begin(115200);



    // Set Reset-Pin to Input Mode
    pinMode(TOGGLE_PIN, INPUT);


    if (SPIFFS.begin()) {

        if (SPIFFS.exists("/config.json")) {
            //file exists, reading and loading

            File configFile = SPIFFS.open("/config.json", "r");
            if (configFile) {
                Serial.println("opened config file");
                size_t size = configFile.size();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf(new char[size]);

                configFile.readBytes(buf.get(), size);
                DynamicJsonDocument json(1024);
                deserializeJson(json, buf.get());
                serializeJson(json, Serial);

                strcpy(deviceName, json["deviceName"]);

                JsonVariant jsonInterval = json["interval"];
                if (!jsonInterval.isNull()) {
                    interval = jsonInterval.as<int>();
                }
            }
        }
    }
    else {
        Serial.println("failed to mount FS");
    }
    //end read

    WiFiManager wifiManager;

    // Requesting name and interval
    WiFiManagerParameter custom_devicename("Gerätename", "Gerätename", deviceName, 40);
    WiFiManagerParameter custom_interval("Gerätename", "Interval", intervalBuffer, 5);

    // Add params to wifiManager
    wifiManager.addParameter(&custom_devicename);
    wifiManager.addParameter(&custom_interval);


    // Warte damit das Display initialisiert werden kannu
    delay(1000);



    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 0);  // https://github.com/nayarsystems/posix_tz_db 

    Serial.println("Config");

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);


    wifiManager.autoConnect("FollowerCounter");


    server.on("/", handleRoot);
    server.on("/format", getFormat);
    server.on("/update", getUpdate);
    server.on("/reset", getReset);
    server.on("/config", getConfig);

    server.begin();

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    //read updated parametersu
    strcpy(deviceName, custom_devicename.getValue());
    strcpy(intervalBuffer, custom_interval.getValue());

    interval = String(intervalBuffer).toInt();



    //save the custom parameters to FS
    if (shouldSaveConfig) {


        saveConfig();

        //end save
    }

    Serial.println("Starte");

}

void saveConfig() {
    DynamicJsonDocument json(1024);

    json["deviceName"] = deviceName;
    json["interval"] = interval;

    File configFile = SPIFFS.open("/config.json", "w");

    if (!configFile) {
        Serial.println("failed to open config file for writing");
    }

    serializeJson(json, Serial);
    serializeJson(json, configFile);
}

void infoWlan() {

    if (WiFi.status() == WL_CONNECTED) {

        // WLAN Ok
        Serial.println("WIFI OK");

    }
    else {

        // WLAN Error
        Serial.println("WIFI Error");

    }
}

void infoIP() {
    String localIP = WiFi.localIP().toString();

    USE_SERIAL.println(localIP);
}

void infoVersion() {

    char versionString[8];
    sprintf(versionString, "Ver. %s", VERSION);
    USE_SERIAL.println(versionString);
}


void infoReset() {

    USE_SERIAL.println("Format System");

    // Reset Wifi-Setting
    WiFiManager wifiManager;
    wifiManager.resetSettings();

    // Format Flash
    SPIFFS.format();

    // Restart
    ESP.reset();

}

void restartX() {

    USE_SERIAL.println("Restarte…");
    ESP.reset();
}

void showIntensity() {

    for (int intensity = 0; intensity < 16; intensity++) {
        char intensityString[8];
        sprintf(intensityString, " Int %d", intensity);
    }
}

void update_started() {
    USE_SERIAL.println("Update …");
    USE_SERIAL.println("CALLBACK:  HTTP update process started");
}

void update_finished() {

    USE_SERIAL.println("Done");
    USE_SERIAL.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
    char progressString[10];
    float percent = ((float)cur / (float)total) * 100;
    sprintf(progressString, " %s", String(percent).c_str());
    USE_SERIAL.println(progressString);
    USE_SERIAL.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
    char errorString[8];
    sprintf(errorString, "Err %d", err);
    USE_SERIAL.println(errorString);
    USE_SERIAL.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

//  
void loop() {

    server.handleClient();

    buttonState = digitalRead(TOGGLE_PIN);
    unsigned long currentMillis = millis();



    if (currentMillis % 2000 == 0) {
        //helloFullScreenPartialMode(timeString);
    }

    if (buttonState != lastButtonState && currentMillis > lastPressed + 50) {

        // if the state has changed, increment the counter
        if (buttonState == LOW) {
            // if the current state is HIGH then the button went from off to on:
            buttonPushCounter++;
            lastPressed = currentMillis;

            USE_SERIAL.println("push");


            USE_SERIAL.println(buttonPushCounter);

        }
        else {
            // if the current state is LOW then the button went from on to off:
            USE_SERIAL.println("off");
        }
    }

    // Warte 1sec nach dem letzten Tastendruck 
    if (currentMillis > lastPressed + 1000) {

        if (buttonPushCounter > 0) {

            USE_SERIAL.print("number of button pushes: ");
            USE_SERIAL.println(buttonPushCounter);

            switch (buttonPushCounter) {

            case 1:
                // Einmal gedrückt / FollowerCounter-Modus
                //mode = 1;
                break;

            case 2:
                // Zweimal gedrückt / Uhrzeit-Modus
                //mode = 2;
                break;

            case 3:
                // Dreimal gedrückt / Wechselmodus
                //mode = 3;
                break;

            case 4:
                infoWlan();
                break;

            case 5:
                infoIP();
                break;

            case 6:
                infoVersion();
                break;

            case 7:
                updateFirmware();
                break;

            case 8:
                restartX();
                break;

            case 10:
                infoReset();
                break;

            default:

                USE_SERIAL.println("Too many");
                break;
            }



        }

        buttonPushCounter = 0;
    }

    // save the current state as the last state, for next time through the loop
    lastButtonState = buttonState;


    // Update follower count
    if (currentMillis - previousMillis >= interval * 1000) {

        previousMillis = currentMillis;
        Serial.println(deviceName);

        Serial.print("Number of followers: ");




    }
}

void printTime() {

    time_t now = time(nullptr);
    String time = String(ctime(&now));
    time.trim();
    time.substring(11, 16).toCharArray(time_value, 10);

    USE_SERIAL.println(time_value);
}

void printCurrentFollower() {



    
}
