#include "fan_heat_massage.h"

#define MAX_TX_BUF_SIZE 20
#define MAX_RX_BUF_SIZE 20

uint8_t tx_buf1[MAX_TX_BUF_SIZE];

uint8_t rx_buf1[MAX_RX_BUF_SIZE];

uint8_t rx_count1 = 0;
FlagStatus Reply_flag1 = RESET;

QueueHandle_t Set_Fan_Heat_Massage_Queue1 = NULL;
QueueHandle_t Get_Fan_Heat_Massage_Queue1 = NULL;
QueueHandle_t cs_status_Queue = NULL;

Fan_Heat_Massage_Typedef sys_Fan_Heat_Massage_Data1;

#define MASSAGE_MOTOR_ID 2U

#if MAX_MOTOR_NUM < 3
#error "Massage motor requires motor[2]"
#endif

typedef enum
{
    MASSAGE_MOTOR_STATE_IDLE = 0,
    MASSAGE_MOTOR_STATE_EXTEND,
    MASSAGE_MOTOR_STATE_INTERVAL,
    MASSAGE_MOTOR_STATE_RETRACT,
    MASSAGE_MOTOR_STATE_CLOSE_WAIT,
    MASSAGE_MOTOR_STATE_CLOSE_RETRACT
} Massage_Motor_State_Typedef;

typedef struct
{
    Massage_Motor_State_Typedef state;
    uint8_t active_level;
    uint8_t requested_level;
    TickType_t phase_start_tick;
    TickType_t phase_duration_ticks;
    TickType_t close_retract_ticks;
} Massage_Motor_Control_Typedef;

TaskHandle_t Massage_Motor_Task_Handle = NULL;

static const uint32_t Massage_Motor_Extend_Time_MS[4] =
{
    0U,
    MASSAGE_MOTOR_LEVEL1_EXTEND_TIME_MS,
    MASSAGE_MOTOR_LEVEL2_EXTEND_TIME_MS,
    MASSAGE_MOTOR_LEVEL3_EXTEND_TIME_MS
};

static const uint32_t Massage_Motor_Interval_Time_MS[4] =
{
    0U,
    MASSAGE_MOTOR_LEVEL1_INTERVAL_TIME_MS,
    MASSAGE_MOTOR_LEVEL2_INTERVAL_TIME_MS,
    MASSAGE_MOTOR_LEVEL3_INTERVAL_TIME_MS
};

static const uint32_t Massage_Motor_Retract_Time_MS[4] =
{
    0U,
    MASSAGE_MOTOR_LEVEL1_RETRACT_TIME_MS,
    MASSAGE_MOTOR_LEVEL2_RETRACT_TIME_MS,
    MASSAGE_MOTOR_LEVEL3_RETRACT_TIME_MS
};

static TickType_t Massage_Motor_MS_To_Ticks(uint32_t time_ms)
{
    return pdMS_TO_TICKS(time_ms);
}

static void Massage_Motor_Start_Extend(Massage_Motor_Control_Typedef *control,
                                       uint8_t level,
                                       TickType_t now)
{
    control->state = MASSAGE_MOTOR_STATE_EXTEND;
    control->active_level = level;
    control->requested_level = level;
    control->phase_start_tick = now;
    control->phase_duration_ticks = Massage_Motor_MS_To_Ticks(Massage_Motor_Extend_Time_MS[level]);
    motor_push(MASSAGE_MOTOR_ID);
}

static void Massage_Motor_Start_Interval(Massage_Motor_Control_Typedef *control,
                                         TickType_t now)
{
    control->state = MASSAGE_MOTOR_STATE_INTERVAL;
    control->phase_start_tick = now;
    control->phase_duration_ticks = Massage_Motor_MS_To_Ticks(Massage_Motor_Interval_Time_MS[control->active_level]);
    motor_stop(MASSAGE_MOTOR_ID);
}

static void Massage_Motor_Start_Retract(Massage_Motor_Control_Typedef *control,
                                        TickType_t now,
                                        TickType_t retract_ticks,
                                        Massage_Motor_State_Typedef state)
{
    control->state = state;
    control->phase_start_tick = now;
    control->phase_duration_ticks = retract_ticks;
    motor_pull(MASSAGE_MOTOR_ID);
}

static void Massage_Motor_Stop(Massage_Motor_Control_Typedef *control)
{
    control->state = MASSAGE_MOTOR_STATE_IDLE;
    control->active_level = 0U;
    control->phase_duration_ticks = 0U;
    control->close_retract_ticks = 0U;
    motor_stop(MASSAGE_MOTOR_ID);
}

