// Based on examples from Adafruit
// USB Host: https://learn.adafruit.com/using-a-mouse-with-usb-host?view=all#arduino
// DVI: https://github.com/adafruit/Adafruit-DVI-HSTX/blob/main/examples/00simpletest/00simpletest.ino
// (Same as https://learn.adafruit.com/adafruit-picowbell-hstx-dvi-output/arduino I think)

#include <Arduino.h>
#include "pio_usb.h"

// USBHost is defined in usbh_helper.h
#include "usbh_helper.h"
#include "tusb.h"
#include "Adafruit_TinyUSB.h"

#include "snes-gamepad.h"
#include "colors.h"
#include "utils.h"

#include <Adafruit_dvhstx.h>

DVHSTXPinout pinConfig = ADAFRUIT_HSTXDVIBELL_CFG;
DVHSTX16 display(pinConfig, DVHSTX_RESOLUTION_320x240, true);

volatile bool dvi_initialized = false;
volatile SnesGamepad_StatusWord_t snes_status;
SnesGamepad_StatusWord_t snes_status_old;

enum
{
  Black,
  Red,
  Yellow,
  Blue,
  Green,
};

void setup1() {
  while(!dvi_initialized) { delay(1); }
  rp2040_configure_pio_usb();
  USBHost.begin(1);
  Serial.println("Core1: USB Host Started");
}

void loop1() {
  USBHost.task();
  Serial.flush();
}

void setup() {
  Serial.begin(115200);

  // while(!Serial);
  // delay(2000);

  if (!display.begin()) { // Blink LED if insufficient RAM
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;)
      digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }
  Serial.println("Core0: Display initialized");
  dvi_initialized = true;
}

void loop() {
  // atomic read (Don't want the USB core changing this on us unexpectedly)
  SnesGamepad_StatusWord_t status;
  status.word = snes_status.word;

  // Only what has changed and is currently pressed
  status.word = (status.word ^ snes_status_old.word) & status.word;

  snes_status_old.word = snes_status.word;

  const int width = 30;
  const int height = 30;
  const int screenWidth = display.width() - (display.width() % width);
  const int screenHeight = display.height() - (display.height() % height);
  static int xPos = screenWidth / 2;
  static int yPos = screenHeight / 2;
  static uint16_t color = WHITE;

  static uint8_t pixelGrid[10][8] = {0};

  if (status.bits.A) {
    color = RED;
  }
  if (status.bits.B) {
    color = YELLOW;
  }
  if (status.bits.X) {
    color = BLUE;
  }
  if (status.bits.Y) {
    color = GREEN;
  }

  if (status.bits.Left) {
    xPos -= 30;
  }
  if (status.bits.Right) {
    xPos += 30;
  }
  if (status.bits.Up) {
    yPos -= 30;
  }
  if (status.bits.Down) {
    yPos += 30;
  }

  if (status.bits.Select) {
    if (color == RED)
      pixelGrid[xPos / width][yPos / height] = Red;
    if (color == GREEN)
      pixelGrid[xPos / width][yPos / height] = Green;
    if (color == BLUE)
      pixelGrid[xPos / width][yPos / height] = Blue;
    if (color == YELLOW)
      pixelGrid[xPos / width][yPos / height] = Yellow;
  }

  xPos = CLAMP(0, xPos, screenWidth);
  yPos = CLAMP(0, yPos, screenHeight);

  display.fillScreen(BLACK);

  // drawRect
  display.fillRect(xPos, yPos, width, height, color);

  // Draw Screen
  for(int i = 0; i < 10; i++)
  {
    for(int j = 0; j < 8; j++)
    {
      switch(pixelGrid[i][j])
      {
        case Black:
          // do nothing
          break;

        case Red:
          display.fillRect(i * width, j * height, width, height, RED);
          break;

        case Yellow:
          display.fillRect(i * width, j * height, width, height, YELLOW);
          break;

        case Blue:
          display.fillRect(i * width, j * height, width, height, BLUE);
          break;

        case Green:
          display.fillRect(i * width, j * height, width, height, GREEN);
          break;
      }

    }
  }

  display.swap();

  sleep_ms(1);
}

//--------------------------------------------------------------------+
// HID Host Callback Functions
//--------------------------------------------------------------------+

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  Serial.printf("HID device mounted (address %d, instance %d)\n", dev_addr, instance);

  // Start receiving HID reports
  if (!tuh_hid_receive_report(dev_addr, instance))
  {
    Serial.printf("Error: cannot request to receive report\n");
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  Serial.printf("HID device unmounted (address %d, instance %d)\n", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  if (len >= 7)
  {
    SnesGamepad_StatusWord_t new_snes_status;

    new_snes_status.bits.Left = (uint8_t)(report[SnesGamepad_Report_XAxis]) < 127;
    new_snes_status.bits.Right = (uint8_t)(report[SnesGamepad_Report_XAxis]) > 127;
    new_snes_status.bits.Up = (uint8_t)(report[SnesGamepad_Report_YAxis]) < 127;
    new_snes_status.bits.Down = (uint8_t)(report[SnesGamepad_Report_YAxis]) > 127;

    new_snes_status.bits.A = report[SnesGamepad_Report_ABXY] & SnesGamepad_Mask_A;
    new_snes_status.bits.B = report[SnesGamepad_Report_ABXY] & SnesGamepad_Mask_B;
    new_snes_status.bits.X = report[SnesGamepad_Report_ABXY] & SnesGamepad_Mask_X;
    new_snes_status.bits.Y = report[SnesGamepad_Report_ABXY] & SnesGamepad_Mask_Y;

    new_snes_status.bits.L = report[SnesGamepad_Report_LR] & SnesGamepad_Mask_L;
    new_snes_status.bits.R = report[SnesGamepad_Report_LR] & SnesGamepad_Mask_R;

    new_snes_status.bits.Start = report[SnesGamepad_Report_StartSelect] & SnesGamepad_Mask_Start;
    new_snes_status.bits.Select = report[SnesGamepad_Report_StartSelect] & SnesGamepad_Mask_Select;

    // Update the global all at once so it's atomic
    snes_status.word = new_snes_status.word;
  }
  else
  {
    Serial.println("I don't think this is a super nintendo controller, less than 7 USB report bytes");
  }

  // Continue to receive the next report
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    Serial.println("Error: cannot request to receive report");
  }
}
