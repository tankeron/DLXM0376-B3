#include "motor_hall.h"

#if ENABLE_HALL

motor_port_t motor_port[MAX_MOTOR_NUM] = {
    {GPIOB,GPIO_PIN_10,GPIOB,GPIO_PIN_2,    GPIOA,GPIO_PIN_8,GPIOA,GPIO_PIN_9},
    {GPIOA,GPIO_PIN_7,GPIOA,GPIO_PIN_6,     GPIOA,GPIO_PIN_10,GPIOA,GPIO_PIN_11},
    {GPIOA,GPIO_PIN_4,GPIOA,GPIO_PIN_5,     GPIOD,GPIO_PIN_12,GPIOA,GPIO_PIN_12},
    {GPIOC,GPIO_PIN_15,GPIOD,GPIO_PIN_14,   GPIOB,GPIO_PIN_4,GPIOB,GPIO_PIN_3,},
    {GPIOC,GPIO_PIN_14,GPIOD,GPIO_PIN_15,   GPIOB,GPIO_PIN_5,GPIOB,GPIO_PIN_6}
};
#endif

#if !ENABLE_HALL
uint16_t ADCConvertedValue[MAX_MOTOR_NUM] = {0};
#define ADC_LEN     ADC_REGULAR_LEN_2
motor_port_t motor_port[MAX_MOTOR_NUM] ={
	{GPIOA,GPIO_PIN_10,GPIOA,GPIO_PIN_11,GPIOB,GPIO_PIN_12,ADC_Channel_14_PB12},
    {GPIOA,GPIO_PIN_9,GPIOA,GPIO_PIN_8,GPIOB,GPIO_PIN_13,ADC_Channel_15_PB13},
	{GPIOA,GPIO_PIN_12,GPIOB,GPIO_PIN_1,GPIOA,GPIO_PIN_0,ADC_Channel_01_PA0}
};
#endif

QueueHandle_t motor_queue = NULL;
TaskHandle_t Motor_test_Task_Handle = NULL;

motor_t motor[MAX_MOTOR_NUM];
uint16_t sys_save_step[2][MAX_MOTOR_NUM];

uint32_t flash_data[9*MAX_MOTOR_NUM+1] = {0};

void Motor_Gpio_Initialize(GPIO_Module* GPIOx, uint16_t pin)
{
    /* Define a structure of type GPIO_InitType */
    GPIO_InitType GPIO_InitStructure;

    /* Enable LED related GPIO peripheral clock */
    if(GPIOx == GPIOA)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOA);
    }
    else if(GPIOx == GPIOB)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOB);
    }
    else if(GPIOx == GPIOC)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOC);
    }
    else
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOD);
    }

    if(pin < GPIO_PIN_ALL)
    {
        /* Assign default value to GPIO_InitStructure structure */
        GPIO_Structure_Initialize(&GPIO_InitStructure);
        
        /* Select the GPIO pin to control */
        GPIO_InitStructure.Pin          = pin;
        /* Set pin mode to general push-pull output */
        GPIO_InitStructure.GPIO_Mode    = GPIO_MODE_OUT_PP;
        /* Set the pin drive current to 4MA*/
        GPIO_InitStructure.GPIO_Current = GPIO_DS_4MA;
        /* Initialize GPIO */
        GPIO_Peripheral_Initialize(GPIOx, &GPIO_InitStructure);
    }
}

void Hall_Input_Initialize(GPIO_Module* GPIOx, uint16_t pin)
{
    /* Define a structure of type GPIO_InitType */
    GPIO_InitType GPIO_InitStructure;

    /* Enable KEY related GPIO peripheral clock */
    if(GPIOx == GPIOA)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOA);
    }
    else if(GPIOx == GPIOB)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOB);
    }
    else if(GPIOx == GPIOC)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOC);
    }
    else
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOD);
    }

    if(pin < GPIO_PIN_ALL)
    {
        /* Assign default value to GPIO_InitStructure structure */
        GPIO_Structure_Initialize(&GPIO_InitStructure);
        
        GPIO_InitStructure.Pin       = pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_MODE_INPUT;
        GPIO_InitStructure.GPIO_Pull = GPIO_NO_PULL;
        /* Initialize GPIO */
        GPIO_Peripheral_Initialize(GPIOx, &GPIO_InitStructure);
    }
}

