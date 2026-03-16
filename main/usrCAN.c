#include "usrGeneral.h"
#include "usrLightingPage.h"
#include "usrCanDynamicIDMaster.h"

static const char *tag = "usrCan";
static TaskHandle_t s_canReceiveTaskHandle;
static TaskHandle_t s_canMasterTaskHandle;

void initCAN(void)
{
    ESP_LOGI("usrCanInit", "========================================");
    ESP_LOGI("usrCanInit", "  ESP32-S3 CAN BUS INITIALIZATION");
    ESP_LOGI("usrCanInit", "========================================");
    
    // ← DÜZELTME: Önce GPIO pinlerini kontrol et
    ESP_LOGI("usrCanInit", "Configuring GPIO pins...");
    ESP_LOGI("usrCanInit", "  CAN_TX: GPIO%d", CAN_TX_PIN);
    ESP_LOGI("usrCanInit", "  CAN_RX: GPIO%d", CAN_RX_PIN);
    
    // GPIO pinlerini manuel olarak ayarla (güvenlik için)
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
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // ← RX için pull-up
    gpio_config(&io_conf);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Tam temizlik
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
    
    g_config.tx_queue_len = 20;
    g_config.rx_queue_len = 20;
    g_config.alerts_enabled = TWAI_ALERT_ALL;  // ← Tüm alert'leri aktif et
    
    // ← DÜZELTME: Timing ayarları (ESP32-S3 için optimize)
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE("usrCanInit", "❌ CAN driver install failed: %s", esp_err_to_name(ret));
        ESP_LOGE("usrCanInit", "⚠️ Check CAN transceiver connections!");
        ESP_LOGE("usrCanInit", "   - CAN_TX (GPIO%d) → Transceiver TX", CAN_TX_PIN);
        ESP_LOGE("usrCanInit", "   - CAN_RX (GPIO%d) → Transceiver RX", CAN_RX_PIN);
        ESP_LOGE("usrCanInit", "   - Transceiver VCC → 3.3V");
        ESP_LOGE("usrCanInit", "   - Transceiver GND → GND");
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
    ESP_LOGI("usrCanInit", "  State           : %d", status_info.state);
    ESP_LOGI("usrCanInit", "                    0=STOPPED, 1=RUNNING");
    ESP_LOGI("usrCanInit", "                    2=BUS_OFF, 3=RECOVERING");
    ESP_LOGI("usrCanInit", "  TX Errors       : %d", status_info.tx_error_counter);
    ESP_LOGI("usrCanInit", "  RX Errors       : %d", status_info.rx_error_counter);
    ESP_LOGI("usrCanInit", "  TX Failed Count : %lu", status_info.tx_failed_count);
    ESP_LOGI("usrCanInit", "  RX Missed Count : %lu", status_info.rx_missed_count);
    ESP_LOGI("usrCanInit", "  RX Queue        : %lu", status_info.msgs_to_rx);
    ESP_LOGI("usrCanInit", "  TX Queue        : %lu", status_info.msgs_to_tx);
    ESP_LOGI("usrCanInit", "========================================");
    
    if (status_info.state != TWAI_STATE_RUNNING)
    {
        ESP_LOGE("usrCanInit", "❌ CAN NOT RUNNING! State: %d", status_info.state);
        ESP_LOGW("usrCanInit", "⚠️ Possible causes:");
        ESP_LOGW("usrCanInit", "   1. No termination resistor (120Ω)");
        ESP_LOGW("usrCanInit", "   2. CAN transceiver not powered");
        ESP_LOGW("usrCanInit", "   3. Wrong TX/RX pin connections");
        ESP_LOGW("usrCanInit", "   4. Bus wiring issue");
    }
    else
    {
        ESP_LOGI("usrCanInit", "✅ CAN Bus Ready!");
    }
    
    initDynamicIDMaster();
}


