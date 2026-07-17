/**
 * @file        ble_module.c
 * @author      KimQi
 * @date        2024-12-18
 * @copyright   Copyright (c) 2024, KimQi. All rights reserved.
 *              This file is part of an embedded project and is licensed
 *              under the terms specified in the LICENSE file.
 */

/*******************************************************
 *                  Include Files
 *******************************************************/
#include "ble_module.h"
#include "motor_hall.h"
#include "fan_heat_massage.h"
#include "sound_box.h"
#include "bsp_spi.h"
#include <string.h>
/*******************************************************
 *                  Macro Definitions
 *******************************************************/

/*******************************************************
 *                  Global Variables
 *******************************************************/
uint8_t ble_tx_buf[BLE_MAX_BUF_LEN] = {0};
uint8_t ble_rx_buf[BLE_MAX_BUF_LEN] = {0};

TaskHandle_t xBleModeHandle = NULL;

QueueHandle_t ble_rx_queue = NULL;
QueueHandle_t ble_mac_queue = NULL;
TimerHandle_t xTimer_UsartTimeout = NULL;

BLE_RECEIVE_BUF_type ble_receive;
BLE_SEND_STRUCT_type ble_send;

TimerHandle_t xTimer_HeartBeatTimeout = NULL;
/*******************************************************
 *                  Function Definitions
 *******************************************************/


void BLE_USART_init(void)
{
    GPIO_InitType GPIO_InitStructure;
    USART_InitType USART_InitStructure;
    NVIC_InitType NVIC_InitStructure;
	
    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOB);
    RCC_APB2_Peripheral_Clock_Enable(RCC_APB2_PERIPH_UART4);
	
    NVIC_InitStructure.NVIC_IRQChannel                   = UART4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority    = 4;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority           = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Initializes(&NVIC_InitStructure);
	
    GPIO_InitStructure.Pin            = GPIO_PIN_14;    
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF7_UART4;
    GPIO_Peripheral_Initialize(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.Pin            = GPIO_PIN_15;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF7_UART4;
    GPIO_Peripheral_Initialize(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.BaudRate            = 115200;
    USART_InitStructure.WordLength          = USART_WL_8B;
    USART_InitStructure.StopBits            = USART_STPB_1;
    USART_InitStructure.Parity              = USART_PE_NO;
    USART_InitStructure.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_InitStructure.Mode                = USART_MODE_RX | USART_MODE_TX;

    USART_Initializes(UART4, &USART_InitStructure);

    USART_Interrput_Enable(UART4, USART_INT_RXDNE);
    //USART_Interrput_Enable(UART4, USART_INT_IDLEF);

    USART_Enable(UART4);
}

void BLE_UART_DMA_Init(void)
{
    DMA_InitType DMA_InitStructure;
    NVIC_InitType NVIC_InitStructure;
    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_DMA);

    NVIC_Priority_Group_Set(NVIC_PER4_SUB0_PRIORITYGROUP);

    NVIC_InitStructure.NVIC_IRQChannel                   = DMA_Channel4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority           = 5;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Initializes(&NVIC_InitStructure);

    /* USARTy_Tx_DMA_Channel (triggered by USARTy Tx event) Config */
    DMA_Reset(DMA_CH2);
    DMA_InitStructure.PeriphAddr = (UART4_BASE + 0x04);
    DMA_InitStructure.MemAddr = (uint32_t)ble_tx_buf;
    DMA_InitStructure.Direction = DMA_DIR_PERIPH_DST;
    DMA_InitStructure.BufSize = BLE_MAX_BUF_LEN;
    DMA_InitStructure.PeriphInc = DMA_PERIPH_INC_MODE_DISABLE;
    DMA_InitStructure.MemoryInc = DMA_MEM_INC_MODE_ENABLE;
    DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_WIDTH_BYTE;
    DMA_InitStructure.MemDataSize = DMA_MEM_DATA_WIDTH_BYTE;
    DMA_InitStructure.CircularMode = DMA_CIRCULAR_MODE_DISABLE;
    DMA_InitStructure.Priority = DMA_CH_PRIORITY_HIGHEST;
    DMA_InitStructure.Mem2Mem = DMA_MEM2MEM_DISABLE;
    DMA_Initializes(DMA_CH2, &DMA_InitStructure);
    DMA_Interrupts_Enable(DMA_CH2, DMA_INT_TXC);
    DMA_Channel_Request_Remap(DMA_CH2, DMA_REMAP_UART4_TX);
}

void BLE_dma_send(uint8_t *buf, uint8_t len)
{
    DMA_Memory_Address_Config(DMA_CH2, (uint32_t)buf);
    DMA_Buffer_Size_Config(DMA_CH2, len);
    USART_DMA_Transfer_Enable(UART4, USART_DMAREQ_TX);
    DMA_Channel_Enable(DMA_CH2);
}


uint8_t check_sum(uint8_t *data, uint8_t size)
{
    int sum = 0;
    while (size--)
    {
        sum += *(unsigned char *)data++;
    }
    return sum;
}

