#include "bsp_spi.h"
//#include "display_control.h"

#define SPI_DMA_TX_BUF_LEN 9

uint8_t spi_tx_buf[SPI_DMA_TX_BUF_LEN] = {RGB_OFF_MODE,0xff,0x00,0x00,0x00,0x00};

TaskHandle_t rgb_spi_Handle = NULL;
TimerHandle_t xTimer_RGBTimeout = NULL;
QueueHandle_t RGB_SPI_Send_Queue = NULL;
QueueHandle_t RGB_Cmd_Queue;
QueueHandle_t lvdong_Queue;

void spi_gpio_init(void)
{
    GPIO_InitType GPIO_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(SPI_MASTER_PERIPH_GPIO);

    GPIO_Structure_Initialize(&GPIO_InitStructure);
    GPIO_InitStructure.Pin            = SPI_MASTER_MOSI_PIN | SPI_MASTER_CLK_PIN;
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Slew_Rate = GPIO_SLEW_RATE_FAST;
    GPIO_InitStructure.GPIO_Alternate = SPI_MASTER_GPIO_ALTERNATE;
    GPIO_Peripheral_Initialize(SPI_MASTER_GPIO, &GPIO_InitStructure);

    GPIO_InitStructure.Pin            = SPI_MASTER_CLK_PIN;
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Slew_Rate = GPIO_SLEW_RATE_FAST;
    GPIO_InitStructure.GPIO_Pull      = GPIO_PULL_DOWN;
    GPIO_InitStructure.GPIO_Alternate = SPI_MASTER_GPIO_ALTERNATE;
    GPIO_Peripheral_Initialize(SPI_MASTER_GPIO, &GPIO_InitStructure);

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOB);

    /* Assign default value to GPIO_InitStructure structure */
    GPIO_Structure_Initialize(&GPIO_InitStructure);
    
    /* Select the GPIO pin to control */
    GPIO_InitStructure.Pin          = SPI_SALVE1_NSS_PIN ;
    /* Set pin mode to general push-pull output */
    GPIO_InitStructure.GPIO_Mode    = GPIO_MODE_OUT_PP;
    /* Set the pin drive current to 4MA*/
    GPIO_InitStructure.GPIO_Current = GPIO_DS_12MA;
    /* Initialize GPIO */
    GPIO_Peripheral_Initialize(SPI_SALVE_NSS_GPIO, &GPIO_InitStructure);

    GPIO_Pins_Reset(SPI_SALVE_NSS_GPIO,SPI_SALVE1_NSS_PIN);
//    GPIO_Pins_Reset(SPI_SALVE_NSS_GPIO,SPI_SALVE2_NSS_PIN);
//    GPIO_Pins_Reset(SPI_SALVE_NSS_GPIO,SPI_SALVE3_NSS_PIN);
//    GPIO_Pins_Reset(SPI_SALVE_NSS_GPIO,SPI_SALVE4_NSS_PIN);
//    GPIO_Pins_Reset(SPI_SALVE_NSS_GPIO,SPI_SALVE5_NSS_PIN);
}


//void gpio_out_init(GPIO_Module* GPIOx, uint16_t pin)
//{
//    GPIO_InitType GPIO_InitStructure;

//    GPIO_Structure_Initialize(&GPIO_InitStructure);
//    
//    /* Select the GPIO pin to control */
//    GPIO_InitStructure.Pin          = pin;
//    /* Set pin mode to general push-pull output */
//    GPIO_InitStructure.GPIO_Mode    = GPIO_MODE_OUT_PP;
//    /* Set the pin drive current to 4MA*/
//    GPIO_InitStructure.GPIO_Current = GPIO_DS_12MA;
//    /* Initialize GPIO */
//    GPIO_Peripheral_Initialize(GPIOx, &GPIO_InitStructure);
//}



