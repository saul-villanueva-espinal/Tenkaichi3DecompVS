#ifndef PORT_LIMITS_H
#define PORT_LIMITS_H
/* One build-time setting for the PC roster. Increase this and rebuild to grow
   beyond 290; package IDs, file tables and menu storage derive from it. */
#ifndef PORT_TOTAL_CHARAS
#define PORT_TOTAL_CHARAS 290
#endif
#define PORT_DISC_CHARAS 161
#define PORT_CHARA_FIRST 198
#define PORT_CHARA_MAX (PORT_TOTAL_CHARAS - PORT_DISC_CHARAS)
#define PORT_CHARA_ID_LIMIT (PORT_CHARA_FIRST + PORT_CHARA_MAX)
#define PORT_CHARA_GRID_CAPACITY (((PORT_TOTAL_CHARAS + 2 + 6) / 7) * 7)
#define PORT_CHARA_FILE_INDEX 80000
#define PORT_CHARA_LINE_INDEX (PORT_CHARA_FILE_INDEX + PORT_CHARA_ID_LIMIT * 34 + 4096)
#define PORT_CHARA_FACE_INDEX (PORT_CHARA_LINE_INDEX + PORT_CHARA_MAX * 200 + 4096)
#define PORT_CHARA_MEMBER_INDEX (PORT_CHARA_FACE_INDEX + PORT_CHARA_ID_LIMIT * 8 + 4096)
#define PORT_RELAY_MAX 10
#define PORT_COOP_MAX 6
#if PORT_TOTAL_CHARAS < PORT_DISC_CHARAS || PORT_TOTAL_CHARAS > 1024
#error PC roster capacity must be between 161 and 1024
#endif
#endif
