#include "usrCanDynamicIDMaster.h"
#include "usrCAN.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_mac.h"
#include "nvs.h"        
#include "nvs_flash.h"


#define NVS_ROOM_NAMESPACE  "room_table"


static const char *TAG = "CentralMaster";
static central_master_state_t g_central_state = {0};

// ========== INITIALIZATION ==========
void initCentralMaster(void)
{
    memset(&g_central_state, 0, sizeof(central_master_state_t));
    loadRoomTableFromNVS();
    
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK)
    {
        g_central_state.central_master_id = ((uint32_t)mac[4] << 8) | mac[5];
    }
    else
    {
        g_central_state.central_master_id = 0xCE00;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  CENTRAL MASTER INITIALIZED");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Central ID: 0x%08lX", g_central_state.central_master_id);
    ESP_LOGI(TAG, "  Max Rooms: %d", MAX_ROOM_ID);
    ESP_LOGI(TAG, "========================================");
}

// ========== HEARTBEAT ==========
void sendCentralHeartbeat(void)
{
    uint8_t hb_data[8] = {0};
    
    hb_data[0] = 0xFF;
    hb_data[1] = (uint8_t)(g_central_state.central_master_id >> 24);
    hb_data[2] = (uint8_t)(g_central_state.central_master_id >> 16);
    hb_data[3] = (uint8_t)(g_central_state.central_master_id >> 8);
    hb_data[4] = (uint8_t)(g_central_state.central_master_id);
    hb_data[5] = g_central_state.active_room_count;
    
    sendCanMessage(CENTRAL_MASTER_BROADCAST, hb_data, 6);
}


// ========== ROOM REGISTER HANDLING ==========
void handleRoomRegister(uint16_t master_id, uint8_t* data, uint8_t dlc)
{
    if (dlc < 3) return;
    
    // MAC son 3 byte'ı al (eğer gönderildiyse)
    bool has_mac = (dlc >= 6);
    uint8_t received_mac[6] = {0};
    if (has_mac) {
        received_mac[3] = data[3];
        received_mac[4] = data[4];
        received_mac[5] = data[5];
    }

    // Odayı öncelikle MAC adresinden bulmaya çalış
    int existing_room = -1;
    for (int i = 0; i < MAX_ROOM_ID; i++)
    {
        if (g_central_state.rooms[i].status == ROOM_STATUS_OFFLINE) continue;

        if (has_mac && g_central_state.rooms[i].mac_verified) {
            // Kayıtlı MAC adresi ile gelen MAC adresini karşılaştır
            if (g_central_state.rooms[i].mac_addr[3] == received_mac[3] &&
                g_central_state.rooms[i].mac_addr[4] == received_mac[4] &&
                g_central_state.rooms[i].mac_addr[5] == received_mac[5]) {
                existing_room = i;
                break;
            }
        } else {
            // Eğer MAC doğrulanmamış eski bir kayıt varsa fallback olarak master_id kullan
            if (g_central_state.rooms[i].master_id == master_id) {
                existing_room = i;
                break;
            }
        }
    }
    
    if (existing_room >= 0)
    {
        // Bilinen cihaz — mevcut kaydı güncelle
        uint8_t room_id = existing_room + 1;
        
        // Cihaz resetlenip yeni bir master_id almış olabilir, eski ID'yi güncelle
        if (g_central_state.rooms[existing_room].master_id != master_id) {
            ESP_LOGW(TAG, "Room %d master_id changed from 0x%04X to 0x%04X", 
                     room_id, g_central_state.rooms[existing_room].master_id, master_id);
            g_central_state.rooms[existing_room].master_id = master_id;
        }

        if (has_mac) {
            memcpy(g_central_state.rooms[existing_room].mac_addr, received_mac, 6);
            g_central_state.rooms[existing_room].mac_verified = true;
        }
        
        g_central_state.rooms[existing_room].last_heartbeat_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        if (g_central_state.rooms[existing_room].status == ROOM_STATUS_LOST)
            g_central_state.rooms[existing_room].status = ROOM_STATUS_WAITING_CONFIRM;
        
        ESP_LOGI(TAG, "Re-registering known room %d (master 0x%04X)", room_id, master_id);
        
        // Hafızaya kaydet
        saveRoomTableToNVS();
        
        // Odaya tekrar numarasını ata
        assignRoomID(master_id, room_id);
    }
    else
    {
        // Yeni cihaz — boş slot bul
        for (int i = 0; i < MAX_ROOM_ID; i++)
        {
            if (g_central_state.rooms[i].status == ROOM_STATUS_OFFLINE)
            {
                uint8_t room_id = i + 1;
                g_central_state.rooms[i].master_id         = master_id;
                g_central_state.rooms[i].assigned_room_id  = room_id;
                g_central_state.rooms[i].status            = ROOM_STATUS_WAITING_CONFIRM;
                g_central_state.rooms[i].last_heartbeat_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                
                if (has_mac) {
                    memcpy(g_central_state.rooms[i].mac_addr, received_mac, 6);
                    g_central_state.rooms[i].mac_verified = true;
                }
                
                // Hafızaya kaydet
                saveRoomTableToNVS();
                
                assignRoomID(master_id, room_id);
                ESP_LOGI(TAG, "New room assigned: Room %d to master 0x%04X", room_id, master_id);
                break;
            }
        }
    }
}

