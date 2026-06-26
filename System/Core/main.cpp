/**
 * @file System/Core/main.c
 * @brief Entry point and initialization
 */



#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include "keyboard.h"
#include "llapi_code.h"
#include "sys_llapi.h"
#include "sys_llapi.hpp" // 强类型门面(ll::mem_phy_info 等);须在下方 extern "C" 块之前包含

#include "SysConf.h"

#include "FreeRTOS.h"
#include "task.h"

//#include "ff.h"

#include "debug.h"
#include "Crash/CrashLog.h"

#include "SystemFs.h"
#include "SystemUI.h"

#include "VROMLoader.h"

#include "ff.h"
//#include "mpy_port.h"

// main.c 是 C 入口:被 startup/FreeRTOS/newlib/asm 以及 UICore 按 C 名引用
// (enableMemSwap / StartKhiCAS / UI_OOM / khicasLaunch / IRQ_ISR / SWI_ISR /
// symtab_def 等两端均为 C 链接)。整文件包 extern "C",迁移到 .cpp 后符号名不变。
extern "C" {

void check_emulator_status();

uint32_t OnChipMemorySize = BASIC_RAM_SIZE;
uint32_t SwapMemorySize = 0;
uint32_t TotalAllocatableSize = BASIC_RAM_SIZE;
bool MemorySwapEnable = false;

void enableMemSwap(bool enable) {
    MemorySwapEnable = enable;
    ll_mem_swap_enable((uint32_t)enable);
    if (enable) {
        SwapMemorySize = ll_mem_swap_size();
    } else {
        SwapMemorySize = 0;
    }
    TotalAllocatableSize = OnChipMemorySize + SwapMemorySize;
}

volatile unsigned long ulHighFrequencyTimerTicks;

char pcWriteBuffer[4096];
void printTaskList() {

    size_t getOnChipHeapAllocated();
    size_t getSwapMemHeapAllocated();
    uint32_t getHeapAllocateSize();

    vTaskList((char *)&pcWriteBuffer);
    printf("=============SYSTEM STATUS=================\r\n");
    printf("Task Name         Task Status   Priority   Stack   ID\n");
    printf("%s\n", pcWriteBuffer);
    printf("Task Name                Running Count         CPU %%\n");
    vTaskGetRunTimeStats((char *)&pcWriteBuffer);
    printf("%s\n", pcWriteBuffer);
    printf("Status:  X-Running  R-Ready  B-Block  S-Suspend  D-Delete\n");

    float mem_cmpr = ll_mem_comprate();
    auto [total_phy_mem, free, total] = ll::mem_phy_info();
    total_phy_mem /= 1024;
    free /= 1024;
    total /= 1024;

    printf("Allocate MEM:%d/%d KB\n", getHeapAllocateSize() / 1024, TotalAllocatableSize / 1024);
    printf("ZRAM:%d/%d KB\n", total - free, total);
    printf("Compression_rate: %.2f\n", mem_cmpr);
    printf("SRAM Heap Pre-allocated: %d KB\n", getOnChipHeapAllocated() / 1024);
    printf("Swap Heap Pre-allocated: %d KB\n", getSwapMemHeapAllocated() / 1024);
    //    printf("Free memory:   %d Bytes\n", (unsigned int)xPortGetFreeHeapSize());
}

void vTask1(void *par1) {
    while (1) {
        printTaskList();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void softDelayMs(uint32_t ms) {
    uint32_t cur = ll_get_time_ms();
    while ((ll_get_time_ms() - cur) < ms) {
        ;
    }
}

void delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void vTask2(void *par1) {
    uint32_t ticks = 0;
    while (1) {
        printf("SYS Run Time: %d s\n", ticks);
        ticks++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vApplicationIdleHook(void) {
    ll_system_idle();
}

void exp_exec(void *par);
static bool time_label_refresh = true;

#define EMU_DATA_PORT ((volatile uint8_t *)0x20000000)
extern bool g_system_in_emulator;

void khicasTask(void *_) {

    SystemUISuspend();

    void khicasLaunch();
    khicasLaunch();

    SystemUIResume();

    vTaskDelete(NULL);
}

void StartKhiCAS() {
    xTaskCreate(khicasTask, "KhiCAS", KhiCAS_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, (NULL));
}

void main_thread(void *) {

    // printf("R13:%08x\n", get_stack());

    SystemUIInit();
    SystemFSInit();
    
    // 初始化崩溃日志系统
    crash_log_init();

    // StartKhiCAS();

    for (;;) {

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

uint32_t __attribute__((naked)) getCurStackAdr() {
    __asm volatile("mov r0,r13");
    __asm volatile("mov pc,lr");
}
extern uint32_t SYSTEM_STACK; // in ld script
int main() {
    void IRQ_ISR(uint32_t IRQNum, uint32_t par1, uint32_t par2, uint32_t par3);
    void SWI_ISR(void);
    ll_set_irq_stack((uint32_t)&SYSTEM_STACK);
    ll_set_irq_vector(((uint32_t)IRQ_ISR) + 4);
    ll_set_svc_stack(((uint32_t)&SYSTEM_STACK) - 0x500);
    ll_set_svc_vector(((uint32_t)SWI_ISR) + 4);
    ll_enable_irq(false);

    ll_cpu_slowdown_enable(false);

    uint32_t memsz = ll::mem_phy_info().total_compressed;
    if (memsz > OnChipMemorySize) {
        OnChipMemorySize = memsz;
    }

    VROMLoader_Initialize();

    printf("System Booting...\n");

    printf("SP:%08x\n", getCurStackAdr());

    uint32_t total_comp = ll::mem_phy_info().total_compressed;
    if (total_comp > OnChipMemorySize) {
        OnChipMemorySize = total_comp;
    }
    SwapMemorySize = ll_mem_swap_size();
    TotalAllocatableSize = OnChipMemorySize + SwapMemorySize;

    xTaskCreate(vTask1, "PrintTask", 400, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(main_thread, "System", 400, NULL, configMAX_PRIORITIES - 3, NULL);

    vTaskStartScheduler();

    for (;;) {
        void symtab_def();
        symtab_def();
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    STACK_OVERFLOW("SYS StackOverflowHook:%s\n", pcTaskName);
}

void vAssertCalled(char *file, int line) {
    ASSERT_FAIL("SYS ASSERTION FAILED AT %s:%d\n", file, line);
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
    *ppxTimerTaskTCBBuffer = (StaticTask_t *)pvPortMalloc(sizeof(StaticTask_t));
    *ppxTimerTaskStackBuffer = (StackType_t *)pvPortMalloc(configMINIMAL_STACK_SIZE * 4);
    *pulTimerTaskStackSize = configMINIMAL_STACK_SIZE * 4;
}

void vApplicationMallocFailedHook() {

    void UI_OOM();
    UI_OOM();
    OUT_OF_MEMORY("SYS ASSERT: Out of Memory.\n");
}

void check_emulator_status() {

    if (g_system_in_emulator) {
        if (EMU_DATA_PORT[0]) {
            FIL savef;
            FRESULT fr;
            UINT br = 0;
            char *fname = (char *)&EMU_DATA_PORT[10];
            uint32_t fsz = ((uint32_t *)(&EMU_DATA_PORT[4]))[0];
            printf("File send command detected.\n");
            printf("Receive file:%s\n", fname);
            printf("file size:%d\n", fsz);

            fname--;
            *fname = '/';
            fr = f_open(&savef, fname, FA_CREATE_ALWAYS | FA_WRITE);
            if (fr) {
                printf("Failed to create file:%s\n", fname);
            } else {
                f_write(&savef, (const void *)&EMU_DATA_PORT[200], fsz, &br);
                printf("File wrote to:%s, wsz:%d\n", fname, br);
                f_close(&savef);

                char *testname = fname + 1;
                while (*testname) {
                    if (
                        (testname[0] == '.') &&
                        (testname[1] == 'e') &&
                        (testname[2] == 'x') &&
                        (testname[3] == 'p') &&
                        (testname[4] == 0)) {
                        // xTaskCreate(exp_exec, fname + 1, configMINIMAL_STACK_SIZE, fname, configMAX_PRIORITIES - 3, NULL);
                    }
                    testname++;
                }
            }

            EMU_DATA_PORT[0] = 0;
        }
    }
}

}  // extern "C"
