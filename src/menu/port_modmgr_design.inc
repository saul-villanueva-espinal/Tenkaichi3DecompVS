/* Approved aqua library, destination review and install pages. The original
   roster/forms editors remain available through the library menu. */
#define MM_MAPS 10
#define MM_INSTALL 11
#define MM_DESTINATION 12
extern s32 Port_ManagerChoose(s32 folder), Port_ManagerChosen(char *out);
extern s32 Port_ManagerDestination(s32 i, s32 isNew, s32 target);
extern s32 Port_ManagerBackup(s32 i), Port_ManagerRestore(s32 i);
extern s32 Port_ManagerMapCount(void);
extern void Port_ManagerMapInfo(s32 i, char *name, char *file);
extern s32 Port_ManagerMapPicture(s32 i, void *out, s32 size);
extern volatile s32 gPortNativeRosterActive;
extern s32 Port_RosterPointerRead(s32 *x, s32 *y);
extern s32 Mx_TabAt(f32 x, f32 y);

static s32 sMdFocus, sMdRow, sMdTarget, sMdNew, sMdConfirm, sMdMap, sMdMapTop;
static s32 sMdDialog, sMdImport, sMdImported, sMdPictureOf = -1;
static MmRes *sMdPicture;
static char sMdMapName[64], sMdMapFile[512];

static void Md_Button(f32 x0, f32 y, f32 x1, char *label, s32 selected, s32 enabled) {
    Mx_Row(x0, y, x1, y + 42, selected && enabled, enabled ? 128 : 68);
    Mx_Text((x0+x1)*0.5f, y+21, label, 23, x1-x0-30,
            selected && enabled ? MX_GOLD : enabled ? MX_LAV : MX_DIM, 1);
}

static void Md_InstallBox(f32 y) {
    Mx_Window(968, y, 1348, y+180);
    Mx_Text(990, y+27, "INSTALAR MOD", 26, 330, MX_GOLD, 0);
    Md_Button(986,y+52,1330,"Seleccionar archivo",sMdFocus && sMdRow==3,1);
    Md_Button(986,y+104,1330,"Seleccionar carpeta",sMdFocus && sMdRow==4,1);
    Mx_Text(1158,y+162,"Nuevo slot / Reemplazar",18,330,MX_WHITE,1);
}

static void Md_Info(f32 rise) {
    PortModInfo *m = &sMm->info[sMm->cur];
    char line[96];
    Mx_Window(968,140+rise,1348,482+rise);
    Mx_Text(990,170+rise,m->name,29,336,MX_GOLD,0);
    Mx_Text(990,212+rise,"TIPO",18,90,MX_CYAN,0);
    Mx_Text(1092,212+rise,m->isNew ? "Personaje nuevo" : "Reemplazo",22,235,MX_WHITE,0);
    Mx_Text(990,247+rise,"DESTINO",18,96,MX_CYAN,0);
    Mx_Text(1092,247+rise,m->target,22,235,MX_WHITE,0);
    if (Mm_Broken(m)) sprintf(line,"Revisar archivos del mod");
    else if (m->conflict && !m->wins) sprintf(line,"Otro mod ocupa este destino");
    else sprintf(line,m->on ? "ACTIVADO" : "DESACTIVADO");
    Mx_Text(990,282+rise,line,21,330,Mm_Broken(m) || m->conflict ? MX_ORANGE : MX_GOLD,0);
    sprintf(line,"<  Traje %d / %d  >",(int)sMm->costume+1,(int)(m->costumes > 0 ? m->costumes : 1));
    Mx_Bar(MM_NAVBAR,986,302+rise,1330,337+rise,128);
    Mx_Text(1158,320+rise,line,23,320,MX_WHITE,1);
    Md_Button(986,348+rise,1330,"Cambiar destino",sMdFocus && sMdRow==0,1);
    Md_Button(986,394+rise,1330,m->on ? "Desactivar" : "Activar",sMdFocus && sMdRow==1,1);
    Md_Button(986,440+rise,1330,"Restaurar respaldo",sMdFocus && sMdRow==2,Port_ManagerBackup(sMm->cur));
    Md_InstallBox(504+rise);
    Mx_Text(785,651,m->white[0] ? m->white : m->name,25,340,MX_DARK,1);
    Mx_Text(785,679,"L2 / R2: traje",18,340,MX_DARK,1);
}

static void Md_OpenDestination(s32 imported) {
    PortModInfo *m=&sMm->info[sMm->cur];
    sMdTarget=m->chara >= 0 ? m->chara : 0;
    sMdNew=m->isNew; sMdFocus=0; sMdRow=0; sMdConfirm=0; sMdImported=imported;
    Mm_SetMode(MM_DESTINATION);
}

