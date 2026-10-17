/* Exercises the real mode/roster code without disc assets. File paths point to this nonempty fixture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "plat_fighters.h"
#include "plat_charas.h"

static int mode, players = 2, online, missing, last_file;
unsigned gGsFrame;
int Port_Setting(const char *name, int def) {
    if (!strcmp(name, "modes_fight")) return mode;
    if (!strcmp(name, "local_players")) return players;
    return def;
}
void Port_SettingSave(const char *name, int value) {
    if (!strcmp(name, "modes_fight")) mode = value;
    if (!strcmp(name, "local_players")) players = value;
}
void Port_SettingsWrite(void) {}
int Port_NetActive(void) { return online; }
int Port_NetSession(void) { return online; }
int Port_FilePath(int part, int file, const char *name, char *out, int size) {
    (void)part; (void)name;
    last_file = file;
    snprintf(out, size, "%s", __FILE__);
    return !missing;
}
extern int Port_ModesChoosePlayers(int entry, int n);
extern int Port_ModesLeave(void);

static void raid_players(int n) {
    int k;
    assert(Port_ModesChoosePlayers(PORT_MODE_RAID - 1, n) == 40);
    assert(Port_LocalPlayers() == n);
    assert(Port_ExtraSetup(0, 4, 1, n > 1, 0, 0) == PORT_MODE_RAID);
    for (k = 0; k < 3; k++) assert(Port_ExtraAdd(0, k + 1, k, 7, 13));
    Port_ExtraDone();
    for (k = 0; k < 3; k++) {
        assert(Port_FightTeam(k) == 0);
        assert(Port_FightCostume(k) == 7);
        assert(Port_FightPad(k) == (k < n - 1 ? k + 1 : -1));
    }
}
int main(void) {
    int n;
    char why[160];
    Port_LocalPlayersSet(0);
    assert(Port_LocalPlayers() == 1);
    Port_LocalPlayersSet(5);
    assert(Port_LocalPlayers() == 4);
    for (n = 1; n <= 4; n++) raid_players(n);
    mode = PORT_MODE_RAID;
    assert(Port_ExtraSetup(0, 1, 1, 0, 0, 0) == PORT_MODE_RAID); /* solo lead versus boss */
    assert(Port_ExtraSetup(0, 2, 2, 1, 0, 0) == PORT_MODE_RAID);
    assert(Port_ExtraAdd(1, 1, PORT_CHARA_FIRST + PORT_CHARA_MAX - 1, 7, 13));
    assert(Port_FightTeam(0) == 0); /* side 2 reserves join the cooperative team */
    assert(last_file == 0xD48 + PORT_CHARA_FILE_INDEX +
           (PORT_CHARA_FIRST + PORT_CHARA_MAX - 1) * PORT_FILES_PER_CHARA + 7 + 24 - 1);
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
    assert(Port_ExtraSetup(0, 2, 2, 1, 0, 0) == PORT_MODE_TEAM);
    assert(Port_ExtraAdd(1, 1, 1, 3, 13));
    assert(Port_FightTeam(0) == 1);
    mode = PORT_MODE_FFA;
    assert(Port_ExtraSetup(0, 2, 2, 1, 0, 0) == PORT_MODE_FFA);
    assert(Port_ExtraAdd(0, 1, 0, 0, 13));
    assert(Port_ExtraAdd(1, 1, 1, 0, 13));
    assert(Port_FightTeam(0) != Port_FightTeam(1));
    mode = PORT_MODE_RAID;
    assert(!Port_ExtraSetup(0, 4, 1, 1, 1, 0)); /* replay */
    assert(!Port_ExtraSetup(0, 4, 1, 1, 0, 1)); /* scripted fight */
    online = 1;
    assert(Port_ModesFight() == PORT_MODE_NONE);
    assert(!Port_ExtraSetup(0, 4, 1, 1, 0, 0));
    online = 0;
    assert(Port_ModesLeave() == 40);
    assert(Port_ModesFight() == PORT_MODE_NONE);
    puts("modes_test: all roster, controller, costume, package and isolation checks passed");
    return 0;
}
