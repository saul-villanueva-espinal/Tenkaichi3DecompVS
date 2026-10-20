/* Exercises the real mode/roster code without disc assets. File paths point to this nonempty fixture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "plat_fighters.h"
#include "plat_charas.h"
typedef int s32;
#define PORT
#include "../../include/port_chara.h"

static int coop;
static int mode, players = 2, online, session, missing, last_file, extras, mode_extras;
unsigned gGsFrame;
int Port_Setting(const char *name, int def) {
    if (!strcmp(name, "modes_fight")) return mode;
    if (!strcmp(name, "local_players")) return players;
    if (!strcmp(name, "extra_fighters")) return extras;
    if (!strcmp(name, "modes_extras")) return mode_extras;
    return def;
}
void Port_SettingSave(const char *name, int value) {
    if (!strcmp(name, "modes_fight")) mode = value;
    if (!strcmp(name, "local_players")) players = value;
    if (!strcmp(name, "modes_extras")) mode_extras = value;
}
void Port_SettingsWrite(void) {}
int Port_NetActive(void) { return online; }
int Port_NetSession(void) { return session; }
int Port_CoopEnabled(void) { return coop; }
int Port_FilePath(int part, int file, const char *name, char *out, int size) {
    (void)name;
    assert(part == 2); /* match File_OpenById's global-to-partition translation */
    last_file = file;
    snprintf(out, size, "%s", __FILE__);
    return !missing;
}
extern int Port_ModesChoosePlayers(int entry, int n);
extern int Port_ModesLeave(void);

