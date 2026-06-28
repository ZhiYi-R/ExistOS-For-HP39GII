/**
 * @file Bootloader/Hal/board_up.c
 * @brief board_up module
 */


#include "FreeRTOS.h"
#include "task.h"

#include "stmp_clkctrl.hpp"
#include "uart_up.h"
#include "interrupt_up.h"

#include "stmp_board.hpp"
#include "stmp_audioout.hpp"
#include "mtd_up.h"
#include "display_up.h"
#include "keyboard_up.h"
#include "stmp_gpio.hpp"
#include "rtc_up.h"

#include "debug.h"

bool driverWaitTrueF(bool (*f)(), TickType_t timeout)
{
    while((int)((*f)()) == false)
    {
        //vTaskDelay(1);
        portDelayus(1);
        if(timeout != portMAX_DELAY)
        {
            if(timeout >= 1)
            {
                timeout -= 1;
            }else{
                return true;
            }
        }
    }
    return false;
}

bool driverWaitFalseF(bool (*f)(), TickType_t timeout)
{
    while((int)((*f)()) == true)
    {
        //vTaskDelay(1);
        portDelayus(1);
        if(timeout != portMAX_DELAY)
        {
            if(timeout >= 1)
            {
                timeout -= 1;
            }else{
                return true;
            }
        }
    }
    return false;
}

void boardInit(void)
{
    INFO("portBoardInit\n");

    Clk::init();

    

    portBoardInit();

    uartInit();

    IRQInit();

    MTD_InterfaceInit();

    Display_InterfaceInit();

    Keyboard::gpioInit();

    
    rtc_init();

    
#ifdef ENABLE_AUIDIOOUT
    stmp_audio_init();
#endif
}

 
