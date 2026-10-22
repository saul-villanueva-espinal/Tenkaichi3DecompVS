/* Regressions for eight independent model/FA/KZE sets and legacy disc damage IDs. */
#include <assert.h>
#include <stdint.h>
#include "../src/plat_charas.c"
int Port_NetSession(void) {return 0;}
int PortExtras_Dir(const char *k,const char *e,char *out,unsigned size) {(void)k;(void)e;(void)out;(void)size;return 0;}
int Port_Setting(const char *n,int d) {(void)n;return d;}
void Port_SettingSave(const char *n,int v) {(void)n;(void)v;}
const char *Port_DiscCharaName(int chara) {(void)chara;return "?";}
void *Heap_Alloc(int n,int a,int t,int h) {(void)a;(void)t;(void)h;return malloc(n);}
void Heap_Free(void *p) {free(p);}
void Res_RelocateOffsets(void *a,void *b,void *c) {(void)a;(void)b;(void)c;}
int main(void) {
    Chara *c=&sRepl[0];char rel[80],out[400];int k;
    assert(kind_of("Goku_anm_3p.pak")==F_A0+2);
    assert(kind_of("Goku_eff_3p.pak")==F_E0+2);
    assert(kind_of("Goku_dmg_3p.pak")==F_D0+2);
    assert(kind_of("Goku_3p.pak")==2);
    sInit=1;sReplCount=1;sReplOf[91]=1;c->replaces=91;c->pkg=-1;
    for(k=0;k<F_COUNT;k++) snprintf(c->file[k],400,"independent-slot-%d",k);
    for(k=0;k<F_COUNT;k++) {
        snprintf(rel,sizeof(rel),"pzs3us2/%05d.bin",PORT_CHARA_FILE_INDEX+91*PORT_FILES_PER_CHARA+k);
        assert(PortCharas_Alias(rel,out,sizeof(out)) && !strcmp(out,c->file[k]));
    }
    for(k=0;k<4;k++) {
        snprintf(rel,sizeof(rel),"pzs3us1/%05d.bin",1423+91*10+4+k);
        assert(PortCharas_Alias(rel,out,sizeof(out)) && !strcmp(out,c->file[F_D0+k]));
        c->file[F_D0+k][0]=0;
        assert(PortCharas_Alias(rel,out,sizeof(out)) && !strcmp(out,c->file[k]));
    }
    strcpy(c->form[0],"none");strcpy(c->form[1],"NONE");strcpy(c->form[2],"none");resolve(c);
    assert(c->target[0]==255 && c->target[1]==255 && c->target[2]==255 && c->target[3]==-1);
    puts("Eight-costume routing and terminal forms PASS");return 0;
}
