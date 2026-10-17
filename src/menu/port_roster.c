#include "common.h"
#ifdef PORT
#include "menu/menu_e.h"
#include "menu/menu_f.h"
#include "menu/port_screen.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "sys/load_sprite.h"
#include "port_pc_battle.h"
#include "port_coop.h"
#include "port_fighters.h"
#include "port_chara.h"
#include "plat_songs.h"

/* Sparking! ZERO Inspired Modern Native Roster Interface.
   Every panel, portrait and tile is submitted through the game's sprite/DMA path. */
extern s32 Port_ModesFight(void), Port_LocalPlayers(void), Port_NetActive(void), Port_NetSession(void);
extern s32 Port_ExtraCostumeOk(s32,s32,char *,s32), ChrTbl_GetCost(s32);
extern void Port_LocalPlayersSet(s32);
extern void Port_SettingSave(const char *,s32);
extern void Port_RosterLog(s32,s32,s32,s32,s32);
extern void Port_RosterName(char *,s32,s32);
extern void Port_PcNativeRules(s32,s32,s32);
extern void TeamSel_Init(s32), TeamSel_Term(void), TeamSel_UpdateFaceLoad(void), TeamSel_UpdateStageLoad(void);
extern void TeamSel_RequestFace(s32), TeamSel_RequestStage(void);
extern void Sprite_DrawList(void *,s32,s32,LoadSprite *);
extern f32 sinf(f32),cosf(f32);
extern void Ps_PageSet(void), Font_FlushAll(void);

#define NR_COLS 5
#define NR_ROWS 6
#define NR_PAGE 30
#define NR_CX 16
#define NR_CY 72
#define NR_CARD_W 37
#define NR_CARD_H 44
#define NR_DX 40
#define NR_DY 46

enum {
    NR_SAGA_ALL = 0,
    NR_SAGA_Z,
    NR_SAGA_SUPER,
    NR_SAGA_GT,
    NR_SAGA_MOVIES,
    NR_SAGA_CLASSIC,
    NR_SAGA_FAV,
    NR_SAGA_COUNT
};

static const char *sSagaTitles[NR_SAGA_COUNT] = {
    "TODOS LOS PERSONAJES",
    "DRAGON BALL Z",
    "DRAGON BALL SUPER",
    "DRAGON BALL GT",
    "PEL\303\215CULAS Y ESPECIALES",
    "DRAGON BALL CL\303\201SICO",
    "PERSONAJES FAVORITOS"
};

static const char *sSagaTabs[NR_SAGA_COUNT] = {
    "TODOS", "Z", "SUPER", "GT", "PEL\303\215C.", "CL\303\201S.", "\342\230\205"
};

typedef struct {
    s32 id;
    const char *b1_1; s32 c1_1;
    const char *b1_2; s32 c1_2;
    const char *b2_1; s32 k2_1;
    const char *b2_2; s32 k2_2;
    const char *ult;
} NrTechData;

