//-----------------------------------------------------------------------------/
//文件名	称:DLXM-0376
//描	述：第二款 黄丽珍 入仓式控制盒MCU程序(主机程序 带蓝牙音箱和灯带)
//配置参数:	双电机，通风，按摩，加热 ，音箱，灯带控制
//			与主蓝牙芯片串口通讯，
//修改记录：
//-----------------------------------------------------------------------------/
#include "main.h"
#include "motor_hall.h"
#include "ble_module.h"
#include "remote_control.h"
#include "fan_heat_massage.h"
#include "key.h"
#include "sound_box.h"
#include "bsp_spi.h"

ErrorStatus HSIStartUpStatus;
volatile uint32_t FreeRTOSFaultCode = 0U;
volatile TaskHandle_t FreeRTOSFaultTaskHandle = NULL;
volatile const char *FreeRTOSFaultTaskName = NULL;

void vApplicationMallocFailedHook(void)
{
    FreeRTOSFaultCode = 1U;
    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    FreeRTOSFaultTaskName = pcTaskName;
    FreeRTOSFaultCode = 2U;
    FreeRTOSFaultTaskHandle = xTask;
    taskDISABLE_INTERRUPTS();
    while (1)
    {
    }
}

void system_clock_init(void)
{
    /* Set up the system clock */
    RCC_Reset();
    /* Enable HSI */
    RCC_HSI_Enable();
    
    /* Wait till HSI is ready */
    HSIStartUpStatus = RCC_HSI_Stable_Wait();

    if (HSIStartUpStatus != SUCCESS)
    {
        /* If HSI fails to start-up, the application will have wrong clock
            configuration. User can add here some code to deal with this
            error */

        /* Go to infinite loop */
        while (1);
    }

    FLASH_Latency_Set(FLASH_LATENCY_3);
    /* HCLK = SYSCLK */
    RCC_Hclk_Config(RCC_SYSCLK_DIV1);

    /* PCLK2 = HCLK */
    RCC_Pclk2_Config(RCC_SYSCLK_DIV4);

    /* PCLK1 = HCLK */
    RCC_Pclk1_Config(RCC_SYSCLK_DIV4);

    RCC_PLL_Config(RCC_PLL_SRC_HSI_DIV2,RCC_PLL_MUL_32);

    /* Enable PLL */
    RCC_PLL_Enable();
    /* Wait till PLL is ready */
    while ((RCC->CTRL & RCC_CTRL_PLLRDF) == 0)
    {
    }
    /* Select PLL as system clock source */
    RCC_Sysclk_Config(RCC_SYSCLK_SRC_PLLCLK);

    /* Wait till PLL is used as system clock source */
    while (RCC_Sysclk_Source_Get() != RCC_CFG_SCLKSTS_PLL);
}

