#include "sound_box.h"

#define SOUND_TX_BUF_LEN 10
#define SOUND_RX_BUF_LEN 10
uint8_t Sound_tx_buf[SOUND_TX_BUF_LEN] = {0};
uint8_t Sound_rx_buf[SOUND_RX_BUF_LEN] = {0};
uint8_t sound_rx_count = 0;
uint8_t sound_tx_id = 0;
QueueHandle_t Sound_Box_CMD_Queue = NULL;
QueueHandle_t Sound_Box_Reply_Queue = NULL;
TimerHandle_t xTimer_UsartSoundTimeout = NULL;
uint8_t EQ_mode = 0x01;
uint8_t sound_high_pitch = 50;
uint8_t sound_low_pitch = 50;
uint8_t vibrate_level = 0;
uint8_t sound_state = 0x01;
uint8_t sound_EQ = 0;

void USART1_init(void)
{
    GPIO_InitType GPIO_InitStructure;
    USART_InitType USART_InitStructure;
    NVIC_InitType NVIC_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOB);
    RCC_APB2_Peripheral_Clock_Enable(RCC_APB2_PERIPH_AFIO);
    RCC_APB2_Peripheral_Clock_Enable(RCC_APB2_PERIPH_USART1);

    NVIC_Priority_Group_Set(NVIC_PER4_SUB0_PRIORITYGROUP);

    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Initializes(&NVIC_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF1_USART1;
    GPIO_Peripheral_Initialize(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_7;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF1_USART1;
    GPIO_Peripheral_Initialize(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.BaudRate = 115200;
    USART_InitStructure.WordLength = USART_WL_8B;
    USART_InitStructure.StopBits = USART_STPB_1;
    USART_InitStructure.Parity = USART_PE_NO;
    USART_InitStructure.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_InitStructure.Mode = USART_MODE_RX | USART_MODE_TX;

    USART_Initializes(USART1, &USART_InitStructure);

    USART_Interrput_Enable(USART1, USART_INT_RXDNE);
    //USART_Interrput_Enable(USART2, USART_INT_IDLEF);

    USART_Enable(USART1);
	
	RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOD);
	/* Assign default value to GPIO_InitStructure structure */
	GPIO_Structure_Initialize(&GPIO_InitStructure);
	
	/* Select the GPIO pin to control */
	GPIO_InitStructure.Pin          = GPIO_PIN_13;
	/* Set pin mode to general push-pull output */
	GPIO_InitStructure.GPIO_Mode    = GPIO_MODE_OUT_PP;
	/* Set the pin drive current to 4MA*/
	GPIO_InitStructure.GPIO_Current = GPIO_DS_4MA;
	/* Initialize GPIO */
	GPIO_Peripheral_Initialize(GPIOD, &GPIO_InitStructure);
	GPIO_Pins_Set(GPIOD,GPIO_PIN_13);
}


void USART1_DMA_Init(void)
{
    DMA_InitType DMA_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_DMA);

    /* USARTy_Tx_DMA_Channel (triggered by USARTy Tx event) Config */
    DMA_Reset(DMA_CH6);
    DMA_InitStructure.PeriphAddr = (USART1_BASE + 0x04);
    DMA_InitStructure.MemAddr = (uint32_t)Sound_tx_buf;
    DMA_InitStructure.Direction = DMA_DIR_PERIPH_DST;
    DMA_InitStructure.BufSize = SOUND_TX_BUF_LEN;
    DMA_InitStructure.PeriphInc = DMA_PERIPH_INC_MODE_DISABLE;
    DMA_InitStructure.MemoryInc = DMA_MEM_INC_MODE_ENABLE;
    DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_WIDTH_BYTE;
    DMA_InitStructure.MemDataSize = DMA_MEM_DATA_WIDTH_BYTE;
    DMA_InitStructure.CircularMode = DMA_CIRCULAR_MODE_DISABLE;
    DMA_InitStructure.Priority = DMA_CH_PRIORITY_HIGHEST;
    DMA_InitStructure.Mem2Mem = DMA_MEM2MEM_DISABLE;
    DMA_Initializes(DMA_CH6, &DMA_InitStructure);
    DMA_Channel_Request_Remap(DMA_CH6, DMA_REMAP_USART1_TX);
}

