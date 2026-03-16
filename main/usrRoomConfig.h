#ifndef _USR_ROOM_CONFIG_H_
#define _USR_ROOM_CONFIG_H_

#include <stdint.h>
#include "esp_err.h"

// ========== ODA KONFIGÜRASYONU ==========
// Room ID tamamen dinamik - MAC adresinden veya NVS'den

#define MIN_ROOM_ID             1
#define MAX_ROOM_ID             7
#define TOTAL_ROOMS             7

// ========== 32-BIT CAN ID YAPISI ==========
// Bit Layout: [ODA_ID 8bit][CİHAZ_TİPİ 8bit][NODE_ID 16bit]
// Örnek: Oda 3, LED Kartı 5 → 0x03100005

#define ROOM_ID_SHIFT           24
#define DEVICE_TYPE_SHIFT       16
#define NODE_ID_MASK            0xFFFF

// Cihaz tipleri
#define DEVICE_TYPE_MASTER      0x00
#define DEVICE_TYPE_LED         0x10
#define DEVICE_TYPE_RELAY       0x20
#define DEVICE_TYPE_SENSOR      0x30

// Broadcast
#define ROOM_BROADCAST          0xFF
#define DEVICE_ALL              0xFF
#define NODE_ALL                0xFFFF

// ========== MERKEZİ MASTER PROTOKOLÜ ==========
#define CENTRAL_MASTER_BASE_ID      MAKE_CAN_ID(0x00, 0x00, 0x0000)
#define CENTRAL_MASTER_BROADCAST    0x1FF0000    

// Merkezi master komutları
#define CMD_CENTRAL_REGISTER        0x10  // Oda master'ı kendini tanıtır
#define CMD_CENTRAL_ASSIGN_ROOM     0x11  // Merkezi master oda ID atar
#define CMD_CENTRAL_ROOM_CONFIRM    0x12  // Oda master onaylar
#define CMD_CENTRAL_LED_CONTROL     0x13  // Direkt LED kontrolü
#define CMD_CENTRAL_RELAY_CONTROL   0x14  // Direkt röle kontrolü
#define CMD_CENTRAL_STATUS_REQUEST  0x15  // Durum sorgula
#define CMD_CENTRAL_STATUS_RESPONSE 0x16  // Durum yanıtı

// ========== ID MAKROLARI ==========

#define MAKE_CAN_ID(room, type, node) \
    (((uint32_t)(room) << ROOM_ID_SHIFT) | \
     ((uint32_t)(type) << DEVICE_TYPE_SHIFT) | \
     ((uint32_t)(node) & NODE_ID_MASK))

#define GET_ROOM_ID(can_id)         (((can_id) >> ROOM_ID_SHIFT) & 0xFF)
#define GET_DEVICE_TYPE(can_id)     (((can_id) >> DEVICE_TYPE_SHIFT) & 0xFF)
#define GET_NODE_ID(can_id)         ((can_id) & NODE_ID_MASK)

// ========== PROTOKOL ID'LERİ ==========
#define CAN_ID_REQUEST_ID           MAKE_CAN_ID(0x00, 0xFF, 0xFFFE)
#define CAN_ID_ASSIGNMENT_ID        MAKE_CAN_ID(0x00, 0xFF, 0xFFFD)
#define CAN_MASTER_HEARTBEAT_ID     MAKE_CAN_ID(0x00, 0xFF, 0xFFFC)
#define CAN_CENTRAL_COMMAND_ID      MAKE_CAN_ID(0xFF, 0x00, 0xFFFF) 

// ========== FONKSİYONLAR ==========

/**
 * @brief ESP32'nin MAC adresinden otomatik Room ID üretir
 * Önce NVS'i kontrol eder, yoksa MAC'den hesaplar
 * 
 * @return Room ID (1-7 arası)
 */
uint8_t getRoomIDFromUID(void);

/**
 * @brief Room ID'yi NVS'ye kaydeder (manuel ayar için)
 * 
 * @param room_id Oda numarası (1-7)
 * @return ESP_OK veya hata kodu
 */
esp_err_t setRoomIDToNVS(uint8_t room_id);

/**
 * @brief NVS'den Room ID okur
 * 
 * @return Room ID, yoksa 0
 */
uint8_t getRoomIDFromNVS(void);

/**
 * @brief Oda adını döndürür
 * 
 * @param room_id Oda numarası
 * @param buffer Hedef buffer
 * @param size Buffer boyutu
 */
void getRoomName(uint8_t room_id, char *buffer, size_t size);

/**
 * @brief Tüm oda bilgisini döndürür (debug için)
 */
void printRoomInfo(void);

#endif /* _USR_ROOM_CONFIG_H_ */