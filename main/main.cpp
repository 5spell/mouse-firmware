#include "PMW3360.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb.h"
#include "tusb_cdc_acm.h" // tusb_cdc_acm_init, tinyusb_config_cdcacm_t
#include "tusb_console.h" // esp_tusb_init_console

static const char *TAG = "main";

extern "C" {
void app_main(void);
}

#define MOSI_PIN GPIO_NUM_5
#define MISO_PIN GPIO_NUM_6
#define SCLK_PIN GPIO_NUM_4

const spi_bus_config_t spibus_cfg = {
    .mosi_io_num = MOSI_PIN,
    .miso_io_num = MISO_PIN,
    .sclk_io_num = SCLK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
};

void app_main(void) {
    spi_bus_initialize(SPI2_HOST, &spibus_cfg, SPI_DMA_CH_AUTO);

    tinyusb_config_t tusb_cfg = {};
    tusb_cfg.external_phy = false;
    tusb_cfg.self_powered = false;
    tusb_cfg.vbus_monitor_io = -1;

    esp_err_t err = tinyusb_driver_install(&tusb_cfg);

    tinyusb_config_cdcacm_t acm_cfg = {};
    acm_cfg.usb_dev = TINYUSB_USBDEV_0;
    acm_cfg.cdc_port = TINYUSB_CDC_ACM_0;
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    ESP_ERROR_CHECK(esp_tusb_init_console(TINYUSB_CDC_ACM_0));

    // Wait up to 3s for host to open the CDC port; logs written before DTR is
    // asserted are dropped because the host hasn't finished re-enumerating yet.
    for (int i = 0; i < 300 && !tud_cdc_connected(); i++)
        vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "boot: tinyusb_driver_install -> %s (0x%x)",
             esp_err_to_name(err), err);

    PMW3360 sens = PMW3360(SPI2_HOST, GPIO_NUM_7);
    esp_err_t sens_init_status = sens.begin();
    ESP_LOGI(TAG, "sensor init %s (0x%x)", esp_err_to_name(sens_init_status),
             sens_init_status);

    while (true) {
        ESP_LOGI(TAG, "alive");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
