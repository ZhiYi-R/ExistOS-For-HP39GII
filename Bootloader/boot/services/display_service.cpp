/**
 * @file Bootloader/boot/services/display_service.c
 * @brief display_service module
 */

#include "services.h"
#include "display_up.h"

void vDispSvc(void *pvParameters)
{
    Display::init();

    Display::setIndicate(0, 0);

    for(;;){
        Display::task();
    }
}