static void Massage_Motor_Handle_Command(Massage_Motor_Control_Typedef *control,
                                         uint8_t level,
                                         TickType_t now)
{
    TickType_t elapsed_ticks;

    if (control->state == MASSAGE_MOTOR_STATE_IDLE)
    {
        control->requested_level = level;
        if (level != 0U)
        {
            Massage_Motor_Start_Extend(control, level, now);
        }
        return;
    }

    if (control->state == MASSAGE_MOTOR_STATE_EXTEND)
    {
        if (level == 0U)
        {
            elapsed_ticks = now - control->phase_start_tick;
            if (elapsed_ticks > control->phase_duration_ticks)
            {
                elapsed_ticks = control->phase_duration_ticks;
            }

            control->requested_level = 0U;
            control->close_retract_ticks = elapsed_ticks +
                Massage_Motor_MS_To_Ticks(MASSAGE_MOTOR_RETRACT_MARGIN_TIME_MS);
            control->state = MASSAGE_MOTOR_STATE_CLOSE_WAIT;
            control->phase_start_tick = now;
            control->phase_duration_ticks = Massage_Motor_MS_To_Ticks(
                Massage_Motor_Interval_Time_MS[control->active_level]);
            motor_stop(MASSAGE_MOTOR_ID);
        }
        else if (level > control->active_level)
        {
            control->active_level = level;
            control->requested_level = level;
            control->phase_duration_ticks = Massage_Motor_MS_To_Ticks(
                Massage_Motor_Extend_Time_MS[level]);
        }
        else
        {
            control->requested_level = level;
        }
        return;
    }

    control->requested_level = level;
}

static void Massage_Motor_Update(Massage_Motor_Control_Typedef *control,
                                 TickType_t now)
{
    if ((control->state == MASSAGE_MOTOR_STATE_IDLE) ||
        ((now - control->phase_start_tick) < control->phase_duration_ticks))
    {
        return;
    }

    switch (control->state)
    {
        case MASSAGE_MOTOR_STATE_EXTEND:
            Massage_Motor_Start_Interval(control, now);
            break;

        case MASSAGE_MOTOR_STATE_INTERVAL:
            Massage_Motor_Start_Retract(
                control,
                now,
                Massage_Motor_MS_To_Ticks(Massage_Motor_Retract_Time_MS[control->active_level]),
                MASSAGE_MOTOR_STATE_RETRACT);
            break;

        case MASSAGE_MOTOR_STATE_RETRACT:
            motor_stop(MASSAGE_MOTOR_ID);
            if (control->requested_level != 0U)
            {
                Massage_Motor_Start_Extend(control, control->requested_level, now);
            }
            else
            {
                Massage_Motor_Stop(control);
            }
            break;

        case MASSAGE_MOTOR_STATE_CLOSE_WAIT:
            Massage_Motor_Start_Retract(control,
                                         now,
                                         control->close_retract_ticks,
                                         MASSAGE_MOTOR_STATE_CLOSE_RETRACT);
            break;

        case MASSAGE_MOTOR_STATE_CLOSE_RETRACT:
            motor_stop(MASSAGE_MOTOR_ID);
            if (control->requested_level != 0U)
            {
                Massage_Motor_Start_Extend(control, control->requested_level, now);
            }
            else
            {
                Massage_Motor_Stop(control);
            }
            break;

        default:
            Massage_Motor_Stop(control);
            break;
    }
}

void Massage_Motor_Set_Level(uint8_t level)
{
    if ((level <= 3U) && (Massage_Motor_Task_Handle != NULL))
    {
        xTaskNotify(Massage_Motor_Task_Handle, (uint32_t)level, eSetValueWithOverwrite);
    }
}

void Massage_Motor_Task(void *parameter)
{
    Massage_Motor_Control_Typedef control =
    {
        MASSAGE_MOTOR_STATE_IDLE,
        0U,
        0U,
        0U,
        0U,
        0U
    };
    uint32_t notified_level;

    (void)parameter;

    while (1)
    {
        if (xTaskNotifyWait(0U,
                            0xFFFFFFFFUL,
                            &notified_level,
                            Massage_Motor_MS_To_Ticks(MASSAGE_MOTOR_TASK_PERIOD_MS)) == pdTRUE)
        {
            if (notified_level <= 3U)
            {
                Massage_Motor_Handle_Command(&control,
                                              (uint8_t)notified_level,
                                              xTaskGetTickCount());
            }
        }

        Massage_Motor_Update(&control, xTaskGetTickCount());
    }
}

