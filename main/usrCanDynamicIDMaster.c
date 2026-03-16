#include "usrCanDynamicIDMaster.h"
#include "usrCAN.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_mac.h"
#include "esp_random.h" 
#include "usrRoomConfig.h"

static const char *TAG = "CANMaster";
static master_state_t g_master_state = {0};

static central_connection_t g_central_connection = {
    .mode = MASTER_MODE_STANDALONE,
    .central_master_id = 0,
    .room_id_assigned = false,
    .last_central_heartbeat = 0
};

// Confirmation Buffer
static bool g_confirmation_part1_received = false;
static uint8_t g_confirmation_buffer[8] = {0};
static uint32_t g_confirmation_time = 0;


#define NVS_NAMESPACE "can_nodes"

// ========== NVS KAYIT YAPISI ==========
typedef struct {
    uint32_t unique_id;
    uint32_t assigned_can_id;
    uint32_t master_id;
    uint8_t  room_id;      
    uint16_t node_offset;  
    node_type_t type;
} nvs_node_t;

// Master ID üretimi
uint32_t generateMasterID(void)
{
    uint8_t mac[6];
    uint16_t master_id = 0;
    
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK)
    {
        master_id = ((uint16_t)mac[4] << 8) | mac[5];
    }
    else
    {
        master_id = (uint16_t)(esp_random() & 0xFFFF);
    }
    
    ESP_LOGI(TAG, "Master ID (16-bit): 0x%04X", master_id);
    return (uint32_t)master_id;
}

uint32_t getMasterID(void)
{
    return g_master_state.master_id;
}

uint8_t getMasterRoomID(void)
{
    return g_master_state.room_id;
}

// Master Heartbeat (Artık Extended ID ile)
void sendMasterHeartbeat(void)
{
    uint8_t heartbeat_data[8] = {0};
    
    heartbeat_data[0] = CMD_MASTER_HEARTBEAT;
    
    uint16_t master_id_16 = (uint16_t)(g_master_state.master_id & 0xFFFF);
    heartbeat_data[1] = (uint8_t)((master_id_16 >> 8) & 0xFF);
    heartbeat_data[2] = (uint8_t)(master_id_16 & 0xFF);
    heartbeat_data[3] = g_master_state.room_id;  // ← Oda ID'si ekle
    
    esp_err_t result = sendCanMessage(CAN_MASTER_HEARTBEAT_ID, heartbeat_data, 4);
    
    static uint8_t master_hb_log_counter = 0;
    if (++master_hb_log_counter >= 10)
    {
        master_hb_log_counter = 0;
        if (result == ESP_OK)
        {
            ESP_LOGD(TAG, "💓 Master HB sent - ID:0x%04X, Room:%d", master_id_16, g_master_state.room_id);
        }
        else
        {
            ESP_LOGE(TAG, "❌ Master HB failed: %s", esp_err_to_name(result));
        }
    }
}

// Helper functions
static int findNodeByUniqueID(uint32_t unique_id)
{
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        if (g_master_state.nodes[i].unique_id == unique_id && 
            g_master_state.nodes[i].status != NODE_STATUS_FREE)
        {
            return i;
        }
    }
    return -1;
}

static int findFreeSlot(void)
{
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        if (g_master_state.nodes[i].status == NODE_STATUS_FREE)
        {
            return i;
        }
    }
    return -1;
}

void handleCentralAssignRoom(uint8_t* data, uint8_t dlc)
{
    if (dlc < 5) return;
    
    // Bizim için mi kontrol et
    uint16_t target_master_id = ((uint16_t)data[1] << 8) | data[2];
    uint16_t my_master_id = (uint16_t)(g_master_state.master_id & 0xFFFF);
    
    if (target_master_id != my_master_id) return;
    
    uint8_t assigned_room_id = data[3];
    uint32_t central_master_id = ((uint32_t)data[4] << 24) |
                                 ((uint32_t)data[5] << 16) |
                                 ((uint32_t)data[6] << 8) |
                                 data[7];
    
    if (assigned_room_id < MIN_ROOM_ID || assigned_room_id > MAX_ROOM_ID) {
        ESP_LOGE(TAG, "❌ Invalid room ID received: %d", assigned_room_id);
        return;
    }
    
    // Oda ID'sini kaydet
    g_master_state.room_id = assigned_room_id;
    g_central_connection.central_master_id = central_master_id;
    g_central_connection.room_id_assigned = true;
    g_central_connection.mode = MASTER_MODE_MANAGED;
    
    // NVS'ye kaydet
    setRoomIDToNVS(assigned_room_id);
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ✅ ROOM ID ASSIGNED BY CENTRAL MASTER");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Assigned Room ID: %d", assigned_room_id);
    ESP_LOGI(TAG, "  Central Master ID: 0x%08lX", central_master_id);
    ESP_LOGI(TAG, "========================================");
    
    // Onay gönder
    sendCentralRoomConfirmation();
}