void USART1_dma_send(uint8_t *buf, uint8_t len)
{
    DMA_Memory_Address_Config(DMA_CH6, (uint32_t)buf);
    DMA_Buffer_Size_Config(DMA_CH6, len);
    USART_DMA_Transfer_Enable(USART1, USART_DMAREQ_TX);
    DMA_Channel_Enable(DMA_CH6);
}

uint8_t sound_check_sum(uint8_t *data, uint8_t size)
{
    int sum = 0;
    while (size--)
    {
        sum += *(unsigned char *)data++;
    }
    return sum;
}

void Sound_Box_Config_Task(void *parameter)
{
    vTaskDelay(10000);
    Sound_CMD_Typedef Sound_CMD;
    Sound_CMD.msg_fun = 0x05;
    Sound_CMD.msg_data = 0;
    xQueueSend(Sound_Box_CMD_Queue, &Sound_CMD, 0);

    vTaskDelete(NULL);
}

FlagStatus wait_reply_flag = RESET;
uint8_t wait_reply_cmd = 0x00;
FlagStatus sound_flag = RESET;
void Sound_Box_Task(void *parameter)
{
    uint8_t i,j,CMD;
    Sound_CMD_Typedef Sound_CMD;
    Sound_CMD_Typedef Sound_Reply;
	
	FunctionalState auracast_Flag = DISABLE;
    
    USART1_DMA_Init();
    USART1_init();
    while (1)
    {
        if (xQueueReceive(Sound_Box_CMD_Queue, &CMD, portMAX_DELAY) == pdTRUE)
        {
            if(sound_flag == RESET)
            {
                if(CMD == CMD_TREBLE_UP)
                {
                    if(sound_high_pitch < 100) sound_high_pitch += 5;
                    Sound_CMD.msg_fun = 0x02;
                    Sound_CMD.msg_data = sound_high_pitch;
                }

                if(CMD == CMD_TREBLE_DOWN)
                {
                    if(sound_high_pitch > 0) sound_high_pitch -= 5;
                    Sound_CMD.msg_fun = 0x02;
                    Sound_CMD.msg_data = sound_high_pitch;
                }

                if(CMD == CMD_BASS_UP)
                {
                    if(sound_low_pitch < 100) sound_low_pitch += 5;
                    Sound_CMD.msg_fun = 0x03;
                    Sound_CMD.msg_data = sound_low_pitch;
                }

                if(CMD == CMD_BASS_DOWN)
                {
                    if(sound_low_pitch > 0) sound_low_pitch -= 5;
                    Sound_CMD.msg_fun = 0x03;
                    Sound_CMD.msg_data = sound_low_pitch;
                }

                if(CMD == CMD_VOLUME_UP)
                {
                    Sound_CMD.msg_fun = 0x09; 
                    Sound_CMD.msg_data = 0x01;
                }

                if(CMD == CMD_VOLUME_DOWN)
                {
                    Sound_CMD.msg_fun = 0x0a; 
                    Sound_CMD.msg_data = 0x01;
                }

                if(CMD == CMD_VIBRATE_UP)
                {
                    if(vibrate_level < 75) 
					{
						vibrate_level += 25;
					}
					else
					{
						vibrate_level = 100;
					}
                    Sound_CMD.msg_fun = 0x05;
                    Sound_CMD.msg_data = vibrate_level;
                }

                if(CMD == CMD_VIBRATE_DOWN)
                {
                    if(vibrate_level > 25) 
					{
						vibrate_level -= 25;
					}
					else
					{
						vibrate_level = 0;
					}
                    Sound_CMD.msg_fun = 0x05;
                    Sound_CMD.msg_data = vibrate_level;
                }

                if(CMD == CMD_BLE_DISABLE)
                {
                    Sound_CMD.msg_fun = 0x0B;
                    Sound_CMD.msg_data = 0X01;
                }
				
				if(CMD == CMD_PLAY_STOP)
                {
                    Sound_CMD.msg_fun = 0x06;
                    Sound_CMD.msg_data = 0X01;
                }

                if(CMD == CMD_AURACAST_ON_OFF && auracast_Flag == DISABLE)
                {
                    Sound_CMD.msg_fun = 0x0D;
                    Sound_CMD.msg_data = 0X01;
                }

                if(CMD == CMD_AURACAST_ON_OFF && auracast_Flag == ENABLE)
                {
                    Sound_CMD.msg_fun = 0x0E;
                    Sound_CMD.msg_data = 0X01;
                }

                vTaskDelay(160);
                j = 0;
                Sound_tx_buf[j++] = 0x55;
                Sound_tx_buf[j++] = 0xaa;
                if(sound_tx_id < 200) sound_tx_id++;
                else sound_tx_id = 0;
                Sound_tx_buf[j++] = sound_tx_id;
                Sound_tx_buf[j++] = Sound_CMD.msg_fun;
                Sound_tx_buf[j++] = Sound_CMD.msg_data;
                Sound_tx_buf[j] = sound_check_sum(Sound_tx_buf,j);
                USART1_dma_send(Sound_tx_buf,++j);
                wait_reply_flag = SET;
                if(Sound_CMD.msg_fun == 0x09 || Sound_CMD.msg_fun == 0x0a)
                {
                    wait_reply_cmd = 0xff;
                }
                else
                {
                    wait_reply_cmd = sound_tx_id;
                }
                for ( i = 0; i < 3; i++)
                {
                    /* code */
                    if (xQueueReceive(Sound_Box_Reply_Queue, &Sound_Reply, 300) == pdTRUE)
                    {
                        if(Sound_Reply.msg_fun == 0 && Sound_Reply.msg_data == sound_tx_id)
                        {
                            if(Sound_Reply.msg_data == sound_tx_id)
                            {
                                switch (Sound_CMD.msg_fun)
                                {
                                case 0x01:
                                    /* code */
                                    EQ_mode = Sound_CMD.msg_data;
                                    break;
                                case 0x02:
                                    sound_high_pitch = Sound_CMD.msg_data;
                                    break;
                                case 0x03:
                                    sound_low_pitch = Sound_CMD.msg_data;
                                    break;
                                case 0x05:
                                    vibrate_level = Sound_CMD.msg_data;
                                    break;
                                case 0x0d:
                                case 0x0e:
                                    if(auracast_Flag == DISABLE)
                                    {
                                        auracast_Flag = ENABLE;
                                    }
                                    else
                                    {
                                        auracast_Flag = DISABLE;
                                    }
                                    break;
                                default:
                                    break;
                                }
                            }
                        }
                        wait_reply_flag = RESET;
                        break;
                    }
                    else
                    {
                        USART1_dma_send(Sound_tx_buf,j);
						wait_reply_flag = SET;
                    }
                }
            }
            
        }
    }
}

