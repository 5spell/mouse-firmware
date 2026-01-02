#include "freertos/FreeRTOS.h" // For general FreeRTOS functions if needed
#include "freertos/task.h"     // For task management if needed
#include "esp_system.h"      // For general ESP functions if needed
// #include "spi_flash_mmap.h"  // For flash memory access if needed
#include "esp_log.h"
#include "PMW3360.h"    // Custom sensor library
#include "tinyusb.h"    // HID thing
#include "tusb.h"
#include "class/hid/hid_device.h"

static const char* LOG_TAG = "main.cpp";

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

const spi_bus_config_t spibus_cfg = {
    .mosi_io_num = MOSI_PIN,
    .miso_io_num = MISO_PIN,
    .sclk_io_num = SCLK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
};

const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE()
};


// Return pointer to HID report descriptor
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void) instance;
    // Return pointer to your descriptor
    extern uint8_t const hid_report_descriptor[];
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t* buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;

    if (buffer && reqlen) memset(buffer, 0, reqlen);

    return 0; // zero bytes written
}

// Called when host sends SET_REPORT
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize)
{
    // optional: do nothing
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

void app_main(void) {

    spi_bus_initialize(SPI2_HOST, &spibus_cfg, SPI_DMA_CH_AUTO);

    // instance of `PMW3360` class, represents the sensor
    PMW3360 sens = PMW3360(SPI2_HOST, GPIO_NUM_7);
    esp_err_t sens_init_status = sens.begin();

    uint8_t pid = sens.readReg(REG_Product_ID);
    uint8_t rid = sens.readReg(REG_Revision_ID);
    ESP_LOGI(LOG_TAG, "PMW3360 PID=0x%02X revID=0x%02X", pid, rid);
    ESP_LOGI(LOG_TAG, "Sensor init status: %s", esp_err_to_name(sens_init_status));

    tud_init(0);

    while (true) {
        tud_task();

        PMW3360_DATA sens_data = sens.readBurst();
        
        ESP_LOGI(LOG_TAG, "Sensor X | Y | tud_mounted() : \t %d \t | \t %d \t | \t %s", sens_data.dx, sens_data.dy, tud_mounted() ? "true" : "false");

        tud_hid_mouse_report(0, 0, 1, 1, 0, 1);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

}