void sendCentralRoomConfirmation(void)
{
    uint8_t confirm_data[8] = {0};
    
    confirm_data[0] = CMD_CENTRAL_ROOM_CONFIRM;
    
    uint16_t master_id_16 = (uint16_t)(g_master_state.master_id & 0xFFFF);
    confirm_data[1] = (uint8_t)(master_id_16 >> 8);
    confirm_data[2] = (uint8_t)(master_id_16);
    confirm_data[3] = g_master_state.room_id;
    
    // Merkezi master'ın ID'sini de ekle
    confirm_data[4] = (uint8_t)(g_central_connection.central_master_id >> 24);
    confirm_data[5] = (uint8_t)(g_central_connection.central_master_id >> 16);
    confirm_data[6] = (uint8_t)(g_central_connection.central_master_id >> 8);
    confirm_data[7] = (uint8_t)(g_central_connection.central_master_id);
    
    sendCanMessage(CENTRAL_MASTER_BROADCAST, confirm_data, 8);
    
    ESP_LOGI(TAG, "📤 Confirmation sent to CENTRAL_MASTER_BROADCAST=0x%08lX", 
             (uint32_t)CENTRAL_MASTER_BROADCAST);
    ESP_LOGI(TAG, "   Master ID: 0x%04X, Room: %d, Central: 0x%08lX",
             master_id_16, g_master_state.room_id, g_central_connection.central_master_id);
}

void handleCentralRelayControl(uint8_t* data, uint8_t dlc)
{
    // data[1] = target room ID
    // data[2] = Relay node offset (1, 2, 3...)
    // data[3] = command (ON/OFF)
    
    if (dlc < 4) {
        ESP_LOGW(TAG, "Invalid relay control message length: %d", dlc);
        return;
    }
    
    if (data[1] != g_master_state.room_id && data[1] != ROOM_BROADCAST) {
        return; // Bize ait değil
    }
    
    uint16_t target_node_offset = data[2];
    uint8_t relay_cmd = data[3];
    
    // İlgili Relay node'unu bul
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++) {
        node_info_t *node = &g_master_state.nodes[i];
        
        if (node->type == NODE_TYPE_RELAY && 
            node->node_offset == target_node_offset &&
            (node->status == NODE_STATUS_ACTIVE || 
             node->status == NODE_STATUS_ASSIGNED)) {
            
            // Komutu STM32'ye ilet
            uint8_t stm32_cmd[8] = {0};
            stm32_cmd[0] = relay_cmd; // ON/OFF komutu
            
            esp_err_t result = sendCanMessage(node->assigned_can_id, stm32_cmd, 1);
            
            if (result == ESP_OK) {
                ESP_LOGI(TAG, "📨 Relay control forwarded: Room=%d, Node=%d, CMD=%s", 
                         g_master_state.room_id, target_node_offset, 
                         relay_cmd ? "ON" : "OFF");
            } else {
                ESP_LOGE(TAG, "❌ Failed to forward relay control");
            }
            break;
        }
    }
}

void handleCentralStatusRequest(uint8_t* data, uint8_t dlc)
{
    // data[1] = target room ID (0xFF = all rooms)
    
    if (dlc < 2) return;
    
    uint8_t target_room = data[1];
    
    // Bize mi sorulmuş?
    if (target_room != g_master_state.room_id && target_room != ROOM_BROADCAST) {
        return;
    }
    
    // Durum yanıtı hazırla
    uint8_t status_data[8] = {0};
    
    status_data[0] = CMD_CENTRAL_STATUS_RESPONSE;
    status_data[1] = g_master_state.room_id;
    status_data[2] = getActiveNodeCount();
    status_data[3] = getActiveNodeCountByType(NODE_TYPE_LED);
    status_data[4] = getActiveNodeCountByType(NODE_TYPE_RELAY);
    status_data[5] = getActiveNodeCountByType(NODE_TYPE_SENSOR);
    status_data[6] = (g_central_connection.mode == MASTER_MODE_MANAGED) ? 0x01 : 0x00;
    
    // Merkezi master'a yanıt gönder
    uint32_t response_id = MAKE_CAN_ID(g_master_state.room_id, DEVICE_TYPE_MASTER, 0);
    sendCanMessage(response_id, status_data, 7);
    
    ESP_LOGI(TAG, "📤 Status response sent - Room: %d, Nodes: %d", 
             g_master_state.room_id, getActiveNodeCount());
}

void sendCentralRegisterRequest(void)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    uint16_t master_id_16 = (uint16_t)(g_master_state.master_id & 0xFFFF);

    // Tek bir mesajda ID ve Benzersiz MAC (Son 3 byte) gönderimi
    uint8_t register_data[8] = {0};
    register_data[0] = CMD_CENTRAL_REGISTER;
    register_data[1] = (uint8_t)(master_id_16 >> 8);
    register_data[2] = (uint8_t)(master_id_16);
    
    // Sadece benzersiz olan son 3 byte'ı gönderiyoruz
    register_data[3] = mac[3];
    register_data[4] = mac[4];
    register_data[5] = mac[5];

    sendCanMessage(CENTRAL_MASTER_BROADCAST, register_data, 6); // 6 byte yeterli

    ESP_LOGI(TAG, "📤 Register request sent - MasterID: 0x%04X, MAC End: %02X:%02X:%02X",
             master_id_16, mac[3], mac[4], mac[5]);
}

