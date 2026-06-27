/**
 * @file Bootloader/boot/services/usb_service.c
 * @brief usb_service module
 */

#include "services.h"
#include "tusb.h"

void vTaskTinyUSB(void *pvParameters)
{
    tusb_init();
    for(;;)
        tud_task();
}