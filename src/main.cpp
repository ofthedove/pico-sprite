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


#include <Adafruit_dvhstx.h>

#if defined(ADAFRUIT_FEATHER_RP2350_HSTX)
DVHSTXPinout pinConfig = ADAFRUIT_FEATHER_RP2350_CFG;
#elif defined(ADAFRUIT_METRO_RP2350)
DVHSTXPinout pinConfig = ADAFRUIT_METRO_RP2350_CFG;
#elif defined(ARDUINO_ADAFRUIT_FRUITJAM_RP2350)
DVHSTXPinout pinConfig = ADAFRUIT_FRUIT_JAM_CFG;
#elif (defined(ARDUINO_RASPBERRY_PI_PICO_2) || defined(ARDUINO_RASPBERRY_PI_PICO_2W))
DVHSTXPinout pinConfig = ADAFRUIT_HSTXDVIBELL_CFG;
#else
// If your board definition has PIN_CKP and related defines,
// DVHSTX_PINOUT_DEFAULT is available
DVHSTXPinout pinConfig = DVHSTX_PINOUT_DEFAULT;
#endif

DVHSTX16 display(pinConfig, DVHSTX_RESOLUTION_320x240);

volatile bool dvi_initialized = false;
volatile SnesGamepad_StatusWord_t snes_status;
SnesGamepad_StatusWord_t snes_status_old;

volatile bool pauseForUsb = false;
volatile bool usbDone = false;

void setup() {
  Serial.begin(115200);



  while(!Serial);
  delay(2000);

  // pauseForUsb = true;
  // while(!usbDone) { delay(1); }

  if (!display.begin()) { // Blink LED if insufficient RAM
    pinMode(LED_BUILTIN, OUTPUT);
    for (;;)
      digitalWrite(LED_BUILTIN, (millis() / 500) & 1);
  }
  Serial.println("display initialized");
  dvi_initialized = true;
}

void setup1() {
  while(!dvi_initialized) { delay(1); }

  // configure pio-usb: defined in usbh_helper.h
  rp2040_configure_pio_usb();

  Serial.println("helper done");

  Serial.print("Current clock speed: ");
  Serial.print(rp2040.f_cpu() / 1000000);
  Serial.println(" MHz");

  // run host stack on controller (rhport) 1
  // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have most of the
  // host bit-banging processing works done in core1 to free up core0 for other works
  USBHost.begin(1);
  Serial.println("begin done");
  delay(3000);
  Serial.print("USB D+ Pin:");
  Serial.println(PIN_USB_HOST_DP);
  Serial.print("USB 5V Pin:");
  Serial.println(PIN_5V_EN);
}

void loop() {
  // atomic read (Don't want the USB core changing this on us unexpectedly)
  SnesGamepad_StatusWord_t status;
  status.word = snes_status.word;

  // Only what has changed and is currently pressed
  status.word = (status.word ^ snes_status_old.word) & status.word;

  snes_status_old.word = snes_status.word;

  if (status.bits.A) {
    display.fillRect(display.width() / 2 + 10, display.height() / 2 - 10, 20, 20, RED);
  }
  if (status.bits.B) {
    display.fillRect(display.width() / 2 - 10, display.height() / 2 + 10, 20, 20, YELLOW);
  }
  if (status.bits.X) {
    display.fillRect(display.width() / 2 - 10, display.height() / 2 - 30, 20, 20, BLUE);
  }
  if (status.bits.Y) {
    display.fillRect(display.width() / 2 - 30, display.height() / 2 - 10, 20, 20, GREEN);
  }

  display.drawLine(random(display.width()), random(display.height()),
                  random(display.width()), random(display.height()),
                  random(65536));
  sleep_ms(1);
}

void loop1() {
  USBHost.task();
  Serial.flush();
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

    Serial.print("Status: ");
    Serial.println(snes_status.word, HEX);

    // if(x < 127) { Serial.print("Left   ");} else
    // if(x > 127) { Serial.print("Right  ");} else
    //             { Serial.print("       ");}

    // if(y < 127) { Serial.print("Up    ");} else
    // if(y > 127) { Serial.print("Down  ");} else
    //             { Serial.print("      ");}

    // if(A) { Serial.print("A ");} else
    //       { Serial.print("  ");}

    // if(B) { Serial.print("B ");} else
    //       { Serial.print("  ");}

    // if(X) { Serial.print("X ");} else
    //       { Serial.print("  ");}

    // if(Y) { Serial.print("Y ");} else
    //       { Serial.print("  ");}

    // if(L) { Serial.print("L ");} else
    //       { Serial.print("  ");}

    // if(R) { Serial.print("R ");} else
    //       { Serial.print("  ");}

    // if(Start) { Serial.print("Start ");} else
    //           { Serial.print("      ");}

    // if(Select) { Serial.print("Select ");} else
    //            { Serial.print("       ");}

    // Serial.println();
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