void initDynamicIDMaster(void)
{
    memset(&g_master_state, 0, sizeof(master_state_t));
    
    g_master_state.master_id = generateMasterID();
    
    // ÖNEMLİ: Oda ID'sini NVS'den al, yoksa 0 olarak başla
    uint8_t nvs_room_id = getRoomIDFromNVS();
    
    if (nvs_room_id != 0) {
        // Manuel ayarlanmış veya merkezi master'dan alınmış
        g_master_state.room_id = nvs_room_id;
        g_central_connection.mode = MASTER_MODE_MANAGED;
        g_central_connection.room_id_assigned = true;
        ESP_LOGI(TAG, "✅ Room ID from NVS: %d (Managed Mode)", nvs_room_id);
    } else {
        // Oda ID yok, merkezi master'ı bekle
        g_master_state.room_id = 0;
        g_central_connection.mode = MASTER_MODE_STANDALONE;
        g_central_connection.room_id_assigned = false;
        ESP_LOGI(TAG, "⏳ Waiting for central master to assign Room ID...");
    }
    
    // NVS'ten node'ları yükle
    esp_err_t load_err = loadNodesFromNVS();
    if (load_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load nodes from NVS: %s", esp_err_to_name(load_err));
    }
    
    // Offset'leri başlat
    g_master_state.next_led_offset = 1;
    g_master_state.next_relay_offset = 1;
    g_master_state.next_sensor_offset = 1;
    
    // Mevcut node'lara göre offset'leri ayarla
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++) {
        if (g_master_state.nodes[i].status != NODE_STATUS_FREE) {
            uint16_t offset = g_master_state.nodes[i].node_offset;
            
            switch (g_master_state.nodes[i].type) {
                case NODE_TYPE_LED:
                    if (offset >= g_master_state.next_led_offset) {
                        g_master_state.next_led_offset = offset + 1;
                    }
                    break;
                case NODE_TYPE_RELAY:
                    if (offset >= g_master_state.next_relay_offset) {
                        g_master_state.next_relay_offset = offset + 1;
                    }
                    break;
                case NODE_TYPE_SENSOR:
                    if (offset >= g_master_state.next_sensor_offset) {
                        g_master_state.next_sensor_offset = offset + 1;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  CAN DYNAMIC ID MASTER INITIALIZED");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Master ID     : 0x%04lX", g_master_state.master_id);
    ESP_LOGI(TAG, "  Room ID       : %d", g_master_state.room_id);
    ESP_LOGI(TAG, "  Max Nodes     : %d", MAX_NODES_PER_ROOM);
    ESP_LOGI(TAG, "  Next LED ID   : %d", g_master_state.next_led_offset);
    ESP_LOGI(TAG, "  Next Relay ID : %d", g_master_state.next_relay_offset);
    ESP_LOGI(TAG, "  Next Sensor ID: %d", g_master_state.next_sensor_offset);
    ESP_LOGI(TAG, "========================================");
    
    if (g_master_state.room_id == 0) {
        printRoomInfo();
    }
    
    // Merkezi master'ı dinlemeye başla
    sendCentralRegisterRequest();
}

void updateCentralHeartbeat(void)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t old_central_id = g_central_connection.central_master_id;
    
    // Merkezi master ID gelmiş mi? (heartbeat içinde gönderilmeli)
    // Bu fonksiyon çağrılmadan önce central_master_id güncellenmeli
    
    g_central_connection.last_central_heartbeat = now;
    
    if (g_central_connection.mode == MASTER_MODE_STANDALONE) {
        // Merkezi master yeni bağlandı - kayıt isteği gönder
        ESP_LOGI(TAG, "Central master detected! Sending register request...");
        g_central_connection.mode = MASTER_MODE_MANAGED;
        sendCentralRegisterRequest();
    }
    else if (g_master_state.room_id == 0) {
        // Hala room ID yok, tekrar iste
        sendCentralRegisterRequest();
    }
}


void handleIDRequest(uint32_t unique_id, node_type_t node_type) 
{

    if (g_master_state.room_id == 0) {
        ESP_LOGW(TAG, "⚠️ Room ID not assigned yet! Ignoring ID request from 0x%08lX", unique_id);
        ESP_LOGW(TAG, "   Waiting for central master to assign room ID...");
        return;  // Room ID olmadan node'lara ID atama!
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ID REQUEST RECEIVED");
    
    const char *type_str = 
        (node_type == NODE_TYPE_LED) ? "LED" :
        (node_type == NODE_TYPE_RELAY) ? "RELAY" :
        (node_type == NODE_TYPE_SENSOR) ? "SENSOR" : "UNKNOWN";
    
    ESP_LOGI(TAG, "Unique ID: 0x%08lX, Type: %s", unique_id, type_str);
    
    int node_index = findNodeByUniqueID(unique_id);
    
    if (node_index >= 0)
    {
        node_info_t *node = &g_master_state.nodes[node_index];
        
        if (node->master_id != g_master_state.master_id ||
            node->room_id != g_master_state.room_id)
        {
            ESP_LOGW(TAG, "⚠️ Node was registered by different master/room!");
            ESP_LOGW(TAG, "   Old: Master=0x%04lX, Room=%d", node->master_id, node->room_id);
            ESP_LOGW(TAG, "   New: Master=0x%04lX, Room=%d", g_master_state.master_id, g_master_state.room_id);
            
            removeNodeFromNVS(unique_id);
            node->status = NODE_STATUS_FREE;
            node_index = -1;
        }
        else
        {
            ESP_LOGI(TAG, "✅ Known node reconnecting");
            
            if (node->type != node_type && node_type != NODE_TYPE_UNKNOWN) {
                ESP_LOGW(TAG, "Type change: %d → %d", node->type, node_type);
                node->type = node_type;
                saveNodeToNVS(node);
            }
            
            node->status = NODE_STATUS_PENDING;
            node->retry_count = 0;
            node->last_heartbeat_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        }
    }
    
    if (node_index < 0)
    {
        ESP_LOGI(TAG, "🆕 NEW NODE - Type: %s", type_str);
        
        node_index = findFreeSlot();
        if (node_index < 0) {
            ESP_LOGE(TAG, "❌ No free slots!");
            return;
        }
        
        uint16_t node_offset;
        uint8_t device_type;
        
        switch (node_type) {
            case NODE_TYPE_LED:
                node_offset = g_master_state.next_led_offset++;
                device_type = DEVICE_TYPE_LED;
                break;
            case NODE_TYPE_RELAY:
                node_offset = g_master_state.next_relay_offset++;
                device_type = DEVICE_TYPE_RELAY;
                break;
            case NODE_TYPE_SENSOR:
                node_offset = g_master_state.next_sensor_offset++;
                device_type = DEVICE_TYPE_SENSOR;
                break;
            default:
                ESP_LOGE(TAG, "❌ Unknown node type!");
                return;
        }
        
        uint32_t assigned_can_id = MAKE_CAN_ID(
            g_master_state.room_id, 
            device_type, 
            node_offset
        );
        
        node_info_t *node = &g_master_state.nodes[node_index];
        node->unique_id = unique_id;
        node->assigned_can_id = assigned_can_id;
        node->room_id = g_master_state.room_id;
        node->node_offset = node_offset;
        node->master_id = g_master_state.master_id;
        node->status = NODE_STATUS_PENDING;
        node->type = node_type;
        node->retry_count = 0;
        node->last_heartbeat_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        saveNodeToNVS(node);
        
        ESP_LOGI(TAG, "✅ Node registered:");
        ESP_LOGI(TAG, "   UID       : 0x%08lX", unique_id);
        ESP_LOGI(TAG, "   CAN ID    : 0x%08lX", assigned_can_id);
        ESP_LOGI(TAG, "   Room      : %d", g_master_state.room_id);
        ESP_LOGI(TAG, "   Type      : %s", type_str);
        ESP_LOGI(TAG, "   Offset    : %d", node_offset);
    }
    
    // ← ID atamasını gönder (32-bit CAN ID)
    node_info_t *node = &g_master_state.nodes[node_index];
    
    uint8_t assign_data[8] = {0};
    assign_data[0] = CMD_ASSIGN_ID;
    
    // UID (4 byte)
    assign_data[1] = (uint8_t)(unique_id >> 24);
    assign_data[2] = (uint8_t)(unique_id >> 16);
    assign_data[3] = (uint8_t)(unique_id >> 8);
    assign_data[4] = (uint8_t)(unique_id);
    
    // Assigned CAN ID'nin ilk 3 byte'ı
    assign_data[5] = (uint8_t)(node->assigned_can_id >> 24);
    assign_data[6] = (uint8_t)(node->assigned_can_id >> 16);
    assign_data[7] = (uint8_t)(node->assigned_can_id >> 8);
    
    sendCanMessage(CAN_ID_ASSIGNMENT_ID, assign_data, 8);
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // İkinci mesaj: Kalan byte + Master ID
    uint8_t assign_data2[8] = {0};
    assign_data2[0] = CMD_ASSIGN_ID;
    assign_data2[1] = (uint8_t)(node->assigned_can_id);  // Son byte
    assign_data2[2] = (uint8_t)(g_master_state.master_id >> 8);
    assign_data2[3] = (uint8_t)(g_master_state.master_id);
    assign_data2[4] = g_master_state.room_id;
    
    sendCanMessage(CAN_ID_ASSIGNMENT_ID, assign_data2, 5);
    
    ESP_LOGI(TAG, "📤 Assignment sent - CAN ID: 0x%08lX", node->assigned_can_id);
    ESP_LOGI(TAG, "========================================");
}

void handleIDConfirmation(uint32_t unique_id, uint32_t assigned_can_id_or_partial, uint8_t* data, uint8_t dlc)
{
    // ← İLK MESAJ (DLC=8): UID + CAN ID'nin ilk 3 byte'ı
    if (dlc >= 8 && !g_confirmation_part1_received)
    {
        ESP_LOGD(TAG, "Confirmation Part 1/2 received (DLC=%d)", dlc);
        
        // UID parse et
        uint32_t uid = ((uint32_t)data[1] << 24) |
                       ((uint32_t)data[2] << 16) |
                       ((uint32_t)data[3] << 8) |
                       data[4];
        
        // Bu confirmation bize mi?
        int node_index = findNodeByUniqueID(uid);
        if (node_index < 0) {
            ESP_LOGW(TAG, "Confirmation from unknown node: 0x%08lX", uid);
            return;
        }
        
        // Buffer'a kaydet
        memcpy(g_confirmation_buffer, data, 8);
        g_confirmation_part1_received = true;
        g_confirmation_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        ESP_LOGD(TAG, "Confirmation Part 1 buffered, waiting Part 2...");
        return;
    }
    // ← İKİNCİ MESAJ (DLC=2): Kalan 1 byte
    else if (dlc >= 2 && g_confirmation_part1_received)
    {
        ESP_LOGD(TAG, "Confirmation Part 2/2 received (DLC=%d)", dlc);
        
        // İlk mesajdan UID
        uint32_t uid = ((uint32_t)g_confirmation_buffer[1] << 24) |
                       ((uint32_t)g_confirmation_buffer[2] << 16) |
                       ((uint32_t)g_confirmation_buffer[3] << 8) |
                       g_confirmation_buffer[4];
        
        // İlk mesajdan CAN ID'nin ilk 3 byte'ı
        uint32_t assigned_can_id = ((uint32_t)g_confirmation_buffer[5] << 24) |
                                   ((uint32_t)g_confirmation_buffer[6] << 16) |
                                   ((uint32_t)g_confirmation_buffer[7] << 8);
        
        // İkinci mesajdan son byte
        assigned_can_id |= data[1];
        
        g_confirmation_part1_received = false;  // Reset flag
        
        ESP_LOGI(TAG, "✅ Confirmation complete: UID=0x%08lX, CAN=0x%08lX", 
                 uid, assigned_can_id);
        
        int node_index = findNodeByUniqueID(uid);
        
        if (node_index < 0) {
            ESP_LOGW(TAG, "⚠️ Confirmation from unknown node");
            return;
        }
        
        node_info_t *node = &g_master_state.nodes[node_index];
        
        if (assigned_can_id != node->assigned_can_id) {
            ESP_LOGE(TAG, "❌ CAN ID mismatch!");
            ESP_LOGE(TAG, "   Expected: 0x%08lX", node->assigned_can_id);
            ESP_LOGE(TAG, "   Got     : 0x%08lX", assigned_can_id);
            return;
        }
        
        node->status = NODE_STATUS_ASSIGNED;
        node->last_heartbeat_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        const char *type_str = 
            (node->type == NODE_TYPE_LED) ? "LED" :
            (node->type == NODE_TYPE_RELAY) ? "RELAY" :
            (node->type == NODE_TYPE_SENSOR) ? "SENSOR" : "UNKNOWN";
        
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "  NODE CONFIRMED");
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "  UID     : 0x%08lX", uid);
        ESP_LOGI(TAG, "  CAN ID  : 0x%08lX", assigned_can_id);
        ESP_LOGI(TAG, "  Room    : %d", node->room_id);
        ESP_LOGI(TAG, "  Type    : %s", type_str);
        ESP_LOGI(TAG, "  Active  : %d / %d", getActiveNodeCount(), MAX_NODES_PER_ROOM);
        ESP_LOGI(TAG, "========================================");
    }
    else
    {
        ESP_LOGW(TAG, "Invalid confirmation message! DLC:%d, Part1:%d", 
                 dlc, g_confirmation_part1_received);
        
        if (g_confirmation_part1_received)
        {
            ESP_LOGW(TAG, "Resetting Part 1 flag");
            g_confirmation_part1_received = false;
        }
    }
}