void USART1_IRQHandler(void)
{
    INTStatus status;
    uint8_t rx_data;
    if (USART_Interrupt_Status_Get(USART1, USART_INT_RXDNE) == SET)
    {
        if (sound_rx_count < SOUND_RX_BUF_LEN)
        {
            Sound_rx_buf[sound_rx_count++] = USART_Data_Receive(USART1); // 接收数据字节
        }
        xTimerResetFromISR(xTimer_UsartSoundTimeout, 0);
        USART_Interrupt_Status_Clear(USART1, USART_INT_RXDNE);
    }
}
FlagStatus power_on_flag = RESET;
void UsartSoundTimeoutCallback(TimerHandle_t xTimer)
{
    uint8_t check;
    Sound_CMD_Typedef Sound_Reply;
    if(sound_rx_count > 4)
    {
        if(Sound_rx_buf[0] == 0x55 && Sound_rx_buf[1] == 0xaa)
        {
            if(Sound_rx_buf[sound_rx_count-1] == sound_check_sum(Sound_rx_buf,sound_rx_count-1))
            {
                if(wait_reply_flag == SET && Sound_rx_buf[3] == wait_reply_cmd)
                {
                    Sound_Reply.msg_fun = 0;
                    Sound_Reply.msg_data = Sound_rx_buf[3];
                    xQueueSend( Sound_Box_Reply_Queue, &Sound_Reply, NULL);
                }

                if(Sound_rx_buf[3] == 0xff && Sound_rx_buf[4] == 0x02 && power_on_flag == RESET)
                {
                    power_on_flag = SET;
                    Sound_CMD_Typedef Sound_CMD;
                    Sound_CMD.msg_fun = 0x05;
                    Sound_CMD.msg_data = 0;
                    xQueueSend(Sound_Box_CMD_Queue, &Sound_CMD, 0);
                }
            }
        }
    }
    sound_rx_count = 0;
}