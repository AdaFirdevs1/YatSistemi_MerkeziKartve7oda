#ifndef INC_USRGENERAL_H_
#define INC_USRGENERAL_H_

#include "main.h"
#include <stdio.h>
#include <string.h>

extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart4;
extern TIM_HandleTypeDef htim2;

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_CAN1_Init(void);
void MX_UART4_Init(void);
void MX_TIM2_Init(void);


#endif /* INC_USRGENERAL_H_ */
