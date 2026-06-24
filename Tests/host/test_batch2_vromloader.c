/**
 * @file Tests/host/test_batch2_vromloader.c
 * @brief test_batch2_vromloader module
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ff.h"

static void *pvPortMalloc(size_t size) {
    return calloc(1, size);
}

static void vPortFree(void *ptr) {
    free(ptr);
}

static int f_lseek(FIL *f, unsigned int ofs) {
    (void)f;
    (void)ofs;
    return 0;
}

static int f_read(FIL *f, void *buf, unsigned int btr, UINT *br) {
    (void)f;
    memset(buf, 0xA5, btr);
    *br = btr;
    return 0;
}

bool g_system_in_emulator = false;

#include "../../System/core/VROMLoader.c"

static void reset_maps(void) {
    while (vmmap_list) {
        VROMMapInfo_t *next = vmmap_list->next;
        vPortFree(vmmap_list);
        vmmap_list = next;
    }
}

static void test_delete_middle_keeps_tail(void) {
    FIL file = {0};
    assert(VROMLoaderCreateFileMap(&file, 0, 0x1000, 0x100) == 0);
    assert(VROMLoaderCreateFileMap(&file, 0, 0x2000, 0x100) == 0);
    assert(VROMLoaderCreateFileMap(&file, 0, 0x3000, 0x100) == 0);

    assert(findMappedMap(0x1000) != NULL);
    assert(findMappedMap(0x2000) != NULL);
    assert(findMappedMap(0x3000) != NULL);

    assert(VROMLoaderDeleteMap(0x2000) == 0);
    assert(findMappedMap(0x1000) != NULL);
    assert(findMappedMap(0x2000) == NULL);
    assert(findMappedMap(0x3000) != NULL);

    reset_maps();
}

static void test_delete_head_keeps_remaining_list(void) {
    FIL file = {0};
    assert(VROMLoaderCreateFileMap(&file, 0, 0x1000, 0x100) == 0);
    assert(VROMLoaderCreateFileMap(&file, 0, 0x2000, 0x100) == 0);

    assert(VROMLoaderDeleteMap(0x1000) == 0);
    assert(findMappedMap(0x1000) == NULL);
    assert(findMappedMap(0x2000) != NULL);

    reset_maps();
}

static void test_overlap_check_hits_last_node(void) {
    FIL file = {0};
    assert(VROMLoaderCreateFileMap(&file, 0, 0x4000, 0x200) == 0);
    assert(VROMLoaderCreateFileMap(&file, 0, 0x5000, 0x200) == 0);

    assert(VROMMapCheck(0x5000, 0x20) == 1);
    assert(VROMMapCheck(0x4F00, 0x20) == 0);

    reset_maps();
}

int main(void) {
    test_delete_middle_keeps_tail();
    test_delete_head_keeps_remaining_list();
    test_overlap_check_hits_last_node();
    return 0;
}
