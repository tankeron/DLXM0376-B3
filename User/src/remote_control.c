#include "remote_control.h"
#include "motor_hall.h"
#include "ble_module.h"
#include "key.h"
#include "sound_box.h"
#include "bsp_spi.h"
#include <string.h>
#define REMOTE_TX_BUF_LEN 20
#define REMOTE_RX_BUF_LEN 20
uint8_t remote_tx_buf[REMOTE_TX_BUF_LEN] = {0};
uint8_t remote_rx_buf[REMOTE_TX_BUF_LEN] = {0};
uint8_t remote_rx_count = 0;


void RwIoInit(void)
{
	GPIO_InitType GPIO_InitStructure;
	RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOD);
	/* Assign default value to GPIO_InitStructure structure */
	GPIO_Structure_Initialize(&GPIO_InitStructure);
	/* Select the GPIO pin to control */
	GPIO_InitStructure.Pin          = GPIO_PIN_2;
	/* Set pin mode to general push-pull output */
	GPIO_InitStructure.GPIO_Mode    = GPIO_MODE_OUT_PP;
	/* Set the pin drive current to 4MA*/
	GPIO_InitStructure.GPIO_Current = GPIO_DS_4MA;
	/* Initialize GPIO */
	GPIO_Peripheral_Initialize(GPIOB, &GPIO_InitStructure);
	GPIO_Pins_Reset(GPIOB,GPIO_PIN_2);
}