void handleHeartbeat(uint32_t unique_id, uint32_t heartbeat_id, uint32_t received_master_id)
{

    if (g_master_state.room_id == 0)
    {
        ESP_LOGD(TAG, "Heartbeat ignored - room not assigned: UID=0x%08lX", unique_id);
        return;
    }

    int node_index = findNodeByUniqueID(unique_id);
    
    if (node_index < 0) {
        ESP_LOGW(TAG, "💓 Heartbeat from UNREGISTERED node: 0x%08lX", unique_id);
        
        uint16_t expected_master = (uint16_t)(g_master_state.master_id & 0xFFFF);
        uint16_t received_master = (uint16_t)(received_master_id & 0xFFFF);
        
        if (received_master == expected_master || received_master == 0) {
            ESP_LOGI(TAG, "Node was ours, sending REASSIGN");
            
            // Doğrudan daha önce yazılmış hazır fonksiyonu kullanıyoruz:
            sendReassignmentCommand(unique_id);
        } else {
            ESP_LOGD(TAG, "Node belongs to different master (0x%04X)", received_master);
        }
        return;
    }
    
    node_info_t *node = &g_master_state.nodes[node_index];
    
    // ← DÜZELTİLDİ: 32-bit heartbeat ID karşılaştırması
    // Heartbeat ID = Assigned CAN ID ile aynı
    if (heartbeat_id != node->assigned_can_id) {
        ESP_LOGW(TAG, "Invalid heartbeat ID. Expected: 0x%08lX, Got: 0x%08lX",
                 node->assigned_can_id, heartbeat_id);
        return;
    }
    
    uint16_t expected_master = (uint16_t)(g_master_state.master_id & 0xFFFF);
    uint16_t received_master = (uint16_t)(received_master_id & 0xFFFF);
    
    if (received_master != 0 && received_master != expected_master) {
        ESP_LOGW(TAG, "Master ID mismatch! Expected: 0x%04X, Got: 0x%04X",
                 expected_master, received_master);
        
        node->status = NODE_STATUS_LOST;
        removeNodeFromNVS(unique_id);
        
        uint8_t reassign_data[8] = {0};
        reassign_data[0] = CMD_REASSIGN_REQUEST;
        reassign_data[1] = (uint8_t)(unique_id >> 24);
        reassign_data[2] = (uint8_t)(unique_id >> 16);
        reassign_data[3] = (uint8_t)(unique_id >> 8);
        reassign_data[4] = (uint8_t)(unique_id);
        
        sendCanMessage(CAN_BROADCAST_ID, reassign_data, 5);
        return;
    }
    
    bool was_inactive = (node->status == NODE_STATUS_ASSIGNED || node->status == NODE_STATUS_LOST);
    
    if (was_inactive) {
        if (node->status == NODE_STATUS_LOST) {
            ESP_LOGI(TAG, "✅ Node 0x%08lX reconnected!", unique_id);
        } 
        else {
            ESP_LOGI(TAG, "✅ Node 0x%08lX first heartbeat!", unique_id);
        }
        node->status = NODE_STATUS_ACTIVE;
    }
    
    node->last_heartbeat_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    static uint8_t hb_log_counter = 0;
    if (++hb_log_counter >= 50) {
        hb_log_counter = 0;
        ESP_LOGD(TAG, "💓 Heartbeat OK - Node: 0x%08lX, ID: 0x%08lX", 
                 unique_id, node->assigned_can_id);
    }
}

