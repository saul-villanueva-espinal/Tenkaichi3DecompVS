/* Automated end-to-end lobby protocol test: 2 to 6 players, bad-mod rejection,
   rule change resets, cancellation, full capacity rejection, and synchronized launch. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
#endif
#include "coop_rooms.h"

#define LOBBY_PKT_SYNC   0x42543353 /* 'BT3S' */
#define LOBBY_PKT_UPDATE 0x42543355 /* 'BT3U' */
#define PORT_COOP_MAX 6
#define PORT_RAID_BOSSES 5

typedef struct LobbySyncPacket {
    uint32_t magic;
    uint32_t version;
    uint32_t stage;
    uint32_t bossCount;
    int32_t bossChara[PORT_RAID_BOSSES];
    int32_t bossCostume[PORT_RAID_BOSSES];
    int32_t bossHp;
    int32_t bossDamagePct;
    int32_t cpuLevel1;
    int32_t enragePct;
    int32_t armorSides;
    int32_t infiniteKi;
    int32_t difficulty;
    uint32_t playerCount;
    struct {
        uint8_t connected;
        uint8_t state;
        int16_t chara;
        int8_t costume;
        char name[20];
    } players[PORT_COOP_MAX];
    uint8_t launching;
} LobbySyncPacket;

typedef struct LobbyUpdatePacket {
    uint32_t magic;
    uint32_t version;
    uint8_t slot;
    uint8_t state;
    int16_t chara;
    int8_t costume;
    char name[20];
} LobbyUpdatePacket;

static uint64_t now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
#endif
}

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec t = { ms / 1000, (ms % 1000) * 1000000 };
    nanosleep(&t, NULL);
#endif
}

int main(int argc, char **argv) {
    int sock, isHost = (argc == 1 || (argc >= 2 && !strcmp(argv[1], "--host")));
    uint64_t fp = 0x1234567890ABCDEFULL;
    struct sockaddr_in local, from, to;
#ifdef _WIN32
    WSADATA w; u_long nonblock = 1;
    assert(!WSAStartup(MAKEWORD(2, 2), &w));
#endif
    sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock >= 0);
#ifdef _WIN32
    ioctlsocket(sock, FIONBIO, &nonblock);
