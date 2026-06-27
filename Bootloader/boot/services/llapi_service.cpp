/**
 * @file Bootloader/boot/services/llapi_service.c
 * @brief llapi_service module
 */

#include "services.h"
#include "llapi.h"

extern TaskHandle_t pSysTask;

void vLLAPISvc(void *pvParameters)
{
    LLAPI_init(pSysTask);
    for(;;){
        LLAPI_Task();
    }
}