void sendReassignmentCommand(uint32_t unique_id)
{
    uint8_t reassignment_data[8] = {0};
    
    reassignment_data[0] = CMD_REASSIGN_REQUEST;
    reassignment_data[1] = (uint8_t)(unique_id >> 24);
    reassignment_data[2] = (uint8_t)(unique_id >> 16);
    reassignment_data[3] = (uint8_t)(unique_id >> 8);
    reassignment_data[4] = (uint8_t)(unique_id);
    
    sendCanMessage(CAN_ID_ASSIGNMENT_ID, reassignment_data, 5);
    
    ESP_LOGW(TAG, "Reassignment Request sent to UID: 0x%08lX", unique_id);
}

void processCANMasterMessage(uint32_t can_id, uint8_t* data, uint8_t dlc)
{
    if (dlc < 1) {
        ESP_LOGW(TAG, "⚠️ Message too short! DLC=%d", dlc);
        return;
    }
    
    uint8_t command = data[0];
    
    ESP_LOGI(TAG, "📨 Processing master message: CAN=0x%08lX, CMD=0x%02X, DLC=%d", 
             can_id, command, dlc);
    
    switch (command)
    {
        case CMD_REQUEST_ID:
            ESP_LOGI(TAG, "🔹 CMD_REQUEST_ID detected");
            if (can_id == CAN_ID_REQUEST_ID && dlc >= 5)
            {
                uint32_t unique_id = ((uint32_t)data[1] << 24) |
                                     ((uint32_t)data[2] << 16) |
                                     ((uint32_t)data[3] << 8) |
                                     data[4];
                
                node_type_t node_type = NODE_TYPE_UNKNOWN;
                if (dlc >= 6) {
                    node_type = (node_type_t)data[5];
                }
                handleIDRequest(unique_id, node_type);
            }
            break;
            
        case CMD_ID_CONFIRMATION:
            ESP_LOGI(TAG, "🔹 CMD_ID_CONFIRMATION detected! CAN=0x%08lX", can_id);
            if (can_id == CAN_ID_ASSIGNMENT_ID)
            {
                if (dlc >= 5) {
                    uint32_t unique_id = ((uint32_t)data[1] << 24) |
                                         ((uint32_t)data[2] << 16) |
                                         ((uint32_t)data[3] << 8) |
                                         data[4];
                    handleIDConfirmation(unique_id, 0, data, dlc);
                }
            }
            break;
            
        case CMD_HEARTBEAT: 
        {
            ESP_LOGD(TAG, "🔹 CMD_HEARTBEAT detected");
            if (dlc >= 5) {
                uint32_t unique_id = ((uint32_t)data[1] << 24) |
                                     ((uint32_t)data[2] << 16) |
                                     ((uint32_t)data[3] << 8) |
                                     data[4];
                
                uint16_t received_master_id = 0;
                if (dlc >= 7)
                {
                    received_master_id = ((uint16_t)data[5] << 8) | data[6];
                }
                
                handleHeartbeat(unique_id, can_id, (uint32_t)received_master_id);
            }
            break;
        }

        case CMD_MASTER_HEARTBEAT:  // 0x06 - Kendi gönderdiğimiz heartbeat geri geliyor, yoksay
            ESP_LOGD(TAG, "🔹 CMD_MASTER_HEARTBEAT echo ignored (own heartbeat loopback)");
            break;
            
        default:
            ESP_LOGW(TAG, "❓ Unknown command: 0x%02X", command);
            break;
    }
}


