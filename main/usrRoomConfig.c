#include "usrRoomConfig.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "RoomCfg";
static uint8_t g_room_id = 0;  // Cache

/**
 * @brief MAC adresinden otomatik Room ID hesaplar
 * 
 * Algoritma: MAC'in son byte'ını kullanarak 1-7 arası değer üretir
 * Örnek: MAC = XX:XX:XX:XX:XX:A5 → (0xA5 % 7) + 1 = 6
 */
static uint8_t calculateRoomIDFromMAC(const uint8_t mac[6])
{
    // Son byte'ı kullan
    uint8_t last_byte = mac[5];
    
    // 1-7 arası değer üret
    uint8_t room_id = (last_byte % MAX_ROOM_ID) + 1;
    
    ESP_LOGI(TAG, "MAC-based Room ID calculation:");
    ESP_LOGI(TAG, "  MAC Last Byte: 0x%02X", last_byte);
    ESP_LOGI(TAG, "  Calculated Room ID: %d", room_id);
    
    return room_id;
}

uint8_t getRoomIDFromUID(void)
{
    // Cache kontrolü
    if (g_room_id != 0) {
        return g_room_id;
    }
    
    // Önce NVS'i kontrol et (manuel ayarlanmış mı?)
    g_room_id = getRoomIDFromNVS();
    if (g_room_id != 0) {
        ESP_LOGI(TAG, "✅ Room ID from NVS: %d (manually configured)", g_room_id);
        return g_room_id;
    }
    
    // NVS'de yoksa MAC'den hesapla
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to read MAC: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "Using default Room ID: 1");
        g_room_id = 1;
        return g_room_id;
    }
    
    ESP_LOGI(TAG, "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // MAC'den otomatik hesapla
    g_room_id = calculateRoomIDFromMAC(mac);
    
    ESP_LOGI(TAG, "✅ Auto-assigned Room ID: %d (from MAC address)", g_room_id);
    ESP_LOGI(TAG, "💡 Tip: You can change this via NVS or UI");
    
    return g_room_id;
}

esp_err_t setRoomIDToNVS(uint8_t room_id)
{
    if (room_id < MIN_ROOM_ID || room_id > MAX_ROOM_ID) {
        ESP_LOGE(TAG, "❌ Invalid Room ID: %d (must be %d-%d)", 
                 room_id, MIN_ROOM_ID, MAX_ROOM_ID);
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("room_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_u8(nvs_handle, "room_id", room_id);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        g_room_id = room_id;  // Cache güncelle
        ESP_LOGI(TAG, "✅ Room ID %d saved to NVS", room_id);
    } else {
        ESP_LOGE(TAG, "❌ Failed to save Room ID: %s", esp_err_to_name(err));
    }
    
    return err;
}

uint8_t getRoomIDFromNVS(void)
{
    nvs_handle_t nvs_handle;
    uint8_t room_id = 0;
    
    esp_err_t err = nvs_open("room_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return 0;
    }
    
    err = nvs_get_u8(nvs_handle, "room_id", &room_id);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK && room_id >= MIN_ROOM_ID && room_id <= MAX_ROOM_ID) {
        return room_id;
    }
    
    return 0;
}

void getRoomName(uint8_t room_id, char *buffer, size_t size)
{
    // Generic isim
    snprintf(buffer, size, "Room %d", room_id);
}

void printRoomInfo(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    uint8_t room_id = getRoomIDFromUID();
    uint8_t nvs_room_id = getRoomIDFromNVS();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ROOM CONFIGURATION");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Device MAC     : %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "  Current Room ID: %d", room_id);
    
    if (nvs_room_id != 0) {
        ESP_LOGI(TAG, "  Source         : NVS (Manual)");
    } else {
        ESP_LOGI(TAG, "  Source         : MAC (Auto)");
    }
    
    char room_name[32];
    getRoomName(room_id, room_name, sizeof(room_name));
    ESP_LOGI(TAG, "  Room Name      : %s", room_name);
    ESP_LOGI(TAG, "========================================");
}