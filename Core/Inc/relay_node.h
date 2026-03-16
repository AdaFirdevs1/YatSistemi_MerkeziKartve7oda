#ifndef __RELAY_NODE_H
#define __RELAY_NODE_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// ========== NODE TYPES ==========
typedef enum {
    NODE_TYPE_UNKNOWN = 0x00,
    NODE_TYPE_LED = 0x01,
    NODE_TYPE_RELAY = 0x02,
    NODE_TYPE_SENSOR = 0x03
} node_type_t;

#define MY_NODE_TYPE    NODE_TYPE_RELAY

// ========== ROOM CONFIG (usrRoomConfig.h'den kopyalanan) ==========
#define ROOM_ID_SHIFT           24
#define DEVICE_TYPE_SHIFT       16
#define NODE_ID_MASK            0xFFFF

#define DEVICE_TYPE_MASTER      0x00
#define DEVICE_TYPE_LED         0x10
#define DEVICE_TYPE_RELAY       0x20
#define DEVICE_TYPE_SENSOR      0x30

#define ROOM_BROADCAST          0xFF
#define DEVICE_ALL              0xFF
#define NODE_ALL                0xFFFF

// ID oluşturma makrosu
#define MAKE_CAN_ID(room, type, node) \
    (((uint32_t)(room) << ROOM_ID_SHIFT) | \
     ((uint32_t)(type) << DEVICE_TYPE_SHIFT) | \
     ((uint32_t)(node) & NODE_ID_MASK))

#define GET_ROOM_ID(can_id)         (((can_id) >> ROOM_ID_SHIFT) & 0xFF)
#define GET_DEVICE_TYPE(can_id)     (((can_id) >> DEVICE_TYPE_SHIFT) & 0xFF)
#define GET_NODE_ID(can_id)         ((can_id) & NODE_ID_MASK)

// ========== PROTOKOL ID'LERİ ==========
#define CAN_ID_REQUEST_ID           MAKE_CAN_ID(0x00, 0xFF, 0xFFFE)  // 0x00FFFFFE
#define CAN_ID_ASSIGNMENT_ID        MAKE_CAN_ID(0x00, 0xFF, 0xFFFD)  // 0x00FFFFD
#define CAN_MASTER_HEARTBEAT_ID     MAKE_CAN_ID(0x00, 0xFF, 0xFFFC)  // 0x00FFFFC
#define CAN_CENTRAL_COMMAND_ID      MAKE_CAN_ID(0xFF, 0xFF, 0xFFFF)  // 0xFFFFFFFF

// Eski broadcast (11-bit standard)
#define CAN_BROADCAST_ID            0x3FF

// ========== KOMUTLAR ==========
#define CMD_REQUEST_ID              0x01
#define CMD_ASSIGN_ID               0x02
#define CMD_ID_CONFIRMATION         0x03
#define CMD_HEARTBEAT               0x04
#define CMD_REASSIGN_REQUEST        0x05
#define CMD_MASTER_HEARTBEAT        0x06

// ========== RELAY CONTROL COMMANDS ==========
#define RELAY_CMD_TOGGLE            0x20
#define RELAY_CMD_SET               0x21
#define RELAY_CMD_ALL_ON            0x22  // ← YENİ
#define RELAY_CMD_ALL_OFF           0x23  // ← YENİ

// ========== KONFIGÜRASYON ==========
#define HEARTBEAT_INTERVAL_MS       2000
#define ENROLLMENT_TIMEOUT_MS       5000
#define MASTER_HEARTBEAT_TIMEOUT_MS 15000

#define DEBUG_ENABLE                1

// ========== FLASH MEMORY DEFINES ==========
#define ID_STORAGE_ADDRESS ((uint32_t)0x0801FC00)
#define FLASH_MAGIC_NUMBER 0xDEADBEEF

// ========== NODE STATES ==========
typedef enum {
    NODE_STATE_UNENROLLED = 0,
    NODE_STATE_WAITING_ID,
    NODE_STATE_ENROLLED,
    NODE_STATE_ACTIVE
} node_state_t;

// ========== NODE INFO STRUCTURE (GÜNCELLENDİ) ==========
typedef struct {
    uint32_t unique_id;                      // STM32 UID
    uint32_t assigned_can_id;                // Tam 32-bit CAN ID (Room+Type+Offset)
    uint8_t  room_id;                        // ← YENİ: Hangi oda
    uint16_t node_offset;                    // ← YENİ: O oda içindeki offset
    uint16_t master_id;                      // Master ID (16-bit)
    node_state_t state;
    node_type_t type;
    uint32_t last_heartbeat_time;
    uint32_t last_master_heartbeat_time;
    uint32_t enrollment_start_time;
    bool relay_states[6];

    // ID assignment buffer (2 mesaj için)
    bool assignment_part1_received;          // ← YENİ
    uint8_t assignment_buffer[16];           // ← YENİ
} relay_node_t;

// ========== FONKSİYON PROTOTPLERI ==========
void RelayNode_Init(CAN_HandleTypeDef *hcan, UART_HandleTypeDef *huart);
void RelayNode_Process(void);
void RelayNode_SendHeartbeat(void);
void RelayNode_RequestID(void);
void RelayNode_ProcessCANMessage(uint32_t can_id, uint8_t *data, uint8_t dlc, bool is_extended);

// Relay control
void RelayNode_SetRelay(uint8_t relay_num, bool state);
void RelayNode_SetAllRelays(bool state);

// Debug
void Debug_Init(UART_HandleTypeDef *huart);
void Debug_Printf(const char *format, ...);
void Debug_PrintNodeInfo(void);

// Utility
uint32_t RelayNode_GetUniqueID(void);


// Hafıza Temizleme (Factory Reset)
void RelayNode_EraseMemory(void);

#endif /* __RELAY_NODE_H */
