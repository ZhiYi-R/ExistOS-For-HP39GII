/**
 * @file Bootloader/drivers/timer/timer_up.c
 * @brief timer_up module
 */

#include "FreeRTOS.h"
#include "task.h"

#include "stmp_timer.hpp"
#include "timer_up.h"
#include "interrupt_up.h"

#define LF_TimerFreq    (configTICK_RATE_HZ)

//volatile unsigned long ulHighFrequencyTimerTicks;

int LFTimer = -1;


bool up_TimerSetup( void ){

    Timer::init();

    LFTimer = Timer::getTimer();

    Timer::setPeriod(LFTimer, 1000000 / (LF_TimerFreq));

    Timer::enableIRQ(LFTimer, true);

    Timer::enable(LFTimer, true);

    return true;
}



void up_TimerTick()
{
    
    if( xTaskIncrementTick() != pdFALSE )
	{	
		vTaskSwitchContext();
	}
}


