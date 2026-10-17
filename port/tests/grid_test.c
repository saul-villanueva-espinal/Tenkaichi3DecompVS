/* Fixtures for the real ChrGrid_Build/ChrGrid_BuildPort bodies, inserted by run_grid_test.py. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int s32;
#define PORT
#include "port_chara.h"
#define CHRGRID_COLS 7
#define CHRGRID_FORM_MAX 7
#define CHRGRID_ID_RANDOM 0xA1
#define CHRGRID_ID_LOCKED 0xA2
#define CHRGRID_ID_CUSTOM 0xA3
#define CHRGRID_ID_EMPTY 0xA4
#define SAVE_REC_COUNT 14
#define SAVE_CHARA_WORD(save, id) UINT64_MAX
typedef struct { s32 id, formCount, form[CHRGRID_FORM_MAX]; } ChrGridCell;
static struct { s32 mode, unk624; } progress;
static struct { struct { s32 chara; } rec[SAVE_REC_COUNT]; } save;
#define gProgress (&progress)
#define gSaveData (&save)
static int session;
static int installed = PORT_CHARA_MAX;
int Port_NetSession(void) { return session; }
int Port_CharaCount(void) { return installed; }
int Port_CharaForms(int chara, int *out, int max) {
    assert(max == CHRGRID_FORM_MAX);
    out[0] = chara;
    if (chara == PORT_CHARA_FIRST) { out[1] = chara + 1; out[2] = chara + 2; return 3; }
    if (chara == PORT_CHARA_FIRST + 1 || chara == PORT_CHARA_FIRST + 2) return -1;
    return 1;
}
void Port_CharaCellLog(int cell, int chara, int forms) { (void)cell; (void)chara; (void)forms; }

/* IMPLEMENTATION */

int main(void) {
    struct { ChrGridCell cells[PORT_CHARA_GRID_CAPACITY]; uint32_t guard; } large;
    struct { ChrGridCell cells[165]; uint32_t guard; } legacy;
    ChrGridCell input[97], custom[14];
    int customCount;
    int count, n, i, k;
    progress.mode = 40;
    for (i = 0; i < SAVE_REC_COUNT; i++) save.rec[i].chara = -1;
    memset(input, 0, sizeof(input));
    /* A real grouped master: 33 triples plus 62 single forms and two specials. */
    for (i = 0; i < 33; i++) {
        input[i].id = i * 3;
        input[i].formCount = 3;
        for (k = 0; k < 3; k++) input[i].form[k] = i * 3 + k;
    }
    for (i = 33; i < 95; i++) input[i].id = 99 + i - 33;
    input[95].id = CHRGRID_ID_RANDOM;
    input[96].id = CHRGRID_ID_CUSTOM;
    save.rec[0].chara = 2;
    n = 97;
    large.guard = 0x1234abcd;
    ChrGrid_BuildPort(&count, large.cells, &n, input, &customCount, custom);
    assert(count == 294); /* 161 originals + 129 packages + two specials, padded */
    assert(large.guard == 0x1234abcd);
    assert(customCount == 1 && custom[0].id == 2 && custom[0].formCount == 0);
    for (k = 0; k < 161; k++) {
        int found = 0;
        for (i = 0; i < count; i++) {
            found += large.cells[i].id == k;
            assert(large.cells[i].formCount == 0);
        }
        assert(found == 1);
        assert(ChrGrid_PortSaveIndex(k, -1) == (k < 99 ? k / 3 : 33 + k - 99));
    }
    assert(ChrGrid_PortSaveIndex(PORT_CHARA_FIRST, 1) == -1);
    for (k = 0; k < PORT_CHARA_MAX; k++) {
        int found = 0;
        for (i = 0; i < count; i++) found += large.cells[i].id == PORT_CHARA_FIRST + k;
        assert(found == 1);
    }
    assert(large.cells[count - 1].id == CHRGRID_ID_EMPTY);
    /* A later legacy caller must retain its smaller capacity after the large build. */
    n = 97;
    legacy.guard = 0x5678abcd;
    ChrGrid_Build(&count, legacy.cells, &n, input, NULL, NULL);
    assert(count <= 165 && count % 7 == 0);
    assert(legacy.guard == 0x5678abcd);
    /* The large local grid must not leak package cells into online menus. */
    assert(legacy.cells[0].formCount == 3);
    n = 97;
    session = 1;
    ChrGrid_BuildPort(&count, large.cells, &n, input, NULL, NULL);
    assert(count == 98);
    assert(large.cells[0].formCount == 3);
    assert(ChrGrid_PortSaveIndex(2, 12) == 12);
    for (i = 0; i < count; i++) assert(large.cells[i].id < PORT_CHARA_FIRST);
    assert(large.guard == 0x1234abcd);
    session = 0;
    installed = 8;
    ChrGrid_BuildPort(&count, large.cells, &n, input, NULL, NULL);
    assert(count == 175 && large.guard == 0x1234abcd);
    puts("grid_test: 290 independent characters/forms fit; saved sets, grouped legacy/online, sparse packages and bounds pass");
    return 0;
}
