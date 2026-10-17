#include <assert.h>
#include <stdio.h>
typedef int s32;
#define PORT
typedef struct { s32 chara; } Member;
typedef struct { s32 memberCount, cur; Member member[5]; } Side;
static Side sides[2];
static struct { Side *side[2]; } team = {{&sides[0], &sides[1]}};
#define gTeamSel (&team)
static int allow_duplicates;
int Port_TeamDuplicatesAllowed(void) { return allow_duplicates; }
int ChrTbl_IsRelated(int a, int b) { return a == b; }

/* IMPLEMENTATION */

int main(void) {
    const int ids[] = {0, 79, 109, 128, 198, 326};
    unsigned k;
    int i;
    for (k = 0; k < sizeof(ids) / sizeof(ids[0]); k++) {
        sides[0].memberCount = 5;
        sides[0].cur = 0;
        for (i = 0; i < 5; i++) sides[0].member[i].chara = ids[k];
        allow_duplicates = 1;
        assert(TeamSel_IsCharaFree(0, ids[k]));
        allow_duplicates = 0;
        assert(!TeamSel_IsCharaFree(0, ids[k]));
    }
    sides[0].memberCount = 2;
    sides[0].cur = 1;
    sides[0].member[0].chara = 7;
    sides[0].member[1].chara = 42;
    assert(TeamSel_IsCharaFree(0, 42)); /* replacing the current slot still works online */
    assert(!TeamSel_IsCharaFree(0, 7));
    puts("duplicates_test: repeated local selections accepted; existing restricted/current-slot rules preserved");
    return 0;
}
