#include "usrGeneral.h"
#include "usrCan.h"
#include "usrLedControl.h"

// Flash temizleme fonksiyonu
void eraseStoredID(void)
{
    HAL_StatusTypeDef status;
    uint32_t pageError = 0;
    char buffer[128];

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = 0x0801FC00; // ID_STORAGE_ADDRESS
    EraseInitStruct.NbPages     = 1;

    snprintf(buffer, sizeof(buffer), "\r\n ERASING STORED CAN ID FROM FLASH...\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    HAL_FLASH_Unlock();

    status = HAL_FLASHEx_Erase(&EraseInitStruct, &pageError);

    if (status == HAL_OK)
    {
        snprintf(buffer, sizeof(buffer), "Flash erased successfully!\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Flash erase failed! Error: 0x%lX\r\n", pageError);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }

    HAL_FLASH_Lock();

    snprintf(buffer, sizeof(buffer), "System will restart in 2 seconds...\r\n\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    HAL_Delay(2000);
    NVIC_SystemReset();
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_UART4_Init();
    MX_TIM2_Init();
    MX_CAN1_Init();

    // FLASH TEMİZLEME
    //eraseStoredID();

    // LED kontrol sistemini başlat
    ledControl_init();

    // CAN ID sistemini başlat
    initNodeID();



    while (1)
    {
    	canDynamicIDTask();
		readCanStart();
		HAL_Delay(1);
    }
}
