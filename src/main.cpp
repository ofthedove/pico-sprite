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
#include "hid_mouse_reports.h"




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



void setup() {
  Serial.begin(115200);



  while(!Serial);
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

  // run host stack on controller (rhport) 1
  // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have most of the
  // host bit-banging processing works done in core1 to free up core0 for other works
  USBHost.begin(1);
  delay(3000);
  Serial.print("USB D+ Pin:");
  Serial.println(PIN_USB_HOST_DP);
  Serial.print("USB 5V Pin:");
  Serial.println(PIN_5V_EN);
}

void loop() {
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

      if (len > 0){

        // // debug print report data
        // Serial.print("Report data: ");
        // for (int i = 0; i < len; i++) {
        //   Serial.print(report[i], BIN);
        //   Serial.print(" ");
        //   // if (i == 0 || i == 3){
        //   //   Serial.print(report[i], HEX);
        //   //   Serial.print(" ");
        //   // }else { // i==1 or i==2
        //   //   Serial.print((int8_t)report[i]);
        //   //   Serial.print(" ");
        //   // }
        // }
        // Serial.println();

        if (len >= 7)
        {
          uint8_t x = report[0];
          uint8_t y = report[1];

          // ABXY are in the upper nibble of the 6th report byte
          bool A = report[5] & 0b0010 << 4;
          bool B = report[5] & 0b0100 << 4;
          bool X = report[5] & 0b0001 << 4;
          bool Y = report[5] & 0b1000 << 4;

          // LR are in the lower nibble of the 7th report byte
          bool L = report[6] & 0b0001;
          bool R = report[6] & 0b0010;

          // Start and Select are in the upper nibble of the 7th report byte
          bool Start = report[6] & 0b0010 << 4;
          bool Select = report[6] & 0b0001 << 4;

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

        // Serial.print("X: ");
        // Serial.print((int8_t)report[1]);
        // Serial.print(" ");
        // Serial.print("Y: ");
        // Serial.print((int8_t)report[2]);
        // Serial.print(" ");

        // if (report[BYTE_BUTTONS] != BUTTON_NEUTRAL){
        //   if ((report[BYTE_BUTTONS] & BUTTON_LEFT) == BUTTON_LEFT){
        //     Serial.print("Left ");
        //   }
        //   if ((report[BYTE_BUTTONS] & BUTTON_RIGHT) == BUTTON_RIGHT){
        //     Serial.print("Right ");
        //   }
        //   if ((report[BYTE_BUTTONS] & BUTTON_MIDDLE) == BUTTON_MIDDLE){
        //     Serial.print("Middle ");
        //   }
        // }
        // Serial.println();
      }

  // Continue to receive the next report
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    Serial.println("Error: cannot request to receive report");
  }
}