// Genel CAN mesaj gönderme fonksiyonu (değişken uzunlukta mesajlar için)
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

    // STOPPED veya BUS_OFF durumunda TAM RESET
    if (status_info.state == TWAI_STATE_STOPPED || 
        status_info.state == TWAI_STATE_BUS_OFF ||
        status_info.tx_error_counter >= 128)
    {
        ESP_LOGW(tag, "🔄 CAN Critical State (State:%d, TEC:%d) - Full Reset", 
                 status_info.state, status_info.tx_error_counter);
        
        // TAM RESET PROSEDÜRÜ
        twai_stop();
        vTaskDelay(pdMS_TO_TICKS(100));
        
        twai_driver_uninstall();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Yeniden kurulum
        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
            CAN_TX_PIN, 
            CAN_RX_PIN, 
            TWAI_MODE_NORMAL
        );
        
        g_config.tx_queue_len = 20;
        g_config.rx_queue_len = 20;
        g_config.alerts_enabled = TWAI_ALERT_TX_FAILED | 
                                 TWAI_ALERT_TX_SUCCESS | 
                                 TWAI_ALERT_RX_DATA |
                                 TWAI_ALERT_ERR_PASS |
                                 TWAI_ALERT_BUS_ERROR |
                                 TWAI_ALERT_BUS_OFF;
        
        twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
        if (ret != ESP_OK)
        {
            ESP_LOGE(tag, "❌ CAN reinstall failed: %s", esp_err_to_name(ret));
            return ESP_FAIL;
        }

        ret = twai_start();
        if (ret != ESP_OK)
        {
            ESP_LOGE(tag, "❌ CAN restart failed: %s", esp_err_to_name(ret));
            return ESP_FAIL;
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
        
        twai_get_status_info(&status_info);
        ESP_LOGI(tag, "✅ CAN Reset Complete - State:%d, TEC:%d, REC:%d", 
                 status_info.state, status_info.tx_error_counter, status_info.rx_error_counter);
        
        if (status_info.state != TWAI_STATE_RUNNING)
        {
            ESP_LOGE(tag, "❌ CAN Still Not Running - State: %d", status_info.state);
            return ESP_FAIL;
        }
    }
    // ← DÜZELTME: RECOVERING durumu için bekle
    else if (status_info.state == TWAI_STATE_RECOVERING)
    {
        ESP_LOGW(tag, "⏳ CAN Recovering - Waiting...");
        
        // 500ms kadar bekle
        for (int i = 0; i < 10; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            twai_get_status_info(&status_info);
            
            if (status_info.state == TWAI_STATE_RUNNING)
            {
                ESP_LOGI(tag, "✅ CAN Recovered");
                break;
            }
        }
        
        if (status_info.state != TWAI_STATE_RUNNING)
        {
            ESP_LOGE(tag, "❌ CAN Recovery timeout - State: %d", status_info.state);
            return ESP_FAIL;
        }
    }

    if (status_info.state != TWAI_STATE_RUNNING)
    {
        ESP_LOGE(tag, "❌ CAN Not Ready - State: %d", status_info.state);
        return ESP_FAIL;
    }

    twai_message_t txMessage;
    txMessage.identifier = m_canID;
    txMessage.data_length_code = length;

    if (m_canID > 0x7FF) {
        // 29-bit Extended ID
        txMessage.flags = TWAI_MSG_FLAG_EXTD;
        txMessage.extd = 1;
    } else {
        // 11-bit Standard ID
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

    
    // Timeout artırıldı (1 saniye)
    esp_err_t status = twai_transmit(&txMessage, pdMS_TO_TICKS(50));

    if (status != ESP_OK)
    {
        if (status == ESP_ERR_TIMEOUT)
        {
            ESP_LOGE(tag, "❌ CAN TX TIMEOUT - ID: 0x%03lX", m_canID);
            
            // Status logla
            twai_get_status_info(&status_info);
            ESP_LOGE(tag, "   State:%d, TEC:%d, REC:%d, TX_Failed:%lu", 
                     status_info.state, 
                     status_info.tx_error_counter,
                     status_info.rx_error_counter,
                     status_info.tx_failed_count);
        }
        else
        {
            ESP_LOGE(tag, "❌ CAN TX FAILED - ID: 0x%03lX, Status: %s", 
                     m_canID, esp_err_to_name(status));
        }
        
        return status;
    }
    
    ESP_LOGD(tag, "✅ CAN TX OK - ID: 0x%03lX, DLC: %d", m_canID, length);

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

    // TIMEOUT'U AZALT - Daha hızlı kontrol
    if (twai_receive(&m_message, pdMS_TO_TICKS(10)) == ESP_OK)
    {
        // Extended ID kontrolü
        if (m_message.extd || (m_message.flags & TWAI_MSG_FLAG_EXTD)) {
            *id = m_message.identifier;  // 29-bit ID
        } else {
            *id = m_message.identifier & 0x7FF;  // 11-bit ID
        }
        
        *dataLen = m_message.data_length_code;

        for (size_t i = 0; i < *dataLen; i++)
        {
            data[i] = m_message.data[i];
        }
        
        ESP_LOGD(tag, "RX: ID=0x%08lX DLC=%d %s", 
                 *id, *dataLen, 
                 (m_message.extd) ? "Extended" : "Standard");
        
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void canReceiveTask(void *arg)
{
    ESP_LOGI(tag, "CAN Receive Task started");
    uint32_t id;
    uint8_t data[8];
    size_t dataLen;
    uint32_t rx_count = 0;
    uint32_t last_stats_time = 0;

    while (1)
    {
        if (receiveCANMessage(&id, data, &dataLen) == ESP_OK)
        {
            rx_count++;
            
            ESP_LOGD(tag, "📥 RX: ID=0x%08lX, DLC=%d, CMD=0x%02X", 
                     id, dataLen, dataLen > 0 ? data[0] : 0);
            
            uint8_t cmd = dataLen > 0 ? data[0] : 0;
            uint8_t room_id_in_msg = GET_ROOM_ID(id);
            
            // ========== MERKEZİ MASTER MESAJLARI ==========
            // CENTRAL_MASTER_BROADCAST = 0xFF00FFFF veya benzeri ID'lerden gelen
            // CMD_CENTRAL_ASSIGN_ROOM (0x11) veya diğer merkezi komutlar
            if (cmd == CMD_CENTRAL_ASSIGN_ROOM ||
                cmd == CMD_CENTRAL_LED_CONTROL ||
                cmd == CMD_CENTRAL_RELAY_CONTROL ||
                cmd == CMD_CENTRAL_STATUS_REQUEST)
            {
                ESP_LOGI(tag, "🟣 Central command: CMD=0x%02X from ID=0x%08lX", cmd, id);
                processCentralCommand(id, data, dataLen);
            }
            // Merkezi master heartbeat
            else if (cmd == 0xFF)
            {
                // Heartbeat içinden central master ID'yi oku
                uint32_t received_central_id = 0;
                if (dataLen >= 5) {
                    received_central_id = ((uint32_t)data[1] << 24) |
                                        ((uint32_t)data[2] << 16) |
                                        ((uint32_t)data[3] << 8) |
                                        data[4];
                }
                
                // Merkezi master değişti mi?
                uint32_t current_central_id = getCentralMasterID();
                
                
                bool central_changed = (received_central_id != 0 && 
                                        current_central_id != 0 &&  
                                        received_central_id != current_central_id);
                
                if (central_changed) {
                    ESP_LOGW(tag, "Central master changed! Old: 0x%08lX, New: 0x%08lX",
                            current_central_id, received_central_id);
                    // Master gerçekten değiştiyse sıfırla
                    resetRoomAssignment(); 
                }
                else if (current_central_id == 0 && received_central_id != 0) {
                    // Sistem yeni açıldı ve Merkezi Master'ı İLK DEFA gördü! Hafızayı silme, sadece kaydet.
                    setCentralMasterID(received_central_id); 
                    ESP_LOGI(tag, "Central master ID registered: 0x%08lX", received_central_id);
                }
                
                updateCentralHeartbeat();
            }

            // Protokol mesajları
            else if (id == CAN_ID_REQUEST_ID ||
                    id == CAN_ID_ASSIGNMENT_ID)
            {
                ESP_LOGI(tag, "🔵 Protocol message detected!");
                processCANMasterMessage(id, data, dataLen);
            }
            // Kendi master heartbeat ID'sinden gelen mesajları atla (loopback)
            else if (id == CAN_MASTER_HEARTBEAT_ID)
            {
                ESP_LOGD(tag, "⏭️ Own master heartbeat ID - skipping (loopback)");
            }
            // Room bazlı - SADECE kendi odası VE oda ID'si atanmışsa
            else if (getMasterRoomID() != 0 && room_id_in_msg == getMasterRoomID())
            {
                uint8_t device_type = GET_DEVICE_TYPE(id);
                if (device_type == DEVICE_TYPE_LED ||
                    device_type == DEVICE_TYPE_RELAY ||
                    device_type == DEVICE_TYPE_SENSOR)
                {
                    if (cmd == CMD_HEARTBEAT) {
                        processCANMasterMessage(id, data, dataLen);
                    } else if (dataLen >= 4) {
                        uint32_t receivedValue = ((uint32_t)data[0] << 24) |
                                            ((uint32_t)data[1] << 16) |
                                            ((uint32_t)data[2] << 8) |
                                            data[3];
                        processSensorData(id, receivedValue);
                    }
                }
            }
            else
            {
                // Room ID'si 0 iken node heartbeat'lerini LOGLA ama işleme
                if (getMasterRoomID() == 0 && cmd == CMD_HEARTBEAT) {
                    ESP_LOGW(tag, "⚠️ Node heartbeat ignored - room not assigned yet: ID=0x%08lX", id);
                } else {
                    ESP_LOGD(tag, "⏭️ Ignored: ID=0x%08lX, Room=%d (my room=%d)", 
                            id, room_id_in_msg, getMasterRoomID());
                }
            }
        }
        
        // İstatistik (10 saniyede bir)
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - last_stats_time > 10000)
        {
            twai_status_info_t status_info;
            twai_get_status_info(&status_info);
            
            ESP_LOGI(tag, "=== CAN STATS ===");
            ESP_LOGI(tag, "Total RX: %ld", rx_count);
            ESP_LOGI(tag, "Active nodes: %d", getActiveNodeCount());
            ESP_LOGI(tag, "CAN State: %d", status_info.state);
            ESP_LOGI(tag, "=================");
            
            printNodeStatus();
            
            last_stats_time = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Master görevleri için ayrı task
static void canMasterTask(void *arg)
{
    ESP_LOGI(tag, "CAN Master Task started");
    
    uint32_t last_master_heartbeat_time = 0;  // Master heartbeat timer
    
    while (1)
    {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Master Heartbeat gönderimi (her 5 saniyede bir)
        if (current_time - last_master_heartbeat_time >= MASTER_HEARTBEAT_INTERVAL_MS)
        {
            sendMasterHeartbeat();
            last_master_heartbeat_time = current_time;
        }
        
        // Heartbeat timeout kontrolü ve diğer master görevleri
        updateDynamicIDMaster();
        
        // Her 100ms'de bir kontrol et
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void processSensorData(uint32_t can_id, uint32_t data)
{
    // Battery sensor responses (0x300-0x303)
    if (can_id >= 0x300 && can_id <= 0x303)
    {
        // Process battery data using the existing function
        usrLightingPage_processCanData(can_id, data);
        return;
    }

    // 4-20mA sensor responses (0x310-0x313)
    if (can_id >= 0x310 && can_id <= 0x313)
    {
        // TODO: Process 4-20mA sensor data when that page is ready
        ESP_LOGI(tag, "4-20mA Sensor %ld: ADC=%ld", can_id - 0x310 + 1, data);
        return;
    }

    // 0-190Ω sensor responses (0x320-0x323)
    if (can_id >= 0x320 && can_id <= 0x323)
    {
        // TODO: Process 0-190Ω sensor data when that page is ready
        ESP_LOGI(tag, "0-190Ω Sensor %ld: ADC=%ld", can_id - 0x320 + 1, data);
        return;
    }

    // 30-240Ω sensor responses (0x330-0x333)
    if (can_id >= 0x330 && can_id <= 0x333)
    {
        // TODO: Process 30-240Ω sensor data when that page is ready
        ESP_LOGI(tag, "30-240Ω Sensor %ld: ADC=%ld", can_id - 0x330 + 1, data);
        return;
    }

    // Unknown sensor ID
    ESP_LOGD(tag, "Sensor data - ID: 0x%lX, Data: %ld", can_id, data);
}

void startCanCommunication(void)
{
    // CAN receive task'ını YÜKSEK priority ile başlat (mesajları kaçırmaması için)
    xTaskCreatePinnedToCore(
        canReceiveTask,
        "CAN Receive",
        8192,  // Stack boyutu artırıldı
        NULL,
        5,  // Priority artırıldı (LVGL ile aynı)
        &s_canReceiveTaskHandle,
        1   // Core 1'de çalıştır
    );
    
    // CAN master task'ını başlat
    xTaskCreatePinnedToCore(
        canMasterTask,
        "CAN Master",
        4096,
        NULL,
        4,  // Orta priority
        &s_canMasterTaskHandle,
        0   // Core 0'de çalıştır
    );
    
    ESP_LOGI(tag, "CAN communication started with Dynamic ID Master");
    ESP_LOGI(tag, "CAN tasks pinned to Core 1");
}



// Node status bilgisi için yardımcı fonksiyon
void requestNodeStatus(void)
{
    printNodeStatus();
}
