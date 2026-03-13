#include "usrGeneral.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  CENTRAL MASTER CONTROLLER");
    ESP_LOGI(TAG, "========================================");
    
    // ========== NVS FLASH BAŞLATMA ==========
    ESP_LOGI(TAG, "Initializing NVS Flash...");
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS Flash initialized successfully");

    // ========== DISPLAY & LCD ==========
    ESP_LOGI(TAG, "Initializing display...");
    waveshare_esp32_s3_rgb_lcd_init();

    // ========== CAN BUS ==========
    ESP_LOGI(TAG, "Initializing CENTRAL MASTER CAN bus...");
    initCAN();
    startCanCommunication();

    // ========== LVGL UI ==========
    ESP_LOGI(TAG, "Starting LVGL interface...");
    if (lvgl_port_lock(-1))
    {
        usrGraphicalInterface_init();
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "Central Master system initialization complete!");
}