#include "usrLedControl.h"
#include "usrGeneral.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart4;
extern TIM_HandleTypeDef htim2;

// LED tanımları (0-100 brightness)
static led_info_t leds[LED_COUNT] = {
    {GPIOB, GPIO_PIN_3, 0, DEFAULT_BRIGHTNESS, 0},  // LED 1
    {GPIOB, GPIO_PIN_4, 0, DEFAULT_BRIGHTNESS, 0},  // LED 2
    {GPIOB, GPIO_PIN_5, 0, DEFAULT_BRIGHTNESS, 0},  // LED 3
    {GPIOB, GPIO_PIN_6, 0, DEFAULT_BRIGHTNESS, 0},  // LED 4
    {GPIOB, GPIO_PIN_7, 0, DEFAULT_BRIGHTNESS, 0},  // LED 5
    {GPIOB, GPIO_PIN_8, 0, DEFAULT_BRIGHTNESS, 0}   // LED 6
};

static volatile uint8_t pwm_cycle_counter = 0;

void ledControl_init(void)
{
    char buffer[256];

    // Tüm LED'leri söndür
    for(int i = 0; i < LED_COUNT; i++)
    {
        HAL_GPIO_WritePin(leds[i].port, leds[i].pin, GPIO_PIN_RESET);
        leds[i].state = 0;
    }

    // TIM2'yi başlat
    HAL_TIM_Base_Start_IT(&htim2);

    snprintf(buffer, sizeof(buffer),
             "\r\n"
             "================================================\r\n"
             " LED SOFTWARE PWM CONTROL INITIALIZED          \r\n"
             "================================================\r\n"
             " Total LEDs    : %d                            \r\n"
             " Timer Freq    : 10 kHz (100us tick)           \r\n"
             " PWM Frequency : 100 Hz                        \r\n"
             " Resolution    : 0-100                         \r\n"
             " Default Level : %d/100                        \r\n"
             "================================================\r\n\r\n",
             LED_COUNT, DEFAULT_BRIGHTNESS);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}

// Timer interrupt - Her 100µs'de çağrılır (10 kHz)
void ledControl_pwmUpdate(void)
{
    pwm_cycle_counter++;
    if(pwm_cycle_counter >= PWM_PERIOD) {
        pwm_cycle_counter = 0;
    }

    // Optimize edilmiş PWM (tek döngü)
    uint8_t counter = pwm_cycle_counter;

    // LED 1-6 için unroll edilmiş loop
    if(leds[0].state && counter < leds[0].brightness)
        GPIOB->BSRR = GPIO_PIN_3;  // Set
    else
        GPIOB->BRR = GPIO_PIN_3;   // Reset

    if(leds[1].state && counter < leds[1].brightness)
        GPIOB->BSRR = GPIO_PIN_4;
    else
        GPIOB->BRR = GPIO_PIN_4;

    if(leds[2].state && counter < leds[2].brightness)
        GPIOB->BSRR = GPIO_PIN_5;
    else
        GPIOB->BRR = GPIO_PIN_5;

    if(leds[3].state && counter < leds[3].brightness)
        GPIOB->BSRR = GPIO_PIN_6;
    else
        GPIOB->BRR = GPIO_PIN_6;

    if(leds[4].state && counter < leds[4].brightness)
        GPIOB->BSRR = GPIO_PIN_7;
    else
        GPIOB->BRR = GPIO_PIN_7;

    if(leds[5].state && counter < leds[5].brightness)
        GPIOB->BSRR = GPIO_PIN_8;
    else
        GPIOB->BRR = GPIO_PIN_8;
}

void ledControl_setBrightness(uint8_t led_number, uint8_t brightness)
{
    if(led_number < 1 || led_number > LED_COUNT) return;

    uint8_t index = led_number - 1;

    if(brightness > 100) brightness = 100;

    leds[index].brightness = brightness;

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[LED %d] BRIGHTNESS -> %d/100 (%d%%)\r\n",
             led_number, brightness, brightness);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
}

void ledControl_toggle(uint8_t led_number)
{
    if(led_number < 1 || led_number > LED_COUNT) return;

    uint8_t index = led_number - 1;
    leds[index].state = !leds[index].state;

    // LED açılıyorsa ve brightness=0 ise, varsayılan 50'ye ayarla
    if(leds[index].state && leds[index].brightness == 0) {
        leds[index]. brightness = DEFAULT_BRIGHTNESS;  // 50

        char buffer[128];
        snprintf(buffer, sizeof(buffer),
                 "[LED %d] Auto-brightness set to %d/100\r\n",
                 led_number, DEFAULT_BRIGHTNESS);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[LED %d] TOGGLE -> State: %s, Brightness: %d/100\r\n",
             led_number,
             leds[index].state ? "ON " : "OFF",
             leds[index].brightness);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
}


void ledControl_set(uint8_t led_number, uint8_t state)
{
    if(led_number < 1 || led_number > LED_COUNT) return;

    uint8_t index = led_number - 1;
    leds[index].state = (state != 0) ? 1 : 0;

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "[LED %d] SET -> State: %s\r\n",
             led_number,
             leds[index].state ? "ON " : "OFF");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
}

void ledControl_processCommand(uint8_t* data, uint8_t dlc)
{
    if(dlc < 2) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer),
                 "[CMD ERROR] DLC too short: %d\r\n", dlc);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
        return;
    }

    uint8_t command = data[0];
    uint8_t led_number = data[1];

    char buffer[128];

    switch(command)
    {
        case LED_CMD_TOGGLE:
            snprintf(buffer, sizeof(buffer),
                     "[CMD] TOGGLE - LED:%d\r\n", led_number);
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);

            ledControl_toggle(led_number);
            break;

        case LED_CMD_SET:
            if(dlc >= 3)
            {
                uint8_t state = data[2];
                snprintf(buffer, sizeof(buffer),
                         "[CMD] SET - LED:%d State:%d\r\n", led_number, state);
                HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);

                ledControl_set(led_number, state);
            }
            break;

        case LED_CMD_SET_BRIGHTNESS:
            if(dlc >= 4)
            {
                uint16_t brightness_1000 = (data[2] << 8) | data[3];
                uint8_t brightness_100 = (brightness_1000 / 10);

                if(brightness_100 > 100) brightness_100 = 100;

                snprintf(buffer, sizeof(buffer),
                         "[CMD] BRIGHTNESS - LED:%d Value:%d/1000 -> %d/100\r\n",
                         led_number, brightness_1000, brightness_100);
                HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);

                ledControl_setBrightness(led_number, brightness_100);
            }
            else
            {
                snprintf(buffer, sizeof(buffer),
                         "[CMD ERROR] BRIGHTNESS needs 4 bytes, got %d\r\n", dlc);
                HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
            }
            break;

        default:
            snprintf(buffer, sizeof(buffer),
                     "[CMD ERROR] Unknown command: 0x%02X\r\n", command);
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
            break;
    }
}



void ledControl_printStatus(void)
{
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "\r\n=== LED STATUS ===\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    for(int i = 0; i < LED_COUNT; i++)
    {
        snprintf(buffer, sizeof(buffer),
                 "  LED %d: %s - %d/100 (%d%%)\r\n",
                 i + 1,
                 leds[i].state ? "ON " : "OFF",
                 leds[i].brightness,
                 leds[i].brightness);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }

    snprintf(buffer, sizeof(buffer), "==================\r\n\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}
