/**
 * @file System/Utils/khicas_launch.cpp
 * @brief KhiCAS launcher: runs a full KhiCAS session for the calculator app.
 *
 * Entry point khicasLaunch() is invoked from khicasTask() in System/Core/main.c.
 * This file was historically named test.cpp / testcpp() back when it served as a
 * giac porting test harness; the test scaffolding has been removed and the live
 * entry point renamed. The giac porting hooks below (rtc_get_tick_ms, cout) and
 * the interrupt flags are referenced by libgiac and must stay.
 */

#include <stdlib.h>

#include "ff.h"
#include "Apps/user/khicas/kcasporing_gl.h"

#include "../External/KhiCAS/khicas/iostream_sub.h"
#include "giac.h"
#include "gen.h"

#include "sys_llapi.h"

extern "C" uint32_t getCurStackAdr(void);

// giac interrupt flags (referenced by libgiac).
volatile bool interrupted = false;
volatile bool ctrl_c = false;

namespace giac {

// giac porting hooks referenced by libgiac.
unsigned int rtc_get_tick_ms()
{
    return ll_get_time_ms();
}

stdostream cout;

}

// Corrected KhiCAS REPL entry (System/Apps/user/khicas/khicas_repl.cpp). Replaces
// the prebuilt kcas_main(), whose loop reports a bogus "memory error" and hangs
// when the user presses Home/MENU to quit (Console_GetLine returns NULL).
int khicas_repl(int isAppli, unsigned short OptionNum);

extern "C" {
extern bool khicasRunning;
#include "SystemFs.h"
#include "SysConf.h"

void khicasLaunch()
{
    khicasRunning = true;

    if (vGL_Initialize() != 0)
    {
        khicasRunning = false;
        return;
    }

#if FS_TYPE == FS_FATFS
    FRESULT fr = f_mkdir("/xcas");
    if (fr != FR_OK)
    {
        if (fr != FR_EXIST) {
            std::cout << "Failed to create dir /xcas, " << fr << std::endl;
        }
    }
#else
    lfs *fs = (lfs *)GetFsObj();
    int fr = lfs_mkdir(fs, "/xcas");
    if (fr != 0) {
        if (fr != LFS_ERR_EXIST)
            std::cout << "Failed to create dir /xcas, " << fr << std::endl;
    }
#endif
    printf("KhiCAS STACK ADDR:%08x\n", getCurStackAdr());

    khicas_repl(0, 0);

    // khicas_repl() returns when the user picks "Quit" from the Home menu
    // (Console_GetLine -> NULL). Stop the KhiCAS helper tasks before releasing
    // the framebuffers, otherwise the orphaned vGL flush task keeps painting
    // KhiCAS over the resumed system UI (flicker) and stale tasks eat input.
    void khicasStopHelpers();
    khicasStopHelpers();

    vGL_Release();
}

}
