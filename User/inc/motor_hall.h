#ifndef __MOTOR_HALL_H__
#define __MOTOR_HALL_H__

#include "main.h"

#define ENABLE_HALL 0

#define MAX_MOTOR_NUM   3
#define NORMAL_MOTOR_NUM  2U
#if NORMAL_MOTOR_NUM > MAX_MOTOR_NUM
#error "NORMAL_MOTOR_NUM must not exceed MAX_MOTOR_NUM"
#endif

#define MAX_MOTOR_STEP  0XFFFF
#define MIN_ERR         10
#define MAX_TIMEROUT    100
#define MAX_BEI_STEP    932

#define MOTOR_STOP_POWER 30


#define FLASH_ADDR  0X0800F800

#if ENABLE_HALL
typedef struct
{
    /* data */
    GPIO_Module* MA_Port;
    uint32_t MA_pin;
    GPIO_Module* MB_Port;
    uint32_t MB_pin;
    GPIO_Module* HA_port;
    uint32_t HA_pin;
    GPIO_Module* HB_port;
    uint32_t HB_pin;
}motor_port_t;
#else

typedef struct
{
    /* data */
    GPIO_Module* MA_Port;
    uint32_t MA_pin;
    GPIO_Module* MB_Port;
    uint32_t MB_pin;
    GPIO_Module* ADC_Port;
    uint32_t ADC_pin;
    uint8_t ADC_channel;
}motor_port_t;

#endif


typedef enum
{
    push = 0xffff,
    pull = 0x0001,
    stop = 0x0
}motor_state_t;

typedef struct
{
    motor_state_t state;
    uint16_t set_step;
    uint16_t cur_step;
	uint16_t adc_zero;
    uint8_t count;
}motor_t;

//typedef struct
//{
//    uint8_t motor_id;
//    uint16_t step;
//    motor_state_t state;
//}motor_msg_t;
extern motor_t motor[MAX_MOTOR_NUM];

extern uint16_t sys_save_step[2][MAX_MOTOR_NUM];
extern QueueHandle_t motor_queue;

extern void MotoInitTask(void *parameter);
extern TaskHandle_t Motor_test_Task_Handle;
void test_Task(void* parameter);
void Motor_Task(void* parameter);

#endif /* __MOTOR_HALL_H__ */
