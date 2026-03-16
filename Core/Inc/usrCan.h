#ifndef INC_USRCAN_H_
#define INC_USRCAN_H_

#include "main.h"
#include "stm32f1xx_hal.h"
#include "usrCanIDList.h"

// CAN Initialization
void initCAN(void);

// CAN message handling
void readCanStart(void);
void sendCanHeader(uint32_t m_canID, uint32_t m_value);
void sendCanMessage(uint32_t m_canID, uint8_t* data, uint8_t length, bool use_extended);

// Dynamic ID management
void initNodeID(void);
void canDynamicIDTask(void);

// Extern değişkenler
extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart4;
extern node_type_t g_node_type;

#endif /* INC_USRCAN_H_ */
