#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_EPD.h"
#include <SPI.h>
#include "BLEDevice.h"
#include "esp32-hal-log.h"

// SSD1681 E-Ink Display
// 1.54" 200x200 Monochrome E-Ink

#define PIN_NEOPIXEL 48
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#define EPD_CS      10
#define EPD_DC      16
#define SRAM_CS     41
#define SD_CS       42
#define EPD_RESET   18 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    17 // can set to -1 to not use a pin (will wait a fixed delay)

#define EPD_WIDTH   200
#define EPD_HEIGHT  200
#define HSPI_MISO   13
#define HSPI_MOSI   11
#define HSPI_SCLK   12
#define HSPI_CS     EPD_CS
// SPIClass *hspi = NULL;
// SPIClass SPI1(HSPI);
// Adafruit_SSD1681 display(200, 200, HSPI_MOSI, HSPI_SCLK, EPD_DC,
//                    EPD_RESET, EPD_CS, SRAM_CS, HSPI_MISO,
//                    EPD_BUSY);
// Adafruit_SSD1681 display(EPD_WIDTH, EPD_HEIGHT, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
Adafruit_SSD1681 display(EPD_WIDTH, EPD_HEIGHT, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// Bluetooth Low Energy
// Turn Touch Remote Control

#define TT_BLE_NAME "Turn Touch Rem"
static BLEUUID ttButtonStatusServiceUuid("99c31523-dc4f-41b1-bb04-4e4deb81fadd");
static BLEUUID ttButtonStatusCharacteristicUuid("99c31525-dc4f-41b1-bb04-4e4deb81fadd");
static BLEUUID ttDeviceNicknameCharacteristicUuid("99c31526-dc4f-41b1-bb04-4e4deb81fadd");
static BLEUUID ttBatteryStatusServiceUuid("180F");
static BLEUUID ttBatteryLevelCharacteristicUuid("99c31523-dc4f-41b1-bb04-4e4deb81fadd");
static bool needsConnect = false;
static bool isConnected = false;
static BLEAddress *pRemoteAddress;
static BLERemoteService *buttonStatusService;
static BLERemoteCharacteristic *buttonStatusCharacteristic;
static BLERemoteCharacteristic *deviceNicknameCharacteristic;
static BLERemoteCharacteristic *batteryLevelCharacteristic;
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};
uint8_t buttonStatus[] = {0x0, 0x0};
char *deviceNickname;
float batteryLevel;
bool newButtonStatus = false;
bool newDeviceNickname = false;
bool newBatteryLevel = false;

static void handleButtonStatusNotification(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify);

void epdDisplayText(String text) {
  int offset = 6*text.length();
  display.clearDisplay();

  display.clearBuffer();
  display.setCursor(display.width()/2-offset, display.height()/2-8);
  display.setTextColor(EPD_BLACK);
  display.setTextSize(2);
  display.print(text);

  display.drawLine(0, display.height()/2, display.width()/2, display.height(), EPD_BLACK);
  display.drawLine(display.width()/2, display.height(), display.width(), display.height()/2, EPD_BLACK);
  display.drawLine(display.width(), display.height()/2, display.width()/2, 0, EPD_BLACK);
  display.drawLine(display.width()/2, 0, 0, display.height()/2, EPD_BLACK);
  display.display();
}

bool connectToRemoteDevice(BLEAddress pAddress)
{
  BLEClient *pClient = BLEDevice::createClient();
  pClient->connect(pAddress, BLE_ADDR_TYPE_RANDOM);
  Serial.println(" ---> Connected to Turn Touch, registering for notifications...");

  buttonStatusService = pClient->getService(ttButtonStatusServiceUuid);
  if (!buttonStatusService) {
    Serial.println(" ---> Failed to find button status service.");
    return false;
  } else {
    Serial.print(" ---> Found service: ");
    Serial.println(buttonStatusService->toString().c_str());
  }
  // BLERemoteService *pBatteryService = pClient->getService(ttBatteryStatusServiceUuid);
  // if (!pBatteryService) {
  //   Serial.println(" ---> Failed to find button status service.");
  //   return false;
  // }

  buttonStatusCharacteristic = buttonStatusService->getCharacteristic(ttButtonStatusCharacteristicUuid);
  // deviceNicknameCharacteristic = buttonStatusService->getCharacteristic(ttDeviceNicknameCharacteristicUuid);
  // batteryLevelCharacteristic = pBatteryService->getCharacteristic(ttBatteryLevelCharacteristicUuid);

  if (!buttonStatusCharacteristic) {//} || !deviceNicknameCharacteristic || !batteryLevelCharacteristic) {
    Serial.println(" ---> Failed to find characteristics.");
    return false;
  } else {
    Serial.print(" ---> Found characteristic: ");
    Serial.println(buttonStatusCharacteristic->toString().c_str());
  }

  buttonStatusCharacteristic->registerForNotify(handleButtonStatusNotification);

  return true;
}

static void handleButtonStatusNotification(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  if (isNotify) {
    Serial.print(" ---> Button status notification received: ");
    Serial.print(length);
    Serial.print(" bytes: ");
    for (int i = 0; i < length; i++) {
      Serial.print(pData[i], HEX);
      Serial.print(" ");
      buttonStatus[i] = pData[i];
    }
    Serial.println("");
    newButtonStatus = true;
  } else {
    Serial.println(" ---> Button status notification received but ignored.");
  }
}