static const NrTechData sTechTable[] = {
    {0, "Sentido Salvaje", 2, "Kaio-ken", 2, "Kamehameha", 3, "Golpe Kaio-ken", 3, "Genkidama"},
    {1, "Sentido Salvaje", 2, "Dar Poder", 1, "Kamehameha x20", 3, "Meteoro Combinado", 3, "Super Genkidama"},
    {2, "Sentido Salvaje", 2, "Carga Instantanea", 1, "Super Kamehameha", 3, "Asalto Furioso", 3, "Genkidama Furiosa"},
    {3, "Sentido Salvaje", 2, "Carga Rapida", 1, "Kamehameha", 3, "Golpe Meteoro", 3, "Super Genkidama"},
    {4, "Sentido Salvaje", 2, "Teletransporte", 2, "Super Kamehameha", 3, "Rafaga Meteoro", 3, "Ataque Meteoro"},
    {5, "Sentido Salvaje", 2, "Potencia Ki", 1, "Kamehameha", 3, "Super Onda", 3, "Kamehameha Instantaneo"},
    {6, "Sentido Salvaje", 2, "Poder Maximo", 2, "Super Kamehameha", 3, "Impacto Dragon", 4, "Puno del Dragon"},
    {7, "Sentido Salvaje", 2, "Carga Ki", 1, "Kamehameha", 3, "Golpe Dragon", 3, "Genkidama Universal"},
    {8, "Sentido Salvaje", 2, "Aumento de Ki", 1, "Super Kamehameha", 3, "Meteoro Giratorio", 3, "Super Genkidama"},
    {9, "Sentido Salvaje", 2, "Potencia Ki", 1, "Kamehameha x10", 3, "Golpe Meteoro", 3, "Puno del Dragon"},
    {10, "Sentido Salvaje", 2, "Poder Maximo", 2, "Kamehameha x10", 3, "Meteoro Dragon", 4, "Puno Dragon x10"},
    {11, "Espejismo", 1, "Poder Oculto", 1, "Kamehameha", 3, "Asalto Feroz", 3, "Puno de Ozaru"},
    {12, "Rugido", 1, "Aliento Igneo", 1, "Super Choque", 3, "Onda Destructora", 3, "Pisoton Gigante"},
    {13, "Sentido Salvaje", 2, "Ira Oculta", 1, "Masenko", 3, "Ataque Meteoro", 3, "Asalto Oculto"},
    {14, "Sentido Salvaje", 2, "Ira Ki", 1, "Masenko", 3, "Super Ataque", 3, "Super Kamehameha"},
    {15, "Sentido Salvaje", 2, "Poder Silencioso", 1, "Super Masenko", 3, "Golpe Feroz", 3, "Super Kamehameha"},
    {16, "Sentido Salvaje", 2, "Ira Desatada", 1, "Super Kamehameha", 3, "Choque Feroz", 3, "Kamehameha Padre e Hijo"},
    {17, "Sentido Salvaje", 2, "Carga Ki", 1, "Masenko", 3, "Kamehameha", 3, "Super Masenko"},
    {18, "Sentido Salvaje", 2, "Potencia Saiyan", 1, "Super Kamehameha", 3, "Rafaga Meteoro", 3, "Kamehameha Feroz"},
    {19, "Sentido Salvaje", 2, "Poder Maximo", 1, "Super Kamehameha", 3, "Choque Final", 3, "Kamehameha Feroz"},
    {20, "Pose Justiciera", 1, "Justicia", 1, "Rafaga Justiciera", 3, "Patada Saiyaman", 3, "Puno de la Justicia"},
    {21, "Sentido Salvaje", 2, "Poder Desbloqueado", 1, "Super Kamehameha", 3, "Rafaga Feroz", 3, "Kamehameha Definitivo"},
    {22, "Sentido Salvaje", 2, "Regeneracion", 2, "Makankosappo", 3, "Onda Explosiva", 3, "Makankosappo Feroz"},
    {23, "Sentido Salvaje", 2, "Regeneracion", 2, "Makankosappo", 3, "Granada Infernal", 4, "Makankosappo Maximo"},
    {24, "Sentido Salvaje", 2, "Regeneracion", 2, "Rafaga Namekiana", 3, "Golpe Feroz", 3, "Castigo de Dragon"},
    {25, "Destello Solar", 1, "Zanzoken", 1, "Kienzan", 3, "Kamehameha", 3, "Kienzan Multiple"},
    {26, "Zanzoken", 1, "Espiritu Feroz", 1, "Golpe Colmillo Lobo", 3, "Kamehameha", 3, "Sokidan"},
    {27, "Destello Solar", 1, "Zanzoken", 1, "Dodonpa", 2, "Kikoho", 3, "Shin Kikoho"},
    {28, "Paralisis", 2, "Zanzoken", 1, "Dodonpa", 2, "Psicoquinesia", 3, "Autodestruccion"},
    {29, "Sentido Salvaje", 2, "Orgullo Saiyan", 1, "Canon Galick", 3, "Disparo Dedo", 3, "Canon Rompedor"},
    {30, "Rugido", 1, "Furia Saiyan", 1, "Super Canon", 3, "Rafaga Gigante", 3, "Aplastamiento Fatal"},
    {31, "Sentido Salvaje", 2, "Orgullo Saiyan", 1, "Canon Galick", 3, "Ataque Big Bang", 3, "Final Flash"},
    {32, "Sentido Salvaje", 2, "Orgullo Saiyan", 1, "Big Bang Attack", 3, "Destello Galick", 3, "Final Flash"},
    {33, "Sentido Salvaje", 2, "Orgullo Maximo", 1, "Big Bang Attack", 3, "Bombardero Foton", 3, "Super Final Flash"},
    {34, "Sentido Salvaje", 2, "Carga Ki", 1, "Canon Galick", 3, "Ataque Big Bang", 3, "Final Flash"},
    {35, "Sentido Salvaje", 2, "Potencia Saiyan", 1, "Big Bang Attack", 3, "Destello Final", 3, "Final Flash Feroz"},
    {36, "Sentido Salvaje", 2, "Orgullo Maximo", 1, "Big Bang Attack", 3, "Impacto Infinito", 3, "Super Final Flash"},
    {37, "Sentido Salvaje", 2, "Control Babidi", 1, "Impacto Atomico", 3, "Canon Final", 3, "Explosion Final"},
    {38, "Sentido Salvaje", 2, "Poder Maximo", 2, "Final Shine Attack", 3, "Meteoro Galick", 4, "Final Shine Maximo"},
    {39, "Sentido Salvaje", 2, "Desenvainar", 1, "Ataque Ardiente", 3, "Corte Feroz", 3, "Cupula de Calor"},
    {40, "Sentido Salvaje", 2, "Espada Mortal", 1, "Ataque Ardiente", 3, "Corte Feroz", 3, "Cupula de Calor"},
    {41, "Sentido Salvaje", 2, "Carga Ki", 1, "Canon Buster", 3, "Rafaga Meteoro", 3, "Cupula de Calor"},
    {42, "Sentido Salvaje", 2, "Potencia Saiyan", 1, "Canon Buster", 3, "Ataque Ardiente", 3, "Cupula de Calor"},
    {43, "Sentido Salvaje", 2, "Musculo Maximo", 1, "Rompedor Feroz", 3, "Onda Destructora", 3, "Super Cupula de Calor"},
    {44, "Sentido Salvaje", 1, "Finta Saiyan", 1, "Canon Flash", 3, "Ataque Meteoro", 3, "Canon Flash Doble"},
    {45, "Sentido Salvaje", 1, "Potencia Ki", 1, "Canon Flash", 3, "Rafaga Ardiente", 3, "Super Canon Flash"},
    {46, "Sentido Salvaje", 1, "Finta", 1, "Kamehameha", 3, "Golpe Meteoro", 3, "Super Kamehameha"},
    {47, "Sentido Salvaje", 1, "Potencia Ki", 1, "Kamehameha", 3, "Asalto Feroz", 3, "Super Kamehameha"},
    {48, "Zanzoken", 1, "Burlarse", 1, "Ataque Kamikaze", 3, "Misil Fantasma", 3, "Ultra Volleyball"},
    {49, "Zanzoken", 1, "Poder Saiyan", 1, "Super Kamikaze", 3, "Misiles de Ki", 3, "Ultra Buu Buu Volleyball"},
    {50, "Zanzoken", 1, "Poder Divino", 1, "Super Kamikaze 10x", 4, "Misiles Fantasma", 3, "Super Volleyball Ultra"},
    {51, "Sentido Salvaje", 2, "Burlarse", 1, "Espada de Espiritu", 3, "Disparo Big Bang", 3, "Espada de Espiritu Final"},
    {52, "Sentido Salvaje", 2, "Espiritu Guerrero", 1, "Espada de Espiritu", 3, "Canon Big Bang", 3, "Espada Final Destructora"},
    {53, "Sentido Salvaje", 2, "Poder Divino", 1, "Big Bang Kamehameha", 3, "Castigador de Almas", 4, "Polvo Estelar"},
    {54, "Sentido Salvaje", 2, "Poder Definitivo", 1, "Big Bang Kamehameha x100", 4, "Golpe Flash", 3, "Big Bang Ultra"},
    {55, "Finta", 1, "Regalo Especial", 1, "Puno Dinamita", 2, "Patada Satan", 2, "Ataque Milagro"},
    {56, "Zanzoken", 1, "Espiritu", 1, "Patada Aguila", 2, "Furia Videl", 3, "Justicia de Videl"},
    {57, "Pose Justiciera", 1, "Justicia", 1, "Rafaga Rompedora", 3, "Patada Justiciera", 3, "Puno de Paz"},
    {58, "Teletransporte", 2, "Poder Divino", 1, "Onda Espiritual", 3, "Rafaga Sagrada", 3, "Fuerza Divina"},
    {59, "Teletransporte", 2, "Curacion Sagrada", 2, "Onda Celestial", 3, "Rafaga de Dios", 3, "Juicio Sagrado"},
    {60, "Curacion Senzu", 3, "Finta Feroz", 1, "Tajo de Espada", 3, "Ataque Furtivo", 3, "Corte Maestro"},
    {61, "Sentido Salvaje", 2, "Hiper Tornado", 1, "Rafaga Ignea", 3, "Disparo Feroz", 3, "Trueno Ardiente"},
    {62, "Melodia Magica", 2, "Heroe Valiente", 1, "Tajo Valiente", 3, "Canon Heroe", 3, "Espada Valiente"},
    {63, "Finta", 1, "Espiritu Infantil", 1, "Kamehameha", 3, "Patada Doncella", 3, "Furia Doncella"},
    {64, "Sentido Salvaje", 2, "Poder Oculto", 1, "Kamehameha", 3, "Rafaga Feroz", 3, "Canon de Energia"},
    {65, "Sentido Salvaje", 2, "Rayo Dulce", 2, "Kamehameha Chocolate", 3, "Rafaga Majin", 3, "Furia Mistica"},
    {66, "Zanzoken", 1, "Poder Sabio", 1, "Kamehameha", 3, "Onda del Trueno", 3, "Kamehameha Original"},
    {67, "Sentido Salvaje", 2, "Poder Maximo", 1, "Kamehameha Maximo", 3, "Impacto Muscular", 3, "Kamehameha Destructor"},
    {68, "Zanzoken", 1, "Poder Antiguo", 1, "Kamehameha", 3, "Golpe Sabio", 3, "Asalto Cuatro Manos"},
    {71, "Sentido Salvaje", 2, "Orgullo Real", 1, "Canon Real", 3, "Disparo Verdugo", 3, "Onda Rompedora"},
    {73, "Sentido Salvaje", 2, "Espiritu Saiyan", 1, "Gatillo Rebelde", 3, "Lanza Rebelde", 3, "Vengador Final"},
    {77, "Sentido Salvaje", 1, "Aullido Saiyan", 1, "Onda Doble", 3, "Lunes Sabado", 3, "Bombardero Letal"},
    {80, "Sentido Salvaje", 1, "Bomba Gigante", 1, "Tormenta Feroz", 3, "Canon Rompedor", 3, "Aplastamiento Letal"},
    {82, "Sentido Salvaje", 1, "Elegancia", 1, "Disparo Elegante", 3, "Rafaga Feroz", 3, "Bala Monstruosa"},
    {84, "Sentido Salvaje", 1, "Furia Bruta", 1, "Canon Dodoria", 3, "Cabezazo Feroz", 3, "Bombardero Fatal"},
    {86, "Pose Especial", 1, "Zanzoken", 1, "Rafaga Ginyu", 3, "Dinamita Galactica", 3, "Cambio de Cuerpo"},
    {91, "Sentido Salvaje", 2, "Psicoquinesia", 1, "Rayo de la Muerte", 3, "Bola de la Muerte", 4, "Supernova"},
    {94, "Sentido Salvaje", 2, "Psicoquinesia", 1, "Rayo de la Muerte", 3, "Cometa Muerte", 3, "Supernova"},
    {95, "Sentido Salvaje", 2, "Poder 100%", 1, "Platillo Letal", 3, "Golpe Nova", 4, "Supernova Destructora"},
    {96, "Sentido Salvaje", 2, "Armadura Ciber", 1, "Rayo Cibernetico", 3, "Supernova", 4, "Rayo Pesadilla"},
    {97, "Sentido Salvaje", 2, "Comando Real", 1, "Rayo Real", 3, "Canon Feroz", 3, "Supernova Real"},
    {100, "Sentido Salvaje", 2, "Ojo Mecanico", 1, "Canon Cohete", 3, "Flash Infernal", 4, "Bombardero Infernal"},
    {101, "Barrera Androide", 2, "Energia Infinita", 1, "Rafaga Androide", 3, "Canon Poderoso", 3, "Danza Sadica"},
    {102, "Barrera Androide", 2, "Energia Infinita", 1, "Destello Destructor", 3, "Rafaga Rosa", 3, "Danza de la Muerte"},
    {105, "Sentido Salvaje", 2, "Absorcion", 2, "Kamehameha", 3, "Choque Feroz", 3, "Kamehameha Drenaje"},
    {107, "Sentido Salvaje", 2, "Barrera Perfecta", 2, "Kamehameha", 3, "Choque Perfecto", 3, "Kamehameha Solar"},
    {108, "Sentido Salvaje", 2, "Regeneracion", 2, "Kamehameha Solar", 4, "Makankosappo", 3, "Kamehameha Destructor"},
    {112, "Sentido Salvaje", 2, "Aliento Dulce", 2, "Kamehameha", 3, "Canon Inocente", 3, "Ataque Furioso"},
    {114, "Sentido Salvaje", 2, "Absorcion", 2, "Canon Vice", 3, "Rafaga Venganza", 3, "Asalto Humanidad"},
    {117, "Sentido Salvaje", 2, "Regeneracion", 2, "Kamehameha", 3, "Impacto Chiflado", 3, "Bola Desvanecedora"},
    {118, "Inmortalidad", 2, "Furia Demoniaca", 1, "Rafaga Oscuridad", 3, "Impacto Feroz", 3, "Zona Muerta"},
    {121, "Fruta del Arbol", 2, "Sentido Salvaje", 1, "Destello Letal", 3, "Golpe Feroz", 3, "Meteoro Muerte"},
    {123, "Regeneracion", 2, "Gigantismo", 1, "Canon Letal", 3, "Rafaga Gigante", 3, "Oscuridad Letal"},
    {126, "Sentido Salvaje", 2, "Psicoquinesia", 1, "Rayo de la Muerte", 3, "Lluvia de la Muerte", 3, "Supernova"},
    {127, "Sentido Salvaje", 2, "Armadura Ki", 1, "Rayo Destructor", 3, "Supernova", 4, "Supernova Cosmica"},
    {128, "Regeneracion Instantanea", 2, "Teletransporte", 2, "Rayo Metalico", 3, "Lluvia de Nucleos", 3, "Supernova Metalica"},
    {129, "Sentido Salvaje", 1, "Radar Androide", 1, "Bomba SS Letal", 3, "Rafaga Letal", 3, "Impacto SS Letal"},
    {131, "Sentido Salvaje", 2, "Kiai Demoledor", 1, "Canon Borrador", 3, "Choque Meteoro", 3, "Omega Blaster"},
    {133, "Sentido Salvaje", 2, "Kiai Demoledor", 1, "Canon Borrador", 3, "Omega Blaster", 4, "Meteoro Gigantesco"},
    {135, "Sentido Salvaje", 2, "Poder Tirano", 1, "Disparo Trampa", 3, "Buster Tirano", 3, "Rompedor Galactico"},
    {138, "Sentido Salvaje", 2, "Desmaterializacion", 2, "Espada Dimensional", 3, "Disparo Espectral", 3, "Rayo Fantasma"},
    {139, "Desmaterializacion", 2, "Rugido Furioso", 1, "Llamarada Espectral", 3, "Cola Letal", 3, "Llamarada Apocaliptica"},
    {140, "Sentido Salvaje", 2, "Posesion Tuffle", 1, "Canon Big Bang", 3, "Destello Vengador", 3, "Bola del Rencor"},
    {144, "Absorcion Ki", 2, "Flash Bomber", 1, "Rafaga de Rayos", 3, "Bomba Infierno", 4, "Bola Infernal Electro"},
    {147, "Poder Dragon Oscuro", 2, "Viento Letal", 1, "Rafaga Espinas", 3, "Onda Dragon", 3, "Bola Energia Negativa"},
    {148, "Zanzoken", 1, "Poder Asesino", 1, "Dodonpa", 2, "Golpe Lengua", 3, "Dodonpa Asesino"},
    {155, "Engendrar Demonio", 2, "Poder Antiguo", 1, "Makosen", 3, "Onda Explosiva", 3, "Impacto Demoniaco"},
    {159, "Sentido Salvaje", 2, "Espiritu Indomable", 1, "Masenko", 3, "Kamehameha", 3, "Kamehameha Definitivo"},
};

