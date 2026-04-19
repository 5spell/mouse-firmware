#include "common/tusb_types.h"
#include "driver/spi_common.h"
#include "esp_system.h"        // For general ESP functions if needed
#include "freertos/FreeRTOS.h" // For general FreeRTOS functions if needed
#include "freertos/task.h"     // For task management if needed
// #include "spi_flash_mmap.h"  // For flash memory access if needed
#include "PMW3360.h" // Custom sensor library
#include "class/hid/hid_device.h"
#include "esp_log.h"
#include "tinyusb.h" // HID thing
#include "tusb.h"

static const char *LOG_TAG = "main.cpp";

#define MOSI_PIN GPIO_NUM_5
#define MISO_PIN GPIO_NUM_6
#define SCLK_PIN GPIO_NUM_4

extern "C" {
/**
 * The main application entry point for ESP-IDF.
 * This function is called by the bootloader and FreeRTOS startup code.
 * It must have C linkage to be found by the linker.
 */
void app_main(void);
}

spi_bus_config_t spibus_cfg = {};

const uint8_t hid_report_descriptor[] = {TUD_HID_REPORT_DESC_MOUSE()};

void configure_spi_bus(spi_bus_config_t *spibus_cfg) {
  spibus_cfg->mosi_io_num = MOSI_PIN;
  spibus_cfg->miso_io_num = MISO_PIN;
  spibus_cfg->sclk_io_num = SCLK_PIN;
  spibus_cfg->quadwp_io_num = -1;
  spibus_cfg->quadhd_io_num = -1;
}

// Return pointer to HID report descriptor
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void)instance;
  // Return pointer to your descriptor
  extern uint8_t const hid_report_descriptor[];
  return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
  (void)instance;
  (void)report_id;
  (void)report_type;

  if (buffer && reqlen)
    memset(buffer, 0, reqlen);

  return 0; // zero bytes written
}

// Called when host sends SET_REPORT
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
  // optional: do nothing
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)bufsize;
}

extern "C" {

// ---- Device Descriptor ----
uint8_t const *tud_descriptor_device_cb(void) {
  static tusb_desc_device_t const desc_device = {
      .bLength = sizeof(tusb_desc_device_t),
      .bDescriptorType = TUSB_DESC_DEVICE,
      .bcdUSB = 0x0200,
      .bDeviceClass = 0x00,
      .bDeviceSubClass = 0x00,
      .bDeviceProtocol = 0x00,
      .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
      .idVendor = 0xCAFE,  // replace with your VID
      .idProduct = 0x4004, // replace with your PID
      .bcdDevice = 0x0100,
      .iManufacturer = 0x01,
      .iProduct = 0x02,
      .iSerialNumber = 0x03,
      .bNumConfigurations = 0x01};
  return (uint8_t const *)&desc_device;
}

// ---- Configuration Descriptor ----
#define EPNUM_HID 0x81 // EP1 IN

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;

  static uint8_t const desc_config[] = {
      // Config header
      TUD_CONFIG_DESCRIPTOR(
          1,                                      // config number
          1,                                      // interface count
          0,                                      // string index
          TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, // total length
          0x00, // attributes (bus powered, no remote wakeup)
          100   // power in mA
          ),
      // HID interface
      TUD_HID_DESCRIPTOR(
          0,                     // interface number
          0,                     // string index
          HID_ITF_PROTOCOL_NONE, // protocol (none = boot protocol not needed)
          sizeof(hid_report_descriptor), EPNUM_HID,
          CFG_TUD_HID_EP_BUFSIZE, // EP size
          10                      // polling interval in ms
          )};

  return desc_config;
}

// ---- String Descriptors ----
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;

  static uint16_t desc_str[32];
  uint8_t len;

  if (index == 0) {
    // Language ID: English
    desc_str[1] = 0x0409;
    len = 1;
  } else {
    const char *str;
    switch (index) {
    case 1:
      str = "YourName";
      break; // Manufacturer
    case 2:
      str = "ESP32 Mouse";
      break; // Product
    case 3:
      str = "000001";
      break; // Serial
    default:
      return NULL;
    }
    len = strlen(str);
    if (len > 31)
      len = 31;
    for (uint8_t i = 0; i < len; i++) {
      desc_str[1 + i] = str[i]; // ASCII to UTF-16
    }
  }

  // Header: length + string descriptor type
  desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
  return desc_str;
}

} // extern "C"

void app_main(void) {

  configure_spi_bus(&spibus_cfg);

  spi_bus_initialize(SPI2_HOST, &spibus_cfg, SPI_DMA_CH_AUTO);

  // instance of `PMW3360` class, represents the sensor
  PMW3360 sens = PMW3360(SPI2_HOST, GPIO_NUM_7);
  esp_err_t sens_init_status = sens.begin();

  uint8_t pid = sens.readReg(REG_Product_ID);
  uint8_t rid = sens.readReg(REG_Revision_ID);
  ESP_LOGI(LOG_TAG, "PMW3360 PID=0x%02X revID=0x%02X", pid, rid);
  ESP_LOGI(LOG_TAG, "Sensor init status: %s",
           esp_err_to_name(sens_init_status));

  tud_init(0);

  while (true) {
    tud_task();

    // PMW3360_DATA sens_data = sens.readBurst();

    // ESP_LOGI(LOG_TAG,
    //          "Sensor X | Y | tud_mounted() : \t %d \t | \t %d \t | \t %s",
    //          sens_data.dx, sens_data.dy, tud_mounted() ? "true" : "false");

    tud_hid_mouse_report(0, 0, 1, 1, 0, 1);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
