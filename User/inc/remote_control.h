#ifndef _REMOTE_CONTROL_H_
#define _REMOTE_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Constants */

/* Macros */

/* Types */

typedef struct
{
    uint8_t cs_cmd;
    uint8_t cs_data;
}CS_STATUS_Typedef;




/* Functions */
void vDummyCallback(TimerHandle_t xTimer);
void Remote_Task(void *parameter);
#ifdef __cplusplus
}
#endif

#endif /* _SOUND_BOX_H_ */