void UART3_init(void)
{
    GPIO_InitType GPIO_InitStructure;
    USART_InitType USART_InitStructure;
    NVIC_InitType NVIC_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOB);
    RCC_APB2_Peripheral_Clock_Enable(RCC_APB2_PERIPH_AFIO);
    RCC_APB2_Peripheral_Clock_Enable(RCC_APB2_PERIPH_UART3);

    NVIC_Priority_Group_Set(NVIC_PER4_SUB0_PRIORITYGROUP);

    NVIC_InitStructure.NVIC_IRQChannel = UART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Initializes(&NVIC_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF10_UART3;
    GPIO_Peripheral_Initialize(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.Pin = GPIO_PIN_11;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF10_UART3;
    GPIO_Peripheral_Initialize(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.BaudRate = 115200;
    USART_InitStructure.WordLength = USART_WL_8B;
    USART_InitStructure.StopBits = USART_STPB_1;
    USART_InitStructure.Parity = USART_PE_NO;
    USART_InitStructure.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_InitStructure.Mode = USART_MODE_RX | USART_MODE_TX;

    USART_Initializes(UART3, &USART_InitStructure);

    USART_Interrput_Enable(UART3, USART_INT_RXDNE);
    USART_Interrput_Enable(UART3, USART_INT_IDLEF);

    USART_Enable(UART3);
}


void UART3_DMA_Init(void)
{
    DMA_InitType DMA_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_DMA);

    /* USARTy_Tx_DMA_Channel (triggered by USARTy Tx event) Config */
    DMA_Reset(DMA_CH5);
    DMA_InitStructure.PeriphAddr = (UART3_BASE + 0x04);
    DMA_InitStructure.MemAddr = (uint32_t)remote_tx_buf;
    DMA_InitStructure.Direction = DMA_DIR_PERIPH_DST;
    DMA_InitStructure.BufSize = REMOTE_TX_BUF_LEN;
    DMA_InitStructure.PeriphInc = DMA_PERIPH_INC_MODE_DISABLE;
    DMA_InitStructure.MemoryInc = DMA_MEM_INC_MODE_ENABLE;
    DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_WIDTH_BYTE;
    DMA_InitStructure.MemDataSize = DMA_MEM_DATA_WIDTH_BYTE;
    DMA_InitStructure.CircularMode = DMA_CIRCULAR_MODE_DISABLE;
    DMA_InitStructure.Priority = DMA_CH_PRIORITY_HIGHEST;
    DMA_InitStructure.Mem2Mem = DMA_MEM2MEM_DISABLE;
    DMA_Initializes(DMA_CH5, &DMA_InitStructure);
    DMA_Channel_Request_Remap(DMA_CH5, DMA_REMAP_UART3_TX);
}

void UART3_dma_send(uint8_t *buf, uint8_t len)
{
    DMA_Memory_Address_Config(DMA_CH5, (uint32_t)buf);
    DMA_Buffer_Size_Config(DMA_CH5, len);
    USART_DMA_Transfer_Enable(UART3, USART_DMAREQ_TX);
    DMA_Channel_Enable(DMA_CH5);
}

uint8_t remote_check_sum(uint8_t *data, uint8_t size)
{
    int sum = 0;
    while (size--)
    {
        sum += *(unsigned char *)data++;
    }
    return sum;
}

void vDummyCallback(TimerHandle_t xTimer)
{
    // 什么都不做
}
uint8_t CMD;
uint8_t rgb_d = 0;
void Remote_Task(void *parameter)
{
    uint8_t i,j;
    CS_STATUS_Typedef cs_data;

    UART3_DMA_Init();
    UART3_init();
	RwIoInit();
    while (1)
    {
//        if (xQueueReceive(cs_status_Queue, &cs_data, 0) == pdTRUE)
//        {
//            j = 0;
//            remote_tx_buf[j++] = 0x55;
//            remote_tx_buf[j++] = 0xaa;
//			remote_tx_buf[j++] = cs_data.cs_cmd;
//			remote_tx_buf[j++] = 6;
//            remote_tx_buf[j++] = cs_data.cs_data;
//			remote_tx_buf[j]   = remote_check_sum(remote_tx_buf,j);
//            UART3_dma_send(remote_tx_buf,++j);
//        }
//		else
//		{
//			j = 0;
//            remote_tx_buf[j++] = 0x55;
//            remote_tx_buf[j++] = 0xaa;
//			remote_tx_buf[j++] = MACHINE_HEARTBEAT;
//			remote_tx_buf[j++] = 5;
//			remote_tx_buf[j]   = remote_check_sum(remote_tx_buf,j);
//            UART3_dma_send(remote_tx_buf,++j);
//		}
		vTaskDelay(500);
    }
}

void UART3_IRQHandler(void)
{
	uint16_t moto_cmd[MAX_MOTOR_NUM];
    INTStatus status;
    uint8_t rx_data;
    uint8_t check;
	uint8_t mac_temp[6];
    if (USART_Interrupt_Status_Get(UART3, USART_INT_RXDNE) == SET)
    {
		rx_data = USART_Data_Receive(UART3); // 接收数据字节
        if (remote_rx_count < REMOTE_RX_BUF_LEN)
        {
            remote_rx_buf[remote_rx_count++] = rx_data; // 接收数据字节
        }
        USART_Interrupt_Status_Clear(UART3, USART_INT_RXDNE);
    }
	
    if (USART_Interrupt_Status_Get(UART3, USART_INT_IDLEF) == SET)
    {
        status = USART_Interrupt_Status_Get(UART3, USART_INT_IDLEF);
        rx_data = USART_Data_Receive(UART3);
        if(remote_rx_count > 4)
        {
            if(remote_rx_buf[0] == 0x55 && remote_rx_buf[1] == 0xaa)
            {
                if(remote_rx_buf[remote_rx_count-1] == remote_check_sum(remote_rx_buf,remote_rx_count-1))
                {
					switch(remote_rx_buf[3])
					{
						case MACHINE_BLE_REST:
							CMD = CMD_AURACAST_ON_OFF;
							xQueueSendFromISR(Sound_Box_CMD_Queue, &CMD, 0);
						break;
						case MACHINE_BLE_DISCONNECT:
							CMD = CMD_BLE_DISABLE;
							xQueueSendFromISR(Sound_Box_CMD_Queue, &CMD, 0);
						break;
						case MACHINE_UP_LIGHT:
							rgb_d = RGB_COLOR_UP;
							xQueueSendFromISR(RGB_Cmd_Queue, &rgb_d, 0);
						break;
						case MACHINE_DOWN_LIGHT:
							rgb_d = RGB_COLOR_DOWN;
							xQueueSendFromISR(RGB_Cmd_Queue, &rgb_d, 0);
						break;
						case MACHINE_UP_SHAKE:
							CMD = CMD_VIBRATE_UP;
							xQueueSendFromISR(Sound_Box_CMD_Queue, &CMD, 0);
						break;
						case MACHINE_DOWN_SHAKE:
							CMD = CMD_VIBRATE_DOWN;
							xQueueSendFromISR(Sound_Box_CMD_Queue, &CMD, 0);
						break;
						case MACHINE_UP_VOL:
							CMD = CMD_VOLUME_UP;
							xQueueSendFromISR(Sound_Box_CMD_Queue, &CMD, 0);
						break;
						case MACHINE_DOWN_VOL:
							CMD = CMD_VOLUME_DOWN;
							xQueueSendFromISR(Sound_Box_CMD_Queue, &CMD, 0);
						break;
					}
                }
            }
        }
		remote_rx_count = 0;
    }
}