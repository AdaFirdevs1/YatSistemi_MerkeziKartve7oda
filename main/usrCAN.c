#include "usrGeneral.h"
#include "usrLightingPage.h"
#include "usrCanDynamicIDMaster.h"

static const char *tag = "CentralCAN";
static TaskHandle_t s_canReceiveTaskHandle;
static TaskHandle_t s_canMasterTaskHandle;

void initCAN(void)
{
    ESP_LOGI("usrCanInit", "========================================");
    ESP_LOGI("usrCanInit", "  CENTRAL MASTER CAN BUS INIT");
    ESP_LOGI("usrCanInit", "========================================");
    
    ESP_LOGI("usrCanInit", "Configuring GPIO pins...");
    ESP_LOGI("usrCanInit", "  CAN_TX: GPIO%d", CAN_TX_PIN);
    ESP_LOGI("usrCanInit", "  CAN_RX: GPIO%d", CAN_RX_PIN);
    
    // GPIO configuration
    gpio_config_t io_conf = {};
    
    // TX pin (output)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CAN_TX_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // RX pin (input)
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << CAN_RX_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Full cleanup
    twai_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    twai_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    ESP_LOGI("usrCanInit", "Installing CAN driver...");
    
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX_PIN, 
        CAN_RX_PIN, 
        TWAI_MODE_NORMAL
    );
    
    g_config.tx_queue_len = 30;
    g_config.rx_queue_len = 30;
    g_config.alerts_enabled = TWAI_ALERT_ALL;
    
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE("usrCanInit", "❌ CAN driver install failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI("usrCanInit", "✅ CAN driver installed");

    ret = twai_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE("usrCanInit", "❌ CAN start failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI("usrCanInit", "✅ CAN started");

    vTaskDelay(pdMS_TO_TICKS(200));
    
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);
    
    ESP_LOGI("usrCanInit", "========================================");
    ESP_LOGI("usrCanInit", "  CAN BUS STATUS");
    ESP_LOGI("usrCanInit", "========================================");
    ESP_LOGI("usrCanInit", "  State: %d (1=RUNNING)", status_info.state);
    ESP_LOGI("usrCanInit", "  TX Errors: %d", status_info.tx_error_counter);
    ESP_LOGI("usrCanInit", "  RX Errors: %d", status_info.rx_error_counter);
    ESP_LOGI("usrCanInit", "========================================");
    
    if (status_info.state == TWAI_STATE_RUNNING)
    {
        ESP_LOGI("usrCanInit", "✅ CAN Bus Ready!");
    }
    
    // CENTRAL MASTER BAŞLAT
    initCentralMaster();
}

esp_err_t sendCanMessage(uint32_t m_canID, uint8_t* data, uint8_t length)
{
    if (length > 8)
    {
        ESP_LOGE(tag, "Invalid CAN message length: %d", length);
        return ESP_ERR_INVALID_ARG;
    }
    
    twai_status_info_t status_info;
    esp_err_t canState = twai_get_status_info(&status_info);

    if (canState != ESP_OK)
    {
        ESP_LOGE(tag, "❌ CAN get_status failed: %s", esp_err_to_name(canState));
        return ESP_FAIL;
    }

    // --- DOĞRU VE GÜVENLİ KURTARMA BLOĞU ---
    if (status_info.state == TWAI_STATE_BUS_OFF)
    {
        ESP_LOGW(tag, "🔄 CAN BUS-OFF Durumu! Donanımsal kurtarma başlatılıyor...");
        twai_initiate_recovery(); // Sürücüyü silmez, donanımı güvenlice resetler
        return ESP_FAIL;
    }
    else if (status_info.state == TWAI_STATE_RECOVERING)
    {
        ESP_LOGW(tag, "🔄 CAN kurtarılıyor, bekleniyor...");
        for (int i = 0; i < 10; i++) {
            vTaskDelay(pdMS_TO_TICKS(50));
            twai_get_status_info(&status_info);
            // Kurtarma bitince state STOPPED olur
            if (status_info.state == TWAI_STATE_STOPPED || status_info.state == TWAI_STATE_RUNNING) break;
        }
    }
    
    if (status_info.state == TWAI_STATE_STOPPED)
    {
        ESP_LOGI(tag, "▶️ CAN Durdurulmuş (STOPPED). Yeniden başlatılıyor...");
        twai_start();
    }

    if (status_info.tx_error_counter >= 128)
    {
        ESP_LOGW(tag, "⚠️ CAN TX Hatası çok yüksek, kuyruk temizleniyor...");
        twai_clear_transmit_queue();
    }

    if (status_info.state != TWAI_STATE_RUNNING && status_info.state != TWAI_STATE_STOPPED)
    {
        ESP_LOGE(tag, "❌ CAN Hazır Değil - Durum: %d", status_info.state);
        return ESP_FAIL;
    }
    
    twai_message_t txMessage;
    txMessage.identifier = m_canID;
    txMessage.data_length_code = length;

    if (m_canID > 0x7FF) {
        txMessage.flags = TWAI_MSG_FLAG_EXTD;
        txMessage.extd = 1;
    } else {
        txMessage.flags = TWAI_MSG_FLAG_NONE;
        txMessage.extd = 0;
    }

    for (int i = 0; i < length; i++)
    {
        txMessage.data[i] = data[i];
    }
    
    for (int i = length; i < 8; i++)
    {
        txMessage.data[i] = 0x00;
    }

    //twai_clear_transmit_queue();
    
    esp_err_t status = twai_transmit(&txMessage, pdMS_TO_TICKS(1000));

    if (status != ESP_OK)
    {
        ESP_LOGE(tag, "❌ CAN TX FAILED - ID: 0x%lX", m_canID);
        return status;
    }
    
    ESP_LOGD(tag, "✅ CAN TX OK - ID: 0x%lX, DLC: %d", m_canID, length);

    return ESP_OK;
}

