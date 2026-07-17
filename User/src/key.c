/**
 * @file        key.c
 * @author      KimQi
 * @date        2024-12-20
 * @copyright   Copyright (c) 2024, KimQi. All rights reserved.
 *              This file is part of an embedded project and is licensed
 *              under the terms specified in the LICENSE file.
 */

/*******************************************************
 *                  Include Files
 *******************************************************/
 
#include "key.h"
#include "fan_heat_massage.h"
#include "motor_hall.h"
#include "ble_module.h"
/*******************************************************
 *                  Macro Definitions
 *******************************************************/


#define NUM_KEYS                1  
#define SCAN_PERIOD_MS          10

#define PRESS_TIME_MS           8
#define LONG_PRESS_TIME_MS      100
#define RELEASE_TIME_MS         8  

#define KEY_PESS_LEVEL          PIN_RESET
/*******************************************************
 *                  Global Variables
 *******************************************************/
 
Key_t keys[NUM_KEYS] = {
	{GPIOB,GPIO_PIN_9,0,!KEY_PESS_LEVEL}
};

QueueHandle_t keyEventQueue;
QueueHandle_t heartbeatQueue;
/*******************************************************
 *                  Function Definitions
 *******************************************************/

void Button_GPIO_Initialize(GPIO_Module* GPIOx, uint16_t pin)
{
    GPIO_InitType GPIO_InitStructure;

    if (GPIOx == GPIOA)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOA);
    }
    else if (GPIOx == GPIOB)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOB);
    }
    else if (GPIOx == GPIOC)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOC);
    }
    else
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOD);
    }

    GPIO_Structure_Initialize(&GPIO_InitStructure);
    GPIO_InitStructure.Pin       = pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.GPIO_Pull = GPIO_PULL_UP;
    GPIO_Peripheral_Initialize(GPIOx, &GPIO_InitStructure);
}

void KeyInit(void)
{
    for (uint8_t i = 0; i < NUM_KEYS; i++)
    {
        // 初始化GPIO，根据具体的GPIO配置修改
        Button_GPIO_Initialize(keys[i].port, keys[i].pin);
    }
}

void KeyScanTask(void *pvParameters)
{
    KeyInit();

    while (1)
    {
        for (uint8_t i = 0; i < NUM_KEYS; i++)
        {
            /* code */
            uint8_t current_level = GPIO_Input_Pin_Data_Get(keys[i].port, keys[i].pin);  
            if(current_level == KEY_PESS_LEVEL && keys[i].last_level != KEY_PESS_LEVEL)
            {
                keys[i].press_time = 0;
            }

            if(current_level == KEY_PESS_LEVEL && keys[i].last_level == KEY_PESS_LEVEL)
            {
                if(keys[i].press_time < 0xff) keys[i].press_time++;
                if(keys[i].press_time == PRESS_TIME_MS)
                {
                    KeyEventInfo_t key_event ={i,KEY_EVENT_PRESS};
                    xQueueSend(keyEventQueue, &key_event, 0);
                }
                else if(keys[i].press_time == LONG_PRESS_TIME_MS)
                {
                    KeyEventInfo_t key_event ={i,KEY_EVENT_LONG_PRESS};
                    xQueueSend(keyEventQueue, &key_event, 0);
                }
            }

            if(current_level != KEY_PESS_LEVEL && keys[i].last_level == KEY_PESS_LEVEL && keys[i].press_time >= PRESS_TIME_MS)
            {
                keys[i].release_time = 0;
            }

            if(current_level != KEY_PESS_LEVEL && keys[i].last_level != KEY_PESS_LEVEL && keys[i].press_time >= PRESS_TIME_MS)
            {
                if(keys[i].release_time < 0xff) keys[i].release_time++;
                if(keys[i].release_time == RELEASE_TIME_MS)
                {
                    if(keys[i].press_time < LONG_PRESS_TIME_MS) 
                    {
                        KeyEventInfo_t key_event ={i,KEY_EVENT_RELEASE_SHORT};
                        xQueueSend(keyEventQueue, &key_event, 0);
                    }
                    else
                    {
                        KeyEventInfo_t key_event ={i,KEY_EVENT_RELEASE_LONG};
                        xQueueSend(keyEventQueue, &key_event, 0);
                    }
                }
            }
            keys[i].last_level = current_level;
        }
        vTaskDelay(SCAN_PERIOD_MS);
    }
}

void KeyEventHandlerTask(void *pvParameters)
{
	uint16_t motor_cmd_t[MAX_MOTOR_NUM] = {0};
    KeyEventInfo_t key_event;
    while (1)
    {
        if (xQueueReceive(keyEventQueue, &key_event, 0) == pdPASS)
        {
            switch (key_event.event)
            {
            case KEY_EVENT_PRESS:
				Massage_Motor_Set_All_Retract(1U);
				if(xTimerIsTimerActive(xTimer_HeartBeatTimeout) == pdTRUE)//有心跳 home功能
				{
					motor_cmd_t[0] = 	pull;
					motor_cmd_t[1] = 	pull;
					xQueueSendFromISR( motor_queue, &motor_cmd_t, NULL);
				}
				else//无心跳，查找遥控器功能
				{
					motor_cmd_t[0] = 	pull;
					motor_cmd_t[1] = 	pull;
					xQueueSendFromISR( motor_queue, &motor_cmd_t, NULL);
					xTaskNotify(xBleModeHandle, 0x55, eSetValueWithOverwrite);
				}
                break;
            case KEY_EVENT_LONG_PRESS:
				
                break;
            case KEY_EVENT_RELEASE_SHORT:
            case KEY_EVENT_RELEASE_LONG:
				Massage_Motor_Set_All_Retract(0U);
				motor_cmd_t[0] = 	stop;
				motor_cmd_t[1] = 	stop;
//				motor_cmd_t[2] = 	stop;
				xQueueSendFromISR( motor_queue, &motor_cmd_t, NULL);
                break;
            default:
                break;
            }
        }
    }
}
