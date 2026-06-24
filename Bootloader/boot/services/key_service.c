/**
 * @file Bootloader/boot/services/key_service.c
 * @brief key_service module
 */

#include "services.h"
#include "keyboard_up.h"

void vKeysSvc(void *pvParameters)
{
    key_svcInit();
    for(;;)
    {
        key_task();
    }
}