esp_err_t sendCanHeader(uint32_t m_canID, uint32_t m_value)
{
    uint8_t canData[8] = {0};
    canData[0] = (uint8_t)(m_value >> 24);
    canData[1] = (uint8_t)(m_value >> 16);
    canData[2] = (uint8_t)(m_value >> 8);
    canData[3] = (uint8_t)(m_value);

    return sendCanMessage(m_canID, canData, 8);
}

esp_err_t receiveCANMessage(uint32_t *id, uint8_t *data, size_t *dataLen)
{
    twai_message_t m_message;

    if (twai_receive(&m_message, pdMS_TO_TICKS(10)) == ESP_OK)
    {
        if (m_message.extd || (m_message.flags & TWAI_MSG_FLAG_EXTD)) {
            *id = m_message.identifier;
        } else {
            *id = m_message.identifier & 0x7FF;
        }
        
        *dataLen = m_message.data_length_code;

        for (size_t i = 0; i < *dataLen; i++)
        {
            data[i] = m_message.data[i];
        }
        
        ESP_LOGD(tag, "RX: ID=0x%08lX DLC=%d", *id, *dataLen);
        
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void canReceiveTask(void *arg)
{
    ESP_LOGI(tag, "CAN Receive Task started (CENTRAL MASTER MODE)");
    uint32_t id;
    uint8_t data[8];
    size_t dataLen;
    uint32_t rx_count = 0;
    uint32_t last_stats_time = 0;

    while (1)
    {
        if (receiveCANMessage(&id, data, &dataLen) == ESP_OK)
        {
            uint8_t cmd = dataLen > 0 ? data[0] : 0;
            
            // Merkezi protokol mesajları
            if (cmd == CMD_CENTRAL_REGISTER ||
                cmd == CMD_CENTRAL_ROOM_CONFIRM ||
                cmd == CMD_CENTRAL_STATUS_RESPONSE)
            {
                processCentralMasterMessage(id, data, dataLen);
            }
            // Oda master heartbeat'leri
            else if (cmd == CMD_MASTER_HEARTBEAT)  // 0x06
            {
                // CAN_MASTER_HEARTBEAT_ID'den gelen master HB
                // data[1-2] = master_id_16, data[3] = room_id
                uint16_t master_id = ((uint16_t)data[1] << 8) | data[2];
                uint8_t  room_id   = data[3];
                if (room_id >= 1 && room_id <= MAX_ROOM_ID) {
                    handleRoomHeartbeat(room_id, master_id);
                }
            }
            // Node heartbeat'leri (oda içi, ama master'ın room_id'sini içeriyor)
            else if (cmd == CMD_HEARTBEAT)  // 0x04
            {
                uint8_t room_id = GET_ROOM_ID(id);
                if (room_id >= 1 && room_id <= MAX_ROOM_ID) {
                    uint16_t master_id = (dataLen >= 7) ?
                        (((uint16_t)data[5] << 8) | data[6]) : 0;

                    if (master_id == 0) {
                        ESP_LOGD(tag, "HB ignored: master_id=0");
                        vTaskDelay(pdMS_TO_TICKS(10));
                        continue;  // while(1) döngüsüne devam et
                    }

                    handleRoomHeartbeat(room_id, master_id);
                }
            }
        }
        
        // İstatistik (30 saniyede bir)
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - last_stats_time > 30000)
        {
            twai_status_info_t status_info;
            twai_get_status_info(&status_info);
            
            ESP_LOGI(tag, "=== CENTRAL MASTER STATS ===");
            ESP_LOGI(tag, "Total RX: %ld", rx_count);
            ESP_LOGI(tag, "Active Rooms: %d / %d", getActiveRoomCount(), MAX_ROOM_ID);
            ESP_LOGI(tag, "CAN State: %d", status_info.state);
            ESP_LOGI(tag, "===========================");
            
            printRoomMasterStatus();
            
            last_stats_time = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void canMasterTask(void *arg)
{
    ESP_LOGI(tag, "CAN Master Task started (CENTRAL MASTER MODE)");
    
    uint32_t last_central_hb = 0;
    uint32_t last_status_req = 0; // Durum sorgusu için timer
    
    while (1)
    {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Central heartbeat (her 5 saniye)
        if (current_time - last_central_hb >= 5000)
        {
            sendCentralHeartbeat();
            last_central_hb = current_time;
        }
        
        // Aktif odalardan 10 saniyede bir LED ve Röle sayılarını iste
        if (current_time - last_status_req >= 10000)
        {
            for (uint8_t i = 1; i <= MAX_ROOM_ID; i++) {
                if (isRoomActive(i)) {
                    requestRoomStatus(i);
                }
            }
            last_status_req = current_time;
        }
        
        // Room timeout kontrolü
        updateCentralMaster();
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void processSensorData(uint32_t can_id, uint32_t data)
{
    ESP_LOGD(tag, "Sensor data - ID: 0x%lX, Data: %ld", can_id, data);
}

void startCanCommunication(void)
{
    xTaskCreatePinnedToCore(
        canReceiveTask,
        "CAN Receive",
        8192,
        NULL,
        5,
        &s_canReceiveTaskHandle,
        1
    );
    
    xTaskCreatePinnedToCore(
        canMasterTask,
        "CAN Master",
        4096,
        NULL,
        4,
        &s_canMasterTaskHandle,
        1
    );
    
    ESP_LOGI(tag, "CAN communication started - CENTRAL MASTER MODE");
}

void requestNodeStatus(void)
{
    printRoomMasterStatus();
}