static PcBattle sNr;
static s32 sNrKind,sNrSide,sNrSlot[2],sNrFilled[2][10],sNrCursor[2],sNrPhase,sNrSaga;
static s32 sNrFavorite[PORT_CHARA_ID_LIMIT],sNrList[PORT_TOTAL_CHARAS],sNrCount,sNrAge,sNrDp;
static s32 sNrConnection,sNrField,sNrIp[5]={127,0,0,1,29740};
static char sNrMessage[160];
static s32 sNrCostume[2];
static s32 sNrBgm;

static void Nr_Text(f32 x,f32 y,char *text,f32 size,f32 width,s32 style) {
    Ps_Align(0);Ps_Text(x,y,text,size,width,style);
}
static void Nr_Box(f32 x,f32 y,f32 w,f32 h,s32 selected,s32 side) {
    u32 gold=Ps_Rgba(255,204,50,230),edge=Ps_Rgba(60,85,125,180);
    Ps_Rect(x-1,y-1,x+w+1,y+h+1,selected?gold:edge,selected?gold:edge);
    Ps_Rect(x,y,x+w,y+h,selected?Ps_Rgba(110,65,12,220):Ps_Rgba(side?22:12,side?30:26,side?52:44,210),Ps_Rgba(6,10,20,230));
}
static void Nr_Pill(f32 x,f32 y,f32 w,f32 h,u32 c0,u32 c1) {
    Ps_Rect(x,y,x+w,y+h,c0,c1);
}
static void Nr_Image(void *resource,s32 tex,f32 x,f32 y,f32 w,f32 h,s32 flip,s32 alpha) {
    MTexRes *res=resource;u64 tex0;LoadSprite list[2];s32 tw,th;
    if(!res || !res->tex) return;
    tex0=*(u64 *)(res->tex+tex*0x40+0x30);
    tw=1<<((tex0>>26)&15);th=1<<((tex0>>30)&15);
    if(tw<1||tw>1024||th<1||th>1024) return;
    memset(list,0,sizeof(list));list[0].flags=LOAD_SPR_DRAW;list[1].flags=LOAD_SPR_END;
    list[0].x0=(s32)Ps_X(x);list[0].y0=(s32)Ps_Y(y);list[0].x1=(s32)Ps_X(x+w);list[0].y1=(s32)Ps_Y(y+h);
    list[0].u0=flip?tw:0;list[0].u1=flip?0:tw;list[0].v1=th;list[0].tex=tex;
    list[0].r=list[0].g=list[0].b=128;list[0].a=alpha;
    Sprite_DrawList(res,0,0,list);
}
static void Nr_Chip(s32 chara,f32 x,f32 y,f32 w,f32 h,s32 alpha) {
    MTexRes *res;
    if(chara<0)return;
    if(PORT_CHARA_ADDED(chara)) {
        res=(MTexRes *)PortChips_Get(chara);
        if(res) {Nr_Image(res,0,x,y,w,h,0,alpha);return;}
        chara=Port_CharaBase(chara);
    }
    res=(MTexRes *)MPACK_AT(gTeamSel->chipPack,chara+1);
    Nr_Image(res,0,x,y,w,h,0,alpha);
}
static s32 Nr_GetSaga(s32 chara) {
    if(chara<0) return NR_SAGA_ALL;
    if(PORT_CHARA_ADDED(chara) || chara>=161) return NR_SAGA_SUPER;
    if(chara==11 || chara==12 || (chara>=66 && chara<=70) || (chara>=148 && chara<=157)) return NR_SAGA_CLASSIC;
    if((chara>=7 && chara<=10) || chara==38 || chara==54 || chara==63 || chara==64 || chara==65 || (chara>=140 && chara<=147)) return NR_SAGA_GT;
    if(chara==53 || chara==61 || chara==62 || (chara>=71 && chara<=76) || (chara>=118 && chara<=139) || chara==159 || chara==160) return NR_SAGA_MOVIES;
    return NR_SAGA_Z;
}
static void Nr_List(void) {
    s32 i,id,saga;sNrCount=0;
    for(i=0;i<PORT_TOTAL_CHARAS;i++) {
        id=Port_PcRosterId(i);
        if(id<0) {
            if(sNrSaga==NR_SAGA_ALL) sNrList[sNrCount++]=i;
            continue;
        }
        if(sNrSaga==NR_SAGA_FAV) {
            if(sNrFavorite[id]) sNrList[sNrCount++]=i;
            continue;
        }
        saga=Nr_GetSaga(id);
        if(sNrSaga==NR_SAGA_ALL || sNrSaga==saga) sNrList[sNrCount++]=i;
    }
    if(sNrCursor[sNrSide]>=sNrCount) sNrCursor[sNrSide]=sNrCount?sNrCount-1:0;
}
static s32 Nr_Id(void) {return sNrCount?Port_PcRosterId(sNrList[sNrCursor[sNrSide]]):-1;}
static void Nr_Face(void) {
    s32 s,id=Nr_Id();
    for(s=0;s<2;s++) {
        s32 target=s==sNrSide?id:sNr.fighter[s][sNrSlot[s]].chara;
        s32 color=s==sNrSide?sNrCostume[s]:sNr.fighter[s][sNrSlot[s]].costume;
        s32 changed;
        if(sNrKind==3 && s==1) target=-1;
        changed=target!=gTeamSel->side[s]->chara || color!=gTeamSel->side[s]->member[0].color;
        gTeamSel->side[s]->cur=0;
        gTeamSel->side[s]->member[0].color=color;
        if(changed) {gTeamSel->side[s]->chara=target;TeamSel_RequestFace(s);}
    }
}
static s32 Nr_Cost(s32 s) {
    s32 i,n=0;for(i=0;i<sNr.count[s];i++) if(sNrFilled[s][i]) n+=ChrTbl_GetCost(sNr.fighter[s][i].chara);return n;
}
static s32 Nr_Complete(void) {
    s32 s,i;for(s=0;s<(sNrKind==3?1:2);s++) for(i=0;i<sNr.count[s];i++) if(!sNrFilled[s][i]) return 0;
    if(sNrDp>0 && (Nr_Cost(0)>sNrDp || (sNrKind!=3 && Nr_Cost(1)>sNrDp)))return 0;
    return 1;
}
static void Nr_Selected(void) {
    s32 id=Nr_Id(),side=sNrSide,slot=sNrSlot[side],next;char why[160];
    if(id<0) {Ps_Copy(sNrMessage,"Casilla reservada: instala un personaje para usarla.",160);Snd_PlaySe(1,7);return;}
    if(sNrDp>0 && Nr_Cost(side)-(sNrFilled[side][slot]?ChrTbl_GetCost(sNr.fighter[side][slot].chara):0)+ChrTbl_GetCost(id)>sNrDp) {
        Ps_Copy(sNrMessage,"El personaje supera los puntos disponibles del equipo.",160);Snd_PlaySe(1,7);return;
    }
    if(!Port_ExtraCostumeOk(id,sNrCostume[side],why,sizeof(why))) {Ps_Copy(sNrMessage,"Ese traje no tiene todos sus archivos. Elige otro traje.",160);return;}
    sNr.fighter[side][slot].chara=id;sNr.fighter[side][slot].costume=sNrCostume[side];sNrFilled[side][slot]=1;sNrMessage[0]=0;
    Port_RosterLog(1,side,slot,id,sNr.fighter[side][slot].costume);Snd_PlaySe(1,1);
    next=slot+1;
    if(next<sNr.count[side]) sNrSlot[side]=next;
    else if(side==0 && sNrKind!=3) {sNrSide=1;Nr_List();}
}
static void Nr_StageRequest(void) {
    gTeamSel->stage->stage=sNr.stage;TeamSel_RequestStage();
}
static void Nr_Star(f32 x,f32 y,f32 r) {
    s32 k;for(k=0;k<5;k++) {f32 a=-1.5707963f+k*1.2566371f,b=a+0.6283185f,c=a+1.2566371f;u32 red=Ps_Rgba(255,160,20,128);
        Ps_Quad(x,y,x+cosf(a)*r,y+sinf(a)*r,x+cosf(b)*r*.4f,y+sinf(b)*r*.4f,x+cosf(c)*r,y+sinf(c)*r,red,red,red,red);
    }
}
static s32 Nr_GetForms(s32 chara,s32 *forms,s32 max) {
    s32 count=0,i,k,j;
    if(chara<0 || max<1) return 0;
    if(PORT_CHARA_ADDED(chara)) {
        s32 n=Port_CharaForms(chara,forms,max);
        if(n>0) return n;
        forms[0]=chara;return 1;
    }
    if(gTeamSel && gTeamSel->cells[0]) {
        TsCell *cells=gTeamSel->cells[0];
        s32 masterCount=gTeamSel->masterCount[0];
        for(i=0;i<masterCount;i++) {
            if(cells[i].id==chara) {
                count=cells[i].formCount;if(count>max)count=max;
                for(k=0;k<count;k++) forms[k]=cells[i].form[k];
                return count;
            }
            for(k=0;k<cells[i].formCount;k++) {
                if(cells[i].form[k]==chara) {
                    count=cells[i].formCount;if(count>max)count=max;
                    for(j=0;j<count;j++) forms[j]=cells[i].form[j];
                    return count;
                }
            }
        }
    }
    forms[0]=chara;return 1;
}
static void Nr_SelectForm(s32 targetId) {
    s32 i;
    if(targetId<0) return;
    for(i=0;i<sNrCount;i++) {
        if(Port_PcRosterId(sNrList[i])==targetId) {
            sNrCursor[sNrSide]=i;sNrCostume[sNrSide]=0;gTeamSel->side[sNrSide]->chara=-1;
            Nr_Face();Snd_PlaySe(1,0);return;
        }
    }
    sNrSaga=NR_SAGA_ALL;Nr_List();
    for(i=0;i<sNrCount;i++) {
        if(Port_PcRosterId(sNrList[i])==targetId) {
            sNrCursor[sNrSide]=i;sNrCostume[sNrSide]=0;gTeamSel->side[sNrSide]->chara=-1;
            Nr_Face();Snd_PlaySe(1,0);return;
        }
    }
}
static void Nr_GetTechniques(s32 chara,const char **b1_1,s32 *c1_1,const char **b1_2,s32 *c1_2,
                            const char **b2_1,s32 *k2_1,const char **b2_2,s32 *k2_2,const char **ult) {
    s32 i,count=(s32)(sizeof(sTechTable)/sizeof(sTechTable[0]));
    for(i=0;i<count;i++) {
        if(sTechTable[i].id==chara) {
            *b1_1=sTechTable[i].b1_1;*c1_1=sTechTable[i].c1_1;
            *b1_2=sTechTable[i].b1_2;*c1_2=sTechTable[i].c1_2;
            *b2_1=sTechTable[i].b2_1;*k2_1=sTechTable[i].k2_1;
            *b2_2=sTechTable[i].b2_2;*k2_2=sTechTable[i].k2_2;
            *ult=sTechTable[i].ult;return;
        }
    }
    if(chara>=161 || PORT_CHARA_ADDED(chara)) {
        *b1_1="Instinto Divino";*c1_1=2;*b1_2="Sentido Superior";*c1_2=1;
        *b2_1="Kamehameha Divino";*k2_1=3;*b2_2="Hakai Cosmico";*k2_2=4;
        *ult="Fuerza Suprema";return;
    }
    *b1_1="Sentido Salvaje";*c1_1=2;*b1_2="Carga Ki";*c1_2=1;
    *b2_1="Rafaga de Energia";*k2_1=3;*b2_2="Ataque Meteoro";*k2_2=3;
    *ult="Impacto Definitivo";
}
static void Nr_Background(void) {
    s32 i;
    if(gTeamSel->bg[0] && (gTeamSel->flags&TEAMSEL_STAGE_READY)) Sprite_DrawPicture(gTeamSel->bg[0],0,0,96);
    else if(gTeamSel->bg[1]) Sprite_DrawPicture(gTeamSel->bg[1],0,0,96);
    /* Modern deep cosmic backdrop */
    Ps_Rect(0,0,512,448,Ps_Rgba(6,12,24,200),Ps_Rgba(3,6,15,225));
    /* Ambient diagonal beams */
    Ps_Quad(0,0,260,0,0,260,130,130,Ps_Rgba(0,180,255,25),Ps_Rgba(0,180,255,0),Ps_Rgba(0,180,255,0),Ps_Rgba(0,180,255,0));
    Ps_Quad(250,448,512,240,512,448,380,448,Ps_Rgba(255,140,20,15),Ps_Rgba(255,140,20,0),Ps_Rgba(255,140,20,0),Ps_Rgba(255,140,20,0));
    /* Top sleek gold aura bar */
    Ps_Rect(12,8,500,10,Ps_Rgba(0,180,255,180),Ps_Rgba(0,120,220,180));
}