void dma_spi_init(void)
{
    /* Configure RGB DMA */
    DMA_InitType DMA_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_DMA);

    DMA_Reset(DMA_CH1);

    /* SPI_MASTER TX DMA config */
    
    DMA_InitStructure.MemDataSize = DMA_MEM_DATA_WIDTH_BYTE;
    DMA_InitStructure.MemAddr = (uint32_t)&spi_tx_buf[0];
    DMA_InitStructure.MemoryInc = DMA_MEM_INC_MODE_ENABLE;
    DMA_InitStructure.Direction = DMA_DIR_PERIPH_DST;
    DMA_InitStructure.PeriphAddr = (uint32_t)&SPI1->DAT;
    DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_WIDTH_BYTE;
    DMA_InitStructure.PeriphInc = DMA_PERIPH_INC_MODE_DISABLE;
    DMA_InitStructure.BufSize = SPI_DMA_TX_BUF_LEN;
    DMA_InitStructure.CircularMode = DMA_CIRCULAR_MODE_DISABLE;
    DMA_InitStructure.Mem2Mem = DMA_MEM2MEM_DISABLE;
    DMA_InitStructure.Priority = DMA_CH_PRIORITY_MEDIUM;
    DMA_Initializes(DMA_CH1, &DMA_InitStructure);
    DMA_Channel_Request_Remap(DMA_CH1, DMA_REMAP_SPI1_TX);

    DMA_Channel_Enable(DMA_CH1);
}

void bsp_spi_init(void)
{
    SPI_InitType SPI_InitStructure;

    RCC_APB2_Peripheral_Clock_Enable(SPI_MASTER_PERIPH);
    spi_gpio_init();

    dma_spi_init();

    SPI_Initializes_Structure(&SPI_InitStructure);
    SPI_InitStructure.DataDirection = SPI_DIR_DOUBLELINE_FULLDUPLEX;
    SPI_InitStructure.SpiMode       = SPI_MODE_MASTER;
    SPI_InitStructure.DataLen       = SPI_DATA_SIZE_8BITS;
    SPI_InitStructure.CLKPOL        = SPI_CLKPOL_LOW;
    SPI_InitStructure.CLKPHA        = SPI_CLKPHA_SECOND_EDGE;
    SPI_InitStructure.NSS           = SPI_NSS_SOFT;
    /* It is recommended that the SPI master mode of the C version chips should not exceed 18MHz */
    SPI_InitStructure.BaudRatePres  = SPI_BR_PRESCALER_64;
    SPI_InitStructure.FirstBit      = SPI_FB_MSB;
    SPI_InitStructure.CRCPoly       = 7;
    SPI_Initializes(SPI_MASTER, &SPI_InitStructure);

    SPI_Set_Nss_Level(SPI_MASTER, SPI_NSS_HIGH);
    
    SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);

    SPI_ON(SPI_MASTER);
}

uint8_t checksum_8(uint8_t *buf, uint32_t len)
{
    uint8_t sum = 0;
    while(len--)
    {
        sum += *buf++;
    }
    return sum;
}

