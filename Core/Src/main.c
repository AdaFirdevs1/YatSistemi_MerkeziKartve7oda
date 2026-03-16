#include "main.h"
#include "relay_node.h"

CAN_HandleTypeDef hcan1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart4;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_UART4_Init(void);
static void MX_TIM2_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_UART4_Init();  // UART önce başlasın (debug için)
    MX_CAN1_Init();
    MX_TIM2_Init();

    // Relay node başlat
    RelayNode_Init(&hcan1, &huart4);

    //RelayNode_EraseMemory();

    uint32_t last_status_print = 0;

    while (1)
    {
        // Ana işleme döngüsü (CAN okuma burada)
        RelayNode_Process();

        // Her 10 saniyede durum yazdır
        if ((HAL_GetTick() - last_status_print) > 10000)
        {
            Debug_PrintNodeInfo();
            last_status_print = HAL_GetTick();
        }

        HAL_Delay(10);  // 10ms döngü
    }
}



void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.Prediv1Source = RCC_PREDIV1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL2.PLL2State = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }


  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }


  __HAL_RCC_PLLI2S_ENABLE();
}


static void MX_CAN1_Init(void)
{
    char buffer[256];

    hcan1.Instance = CAN1;
    hcan1.Init.Prescaler = 4;
    hcan1.Init.Mode = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_15TQ;
    hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan1.Init.TimeTriggeredMode = DISABLE;
    hcan1.Init.AutoBusOff = ENABLE;      // ✅ ENABLE
    hcan1.Init.AutoWakeUp = ENABLE;      // ✅ ENABLE
    hcan1.Init.AutoRetransmission = ENABLE;  // ✅ ENABLE
    hcan1.Init.ReceiveFifoLocked = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan1) != HAL_OK)
    {
        snprintf(buffer, sizeof(buffer), "CAN Init FAILED\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        Error_Handler();
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "CAN Init OK\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }

    // ✅ Filter config
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;

    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;

    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.SlaveStartFilterBank = 14;

    // ✅ EKSIK:  Filter'ı uygula!
    if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
    {
        snprintf(buffer, sizeof(buffer), "CAN Filter FAILED\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        Error_Handler();
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "CAN Filter OK\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }

    // ✅ EKSIK: CAN'ı başlat!
    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        snprintf(buffer, sizeof(buffer), "CAN Start FAILED - Error: 0x%lX\r\n", HAL_CAN_GetError(&hcan1));
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        Error_Handler();
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "CAN Started Successfully\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }

    // ✅ CAN durumunu kontrol et
    HAL_CAN_StateTypeDef state = HAL_CAN_GetState(&hcan1);
    snprintf(buffer, sizeof(buffer), "CAN State after start: %d\r\n", state);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    // ✅ Baudrate hesaplama
    uint32_t apb1_clock = HAL_RCC_GetPCLK1Freq();
    uint32_t prescaler = hcan1.Init.Prescaler;
    uint32_t bs1_value = ((hcan1.Init.TimeSeg1) >> 16) + 1;
    uint32_t bs2_value = ((hcan1.Init.TimeSeg2) >> 20) + 1;
    uint32_t baudrate = apb1_clock / (prescaler * (1 + bs1_value + bs2_value));

    snprintf(buffer, sizeof(buffer),
            "CAN Config:\r\n"
            "APB1 Clock: %lu Hz\r\n"
            "Prescaler: %lu\r\n"
            "BS1: %lu tq, BS2: %lu tq\r\n"
            "Calculated Baudrate: %lu bps\r\n",
            apb1_clock, prescaler, bs1_value, bs2_value, baudrate);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}


static void MX_TIM2_Init(void)
{


  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};


  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 35;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }


}

static void MX_UART4_Init(void)
{

  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }

}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, Output_1_Pin|Output_2_Pin|Output_3_Pin|Output_4_Pin
                          |Output_5_Pin|Output_6_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : Output_1_Pin Output_2_Pin Output_3_Pin Output_4_Pin
                           Output_5_Pin Output_6_Pin */
  GPIO_InitStruct.Pin = Output_1_Pin|Output_2_Pin|Output_3_Pin|Output_4_Pin
                          |Output_5_Pin|Output_6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);


}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{

}
#endif