// ========== ROOM ID ASSIGNMENT ==========
void assignRoomID(uint16_t master_id, uint8_t room_id)
{
    uint8_t assign_data[8] = {0};
    
    assign_data[0] = CMD_CENTRAL_ASSIGN_ROOM;
    assign_data[1] = (uint8_t)(master_id >> 8);
    assign_data[2] = (uint8_t)(master_id);
    assign_data[3] = room_id;
    assign_data[4] = (uint8_t)(g_central_state.central_master_id >> 24);
    assign_data[5] = (uint8_t)(g_central_state.central_master_id >> 16);
    assign_data[6] = (uint8_t)(g_central_state.central_master_id >> 8);
    assign_data[7] = (uint8_t)(g_central_state.central_master_id);
    
    esp_err_t result = sendCanMessage(CENTRAL_MASTER_BROADCAST, assign_data, 8);
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "📤 Room %d assignment sent to Master 0x%04X", room_id, master_id);
    } else {
        ESP_LOGE(TAG, "❌ Room assignment failed");
    }
}


// ========== CONFIRMATION HANDLING ==========
void handleRoomConfirm(uint8_t room_id, uint16_t master_id)
{
    if (room_id < 1 || room_id > MAX_ROOM_ID) return;
    
    int idx = room_id - 1;
    
    if (g_central_state.rooms[idx].master_id == master_id)
    {
        g_central_state.rooms[idx].status = ROOM_STATUS_ACTIVE;
        g_central_state.rooms[idx].last_heartbeat_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // ODA ONAYLANDI -> HAFIZAYA YAZ
        saveRoomTableToNVS();
        
        g_central_state.active_room_count = 0;
        for (int i = 0; i < MAX_ROOM_ID; i++) {
            if (g_central_state.rooms[i].status == ROOM_STATUS_ACTIVE) {
                g_central_state.active_room_count++;
            }
        }
        
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "  ✅ ROOM %d CONFIRMED", room_id);
        ESP_LOGI(TAG, "  Master ID: 0x%04X", master_id);
        ESP_LOGI(TAG, "  Active Rooms: %d / %d",
                 g_central_state.active_room_count, MAX_ROOM_ID);
        ESP_LOGI(TAG, "========================================");
    }
    else
    {
        ESP_LOGW(TAG, "Room %d confirm: master_id mismatch! Expected 0x%04X, got 0x%04X",
                 room_id,
                 g_central_state.rooms[idx].master_id,
                 master_id);
    }
}

