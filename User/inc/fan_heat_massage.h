#ifndef _FAN_HEAT_MASSAGE_H_
#define _FAN_HEAT_MASSAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Constants */

/* Macros */
#define MASSAGE_MOTOR_LEVEL1_EXTEND_TIME_MS      2000U
#define MASSAGE_MOTOR_LEVEL1_INTERVAL_TIME_MS    1000U
#define MASSAGE_MOTOR_LEVEL1_RETRACT_TIME_MS     3000U

#define MASSAGE_MOTOR_LEVEL2_EXTEND_TIME_MS      4000U
#define MASSAGE_MOTOR_LEVEL2_INTERVAL_TIME_MS    1000U
#define MASSAGE_MOTOR_LEVEL2_RETRACT_TIME_MS     5000U

#define MASSAGE_MOTOR_LEVEL3_EXTEND_TIME_MS      5000U
#define MASSAGE_MOTOR_LEVEL3_INTERVAL_TIME_MS    1000U
#define MASSAGE_MOTOR_LEVEL3_RETRACT_TIME_MS     6000U

#define MASSAGE_MOTOR_RETRACT_MARGIN_TIME_MS     1000U
#define MASSAGE_MOTOR_TASK_PERIOD_MS             10U
/* Types */
typedef struct
{
    uint8_t msg_select;
    uint8_t fan_level;
    uint8_t hot_level;
    uint8_t Massage_level;
    uint8_t lumbar_support;
}Fan_Heat_Massage_Typedef;

typedef struct
{
    uint8_t msg_fun;
    uint8_t msg_data;
}msg_Typedef;


/* Variables */
extern QueueHandle_t Set_Fan_Heat_Massage_Queue1;
extern QueueHandle_t Set_Fan_Heat_Massage_Queue2;

extern QueueHandle_t Get_Fan_Heat_Massage_Queue1;
extern QueueHandle_t Get_Fan_Heat_Massage_Queue2;

extern QueueHandle_t cs_status_Queue;
extern Fan_Heat_Massage_Typedef sys_Fan_Heat_Massage_Data1;
extern Fan_Heat_Massage_Typedef sys_Fan_Heat_Massage_Data2;
extern TaskHandle_t Massage_Motor_Task_Handle;
/* Function Prototypes */
void fan_heat_massage_select_tx_Task(void *parameter);
void fan_heat_massage1_tx_Task(void *parameter);
void fan_heat_massage2_tx_Task(void *parameter);
void Massage_Motor_Task(void *parameter);
void Massage_Motor_Set_Level(uint8_t level);
#ifdef __cplusplus
}
#endif

#endif 
