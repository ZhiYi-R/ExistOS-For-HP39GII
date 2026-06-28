/**
 * @file Bootloader/boot/services/ftl_service.c
 * @brief ftl_service module
 */

#include "services.h"
#include "FTL_up.h"

extern uint32_t g_FTL_status;

void vFTLSvc(void *pvParameters)
{
    g_FTL_status = Ftl::init();
    for(;;)
        Ftl::task();
}