static void Md_PictureClear(void) {
    if(sMdPicture) { Heap_Free(sMdPicture); sMdPicture=NULL; }
    sMdPictureOf=-1;
}

static void Md_MapsDraw(void) {
    s32 i, n=Port_ManagerMapCount(); char name[64],file[512],line[96];
    Mx_Text(72,122,"MAPAS INSTALADOS",26,500,MX_DARK,0);
    for(i=0;i<7 && i+sMdMapTop<n;i++) {
        Port_ManagerMapInfo(i+sMdMapTop,name,file);
        Mx_Row(62,150+i*72,595,214+i*72,i+sMdMapTop==sMdMap,128);
        sprintf(line,"%02d",i+sMdMapTop+1);
        Mx_Text(87,182+i*72,line,24,48,MX_GOLD,0);
        Mx_Text(148,172+i*72,name,24,415,MX_LAV,0);
        Mx_Text(148,197+i*72,"Slot adicional",17,390,MX_LAV,0);
    }
    if(n) {
        Mx_Window(630,156,1348,580);
        if(sMdPicture) {
            s32 k;
            for(k=0;k<sMdPicture->count && k<8;k++) {
                MmTex *t=&sMdPicture->tex[k];
                f32 x=652+(k%4)*168.0f,y=177+(k/4)*168.0f;
                Mx_Tex(t,x,y,x+168,y+168,0,0,128,256,128,128);
            }
        }
        Mx_Text(990,547,sMdMapName,31,650,MX_GOLD,1);
        Mx_Text(650,616,"Disponible en la seleccion de escenarios",25,680,MX_DARK,0);
        Mx_Text(650,653,"Los mapas originales se conservan",22,680,MX_DARK,0);
    } else {
        Mx_Window(62,230,1348,480);
        Mx_Text(688,320,"No hay mapas adicionales instalados",32,1000,MX_GOLD,1);
    }
    sprintf(line,"%d mapas adicionales",n);
    Mx_Text(328,673,line,25,500,MX_DARK,1);
}

static void Md_InstallDraw(void) {
    Mx_Window(62,150,595,690);
    Mx_Text(92,188,"INSTALAR UN PERSONAJE",30,475,MX_GOLD,0);
    Mx_Text(92,239,"1 Archivo   >   2 Destino   >   3 Confirmar",21,475,MX_WHITE,0);
    Md_Button(86,286,570,"Seleccionar archivo",sMdRow==0, !sMdDialog);
    Md_Button(86,344,570,"Seleccionar carpeta",sMdRow==1, !sMdDialog);
    Md_Button(86,402,570,"Mods retirados / Restaurar",sMdRow==2, !sMdDialog);
    Mx_Text(92,492,"Paquetes BT3 y archivos ZIP / 7Z / RAR",22,470,MX_WHITE,0);
    Mx_Text(92,533,"Tambien puedes arrastrar una carpeta",22,470,MX_WHITE,0);
    Mx_Text(92,563,"o un archivo a esta ventana.",22,470,MX_WHITE,0);
    Mx_Text(92,632,sMdDialog ? "Esperando seleccion de Windows..." : "Despues elegiras su destino en el roster",21,470,MX_GOLD,0);
    Mx_Window(968,156,1348,492);
    Mx_Text(990,191,"TU ELIGES DONDE VA",27,330,MX_GOLD,0);
    Mx_Text(990,252,"NUEVO SLOT",24,330,MX_GOLD,0);
    Mx_Text(990,289,"Agrega un personaje al roster",21,330,MX_WHITE,0);
    Mx_Text(990,350,"REEMPLAZAR",24,330,MX_GOLD,0);
    Mx_Text(990,387,"Elige el personaje de destino",21,330,MX_WHITE,0);
    Mx_Text(990,456,"Con respaldo de la configuracion",20,330,MX_WHITE,0);
}

