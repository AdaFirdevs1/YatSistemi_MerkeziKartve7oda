#ifndef _USR_CAN_DYNAMIC_ID_MASTER_H_
#define _USR_CAN_DYNAMIC_ID_MASTER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "usrRoomConfig.h"  

// ========== PROTOKOL ID'LERİ (usrRoomConfig.h'den gelecek ama tekrar tanımlayalım) ==========
#ifndef CAN_ID_REQUEST_ID
#define CAN_ID_REQUEST_ID           MAKE_CAN_ID(0x00, 0xFF, 0xFFFE)
#define CAN_ID_ASSIGNMENT_ID        MAKE_CAN_ID(0x00, 0xFF, 0xFFFD)
#define CAN_MASTER_HEARTBEAT_ID     MAKE_CAN_ID(0x00, 0xFF, 0xFFFC)
#define CAN_CENTRAL_COMMAND_ID      MAKE_CAN_ID(0xFF, 0xFF, 0xFFFF)
#endif

// ========== ESKİ BROADCAST ID (Geriye uyumluluk için) ==========
#define CAN_BROADCAST_ID            0x3FF    // 11-bit standard ID

// ========== KOMUTLAR ==========
#define CMD_REQUEST_ID              0x01
#define CMD_ASSIGN_ID               0x02
#define CMD_ID_CONFIRMATION         0x03
#define CMD_HEARTBEAT               0x04
#define CMD_REASSIGN_REQUEST        0x05
#define CMD_MASTER_HEARTBEAT        0x06
#define CMD_CENTRAL_CONTROL         0x07  // Merkezi kontrol komutu

// ========== KONFIGÜRASYON ==========
#define MAX_NODES_PER_ROOM          50   // ← DÜZELTİLDİ: Her odada max 50 node
#define HEARTBEAT_TIMEOUT_MS        6000
#define ID_ASSIGNMENT_RETRY_MS      1000
#define MASTER_HEARTBEAT_INTERVAL_MS 5000

// Eski tanımları kaldır - geriye uyumluluk için macro
#define MAX_NODES MAX_NODES_PER_ROOM

// ========== NODE TİPLERİ ==========
typedef enum {
    NODE_TYPE_UNKNOWN = 0x00,
    NODE_TYPE_LED = 0x01,
    NODE_TYPE_RELAY = 0x02,
    NODE_TYPE_SENSOR = 0x03
} node_type_t;

// ========== NODE DURUMLARI ==========
typedef enum {
    NODE_STATUS_FREE = 0x00,
    NODE_STATUS_PENDING,
    NODE_STATUS_ASSIGNED,
    NODE_STATUS_ACTIVE,
    NODE_STATUS_LOST
} node_status_t;

// ========== NODE BİLGİSİ (GÜNCELLENDİ) ==========
typedef struct {
    uint32_t unique_id;              // Node'un UID'si (STM32'nin UID'si)
    uint32_t assigned_can_id;        // Atanan tam 32-bit CAN ID (ODA+TİP+NODE)
    uint8_t  room_id;                // ← Hangi odaya ait
    uint16_t node_offset;            // ← O oda içindeki sıra numarası
    uint32_t master_id;              // Bu node'u kaydeden master'ın ID'si
    uint32_t last_heartbeat_time;
    node_status_t status;
    node_type_t type;
    uint8_t retry_count;
} node_info_t;

// ========== MASTER STATE ==========
typedef struct {
    uint32_t master_id;                      // Bu ESP32'nin unique ID'si
    uint8_t  room_id;                        // ← Bu odanın ID'si (1-7)
    node_info_t nodes[MAX_NODES_PER_ROOM];   // Bu odadaki node'lar
    uint16_t next_led_offset;                // ← Sıradaki LED offset'i
    uint16_t next_relay_offset;              // ← Sıradaki Relay offset'i
    uint16_t next_sensor_offset;             // ← Sıradaki Sensor offset'i
} master_state_t;


typedef enum {
    MASTER_MODE_STANDALONE,    // Merkezi master yok (şu anki durum)
    MASTER_MODE_MANAGED        // Merkezi master'a bağlı
} master_mode_t;

typedef struct {
    master_mode_t mode;
    uint32_t central_master_id;      // Merkezi master'ın ID'si
    bool room_id_assigned;           // Oda ID atandı mı?
    uint32_t last_central_heartbeat; // Merkezi master heartbeat timer
} central_connection_t;

// ========== FONKSİYON PROTOTPLERI ==========

// Master başlatma
void initDynamicIDMaster(void);
void updateDynamicIDMaster(void);

// Node yönetimi
void handleIDRequest(uint32_t unique_id, node_type_t node_type);
void handleIDConfirmation(uint32_t unique_id, uint32_t assigned_can_id_or_partial, uint8_t* data, uint8_t dlc);
void handleHeartbeat(uint32_t unique_id, uint32_t heartbeat_id, uint32_t received_master_id);

// Mesaj işleme
void processCANMasterMessage(uint32_t can_id, uint8_t* data, uint8_t dlc);
void processCentralCommand(uint32_t can_id, uint8_t* data, uint8_t dlc);  // ← YENİ

// Master bilgileri
void sendMasterHeartbeat(void);
void sendReassignmentCommand(uint32_t unique_id);
uint32_t generateMasterID(void);
uint32_t getMasterID(void);
uint8_t getMasterRoomID(void);  // ← YENİ

// NVS işlemleri
esp_err_t loadNodesFromNVS(void);
esp_err_t saveNodeToNVS(const node_info_t *node);
esp_err_t removeNodeFromNVS(uint32_t unique_id);

// Node sorgulama
uint8_t getActiveNodeCount(void);
uint8_t getActiveNodeCountByType(node_type_t type);
bool isNodeActive(uint32_t unique_id);
uint32_t getAssignedID(uint32_t unique_id);
uint32_t getNodeIDByType(node_type_t type);
bool isNodeActiveByIndex(int index);
uint32_t getNodeAssignedIDByIndex(int index);

// Debug
void printNodeStatus(void);



// ========== MERKEZİ MASTER FONKSİYONLARI ==========
void sendCentralRegisterRequest(void);
void sendCentralRoomConfirmation(void);
void handleCentralAssignRoom(uint8_t* data, uint8_t dlc);
void handleCentralLEDControl(uint8_t* data, uint8_t dlc);
void handleCentralRelayControl(uint8_t* data, uint8_t dlc);
void handleCentralStatusRequest(uint8_t* data, uint8_t dlc);

void updateCentralHeartbeat(void);
void resetRoomAssignment(void);

uint32_t getCentralMasterID(void);
void setCentralMasterID(uint32_t central_id);
void resetRoomAssignment(void);

#endif /* _USR_CAN_DYNAMIC_ID_MASTER_H_ */