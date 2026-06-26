/**
 * @file System/Drivers/SystemFs.c
 * @brief Filesystem initialization
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"


#include "SysConf.h"

#include "ff.h"


//#include "lvgl.h"

#include "debug.h"

#include "SystemFs.h"
#include "SystemUI.h"


FATFS *fs;

void SystemFSInit() {
    uint32_t sel = 0;
    int err  = 0;


    FRESULT fres;
    fs = pvPortMalloc(sizeof(FATFS));


/*
    fres = f_mount(fs, FS_FLASH_PATH, 1);

    printf("f_mount res:%d\n", fres);
    if (fres != FR_OK) {
        sprintf(textbuf, "Mount Fatfs Failed:-%d, would you like to format the flash?", fres);
        sel = SystemUIMsgBox(NULL, textbuf, "Mount " FS_FLASH_PATH " Failed", SYSTEMUI_MSGBOX_BUTTON_CANCAL | SYSTEMUI_MSGBOX_BUTTON_OK);
        if (sel == 0) {

            lv_obj_t *spinner = lv_spinner_create(lv_scr_act(), 1000, 60);
            lv_obj_set_size(spinner, 50, 50);
            lv_obj_center(spinner);

            BYTE *work = pvPortMalloc(FF_MAX_SS);
            fres = f_mkfs(FS_FLASH_PATH, 0, work, FF_MAX_SS);
            printf("mkfs:%d\n", fres);

            lv_obj_del(spinner);

            vPortFree(work);

            if (fres == FR_OK) {
                SystemUIMsgBox(NULL,"Format " FS_FLASH_PATH " Succeeded.", "Format", 0);
            } else {
                SystemUIMsgBox(NULL,"Format " FS_FLASH_PATH " Failed.", "Format", 0);
            }
        }
        goto mount_flash;
    }

    */




}


void SystemFSDeInit() 
{



}
