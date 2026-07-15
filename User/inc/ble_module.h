/**
 * @file        ble_module.h
 * @author      KimQi
 * @date        2024-12-18
 */

#ifndef BLE_MODULE_H
#define BLE_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************
 *                  Include Files
 *******************************************************/
#include "main.h"

/*******************************************************
 *                  Macro Definitions
 *******************************************************/
#define BLE_MAX_BUF_LEN     20
#define BLE_FUN_BYTE0       0X00
//#define BLE_FUN_BYTE1       0X36
#define BLE_FUN_BYTE1       0XF6

enum{
	MACHINE_NULL		= 0X00,
	MACHINE_HEARTBEAT,
	MACHINE_MAC,
	MACHINE_MOTO,
	MACHINE_READ_LOCATION,
	MACHINE_MASSAGE,
	MACHINE_FAN,
	MACHINE_HEAT,
	MACHINE_FIND,
	MACHINE_BLE_REST,
	MACHINE_BLE_DISCONNECT,
	MACHINE_UP_LIGHT,
	MACHINE_DOWN_LIGHT,
	MACHINE_UP_SHAKE,
	MACHINE_DOWN_SHAKE,
	MACHINE_UP_VOL,
	MACHINE_DOWN_VOL,
	MACHINE_LUMBAR
};
/*******************************************************
 *                  Type Definitions
 *******************************************************/
typedef struct{
	uint16_t machine_cmd;
	uint16_t cmd[5];
}BLE_RECEIVE_DATA_type;

typedef union
{
	BLE_RECEIVE_DATA_type cmd_data;
	uint8_t data[12];
}BLE_RECEIVE_BUF_type;

typedef struct{
	uint16_t machine_cmd;
	uint16_t data[5];
}BLE_SEND_DATA_type;

typedef union
{
	BLE_SEND_DATA_type cmd_data;
	uint8_t data[12];
}BLE_SEND_BUF_type;
typedef struct{
	uint16_t lenth;
	BLE_SEND_BUF_type ble_send_data;
}BLE_SEND_STRUCT_type;

/*******************************************************
 *                  Global Variables
 *******************************************************/
extern TaskHandle_t xBleModeHandle ;

extern TimerHandle_t xTimer_HeartBeatTimeout;

extern QueueHandle_t ble_rx_queue;
extern QueueHandle_t ble_mac_queue;
extern TimerHandle_t xTimer_UsartTimeout;
/*******************************************************
 *                  Function Prototypes
 *******************************************************/
void ble_control_Task(void* parameter);
void UsartTimeoutCallback(TimerHandle_t xTimer);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MODULE_H */