void updateDynamicIDMaster(void)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool timeout_detected = false;

    // Room ID atanmamışsa periyodik register isteği gönder
    if (g_master_state.room_id == 0 || 
        g_central_connection.mode == MASTER_MODE_STANDALONE)
    {
        static uint32_t last_register_time = 0;
        uint32_t interval = (g_master_state.room_id == 0) ? 3000 : 10000; // 3sn vs 10sn
        
        if (current_time - last_register_time > interval)
        {
            ESP_LOGI(TAG, "Sending periodic register request (room_id=%d)...", 
                    g_master_state.room_id);
            sendCentralRegisterRequest();
            last_register_time = current_time;
        }
    }
    
    // Merkezi master timeout kontrolü
    if (g_central_connection.mode == MASTER_MODE_MANAGED) {
        uint32_t time_since_central = current_time - g_central_connection.last_central_heartbeat;
        
        if (time_since_central > 30000) {
            ESP_LOGW(TAG, "Central master timeout! Going standalone mode...");
            g_central_connection.mode = MASTER_MODE_STANDALONE;
            // room_id'yi KORUYUN - merkezi master geri gelince aynı room ile devam etsin
            // ama bağlantı kurulamadıysa yeniden kayıt isteği gönderilecek
        }
    }
    
    // Confirmation Part 1 timeout kontrolü
    if (g_confirmation_part1_received)
    {
        if ((current_time - g_confirmation_time) > 1000)  // 1 saniye timeout
        {
            ESP_LOGW(TAG, "Confirmation Part 2 timeout! Resetting.");
            g_confirmation_part1_received = false;
        }
    }
    
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        node_info_t *node = &g_master_state.nodes[i];
        
        if (node->status == NODE_STATUS_FREE) continue;
        
        if (node->status == NODE_STATUS_ACTIVE || node->status == NODE_STATUS_ASSIGNED)
        {
            uint32_t time_since_heartbeat = current_time - node->last_heartbeat_time;
            
            if (time_since_heartbeat > HEARTBEAT_TIMEOUT_MS)
            {
                ESP_LOGW(TAG, "Node timeout! UID: 0x%08lX, CAN: 0x%08lX",
                         node->unique_id, node->assigned_can_id);
                
                node->status = NODE_STATUS_LOST;
                timeout_detected = true;
            }
        }
        
        if (node->status == NODE_STATUS_LOST)
        {
            uint32_t time_since_heartbeat = current_time - node->last_heartbeat_time;
            
            if (time_since_heartbeat > 60000 && node->last_heartbeat_time != 0)
            {
                ESP_LOGI(TAG, "Removing dead node: UID=0x%08lX", node->unique_id);
                
                removeNodeFromNVS(node->unique_id);

                node->status = NODE_STATUS_FREE;
                node->unique_id = 0;
                node->assigned_can_id = 0;
                node->last_heartbeat_time = 0;
                node->retry_count = 0;
            }
        }
    }
    
    if (timeout_detected)
    {
        ESP_LOGI(TAG, "Active nodes: %d / %d", 
                 getActiveNodeCount(), MAX_NODES_PER_ROOM);
    }
}

