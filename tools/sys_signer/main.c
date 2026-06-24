/**
 * @file tools/sys_signer/main.c
 * @brief Entry point and initialization
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libelf/elf32.h"
#include "libelf/elf_user.h"

char path_syself[512];
char path_sysbin[512];

FILE *f_sysbin;
FILE *f_syself;

uint8_t *buf_syself;
uint8_t *buf_sysbin;

size_t sz_syself;
size_t sz_sysbin;

elf_t elf_sys;

static Elf32_Sym *sys_symtab;
static const char *sys_symstr;
static size_t syssym_num;

uint32_t calc_sys_sym_hash() {
    uint32_t hash = 0x5a5a1234;
    uint32_t addr;
    char type;
    char s[1024];

    size_t i = 0;

    while (i < sz_syself) {
        int fr = sscanf((const char *)&buf_syself[i], "%08X %c %1023s", &addr, &type, s);
        if ((fr == 3) && (type >= 'A') && (type <= 'Z')) {
            hash ^= addr;
            hash ^= hash << 16;
        }

        while ((i < sz_syself) && (buf_syself[i] != '\n') && (buf_syself[i] != '\0')) {
            i++;
        }
        if ((i >= sz_syself) || (buf_syself[i] == '\0')) {
            break;
        }
        i++;
    }
    return hash;
    /*
    uint32_t hash = 0x5a5a1234;
    for (int i = 0; i < syssym_num; i++) {
        hash ^= sys_symtab[i].st_value;
        hash ^= hash << 16;
    }
    return hash;*/
}

void Usage() {
    printf("\tUsage:sysign sys_symtab.txt ExistOS.sys\n");
    exit(-1);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        Usage();
    }
    if (snprintf(path_syself, sizeof(path_syself), "%s", argv[1]) >= sizeof(path_syself)) {
        fprintf(stderr, "syself path is too long\n");
        exit(-1);
    }
    if (snprintf(path_sysbin, sizeof(path_sysbin), "%s", argv[2]) >= sizeof(path_sysbin)) {
        fprintf(stderr, "sysbin path is too long\n");
        exit(-1);
    }

    f_syself = fopen(path_syself, "rb");
    if (!f_syself) {
        fprintf(stderr, "Failed to open syself\n");
        exit(-1);
    }
    f_sysbin = fopen(path_sysbin, "rb+");
    if (!f_sysbin) {
        fprintf(stderr, "Failed to open sysbin\n");
        fclose(f_syself);
        exit(-1);
    }

    fseek(f_syself, 0, SEEK_END);
    sz_syself = ftell(f_syself);
    fseek(f_syself, 0, SEEK_SET);

    // fseek(f_sysbin, 0, SEEK_END);
    sz_sysbin = 4 * 32;
    fseek(f_sysbin, 0, SEEK_SET);

    buf_syself = malloc(sz_syself + 1);
    if (!buf_syself) {
        fprintf(stderr, "Failed to alloc memory 1\n");
        exit(-1);
    }

    buf_sysbin = malloc(sz_sysbin);
    if (!buf_sysbin) {
        fprintf(stderr, "Failed to alloc memory 2\n");
        exit(-1);
    }

    if (fread(buf_sysbin, 1, sz_sysbin, f_sysbin) != sz_sysbin) {
        fprintf(stderr, "Failed to read sysbin\n");
        exit(-1);
    }
    if (fread(buf_syself, 1, sz_syself, f_syself) != sz_syself) {
        fprintf(stderr, "Failed to read syself\n");
        exit(-1);
    }
    buf_syself[sz_syself] = '\0';

    if (
        (((uint32_t *)buf_sysbin)[0] != 0xEF5AE0EF) || ((uint32_t *)buf_sysbin)[1] != 0xFECDAFDE) {
        fprintf(stderr, "Error SysBIN Format!\n");
        exit(-1);
    }




    uint32_t hash = calc_sys_sym_hash();
    printf("System Symbol Hash:%08x\n", hash);

    ((uint32_t *)buf_sysbin)[3] = hash;

    fseek(f_sysbin, 0, SEEK_SET);

    fwrite(buf_sysbin, 1, sz_sysbin, f_sysbin);

    fclose(f_sysbin);
    fclose(f_syself);
    free(buf_sysbin);
    free(buf_syself);

    return 0;
}