static void Md_DestinationDraw(void) {
    PortModInfo *m=&sMm->info[sMm->cur];
    char name[64],line[96]; s32 i,top=(sMdTarget/6)*6;
    Mx_Text(72,122,"2 DESTINO   >   3 CONFIRMAR",25,600,MX_DARK,0);
    Md_Button(62,151,595,"< Nuevo slot / Reemplazar >",sMdFocus==0 && sMdRow==0,1);
    for(i=0;i<6 && top+i<MM_CHARA_COUNT;i++) {
        s32 ref=top+i; f32 y=211+i*71;
        Port_ModCharaName(ref,name,sizeof(name));
        Mx_Row(62,y,595,y+63,ref==sMdTarget && (!sMdFocus),128);
        Mx_FaceCell(ref,86,y+7,136,y+56,0);
        Mx_Text(158,y+31,name,25,405,MX_LAV,0);
    }
    sprintf(line,"%d / %d    L2 / R2: pagina",sMdTarget+1,MM_CHARA_COUNT);
    Mx_Text(328,668,line,23,520,MX_DARK,1);
    Mx_Window(968,150,1348,690);
    Mx_Text(990,181,"RESUMEN",29,330,MX_GOLD,0);
    Mx_Text(990,229,m->name,26,330,MX_WHITE,0);
    Mx_Text(990,278,"ACCION",19,330,MX_CYAN,0);
    Mx_Text(990,308,sMdNew ? "Anadir en nuevo slot" : "Reemplazar personaje",24,330,MX_WHITE,0);
    Port_ModCharaName(sMdTarget,name,sizeof(name));
    Mx_Text(990,355,sMdNew ? "BASE DE MOVIMIENTOS" : "PERSONAJE DE DESTINO",19,330,MX_CYAN,0);
    Mx_Text(990,386,name,25,330,MX_WHITE,0);
    Mx_FaceCell(sMdTarget,1000,411,1070,477,0);
    Mx_Text(1151,445,">",30,70,MX_GOLD,1);
    Mx_FaceCell(PORT_REF_MOD+sMm->cur,1230,411,1300,477,0);
    Mx_Text(990,512,"Copia de seguridad automatica",21,330,MX_GOLD,0);
    Mx_Text(990,548,Mm_Broken(m) ? "Archivos incompatibles: revisar" : "Paquete BT3 reconocido",21,330,Mm_Broken(m)?MX_RED:MX_WHITE,0);
    Md_Button(986,582,1330,sMdConfirm ? "CONFIRMAR CAMBIO" : sMdNew ? "INSTALAR NUEVO SLOT" : "INSTALAR REEMPLAZO",sMdFocus==1 && sMdRow==0,!Mm_Broken(m));
    Md_Button(986,634,1330,"Cancelar",sMdFocus==1 && sMdRow==1,1);
    Mx_Text(785,660,m->white[0] ? m->white : m->name,24,340,MX_DARK,1);
}

static void Md_Draw(void) {
    if(sMm->mode==MM_MAPS) Md_MapsDraw();
    else if(sMm->mode==MM_INSTALL) Md_InstallDraw();
    else if(sMm->mode==MM_DESTINATION) Md_DestinationDraw();
}

static void Md_Tab(s32 direction) {
    s32 tab=sMm->mode==MM_MAPS ? 1 : sMm->mode==MM_INSTALL ? 2 : 0;
    tab=(tab+3+direction)%3;
    sMdFocus=sMdRow=sMdConfirm=0;
    Mm_SetMode(tab==0 ? MM_LIBRARY : tab==1 ? MM_MAPS : MM_INSTALL);
}

static void Md_Choose(s32 folder) {
    if(Port_ManagerChoose(folder)) sMdDialog=1;
    else Mm_Note("Ya hay un selector de archivos abierto");
}

