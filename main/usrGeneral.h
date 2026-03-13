#ifndef _USR_GENERAL_H_
#define _USR_GENERAL_H_

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "waveshare_rgb_lcd_port.h"

#include "usrCAN.h"
#include "usrGraphicalInterface.h"
#include "usrGeneralDefines.h"

#define delay(x) (vTaskDelay((x) / (portTICK_PERIOD_MS)))

#endif /*_USR_GENERAL_H_*/