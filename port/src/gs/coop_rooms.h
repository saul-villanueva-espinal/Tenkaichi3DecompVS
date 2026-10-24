/* Private room service transport. Main co-op protocol remains inside DATA.
   Call only from the game/network thread that owns the socket. */
#ifndef COOP_ROOMS_H
#define COOP_ROOMS_H
#include <stdint.h>
struct sockaddr_in;
const char *RoomRelay_Server(void); /* env BT3_ROOM_SERVER or tbo/online_server.txt */
const char *RoomRelay_Code(void);
const char *RoomRelay_Error(void);
int RoomRelay_Active(void);
int RoomRelay_Slot(void);
int RoomRelay_Socket(void);
void RoomRelay_SetSocket(int sock);
void RoomRelay_GetToken(unsigned char *out);
void RoomRelay_SetToken(const unsigned char *in);
int RoomRelay_Open(int sock, int host, const char *code, uint64_t fingerprint,
                   int protocol, int capacity, const char *name, void (*pump)(void));
/* Menu connection runs DNS/retries on a worker, never inside a graphics frame.
   Poll: 0 pending, 1 connected (*sock), -1 failed, -2 cancelled. */
int RoomRelay_ConnectStart(int host,const char *code,uint64_t fingerprint,
                          int protocol,int capacity,const char *name);
int RoomRelay_ConnectPoll(int *sock);
void RoomRelay_ConnectCancel(void);
/* Preserve membership across the existing process restart. Capability tokens
   stay in the process environment, never in the user-visible invitation. */
int RoomRelay_ExportSession(void);
int RoomRelay_Resume(int sock,uint64_t fingerprint,int protocol,int capacity,void (*pump)(void));
void RoomRelay_Party(const int *slots,int count,int hero,int costume);
int RoomRelay_PeerSlot(int logicalPlayer);
int RoomRelay_LocalHero(int *hero,int *costume);
void RoomRelay_ForgetSession(void);
int RoomRelay_CreateSocket(void);
void RoomRelay_CloseSocket(int sock);
uint64_t RoomRelay_NowMs(void);
void RoomRelay_Address(struct sockaddr_in *address, int slot);
void RoomRelay_Send(int sock, const void *data, int size, const struct sockaddr_in *to);
void RoomRelay_SendSlot(int sock, int targetSlot, const void *data, int size);
/* >0 = inner packet, 0 = no data, -1 = service error. */
int RoomRelay_Receive(int sock, unsigned char *data, struct sockaddr_in *from);
int RoomRelay_RecvSlot(int sock, void *data, int *senderSlot);
void RoomRelay_Close(int sock);
#endif