// ========== ROOM HEARTBEAT HANDLING ==========
// Called when a room master's heartbeat is received (CMD_MASTER_HEARTBEAT 0x06)
// or when a node heartbeat from within a known room is received (CMD_HEARTBEAT 0x04).
void handleRoomHeartbeat(uint8_t room_id, uint16_t master_id)
{
    if (room_id < 1 || room_id > MAX_ROOM_ID) return;
    
    int idx = room_id - 1;
    
    
    if (g_central_state.rooms[idx].status == ROOM_STATUS_OFFLINE)
    {
        // Heartbeat'ten kayıt AÇMA — sadece CMD_CENTRAL_REGISTER kabul et
        ESP_LOGD(TAG, "HB from unregistered room %d (master 0x%04X) - IGNORED (must register first)",
                 room_id, master_id);
        return;  // <-- sadece bunu yap
    }

    // Verify master_id matches (allow 0 = room hasn't been told its master yet)
    if (master_id != 0 &&
        g_central_state.rooms[idx].master_id != 0 &&
        g_central_state.rooms[idx].master_id != master_id)
    {
        ESP_LOGW(TAG, "💓 HB room %d: master mismatch (stored 0x%04X, received 0x%04X)",
                 room_id,
                 g_central_state.rooms[idx].master_id,
                 master_id);
        // Don't update – could be a stale packet from a different device
        return;
    }

    // Update heartbeat timestamp
    g_central_state.rooms[idx].last_heartbeat_time =
        xTaskGetTickCount() * portTICK_PERIOD_MS;

    // If the room was LOST, recover it
    if (g_central_state.rooms[idx].status == ROOM_STATUS_LOST)
    {
        ESP_LOGI(TAG, "✅ Room %d recovered (master 0x%04X)", room_id, master_id);
        g_central_state.rooms[idx].status = ROOM_STATUS_ACTIVE;

        g_central_state.active_room_count = 0;
        for (int i = 0; i < MAX_ROOM_ID; i++) {
            if (g_central_state.rooms[i].status == ROOM_STATUS_ACTIVE)
                g_central_state.active_room_count++;
        }
    }
    else if (g_central_state.rooms[idx].status == ROOM_STATUS_WAITING_CONFIRM)
    {
        // Still waiting for explicit confirm – re-send assignment just in case
        ESP_LOGD(TAG, "💓 HB room %d still waiting confirm, re-sending assignment", room_id);
        assignRoomID(g_central_state.rooms[idx].master_id, room_id);
    }

    ESP_LOGD(TAG, "💓 Room %d heartbeat OK (master 0x%04X)", room_id, master_id);
}

// ========== LED CONTROL TO ROOM ==========
void sendLEDControlToRoom(uint8_t room_id, uint8_t led_channel, bool state, uint16_t brightness)
{
    if (room_id < 1 || room_id > MAX_ROOM_ID) return;
    if (led_channel < 1 || led_channel > 6) return;
    
    int idx = room_id - 1;
    
    if (g_central_state.rooms[idx].status != ROOM_STATUS_ACTIVE) {
        ESP_LOGW(TAG, "Room %d not active, cannot send LED command", room_id);
        return;
    }
    
    uint8_t cmd_data[8] = {0};
    cmd_data[0] = CMD_CENTRAL_LED_CONTROL;
    cmd_data[1] = room_id;
    cmd_data[2] = led_channel;
    cmd_data[3] = state ? 0x11 : 0x10;
    
    if (state) {
        cmd_data[4] = (uint8_t)(brightness >> 8);
        cmd_data[5] = (uint8_t)(brightness & 0xFF);
    }
    
    uint32_t target_id = MAKE_CAN_ID(room_id, DEVICE_TYPE_MASTER, 0);
    esp_err_t result = sendCanMessage(target_id, cmd_data, state ? 6 : 4);
    
    if (result == ESP_OK) {
        g_central_state.rooms[idx].led_states[led_channel - 1] = state;
        if (state)
            g_central_state.rooms[idx].led_brightness[led_channel - 1] = brightness;
        ESP_LOGI(TAG, "📤 LED command: Room %d, LED %d, %s, brightness %d",
                 room_id, led_channel, state ? "ON" : "OFF", brightness);
    } else {
        ESP_LOGE(TAG, "❌ LED command failed");
    }
}