void ADC_input_gpio_Initialize(GPIO_Module* GPIOx, uint16_t pin)
{
    /* Define a structure of type GPIO_InitType */
    GPIO_InitType GPIO_InitStructure;

    /* Enable KEY related GPIO peripheral clock */
    if(GPIOx == GPIOA)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOA);
    }
    else if(GPIOx == GPIOB)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOB);
    }
    else if(GPIOx == GPIOC)
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOC);
    }
    else
    {
        RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOD);
    }

    if(pin < GPIO_PIN_ALL)
    {
        /* Assign default value to GPIO_InitStructure structure */
        GPIO_Structure_Initialize(&GPIO_InitStructure);
        
        GPIO_InitStructure.Pin       = pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_MODE_ANALOG;
        /* Initialize GPIO */
        GPIO_Peripheral_Initialize(GPIOx, &GPIO_InitStructure);
    }
}

void dma_config(void)
{
    /* DMA channel1 configuration ----------------------------------------------*/
#if !ENABLE_HALL
    DMA_InitType DMA_InitStructure;
	RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_DMA);
    DMA_Reset(DMA_CH4);
    DMA_InitStructure.PeriphAddr     = (uint32_t)&ADC->DAT;
    DMA_InitStructure.MemAddr        = (uint32_t)&ADCConvertedValue;
    DMA_InitStructure.Direction      = DMA_DIR_PERIPH_SRC;
    DMA_InitStructure.BufSize        = MAX_MOTOR_NUM;
    DMA_InitStructure.PeriphInc      = DMA_PERIPH_INC_MODE_DISABLE;
    DMA_InitStructure.MemoryInc      = DMA_MEM_INC_MODE_ENABLE;
    DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_WIDTH_HALFWORD;
    DMA_InitStructure.MemDataSize    = DMA_MEM_DATA_WIDTH_HALFWORD;
    DMA_InitStructure.CircularMode   = DMA_CIRCULAR_MODE_ENABLE;
    DMA_InitStructure.Priority       = DMA_CH_PRIORITY_HIGH;
    DMA_InitStructure.Mem2Mem        = DMA_MEM2MEM_DISABLE;
    DMA_Initializes(DMA_CH4, &DMA_InitStructure);
    DMA_Channel_Request_Remap(DMA_CH4, DMA_REMAP_ADC);
    /* Enable DMA channel1 */
    DMA_Channel_Enable(DMA_CH4);
#endif
}

void adc_config(void)
{
    uint8_t i;
    ADC_InitType ADC_InitStructure;
	
    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_ADC);
    ADC_Clock_Mode_Config(ADC_CKMOD_AHB, RCC_ADCHCLK_DIV16);
    RCC_ADC_1M_Clock_Config(RCC_ADC1MCLK_SRC_HSI, RCC_ADC1MCLK_DIV8);  //selsect HSI as RCC ADC1M CLK Source

    ADC_Initializes_Structure(&ADC_InitStructure);
    ADC_InitStructure.MultiChEn      = ENABLE;
    ADC_InitStructure.ContinueConvEn = ENABLE;
    ADC_InitStructure.DatAlign       = ADC_DAT_ALIGN_R;
    ADC_InitStructure.ExtTrigSelect  = ADC_EXT_TRIGCONV_REGULAR_SWSTRRCH;
    ADC_InitStructure.ChsNumber      = ADC_LEN;
    ADC_Initializes(&ADC_InitStructure);
    
    /* ADC channel sampletime configuration */
    for (uint8_t i = 0; i < MAX_MOTOR_NUM; i++)
    {
        /* code */
        ADC_Channel_Sample_Time_Config(motor_port[i].ADC_channel, ADC_SAMP_TIME_55CYCLES5);
    }

    for (uint8_t i = 0; i < MAX_MOTOR_NUM; i++)
    {
        /* code */
        ADC_Regular_Sequence_Conversion_Number_Config(motor_port[i].ADC_channel, i);
    }
    
    /* Enable ADC DMA */
    ADC_DMA_Transfer_Enable();

    /* Enable ADC */
    ADC_ON();
    
    /* Check ADC Ready */
    while(ADC_Flag_Status_Get(ADC_RD_FLAG, ADC_FLAG_JENDCA, ADC_FLAG_RDY) == RESET)
        ;
    
    /* Start ADC1 calibration */
    ADC_Calibration_Operation(ADC_CALIBRATION_ENABLE);
    /* Check the end of ADC1 calibration */
    while (ADC_Calibration_Operation(ADC_CALIBRATION_STS))
        ;
    /* Start ADC Software Conversion */
    ADC_Regular_Channels_Software_Conversion_Operation(ADC_EXTRTRIG_SWSTRRCH_ENABLE);
}

