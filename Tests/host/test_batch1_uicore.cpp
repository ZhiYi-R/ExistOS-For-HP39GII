/**
 * @file Tests/host/test_batch1_uicore.cpp
 * @brief test_batch1_uicore module
 */

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
#include "../../System/third_party/freertos/include/SysConf.h"
#include "../../System/graphics/SystemUI.h"
#include "../../System/drivers/keyboard_gii39.h"
}

static void *pvPortMalloc(size_t size) {
    return std::malloc(size);
}

static void vPortFree(void *ptr) {
    std::free(ptr);
}

static void vTaskDelay(uint32_t) {
}

static uint32_t ll_vm_check_key(void) {
    return (1u << 16) | KEY_ENTER;
}

static void stub_draw(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t) {
}

extern const unsigned char VGA_Ascii_5x8[95 * 8] = {0};
extern const unsigned char VGA_Ascii_6x12[95 * 12] = {0};
extern const unsigned char VGA_Ascii_8x16[95 * 16] = {0};
unsigned int fonts_hzk_start = 0;
unsigned int fonts_hzk_end = 0;

#include "../../System/graphics/UICore.h"

static void test_window_title_copy() {
    UI_Display disp(256, 127, stub_draw);
    UI_Window win(nullptr, nullptr, "Title", &disp, 0, 0, 256, 127);
    win.refreshTitle();
}

static void test_msgbox_reallocates_for_longer_text() {
    UI_Display disp(256, 127, stub_draw);
    UI_Msgbox msgbox(&disp, 16, 32, 224, 64, "Delete File", "Short");
    const char *long_text = "Please wait while the system updates a much longer status message.";
    msgbox.setText(long_text);
    msgbox.refresh();
}

static void test_draw_printf_handles_long_strings() {
    UI_Display disp(256, 127, stub_draw);
    char payload[600];
    std::memset(payload, 'A', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';
    int ret = disp.draw_printf(0, 0, 12, 0, 255, "%s", payload);
    assert(ret == static_cast<int>(std::strlen(payload)));
}

int main() {
    test_window_title_copy();
    test_msgbox_reallocates_for_longer_text();
    test_draw_printf_handles_long_strings();
    return 0;
}
