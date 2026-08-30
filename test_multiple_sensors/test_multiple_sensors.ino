#include <ArduinoJson.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

int numberOfDevices;

DeviceAddress tempDeviceAddress;

const int ioPinsCount = 6;
const int ioPins[ioPinsCount] = {4,5,6,7,8,9};

enum TempState { IDLE, WAITING };
TempState tempState = IDLE;
unsigned long conversionStartTime = 0;
unsigned long CONVERSION_TIME_MS = 800; // default value 800
const int PRECISION = 12; // 9, 10, 11, 12
// Rozdzielczość	  Krok temperatury	Max czas konwersji
// 9-bit	          0.5°C	            93.75 ms
// 10-bit	          0.25°C	          187.5 ms
// 11-bit	          0.125°C	          375 ms
// 12-bit (default) 0.0625°C	        750 ms

unsigned long lastTempTrigger = 0;
unsigned long lastPinSend = 0;
const unsigned long TEMP_INTERVAL_MS = 1000;
const unsigned long PIN_INTERVAL_MS = 250;

DeviceAddress addresses[4];
float temps[4] = {0};

void setup(void) {
  // start serial port
  Serial.begin(9600);
  
  // Temp sensors
  sensors.begin();
  numberOfDevices = sensors.getDeviceCount();
  sensors.setResolution(PRECISION);
  setConversionTime();
  sensors.setWaitForConversion(false);

  if (CONVERSION_TIME_MS >= TEMP_INTERVAL_MS) {
    Serial.println("[ERROR] TEMP_INTERVAL_MS myst be greater than CONVERSION_TIME_MS!");
  }

  printDevices();  
  
  // IO pins
  registerIoPins();

}

void loop(void) { 
    unsigned long now = millis();

  // Start konwersji (raz na sekundę)
  if (tempState == IDLE && now - lastTempTrigger >= TEMP_INTERVAL_MS) {
    lastTempTrigger = now;
    sensors.requestTemperatures();     // nie blokuje
    conversionStartTime = now;
    tempState = WAITING;
  }

  // Odczyt gdy czas minął
  if (tempState == WAITING && now - conversionStartTime >= CONVERSION_TIME_MS) {
    readTemperatureC(addresses, temps, numberOfDevices);
    printTempsJson(addresses, temps, numberOfDevices);
    tempState = IDLE;
  }

  // Piny — działają niezależnie, bez czekania na temperatury
  if (now - lastPinSend >= PIN_INTERVAL_MS) {
    lastPinSend = now;
    int pinValues[ioPinsCount];
    readPinValues(ioPins, pinValues, ioPinsCount);
    printPinsJson(ioPins, pinValues, ioPinsCount);
  }
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
  }
}

void readTemperatureC(DeviceAddress addresses[], float temps[], int devicesCount){
  DeviceAddress tmpAddress;
  
  for(int i=0;i<devicesCount; i++) {
    if(sensors.getAddress(tmpAddress, i)){

    memcpy(addresses[i], tmpAddress, sizeof(DeviceAddress));
    temps[i] = sensors.getTempC(tmpAddress);
    } 	
  }
}

void setConversionTime(){
  switch(PRECISION){
    case 9:
      CONVERSION_TIME_MS = 95;
      break;
    case 10:
      CONVERSION_TIME_MS = 190;
      break;
    case 11:
      CONVERSION_TIME_MS = 400;
      break;
    case 12:
      CONVERSION_TIME_MS = 800;
      break;
    default:
      Serial.println("[WARNING] Default conversation time. PRECISION not set or wrong value.");
      CONVERSION_TIME_MS = 800;
      break;
  }
}

void printDeviceTemperatures(DeviceAddress addresses[], float temps[], int devicesCount){
  for(int i=0;i<devicesCount; i++) {
    Serial.print("[DEBUG]: i:");
    Serial.print(i);
    Serial.print(" Address: ");
    printAddress(addresses[i]);
    Serial.print(" temp: ");
    Serial.print(temps[i]);
    Serial.println();
  }
}

void readPinValues(int pins[], int pinValues[], int pinCount){
  for(int i = 0; i < pinCount; i++){
    pinValues[i] = digitalRead(pins[i]);
  }
}

void printPinValues(int pins[], int pinValues[], int pinCount){
  for(int i = 0; i < pinCount; i++){
    Serial.print("[DEBUG] pin: "),
    Serial.print(pins[i]),
    Serial.print(" value: "),
    Serial.print(pinValues[i]),
    Serial.println();
  }
}

void registerIoPins(){
  for(int i = 0; i < ioPinsCount; i++ ){
    int pin = ioPins[i];
    pinMode(pin, INPUT_PULLUP);
    Serial.print("[DEBUG] REGISTERED io-pin (INPUT_PULLUP) - on pin: ");
    Serial.print(pin, DEC);
    Serial.println();
  }
}

void printDevices(){
  // locate devices on the bus
  Serial.print("[DEBUG] Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");

  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++) {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)) {
      Serial.print("[DEBUG] Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
		} else {
		  Serial.print("[DEBUG] Found ghost device at ");
		  Serial.print(i, DEC);
		  Serial.print(" but could not detect address. Check power and cabling");
		}
  }
}
void printTempsJson(DeviceAddress addresses[], float temps[], int tempSensorsCount) {

  StaticJsonDocument<384> doc;

  JsonArray tempsArr = doc.createNestedArray("temps");
  for (int i = 0; i < tempSensorsCount; i++) {
    JsonObject t = tempsArr.createNestedObject();

    char addrStr[17];
    for (int b = 0; b < 8; b++) {
      sprintf(addrStr + b * 2, "%02X", addresses[i][b]);
    }
    addrStr[16] = '\0';

    t["a"] = addrStr;
    t["t"] = serialized(String(temps[i], 2));
  }

  serializeJson(doc, Serial);
  Serial.println();
}

void printPinsJson(int pins[], int pinValues[], int pinsCount) {

  StaticJsonDocument<128> doc;

  JsonObject pinsObj = doc.createNestedObject("pins");
  for (int i = 0; i < pinsCount; i++) {
    pinsObj[String(pins[i])] = pinValues[i];
  }

  serializeJson(doc, Serial);
  Serial.println();
}
void printBatchJson(DeviceAddress addresses[], float temps[], int tempSensorsCount,
                     int pins[], int pinValues[], int pinsCount) {

  StaticJsonDocument<512> doc;

  JsonArray tempsArr = doc.createNestedArray("temps");
  for (int i = 0; i < tempSensorsCount; i++) {
    JsonObject t = tempsArr.createNestedObject();

    char addrStr[17];
    for (int b = 0; b < 8; b++) {
      sprintf(addrStr + b * 2, "%02X", addresses[i][b]);
    }
    addrStr[16] = '\0';

    t["a"] = addrStr;
    t["t"] = serialized(String(temps[i], 2)); // 2 miejsca po przecinku, bez cudzysłowów
  }

  JsonObject pinsObj = doc.createNestedObject("pins");
  for (int i = 0; i < pinsCount; i++) {
    pinsObj[String(pins[i])] = pinValues[i];
  }

  serializeJson(doc, Serial);
  Serial.println();
}