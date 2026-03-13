#ifndef _USR_CENTRAL_MASTER_H_
#define _USR_CENTRAL_MASTER_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "usrRoomConfig.h"

// ========== CENTRAL MASTER COMMANDS ==========
#define CMD_CENTRAL_REGISTER        0x10  
#define CMD_CENTRAL_ASSIGN_ROOM     0x11  
#define CMD_CENTRAL_ROOM_CONFIRM    0x12  
#define CMD_CENTRAL_LED_CONTROL     0x13  
#define CMD_CENTRAL_RELAY_CONTROL   0x14  
#define CMD_CENTRAL_STATUS_REQUEST  0x15  
#define CMD_CENTRAL_STATUS_RESPONSE 0x16  

// ========== ROOM/NODE HEARTBEAT COMMANDS (from room ESP protocol) ==========
#define CMD_HEARTBEAT               0x04   // Node heartbeat (room ESP → STM32 nodes)
#define CMD_MASTER_HEARTBEAT        0x06   // Room master heartbeat (room ESP → central)


// ========== ROOM MASTER STATE ==========
typedef enum {
    ROOM_STATUS_OFFLINE = 0,
    ROOM_STATUS_WAITING_CONFIRM,
    ROOM_STATUS_ACTIVE,
    ROOM_STATUS_LOST
} room_status_t;

// Room Master bilgisi (artık node değil, ESP32 oda master'ı)
typedef struct {
    uint16_t master_id;              // Oda master'ının 16-bit ID'si
    uint8_t  mac_addr[6];        
    bool     mac_verified; 
    uint8_t  assigned_room_id;       // Atanan oda numarası (1-7)
    uint32_t last_heartbeat_time;
    room_status_t status;
    uint8_t  active_led_count;       // Bu odadaki aktif LED sayısı
    uint8_t  active_relay_count;     // Bu odadaki aktif Röle sayısı
    bool     led_states[6];          // LED durumları
    bool     relay_states[6];        // Röle durumları
    uint16_t led_brightness[6];      // LED parlaklıkları
} room_master_info_t;

// Central Master State
typedef struct {
    uint32_t central_master_id;
    room_master_info_t rooms[MAX_ROOM_ID];  // 7 oda
    uint8_t active_room_count;
} central_master_state_t;

// ========== FUNCTIONS ==========
void initCentralMaster(void);
void updateCentralMaster(void);
void sendCentralHeartbeat(void);

// Room Master Management
void handleRoomRegister(uint16_t master_id, uint8_t* data, uint8_t dlc);
void assignRoomID(uint16_t master_id, uint8_t room_id);
void handleRoomConfirm(uint8_t room_id, uint16_t master_id);
void handleRoomHeartbeat(uint8_t room_id, uint16_t master_id);
void handleRoomStatusResponse(uint8_t room_id, uint8_t* data, uint8_t dlc);

// Control Commands
void sendLEDControlToRoom(uint8_t room_id, uint8_t led_channel, bool state, uint16_t brightness);
void sendRelayControlToRoom(uint8_t room_id, uint8_t relay_channel, bool state);
void requestRoomStatus(uint8_t room_id);

// Message Processing
void processCentralMasterMessage(uint32_t can_id, uint8_t* data, uint8_t dlc);

// Queries
uint8_t getActiveRoomCount(void);
bool isRoomActive(uint8_t room_id);
room_master_info_t* getRoomInfo(uint8_t room_id);
void printRoomMasterStatus(void);



void handleRoomHeartbeat(uint8_t room_id, uint16_t master_id);

void saveRoomTableToNVS(void);
void loadRoomTableFromNVS(void);


#endif /* _USR_CENTRAL_MASTER_H_ */