#ifndef USR_LED_CONTROL_H
#define USR_LED_CONTROL_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define LED_COUNT 6
#define PWM_PERIOD 100  // (1000'den 100'e)

// LED komut tipleri
#define LED_CMD_TOGGLE          0x10
#define LED_CMD_SET             0x11
#define LED_CMD_SET_BRIGHTNESS  0x12

#define DEFAULT_BRIGHTNESS 50   // ( 0-100 aralığı)

// LED yapısı
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    uint8_t state;         // 0 = sönük, 1 = yanık
    uint8_t brightness;    // uint16_t → uint8_t (0-100)
    uint16_t pwm_counter;
} led_info_t;

void ledControl_init(void);
void ledControl_toggle(uint8_t led_number);
void ledControl_set(uint8_t led_number, uint8_t state);
void ledControl_setBrightness(uint8_t led_number, uint8_t brightness);  // ← uint16_t → uint8_t
void ledControl_processCommand(uint8_t* data, uint8_t dlc);
void ledControl_printStatus(void);
void ledControl_pwmUpdate(void);

#endif
