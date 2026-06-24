/**
 * @file Tests/host/test_batch1_sys_signer.c
 * @brief test_batch1_sys_signer module
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define main sys_signer_entry
#include "../../tools/sys_signer/main.c"
#undef main

int main(void) {
    const char sample[] = "00000001 T symbol_one\n00000002 T symbol_two";

    sz_syself = sizeof(sample) - 1;
    buf_syself = (uint8_t *)malloc(sz_syself + 1);
    assert(buf_syself != NULL);

    memcpy(buf_syself, sample, sz_syself);
    buf_syself[sz_syself] = '\0';

    uint32_t hash = calc_sys_sym_hash();
    assert(hash != 0);

    free(buf_syself);
    buf_syself = NULL;
    sz_syself = 0;

    return 0;
}
