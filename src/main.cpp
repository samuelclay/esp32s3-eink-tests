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

#define EPD_CS 10
#define EPD_DC      16
#define SRAM_CS     3
#define SD_CS       46
#define EPD_RESET   18 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    17 // can set to -1 to not use a pin (will wait a fixed delay)

#define EPD_WIDTH   200
#define EPD_HEIGHT  200
#define HSPI_MISO 13
#define HSPI_MOSI 11
#define HSPI_SCLK 12
#define HSPI_CS   EPD_CS
// SPIClass * hspi = new SPIClass(FSPI);
// Adafruit_SSD1681 display(200, 200, HSPI_MOSI, HSPI_SCLK, EPD_DC,
//                    EPD_RESET, EPD_CS, SRAM_CS, HSPI_MISO,
//                    EPD_BUSY);
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
    }
    Serial.println("");
    buttonStatus = (char *)pData;
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
  // pinMode(HSPI_CS, OUTPUT); //HSPI SS
  // hspi = new SPIClass(FSPI);
  // hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); //SCLK, MISO, MOSI, SS

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
  
    // large block of text
  display.clearBuffer();
  display.setCursor(0, 0);
  display.setTextColor(EPD_BLACK);
  display.setTextWrap(true);
  display.print("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur adipiscing ante sed nibh tincidunt feugiat. Maecenas enim massa, fringilla sed malesuada et, malesuada sit amet turpis. Sed porttitor neque ut ante pretium vitae malesuada nunc bibendum. Nullam aliquet ultrices massa eu hendrerit. Ut sed nisi lorem. In vestibulum purus a tortor imperdiet posuere. ");
  // display.display();

  // delay(2000);

  display.clearBuffer();
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, EPD_BLACK);
  }

  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, EPD_BLACK);
  }
  // display.display();

#if defined(FLEXIBLE_213) || defined(FLEXIBLE_290)
  // The flexible displays have different buffers and invert settings!
  display.setBlackBuffer(1, false);
  display.setColorBuffer(1, false);
#endif


  // display.setRotation(2);

  // large block of text
  // display.clearBuffer();
  // display.setTextWrap(true);

  display.setCursor(10, 10);
  display.setTextSize(1);
  display.setTextColor(EPD_BLACK);
  display.print("Get as much education as you can. Nobody can take that away from you");

  display.setCursor(50, 70);
  display.setTextColor(EPD_RED);
  display.print("--Eben Upton");

  pixels.fill(0x00FF00);
  pixels.show();

  // display.display();

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
    delay(1500); // wait half a second
  }
}