static void Nr_DrawSparkingZeroUI(void) {
    s32 id=Nr_Id(),s=sNrSide,i,forms[8],formCount,k;char line[160];
    const char *b1_1,*b1_2,*b2_1,*b2_2,*ult;s32 c1_1,c1_2,k2_1,k2_2;
    u32 teamCol=s?Ps_Rgba(255,60,60,240):Ps_Rgba(0,195,255,240);
    u32 gold=Ps_Rgba(255,204,50,255);

    /* 1. TOP TEAM CAPSULES (x: 16..215, y: 12..38) */
    {
        s32 teamSlots=sNr.count[s];
        f32 capW=teamSlots<=5?(194.0f/teamSlots)-3.0f:(194.0f/5.0f)-3.0f;
        f32 capH=teamSlots<=5?24.0f:12.0f;
        for(i=0;i<teamSlots;i++) {
            f32 cx=16+(i%5)*(capW+3.0f);
            f32 cy=12+(i/5)*(capH+2.0f);
            s32 activeSlot=(i==sNrSlot[s]);
            if(activeSlot) Ps_Rect(cx-1,cy-1,cx+capW+1,cy+capH+1,gold,gold);
            else Ps_Rect(cx,cy,cx+capW,cy+capH,Ps_Rgba(20,30,50,180),Ps_Rgba(10,16,28,200));
            Ps_Rect(cx,cy,cx+capW,cy+capH,activeSlot?Ps_Rgba(80,50,10,220):Ps_Rgba(12,18,32,220),Ps_Rgba(6,9,18,240));
            if(sNrFilled[s][i]) {
                Nr_Chip(sNr.fighter[s][i].chara,cx+2,cy+1,capW-4,capH-2,128);
            } else {
                sprintf(line,"%d",i+1);
                Nr_Text(cx+capW*0.5f-4.0f,cy+capH*0.5f-5.0f,line,0.36f,16,activeSlot?1:0);
            }
        }
    }

    /* 2. SAGAS FILTER TABS (x: 16..215, y: 44..64) */
    {
        f32 tabW=198.0f/NR_SAGA_COUNT;
        for(i=0;i<NR_SAGA_COUNT;i++) {
            f32 tx=16+i*tabW;
            s32 activeTab=(i==sNrSaga);
            if(activeTab) {
                Ps_Rect(tx,44,tx+tabW-2,64,gold,Ps_Rgba(220,120,10,240));
                Nr_Text(tx+2,47,(char *)sSagaTabs[i],0.31f,tabW-4,1);
            } else {
                Ps_Rect(tx,45,tx+tabW-2,63,Ps_Rgba(25,35,55,160),Ps_Rgba(12,18,30,190));
                Nr_Text(tx+2,47,(char *)sSagaTabs[i],0.28f,tabW-4,0);
            }
        }
    }

    /* 3. CENTERPIECE FIGHTER PRESENTATION (x: 165..335, y: 38..375) */
    if((gTeamSel->side[s]->flags&(TEAMSEL_SIDE_FACE_READY|TEAMSEL_SIDE_NO_FACE))==TEAMSEL_SIDE_FACE_READY) {
        /* Floor soft radial shadow */
        Ps_Disc(255,365,56,Ps_Rgba(0,0,0,170),Ps_Rgba(0,0,0,0));
        /* Prominent full character portrait */
        Nr_Image(gTeamSel->faceRes[s],0,166,36,176,338,s?1:0,128);
    }

    /* 4. RIGHT PANEL: SPARKING! ZERO FIGHTER HUB (x: 326..502, y: 12..398) */
    Ps_Rect(326,12,502,398,Ps_Rgba(8,16,30,225),Ps_Rgba(5,10,20,240));
    Ps_Rect(326,12,502,14,teamCol,teamCol);

    /* Player Badge and Costume */
    Ps_Rect(332,18,406,31,teamCol,teamCol);
    Nr_Text(336,19,s?"JUGADOR 2":"JUGADOR 1",0.34f,68,1);
    sprintf(line,"Traje %d/8",sNrCostume[s]+1);
    Nr_Text(414,19,line,0.32f,84,0);

    /* Character Name */
    if(id<0) Ps_Copy(line,"CASILLA RESERVADA",160);
    else Port_RosterName(line,160,id);
    Nr_Text(332,34,line,0.58f,166,1);

    /* Mastery Stars & Cost */
    for(k=0;k<5;k++) Nr_Star(338+k*12.0f,55,3.0f);
    sprintf(line,"COSTE: %d PD",id>=0?ChrTbl_GetCost(id):0);
    Nr_Text(414,51,line,0.34f,84,1);

    /* Form Wheel Track */
    Nr_Text(332,67,"FORMAS Y TRANSFORMACIONES",0.32f,166,1);
    formCount=Nr_GetForms(id,forms,8);
    if(formCount>1) {
        Ps_Rect(340,94,340+(formCount-1)*38.0f,96,Ps_Rgba(60,90,130,160),Ps_Rgba(30,50,80,180));
    }
    for(k=0;k<formCount;k++) {
        f32 nx=346+(k%4)*38.0f;
        f32 ny=92+(k/4)*30.0f;
        s32 activeForm=(forms[k]==id);
        if(activeForm) {
            Ps_Disc(nx,ny,16,gold,Ps_Rgba(255,140,0,255));
        } else {
            Ps_Disc(nx,ny,14,Ps_Rgba(45,65,95,190),Ps_Rgba(25,35,55,200));
        }
        Ps_Disc(nx,ny,12,Ps_Rgba(10,16,28,255),Ps_Rgba(6,10,20,255));
        Nr_Chip(forms[k],nx-10,ny-10,20,20,128);
    }

    /* Techniques & Blasts */
    Nr_Text(332,138,"T\303\211CNICAS Y HABILIDADES",0.34f,166,1);
    Nr_GetTechniques(id,&b1_1,&c1_1,&b1_2,&c1_2,&b2_1,&k2_1,&b2_2,&k2_2,&ult);

    /* Blast 1 - Slot 1 */
    Ps_Rect(332,154,352,168,Ps_Rgba(0,180,240,240),Ps_Rgba(0,120,200,240));
    Nr_Text(334,155,"B1",0.29f,16,1);
    Nr_Text(356,155,(char *)b1_1,0.33f,112,0);
    sprintf(line,"%d",c1_1);Nr_Text(478,155,line,0.33f,18,1);

    /* Blast 1 - Slot 2 */
    Ps_Rect(332,172,352,186,Ps_Rgba(0,180,240,240),Ps_Rgba(0,120,200,240));
    Nr_Text(334,173,"B1",0.29f,16,1);
    Nr_Text(356,173,(char *)b1_2,0.33f,112,0);
    sprintf(line,"%d",c1_2);Nr_Text(478,173,line,0.33f,18,1);

    /* Blast 2 - Slot 1 */
    Ps_Rect(332,190,352,204,Ps_Rgba(255,140,20,240),Ps_Rgba(200,90,10,240));
    Nr_Text(334,191,"B2",0.29f,16,1);
    Nr_Text(356,191,(char *)b2_1,0.33f,110,0);
    sprintf(line,"Ki:%d",k2_1);Nr_Text(468,191,line,0.31f,30,1);

    /* Blast 2 - Slot 2 */
    Ps_Rect(332,208,352,222,Ps_Rgba(255,140,20,240),Ps_Rgba(200,90,10,240));
    Nr_Text(334,209,"B2",0.29f,16,1);
    Nr_Text(356,209,(char *)b2_2,0.33f,110,0);
    sprintf(line,"Ki:%d",k2_2);Nr_Text(468,209,line,0.31f,30,1);

    /* Ultimate Blast */
    Ps_Rect(332,226,354,240,Ps_Rgba(180,50,240,240),Ps_Rgba(120,30,180,240));
    Nr_Text(334,227,"ULT",0.27f,18,1);
    Nr_Text(358,227,(char *)ult,0.33f,108,1);
    Nr_Text(470,227,"MAX",0.29f,26,1);

    /* Team Summary */
    sprintf(line,"INTEGRANTE %d DE %d",sNrSlot[s]+1,sNr.count[s]);
    Nr_Text(332,248,line,0.33f,166,0);
    if(sNrDp>0) {
        sprintf(line,"Puntos de Equipo: %d / %d PD",Nr_Cost(s),sNrDp);
        Nr_Text(332,262,line,0.33f,166,0);
    }
    for(i=0;i<sNr.count[s];i++) {
        f32 mx=332+i*24.0f;
        Nr_Box(mx,278,21,21,i==sNrSlot[s],s);
        if(sNrFilled[s][i]) Nr_Chip(sNr.fighter[s][i].chara,mx+1,279,19,19,128);
        else {sprintf(line,"%d",i+1);Nr_Text(mx+6,281,line,0.34f,14,3);}
    }

    /* Command Hints */
    Nr_Text(332,308,"[CUADRADO] Alternar Traje",0.31f,166,3);
    Nr_Text(332,322,"[R3] Alternar Transformacion",0.31f,166,3);
    Nr_Text(332,336,"[TRIANGULO] Cambiar a Equipo Rival",0.31f,166,3);
    Nr_Text(332,350,"[L3] Marcar como Favorito",0.31f,166,3);
    Nr_Text(332,364,"[L1 / R1] Filtrar Sagas",0.31f,166,3);
}

