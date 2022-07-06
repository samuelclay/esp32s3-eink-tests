#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_EPD.h"
#include <SPI.h>

#define PIN_NEOPIXEL 48
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#define EPD_CS      15
#define EPD_DC      16
#define SRAM_CS     3
#define SD_CS       46
#define EPD_RESET   -1 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    -1 // can set to -1 to not use a pin (will wait a fixed delay)

#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_SCLK 14
#define HSPI_CS   EPD_CS
SPIClass * hspi = new SPIClass(HSPI);
SPIClass SPI3(HSPI);
// Adafruit_SSD1681 display(200, 200, HSPI_MOSI, HSPI_SCLK, EPD_DC,
//                    EPD_RESET, EPD_CS, SRAM_CS, HSPI_MISO,
//                    EPD_BUSY);
Adafruit_SSD1681 display = Adafruit_SSD1681(200, 200, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY, &SPI3);

void setup() {
  hspi->begin();
  hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); //SCLK, MISO, MOSI, SS
  pinMode(HSPI_CS, OUTPUT); //HSPI SS

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(20); // not so bright

  Serial.begin(115200);
  Serial.print("Hello! EPD Test");

  display.begin();
#if defined(FLEXIBLE_213) || defined(FLEXIBLE_290)
  // The flexible displays have different buffers and invert settings!
  display.setBlackBuffer(1, false);
  display.setColorBuffer(1, false);
#endif

  Serial.println("Initialized");

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

  display.display();

}

void loop() {
  Serial.println("Hello!");
  
  // set color to red
  pixels.fill(0xFF0000);
  pixels.show();
  delay(500); // wait half a second

  // turn off
  pixels.fill(0x00FF00);
  pixels.show();
  delay(500); // wait half a second
}
