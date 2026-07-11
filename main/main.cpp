#include "PMW3360.h"
#include "class/hid/hid_device.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb.h"
#include "tusb_cdc_acm.h" // tusb_cdc_acm_init, tinyusb_config_cdcacm_t
#include "tusb_console.h" // esp_tusb_init_console
#include <stdint.h>

static const char *TAG = "main";

extern "C" {
void app_main(void);
}

#define MOSI_PIN GPIO_NUM_5
#define MISO_PIN GPIO_NUM_6
#define SCLK_PIN GPIO_NUM_4

// --- USB descriptors --------------------------------------------------------

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID,
    ITF_NUM_TOTAL,
};

static const uint8_t hid_report_descriptor[] = {TUD_HID_REPORT_DESC_MOUSE()};

#define TUSB_DESC_TOTAL_LEN                                                    \
    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, 0x81, 8, 0x02, 0x82, 64),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(hid_report_descriptor), 0x83, 16, 10),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {}

// ---------------------------------------------------------------------------

const spi_bus_config_t spibus_cfg = {
    .mosi_io_num = MOSI_PIN,
    .miso_io_num = MISO_PIN,
    .sclk_io_num = SCLK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
};

static volatile struct {
    int16_t dx;
    int16_t dy;
} motion_data;

void app_main(void) {
    spi_bus_initialize(SPI2_HOST, &spibus_cfg, SPI_DMA_CH_AUTO);

    static PMW3360 sens = PMW3360(SPI2_HOST, GPIO_NUM_7);
    esp_err_t sens_init_status = sens.begin(2000);
    uint8_t pid = sens.readReg(REG_Product_ID);
    uint8_t rid = sens.readReg(REG_Revision_ID);

    tinyusb_config_t tusb_cfg = {};
    tusb_cfg.external_phy = false;
    tusb_cfg.self_powered = false;
    tusb_cfg.vbus_monitor_io = -1;
    tusb_cfg.configuration_descriptor = desc_configuration;

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {};
    acm_cfg.usb_dev = TINYUSB_USBDEV_0;
    acm_cfg.cdc_port = TINYUSB_CDC_ACM_0;
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    ESP_ERROR_CHECK(esp_tusb_init_console(TINYUSB_CDC_ACM_0));

    xTaskCreate(
        [](void *arg) {
            PMW3360 *sens = (PMW3360 *)arg;
            while (true) {
                PMW3360_DATA d = sens->readBurst();
                motion_data.dx = d.dx;
                motion_data.dy = d.dy;
                if (tud_hid_ready())
                    tud_hid_mouse_report(0, 0, d.dx, d.dy, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        },
        "hid", 4096, &sens, 3, nullptr);

    // Wait up to 3s for host to open the CDC port; logs written before DTR is
    // asserted are dropped because the host hasn't finished re-enumerating yet.
    for (int i = 0; i < 300 && !tud_cdc_connected(); i++)
        vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "sensor init %s (0x%x)", esp_err_to_name(sens_init_status),
             sens_init_status);
    ESP_LOGI(TAG, "PMW3360 PID=0x%02X revID=0x%02X", pid, rid);

    xTaskCreate(
        [](void *) {
            while (true) {
                ESP_LOGI(TAG, "X | Y : %d | %d", motion_data.dx,
                         motion_data.dy);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        },
        "log", 2048, nullptr, 1, nullptr);
}