uint8_t ble_rx_count = 0;
void UART4_IRQHandler(void)//主蓝牙芯片
{
    uint8_t rx_data;
//    uint8_t check;
    FlagStatus status;

    if (USART_Interrupt_Status_Get(UART4, USART_INT_RXDNE) == SET)
    {
        if(ble_rx_count<BLE_MAX_BUF_LEN)
        {
            ble_rx_buf[ble_rx_count++] = USART_Data_Receive(UART4);   //接收数据字节
        }
        xTimerResetFromISR(xTimer_UsartTimeout, 0);
        USART_Interrupt_Status_Clear(UART4, USART_INT_RXDNE);
    }

    if (USART_Interrupt_Status_Get(UART4, USART_INT_IDLEF) == SET)
    {
        // if(ble_rx_count > 4)
        // {
        //     check =  check_sum(ble_rx_buf,ble_rx_count-1);
        //     if(ble_rx_buf[0] == 0xd0 && ble_rx_buf[1] == 0xd0)
        //     {
        //         if(ble_rx_buf[ble_rx_count-1] == check)
        //         {
        //             xQueueSendFromISR(ble_rx_queue, &ble_rx_buf, NULL);
        //         }
        //     }
        // }
        // ble_rx_count = 0;
        status = USART_Interrupt_Status_Get(UART4,USART_INT_IDLEF);
        rx_data = USART_Data_Receive(UART4);
    }
}

void UsartTimeoutCallback(TimerHandle_t xTimer)//仓体蓝牙芯片发过来的数据
{
    uint8_t check;
    if(ble_rx_count > 4)
    {
        check =  check_sum(ble_rx_buf,ble_rx_count-1);
        if(ble_rx_buf[0] == 0x55 && ble_rx_buf[1] == 0xaa)
        {
            if(ble_rx_buf[ble_rx_count-1] == check)
            {
                xQueueSend(ble_rx_queue, &ble_rx_buf, NULL);
            }
        }
    }
    ble_rx_count = 0;
}


