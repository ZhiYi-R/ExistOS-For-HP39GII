/**
 * @file Tests/host/stubs_batch2/FreeRTOS.h
 * @brief FreeRTOS module
 */

#pragma once

#include <stddef.h>

void *pvPortMalloc(size_t size);
void vPortFree(void *ptr);
