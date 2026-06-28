/**
 * @file Bootloader/Hal/mtd_up.h
 * @brief mtd_up module — compatibility shim.
 *
 * The MTD HAL is now the pure-static C++ `Mtd` class over the `Gpmi` driver;
 * every former free `MTD_*()` entry is an `Mtd::xxx` method and the geometry /
 * operation types live in mtd_up.hpp (Phase 3.5b fold). This header is retained
 * only as a thin shim for translation units that still `#include "mtd_up.h"`;
 * new code should include "mtd_up.hpp" directly.
 */

#ifndef __MTD_UP_H__
#define __MTD_UP_H__

#include "mtd_up.hpp"

#endif
