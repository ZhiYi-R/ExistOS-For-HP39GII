#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int dummy;
} lv_obj_t;

static char textbuf[128];

#include "../../System/filesystem/littlefs/lfs.h"

static uint8_t fake_flash_page[2048];
static uint32_t read_block_seen;
static uint32_t read_pages_seen;
static void *read_buffer_seen;
static uint32_t write_block_seen;
static uint32_t write_pages_seen;
static const void *write_buffer_seen;
static int flash_sync_calls;
static int mount_calls;
static int format_calls;
static int mount_return_values[4];
static int mount_return_count;
static int mount_return_index;
static int format_return_value;

static void *pvPortMalloc(size_t size) {
    return malloc(size);
}

static void vPortFree(void *ptr) {
    free(ptr);
}

static int SystemUIMsgBox(char *msg, char *title, uint32_t button) {
    (void)msg;
    (void)title;
    (void)button;
    return 0;
}

static lv_obj_t *lv_scr_act(void) {
    return NULL;
}

static lv_obj_t *lv_spinner_create(lv_obj_t *parent, uint32_t time, uint32_t arc_length) {
    (void)parent;
    (void)time;
    (void)arc_length;
    return NULL;
}

static void lv_obj_set_size(lv_obj_t *obj, int32_t w, int32_t h) {
    (void)obj;
    (void)w;
    (void)h;
}

static void lv_obj_center(lv_obj_t *obj) {
    (void)obj;
}

static int ll_flash_page_read(uint32_t start_page, uint32_t pages, void *buffer) {
    read_block_seen = start_page;
    read_pages_seen = pages;
    read_buffer_seen = buffer;
    memcpy(buffer, fake_flash_page, sizeof(fake_flash_page));
    return 0;
}

static int ll_flash_page_write(uint32_t start_page, uint32_t pages, char *buffer) {
    write_block_seen = start_page;
    write_pages_seen = pages;
    write_buffer_seen = buffer;
    memcpy(fake_flash_page, buffer, sizeof(fake_flash_page));
    return 0;
}

static void ll_flash_page_trim(uint32_t page) {
    (void)page;
}

static void ll_flash_sync(void) {
    flash_sync_calls++;
}

static uint32_t ll_flash_get_page_size(void) {
    return 2048;
}

static uint32_t ll_flash_get_pages(void) {
    return 128;
}

static int lfs_mount(lfs_t *lfs, const struct lfs_config *config) {
    (void)lfs;
    (void)config;
    mount_calls++;
    if (mount_return_index < mount_return_count) {
        return mount_return_values[mount_return_index++];
    }
    return 0;
}

static int lfs_format(lfs_t *lfs, const struct lfs_config *config) {
    (void)lfs;
    (void)config;
    format_calls++;
    return format_return_value;
}

#include "../../System/drivers/SystemFs.c"

static void reset_state(void) {
    memset(fake_flash_page, 0, sizeof(fake_flash_page));
    read_block_seen = 0;
    read_pages_seen = 0;
    read_buffer_seen = NULL;
    write_block_seen = 0;
    write_pages_seen = 0;
    write_buffer_seen = NULL;
    flash_sync_calls = 0;
    mount_calls = 0;
    format_calls = 0;
    mount_return_count = 0;
    mount_return_index = 0;
    format_return_value = 0;
}

static void test_read_uses_offset_and_size(void) {
    reset_state();
    for (size_t i = 0; i < sizeof(fake_flash_page); ++i) {
        fake_flash_page[i] = (uint8_t)(i & 0xFF);
    }

    uint8_t out[16];
    memset(out, 0, sizeof(out));

    assert(EVM_Flash_Read(&lfs_cfg, 7, 32, out, sizeof(out)) == 0);
    assert(read_block_seen == 7);
    assert(read_pages_seen == 1);
    assert(read_buffer_seen == read_buf);
    assert(memcmp(out, &fake_flash_page[32], sizeof(out)) == 0);
}

static void test_prog_preserves_other_bytes_and_honors_offset(void) {
    reset_state();
    memset(fake_flash_page, 0xEE, sizeof(fake_flash_page));

    uint8_t payload[8];
    memset(payload, 0x5A, sizeof(payload));

    assert(EVM_Flash_Prog(&lfs_cfg, 9, 64, payload, sizeof(payload)) == 0);
    assert(write_block_seen == 9);
    assert(write_pages_seen == 1);
    assert(write_buffer_seen == write_buf);
    assert(memcmp(&fake_flash_page[64], payload, sizeof(payload)) == 0);
    assert(fake_flash_page[63] == 0xEE);
    assert(fake_flash_page[72] == 0xEE);
}

static void test_mount_reports_format_result_not_stale_mount_error(void) {
    reset_state();
    mount_return_values[0] = -5;
    mount_return_values[1] = 0;
    mount_return_count = 2;
    format_return_value = 0;

    SystemFSInit();

    assert(mount_calls == 2);
    assert(format_calls == 1);
}

int main(void) {
    test_read_uses_offset_and_size();
    test_prog_preserves_other_bytes_and_honors_offset();
    test_mount_reports_format_result_not_stale_mount_error();
    return 0;
}