uint8_t getActiveNodeCount(void)
{
    uint8_t count = 0;
    
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        if (g_master_state.nodes[i].status == NODE_STATUS_ACTIVE ||
            g_master_state.nodes[i].status == NODE_STATUS_ASSIGNED)
        {
            count++;
        }
    }
    
    return count;
}

bool isNodeActive(uint32_t unique_id)
{
    int node_index = findNodeByUniqueID(unique_id);
    if (node_index < 0) return false;
    return g_master_state.nodes[node_index].status == NODE_STATUS_ACTIVE;
}

uint32_t getAssignedID(uint32_t unique_id)
{
    int node_index = findNodeByUniqueID(unique_id);
    if (node_index < 0) return 0;
    return g_master_state.nodes[node_index].assigned_can_id;
}

void printNodeStatus(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "  REGISTERED NODES STATUS (Room %d)", g_master_state.room_id);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Active Nodes: %d / %d", getActiveNodeCount(), MAX_NODES_PER_ROOM);
    ESP_LOGI(TAG, "----------------------------------------");
    
    int count = 0;
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        node_info_t *node = &g_master_state.nodes[i];
        if (node->status != NODE_STATUS_FREE)
        {
            const char *status_str;
            switch (node->status)
            {
                case NODE_STATUS_PENDING:   status_str = "PENDING";  break;
                case NODE_STATUS_ASSIGNED:  status_str = "ASSIGNED"; break;
                case NODE_STATUS_ACTIVE:    status_str = "ACTIVE";   break;
                case NODE_STATUS_LOST:      status_str = "LOST";     break;
                default:                    status_str = "UNKNOWN";  break;
            }
            
            const char *type_str = 
                (node->type == NODE_TYPE_LED) ? "LED" :
                (node->type == NODE_TYPE_RELAY) ? "RELAY" :
                (node->type == NODE_TYPE_SENSOR) ? "SENSOR" : "UNKNOWN";
            
            ESP_LOGI(TAG, "  [%d] UID:0x%08lX | CAN:0x%08lX | %s | %s",
                     ++count, node->unique_id, node->assigned_can_id, type_str, status_str);
        }
    }
    ESP_LOGI(TAG, "========================================\n");
}

// ========== NVS FONKSİYONLARI (GÜNCELLENDİ) ==========

esp_err_t saveNodeToNVS(const node_info_t *node)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    char key[16];
    
    int node_index = -1;
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        if (&g_master_state.nodes[i] == node)
        {
            node_index = i;
            break;
        }
    }
    
    if (node_index < 0)
    {
        nvs_close(nvs_handle);
        return ESP_ERR_NOT_FOUND;
    }

    snprintf(key, sizeof(key), "node%02d", node_index);

    // ← GÜNCELLENDİ: Yeni alanlar eklendi
    nvs_node_t nvs_data = {
        .unique_id = node->unique_id,
        .assigned_can_id = node->assigned_can_id,
        .master_id = node->master_id,
        .room_id = node->room_id,          // ← EKLEME
        .node_offset = node->node_offset,  // ← EKLEME
        .type = node->type
    };

    err = nvs_set_blob(nvs_handle, key, &nvs_data, sizeof(nvs_node_t));
    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
        
        const char *type_str = 
            (node->type == NODE_TYPE_LED) ? "LED" :
            (node->type == NODE_TYPE_RELAY) ? "RELAY" :
            (node->type == NODE_TYPE_SENSOR) ? "SENSOR" : "UNKNOWN";
        
        ESP_LOGD(TAG, "💾 Node saved - UID:0x%08lX, CAN:0x%08lX, Type:%s", 
                 node->unique_id, node->assigned_can_id, type_str);
    } else {
        ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t removeNodeFromNVS(uint32_t unique_id)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    // Node'u bul
    int node_index = findNodeByUniqueID(unique_id);
    if (node_index < 0) {
        nvs_close(nvs_handle);
        return ESP_ERR_NOT_FOUND;
    }

    char key[16];
    snprintf(key, sizeof(key), "node%02d", node_index);

    err = nvs_erase_key(nvs_handle, key);
    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "🗑️ Node removed from NVS: 0x%08lX", unique_id);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t loadNodesFromNVS(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Loading nodes from NVS...");
    int node_count = 0;

    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "node%02d", i);
        
        size_t required_size = sizeof(nvs_node_t);
        nvs_node_t nvs_data;

        err = nvs_get_blob(nvs_handle, key, &nvs_data, &required_size);
        if (err == ESP_OK && required_size == sizeof(nvs_node_t)) 
        {
            node_info_t *node = &g_master_state.nodes[i];

            node->unique_id = nvs_data.unique_id;
            node->assigned_can_id = nvs_data.assigned_can_id;
            node->master_id = nvs_data.master_id;
            node->room_id = nvs_data.room_id;          // ← EKLEME
            node->node_offset = nvs_data.node_offset;  // ← EKLEME
            node->type = nvs_data.type;
            node->status = NODE_STATUS_LOST;
            node->last_heartbeat_time = 0;
            node->retry_count = 0;

            const char *type_str = 
                (node->type == NODE_TYPE_LED) ? "LED" :
                (node->type == NODE_TYPE_RELAY) ? "RELAY" :
                (node->type == NODE_TYPE_SENSOR) ? "SENSOR" : "UNKNOWN";
            
            ESP_LOGI(TAG, "📂 Loaded: UID=0x%08lX, CAN=0x%08lX, Type=%s", 
                     node->unique_id, node->assigned_can_id, type_str);
            node_count++;
        }
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Total %d nodes loaded from NVS", node_count);
    return ESP_OK;
}