static void Md_Update(s32 live) {
    Pad *p=&gPad[0]; s32 back=p->gamePressed & (PADG_TRIANGLE|PADG_CIRCLE);
    s32 cross=p->gamePressed & PADG_CROSS;
    s32 down=p->gameRepeat & (PADG_DOWN|PADG_DOWN2), up=p->gameRepeat & (PADG_UP|PADG_UP2);
    s32 right=p->gamePressed & (PADG_RIGHT|PADG_RIGHT2),left=p->gamePressed & (PADG_LEFT|PADG_LEFT2);
    char path[700];
    if(sMdDialog) {
        s32 result=Port_ManagerChosen(path);
        if(result) { sMdDialog=0; if(result==1) {sMdImport=1;Mm_StartImport(path,0,3);} }
        live=0;
    }
    if(sMm->mode==MM_DESTINATION) {
        PortModInfo *m=&sMm->info[sMm->cur];
        sMm->wantKey=10000+sMm->cur*8+sMm->costume;
        sMm->wantChara=m->chara; sMm->wantCostume=sMm->costume; sMm->wantPreview=sMm->cur;
        Mm_SetModel(785,602,423);
        Mm_Prompt(0,"Confirmar");Mm_Prompt(2,"Volver");Mm_Prompt(3,"Destino");
        if(!live)return;
        if(back) {
            if(sMdConfirm) sMdConfirm=0;
            else { Mm_SetMode(MM_LIBRARY);sMdFocus=sMdRow=0;if(sMdImported)Mm_Note("Mod guardado desactivado; puedes configurarlo despues"); }
            return;
        }
        if(p->gamePressed&PADG_SQUARE) {sMdNew=!sMdNew;sMdConfirm=0;}
        if(left || right) {sMdFocus=!sMdFocus;sMdRow=sMdFocus?0:1;sMdConfirm=0;}
        if(!sMdFocus) {
            if(down){if(sMdRow==0)sMdRow=1;else sMdTarget=(sMdTarget+1)%MM_CHARA_COUNT;sMdConfirm=0;}
            if(up){if(sMdRow==1 && sMdTarget==0)sMdRow=0;else sMdTarget=(sMdTarget+MM_CHARA_COUNT-1)%MM_CHARA_COUNT;sMdConfirm=0;}
            if(p->gamePressed&PADG_R2)sMdTarget=(sMdTarget+6)%MM_CHARA_COUNT;
            if(p->gamePressed&PADG_L2)sMdTarget=(sMdTarget+MM_CHARA_COUNT-6)%MM_CHARA_COUNT;
            if(cross){if(!sMdRow)sMdNew=!sMdNew;else{sMdFocus=1;sMdRow=0;}sMdConfirm=0;}
        } else {
            if(down||up){sMdRow=!sMdRow;sMdConfirm=0;}
            if(cross) {
                if(sMdRow) {Mm_SetMode(MM_LIBRARY);sMdFocus=sMdRow=0;if(sMdImported)Mm_Note("Mod guardado desactivado; puedes configurarlo despues");}
                else if(!Mm_Broken(m)) {
                    if(!sMdConfirm) {sMdConfirm=1;Mm_Note("Pulsa confirmar otra vez para aplicar el destino");}
                    else {
                        Mm_Unshow();
                        if(Port_ManagerDestination(sMm->cur,sMdNew,sMdTarget)) {
                            Mm_Refresh();Mm_FreeFaces();Mm_SetMode(MM_LIBRARY);sMdFocus=0;sMdRow=0;
                            Mm_Note("Destino aplicado. Respaldo disponible.");
                        } else Mm_Note("No se pudo guardar el cambio; destino anterior conservado");
                        sMdConfirm=0;
                    }
                }
            }
        }
        return;
    }
    sMm->wantKey=-1;
    if(sMm->mode==MM_INSTALL && sMm->viewCount) {
        sMm->wantKey=10000+sMm->cur*8+sMm->costume;
        sMm->wantChara=sMm->info[sMm->cur].chara;
        sMm->wantCostume=sMm->costume;sMm->wantPreview=sMm->cur;
        Mm_SetModel(785,602,423);
    }
    Mm_Prompt(0,"Confirmar");Mm_Prompt(2,"Volver");Mm_Prompt(3,"Navegar");
    if(sMm->mode==MM_MAPS) {
        s32 n=Port_ManagerMapCount();
        if(live && n) {if(down)sMdMap=(sMdMap+1)%n;if(up)sMdMap=(sMdMap+n-1)%n;}
        if(sMdMap<sMdMapTop)sMdMapTop=sMdMap;
        if(sMdMap>=sMdMapTop+7)sMdMapTop=sMdMap-6;
        if(n && sMdPictureOf!=sMdMap) {
            Md_PictureClear();sMdPictureOf=sMdMap;
            Port_ManagerMapInfo(sMdMap,sMdMapName,sMdMapFile);
            sMdPicture=Heap_Alloc(0x50000,64,0,2);
            if(sMdPicture && Port_ManagerMapPicture(sMdMap,sMdPicture,0x50000)>0)
                Res_RelocateOffsets((void *)&sMdPicture,(u8 *)sMdPicture,(void *)sMdPicture);
            else if(sMdPicture){Heap_Free(sMdPicture);sMdPicture=NULL;Mm_Note("No se pudo leer la vista previa del mapa");}
            else Mm_Note("No hay memoria para la vista previa del mapa");
        }
    } else if(sMm->mode==MM_INSTALL && live) {
        if(down)sMdRow=(sMdRow+1)%3;
        if(up)sMdRow=(sMdRow+2)%3;
        if(cross){if(sMdRow<2)Md_Choose(sMdRow);else Mm2_OpenRemoved();}
    }
    if(!live)return;
    if(back){sMdFocus=sMdRow=0;Mm_SetMode(MM_LIBRARY);}
    else if(p->gamePressed&PADG_R1)Md_Tab(1);
    else if(p->gamePressed&PADG_L1)Md_Tab(-1);
}

