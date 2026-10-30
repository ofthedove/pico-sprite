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
  HistoryDepth = 100, // Max number of times you can undo

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

typedef uint8_t Color_t;

typedef struct {
  uint8_t xPos;
  uint8_t yPos;
  Color_t prevColor;
} HistoryItem_t;

// Buffer needs to be one bigger than HistoryDepth b/c one slot must remain empty
// (b/c is head == tail full or empty? We decide empty, always leave one slot empty, snake doesn't get to eat it's own tail)
static_assert((HistoryDepth + 1) <= INT8_MAX);
HistoryItem_t history[HistoryDepth + 1];
uint8_t nextHistoryItem = 0;
uint8_t oldestHistoryItem = 0;
uint8_t redoDoneNextHistoryIndex = 0;

/*!
 * @param value The value to be incremented
 * @param max Value is allowed to be equal to this, but not to exceed it
 * @return value + 1 if value was less than max. If value is equal to max, returns 0
*/
uint8_t IncrementWithWrap(uint8_t value, uint8_t max)
{
  if(value == max)
  {
    value = 0;
  }
  else
  {
    value++;
  }

  return value;
}

uint8_t HistoryPush(void)
{
  // Since next always points to the next unused space, knowing what to return is easy
  // The hard part is figuring out what the new value of next should be
  uint8_t returnValue = nextHistoryItem;

  nextHistoryItem = IncrementWithWrap(nextHistoryItem, HistoryDepth);

  // Because "next" always points to the next unused space, there will always be at least 1 space
  //  in the buffer that can't be used. This is required to maintain the convention that if
  //  next == tail the buffer is empty.
  if(nextHistoryItem == oldestHistoryItem)
  {
    oldestHistoryItem = IncrementWithWrap(oldestHistoryItem, HistoryDepth);
  }

  redoDoneNextHistoryIndex = nextHistoryItem;

  Serial.printf("History Push next: %d oldest: %d redo: %d\r\n", nextHistoryItem, oldestHistoryItem, redoDoneNextHistoryIndex);

  return returnValue;
}

bool HistoryPop(uint8_t *value)
{
  if(nextHistoryItem == oldestHistoryItem)
  {
    // Buffer is empty, you can't pop!
    return false;
  }

  if(nextHistoryItem == 0)
  {
    nextHistoryItem = HistoryDepth;
  }
  else
  {
    nextHistoryItem--;
  }

  Serial.printf("History Pop next: %d oldest: %d redo: %d\r\n", nextHistoryItem, oldestHistoryItem, redoDoneNextHistoryIndex);

  *value = nextHistoryItem;
  return true;
}

bool HistoryUnPop(uint8_t *value)
{
  if(nextHistoryItem == redoDoneNextHistoryIndex)
  {
    return false;
  }

  *value = nextHistoryItem;
  nextHistoryItem = IncrementWithWrap(nextHistoryItem, HistoryDepth);

  Serial.printf("History UnPop next: %d oldest: %d redo: %d\r\n", nextHistoryItem, oldestHistoryItem, redoDoneNextHistoryIndex);

  return true;
}

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
    if (pixelGrid[xPos][yPos] != color)
    {
      uint8_t historyIndex = HistoryPush();
      history[historyIndex] = {
        .xPos = xPos,
        .yPos = yPos,
        .prevColor = pixelGrid[xPos][yPos],
      };

      // Serial.printf("History add Head %d color %d\r\n", historyIndex, color);

      pixelGrid[xPos][yPos] = color;
    }
  }

  if (pressed.L || held.L)
  {
    // Serial.printf("History pop attempt\r\n");

    uint8_t historyIndex;

    if(HistoryPop(&historyIndex))
    {
      // We don't just restore the prev color, we swap the color between the grid and the history, so we can un-pop later
      HistoryItem_t *restore = &history[historyIndex];
      Color_t swapColor = pixelGrid[restore->xPos][restore->yPos];
      Serial.printf("Pop color grid: %d history: %d\r\n", swapColor, restore->prevColor);
      pixelGrid[restore->xPos][restore->yPos] = restore->prevColor;
      restore->prevColor = swapColor;

      // Serial.printf("History pop Head %d color %d\r\n", historyIndex, restore.prevColor);
    }
  }

  if (pressed.R || held.R)
  {
    // Serial.printf("History unpop attempt\r\n");

    uint8_t historyIndex;

    if(HistoryUnPop(&historyIndex))
    {
      // Since we swapped these on the way down the stack, now we swap them back.
      HistoryItem_t *restore = &history[historyIndex];
      Color_t swapColor = pixelGrid[restore->xPos][restore->yPos];
      Serial.printf("Unpop color grid: %d history: %d\r\n", swapColor, restore->prevColor);
      pixelGrid[restore->xPos][restore->yPos] = restore->prevColor;
      restore->prevColor = swapColor;

      // Serial.printf("History unp Head %d color %d\r\n", historyIndex, restore.prevColor);
    }
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
