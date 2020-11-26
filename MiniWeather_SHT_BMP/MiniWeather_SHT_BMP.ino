
#include <FS.h>                    //this needs to be first, or it all crashes and burns...
#include "Arduino.h"

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson ArduinoJSON


#include <NTPClient.h>
#include <time.h>

#include <ESPStringTemplate.h>
#include "index.h"

#include <Adafruit_BME280.h>
#include <Wire.h>
#include "SHTSensor.h"
#include <Adafruit_BMP085.h>
#include "Adafruit_VEML7700.h"


#define USE_SERIAL Serial
#define FW_REV "0.03.1"

#define SHT "SHT85"
//#define BME "BME280"
#define BMP "BMP180"
#define VEML "VEML7700"



const int ledPin = LED_BUILTIN;// the number of the LED pin
const int shtPowerPin = D5;
//const int bmePowerPin = D0;

const float errorValue = 0;

ESP8266WebServer server(80);
#ifdef SHT
SHTSensor sht;
#endif
#ifdef BME
Adafruit_BME280 bme(0x76); // I2C
#endif
#ifdef BMP
Adafruit_BMP085 bmp; // I2C
#endif
#ifdef VEML
Adafruit_VEML7700 veml = Adafruit_VEML7700();
#endif


//define your default values here, if there are different values in config.json, they are overwritten.
int interval = 300;
char deviceName[40] = "MiniWeather";
char ubidotsToken[160];


//flag for saving data
bool shouldSaveConfig = true;
bool inLoop = false;

#ifdef SHT
float temperatureSht, humiditySht;
#endif
#ifdef BME
float temperatureBme, humidityBme, pressureBme;
#endif
#ifdef BMP
float temperatureBmp, pressureBmp;
#endif
#ifdef VEML
float illuminance, whiteLightLvl, rawAmbiLightLvl, illuminanceNom, whiteLightLvlNom;
#endif
time_t measuretime;
long secureCounter;



void setup()
{
	// Serial debugging
	USE_SERIAL.begin(115200);

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW); //Led on
#ifdef SHT
    pinMode(shtPowerPin, OUTPUT);
    digitalWrite(shtPowerPin, HIGH);
#endif
    //pinMode(bmePowerPin, OUTPUT);
    //digitalWrite(bmePowerPin, HIGH);


    if (SPIFFS.begin())
    {
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

                JsonVariant jsonMode = json["mode"];
                if (!jsonMode.isNull()) {
                    interval = jsonMode.as<int>();
                }
            }
        }
    }
    else {
        USE_SERIAL.println("failed to mount FS");
    }
    //end read

    WiFiManager wifiManager;

    // Requesting Instagram and Intensity for Display
    WiFiManagerParameter custom_deviceName("deviceName", "Device name", deviceName, 40);
    WiFiManagerParameter custom_interval("interval", "Update Interval (s)", String(interval).c_str(), 6, "type=\"number\" step=\"any\"");
    WiFiManagerParameter custom_ubidotsToken("ubidotsToken", "Ubidots token", ubidotsToken, 160);

    // Add params to wifiManager
    wifiManager.addParameter(&custom_deviceName);
    wifiManager.addParameter(&custom_interval);
    wifiManager.addParameter(&custom_ubidotsToken);

    delay(1000);


    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 0);  // https://github.com/nayarsystems/posix_tz_db 



      //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);


    wifiManager.autoConnect("MiniWeather");



    server.on("/", handleRoot);
    server.on("/api", getApi);
    //server.on("/update", getUpdate);
    //server.on("/reset", getReset);
    //server.on("/config", getConfig);

    server.begin();



    USE_SERIAL.print("IP address: ");
    USE_SERIAL.println(WiFi.localIP());



    //read updated parametersu
    strcpy(deviceName, custom_deviceName.getValue());
    interval = String(custom_interval.getValue()).toInt();
    strcpy(ubidotsToken, custom_ubidotsToken.getValue());


    //save the custom parameters to FS
    if (shouldSaveConfig) {


        saveConfig();

        //end save
    }


    secureCounter = 0;
#ifdef SHT
    digitalWrite(shtPowerPin, HIGH);
    Wire.begin();

    if (!sht.init())
    {
        USE_SERIAL.println("Could not find a valid " + String(SHT) + " sensor sensor, check wiring!");
        while (1);
    }

    //sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM); // only supported by SHT3x

    if (sht.readSample())
    {
        USE_SERIAL.println(String(SHT) + " meas values:");
        USE_SERIAL.println(String(sht.getHumidity()) + " %RH");
        USE_SERIAL.println(String(sht.getTemperature()) + " °C");
    }

