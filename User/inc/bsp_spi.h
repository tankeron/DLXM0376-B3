#ifndef __BSP_SPI_H__
#define __BSP_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Constants */
#define SPI_MASTER                SPI1
#define SPI_MASTER_PERIPH         RCC_APB2_PERIPH_SPI1
#define SPI_MASTER_PERIPH_GPIO    RCC_AHB_PERIPH_GPIOA
#define SPI_MASTER_GPIO           GPIOA
#define SPI_MASTER_GPIO_ALTERNATE GPIO_AF1_SPI1
#define SPI_MASTER_MISO_PIN       GPIO_PIN_6
#define SPI_MASTER_MOSI_PIN       GPIO_PIN_7
#define SPI_MASTER_CLK_PIN        GPIO_PIN_5
#define SPI_MASTER_NSS_PIN        GPIO_PIN_4


#define SPI_SALVE_NSS_GPIO        GPIOB
#define SPI_SALVE1_NSS_PIN        GPIO_PIN_0
//#define SPI_SALVE2_NSS_PIN        GPIO_PIN_1
//#define SPI_SALVE3_NSS_PIN        GPIO_PIN_2
//#define SPI_SALVE4_NSS_PIN        GPIO_PIN_10
//#define SPI_SALVE5_NSS_PIN        GPIO_PIN_11


// #define SOFT_SPI1_CLK_GPIO   GPIOB
// #define SOFT_SPI1_CLK_PIN    GPIO_PIN_0
// #define SOFT_SPI2_CLK_GPIO   GPIOB
// #define SOFT_SPI2_CLK_PIN    GPIO_PIN_1
// #define SOFT_SPI3_CLK_GPIO   GPIOB
// #define SOFT_SPI3_CLK_PIN    GPIO_PIN_2
// #define SOFT_SPI4_CLK_GPIO   GPIOB
// #define SOFT_SPI4_CLK_PIN    GPIO_PIN_10
// #define SOFT_SPI5_CLK_GPIO   GPIOB
// #define SOFT_SPI5_CLK_PIN    GPIO_PIN_11

// #define SOFT_SPI_DATA0_GPIO GPIOA
// #define SOFT_SPI_DATA0_PIN  GPIO_PIN_5
// #define SOFT_SPI_DATA1_GPIO GPIOA
// #define SOFT_SPI_DATA1_PIN  GPIO_PIN_6
// #define SOFT_SPI_DATA2_GPIO GPIOA
// #define SOFT_SPI_DATA2_PIN  GPIO_PIN_7






#define RGB_OFF_MODE            0X00
#define RGB_ONE_COLOR_MODE      0X01
#define RGB_GRADUAL_MODE        0X02
#define RGB_RAINBOW_MODE        0X03

#define SPI_SEND_EVENT  (0x01 << 0)//设置事件掩码的位0



#define RGB_COLOR_UP    0X01
#define RGB_COLOR_DOWN  0X02
#define RGB_LIGHT_UP    0X03
#define RGB_LIGHT_DOWN  0X04
#define RGB_ON          0X05
#define RGB_OFF         0X06
#define LVDONG_ON_OFF   0X07
/* Macros */

/* Types */
typedef union {
    struct {
        uint8_t b;  /*!< Blue component */
        uint8_t g;  /*!< Green component */
        uint8_t r;  /*!< Red component */
    } color;
    uint32_t value; /*!< Whole color value */
}rgb_t;

typedef struct
{
    /* data */
    uint8_t msg_state;
    uint8_t rgb_mode;
    uint8_t lvdong_en;
    uint8_t light_level;
    uint8_t rgb_state;
    uint8_t led_state; 
    rgb_t rgb;
}rgb_sys_t;

extern QueueHandle_t RGB_SPI_Send_Queue;
extern QueueHandle_t lvdong_Queue;
extern QueueHandle_t RGB_Cmd_Queue;

extern rgb_sys_t rgb_sys_slave;
extern TaskHandle_t rgb_spi_Handle;
extern TimerHandle_t xTimer_RGBTimeout;
/* Functions */
void bsp_spi_Task(void *parameter);
void rgb_auto_off_callback(TimerHandle_t xTimer);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SPI_H__ */
