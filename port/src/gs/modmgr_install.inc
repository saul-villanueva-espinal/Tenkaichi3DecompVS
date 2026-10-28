/* Manager-only operations. Original disc assets are never overwritten. */
static SDL_AtomicInt sManagerDialog;
static char sManagerPath[700];

static void SDLCALL manager_chosen(void *unused, const char *const *files, int filter) {
    (void)unused; (void)filter;
    sManagerPath[0] = 0;
    if (files && files[0] && strlen(files[0]) < sizeof(sManagerPath))
        snprintf(sManagerPath, sizeof(sManagerPath), "%s", files[0]);
    SDL_SetAtomicInt(&sManagerDialog, files == NULL ? -2 : sManagerPath[0] ? 2 : -1);
}

int Port_ManagerChoose(int folder) {
    static const SDL_DialogFileFilter filters[] = {{"Mods BT3", "zip;7z;rar;pak;bin;txt"}};
    if (Port_NetSession() || SDL_GetAtomicInt(&sManagerDialog)) return 0;
    SDL_SetAtomicInt(&sManagerDialog, 1);
    if (folder) SDL_ShowOpenFolderDialog(manager_chosen, NULL, NULL, NULL, false);
    else SDL_ShowOpenFileDialog(manager_chosen, NULL, NULL, filters, 1, NULL, false);
    return 1;
}

int Port_ManagerChosen(char *out) {
    int state = SDL_GetAtomicInt(&sManagerDialog);
    if (state == 1 || state == 0) return 0;
    if (state == 2) snprintf(out, 700, "%s", sManagerPath);
    SDL_SetAtomicInt(&sManagerDialog, 0);
    return state == 2 ? 1 : state;
}

static int manager_paths(int i, char *manifest, char *backup) {
    char root[300], dir[440];
    if (!valid(i) || Port_NetSession() || !PortCharas_Root(root, sizeof(root))) return 0;
    snprintf(dir, sizeof(dir), "%s/%s", root, sLib[i].folder);
    if (path_kind(dir) != 2 || !in_folder(dir, root)) return 0;
    snprintf(manifest, 600, "%s/character.txt", dir);
    snprintf(backup, 600, "%s/.manager-destination.bak", dir);
    return !is_link(manifest) && !is_link(backup);
}

/* Only a package's destination is restored; unrelated mods and their order stay intact. */
int Port_ManagerBackup(int i) {
    char manifest[600], backup[600];
    return manager_paths(i, manifest, backup) && path_kind(backup) == 1;
}

static int manager_restore(const char *manifest, const unsigned char *snapshot, long n) {
    if (n < 8 || memcmp(snapshot, "MMD1", 4)) return 0;
    if (!snapshot[4]) return !exists(manifest) || remove(manifest) == 0;
    return put_file(manifest, snapshot + 8, n - 8);
}

int Port_ManagerDestination(int i, int isNew, int target) {
    char manifest[600], backup[600], number[24], folder[128];
    unsigned char *before, *snapshot;
    long n = 0;
    int had, ok, k, on;
    if (target < 0 || target >= DISC || !manager_paths(i, manifest, backup)) return 0;
    had = path_kind(manifest) == 1;
    before = had ? read_file(manifest, &n, 65536) : NULL;
    if (had && (!before && n != 0)) return 0;
    snapshot = calloc(1, (size_t)n + 8);
    if (!snapshot) { free(before); return 0; }
    memcpy(snapshot, "MMD1", 4); snapshot[4] = (unsigned char)had;
    snapshot[5] = (unsigned char)sLib[i].on;
    if (n) memcpy(snapshot + 8, before, (size_t)n);
    free(before);
    if (!put_file(backup, snapshot, n + 8)) { free(snapshot); return 0; }
    snprintf(folder, sizeof(folder), "%s", sLib[i].folder);
    on = sLib[i].on;
    snprintf(number, sizeof(number), "%d", target);
    ok = PortCharas_ManifestSet(folder, "base", number);
    if (ok) ok = PortCharas_ManifestSet(folder, "replaces", isNew ? NULL : number);
    if (!ok) manager_restore(manifest, snapshot, n + 8);
    free(snapshot);
    changed();
    if (!ok) return 0;
    /* Promote through the normal toggle path so replacement conflicts are resolved. */
    for (k = 0; k < sLibCount; k++) if (!strcmp(sLib[k].folder, folder)) {
        if (on && sLib[k].on) Port_ModToggle(k);
        if (!sLib[k].on) Port_ModToggle(k);
        return 1;
    }
    return 0;
}

int Port_ManagerRestore(int i) {
    char manifest[600], backup[600], folder[128];
    unsigned char *snapshot;
    long n = 0;
    int ok, on, k;
    if (!manager_paths(i, manifest, backup)) return 0;
    snapshot = read_file(backup, &n, 65544);
    if (!snapshot) return 0;
    if(n<8 || memcmp(snapshot,"MMD1",4)) {free(snapshot);return 0;}
    snprintf(folder,sizeof(folder),"%s",sLib[i].folder);
    on=snapshot[5];
    ok = manager_restore(manifest, snapshot, n);
    free(snapshot);
    if (ok) {
        remove(backup); changed();
        for(k=0;k<sLibCount;k++)if(!strcmp(folder,sLib[k].folder)) {
            if(sLib[k].on)Port_ModToggle(k);
            if(on)Port_ModToggle(k);
            break;
        }
    }
    return ok;
}

extern int Port_ManagerMapCount(void);
extern void Port_ManagerMapInfo(int i, char *name, char *file);
int Port_ManagerMapPicture(int i, void *out, int size) {
    char name[64], path[512], *dot;
    unsigned char *raw, *body;
    long n=0, bytes=0;
    int k;
    if(i<0 || i>=Port_ManagerMapCount() || size<16)return 0;
    Port_ManagerMapInfo(i,name,path);dot=strrchr(path,'.');
    if(!dot)return 0;
    strcpy(dot,".thumb");
    raw=read_file(path,&n,0x3B800);
    if(!raw){fprintf(stderr,"bt3: mod manager: cannot read stage preview %s\n",path);return 0;}
    body=bpe_unpack(raw,n,size,&bytes);free(raw);
    if(!body){fprintf(stderr,"bt3: mod manager: cannot decode stage preview %s (%ld bytes)\n",path,n);return 0;}
    /* Native preview: eight indexed textures; validate before relocation. */
    if(bytes<16 || rd32(body)!=8) {free(body);return 0;}
    {
        unsigned at=rd32(body+4)*4u;
        if(at<16 || at+8*0x40u>(unsigned long)bytes) {free(body);return 0;}
        for(k=0;k<8;k++) {
            unsigned char *t=body+at+k*0x40;
            unsigned pix=rd32(t)*4u,clut=rd32(t+4)*4u;
            if(pix+0x60u+128*256>(unsigned long)bytes || clut+0x60u+1024>(unsigned long)bytes) {free(body);return 0;}
        }
    }
    memcpy(out,body,(size_t)bytes);free(body);return (int)bytes;
}