void ble_control_Task(void* parameter)
{
	uint8_t CMD;
	uint8_t rgb_d = 0;
	uint32_t find_data = 0;
	uint16_t motor_cmd_t[MAX_MOTOR_NUM] = {0};
	uint8_t machine_mac[6];
    uint8_t ble_rx_buf[BLE_MAX_BUF_LEN] = {0};
    uint8_t ble_tx_count = 0;
	Fan_Heat_Massage_Typedef notfiy_Fan_Heat_Massage_Data1;
	Fan_Heat_Massage_Typedef set_fan_heat_massage_temp;
    uint8_t reset_flag = 0;
    uint8_t light_data = 0;
    BLE_UART_DMA_Init();
    BLE_USART_init();
    while (1)
    {
		if (xTaskNotifyWait(0, 0, &find_data, 0) == pdTRUE)//收到搜寻按键消息，发给BLE通知遥控器
		{
			ble_tx_count = 0;
			ble_tx_buf[ble_tx_count++] = 0x55;
			ble_tx_buf[ble_tx_count++] = 0xAA;
			ble_tx_buf[ble_tx_count++] = MACHINE_FIND;
			ble_tx_buf[ble_tx_count++] = 5;
			ble_tx_buf[ble_tx_count] = check_sum(ble_tx_buf,ble_tx_count);
			BLE_dma_send(ble_tx_buf,++ble_tx_count);
		}
        if (xQueueReceive(ble_rx_queue, &ble_rx_buf, 0) == pdTRUE)//BLE通过串口发过来的数据
        {
			uint8_t lumbar_support_cmd = 0;
			switch(ble_rx_buf[2])
			{
				case MACHINE_HEARTBEAT://心跳
					xTimerReset(xTimer_HeartBeatTimeout, 0);
				break;
				case MACHINE_READ_LOCATION:
					ble_tx_count = 0;
					ble_tx_buf[ble_tx_count++] = 0x55;
					ble_tx_buf[ble_tx_count++] = 0xAA;
					ble_tx_buf[ble_tx_count++] = MACHINE_READ_LOCATION;
					ble_tx_buf[ble_tx_count++] = 11;
					ble_tx_buf[ble_tx_count++] = motor[0].cur_step>>8;
					ble_tx_buf[ble_tx_count++] = motor[0].cur_step&0x00ff;
					ble_tx_buf[ble_tx_count++] = motor[1].cur_step>>8;
					ble_tx_buf[ble_tx_count++] = motor[1].cur_step&0x00ff;
					ble_tx_buf[ble_tx_count++] = motor[2].cur_step>>8;
					ble_tx_buf[ble_tx_count++] = motor[2].cur_step&0x00ff;
					ble_tx_buf[ble_tx_count] = check_sum(ble_tx_buf,ble_tx_count);
					BLE_dma_send(ble_tx_buf,++ble_tx_count);
				break;
				case MACHINE_MOTO:
					if(ble_rx_buf[3] == 11)
					{
						motor_cmd_t[0] = 	ble_rx_buf[4];
						motor_cmd_t[0] = 	motor_cmd_t[0]<<8;
						motor_cmd_t[0] |= 	ble_rx_buf[5];
						motor_cmd_t[1] = 	ble_rx_buf[6];
						motor_cmd_t[1] = 	motor_cmd_t[1]<<8;
						motor_cmd_t[1] |= 	ble_rx_buf[7];
						motor_cmd_t[2] = 	ble_rx_buf[8];
						motor_cmd_t[2] = 	motor_cmd_t[2]<<8;
						motor_cmd_t[2] |= 	ble_rx_buf[9];

						if ((motor_cmd_t[0] == pull) &&
							(motor_cmd_t[1] == pull) &&
							(motor_cmd_t[2] == pull))
						{
							Massage_Motor_Set_All_Retract(1U);
						}
						else if ((motor_cmd_t[0] == stop) &&
								 (motor_cmd_t[1] == stop) &&
								 (motor_cmd_t[2] == stop))
						{
							Massage_Motor_Set_All_Retract(0U);
						}
						xQueueSendFromISR( motor_queue, &motor_cmd_t, NULL);
					}
				break;
				case MACHINE_MASSAGE:
					set_fan_heat_massage_temp.msg_select = 2;
					set_fan_heat_massage_temp.Massage_level = ble_rx_buf[4];
					xQueueSend(Set_Fan_Heat_Massage_Queue1, &set_fan_heat_massage_temp, 0);
				break;
				case MACHINE_LUMBAR:
					Massage_Motor_Set_Lumbar(ble_rx_buf[4]);
					set_fan_heat_massage_temp.msg_select = 1;
					set_fan_heat_massage_temp.Massage_level = ble_rx_buf[4];
					xQueueSend(Set_Fan_Heat_Massage_Queue1, &set_fan_heat_massage_temp, 0);
				break;
				case MACHINE_FAN:
					set_fan_heat_massage_temp.msg_select = 3;
					set_fan_heat_massage_temp.fan_level = ble_rx_buf[4];
					xQueueSend(Set_Fan_Heat_Massage_Queue1, &set_fan_heat_massage_temp, 0);
				break;
				case MACHINE_HEAT:
					set_fan_heat_massage_temp.msg_select = 4;
					set_fan_heat_massage_temp.hot_level = ble_rx_buf[4];
					xQueueSend(Set_Fan_Heat_Massage_Queue1, &set_fan_heat_massage_temp, 0);
				break;
				case MACHINE_BLE_REST:
					CMD = CMD_AURACAST_ON_OFF;
                    xQueueSend(Sound_Box_CMD_Queue, &CMD, 0);
				break;
				case MACHINE_BLE_DISCONNECT:
					CMD = CMD_BLE_DISABLE;
                    xQueueSend(Sound_Box_CMD_Queue, &CMD, 0);
				break;
				case MACHINE_UP_LIGHT:
					rgb_d = RGB_COLOR_UP;
                    xQueueSend(RGB_Cmd_Queue, &rgb_d, 0);
				break;
				case MACHINE_DOWN_LIGHT:
					rgb_d = RGB_COLOR_DOWN;
                    xQueueSend(RGB_Cmd_Queue, &rgb_d, 0);
				break;
				case MACHINE_UP_SHAKE:
					CMD = CMD_VIBRATE_UP;
                    xQueueSend(Sound_Box_CMD_Queue, &CMD, 0);
				break;
				case MACHINE_DOWN_SHAKE:
					CMD = CMD_VIBRATE_DOWN;
                    xQueueSend(Sound_Box_CMD_Queue, &CMD, 0);
				break;
				case MACHINE_UP_VOL:
					CMD = CMD_VOLUME_UP;
                    xQueueSend(Sound_Box_CMD_Queue, &CMD, 0);
				break;
				case MACHINE_DOWN_VOL:
					CMD = CMD_VOLUME_DOWN;
                    xQueueSend(Sound_Box_CMD_Queue, &CMD, 0);
				break;
			}          
        }
		if (xQueueReceive(cs_status_Queue, &notfiy_Fan_Heat_Massage_Data1, 0) == pdTRUE)//风机更新过来的状态，通过串口发给BLE跟新给手控器
		{
			ble_tx_count = 0;
			ble_tx_buf[ble_tx_count++] = 0x55;
			ble_tx_buf[ble_tx_count++] = 0xAA;
			ble_tx_buf[ble_tx_count++] = MACHINE_MASSAGE;
			ble_tx_buf[ble_tx_count++] = 8;
			ble_tx_buf[ble_tx_count++] = notfiy_Fan_Heat_Massage_Data1.Massage_level;
			ble_tx_buf[ble_tx_count++] = notfiy_Fan_Heat_Massage_Data1.fan_level;
			ble_tx_buf[ble_tx_count++] = notfiy_Fan_Heat_Massage_Data1.hot_level;
			ble_tx_buf[ble_tx_count] = check_sum(ble_tx_buf,ble_tx_count);
			BLE_dma_send(ble_tx_buf,++ble_tx_count);
		}
    }
}