void Motor_config(void)
{
    for (uint8_t i = 0; i < MAX_MOTOR_NUM; i++)
    {
        /* code */
        Motor_Gpio_Initialize(motor_port[i].MA_Port, motor_port[i].MA_pin);
        Motor_Gpio_Initialize(motor_port[i].MB_Port, motor_port[i].MB_pin);
#if ENABLE_HALL
        GPIO_Pins_Reset(motor_port[i].HA_port, motor_port[i].HA_pin);
        GPIO_Pins_Reset(motor_port[i].HB_port, motor_port[i].HB_pin);

        Hall_Input_Initialize(motor_port[i].HA_port, motor_port[i].HA_pin);
        Hall_Input_Initialize(motor_port[i].HB_port, motor_port[i].HB_pin);
#else
        ADC_input_gpio_Initialize(motor_port[i].ADC_Port, motor_port[i].ADC_pin);
#endif
    }

#if !ENABLE_HALL
    dma_config();
    adc_config();
#endif
}

void motor_push(uint16_t motor_id)
{
    GPIO_Pins_Set(motor_port[motor_id].MA_Port, motor_port[motor_id].MA_pin);
    GPIO_Pins_Reset(motor_port[motor_id].MB_Port, motor_port[motor_id].MB_pin);
}

void motor_pull(uint16_t motor_id)
{
    GPIO_Pins_Reset(motor_port[motor_id].MA_Port, motor_port[motor_id].MA_pin);
    GPIO_Pins_Set(motor_port[motor_id].MB_Port, motor_port[motor_id].MB_pin);
}

void motor_stop(uint16_t motor_id)
{
    GPIO_Pins_Reset(motor_port[motor_id].MA_Port, motor_port[motor_id].MA_pin);
    GPIO_Pins_Reset(motor_port[motor_id].MB_Port, motor_port[motor_id].MB_pin);
}

void flash_write(void)
{
    uint32_t Counter_Num = 0;
    taskENTER_CRITICAL();
    /* Unlocks the FLASH Program Erase Controller */
    FLASH_Unlock();
    FLASH_One_Page_Erase(FLASH_ADDR);
    for(Counter_Num = 0;Counter_Num < (2*MAX_MOTOR_NUM+1)*4; Counter_Num+=4)
    {
        FLASH_Word_Program(FLASH_ADDR+Counter_Num, flash_data[Counter_Num/4]);
    }
    /* Locks the FLASH Program Erase Controller */
    FLASH_Lock();
    taskEXIT_CRITICAL();
}

void flash_read(void)
{
    uint32_t Counter_Num = 0;
    taskENTER_CRITICAL();
    for(Counter_Num = 0;Counter_Num < (2*MAX_MOTOR_NUM+1)*4; Counter_Num+=4)
    {
        flash_data[Counter_Num/4] = (*(__IO uint32_t*)(FLASH_ADDR + Counter_Num));
    }
    taskEXIT_CRITICAL();
}

void save_motor_step(uint8_t save_id)
{
    flash_read();
    flash_data[save_id+1] = motor[save_id].cur_step;
    flash_write();
}

void read_step(uint8_t save_id)
{
    flash_read();
    motor[save_id].cur_step = flash_data[save_id+1];
}

//void test_Task(void* parameter)
//{
//    motor_msg_t motor_cmd;
//    while (1)
//    {
//        motor_cmd.motor_id = 0;
//        motor_cmd.state = pull;
//        motor_cmd.step = 0;
//        xQueueSend(motor_queue, &motor_cmd, portMAX_DELAY);
//        motor_cmd.motor_id = 1;
//        xQueueSend(motor_queue, &motor_cmd, portMAX_DELAY);
//        vTaskDelay(15000);