static void raid_players(int n) {
    int k;
    assert(Port_ModesChoosePlayers(PORT_MODE_RAID - 1, n) == 80);
    assert(Port_LocalPlayers() == n);mode=PORT_MODE_RAID;
    assert(Port_ExtraSetup(0, 4, 1, n > 1, 0, 0) == PORT_MODE_RAID);
    for (k = 0; k < 3; k++) assert(Port_ExtraAdd(0, k + 1, k, 7, 13));
    Port_ExtraDone();
    for (k = 0; k < 3; k++) {
        assert(Port_FightTeam(k) == 0);
        assert(Port_FightCostume(k) == 7);
        assert(Port_FightPad(k) == (k < n - 1 ? k + 1 : -1));
    }
}
static void raid_online(int n) {
    int k;coop=1;mode=PORT_MODE_RAID;Port_LocalPlayersSet(n);
    assert(Port_LocalPlayers()==n);
    assert(!Port_LocalQuadFight(0,5,1,0,0));
    assert(Port_ExtraSetup(0,5,1,0,0,0)==PORT_MODE_RAID);
    for(k=0;k<4;k++) assert(Port_ExtraAdd(0,k+1,k,7,13));
    if(n==6) assert(Port_ExtraRaidAdd(91,1,13));
    Port_ExtraDone();assert(Port_FightExtras()==(n==6?5:4));
    for(k=0;k<Port_FightExtras();k++) {
        assert(Port_FightPad(k)==(k<n-1?k+1:-1));
        assert(Port_FightTeam(k)==0);
    }
    if(n==6) {assert(Port_FightChara(4)==91);assert(Port_FightCostume(4)==1);assert(Port_FightMember(4)==-1);}
    coop=0;Port_LocalPlayersSet(2);
}
int main(void) {
    int n;
    char why[160];
    raid_online(4);raid_online(5);raid_online(6);
    coop=1;mode=PORT_MODE_RAID;Port_LocalPlayersSet(6);
    assert(Port_ExtraSetup(0,5,5,0,0,0)==PORT_MODE_RAID);
    for(n=1;n<5;n++) assert(Port_ExtraAdd(0,n,n,0,13));
    assert(Port_ExtraRaidAdd(91,1,13));
    for(n=1;n<5;n++) assert(Port_ExtraAdd(1,n,130+n,0,29));
    Port_ExtraDone();assert(Port_FightExtras()==9);
    for(n=0;n<9;n++) {assert(Port_FightTeam(n)==(n>=5));assert(Port_FightPad(n)==(n<5?n+1:-1));}
    coop=0;Port_LocalPlayersSet(2);
    /* A saved training experiment must never add fighters to ordinary Duel. */
    mode = PORT_MODE_NONE;
    extras = 1;
    assert(Port_TeamDuplicatesAllowed());
    assert(!Port_ExtraSetup(0, 1, 1, 0, 0, 0));
    assert(Port_FightExtras() == 0);
    assert(!Port_ExtraSetup(0, 5, 5, 1, 0, 0));
    assert(Port_FightExtras() == 0);
    assert(!Port_ExtraSetup(6, 1, 1, 0, 0, 0));
    assert(Port_FightExtras() == 1); /* the experiment remains available in Training */
    extras = 0;
    Port_LocalPlayersSet(0);
    assert(Port_LocalPlayers() == 1);
    Port_LocalPlayersSet(5);
    assert(Port_LocalPlayers() == 4);
    for (n = 1; n <= 4; n++) raid_players(n);
    mode = PORT_MODE_RAID;
    assert(Port_ExtraSetup(0, 1, 1, 0, 0, 0) == PORT_MODE_RAID); /* solo lead versus boss */
    assert(Port_ExtraSetup(0, 2, 2, 1, 0, 0) == PORT_MODE_RAID);
    assert(Port_ExtraAdd(1, 1, PORT_CHARA_FIRST + PORT_CHARA_MAX - 1, 7, 13));
    assert(Port_FightTeam(0) == 1); /* opposing reserves remain CPU bosses */
    assert(last_file == PORT_CHARA_FILE_INDEX +
           (PORT_CHARA_FIRST + PORT_CHARA_MAX - 1) * PORT_FILES_PER_CHARA + 7 + 24);
    for (n = 0; n < PORT_CHARA_MAX; n++) {
        int id = PORT_CHARA_FIRST + n;
        assert(CHARA_FROM_FILE(CHARA_MODEL_FILE(id, 7)) == id);
        assert(CHARA_FROM_FILE(CHARA_ANM_FILE(id, 7)) == id);
        assert(CHARA_FROM_FILE(CHARA_EFF_FILE(id, 7)) == id);
    }
    assert(!Port_ExtraCharaOk(161, why, sizeof(why))); /* menu marker is not a fighter */
    assert(!Port_ExtraCharaOk(PORT_CHARA_FIRST + PORT_CHARA_MAX, why, sizeof(why)));
    missing = 1;
    assert(!Port_ExtraAdd(0, 1, 0, 0, 13));
    missing = 0;
    assert(Port_ExtraSetup(0, 5, 5, 1, 0, 0) == PORT_MODE_RAID);
    for (n = 0; n < PORT_EXTRA_MAX; n++) assert(Port_ExtraAdd(n / 4, n % 4 + 1, n, n % 8, 13));
    assert(Port_FightExtras() == PORT_EXTRA_MAX);
    assert(!Port_ExtraAdd(0, 1, 0, 0, 13));
    mode = PORT_MODE_TEAM;
    assert(Port_ExtraSetup(0, 2, 3, 1, 0, 0) == PORT_MODE_TEAM);
    assert(Port_ExtraAdd(1, 1, 0, 3, 13));
    assert(Port_ExtraAdd(1, 2, 0, 7, 13));
    assert(Port_FightTeam(0) == 1);
    assert(Port_FightChara(0) == Port_FightChara(1));
    assert(Port_FightCostume(0) == 3 && Port_FightCostume(1) == 7);
    mode = PORT_MODE_FFA;
    assert(Port_ExtraSetup(0, 2, 2, 1, 0, 0) == PORT_MODE_FFA);
    assert(Port_ExtraAdd(0, 1, 0, 0, 13));
    assert(Port_ExtraAdd(1, 1, 1, 0, 13));
    assert(Port_FightTeam(0) != Port_FightTeam(1));
    mode = PORT_MODE_RAID;
    assert(!Port_ExtraSetup(0, 4, 1, 1, 1, 0)); /* replay */
    assert(!Port_ExtraSetup(0, 4, 1, 1, 0, 1)); /* scripted fight */
    online = 1;
    assert(!Port_TeamDuplicatesAllowed());
    assert(Port_ModesFight() == PORT_MODE_NONE);
    assert(!Port_ExtraSetup(0, 4, 1, 1, 0, 0));
    online = 0;
    session = 1;
    assert(!Port_TeamDuplicatesAllowed());
    assert(!Port_ExtraSetup(6, 1, 1, 0, 0, 0));
    assert(Port_FightExtras() == 0);
    session = 0;
    assert(Port_ModesLeave() == 80);
    assert(Port_ModesFight() == PORT_MODE_NONE);
    puts("modes_test: all roster, controller, costume, package and isolation checks passed");
    return 0;
}