static void Nr_DrawGrid(void) {
    s32 page=sNrCursor[sNrSide]/NR_PAGE,k,idx,id;char text[100];
    u32 gold=Ps_Rgba(255,204,50,255);

    /* Grid cards: 5 cols x 6 rows */
    for(k=0;k<NR_PAGE;k++) {
        f32 x=NR_CX+(k%NR_COLS)*NR_DX,y=NR_CY+(k/NR_COLS)*NR_DY;
        idx=page*NR_PAGE+k;if(idx>=sNrCount) break;id=Port_PcRosterId(sNrList[idx]);
        s32 selected=(idx==sNrCursor[sNrSide]);
        if(selected) {
            Ps_Rect(x-2,y-2,x+NR_CARD_W+2,y+NR_CARD_H+2,gold,Ps_Rgba(255,140,0,255));
            Ps_Rect(x,y,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(70,42,12,230),Ps_Rgba(25,12,6,240));
        } else {
            Ps_Rect(x-1,y-1,x+NR_CARD_W+1,y+NR_CARD_H+1,Ps_Rgba(35,50,75,150),Ps_Rgba(18,28,45,160));
            Ps_Rect(x,y,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(12,18,30,210),Ps_Rgba(6,10,18,230));
        }
        if(id>=0) Nr_Chip(id,x+2,y+2,33,28,128);
        else {Ps_Rect(x+2,y+2,x+35,y+30,Ps_Rgba(16,22,34,160),Ps_Rgba(8,12,20,180));Nr_Text(x+13,y+9,"+",.55f,20,3);}

        /* Card bottom tag */
        Ps_Rect(x,y+31,x+NR_CARD_W,y+NR_CARD_H,Ps_Rgba(5,8,16,220),Ps_Rgba(5,8,16,240));
        sprintf(text,"%03d",sNrList[idx]+1);
        Nr_Text(x+3,y+31,text,0.28f,30,selected?1:0);
        if(id>=0 && sNrFavorite[id]) Nr_Star(x+28,y+6,2.5f);
    }
    if(!sNrCount) Nr_Text(26,180,"No hay luchadores en esta saga.",0.48f,200,0);

    /* Page Navigation */
    sprintf(text,"Pagina %d / %d  (%d personajes)",page+1,(sNrCount+NR_PAGE-1)/NR_PAGE,sNrCount);
    Nr_Text(16,352,text,0.33f,200,0);

    /* Bottom Action Controls */
    Nr_Box(16,404,180,26,0,0);
    Nr_Text(24,408,"X  CONFIRMAR LUCHADOR",0.42f,168,0);

    Nr_Box(202,404,70,26,0,0);
    Nr_Text(210,408,"O  VOLVER",0.42f,56,0);

    Nr_Box(280,404,222,26,Nr_Complete(),1);
    Nr_Text(288,408,sNrKind==3?"START  CONFIRMAR EQUIPO":"START  ELEGIR ESCENARIO",0.42f,206,1);
}

