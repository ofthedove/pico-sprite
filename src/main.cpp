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

// If you change the resolution, you must change the defines too!
// (Can't use display.width() on anything that sets an array size)
DVHSTX16 display(pinConfig, DVHSTX_RESOLUTION_320x240, true);
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

volatile bool dvi_initialized = false;
volatile SnesGamepad_StatusWord_t snes_status;

enum
{
  Black,
  Red,
  Yellow,
  Blue,
  Green,
  MaxColors,

  HoldRepeatTicks = 25,

  blockWidthPx = 30,
  blockHeightPx = 30,
  screenWidthPx = DISPLAY_WIDTH - (DISPLAY_WIDTH % blockWidthPx),
  screenHeightPx = DISPLAY_HEIGHT - (DISPLAY_HEIGHT % blockHeightPx),
  gridWidth = screenWidthPx / blockWidthPx,
  gridHeight = screenHeightPx / blockHeightPx,
};

const uint16_t colors[MaxColors] = {
  BLACK,
  RED,
  YELLOW,
  BLUE,
  GREEN,
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
  static SnesGamepad_StatusWord_t statusOld = { .word = 0 };

  // atomic read (Don't want the USB core changing this on us unexpectedly)
  SnesGamepad_StatusWord_t status;
  status.word = snes_status.word;

  // On Press Bitmap
  SnesGamepad_StatusWord_t pressed = { .word = 0 };
  pressed.word = (status.word ^ statusOld.word) & status.word;

  statusOld.word = snes_status.word;

  // Held Bitmap (this isn't really working dunno why yet.)
  SnesGamepad_StatusWord_t held = { .word = 0 };

  static uint16_t holdAccumulators[sizeof(status.word) * 8] = { 0 };
  for(uint8_t i = 0; i < sizeof(status.word) * 8; i++)
  {
    if(status.word & (0x1 << i))
    {
      holdAccumulators[i]++;
    }
    else
    {
      holdAccumulators[i] = 0;
    }

    if((holdAccumulators[i] != 0) && (holdAccumulators[i] % HoldRepeatTicks == 0))
    {
      held.word |= 0x1 << i;
    }
  }


  static int8_t xPos = gridWidth / 2;
  static int8_t yPos = gridHeight / 2;
  static uint8_t color = Red;

  static uint8_t pixelGrid[gridWidth][gridHeight] = { 0 };

  if (pressed.Left || held.Left) {
    xPos--;
  }
  if (pressed.Right || held.Right) {
    xPos++;
  }
  if (pressed.Up || held.Up) {
    yPos--;
  }
  if (pressed.Down || held.Down) {
    yPos++;
  }

  xPos = CLAMP(0, xPos, gridWidth - 1);
  yPos = CLAMP(0, yPos, gridHeight - 1);

  if (pressed.A) {
    color = Red;
  }
  if (pressed.B) {
    color = Yellow;
  }
  if (pressed.X) {
    color = Blue;
  }
  if (pressed.Y) {
    color = Green;
  }

  if (status.Select) {
    pixelGrid[xPos][yPos] = color;
  }

  display.fillScreen(BLACK);

  // Draw Screen
  for(int i = 0; i < gridWidth; i++)
  {
    for(int j = 0; j < gridHeight; j++)
    {
      uint8_t thisBlockColor = pixelGrid[i][j];
      display.fillRect(i * blockWidthPx, j * blockHeightPx, blockWidthPx, blockHeightPx, colors[thisBlockColor]);
    }
  }

  // drawRect
  uint16_t xPosPx = xPos * blockWidthPx;
  uint16_t yPosPx = yPos * blockHeightPx;
  display.fillRect(xPosPx, yPosPx, blockWidthPx, blockHeightPx, colors[color]);
  display.drawRect(xPosPx, yPosPx, blockWidthPx, blockHeightPx, WHITE);

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

    new_snes_status.Left = (uint8_t)(report[SnesGamepad_Report_XAxis]) < 127;
    new_snes_status.Right = (uint8_t)(report[SnesGamepad_Report_XAxis]) > 127;
    new_snes_status.Up = (uint8_t)(report[SnesGamepad_Report_YAxis]) < 127;
    new_snes_status.Down = (uint8_t)(report[SnesGamepad_Report_YAxis]) > 127;

    new_snes_status.A = report[SnesGamepad_Report_ABXY] & SnesGamepad_Mask_A;
    new_snes_status.B = report[SnesGamepad_Report_ABXY] & SnesGamepad_Mask_B;
    new_snes_status.X = report[SnesGamepad_Report_ABXY] & SnesGamepad_Mask_X;
    new_snes_status.Y = report[SnesGamepad_Report_ABXY] & SnesGamepad_Mask_Y;

    new_snes_status.L = report[SnesGamepad_Report_LR] & SnesGamepad_Mask_L;
    new_snes_status.R = report[SnesGamepad_Report_LR] & SnesGamepad_Mask_R;

    new_snes_status.Start = report[SnesGamepad_Report_StartSelect] & SnesGamepad_Mask_Start;
    new_snes_status.Select = report[SnesGamepad_Report_StartSelect] & SnesGamepad_Mask_Select;

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