#endif

#ifdef BME
    if (!bme.begin())
    {
        Serial.println("Could not find a valid " + String(BME) + " sensor, check wiring!");
        while (1);
    }


    USE_SERIAL.println(String(BME) + " meas values:");
    USE_SERIAL.println(String(bme.readPressure() / 10.0) + " kPa");
    USE_SERIAL.println(String(bme.readHumidity()) + " %RH");
    USE_SERIAL.println(String(bme.readTemperature()) + " °C");
#endif

#ifdef BMP
    if (!bmp.begin())
    {
        USE_SERIAL.println("Could not find a valid " + String(BMP) + " sensor, check wiring!");
        while (1) {}
    }


    USE_SERIAL.println(String(BMP) + " meas values:");
    USE_SERIAL.println(String(bmp.readTemperature()) + " °C");
    USE_SERIAL.println(String(bmp.readPressure() / 100.0) + " kPa");
    USE_SERIAL.println(String(bmp.readAltitude()) + " m");
    USE_SERIAL.println(String(bmp.readSealevelPressure()) + " Pa");

    USE_SERIAL.println(String(bmp.readTemperature()) + " °C");
#endif
#ifdef VEML
    if (!veml.begin())
    {
        USE_SERIAL.println("Could not find a valid " + String(VEML) + " sensor, check wiring!");
        while (1);
    }

    veml.setGain(VEML7700_GAIN_1_8);
    veml.setIntegrationTime(VEML7700_IT_25MS);
    delay(25);

    USE_SERIAL.print("Lux: "); USE_SERIAL.println(veml.readLux());
    USE_SERIAL.print("White: "); USE_SERIAL.println(veml.readWhite());
    USE_SERIAL.print("Raw ALS: "); USE_SERIAL.println(veml.readALS());
#endif

    digitalWrite(ledPin, HIGH);
#ifdef SHT
    //digitalWrite(shtPowerPin, LOW);
#endif
    //digitalWrite(bmePowerPin, LOW);

    // Add a 2 second delay.
    delay(2000); //just here to slow down the output.
}


void saveConfig()
{
    DynamicJsonDocument json(1024);

    json["deviceName"] = deviceName;
    json["interval"] = interval;
    json["ubidotsToken"] = ubidotsToken;

    File configFile = SPIFFS.open("/config.json", "w");

    if (!configFile) {
        USE_SERIAL.println("failed to open config file for writing");
    }

    serializeJson(json, Serial);
    serializeJson(json, configFile);
}

//callback notifying us of the need to save config
void saveConfigCallback()
{
    USE_SERIAL.println("Should save config");
    shouldSaveConfig = true;
}

void handleRoot()
{
    /*ESPStringTemplate webpage(htmlBuffer, sizeof(htmlBuffer));

    TokenStringPair pair[1];
    pair[0].setPair("%INSTAGRAM%", instagramName);

    webpage.add_P(_PAGE_HEAD);
    webpage.add_P(_PAGE_START);

    webpage.add_P(_PAGE_ACTIONS);

    webpage.add_P(_PAGE_CONFIG_NAME, pair, 1);

    switch (mode)
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
    }

    TokenStringPair intensityPair[1];


    intensityPair[0].setPair("%INTENSITY%", matrixIntensity);
    webpage.add_P(_PAGE_CONFIG_INTENSITY, intensityPair, 1);
    webpage.add_P(_PAGE_FOOTER);

    server.send(200, "text/html", htmlBuffer);*/
}