static void Nr_DrawStage(void) {
    s32 i;char text[100];
    Nr_Text(16,46,"ELIGE EL CAMPO DE BATALLA",.8f,470,1);
    for(i=0;i<35;i++) {
        f32 x=16+(i%7)*69.0f,y=83+(i/7)*54.0f;void *res=MPACK_AT(gTeamSel->stagePack,i+1);
        Nr_Box(x,y,64,48,sNr.stage==i,0);Nr_Image(res,0,x+2,y+2,60,44,0,128);
        if(sNr.stage==i) {sprintf(text,"%d",i+1);Nr_Text(x+3,y+32,text,.38f,52,1);}
    }
    Nr_Text(16,363,"Cruceta: escenario   X / START: continuar   C\303\255rculo: personajes",.4f,480,0);
    sprintf(text,"L1 / R1: musica  %d / %d",gTeamSel->stage->bgm+1,gTeamSel->bgmCount);Nr_Text(16,382,text,.42f,480,1);
    Nr_Box(280,403,216,24,1,1);Nr_Text(290,406,sNr.mode==PC_RAID&&sNr.players>1?"X  SALA COOPERATIVA":"X  COMENZAR COMBATE",.46f,198,1);
}

static void Nr_DrawNetwork(void) {
    s32 i;char line[100];static char *choices[]={"COMBATE LOCAL","CREAR SALA ONLINE","UNIRSE A UNA SALA"};
    Nr_Text(26,57,"RAID COOPERATIVO",1.0f,460,1);
    sprintf(line,"%d aliados humanos contra un jefe CPU",sNr.players);Nr_Text(26,94,line,.55f,460,0);
    for(i=0;i<3;i++) {Nr_Box(30,135+i*43,450,34,sNrConnection==i,1);Nr_Text(45,141+i*43,choices[i],.68f,420,sNrConnection==i?1:0);}
    if(sNrConnection) {
        Nr_Text(30,278,"DIRECCI\303\223N DEL ANFITRI\303\223N Y PUERTO",.48f,440,0);
        for(i=0;i<5;i++) {f32 x=30+i*91.0f;Nr_Box(x,306,84,38,sNrField==i,0);sprintf(line,"%d",sNrIp[i]);Nr_Text(x+9,314,line,.65f,70,1);}
        Nr_Text(30,354,"L1/R1: campo   Arriba/abajo: valor   Cuadrado: +10",.43f,450,0);
        Nr_Text(30,374,"Mismos datos y paquetes. El puerto UDP debe ser accesible.",.4f,450,3);
    }
    Nr_Text(30,386,"SELECT: conexion   Triangulo: crear / unirse",.34f,450,3);
    Nr_Box(280,403,216,24,1,1);Nr_Text(290,406,"START  CONFIRMAR",.5f,198,1);
}

static void Nr_Draw(void) {
    char title[120];static char *titles[]={"DUELO","ENTRENAMIENTO","TAG TEAM SIMULT\303\201NEO","RAID COOPERATIVO","RELEVOS 10 CONTRA 10","TODOS CONTRA TODOS"};
    Ps_PageSet();Nr_Background();
    sprintf(title,"%s  |  %s",sNrKind==3?"BATALLA DEFINITIVA":titles[sNr.mode],sSagaTitles[sNrSaga]);
    Nr_Text(16,6,title,0.72f,380,1);
    if(sNrPhase==0) {Nr_DrawSparkingZeroUI();Nr_DrawGrid();}
    else if(sNrPhase==1) Nr_DrawStage();else Nr_DrawNetwork();
    if(sNrMessage[0]) {Ps_Rect(12,430,500,447,Ps_Rgba(63,17,8,128),Ps_Rgba(25,8,7,128));Nr_Text(17,431,sNrMessage,.36f,476,1);}
    else Nr_Text(16,432,"PLANTILLA SPARKING! ZERO  |  Formas independientes  |  L1/R1: Sagas  |  R3: Transformacion",.33f,480,3);
}

