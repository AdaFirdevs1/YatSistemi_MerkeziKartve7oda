#include "usrGeneral.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{

    /*
    nvs_handle_t h;
    if (nvs_open("can_nodes", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGW(TAG, "⚠️ can_nodes NVS erased!");
    }
    if (nvs_open("room_config", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGW(TAG, "⚠️ room_config NVS erased!");
    }
    */

    // ========== NVS FLASH BAŞLATMA (EN BAŞTA) ==========
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

    /* 
     * NVS'i Manuel Sıfırlamak İçin (test amaçlı):
     * Uncomment yapıp bir kere çalıştırın, sonra tekrar comment edin
     */
    // ESP_LOGW(TAG, "ERASING ALL NVS DATA!");
    //nvs_flash_erase();
    // nvs_flash_init();
    // ESP_LOGI(TAG, "NVS erased and reinitialized");

    // ========== DISPLAY & LCD ==========
    ESP_LOGI(TAG, "Initializing display...");
    waveshare_esp32_s3_rgb_lcd_init();

    // ========== CAN BUS ==========
    ESP_LOGI(TAG, "Initializing CAN bus...");
    initCAN();  // Bu içinde initDynamicIDMaster() çağrılıyor ve NVS'ten node'lar yükleniyor
    startCanCommunication();

    // ========== LVGL UI ==========
    ESP_LOGI(TAG, "Starting LVGL interface...");
    if (lvgl_port_lock(-1))
    {
        usrGraphicalInterface_init();
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "System initialization complete!");
}