//        motor_cmd.motor_id = 0;
//        motor_cmd.state = push;
//        motor_cmd.step = 500;
//        xQueueSend(motor_queue, &motor_cmd, portMAX_DELAY);
//        motor_cmd.motor_id = 1;
//        xQueueSend(motor_queue, &motor_cmd, portMAX_DELAY);
//        vTaskDelay(15000);

//        motor_cmd.motor_id = 0;
//        motor_cmd.state = push;
//        motor_cmd.step = MAX_MOTOR_STEP;
//        xQueueSend(motor_queue, &motor_cmd, portMAX_DELAY);
//        motor_cmd.motor_id = 1;
//        xQueueSend(motor_queue, &motor_cmd, portMAX_DELAY);
//        vTaskDelay(15000);

//        motor_cmd.motor_id = 0;
//        motor_cmd.state = push;
//        motor_cmd.step = 500;
//        xQueueSend(motor_queue, &motor_cmd, portMAX_DELAY);
//        motor_cmd.motor_id = 1;
//        xQueueSend(motor_queue, &motor_cmd, portMAX_DELAY);
//        vTaskDelay(15000);
//    }
//}
void MotoInitTask(void *parameter)
{
	BaseType_t xReturn = pdPASS; 
	
    Motor_config();
	for (uint8_t i = 0; i < MAX_MOTOR_NUM; i++)
    {
        motor[i].state = stop;
        motor[i].cur_step = 0;
        motor[i].set_step = 0;
        motor[i].count = MAX_TIMEROUT+1;
        read_step(i);
		motor_stop(i);
    }
	vTaskDelay(5000);//延迟两秒
	for (uint8_t i = 0; i < MAX_MOTOR_NUM; i++)
    {
        motor[i].adc_zero = ADCConvertedValue[i];
    }
	xReturn = xTaskCreate((TaskFunction_t)Motor_Task,
						  (const char *)"Motor_Task",
						  (uint16_t)128,						
						  (void *)NULL,						
						  (UBaseType_t)1,						
						  NULL);
	// 3. 删除自己
    vTaskDelete(NULL);
}
void Motor_Task(void* parameter)
{
    uint16_t motor_cmd[MAX_MOTOR_NUM];
#if ENABLE_HALL
        INTStatus Last_HallA_Level[MAX_MOTOR_NUM];
        INTStatus Last_HallB_Level[MAX_MOTOR_NUM];
#endif
    

//    for (uint8_t i = 0; i < MAX_MOTOR_NUM; i++)
//    {
//        motor[i].state = stop;
//        motor[i].cur_step = 0;
//        motor[i].set_step = 0;
//        motor[i].count = MAX_TIMEROUT+1;
//        read_step(i);
//    }
//    Motor_config();
    while (1)
    {
        /* code */
        if (xQueueReceive(motor_queue, &motor_cmd, 2) == pdTRUE)
        {
			for(uint8_t j = 0;j<MAX_MOTOR_NUM;j++)
			{
				if(motor_cmd[j] == stop)
				{
					motor[j].state = stop;
					motor_stop(j);
				}
				else
				{
					if(motor_cmd[j] == pull)
                    {
                        motor[j].state = pull;
                        motor[j].set_step = motor_cmd[j];
                        motor_pull(j);
                    }
                    else if(motor_cmd[j] == MAX_MOTOR_STEP)
                    {
                        motor[j].state = push;
                        motor[j].set_step = motor_cmd[j];
                        motor_push(j);
                    }
                    else
                    {
                        if(motor[j].cur_step < motor_cmd[j]-MIN_ERR)
                        {
                            motor[j].state = push;
                            motor[j].set_step = motor_cmd[j];
                            motor_push(j);
                        }

                        if(motor[j].cur_step > motor_cmd[j]+MIN_ERR)
                        {
                            motor[j].state = pull;
                            motor[j].set_step = motor_cmd[j];
                            motor_pull(j);
                        }
                    }
					motor[j].count = 0;
				}
			}
        }
		

        for (uint8_t i = 0; i < MAX_MOTOR_NUM; i++)
        {
#if ENABLE_HALL
            /* code */
            INTStatus cur_level = RESET;
            if(GPIO_Input_Pin_Data_Get(motor_port[i].HA_port, motor_port[i].HA_pin) == PIN_SET) cur_level = SET;
            else cur_level = RESET;

            if(Last_HallA_Level[i] == PIN_RESET && cur_level == SET)
            {
                if(GPIO_Input_Pin_Data_Get(motor_port[i].HB_port, motor_port[i].HB_pin) == PIN_SET)
                {
                    if(motor[i].cur_step > 0) motor[i].cur_step--;
                }
                else
                {
                    if(motor[i].cur_step < MAX_MOTOR_STEP) motor[i].cur_step++;
                }
                motor[i].count = 0;
            }
            else if(Last_HallA_Level[i] == PIN_SET && cur_level == PIN_RESET)
            {
                if(GPIO_Input_Pin_Data_Get(motor_port[i].HB_port, motor_port[i].HB_pin) == PIN_SET)
                {
                    if(motor[i].cur_step < MAX_MOTOR_STEP)  motor[i].cur_step++;
                }
                else
                {
                    if(motor[i].cur_step > 0) motor[i].cur_step--;
                }
                motor[i].count = 0;
            }
			Last_HallA_Level[i] = cur_level;
            
            if(GPIO_Input_Pin_Data_Get(motor_port[i].HB_port, motor_port[i].HB_pin) == PIN_SET) cur_level = SET;
            else cur_level = RESET;
            if(Last_HallB_Level[i] == PIN_RESET && cur_level == SET)
            {
                if(GPIO_Input_Pin_Data_Get(motor_port[i].HA_port, motor_port[i].HA_pin) == PIN_SET)
                {
                    if(motor[i].cur_step < MAX_MOTOR_STEP)  motor[i].cur_step++;
                }
                else
                {
                    if(motor[i].cur_step > 0) motor[i].cur_step--;
                }
                motor[i].count = 0;
            }
            else if(Last_HallB_Level[i] == PIN_SET && cur_level == PIN_RESET)
            {
                if(GPIO_Input_Pin_Data_Get(motor_port[i].HA_port, motor_port[i].HA_pin) == PIN_SET)
                {
                    if(motor[i].cur_step > 0) motor[i].cur_step--;
                }
                else
                {
                    if(motor[i].cur_step < MAX_MOTOR_STEP)  motor[i].cur_step++;
                }
                motor[i].count = 0;
            }
			Last_HallB_Level[i] = cur_level;
            if(motor[i].set_step > 0 && motor[i].set_step < MAX_MOTOR_STEP)
            {
                if(motor[i].state == pull)
                {
                    if(motor[i].cur_step <= motor[i].set_step+MIN_ERR)
                    {
                        motor_stop(i);
                        motor[i].state = stop;
                    }
                }
                else if(motor[i].state == push)
                {
                    if(motor[i].cur_step >= motor[i].set_step-MIN_ERR)
                    {
                        motor_stop(i);
                        motor[i].state = stop;
                    }
                }
            }            
#endif

#if !ENABLE_HALL

            if(motor[i].state == push)
            {
                motor[i].cur_step = (motor[i].cur_step < MAX_MOTOR_STEP) ? motor[i].cur_step+1 : MAX_MOTOR_STEP;
            }
            else if(motor[i].state == pull)
            {
                motor[i].cur_step = (motor[i].cur_step > 0) ? motor[i].cur_step-1 : 0;
            }

            if(motor[i].set_step > 1 && motor[i].set_step < MAX_MOTOR_STEP)
            {
                if(motor[i].set_step == motor[i].cur_step)
                {
                    motor_stop(i);
                    motor[i].state = stop;
                }
            }
            
            if(ADCConvertedValue[i] > (MOTOR_STOP_POWER + motor[i].adc_zero)) 
            {
                motor[i].count = 0;
            }
#endif
            if(motor[i].count < MAX_TIMEROUT)
            {
                motor[i].count++;
            }
            else
            {
                if(motor[i].count == MAX_TIMEROUT)
                {
                    if(motor[i].state == pull)
                    {
                        motor[i].cur_step = 0;
                    }
                    motor_stop(i);
                    save_motor_step(i);
                    motor[i].state = stop;
                    motor[i].count++;
                }
            }
        }
    }
}