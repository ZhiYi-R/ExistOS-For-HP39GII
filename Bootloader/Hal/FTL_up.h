/**
 * @file Bootloader/Hal/FTL_up.h
 * @brief FTL_up module — compatibility shim.
 *
 * The FTL HAL is now the pure-static C++ `Ftl` class over Dhara + the `Mtd`
 * NAND service; every former free `FTL_*()` entry is an `Ftl::xxx` method and
 * the geometry / operation types live in FTL_up.hpp (Phase 3.5c fold). This
 * header is retained only as a thin shim for translation units that still
 * `#include "FTL_up.h"`; new code should include "FTL_up.hpp" directly.
 */

#ifndef __FTL_UP_H__
#define __FTL_UP_H__

#include "FTL_up.hpp"

#endif
