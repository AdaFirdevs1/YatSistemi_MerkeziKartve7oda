#ifndef _USR_ROOM_CONFIG_H_
#define _USR_ROOM_CONFIG_H_

#include <stdint.h>
#include "esp_err.h"

// ========== ROOM CONFIGURATION ==========
#define MIN_ROOM_ID             1
#define MAX_ROOM_ID             7
#define TOTAL_ROOMS             7

// ========== 32-BIT CAN ID STRUCTURE ==========
#define ROOM_ID_SHIFT           24
#define DEVICE_TYPE_SHIFT       16
#define NODE_ID_MASK            0xFFFF

// Device types
#define DEVICE_TYPE_MASTER      0x00
#define DEVICE_TYPE_LED         0x10
#define DEVICE_TYPE_RELAY       0x20
#define DEVICE_TYPE_SENSOR      0x30

// Broadcast
#define ROOM_BROADCAST          0xFF
#define DEVICE_ALL              0xFF
#define NODE_ALL                0xFFFF

// ========== CENTRAL MASTER PROTOCOL ==========
#define CENTRAL_MASTER_BASE_ID      MAKE_CAN_ID(0x00, 0x00, 0x0000)
#define CENTRAL_MASTER_BROADCAST    0x1FF0000  

// ========== ID MACROS ==========
#define MAKE_CAN_ID(room, type, node) \
    (((uint32_t)(room) << ROOM_ID_SHIFT) | \
     ((uint32_t)(type) << DEVICE_TYPE_SHIFT) | \
     ((uint32_t)(node) & NODE_ID_MASK))

#define GET_ROOM_ID(can_id)         (((can_id) >> ROOM_ID_SHIFT) & 0xFF)
#define GET_DEVICE_TYPE(can_id)     (((can_id) >> DEVICE_TYPE_SHIFT) & 0xFF)
#define GET_NODE_ID(can_id)         ((can_id) & NODE_ID_MASK)

#endif /* _USR_ROOM_CONFIG_H_ */