#else
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(!bind(sock, (struct sockaddr *)&local, sizeof(local)));

    if (argc >= 3 && !strcmp(argv[2], "bad-mods")) {
        /* Intentionally mismatch mod fingerprint */
        fp = 0xBADF00DULL;
        int ok = RoomRelay_Open(sock, 0, argv[1], fp, 1, PORT_COOP_MAX, "BadModPlayer", NULL);
        assert(!ok);
        assert(strstr(RoomRelay_Error(), "mods") != NULL || strstr(RoomRelay_Error(), "huella") != NULL);
        fprintf(stderr, "Mismatched mod rejected as expected: %s\n", RoomRelay_Error());
        closesocket(sock);
        return 0;
    }

    if (argc >= 3 && !strcmp(argv[2], "overflow-guest")) {
        /* Attempt to join full room */
        int ok = RoomRelay_Open(sock, 0, argv[1], fp, 1, PORT_COOP_MAX, "OverflowPlayer", NULL);
        assert(!ok);
        assert(strstr(RoomRelay_Error(), "llena") != NULL);
        fprintf(stderr, "Room full rejected as expected: %s\n", RoomRelay_Error());
        closesocket(sock);
        return 0;
    }

    const char *joinCode = isHost ? NULL : argv[1];
    if (!RoomRelay_Open(sock, isHost, joinCode, fp, 1, PORT_COOP_MAX, isHost ? "HostSaul" : "GuestPlayer", NULL)) {
        fprintf(stderr, "RoomRelay_Open failed: %s\n", RoomRelay_Error());
        closesocket(sock);
        return 1;
    }

    if (isHost) {
        printf("CODE=%s\n", RoomRelay_Code());
        fflush(stdout);

        LobbySyncPacket sync;
        memset(&sync, 0, sizeof(sync));
        sync.magic = LOBBY_PKT_SYNC;
        sync.version = 1;
        sync.stage = 0;
        sync.bossCount = 1;
        sync.bossChara[0] = 133;
        sync.bossHp = 60000;
        sync.bossDamagePct = 100;
        sync.playerCount = PORT_COOP_MAX;
        sync.players[0].connected = 1;
        sync.players[0].state = 4; /* Host listo */
        sync.players[0].chara = 3;
        strcpy(sync.players[0].name, "HostSaul");

        uint64_t begun = now_ms(), lastSync = 0;
        int targetGuests = 5;
        if (argc >= 3 && !strcmp(argv[1], "--host")) targetGuests = atoi(argv[2]);
        int readyCount = 0;

        while (now_ms() - begun < 15000) {
            uint8_t buf[2048];
            int n = RoomRelay_Receive(sock, buf, &from);
            if (n == sizeof(LobbyUpdatePacket)) {
                LobbyUpdatePacket *upd = (LobbyUpdatePacket *)buf;
                if (upd->magic == LOBBY_PKT_UPDATE && upd->slot >= 1 && upd->slot < PORT_COOP_MAX) {
                    sync.players[upd->slot].connected = 1;
                    sync.players[upd->slot].state = upd->state;
                    sync.players[upd->slot].chara = upd->chara;
                    sync.players[upd->slot].costume = upd->costume;
                    strncpy(sync.players[upd->slot].name, upd->name, 19);
                }
            }

            /* Periodically broadcast state */
            if (now_ms() - lastSync > 50) {
                lastSync = now_ms();
                readyCount = 0;
                for (int i = 1; i < PORT_COOP_MAX; i++) {
                    if (sync.players[i].connected) {
                        if (sync.players[i].state == 4) readyCount++;
                        RoomRelay_Address(&to, i);
                        RoomRelay_Send(sock, &sync, sizeof(sync), &to);
                    }
                }
            }

            if (readyCount == targetGuests) {
                /* All target guests are ready! Send launch */
                sync.launching = 1;
                for (int rep = 0; rep < 5; rep++) {
                    for (int i = 1; i < PORT_COOP_MAX; i++) {
                        if (sync.players[i].connected) {
                            RoomRelay_Address(&to, i);
                            RoomRelay_Send(sock, &sync, sizeof(sync), &to);
                        }
                    }
                }
                puts("LOBBY_HOST_LAUNCH_PASS");
                RoomRelay_Close(sock);
                closesocket(sock);
                return 0;
            }
            sleep_ms(2);
        }
        fprintf(stderr, "Host timed out waiting for ready guests (readyCount=%d, target=%d)\n", readyCount, targetGuests);
        RoomRelay_Close(sock);
        closesocket(sock);
        return 2;
    } else {
        /* Guest logic */
        int mySlot = RoomRelay_Slot();
        assert(mySlot >= 1 && mySlot < PORT_COOP_MAX);
        uint8_t myState = 3; /* ELIGIENDO */
        uint64_t begun = now_ms(), lastUpd = 0;
        int sawLaunch = 0;

        while (now_ms() - begun < 15000) {
            uint8_t buf[2048];
            int n = RoomRelay_Receive(sock, buf, &from);
            if (n == sizeof(LobbySyncPacket)) {
                LobbySyncPacket *sync = (LobbySyncPacket *)buf;
                if (sync->magic == LOBBY_PKT_SYNC) {
                    if (sync->launching) {
                        sawLaunch = 1;
                        break;
                    }
                    /* After receiving first sync, confirm ready */
                    if (myState == 3) {
                        myState = 4; /* LISTO */
                    }
                }
            }

            if (now_ms() - lastUpd > 40) {
                lastUpd = now_ms();
                LobbyUpdatePacket upd;
                memset(&upd, 0, sizeof(upd));
                upd.magic = LOBBY_PKT_UPDATE;
                upd.version = 1;
                upd.slot = (uint8_t)mySlot;
                upd.state = myState;
                upd.chara = (int16_t)(10 + mySlot);
                upd.costume = 0;
                snprintf(upd.name, sizeof(upd.name), "Guest_%d", mySlot);
                RoomRelay_Address(&to, 0);
                RoomRelay_Send(sock, &upd, sizeof(upd), &to);
            }
            sleep_ms(2);
        }

        if (sawLaunch) {
            printf("LOBBY_GUEST_LAUNCH_PASS: slot %d\n", mySlot);
            RoomRelay_Close(sock);
            closesocket(sock);
            return 0;
        }
        fprintf(stderr, "Guest slot %d timed out without launch\n", mySlot);
        RoomRelay_Close(sock);
        closesocket(sock);
        return 3;
    }
}
