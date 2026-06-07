// Minimal CDC-only TinyUSB bring-up on ESP-IDF 5.5 + esp_tinyusb 1.7.x.
// Goal: confirm the chip can enumerate as a USB CDC device at all on this
// board, before we layer HID and the PMW3360 sensor back on top.
//
// If this works, you'll see a new USB device with idVendor=303a and a CDC
// PID in dmesg shortly after boot. The USJ console (over the same physical
// pins) will go away when TinyUSB takes over the OTG peripheral — but TinyUSB
// CDC itself becomes the new console once `tusb_cdc_acm` is hooked up.

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"  // tusb_cdc_acm_init, tinyusb_config_cdcacm_t
#include "tusb_console.h"  // esp_tusb_init_console

static const char *TAG = "main";

extern "C" {
void app_main(void);
}

void app_main(void) {
  ESP_LOGI(TAG, "boot: about to call tinyusb_driver_install ...");

  // All NULL → esp_tinyusb falls back to the Kconfig-driven default
  // descriptors (CDC, plus HID/MSC if those are enabled in Kconfig).
  tinyusb_config_t tusb_cfg = {};
  tusb_cfg.external_phy = false;
  tusb_cfg.self_powered = false;
  tusb_cfg.vbus_monitor_io = -1;

  esp_err_t err = tinyusb_driver_install(&tusb_cfg);
  ESP_LOGI(TAG, "boot: tinyusb_driver_install -> %s (0x%x)",
           esp_err_to_name(err), err);

  // Initialize the CDC ACM class so the host's cdc_acm driver has something
  // to talk to, then redirect stdout/stderr through it so all later ESP_LOG
  // output appears on whichever /dev/ttyACMn the kernel assigns.
  tinyusb_config_cdcacm_t acm_cfg = {};
  acm_cfg.usb_dev = TINYUSB_USBDEV_0;
  acm_cfg.cdc_port = TINYUSB_CDC_ACM_0;
  acm_cfg.rx_unread_buf_sz = 64;
  ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
  ESP_ERROR_CHECK(esp_tusb_init_console(TINYUSB_CDC_ACM_0));

  while (true) {
    // These now go out the TinyUSB CDC port instead of the (dead) USJ console.
    ESP_LOGI(TAG, "alive");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
