#ifndef _SOUND_BOX_H_
#define _SOUND_BOX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define CMD_TREBLE_UP       0x01
#define CMD_TREBLE_DOWN     0x02
#define CMD_BASS_UP         0x03
#define CMD_BASS_DOWN       0x04
#define CMD_VOLUME_UP       0x05
#define CMD_VOLUME_DOWN     0x06
#define CMD_VIBRATE_UP      0x07
#define CMD_VIBRATE_DOWN    0x08
#define CMD_BLE_DISABLE     0x09
#define CMD_PLAY_STOP       0x0A
#define CMD_AURACAST_ON_OFF 0X10

/* Constants */

/* Macros */

/* Types */
typedef struct
{
    uint8_t msg_fun;
    uint8_t msg_data;
}Sound_CMD_Typedef;

extern uint8_t EQ_mode;
extern uint8_t sound_high_pitch;
extern uint8_t sound_low_pitch;
extern uint8_t vibrate_level;
extern QueueHandle_t Sound_Box_CMD_Queue;
extern QueueHandle_t Sound_Box_Reply_Queue;
extern TimerHandle_t xTimer_UsartSoundTimeout;
/* Functions */
void Sound_Box_Task(void *parameter);
void UsartSoundTimeoutCallback(TimerHandle_t xTimer);
void Sound_Box_Config_Task(void *parameter);

#ifdef __cplusplus
}
#endif

#endif /* _SOUND_BOX_H_ */