// ========== RELAY CONTROL TO ROOM ==========
void sendRelayControlToRoom(uint8_t room_id, uint8_t relay_channel, bool state)
{
    if (room_id < 1 || room_id > MAX_ROOM_ID) return;
    if (relay_channel < 1 || relay_channel > 6) return;
    
    int idx = room_id - 1;
    
    if (g_central_state.rooms[idx].status != ROOM_STATUS_ACTIVE) {
        ESP_LOGW(TAG, "Room %d not active, cannot send Relay command", room_id);
        return;
    }
    
    uint8_t cmd_data[8] = {0};
    cmd_data[0] = CMD_CENTRAL_RELAY_CONTROL;
    cmd_data[1] = room_id;
    cmd_data[2] = relay_channel;
    cmd_data[3] = state ? 1 : 0;
    
    uint32_t target_id = MAKE_CAN_ID(room_id, DEVICE_TYPE_MASTER, 0);
    esp_err_t result = sendCanMessage(target_id, cmd_data, 4);
    
    if (result == ESP_OK) {
        g_central_state.rooms[idx].relay_states[relay_channel - 1] = state;
        ESP_LOGI(TAG, "📤 Relay command: Room %d, Relay %d, %s",
                 room_id, relay_channel, state ? "ON" : "OFF");
    } else {
        ESP_LOGE(TAG, "❌ Relay command failed");
    }
}

// ========== MESSAGE PROCESSING ==========
void processCentralMasterMessage(uint32_t can_id, uint8_t* data, uint8_t dlc)
{
    if (dlc < 1) return;
    
    uint8_t command = data[0];
    
    switch (command)
    {
        case CMD_CENTRAL_REGISTER:
        {
            
            if (dlc >= 3) {
                uint16_t master_id = ((uint16_t)data[1] << 8) | data[2];
                handleRoomRegister(master_id, data, dlc);
            }
            break;
        }
        
        case CMD_CENTRAL_ROOM_CONFIRM:
        {
            if (dlc >= 4) {
                uint16_t master_id = ((uint16_t)data[1] << 8) | data[2];
                uint8_t room_id    = data[3];
                handleRoomConfirm(room_id, master_id);
            }
            break;
        }
        
        case CMD_CENTRAL_STATUS_RESPONSE:
        {
            if (dlc >= 3) {
                handleRoomStatusResponse(data[1], data, dlc);
            }
            break;
        }
        
        default:
            ESP_LOGD(TAG, "Unknown central command: 0x%02X", command);
            break;
    }
}

// ========== STATUS RESPONSE ==========
void handleRoomStatusResponse(uint8_t room_id, uint8_t* data, uint8_t dlc)
{
    if (room_id < 1 || room_id > MAX_ROOM_ID) return;
    
    int idx = room_id - 1;
    
    if (dlc >= 5) {
        g_central_state.rooms[idx].active_led_count   = data[3];
        g_central_state.rooms[idx].active_relay_count = data[4];
    }

    // A status response also counts as a sign of life
    g_central_state.rooms[idx].last_heartbeat_time =
        xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    ESP_LOGI(TAG, "Room %d Status: LEDs=%d, Relays=%d",
             room_id,
             g_central_state.rooms[idx].active_led_count,
             g_central_state.rooms[idx].active_relay_count);
}

// ========== UPDATE (Heartbeat Timeout) ==========
void updateCentralMaster(void)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    for (int i = 0; i < MAX_ROOM_ID; i++)
    {
        if (g_central_state.rooms[i].status == ROOM_STATUS_OFFLINE) continue;
        
        uint32_t time_since_hb =
            current_time - g_central_state.rooms[i].last_heartbeat_time;
        
        // 30 second timeout
        if (time_since_hb > 30000)
        {
            if (g_central_state.rooms[i].status == ROOM_STATUS_ACTIVE ||
                g_central_state.rooms[i].status == ROOM_STATUS_WAITING_CONFIRM)
            {
                ESP_LOGW(TAG, "⚠️ Room %d TIMEOUT! Last seen: %lu ms ago",
                         i + 1, time_since_hb);
                g_central_state.rooms[i].status = ROOM_STATUS_LOST;
                
                g_central_state.active_room_count = 0;
                for (int j = 0; j < MAX_ROOM_ID; j++) {
                    if (g_central_state.rooms[j].status == ROOM_STATUS_ACTIVE)
                        g_central_state.active_room_count++;
                }
            }
        }
    }
}