bool isNodeActiveByIndex(int index)
{
    if (index < 0 || index >= MAX_NODES_PER_ROOM) return false;
    
    return (g_master_state.nodes[index].status == NODE_STATUS_ACTIVE ||
            g_master_state.nodes[index].status == NODE_STATUS_ASSIGNED);
}

uint32_t getNodeAssignedIDByIndex(int index)
{
    if (index < 0 || index >= MAX_NODES_PER_ROOM) return 0;
    return g_master_state.nodes[index].assigned_can_id;
}

uint32_t getNodeIDByType(node_type_t type)
{
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        if ((g_master_state.nodes[i].status == NODE_STATUS_ACTIVE ||
             g_master_state.nodes[i].status == NODE_STATUS_ASSIGNED) &&
            g_master_state.nodes[i].type == type)
        {
            return g_master_state.nodes[i].assigned_can_id;
        }
    }
    
    ESP_LOGW(TAG, "No active node found for type: %d", type);
    return 0;
}

uint8_t getActiveNodeCountByType(node_type_t type)
{
    uint8_t count = 0;
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++)
    {
        if ((g_master_state.nodes[i].status == NODE_STATUS_ACTIVE ||
             g_master_state.nodes[i].status == NODE_STATUS_ASSIGNED) &&
            g_master_state.nodes[i].type == type)
        {
            count++;
        }
    }
    return count;
}

// Merkezi kontrol komutlarını işle
void processCentralCommand(uint32_t can_id, uint8_t* data, uint8_t dlc)
{
    if (dlc < 1) return;
    
    uint8_t command = data[0];
    
    switch (command) {
        case CMD_CENTRAL_ASSIGN_ROOM:
            handleCentralAssignRoom(data, dlc);
            break;
            
        case CMD_CENTRAL_LED_CONTROL:
            handleCentralLEDControl(data, dlc);
            break;
            
        case CMD_CENTRAL_RELAY_CONTROL:
            handleCentralRelayControl(data, dlc);
            break;
            
        case CMD_CENTRAL_STATUS_REQUEST:
            handleCentralStatusRequest(data, dlc);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown central command: 0x%02X", command);
            break;
    }
}

void handleCentralLEDControl(uint8_t* data, uint8_t dlc)
{
    // data[1] = target room ID
    // data[2] = LED node offset (1, 2, 3...)
    // data[3] = command (ON/OFF/DIM)
    // data[4-7] = value (brightness, color, etc.)
    
    if (data[1] != g_master_state.room_id && data[1] != ROOM_BROADCAST) {
        return; // Bize ait değil
    }
    
    uint16_t target_node_offset = data[2];
    
    // İlgili LED node'unu bul
    for (int i = 0; i < MAX_NODES_PER_ROOM; i++) {
        node_info_t *node = &g_master_state.nodes[i];
        
        if (node->type == NODE_TYPE_LED && 
            node->node_offset == target_node_offset &&
            (node->status == NODE_STATUS_ACTIVE || 
             node->status == NODE_STATUS_ASSIGNED)) {
            
            // Komutu STM32'ye ilet
            uint8_t stm32_cmd[8];
            stm32_cmd[0] = data[3]; // Komut
            memcpy(&stm32_cmd[1], &data[4], 4); // Değer
            
            sendCanMessage(node->assigned_can_id, stm32_cmd, 5);
            
            ESP_LOGI(TAG, "📨 LED control forwarded: Room=%d, Node=%d", 
                     g_master_state.room_id, target_node_offset);
            break;
        }
    }
}

void resetRoomAssignment(void)
{
    ESP_LOGW(TAG, "Resetting room assignment - will re-register with central master");
    
    g_master_state.room_id = 0;
    g_central_connection.room_id_assigned = false;
    g_central_connection.mode = MASTER_MODE_STANDALONE;
    g_central_connection.central_master_id = 0;
    
    // NVS'deki room_id'yi temizle ki eski atama karışmasın
    setRoomIDToNVS(0); // 0 = geçersiz, NVS'i temizler
    
    // Hemen kayıt isteği gönder
    sendCentralRegisterRequest();
}

uint32_t getCentralMasterID(void)
{
    return g_central_connection.central_master_id;
}

void setCentralMasterID(uint32_t central_id)
{
    g_central_connection.central_master_id = central_id;
}