int main(void)
{
	BaseType_t xReturn = pdPASS; 

	system_clock_init();

	NVIC_Priority_Group_Set(NVIC_PER4_SUB0_PRIORITYGROUP);
	//SEGGER_SYSVIEW_Conf();
	taskENTER_CRITICAL();	

	motor_queue = xQueueCreate((UBaseType_t)30, (UBaseType_t)(MAX_MOTOR_NUM*2));
	RGB_SPI_Send_Queue = xQueueCreate((UBaseType_t)10, (UBaseType_t)sizeof(rgb_sys_t));
	RGB_Cmd_Queue = xQueueCreate((UBaseType_t)20, (UBaseType_t)sizeof(uint8_t));
	lvdong_Queue  = xQueueCreate((UBaseType_t)1, (UBaseType_t)sizeof(uint8_t));
	Set_Fan_Heat_Massage_Queue1 = xQueueCreate((UBaseType_t)10, sizeof(Fan_Heat_Massage_Typedef));
	Get_Fan_Heat_Massage_Queue1 = xQueueCreate((UBaseType_t)10, sizeof(Fan_Heat_Massage_Typedef));
	ble_rx_queue = xQueueCreate((UBaseType_t)10, (UBaseType_t)BLE_MAX_BUF_LEN);
	ble_mac_queue = xQueueCreate((UBaseType_t)10, (UBaseType_t)6);
	keyEventQueue = xQueueCreate(10, sizeof(KeyEventInfo_t));
	cs_status_Queue = xQueueCreate(10, sizeof(Fan_Heat_Massage_Typedef));
	Sound_Box_CMD_Queue = xQueueCreate((UBaseType_t)1, (UBaseType_t)sizeof(uint8_t));
	Sound_Box_Reply_Queue = xQueueCreate((UBaseType_t)5, (UBaseType_t)sizeof(Sound_CMD_Typedef));
//	RGB_SPI_Send_Queue = xQueueCreate((UBaseType_t)10, (UBaseType_t)sizeof(rgb_sys_t));
//	Sound_Box_CMD_Queue = xQueueCreate((UBaseType_t)5, (UBaseType_t)sizeof(Sound_CMD_Typedef));
//	Sound_Box_Reply_Queue = xQueueCreate((UBaseType_t)5, (UBaseType_t)sizeof(Sound_CMD_Typedef));
	
	xTimer_UsartTimeout = xTimerCreate(
       					"SingleShotTimer",         
       					pdMS_TO_TICKS(10),       
       					pdFALSE,                   
       					0,                          
       					UsartTimeoutCallback);
	xTimerStop(xTimer_UsartTimeout, 0);
	
	xTimer_HeartBeatTimeout = xTimerCreate(
       					"heartBeatTimer",         
       					pdMS_TO_TICKS(1000),       
       					pdFALSE,                   
       					0,                          
       					vDummyCallback);
	xTimerStop(xTimer_HeartBeatTimeout, 0);
	
	xTimer_UsartSoundTimeout = xTimerCreate(
      					"SingleShotTimer",         
      					pdMS_TO_TICKS(5),       
      					pdFALSE,                   
      					(void*	)4,                          
      					UsartSoundTimeoutCallback);
	xTimerStop(xTimer_UsartSoundTimeout, 0);
	
	xTimer_RGBTimeout = xTimerCreate(
      					"xTimer_RGBTimeout",         
      					14400000,       
      					pdFALSE,                   
      					(void*	)5,                          
      					rgb_auto_off_callback);
	xTimerStop(xTimer_RGBTimeout, 0);
	

	xReturn = xTaskCreate((TaskFunction_t)ble_control_Task,
						  (const char *)"ble_control_Task",
						  (uint16_t)128,						
						  (void *)NULL,						
						  (UBaseType_t)1,						
						  &xBleModeHandle);
	if (pdPASS != xReturn) return -1;
						  
	xReturn = xTaskCreate((TaskFunction_t)Remote_Task,
						  (const char *)"remote_Task",
						  (uint16_t)128,						
						  (void *)NULL,						
						  (UBaseType_t)1,						
						  NULL);
	if (pdPASS != xReturn) return -1;
						  
	xReturn = xTaskCreate((TaskFunction_t)bsp_spi_Task, 
						  (const char *)"bsp_spi_Task",
						  (uint16_t)96,
						  (void *)NULL,						 
						  (UBaseType_t)1,						  
						  (TaskHandle_t *)rgb_spi_Handle);	
	if (pdPASS != xReturn) return -1;

	xReturn = xTaskCreate((TaskFunction_t)MotoInitTask,
						  (const char *)"moto_init_Task",
						  (uint16_t)128,						
						  (void *)NULL,						
						  (UBaseType_t)1,						
						  NULL);
	if (pdPASS != xReturn) return -1;
						  
	xReturn = xTaskCreate((TaskFunction_t)fan_heat_massage1_tx_Task,
						  (const char *)"fan_heat_massage_Task",
						  (uint16_t)128,						
						  (void *)NULL,						
						  (UBaseType_t)1,						
						  NULL);
	if (pdPASS != xReturn) return -1;
						  
	xReturn = xTaskCreate((TaskFunction_t)Sound_Box_Task, 
						  (const char *)"Sound_Box_Task",
						  (uint16_t)96,
						  (void *)NULL,						 
						  (UBaseType_t)1,						  
						  NULL);
	if (pdPASS != xReturn) return -1;

	xReturn = xTaskCreate((TaskFunction_t)Sound_Box_Config_Task, 
						  (const char *)"Sound_Box_Config_Task",
						  (uint16_t)96,
						  (void *)NULL,						 
						  (UBaseType_t)1,						  
						  NULL);
	if (pdPASS != xReturn) return -1;
						  
	xReturn = xTaskCreate((TaskFunction_t)KeyScanTask,
						  (const char *)"KeyScanTask",
						  (uint16_t)96,
						  (void *)NULL,						
						  (UBaseType_t)1,						
						  NULL);
	if (pdPASS != xReturn) return -1;
						  
	xReturn = xTaskCreate((TaskFunction_t)KeyEventHandlerTask,
						  (const char *)"KeyEventHandlerTask",
						  (uint16_t)96,
						  (void *)NULL,						
						  (UBaseType_t)1,						
						  NULL);
	if (pdPASS != xReturn) return -1;		


	taskEXIT_CRITICAL();
	
	if (pdPASS == xReturn)
		vTaskStartScheduler(); 
	else
		return -1;

	while (1)
		;
}



