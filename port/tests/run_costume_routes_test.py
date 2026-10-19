from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
def function(text,name):
    at=text.index(name+'(');start=text.rfind('\n',0,at)+1;begin=text.index('{',at);level=1;end=begin+1
    while level:
        level+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
tech=(root/'src/battle/btl_tech_a.c').read_text();viewer=(root/'src/battle/view_a_d.c').read_text()
source=r'''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef int32_t s32;typedef uint8_t u8;
#include "port_chara.h"
typedef struct {s32 transDefault;u8 transTarget[4];} Param;
typedef struct {s32 pad[3];s32 chara;Param *param;} Obj;
typedef struct {Obj *obj;} BtlTechChr;
static s32 links[4];
static Obj *BtlChar_GetObj(BtlTechChr *c) {return c->obj;}
int Port_CharaFormTarget(int c,int s) {(void)c;return links[s];}
'''
source+=function(tech,'BtlParam_PortTarget')+'\n'+function(tech,'BtlParam_GetUnkAC')+'\n'+function(viewer,'ChrView_GetModelFile')
source+=r'''
int main(void) {
 Param p={3,{255,255,255,94}};Obj o={{0,0,0},91,&p};BtlTechChr c={&o};int k;
 for(k=0;k<4;k++)links[k]=-1;
 assert(BtlParam_GetUnkAC(&c)==3); /* untouched native default */
 links[1]=92;assert(BtlParam_GetUnkAC(&c)==1); /* Up advances despite legacy Down default */
 links[1]=255;assert(BtlParam_GetUnkAC(&c)==3); /* explicit terminal forward link */
 p.transDefault=0;links[1]=95;assert(BtlParam_GetUnkAC(&c)==1);
 for(k=0;k<8;k++) {
  assert(ChrView_GetModelFile(91,k,0)==CHARA_MODEL_FILE(91,k));
  assert(ChrView_GetModelFile(91,k,1)==CHARA_DMG_FILE(91,k));
 }
 puts("Neutral R3 next-form and eight-costume preview routes PASS");return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 for name,files in [('packages',['port/tests/costume_routes_test.c']),('native',[str(Path(tmp)/'native.c')])]:
  if name=='native':Path(files[0]).write_text(source)
  exe=str(Path(tmp)/name)
  subprocess.run(['cc','-std=c11','-DPORT','-D_POSIX_C_SOURCE=200809L','-Iinclude','-Iport/include','-ffunction-sections','-fdata-sections',*files,'-Wl,--gc-sections','-o',exe],cwd=root,check=True)
  subprocess.run([exe],check=True)
