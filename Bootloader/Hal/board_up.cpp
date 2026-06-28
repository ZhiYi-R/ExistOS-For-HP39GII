/**
 * @file Bootloader/Hal/board_up.c
 * @brief board_up module
 */


#include "FreeRTOS.h"
#include "task.h"

#include "stmp_clkctrl.hpp"
#include "stmp_uartdbg.hpp"
#include "interrupt_up.h"

#include "stmp_board.hpp"
#include "stmp_audioout.hpp"
#include "mtd_up.h"
#include "display_up.h"
#include "keyboard_up.h"
#include "stmp_gpio.hpp"
#include "stmp_rtc.hpp"

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

    Uart::init();

    IRQInit();

    Mtd::interfaceInit();

    Display::interfaceInit();

    Keyboard::gpioInit();


    Rtc::init();

    
#ifdef ENABLE_AUIDIOOUT
    AudioOut::init();
#endif
}

 