// ========== QUERIES ==========
uint8_t getActiveRoomCount(void)
{
    return g_central_state.active_room_count;
}

bool isRoomActive(uint8_t room_id)
{
    if (room_id < 1 || room_id > MAX_ROOM_ID) return false;
    return g_central_state.rooms[room_id - 1].status == ROOM_STATUS_ACTIVE;
}

room_master_info_t* getRoomInfo(uint8_t room_id)
{
    if (room_id < 1 || room_id > MAX_ROOM_ID) return NULL;
    return &g_central_state.rooms[room_id - 1];
}

void printRoomMasterStatus(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "  CENTRAL MASTER STATUS");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Central ID: 0x%08lX", g_central_state.central_master_id);
    ESP_LOGI(TAG, "  Active Rooms: %d / %d",
             g_central_state.active_room_count, MAX_ROOM_ID);
    ESP_LOGI(TAG, "----------------------------------------");
    
    for (int i = 0; i < MAX_ROOM_ID; i++)
    {
        if (g_central_state.rooms[i].status != ROOM_STATUS_OFFLINE)
        {
            const char *status_str =
                (g_central_state.rooms[i].status == ROOM_STATUS_ACTIVE)          ? "ACTIVE"  :
                (g_central_state.rooms[i].status == ROOM_STATUS_WAITING_CONFIRM) ? "WAITING" :
                (g_central_state.rooms[i].status == ROOM_STATUS_LOST)            ? "LOST"    :
                                                                                    "OFFLINE";
            
            ESP_LOGI(TAG, "  Room %d: Master=0x%04X | %s | LEDs=%d Relays=%d",
                     i + 1,
                     g_central_state.rooms[i].master_id,
                     status_str,
                     g_central_state.rooms[i].active_led_count,
                     g_central_state.rooms[i].active_relay_count);
        }
    }
    ESP_LOGI(TAG, "========================================\n");
}

void saveRoomTableToNVS(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_ROOM_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    
    nvs_set_blob(h, "rooms", g_central_state.rooms,
                 sizeof(room_master_info_t) * MAX_ROOM_ID);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Room table saved to NVS");
}

void loadRoomTableFromNVS(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_ROOM_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;

    size_t size = sizeof(room_master_info_t) * MAX_ROOM_ID;
    esp_err_t err = nvs_get_blob(h, "rooms", g_central_state.rooms, &size);
    nvs_close(h);

    if (err == ESP_OK) {
        for (int i = 0; i < MAX_ROOM_ID; i++) {
            // Geçersiz master_id'li hayalet kayıtları temizle
            if (g_central_state.rooms[i].master_id == 0) {
                memset(&g_central_state.rooms[i], 0, sizeof(room_master_info_t));
                g_central_state.rooms[i].status = ROOM_STATUS_OFFLINE;
                continue;
            }

            if (g_central_state.rooms[i].status == ROOM_STATUS_ACTIVE ||
                g_central_state.rooms[i].status == ROOM_STATUS_WAITING_CONFIRM)
            {
                g_central_state.rooms[i].status = ROOM_STATUS_LOST;
                g_central_state.rooms[i].last_heartbeat_time = 0;
            }
        }
        ESP_LOGI(TAG, "Room table loaded from NVS");
    }
}

void requestRoomStatus(uint8_t room_id)
{
    if (room_id < 1 || room_id > MAX_ROOM_ID) return;
    
    int idx = room_id - 1;
    // Sadece aktif odalardan durum iste
    if (g_central_state.rooms[idx].status != ROOM_STATUS_ACTIVE) return;

    uint8_t cmd_data[8] = {0};
    cmd_data[0] = CMD_CENTRAL_STATUS_REQUEST; // 0x15
    cmd_data[1] = room_id;
    
    // MAKE_CAN_ID makrosu ile odanın CAN ID'sini oluşturup mesajı atıyoruz
    uint32_t target_id = MAKE_CAN_ID(room_id, DEVICE_TYPE_MASTER, 0);
    sendCanMessage(target_id, cmd_data, 2);
}