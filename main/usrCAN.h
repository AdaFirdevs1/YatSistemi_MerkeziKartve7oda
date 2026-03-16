#ifndef _USR_CAN_H_
#define _USR_CAN_H_

#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define CAN_RX_PIN  GPIO_NUM_19
#define CAN_TX_PIN  GPIO_NUM_20  

void initCAN(void);

// CAN mesaj gönderme fonksiyonları
esp_err_t sendCanMessage(uint32_t m_canID, uint8_t* data, uint8_t length);
esp_err_t sendCanHeader(uint32_t m_canID, uint32_t m_value);

esp_err_t receiveCANMessage(uint32_t *id, uint8_t *data, size_t *dataLen);



void startCanCommunication(void);

void processSensorData(uint32_t can_id, uint32_t data);

// Node status
void requestNodeStatus(void);

#endif /*_USR_CAN_H_*/