/* Return 1 when the redesigned page consumed this frame's input. */
static s32 Md_LibraryInput(s32 live) {
    Pad *p=&gPad[0];
    if(sMm->confirm || sMm->menu || sMm->act || sMm->finding)return 0;
    if(!live)return 0;
    if(p->gamePressed&PADG_R1){Md_Tab(1);return 1;}
    if(p->gamePressed&PADG_L1){Md_Tab(-1);return 1;}
    if(p->gamePressed&PADG_R3){sMdFocus=!sMdFocus;sMdRow=0;return 1;}
    if(!sMdFocus)return 0;
    if(p->gamePressed&(PADG_CIRCLE|PADG_TRIANGLE)){sMdFocus=0;return 1;}
    if(p->gameRepeat&(PADG_DOWN|PADG_DOWN2))sMdRow=(sMdRow+1)%5;
    if(p->gameRepeat&(PADG_UP|PADG_UP2))sMdRow=(sMdRow+4)%5;
    if(p->gamePressed&PADG_CROSS) {
        if(sMdRow>=3) {Mm_SetMode(MM_INSTALL);Md_Choose(sMdRow==4);sMdRow=0;}
        else if(sMm->viewCount) {
            if(sMdRow==0)Md_OpenDestination(0);
            else if(sMdRow==1)Lib_Toggle();
            else if(Port_ManagerBackup(sMm->cur)) {
                Mm_Unshow();
                if(Port_ManagerRestore(sMm->cur)){Mm_Refresh();Mm_FreeFaces();Mm_Note("Destino anterior restaurado");}
                else Mm_Note("No se pudo restaurar el respaldo");
            }
        }
    }
    return 1;
}

static void Md_Pointer(s32 live) {
    s32 ix,iy,k; f32 x,y; Pad *p=&gPad[0];
    if(!Port_RosterPointerRead(&ix,&iy) || !live || sMdDialog)return;
    if(sMm->confirm || sMm->menu || sMm->act || sMm->finding)return;
    x=ix*(1376.0f/512.0f); y=iy*(768.0f/448.0f);
    if(sMm->mode!=MM_LIBRARY && sMm->mode!=MM_MAPS && sMm->mode!=MM_INSTALL && sMm->mode!=MM_DESTINATION)return;
    if(y>=33 && y<=76 && sMm->mode!=MM_DESTINATION) {
        k=Mx_TabAt(x,y);
        if(k>=0){sMdFocus=sMdRow=0;Mm_SetMode(k==0?MM_LIBRARY:k==1?MM_MAPS:MM_INSTALL);return;}
    }
    if(sMm->mode==MM_LIBRARY) {
        if(x>=62 && x<=595 && y>=150 && y<646) {
            k=sMm->top+(s32)((y-150)/72);
            if(k<sMm->viewCount){Mm_Select(sMm->view[k]);sMdFocus=0;}
        } else if(x>=986 && x<=1330) {
            if(y>=302 && y<=337){Lib_Costume(x<1158?-1:1);return;}
            if(y>=348 && y<482){sMdFocus=1;sMdRow=(s32)((y-348)/46);p->gamePressed|=PADG_CROSS;}
            if(y>=556 && y<=650){sMdFocus=1;sMdRow=y<608?3:4;p->gamePressed|=PADG_CROSS;}
        }
    } else if(sMm->mode==MM_INSTALL && x>=86 && x<=570 && y>=286 && y<444) {
        sMdRow=(s32)((y-286)/58);p->gamePressed|=PADG_CROSS;
    } else if(sMm->mode==MM_MAPS && x>=62 && x<=595 && y>=150 && y<646) {
        k=sMdMapTop+(s32)((y-150)/72);if(k<Port_ManagerMapCount())sMdMap=k;
    } else if(sMm->mode==MM_DESTINATION) {
        if(x>=62 && x<=595) {
            if(y>=151 && y<=193){sMdNew=!sMdNew;sMdConfirm=0;}
            if(y>=211 && y<637){k=(sMdTarget/6)*6+(s32)((y-211)/71);if(k<MM_CHARA_COUNT){sMdTarget=k;sMdConfirm=0;sMdFocus=0;sMdRow=1;}}
        } else if(x>=986 && x<=1330 && y>=582 && y<=676){sMdFocus=1;sMdRow=y<627?0:1;p->gamePressed|=PADG_CROSS;}
    }
}
