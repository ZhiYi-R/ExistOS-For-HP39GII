/**
 * @file Bootloader/boot/services/display_service.c
 * @brief display_service module
 */

#include "services.h"
#include "display_up.h"

void vDispSvc(void *pvParameters)
{
    DisplayInit();

    DisplaySetIndicate(0, 0);

    for(;;){
        DisplayTask();
    }
}