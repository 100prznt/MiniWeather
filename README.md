# MiniWeather

MiniWeather ist ein Projekt welches verschiedene Wetterdaten erfasst und im Netzwerk veröffentlicht.

## Hardware
* WeMos D1 mini
* Sensirion SHT85 (Luftfeuchtigkeit und Temperatur)
* Bosch BMP180 (Luftdruck und Temperatur)
* Vishay VEML7700 (Beleuchtungsstärke)


# Installing the Arduino ESP32 Filesystem Uploader

* [Installing the Arduino ESP32 Filesystem Uploader](https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/)


## ioBroker implemetation

You can use this JS:


const url = 'http://<MiniWeather-IP>/api';
const idTemperature = '0_userdata.0.MiniWeather.Temperature';
const idTemperatureBmp = '0_userdata.0.MiniWeather.TemperatureBMP';
const idHumidity = '0_userdata.0.MiniWeather.Humidity';
const idPressure = '0_userdata.0.MiniWeather.Pressure';
const idIlluminance = '0_userdata.0.MiniWeather.Illuminance';
const idWll = '0_userdata.0.MiniWeather.WhiteLightLevel';
const idIlluminanceNom = '0_userdata.0.MiniWeather.IlluminanceNormalized';
const idWllNom = '0_userdata.0.MiniWeather.WhiteLightLevelNormalized';
const idRSSI = '0_userdata.0.MiniWeather.RSSI';
const idFW = '0_userdata.0.MiniWeather.FirmwareVersion';
const idDewPoint = '0_userdata.0.MiniWeather.DewPoint';
const idHumidityAbs = '0_userdata.0.MiniWeather.HumidityAbs';
const idTemperatureError = '0_userdata.0.MiniWeather.TemperatureError';
 

schedule('* * * * *', function() { //every minute
    request(url, function(error, response, result) {
        let obj = JSON.parse(result);
        //console.log(result);
        let temperatureSht85 = parseFloat(obj['SHT85']['Temperature'].Value);
        let temperatureBmp180 = parseFloat(obj['BMP180']['Temperature'].Value);
        let humidity = parseFloat(obj['SHT85']['Humidity'].Value);
        setState(idTemperature, temperatureSht85, true);
        setState(idTemperatureBmp, temperatureBmp180, true);
        setState(idHumidity, humidity, true);
        setState(idPressure, parseFloat(obj['BMP180']['Pressure'].Value), true);
        setState(idIlluminance, parseFloat(obj['VEML7700']['Illuminance'].Value), true);
        setState(idWll, parseFloat(obj['VEML7700']['White light level'].Value), true);
        setState(idIlluminanceNom, parseFloat(obj['VEML7700']['Illuminance normalized'].Value), true);
        setState(idWllNom, parseFloat(obj['VEML7700']['White light level normalized'].Value), true);
        setState(idRSSI, parseInt(obj['RSSI']), true);
        setState(idFW, obj['Firmware'], true);



        setState(idTemperatureError, Math.round((temperatureSht85 - temperatureBmp180 + Number.EPSILON) * 10000) / 10000, true);
        setState(idDewPoint, Math.round((calcDewPoint(temperatureSht85, humidity) + Number.EPSILON) * 100) / 100, true);
        setState(idHumidityAbs, Math.round((calcHumidityAbs(temperatureSht85, humidity) + Number.EPSILON) * 100) / 100, true);
    });
});

function calcDewPoint(t, r) {
     
  // Konstanten
  var mw = 18.016; // Molekulargewicht des Wasserdampfes (kg/kmol)
  var gk = 8214.3; // universelle Gaskonstante (J/(kmol*K))
  var t0 = 273.15; // Absolute Temperatur von 0 °C (Kelvin)
  var tk = t + t0; // Temperatur in Kelvin
  
  var a, b;
  if (t >= 0) {
    a = 7.5;
    b = 237.3;
  } else if (t < 0) {
    a = 7.6;
    b = 240.7;
  }
  
  // Sättigungsdampfdruck in hPa
  var sdd = 6.1078 * Math.pow(10, (a*t)/(b+t));
  
  // Dampfdruck in hPa
  var dd = sdd * (r/100);
  
  // v-Parameter
  var v = Math.log10(dd/6.1078);
  
  // Taupunkttemperatur (°C)
  var tt = (b*v) / (a-v);
  return tt; 
}

function calcHumidityAbs(t, r) {
     
  // Konstanten
  var mw = 18.016; // Molekulargewicht des Wasserdampfes (kg/kmol)
  var gk = 8214.3; // universelle Gaskonstante (J/(kmol*K))
  var t0 = 273.15; // Absolute Temperatur von 0 °C (Kelvin)
  var tk = t + t0; // Temperatur in Kelvin
  
  var a, b;
  if (t >= 0) {
    a = 7.5;
    b = 237.3;
  } else if (t < 0) {
    a = 7.6;
    b = 240.7;
  }
  
  // Sättigungsdampfdruck in hPa
  var sdd = 6.1078 * Math.pow(10, (a*t)/(b+t));
  
  // Dampfdruck in hPa
  var dd = sdd * (r/100);
  
  // Wasserdampfdichte bzw. absolute Feuchte in g/m3
  var af = Math.pow(10,5) * mw/gk * dd/tk;
  
  return af; 
}

