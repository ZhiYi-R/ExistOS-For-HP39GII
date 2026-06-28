/**
 * @file Bootloader/Hal/display_up.h
 * @brief display_up module
 */

#ifndef __DISPLAY_UP_H__
#define __DISPLAY_UP_H__

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define INDICATE_LEFT      (1 << 0)
#define INDICATE_RIGHT     (1 << 1)
#define INDICATE_A__Z      (1 << 2)
#define INDICATE_a__z      (1 << 3)
#define INDICATE_BUSY      (1 << 4)
#define INDICATE_TX        (1 << 5)
#define INDICATE_RX        (1 << 6)

// The display HAL is now the pure-static C++ `Display` class; every former
// free `DisplayXxx()` entry is a `Display::xxx` method (Phase 3.5a fold). This
// header is retained as a thin compatibility shim — it pulls in the class for
// the translation units that still `#include "display_up.h"` and keeps the
// indicator-bit macros above. New code should include "display_up.hpp" directly.
#include "display_up.hpp"

#endif