void USART2_init(void)
{
    GPIO_InitType GPIO_InitStructure;
    USART_InitType USART_InitStructure;
    NVIC_InitType NVIC_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOA);
    RCC_APB2_Peripheral_Clock_Enable(RCC_APB2_PERIPH_AFIO);
    RCC_APB1_Peripheral_Clock_Enable(RCC_APB1_PERIPH_USART2);

    NVIC_Priority_Group_Set(NVIC_PER4_SUB0_PRIORITYGROUP);

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Initializes(&NVIC_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF5_USART2;
    GPIO_Peripheral_Initialize(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_3;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF5_USART2;
    GPIO_Peripheral_Initialize(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.BaudRate = 9600;
    USART_InitStructure.WordLength = USART_WL_8B;
    USART_InitStructure.StopBits = USART_STPB_1;
    USART_InitStructure.Parity = USART_PE_NO;
    USART_InitStructure.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_InitStructure.Mode = USART_MODE_RX | USART_MODE_TX;

    USART_Initializes(USART2, &USART_InitStructure);

    USART_Interrput_Enable(USART2, USART_INT_RXDNE);
    USART_Interrput_Enable(USART2, USART_INT_IDLEF);

    USART_Enable(USART2);
}


void USART2_DMA_Init(void)
{
    DMA_InitType DMA_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_DMA);

    /* USARTy_Tx_DMA_Channel (triggered by USARTy Tx event) Config */
    DMA_Reset(DMA_CH3);
    DMA_InitStructure.PeriphAddr = (USART2_BASE + 0x04);
    DMA_InitStructure.MemAddr = (uint32_t)tx_buf1;
    DMA_InitStructure.Direction = DMA_DIR_PERIPH_DST;
    DMA_InitStructure.BufSize = MAX_TX_BUF_SIZE;
    DMA_InitStructure.PeriphInc = DMA_PERIPH_INC_MODE_DISABLE;
    DMA_InitStructure.MemoryInc = DMA_MEM_INC_MODE_ENABLE;
    DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_WIDTH_BYTE;
    DMA_InitStructure.MemDataSize = DMA_MEM_DATA_WIDTH_BYTE;
    DMA_InitStructure.CircularMode = DMA_CIRCULAR_MODE_DISABLE;
    DMA_InitStructure.Priority = DMA_CH_PRIORITY_HIGHEST;
    DMA_InitStructure.Mem2Mem = DMA_MEM2MEM_DISABLE;
    DMA_Initializes(DMA_CH3, &DMA_InitStructure);
    DMA_Channel_Request_Remap(DMA_CH3, DMA_REMAP_USART2_TX);
}

void Usart2_dma_send(uint8_t *buf, uint8_t len)
{
    DMA_Memory_Address_Config(DMA_CH3, (uint32_t)buf);
    DMA_Buffer_Size_Config(DMA_CH3, len);
    USART_DMA_Transfer_Enable(USART2, USART_DMAREQ_TX);
    DMA_Channel_Enable(DMA_CH3);
	Reply_flag1 = SET;
}


unsigned char checksum8(uint8_t *data, uint8_t size)
{
    int sum = 0;
    while (size--)
    {
        sum += *(unsigned char *)data++;
    }
    return sum;
}

void fan_heat_massage_select_tx_Task(void *parameter)
{
    Fan_Heat_Massage_Typedef Set_Fan_Heat_Massage_Data;
    Set_Fan_Heat_Massage_Data.msg_select = 0;
    while (1)
    {
        /* code */
        xQueueSend(Set_Fan_Heat_Massage_Queue1, &Set_Fan_Heat_Massage_Data, 0);
        vTaskDelay(500);
    }
}

void fan_heat_massage1_tx_Task(void *parameter)
{
    BaseType_t xReturn = pdTRUE;
    Fan_Heat_Massage_Typedef Set_Fan_Heat_Massage_Data;
    Fan_Heat_Massage_Typedef Get_Fan_Heat_Massage_Data;
    Fan_Heat_Massage_Typedef Led_Fan_Heat_Massage_Data1;
    uint8_t tx_buf[20] = {0};
    uint8_t send_id = 1;
    uint8_t i,j;
    USART2_DMA_Init();
    USART2_init();
    while (1)
    {		
        if (xQueueReceive(Set_Fan_Heat_Massage_Queue1, &Set_Fan_Heat_Massage_Data, portMAX_DELAY) == pdTRUE)
        {
            i = 0;
            switch (Set_Fan_Heat_Massage_Data.msg_select)
            {
            case 0x00:
                tx_buf[i++] = 0xd1;
                tx_buf[i++] = 0xd1;
                tx_buf[i++] = 0x02;
                tx_buf[i++] = send_id;
                tx_buf[i++] = 0x02;
				j 			= checksum8(tx_buf, i);
                tx_buf[i++] = j;
                Usart2_dma_send(tx_buf, i);
                break;
            case 0x01://顶腰
                /* code */
                tx_buf[i++] = 0xd1;   //帧头
                tx_buf[i++] = 0xd1;   //帧头
                tx_buf[i++] = 0x06;   //长度
                tx_buf[i++] = send_id;  //id
                tx_buf[i++] = 0x01;     // 0x01:控制   0x02:查询
                tx_buf[i++] = Set_Fan_Heat_Massage_Data.lumbar_support;   //这个字节暂时没有使用，
                tx_buf[i++] = Set_Fan_Heat_Massage_Data.Massage_level;  //按摩  0x04~0x06  顶腰：0x01打气 0x02放气 
                tx_buf[i++] = sys_Fan_Heat_Massage_Data1.fan_level;      //通风  
                tx_buf[i++] = sys_Fan_Heat_Massage_Data1.hot_level;      //加热
                j 			= checksum8(tx_buf, i);
                tx_buf[i++] = j;         //校验
                Usart2_dma_send(tx_buf, i);         //发送
                break;
            case 0x02://按摩控制
                /* code */
				Massage_Motor_Set_Level(Set_Fan_Heat_Massage_Data.Massage_level);
                tx_buf[i++] = 0xd1;
                tx_buf[i++] = 0xd1;
                tx_buf[i++] = 0x06;
                tx_buf[i++] = send_id;
                tx_buf[i++] = 0x01;
                tx_buf[i++] = 0;
                tx_buf[i++] = (Set_Fan_Heat_Massage_Data.Massage_level != 0)? Set_Fan_Heat_Massage_Data.Massage_level+3 : 3;
                tx_buf[i++] = sys_Fan_Heat_Massage_Data1.fan_level;
                tx_buf[i++] = sys_Fan_Heat_Massage_Data1.hot_level;
                j 			= checksum8(tx_buf, i);
                tx_buf[i++] = j;
                Usart2_dma_send(tx_buf, i);
                break;
            case 0x03:
                /* code */
                tx_buf[i++] = 0xd1;
                tx_buf[i++] = 0xd1;
                tx_buf[i++] = 0x06;
                tx_buf[i++] = send_id;
                tx_buf[i++] = 0x01;
                tx_buf[i++] = 0;
				tx_buf[i++] = (sys_Fan_Heat_Massage_Data1.Massage_level != 0)? sys_Fan_Heat_Massage_Data1.Massage_level+3 : 3;
                tx_buf[i++] = Set_Fan_Heat_Massage_Data.fan_level;
                tx_buf[i++] = 0;//sys_Fan_Heat_Massage_Data1.hot_level;
                j 			= checksum8(tx_buf, i);
                tx_buf[i++] = j;
                Usart2_dma_send(tx_buf, i);
                break;
            case 0x04:
                /* code */
                tx_buf[i++] = 0xd1;
                tx_buf[i++] = 0xd1;
                tx_buf[i++] = 0x06;
                tx_buf[i++] = send_id;
                tx_buf[i++] = 0x01;
                tx_buf[i++] = 0;
                tx_buf[i++] = (sys_Fan_Heat_Massage_Data1.Massage_level != 0)? sys_Fan_Heat_Massage_Data1.Massage_level+3 : 3;
                tx_buf[i++] = 0;//sys_Fan_Heat_Massage_Data1.fan_level;
                tx_buf[i++] = Set_Fan_Heat_Massage_Data.hot_level;
                j 			= checksum8(tx_buf, i);
                tx_buf[i++] = j;
                Usart2_dma_send(tx_buf, i);
                break;

            default:
                break;
            }
            send_id++;
			
			xReturn = xQueueReceive(Get_Fan_Heat_Massage_Queue1, &Get_Fan_Heat_Massage_Data, 100);
			if (pdTRUE == xReturn)
			{
				Led_Fan_Heat_Massage_Data1.Massage_level = (Get_Fan_Heat_Massage_Data.Massage_level>3)?Get_Fan_Heat_Massage_Data.Massage_level-3:0;
				Led_Fan_Heat_Massage_Data1.fan_level = Get_Fan_Heat_Massage_Data.fan_level;
				Led_Fan_Heat_Massage_Data1.hot_level = Get_Fan_Heat_Massage_Data.hot_level;
				if((Led_Fan_Heat_Massage_Data1.Massage_level != sys_Fan_Heat_Massage_Data1.Massage_level)||
				   (Led_Fan_Heat_Massage_Data1.fan_level != sys_Fan_Heat_Massage_Data1.fan_level)||
				   (Led_Fan_Heat_Massage_Data1.hot_level != sys_Fan_Heat_Massage_Data1.hot_level))
				{
					sys_Fan_Heat_Massage_Data1.Massage_level = Led_Fan_Heat_Massage_Data1.Massage_level;
					sys_Fan_Heat_Massage_Data1.fan_level = Led_Fan_Heat_Massage_Data1.fan_level;
					sys_Fan_Heat_Massage_Data1.hot_level = Led_Fan_Heat_Massage_Data1.hot_level;
					xQueueSend(cs_status_Queue, &sys_Fan_Heat_Massage_Data1, 0);
					Reply_flag1 = RESET;
				}
			}
			else
			{
				Usart2_dma_send(tx_buf, i);
				xReturn = xQueueReceive(Get_Fan_Heat_Massage_Queue1, &Get_Fan_Heat_Massage_Data, 100);
				if (pdTRUE == xReturn)
				{
					Led_Fan_Heat_Massage_Data1.Massage_level = (Get_Fan_Heat_Massage_Data.Massage_level>3)?Get_Fan_Heat_Massage_Data.Massage_level-3:0;
					Led_Fan_Heat_Massage_Data1.fan_level = Get_Fan_Heat_Massage_Data.fan_level;
					Led_Fan_Heat_Massage_Data1.hot_level = Get_Fan_Heat_Massage_Data.hot_level;
					if((Led_Fan_Heat_Massage_Data1.Massage_level != sys_Fan_Heat_Massage_Data1.Massage_level)||
					   (Led_Fan_Heat_Massage_Data1.fan_level != sys_Fan_Heat_Massage_Data1.fan_level)||
					   (Led_Fan_Heat_Massage_Data1.hot_level != sys_Fan_Heat_Massage_Data1.hot_level))
					{
						sys_Fan_Heat_Massage_Data1.Massage_level = Led_Fan_Heat_Massage_Data1.Massage_level;
						sys_Fan_Heat_Massage_Data1.fan_level = Led_Fan_Heat_Massage_Data1.fan_level;
						sys_Fan_Heat_Massage_Data1.hot_level = Led_Fan_Heat_Massage_Data1.hot_level;
						xQueueSend(cs_status_Queue, &sys_Fan_Heat_Massage_Data1, 0);
					}
				}
				Reply_flag1 = RESET;
			}
        }
        
    }
}


void USART2_IRQHandler(void)
{
    INTStatus status;
    uint8_t rx_data;
    uint8_t check;
    Fan_Heat_Massage_Typedef Fan_Heat_Massage_Data;
    
    if (USART_Interrupt_Status_Get(USART2, USART_INT_RXDNE) == SET)
    {
        if (rx_count1 < MAX_RX_BUF_SIZE)
        {
            rx_buf1[rx_count1++] = USART_Data_Receive(USART2); // 接收数据字节
        }
        USART_Interrupt_Status_Clear(USART2, USART_INT_RXDNE);
    }

    if (USART_Interrupt_Status_Get(USART2, USART_INT_IDLEF) == SET)
    {
        status = USART_Interrupt_Status_Get(USART2, USART_INT_IDLEF);
        rx_data = USART_Data_Receive(USART2);
        if (rx_buf1[0] == 0xd0 && rx_buf1[1] == 0xd0 && Reply_flag1 == SET)
        {
            if (rx_buf1[rx_count1 - 1] == checksum8(rx_buf1, rx_count1 - 1))
            {
                Fan_Heat_Massage_Data.Massage_level = rx_buf1[6];
                Fan_Heat_Massage_Data.fan_level = rx_buf1[7];
                Fan_Heat_Massage_Data.hot_level = rx_buf1[8];
                xQueueSendFromISR( Get_Fan_Heat_Massage_Queue1, &Fan_Heat_Massage_Data, NULL);
            }
        }
		rx_count1 = 0;
    }
}