class AdvertisingCallback: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print(" ---> Found remote device named ");
    Serial.print(advertisedDevice.getName().c_str());
    Serial.print(" --- ");
    Serial.println(advertisedDevice.toString().c_str());
    if (advertisedDevice.getName() == TT_BLE_NAME || (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(ttButtonStatusServiceUuid))) {
      advertisedDevice.getScan()->stop();
      pRemoteAddress = new BLEAddress(advertisedDevice.getAddress());
      needsConnect = true;
      Serial.println(" ---> Found Turn Touch device, connecting...");
    }
  }
};

static void my_gap_event_handler(esp_gap_ble_cb_event_t  event, esp_ble_gap_cb_param_t* param) {
	log_d("custom gap event handler, event: %d", (uint8_t)event);
}

static void my_gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param) {
	log_d("custom gattc event handler, event: %d", (uint8_t)event);
}

void setup() {
  // hspi = new SPIClass(HSPI);
  // hspi->begin(HSPI_SCLK, HSPI_MOSI, HSPI_MISO, HSPI_CS);
  // Adafruit_SSD1681 display(200, 200, HSPI_MOSI, HSPI_SCLK, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, HSPI_MISO, EPD_BUSY);

  // hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); //SCLK, MISO, MOSI, SS
  // pinMode(HSPI_CS, OUTPUT); //HSPI SS

  pixels.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(20); // not so bright

  pixels.fill(0xFF0000);
  pixels.show();


  Serial.begin(115200);
  Serial.println("Hello! EPD Test");

  Serial.print("SCK:"); Serial.println(SCK);
  Serial.print("MISO:"); Serial.println(MISO);
  Serial.print("MOSI:"); Serial.println(MOSI);
  Serial.print("SS:"); Serial.println(SS);

  // Adafruit_SSD1681 display = Adafruit_SSD1681(EPD_WIDTH, EPD_HEIGHT, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
    // spi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
  Serial.println("Initialized");

  display.begin(true);
  display.setRotation(3);

  pixels.fill(0x00FF00);
  pixels.show();

  epdDisplayText("Searching...");
  delay(2000);
  pixels.fill(0x0000FF);
  pixels.show();

  Serial.println(" ---> Initializing BLE...");
  BLEDevice::init("ESP32-eink-tests");
  BLEDevice::setCustomGapHandler(my_gap_event_handler);
  BLEDevice::setCustomGattcHandler(my_gattc_event_handler);
  BLEScan *pScanner = BLEDevice::getScan();
  pScanner->setAdvertisedDeviceCallbacks(new AdvertisingCallback());
  pScanner->setActiveScan(true);
  pScanner->setInterval(1349);
  pScanner->setWindow(449);
  BLEScanResults foundDevices = pScanner->start(5, false);
  Serial.print(" ---> Found ");
  Serial.print(foundDevices.getCount());
  Serial.println(" devices");

  pixels.fill(0x00FF00);
  pixels.show();
}

void loop() {
  Serial.print("Loop: needsConnect=");
  Serial.print(needsConnect);
  Serial.print(" isConnected=");
  Serial.println(isConnected);

  pixels.fill(0xFF00FF);
  pixels.show();
  delay(500); // wait half a second
  pixels.fill(0x00FFFF);
  pixels.show();
  delay(500); // wait half a second
  pixels.fill(0xFF00FF);
  pixels.show();
  delay(500); // wait half a second
  pixels.fill(0xFFFF00);
  pixels.show();
  delay(500); // wait half a second


  
  if (needsConnect) {
    Serial.println(" ---> needsConnect, connecting to remote...");
    if (connectToRemoteDevice(*pRemoteAddress))
    {
      Serial.println(" ---> Connected to remote");
      buttonStatusCharacteristic->getDescriptor(BLEUUID("2902"))->writeValue((uint8_t *)notificationOn, 2, true);
      isConnected = true;
      
      epdDisplayText("Remote found...");
    }
    else
    {
      Serial.println(" ---> Failed to connect to remote");
    }
    needsConnect = false;
  } else if (!isConnected) {
    BLEScan *pScanner = BLEDevice::getScan();
    pScanner->setAdvertisedDeviceCallbacks(new AdvertisingCallback());
    pScanner->setActiveScan(true);
    pScanner->setInterval(1349);
    pScanner->setWindow(449);
    BLEScanResults foundDevices = pScanner->start(5, false);
    Serial.print(" ---> Found ");
    Serial.print(foundDevices.getCount());
    Serial.println(" devices");
  }

  if (newButtonStatus) {
    newButtonStatus = false;
    pixels.fill(0x0000FF);
    pixels.show();
    Serial.print(" --> Button status: ");
    Serial.println(buttonStatus[0]);
    switch (buttonStatus[0])
    {
    case 0xFE:
      epdDisplayText("North");
      break;
    
    case 0xFD:
      epdDisplayText("East");
      break;
    
    case 0xFB:
      epdDisplayText("West");
      break;
    
    case 0xF7:
      epdDisplayText("South");
      break;
    
    default:
      break;
    }
    delay(1500); // wait half a second
  }
}
