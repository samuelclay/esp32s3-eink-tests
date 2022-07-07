#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_EPD.h"
#include <SPI.h>
#include "BLEDevice.h"


// SSD1681 E-Ink Display
// 1.54" 200x200 Monochrome E-Ink

#define PIN_NEOPIXEL 48
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
#define EPD_DEBUG   1

#define EPD_CS      10
#define EPD_DC      16
#define SRAM_CS     3
#define SD_CS       46
#define EPD_RESET   4 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    17 // can set to -1 to not use a pin (will wait a fixed delay)

#define EPD_WIDTH   200
#define EPD_HEIGHT  200
#define HSPI_MISO 13
#define HSPI_MOSI 11
#define HSPI_SCLK 12
#define HSPI_CS   10
// SPIClass * hspi = new SPIClass(HSPI);
// Adafruit_SSD1681 display(200, 200, HSPI_MOSI, HSPI_SCLK, EPD_DC,
//                    EPD_RESET, EPD_CS, SRAM_CS, HSPI_MISO,
//                    EPD_BUSY);
// SPIClass *spi = NULL;
Adafruit_SSD1681 display(200, 200, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// Bluetooth Low Energy
// Turn Touch Remote Control

#define TT_BLE_NAME "Turn Touch Remote"
static BLEUUID ttButtonStatusServiceUuid("99c31523-dc4f-41b1-bb04-4e4deb81fadd");
static BLEUUID ttButtonStatusCharacteristicUuid("99c31525-dc4f-41b1-bb04-4e4deb81fadd");
static BLEUUID ttDeviceNicknameCharacteristicUuid("99c31526-dc4f-41b1-bb04-4e4deb81fadd");
static BLEUUID ttBatteryStatusServiceUuid("180F");
static BLEUUID ttBatteryLevelCharacteristicUuid("99c31523-dc4f-41b1-bb04-4e4deb81fadd");
static bool needsConnect = false;
static bool isConnected = false;
static BLEAddress *pRemoteAddress;
static BLERemoteCharacteristic *buttonStatusCharacteristic;
static BLERemoteCharacteristic *deviceNicknameCharacteristic;
static BLERemoteCharacteristic *batteryLevelCharacteristic;
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};
char *buttonStatus;
char *deviceNickname;
float batteryLevel;
bool newButtonStatus = false;
bool newDeviceNickname = false;
bool newBatteryLevel = false;

static void handleButtonStatusNotification(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify);

bool connectToRemoteDevice(BLEAddress pAddress)
{
  BLEClient *pClient = BLEDevice::createClient();
  pClient->connect(pAddress);
  Serial.println(" ---> Connected to remote device.");

  BLERemoteService *pButtonService = pClient->getService(ttButtonStatusServiceUuid);
  if (!pButtonService) {
    Serial.println(" ---> Failed to find button status service.");
    return false;
  }
  // BLERemoteService *pBatteryService = pClient->getService(ttBatteryStatusServiceUuid);
  // if (!pBatteryService) {
  //   Serial.println(" ---> Failed to find button status service.");
  //   return false;
  // }

  buttonStatusCharacteristic = pButtonService->getCharacteristic(ttButtonStatusCharacteristicUuid);
  // deviceNicknameCharacteristic = pButtonService->getCharacteristic(ttDeviceNicknameCharacteristicUuid);
  // batteryLevelCharacteristic = pBatteryService->getCharacteristic(ttBatteryLevelCharacteristicUuid);

  if (!buttonStatusCharacteristic) {//} || !deviceNicknameCharacteristic || !batteryLevelCharacteristic) {
    Serial.println(" ---> Failed to find characteristics.");
    return false;
  }

  buttonStatusCharacteristic->registerForNotify(handleButtonStatusNotification);

  return true;
}

static void handleButtonStatusNotification(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  if (isNotify) {
    Serial.println(" ---> Button status notification received.");
    buttonStatus = (char *)pData;
    newButtonStatus = true;
  } else {
    Serial.println(" ---> Button status notification received but ignored.");
  }
}

class AdvertisingCallback: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print(" ---> Found remote device named: ");
    Serial.print(advertisedDevice.getServiceUUID().toString().c_str());
    Serial.print(" --- ");
    Serial.println(advertisedDevice.toString().c_str());
    if (advertisedDevice.getName() == TT_BLE_NAME || (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(ttButtonStatusServiceUuid))) {
      advertisedDevice.getScan()->stop();
      pRemoteAddress = new BLEAddress(advertisedDevice.getAddress());
      needsConnect = true;
      Serial.println(" ---> Found remote device, connecting...");
    }
  }
};

void setup() {
  // pinMode(HSPI_CS, OUTPUT); //HSPI SS
  // hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); //SCLK, MISO, MOSI, SS
  // spi = new SPIClass(SPI);

  pixels.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(20); // not so bright

  pixels.fill(0xFF0000);
  pixels.show();


  Serial.begin(115200);
  Serial.println("Hello! EPD Test");

  // Adafruit_SSD1681 display = Adafruit_SSD1681(EPD_WIDTH, EPD_HEIGHT, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
    // spi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
  Serial.println("Initialized");

  display.begin();
#if defined(FLEXIBLE_213) || defined(FLEXIBLE_290)
  // The flexible displays have different buffers and invert settings!
  display.setBlackBuffer(1, false);
  display.setColorBuffer(1, false);
#endif


  display.setRotation(2);

  // large block of text
  display.clearBuffer();
  display.setTextWrap(true);

  display.setCursor(10, 10);
  display.setTextSize(1);
  display.setTextColor(EPD_BLACK);
  display.print("Get as much education as you can. Nobody can take that away from you");

  display.setCursor(50, 70);
  display.setTextColor(EPD_RED);
  display.print("--Eben Upton");

  pixels.fill(0x00FF00);
  pixels.show();

  display.display();

  pixels.fill(0x0000FF);
  pixels.show();

  Serial.println(" ---> Initializing BLE...");
  BLEDevice::init("");
  BLEScan *pScanner = BLEDevice::getScan();
  pScanner->setAdvertisedDeviceCallbacks(new AdvertisingCallback());
  pScanner->setActiveScan(true);
  BLEScanResults foundDevices = pScanner->start(10, false);
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
    if (connectToRemoteDevice(*pRemoteAddress)) {
      Serial.println(" ---> Connected to remote");
      buttonStatusCharacteristic->getDescriptor(BLEUUID("2902"))->writeValue((uint8_t *)notificationOn, 2, true);
      isConnected = true;
    } else {
      Serial.println(" ---> Failed to connect to remote");
    }
    needsConnect = false;
  }

  if (newButtonStatus) {
    newButtonStatus = false;
    pixels.fill(0x0000FF);
    pixels.show();
    delay(1500); // wait half a second
  }
}