/* kind: 0 individual duel, 1 relay teams, 2 training, 3 the mode's player-only team. */
s32 Port_NativeRosterPick(PcBattle *out,s32 kind,s32 dp) {
    TsTeam saved[2];s32 savedPlayers,savedType,savedDp,savedStage,savedBgm;
    s32 s,i,mode=Port_ModesFight(),result=0,remembered,max,done=0;
    PcBattle last;char why[160];
    if(Port_NetActive()||Port_NetSession()) return 0;
    if(!gMenuArc5) gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);
    saved[0]=gTsProgress->team[0];saved[1]=gTsProgress->team[1];savedPlayers=gTsProgress->players;
    savedType=gTsProgress->battleType;savedDp=gTsProgress->dpLevel;savedStage=gTsProgress->stageCell;savedBgm=gTsProgress->bgm;
    memset(gTsProgress->team,0,sizeof(gTsProgress->team));gTsProgress->players=0;gTsProgress->battleType=1;gTsProgress->dpLevel=0;gTsProgress->stageCell=gTsProgress->bgm=0;
    for(s=0;s<2;s++) for(i=0;i<5;i++) {gTsProgress->team[s].member[i].chara=i?-1:s?4:0;}
    Port_PcBattleDefault(&sNr);sNr.mode=kind==2?PC_TRAINING:kind==1||kind==3?PC_RELAY:PC_DUEL;
    if(kind==1) {if(mode==PORT_MODE_TEAM) sNr.mode=PC_TAG;else if(mode==PORT_MODE_FFA) sNr.mode=PC_FFA;else if(mode==PORT_MODE_RAID) sNr.mode=PC_RAID;}
    sNr.players=(kind==2||kind==3)?1:mode?Port_LocalPlayers():savedPlayers==1?2:1;
    max=sNr.mode==PC_RELAY?10:(sNr.mode==PC_TAG||sNr.mode==PC_FFA)?5:sNr.mode==PC_RAID?4:1;
    sNr.count[0]=sNr.count[1]=max;if(sNr.mode==PC_RAID) {sNr.count[0]=sNr.players;sNr.count[1]=1;}
    if(kind==3) sNr.count[1]=1;
    remembered=Port_RosterRecall(sNr.mode,&last);
    if(remembered && last.count[0]<=max && last.count[1]<=max && last.players==sNr.players) sNr=last;else remembered=0;
    if(kind==3) sNr.count[1]=1;
    sNrCostume[0]=sNr.fighter[0][0].costume;sNrCostume[1]=sNr.fighter[1][0].costume;
    memset(sNrFilled,0,sizeof(sNrFilled));if(remembered) memset(sNrFilled,1,sizeof(sNrFilled));
    memset(sNrSlot,0,sizeof(sNrSlot));sNrCursor[0]=0;sNrCursor[1]=4;sNrSide=0;sNrKind=kind;sNrDp=dp;sNrPhase=sNrAge=sNrConnection=sNrField=sNrSaga=0;sNrMessage[0]=0;
    if(kind==3) sNrFilled[1][0]=1;
    for(i=0;i<PORT_CHARA_ID_LIMIT;i++) sNrFavorite[i]=0;
    for(i=0;i<Port_PcRosterCount();i++) {s32 id=Port_PcRosterId(i);sNrFavorite[id]=Port_RosterFavorite(id,0);}
    TeamSel_Init(2);gPortNativeRosterActive=1;
    {extern volatile s32 gPortPcTeamReady;gPortPcTeamReady=0;}
    Nr_List();Nr_Face();Nr_StageRequest();Port_RosterLog(0,kind,sNr.mode,Port_PcRosterCount(),PORT_TOTAL_CHARAS);
    ColorFade_StartIn(0,0,0,20);
    while(1) {
        u32 p,r; s32 x,y,click;
        Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();
        p=gPad[0].gamePressed|Port_RosterTestInput((unsigned)++sNrAge);r=gPad[0].gameRepeat|p;
        if(sNrSide==1 && sNr.players==2) {p|=gPad[1].gamePressed;r|=gPad[1].gameRepeat|p;}
        if(gProgress->flags&MPROG_FREEZE) p=r=0;
        Ps_PageSet();click=Port_RosterPointerRead(&x,&y);
        if(click) {
            if(sNrPhase==0) {
                s32 page=sNrCursor[sNrSide]/NR_PAGE;
                /* Click on Grid Cards */
                for(i=0;i<NR_PAGE;i++) {
                    f32 xx=NR_CX+(i%NR_COLS)*NR_DX,yy=NR_CY+(i/NR_COLS)*NR_DY;
                    if(x>=Ps_X(xx)&&x<Ps_X(xx+NR_CARD_W)&&y>=Ps_Y(yy)&&y<Ps_Y(yy+NR_CARD_H)&&page*NR_PAGE+i<sNrCount) {
                        if(sNrCursor[sNrSide]!=page*NR_PAGE+i) sNrCostume[sNrSide]=0;
                        sNrCursor[sNrSide]=page*NR_PAGE+i;p|=PADG_CROSS;
                    }
                }
                /* Click on Sagas Tabs */
                if(y>=Ps_Y(44)&&y<Ps_Y(65)&&x>=Ps_X(16)&&x<Ps_X(215)) {
                    s32 tab=(s32)((x-Ps_X(16))*NR_SAGA_COUNT/(Ps_X(215)-Ps_X(16)));
                    if(tab>=0&&tab<NR_SAGA_COUNT) {sNrSaga=tab;sNrCursor[sNrSide]=0;Nr_List();Nr_Face();Snd_PlaySe(1,0);}
                }
                /* Click on Team Capsules */
                for(i=0;i<sNr.count[sNrSide];i++) {
                    f32 capW=sNr.count[sNrSide]<=5?(194.0f/sNr.count[sNrSide])-3.0f:(194.0f/5.0f)-3.0f;
                    f32 capH=sNr.count[sNrSide]<=5?24.0f:12.0f;
                    f32 cx=16+(i%5)*(capW+3.0f);
                    f32 cy=12+(i/5)*(capH+2.0f);
                    if(x>=Ps_X(cx)&&x<Ps_X(cx+capW)&&y>=Ps_Y(cy)&&y<Ps_Y(cy+capH)) {sNrSlot[sNrSide]=i;Snd_PlaySe(1,0);}
                }
                /* Click on Transformation Nodes */
                {
                    s32 forms[8],fCount=Nr_GetForms(Nr_Id(),forms,8),k;
                    for(k=0;k<fCount;k++) {
                        f32 nx=346+(k%4)*38.0f,ny=92+(k/4)*30.0f;
                        if(x>=Ps_X(nx-14)&&x<Ps_X(nx+14)&&y>=Ps_Y(ny-14)&&y<Ps_Y(ny+14)) {
                            Nr_SelectForm(forms[k]);
                        }
                    }
                }
                /* Click on Bottom Action Buttons */
                if(y>=Ps_Y(404)&&y<Ps_Y(430)) {
                    if(x<Ps_X(196)) p|=PADG_CROSS;
                    else if(x<Ps_X(275)) p|=PADG_CIRCLE;
                    else p|=PADG_START;
                }
            } else if(sNrPhase==1) {
                for(i=0;i<35;i++){f32 xx=16+(i%7)*69.0f,yy=83+(i/7)*54.0f;if(x>=Ps_X(xx)&&x<Ps_X(xx+64)&&y>=Ps_Y(yy)&&y<Ps_Y(yy+48)){sNr.stage=i;Nr_StageRequest();p|=PADG_CROSS;}}
                if(y>=Ps_Y(403)&&y<Ps_Y(427)&&x>=Ps_X(280))p|=PADG_CROSS;
            } else {
                for(i=0;i<3;i++)if(x>=Ps_X(30)&&x<Ps_X(480)&&y>=Ps_Y(135+i*43)&&y<Ps_Y(169+i*43))sNrConnection=i;
                if(y>=Ps_Y(403)&&y<Ps_Y(427)&&x>=Ps_X(280))p|=PADG_START;
            }
        }
        if(sNrPhase==0) {
            s32 old=sNrCursor[sNrSide],cur=old,id;
            if(r&(PADG_LEFT|PADG_LEFT2)) cur--;
            else if(r&(PADG_RIGHT|PADG_RIGHT2)) cur++;
            else if(r&(PADG_UP|PADG_UP2)) cur-=NR_COLS;
            else if(r&(PADG_DOWN|PADG_DOWN2)) cur+=NR_COLS;

            /* L1 / R1: Cycle Saga Filter Tabs */
            if(p&(PADG_L1|PADG_R1)) {
                sNrSaga=Ps_Wrap(sNrSaga+((p&PADG_L1)?-1:1),0,NR_SAGA_COUNT-1);
                cur=0;Nr_List();Snd_PlaySe(1,0);
            }
            if(sNrCount) {
                cur=Ps_Wrap(cur,0,sNrCount-1);
                sNrCursor[sNrSide]=cur;
            }
            if(cur!=old) {sNrCostume[sNrSide]=0;Snd_PlaySe(1,0);sNrMessage[0]=0;}
            if(p&PADG_TRIANGLE && kind!=3) {sNrSide^=1;Nr_List();Snd_PlaySe(1,0);}

            /* L2 / R2: Slot or Page Navigation */
            if(sNr.count[sNrSide]>1) {
                if(p&PADG_L2) sNrSlot[sNrSide]=Ps_Wrap(sNrSlot[sNrSide]-1,0,sNr.count[sNrSide]-1);
                if(p&PADG_R2) sNrSlot[sNrSide]=Ps_Wrap(sNrSlot[sNrSide]+1,0,sNr.count[sNrSide]-1);
                if(p&(PADG_L2|PADG_R2)) {
                    sNrCostume[sNrSide]=sNr.fighter[sNrSide][sNrSlot[sNrSide]].chara==Nr_Id()?sNr.fighter[sNrSide][sNrSlot[sNrSide]].costume:0;
                    gTeamSel->side[sNrSide]->chara=-1;
                }
            } else if(sNrCount>NR_PAGE) {
                if(p&(PADG_L2|PADG_R2)) {
                    s32 page=Ps_Wrap(cur/NR_PAGE+((p&PADG_L2)?-1:1),0,(sNrCount-1)/NR_PAGE);
                    cur=page*NR_PAGE+cur%NR_PAGE;if(cur>=sNrCount)cur=sNrCount-1;
                    sNrCursor[sNrSide]=cur;Snd_PlaySe(1,0);
                }
            }

            id=Nr_Id();
            /* SQUARE: Cycle Costumes */
            if((p&PADG_SQUARE)&&id>=0) {
                s32 c=sNrCostume[sNrSide],n;
                for(n=0;n<8;n++){c=(c+1)%8;if(Port_ExtraCostumeOk(id,c,why,sizeof(why)))break;}
                sNrCostume[sNrSide]=c;gTeamSel->side[sNrSide]->chara=-1;Snd_PlaySe(1,0);
            }
            /* R3: Cycle Transformations */
            if((p&PADG_R3)&&id>=0) {
                s32 forms[8],fCount=Nr_GetForms(id,forms,8),k,nextForm=id;
                if(fCount>1) {
                    for(k=0;k<fCount;k++) {
                        if(forms[k]==id) {nextForm=forms[(k+1)%fCount];break;}
                    }
                    Nr_SelectForm(nextForm);
                }
            }
            /* L3: Favorite Toggle */
            if((p&PADG_L3)&&id>=0) {
                sNrFavorite[id]=Port_RosterFavorite(id,1);
                if(sNrSaga==NR_SAGA_FAV) Nr_List();
                Snd_PlaySe(1,1);
            }
            if((p&PADG_SELECT)&&max>1&&sNr.mode!=PC_RAID) {
                static s32 counts[]={1,3,5,10};s32 n=0;
                while(n<4&&counts[n]<=sNr.count[sNrSide])n++;
                sNr.count[sNrSide]=n<4&&counts[n]<=max?counts[n]:1;
                if(sNrSlot[sNrSide]>=sNr.count[sNrSide])sNrSlot[sNrSide]=0;
                Snd_PlaySe(1,0);
            }
            if(p&PADG_CROSS) Nr_Selected();
            if(p&PADG_CIRCLE) {
                s32 slot=sNrSlot[sNrSide];
                if(sNrFilled[sNrSide][slot]) sNrFilled[sNrSide][slot]=0;
                else if(slot>0){sNrSlot[sNrSide]--;sNrFilled[sNrSide][slot-1]=0;}
                else if(sNrSide){sNrSide=0;Nr_List();}
                else {result=0;done=1;}
                Snd_PlaySe(1,2);
            }
            if(p&PADG_START) {
                if(Nr_Complete()){if(kind==3){result=1;done=1;}sNrPhase=1;Nr_StageRequest();sNrMessage[0]=0;Snd_PlaySe(1,1);}
                else{Ps_Copy(sNrMessage,"Completa las posiciones de ambos equipos antes de continuar.",160);Snd_PlaySe(1,7);}
            }
            Nr_Face();
        } else if(sNrPhase==1) {
            s32 old=sNr.stage;
            if(p&(PADG_L1|PADG_R1)) {s32 song;gTeamSel->stage->bgm=Ps_Wrap(gTeamSel->stage->bgm+((p&PADG_L1)?-1:1),0,gTeamSel->bgmCount-1);song=gTeamSel->bgmIds[gTeamSel->stage->bgm];if(song==TS_BGM_RANDOM)song=Rand_Range(9)+8;Bgm_Play(PORT_BGM_FILE(song));Snd_PlaySe(1,0);}
            if(r&(PADG_LEFT|PADG_LEFT2))sNr.stage--;else if(r&(PADG_RIGHT|PADG_RIGHT2))sNr.stage++;else if(r&(PADG_UP|PADG_UP2))sNr.stage-=7;else if(r&(PADG_DOWN|PADG_DOWN2))sNr.stage+=7;
            sNr.stage=Ps_Wrap(sNr.stage,0,34);if(old!=sNr.stage){Nr_StageRequest();Snd_PlaySe(1,0);}
            if(p&PADG_CIRCLE){sNrPhase=0;Snd_PlaySe(1,2);}
            if(p&(PADG_CROSS|PADG_START)){if(sNr.mode==PC_RAID&&sNr.players>1)sNrPhase=2;else {result=1;done=1;}}
        } else {
            if(sNrConnection==0) {if(r&PADG_UP)sNrConnection=2;else if(r&PADG_DOWN)sNrConnection=1;}
            else {if(p&PADG_TRIANGLE)sNrConnection=sNrConnection==1?2:1;
                if(p&PADG_L1)sNrField=Ps_Wrap(sNrField-1,0,4);if(p&PADG_R1)sNrField=Ps_Wrap(sNrField+1,0,4);
                if(r&PADG_UP)sNrIp[sNrField]++;if(r&PADG_DOWN)sNrIp[sNrField]--;if(p&PADG_SQUARE)sNrIp[sNrField]+=10;
                sNrIp[sNrField]=Ps_Wrap(sNrIp[sNrField],sNrField==4?1:0,sNrField==4?65535:255);
            }
            if(p&PADG_CIRCLE){sNrPhase=1;sNrConnection=0;}
            if(p&PADG_SELECT)sNrConnection=(sNrConnection+1)%3;
            if(p&(PADG_START|PADG_CROSS)){if(!sNrConnection){result=1;done=1;}else {char addr[80];sprintf(addr,"%d.%d.%d.%d",sNrIp[0],sNrIp[1],sNrIp[2],sNrIp[3]);if(!Port_CoopLaunch(sNrConnection==1,addr,sNrIp[4],&sNr))Ps_Copy(sNrMessage,"No se pudo abrir la sala. Revisa los personajes y el puerto.",160);}}
        }
        Nr_Draw();Font_FlushAll();ColorFade_Draw();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
        if(done)break;
    }
    /* Finish outstanding native file work before freeing its resource buffers. */
    while(gTeamSel->faceState!=TEAMSEL_LOAD_IDLE || gTeamSel->stageState!=TEAMSEL_LOAD_IDLE) {
        Gfx_BeginFrame();TeamSel_UpdateFaceLoad();TeamSel_UpdateStageLoad();Nr_Draw();Font_FlushAll();Gfx_EndFrame(1);Dma_Flush();File_Stub264D90();
    }
    if(result) {*out=sNr;sNrBgm=gTeamSel->bgmIds[gTeamSel->stage->bgm];if(sNrBgm==TS_BGM_RANDOM)sNrBgm=Rand_Range(9)+8;Port_RosterRemember(&sNr);Port_RosterLog(2,kind,sNr.count[0],sNr.count[1],sNr.stage);}
    gPortNativeRosterActive=0;TeamSel_Term();Dma_ResetBuffers();
    gTsProgress->team[0]=saved[0];gTsProgress->team[1]=saved[1];gTsProgress->players=savedPlayers;gTsProgress->battleType=savedType;gTsProgress->dpLevel=savedDp;gTsProgress->stageCell=savedStage;gTsProgress->bgm=savedBgm;
    return result;
}
s32 Port_NativeRosterBattle(s32 kind) {
    PcBattle chosen;s32 mode=gProgress->mode,dp=0;
    if(kind==1&&gTsProgress->battleType==2) dp=(gTsProgress->dpLevel+2)*5;
    if(!Port_NativeRosterPick(&chosen,kind,dp))return 0;
    if(!Port_PcRosterQueue(&chosen))return 0;
    Port_PcNativeRules(gTsProgress->players,kind==2 && Port_ModesFight()==PORT_MODE_TRAINING,sNrBgm);
    if(!Port_PcBattleBuild())return 0;
    gProgress->mode=mode;return 1;
}
#endif
