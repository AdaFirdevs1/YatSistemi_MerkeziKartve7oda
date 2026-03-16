#ifndef INC_USRCANIDLIST_H_
#define INC_USRCANIDLIST_H_

#include <stdbool.h>
#include <stdint.h>

// ========== NODE TYPES ==========
typedef enum {
    NODE_TYPE_UNKNOWN = 0x00,
    NODE_TYPE_LED = 0x01,
    NODE_TYPE_RELAY = 0x02,
    NODE_TYPE_SENSOR = 0x03
} node_type_t;

// ========== ROOM CONFIG (ESP32 ile uyumlu) ==========
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

// ID makroları
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

// Eski broadcast (geriye uyumluluk)
#define CAN_BROADCAST_ID            0x3FF

// ========== KOMUTLAR ==========
#define CMD_REQUEST_ID              0x01
#define CMD_ASSIGN_ID               0x02
#define CMD_ID_CONFIRMATION         0x03
#define CMD_HEARTBEAT               0x04
#define CMD_REASSIGN_REQUEST        0x05
#define CMD_MASTER_HEARTBEAT        0x06

// ========== LED CONTROL COMMANDS ==========
#define LED_CMD_TOGGLE              0x10
#define LED_CMD_SET                 0x11
#define LED_CMD_SET_BRIGHTNESS      0x12

// ========== TIMEOUTS ==========
#define ID_REQUEST_TIMEOUT_MS       5000
#define HEARTBEAT_INTERVAL_MS       2000
#define MASTER_HEARTBEAT_TIMEOUT_MS 15000

// ========== NODE STATES ==========
#define NODE_STATE_UNASSIGNED       0x00
#define NODE_STATE_REQUESTING       0x01
#define NODE_STATE_ASSIGNED         0x02
#define NODE_STATE_ACTIVE           0x03

// ========== GLOBAL VARIABLES ==========
extern uint32_t g_node_unique_id;
extern uint32_t g_my_assigned_id;    // 32-bit CAN ID
extern uint8_t  g_my_room_id;        // ← YENİ: Room ID
extern uint16_t g_my_node_offset;    // ← YENİ: Node offset
extern uint32_t g_master_id;
extern uint8_t g_node_state;
extern node_type_t g_node_type;

// Assignment buffer (2 mesaj için)
extern bool g_assignment_part1_received;
extern uint8_t g_assignment_buffer[8];

#define MY_ASSIGNED_ID      g_my_assigned_id
#define MY_ROOM_ID          g_my_room_id
#define MY_NODE_OFFSET      g_my_node_offset
#define MY_NODE_TYPE        g_node_type
#define IS_ID_ASSIGNED()    (g_node_state == NODE_STATE_ASSIGNED || g_node_state == NODE_STATE_ACTIVE)

#endif /* INC_USRCANIDLIST_H_ */