void TIM4_base_init(void)
{
    TIM_TimeBaseInitType TIM_TimeBaseStructure;

    /* Enable TIM2 clock */
    RCC_APB1_Peripheral_Clock_Enable(RCC_APB1_PERIPH_TIM4);

    TIM_Base_Struct_Initialize(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.Period    = 16000;
    TIM_TimeBaseStructure.Prescaler = 0;
    TIM_TimeBaseStructure.ClkDiv    = TIM_CLK_DIV2;
    TIM_TimeBaseStructure.CntMode   = TIM_CNT_MODE_UP;
    
    TIM_Base_Initialize(TIM4, &TIM_TimeBaseStructure);

    TIM_Base_Reload_Mode_Set(TIM4, TIM_PSC_RELOAD_MODE_IMMEDIATE);

    TIM_On(TIM4);
}

void TIM4_pwm_init(void)
{
    OCInitType TIM_OCInitStructure;

    RCC_APB1_Peripheral_Clock_Enable(RCC_APB1_PERIPH_TIM4);

    TIM_Output_Channel_Struct_Initialize(&TIM_OCInitStructure);

    TIM_OCInitStructure.OcMode       = TIM_OCMODE_PWM2;
    TIM_OCInitStructure.OutputState  = TIM_OUTPUT_STATE_ENABLE;
    TIM_OCInitStructure.OutputNState = TIM_OUTPUT_NSTATE_ENABLE;
    TIM_OCInitStructure.Pulse        = 8000;
    TIM_OCInitStructure.OcPolarity   = TIM_OC_POLARITY_LOW;
    TIM_OCInitStructure.OcNPolarity  = TIM_OCN_POLARITY_LOW;
    TIM_OCInitStructure.OcIdleState  = TIM_OC_IDLE_STATE_SET;
    TIM_OCInitStructure.OcNIdleState = TIM_OCN_IDLE_STATE_RESET;

    TIM_Output_Channel3_Initialize(TIM4, &TIM_OCInitStructure);
}

void exti_clk_gpio_init(void)
{
    GPIO_InitType GPIO_InitStructure;

    RCC_AHB_Peripheral_Clock_Enable(RCC_AHB_PERIPH_GPIOA);
    RCC_APB2_Peripheral_Clock_Enable(RCC_APB2_PERIPH_AFIO);

    GPIO_Structure_Initialize(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Current = GPIO_DS_12MA;
    GPIO_InitStructure.Pin        = GPIO_PIN_4; 
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF6_TIM4;
    GPIO_Peripheral_Initialize(GPIOA, &GPIO_InitStructure);
}
#define MAX_NUM 9
uint8_t rgb_color[MAX_NUM][4] = {
    {RGB_ONE_COLOR_MODE, 255, 0, 0},
    {RGB_ONE_COLOR_MODE, 255, 64, 0},
    {RGB_ONE_COLOR_MODE, 255, 220, 0},
    {RGB_ONE_COLOR_MODE, 0, 255, 0},
    {RGB_ONE_COLOR_MODE, 0, 0, 255},
    {RGB_ONE_COLOR_MODE, 255, 0, 255},
    {RGB_ONE_COLOR_MODE, 255, 20, 147},
    {RGB_GRADUAL_MODE, 255, 0, 0},
    {RGB_RAINBOW_MODE, 255, 0, 0}
};
void bsp_spi_Task(void *parameter)
{
    rgb_sys_t rgb_sys_slave;
	rgb_sys_t rgb_sys_last;
    BaseType_t xReturn = pdTRUE;
    uint8_t tx_cnt = 0;
	uint8_t lvd_en = 0;
	uint8_t cmd = 0;
	uint8_t color_num = MAX_NUM;
    rgb_sys_slave.lvdong_en = 0;
    rgb_sys_slave.light_level = 100;
    rgb_sys_slave.rgb_mode = RGB_OFF_MODE;
    rgb_sys_slave.rgb.color.r = 255;
    rgb_sys_slave.rgb.color.g = 0;
    rgb_sys_slave.rgb.color.b = 0;
	rgb_sys_slave.msg_state = 0;
	
	rgb_sys_last.lvdong_en = 0;
    rgb_sys_last.light_level = 100;
    rgb_sys_last.rgb_mode = RGB_OFF_MODE;
    rgb_sys_last.rgb.color.r = 255;
    rgb_sys_last.rgb.color.g = 0;
    rgb_sys_last.rgb.color.b = 0;
	rgb_sys_last.msg_state = 0;
	
    vTaskDelay(100);
    bsp_spi_init();
    exti_clk_gpio_init();
    TIM4_pwm_init();
    TIM4_base_init();
    while(1)
    {		
        if (pdTRUE == xQueueReceive(RGB_SPI_Send_Queue, &rgb_sys_slave, 0) || xQueueReceive(lvdong_Queue, &lvd_en, 0) == pdTRUE)
        {
			spi_tx_buf[0] = rgb_sys_slave.rgb_mode;
            spi_tx_buf[1] = rgb_sys_slave.rgb.color.r;
            spi_tx_buf[2] = rgb_sys_slave.rgb.color.g;
            spi_tx_buf[3] = rgb_sys_slave.rgb.color.b;
            spi_tx_buf[4] = rgb_sys_slave.light_level;
            spi_tx_buf[5] = lvd_en;
            spi_tx_buf[6] = rgb_sys_slave.led_state;
            spi_tx_buf[7] = rgb_sys_slave.msg_state;
            spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
            DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
            DMA_Buffer_Size_Config(DMA_CH1,9);
            SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
            DMA_Channel_Enable(DMA_CH1);
            SPI_ON(SPI_MASTER);
			vTaskDelay(2);
            tx_cnt = 0;
        }
//        else
//        {
//            vTaskDelay(10);
//            if(tx_cnt < 10) 
//            {
//                tx_cnt++;
//            }
//            else
//            {
//                tx_cnt = 0;
//				spi_tx_buf[0] = rgb_sys_last.rgb_mode;
//				spi_tx_buf[1] = rgb_sys_last.rgb.color.r;
//				spi_tx_buf[2] = rgb_sys_last.rgb.color.g;
//				spi_tx_buf[3] = rgb_sys_last.rgb.color.b;
//				spi_tx_buf[4] = rgb_sys_last.light_level;
//				spi_tx_buf[5] = rgb_sys_last.lvdong_en;
//				spi_tx_buf[6] = rgb_sys_last.led_state;
//				spi_tx_buf[7] = rgb_sys_last.msg_state;
//				spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
//                DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
//                DMA_Buffer_Size_Config(DMA_CH1,9);
//                SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
//                DMA_Channel_Enable(DMA_CH1);
//                SPI_ON(SPI_MASTER);
//            }
//        }
		
		if (xQueueReceive(RGB_Cmd_Queue, &cmd, 0) == pdTRUE)
        {
            switch (cmd)
            {
            case RGB_ON:
                /* code */
                spi_tx_buf[0] = rgb_color[color_num][0];
                spi_tx_buf[1] = rgb_color[color_num][1];
                spi_tx_buf[2] = rgb_color[color_num][2];
                spi_tx_buf[3] = rgb_color[color_num][3];
                spi_tx_buf[4] = rgb_sys_slave.light_level;
                spi_tx_buf[5] = lvd_en;
                spi_tx_buf[6] = rgb_sys_slave.led_state;
                spi_tx_buf[7] = rgb_sys_slave.msg_state;
                spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
                DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
                DMA_Buffer_Size_Config(DMA_CH1,9);
                SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
                DMA_Channel_Enable(DMA_CH1);
                SPI_ON(SPI_MASTER);
                break;
            case RGB_OFF:
                /* code */
                spi_tx_buf[0] = RGB_OFF_MODE;
                spi_tx_buf[1] = rgb_color[color_num][1];
                spi_tx_buf[2] = rgb_color[color_num][2];
                spi_tx_buf[3] = rgb_color[color_num][3];
                spi_tx_buf[4] = rgb_sys_slave.light_level;
                spi_tx_buf[5] = lvd_en;
                spi_tx_buf[6] = rgb_sys_slave.led_state;
                spi_tx_buf[7] = rgb_sys_slave.msg_state;
                spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
                DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
                DMA_Buffer_Size_Config(DMA_CH1,9);
                SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
                DMA_Channel_Enable(DMA_CH1);
                SPI_ON(SPI_MASTER);
                break;
            case LVDONG_ON_OFF:
                /* code */
                if(lvd_en == 0)lvd_en = 1;
                else lvd_en = 0;
                spi_tx_buf[0] = rgb_color[color_num][0];
                spi_tx_buf[1] = rgb_color[color_num][1];
                spi_tx_buf[2] = rgb_color[color_num][2];
                spi_tx_buf[3] = rgb_color[color_num][3];
                spi_tx_buf[4] = rgb_sys_slave.light_level;
                spi_tx_buf[5] = lvd_en;
                spi_tx_buf[6] = rgb_sys_slave.led_state;
                spi_tx_buf[7] = rgb_sys_slave.msg_state;
                spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
                DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
                DMA_Buffer_Size_Config(DMA_CH1,9);
                SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
                DMA_Channel_Enable(DMA_CH1);
                SPI_ON(SPI_MASTER);
                break;
            case RGB_COLOR_UP:
                /* code */
                if(color_num < MAX_NUM) color_num++;
                else color_num = 0;
				if(color_num == MAX_NUM)
				{
					spi_tx_buf[0] = RGB_OFF_MODE;
					spi_tx_buf[1] = rgb_color[MAX_NUM-1][1];
					spi_tx_buf[2] = rgb_color[MAX_NUM-1][2];
					spi_tx_buf[3] = rgb_color[MAX_NUM-1][3];
				}
				else
				{
					spi_tx_buf[0] = rgb_color[color_num][0];
					spi_tx_buf[1] = rgb_color[color_num][1];
					spi_tx_buf[2] = rgb_color[color_num][2];
					spi_tx_buf[3] = rgb_color[color_num][3];
				}
                spi_tx_buf[4] = rgb_sys_slave.light_level;
                spi_tx_buf[5] = lvd_en;
                spi_tx_buf[6] = rgb_sys_slave.led_state;
                spi_tx_buf[7] = rgb_sys_slave.msg_state;
                spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
                DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
                DMA_Buffer_Size_Config(DMA_CH1,9);
                SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
                DMA_Channel_Enable(DMA_CH1);
                SPI_ON(SPI_MASTER);
                break;
            case RGB_COLOR_DOWN:
                /* code */
                if(color_num > 0) color_num--;
                else color_num = MAX_NUM;
                if(color_num == MAX_NUM)
				{
					spi_tx_buf[0] = RGB_OFF_MODE;
					spi_tx_buf[1] = rgb_color[MAX_NUM-1][1];
					spi_tx_buf[2] = rgb_color[MAX_NUM-1][2];
					spi_tx_buf[3] = rgb_color[MAX_NUM-1][3];
				}
				else
				{
					spi_tx_buf[0] = rgb_color[color_num][0];
					spi_tx_buf[1] = rgb_color[color_num][1];
					spi_tx_buf[2] = rgb_color[color_num][2];
					spi_tx_buf[3] = rgb_color[color_num][3];
				}
                spi_tx_buf[4] = rgb_sys_slave.light_level;
                spi_tx_buf[5] = lvd_en;
                spi_tx_buf[6] = rgb_sys_slave.led_state;
                spi_tx_buf[7] = rgb_sys_slave.msg_state;
                spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
                DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
                DMA_Buffer_Size_Config(DMA_CH1,9);
                SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
                DMA_Channel_Enable(DMA_CH1);
                SPI_ON(SPI_MASTER);
                break;
            case RGB_LIGHT_UP:
                /* code */
                if(rgb_sys_slave.light_level < 100) rgb_sys_slave.light_level +=5;
                spi_tx_buf[0] = rgb_color[color_num][0];
                spi_tx_buf[1] = rgb_color[color_num][1];
                spi_tx_buf[2] = rgb_color[color_num][2];
                spi_tx_buf[3] = rgb_color[color_num][3];
                spi_tx_buf[4] = rgb_sys_slave.light_level;
                spi_tx_buf[5] = lvd_en;
                spi_tx_buf[6] = rgb_sys_slave.led_state;
                spi_tx_buf[7] = rgb_sys_slave.msg_state;
                spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
                DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
                DMA_Buffer_Size_Config(DMA_CH1,9);
                SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
                DMA_Channel_Enable(DMA_CH1);
                SPI_ON(SPI_MASTER);
                break;
            case RGB_LIGHT_DOWN:
                /* code */
                if(rgb_sys_slave.light_level > 0) rgb_sys_slave.light_level -=5;
                spi_tx_buf[0] = rgb_color[color_num][0];
                spi_tx_buf[1] = rgb_color[color_num][1];
                spi_tx_buf[2] = rgb_color[color_num][2];
                spi_tx_buf[3] = rgb_color[color_num][3];
                spi_tx_buf[4] = rgb_sys_slave.light_level;
                spi_tx_buf[5] = lvd_en;
                spi_tx_buf[6] = rgb_sys_slave.led_state;
                spi_tx_buf[7] = rgb_sys_slave.msg_state;
                spi_tx_buf[8] = checksum_8(spi_tx_buf, 8);
                DMA_Memory_Address_Config(DMA_CH1,(uint32_t)spi_tx_buf);
                DMA_Buffer_Size_Config(DMA_CH1,9);
                SPI_I2S_DMA_Transfer_Enable(SPI_MASTER, SPI_I2S_DMA_TX);
                DMA_Channel_Enable(DMA_CH1);
                SPI_ON(SPI_MASTER);
                break;
            default:
                break;
            }
            vTaskDelay(2);
        }
    }
}


void rgb_auto_off_callback(TimerHandle_t xTimer)
{
    rgb_sys_t rgb;
    rgb.rgb_mode = RGB_OFF_MODE;
    rgb.msg_state = 0;
    xQueueSend(RGB_SPI_Send_Queue, &rgb, NULL);
}




