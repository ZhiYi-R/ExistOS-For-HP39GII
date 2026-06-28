/**
 * @file Bootloader/boot/services/mtd_service.c
 * @brief mtd_service module
 */

#include "services.h"
#include "mtd_up.h"

void vMTDSvc(void *pvParameters)
{
    Mtd::deviceInit();
    for(;;)
        Mtd::task();
}