void getApi()
{
    USE_SERIAL.println("GET API");

    performeMeasurement();


    String timestamp = String(ctime(&measuretime));
    timestamp.trim();

    DynamicJsonDocument jsonDoc(1024);
#ifdef SHT
    DynamicJsonDocument jsonSht(1024);
    jsonSht["Humidity"] = generateMeasValueJsonDoc(humiditySht, "Relative humidity", "\u0025RH");
    jsonSht["Temperature"] = generateMeasValueJsonDoc(temperatureSht, "Celsius", "\u00B0C");
    jsonDoc[SHT] = jsonSht;
#endif
#ifdef BME
    DynamicJsonDocument jsonBme(1024);
    jsonBme["Humidity_BME"] = generateMeasValueJsonDoc(humidityBme, "Relative humidity", "\u0025RH");
    jsonBme["Temperature_BME"] = generateMeasValueJsonDoc(temperatureBme, "Celsius", "\u00B0C");
    jsonBme["Pressure_BME"] = generateMeasValueJsonDoc(pressureBme, "Kilopascal", "kPa");
    jsonDoc[BME] = jsonBme;
#endif
#ifdef BMP
    DynamicJsonDocument jsonBmp(1024);
    jsonBmp["Pressure"] = generateMeasValueJsonDoc(pressureBmp, "Kilopascal", "kPa");
    jsonBmp["Temperature"] = generateMeasValueJsonDoc(temperatureBmp, "Celsius", "\u00B0C");
    jsonDoc[BMP] = jsonBmp;
#endif
#ifdef VEML
    DynamicJsonDocument jsonVeml(1024);
    jsonVeml["Illuminance"] = generateMeasValueJsonDoc(illuminance, "Lux", "lx");
    jsonVeml["White light level"] = generateMeasValueJsonDoc(whiteLightLvl, "", "");
    //jsonVeml["Raw ambiance light level"] = generateMeasValueJsonDoc(rawAmbiLightLvl, "", "");
    jsonVeml["Illuminance normalized"] = generateMeasValueJsonDoc(illuminanceNom, "Lux", "lx");
    jsonVeml["White light level normalized"] = generateMeasValueJsonDoc(whiteLightLvlNom, "", "");
    jsonDoc[VEML] = jsonVeml;
#endif
    jsonDoc["Timestamp"] = timestamp;
    jsonDoc["SecureCounter"] = secureCounter;
    jsonDoc["Firmware"] = FW_REV;
    jsonDoc["RSSI"] = WiFi.RSSI();

    char JSONmessageBuffer[1024];
    serializeJsonPretty(jsonDoc, JSONmessageBuffer);


    server.send(200, "application/json", JSONmessageBuffer);
}

void redirectBack()
{
    server.sendHeader("Location", String("/"), true);
    server.send(302, "text/plain", "");
}

DynamicJsonDocument generateMeasValueJsonDoc(float value, String unit, String symbol)
{
    DynamicJsonDocument jsonDoc(256);
    jsonDoc["Value"] = value;
    jsonDoc["Unit"] = unit;
    jsonDoc["Symbol"] = symbol;

    return jsonDoc;
}

void performeMeasurement()
{
    digitalWrite(ledPin, LOW);
    measuretime = time(nullptr);
#ifdef SHT
    //digitalWrite(shtPowerPin, HIGH);
    //delay(5); //start up sht
    if (sht.init() && sht.readSample())
    {
        humiditySht = sht.getHumidity();
        temperatureSht = sht.getTemperature();

        USE_SERIAL.println(String(humiditySht) + " %RH");
        USE_SERIAL.println(String(temperatureSht) + " °C");
    }
    else
    {
        USE_SERIAL.println("Error in readSample() at " + String(SHT));
        humiditySht = errorValue;
        temperatureSht = errorValue;
    }

    //digitalWrite(shtPowerPin, LOW);
#endif
#ifdef BME
    humidityBme = bme.readHumidity();
    temperatureBme = bme.readTemperature();
    pressureBme = bme.readPressure() / 10.0;

    USE_SERIAL.println(String(humidityBme) + " %RH");
    USE_SERIAL.println(String(temperatureBme) + " °C");
    USE_SERIAL.println(String(pressureBme) + " kPa");
#endif
#ifdef BMP
    /*if (bmp.begin())
    {
        temperatureBmp = bmp.readTemperature();
        pressureBmp = bmp.readPressure() / 100;
    }
    else
    {
        USE_SERIAL.println("Error durring bmp.begin()");
    }*/
    delay(100);
    temperatureBmp = bmp.readTemperature();
    pressureBmp = bmp.readPressure() / 100.0;

    USE_SERIAL.println(String(temperatureBmp) + " °C");
    USE_SERIAL.println(String(pressureBmp) + " kPa");
#endif
#ifdef VEML
    illuminance = veml.readLux();
    whiteLightLvl = veml.readWhite();
    //rawAmbiLightLvl = veml.readALS();
    illuminanceNom = veml.readLuxNormalized();
    whiteLightLvlNom = veml.readWhiteNormalized();
#endif

    secureCounter++;
    USE_SERIAL.println("Secure counter: " + String(secureCounter));

    digitalWrite(ledPin, HIGH);
}

void loop()
{
    if (!inLoop)
    {
        USE_SERIAL.println("IN LOOP");
        inLoop = true;
    }

    server.handleClient();
}