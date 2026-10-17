/* Included by ui.cpp, on the render thread. Engine work is queued to the game
   thread; the modern roster does not write into legacy menu/save structures. */
#include "../../include/port_pc_battle.h"
#include "../../include/port_coop.h"
extern "C" volatile int gPortPcHubReady, gPortPcTeamReady, gPortPcTeamDp;
static void pc_battle_ui(void) {
    static PcBattle b;
    static bool ready;
    static int side, slot, port = 29740;
    static char address[200] = "127.0.0.1", message[200];
    static ImGuiTextFilter search;
    const char *const modes[] = {"Duelo", "Entrenamiento", "Tag Team simultaneo", "Raid cooperativo", "Relevos 10 vs 10"};
    const char *const costumes[] = {"Traje 1","Traje 2","Traje 3","Traje 4","Traje 5","Traje 6","Traje 7","Traje 8"};
    if (!ready) {
        Port_PcBattleDefault(&b);
        if (getenv("BT3_UI_PC_TEAMS")) {
            b.mode = PC_RELAY; b.count[0] = b.count[1] = 10;
            for (int s = 0; s < 2; s++) for (int i = 0; i < 10; i++) b.fighter[s][i].chara = s ? 79 : 109;
        }
        ready = true;
    }
    ImGui::SeparatorText("Plantilla y combates PC");
    ImGui::Text("%d personajes/formas instalados / %d casillas", Port_PcRosterCount(), PORT_TOTAL_CHARAS);
    ImGui::TextDisabled("Las casillas restantes se habilitan al instalar paquetes.");
    if (ImGui::Combo("Modo", &b.mode, modes, PC_MODE_COUNT)) {
        b.players = 1;
        b.count[0] = b.mode == PC_RELAY ? 10 : 1;
        b.count[1] = b.mode == PC_RELAY ? 10 : 1;
        side = slot = 0;
    }
    int maxPlayers = b.mode == PC_TRAINING ? 1 : b.mode == PC_TAG || b.mode == PC_RAID ? 4 : 2;
    ImGui::SliderInt("Jugadores", &b.players, 1, maxPlayers);
    if (b.mode == PC_TAG) {
        if (b.players >= 3 && b.count[0] < 2) b.count[0] = 2;
        if (b.players >= 4 && b.count[1] < 2) b.count[1] = 2;
    }
    if (b.mode == PC_RAID && b.count[0] < b.players) b.count[0] = b.players;
    ImGui::SliderInt("Escenario (ID)", &b.stage, 0, 34);
    if (b.mode == PC_RELAY) ImGui::TextWrapped("Diez por equipo, por relevos. Puedes repetir personajes y elegir un traje por integrante.");
    if (ImGui::BeginTable("pc_teams", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
        for (int s = 0; s < 2; s++) {
            ImGui::TableNextColumn(); ImGui::PushID(s);
            ImGui::TextUnformatted(b.mode == PC_RAID ? s ? "JEFE CPU" : "ALIADOS" : s ? "EQUIPO 2" : "EQUIPO 1");
            int max = (b.mode == PC_RELAY || b.mode == PC_DUEL) ? 10 : b.mode == PC_TAG ? 5 : b.mode == PC_RAID && !s ? 4 : 1;
            if (max > 1) ImGui::SliderInt("Integrantes", &b.count[s], b.mode == PC_RAID && !s ? b.players : 1, max);
            for (int i = 0; i < b.count[s]; i++) {
                ImGui::PushID(i);
                const char *name = Port_PcRosterName(b.fighter[s][i].chara);
                char label[140]; snprintf(label, sizeof(label), "%02d  %s  [T%d]", i + 1, name && *name ? name : "Personaje", b.fighter[s][i].costume + 1);
                if (ImGui::Selectable(label, side == s && slot == i)) { side = s; slot = i; }
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (slot >= b.count[side]) slot = b.count[side] - 1;
    ImGui::Text("Editando %s, integrante %d", side ? "equipo 2" : "equipo 1", slot + 1);
    ImGui::Combo("Traje", &b.fighter[side][slot].costume, costumes, 8);
    ImGui::TextDisabled("Cada traje necesita sus archivos; se comprueban antes de iniciar.");
    search.Draw("Buscar personaje o forma");
    ImGui::BeginChild("pc_roster", ImVec2(0, 190), ImGuiChildFlags_Borders);
    for (int i = 0; i < Port_PcRosterCount(); i++) {
        int c = Port_PcRosterId(i); const char *name = Port_PcRosterName(c);
        char label[160]; snprintf(label, sizeof(label), "%03d  %s", c, name && *name ? name : "Personaje");
        if (!search.PassFilter(label)) continue;
        ImGui::PushID(c);
        if (ImGui::Selectable(label, b.fighter[side][slot].chara == c)) {
            b.fighter[side][slot].chara = c; b.fighter[side][slot].costume = 0;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (gPortPcTeamDp > 0) ImGui::Text("Limite DP del modo actual: %d por equipo", gPortPcTeamDp);
    ImGui::BeginDisabled(!gPortPcTeamReady || Port_ModesFight() != PORT_MODE_NONE || (b.mode != PC_RELAY && b.mode != PC_DUEL) || Port_NetActive() || Port_CoopEnabled());
    if (ImGui::Button("Usar estos equipos en el modo actual")) {
        if (!Port_PcBattleValidate(&b, message, sizeof(message))) {}
        else if (Port_PcTeamsRequest(&b)) snprintf(message, sizeof(message), "Equipos guardados para este selector. Confirma el combate para aplicarlos.");
        else snprintf(message, sizeof(message), "El equipo supera el limite DP o el modo no admite relevos.");
    }
    ImGui::EndDisabled();
    if (gPortPcTeamReady) ImGui::TextWrapped("Puedes aplicar hasta diez integrantes por equipo en Duelo y Ultimate Battle. Entraran por relevos; se mantienen las reglas de este modo.");
    ImGui::BeginDisabled(!gPortPcHubReady || Port_NetActive() || Port_CoopEnabled());
    if (ImGui::Button("Iniciar combate local")) {
        if (!Port_PcBattleValidate(&b, message, sizeof(message))) {}
        else if (Port_PcBattleRequest(&b)) snprintf(message, sizeof(message), "Combate preparado. Cierra F1 para jugar.");
        else snprintf(message, sizeof(message), "Vuelve al menu principal para iniciar.");
    }
    if (b.mode == PC_RAID && b.players >= 2) {
        ImGui::SeparatorText("Sala Raid online (UDP directo / LAN)");
        ImGui::InputText("Direccion del anfitrion", address, sizeof(address));
        ImGui::InputInt("Puerto", &port);
        ImGui::TextWrapped("Cada jugador abre su juego. El anfitrion elige el jefe. Al unirte, se usa tu primer aliado; todos necesitan los mismos datos y paquetes. La sala inicia cuando se conectan los jugadores elegidos.");
        if (ImGui::Button("Crear sala Raid")) {
            if (!Port_CoopLaunch(1, address, port, &b)) snprintf(message, sizeof(message), "No se pudo crear la sala: revisa personajes, trajes y puerto.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Unirse a sala Raid")) {
            if (!Port_CoopLaunch(0, address, port, &b)) snprintf(message, sizeof(message), "No se pudo iniciar la conexion.");
        }
    }
    ImGui::EndDisabled();
    if (!gPortPcHubReady) ImGui::TextDisabled("Abre este panel desde el menu principal para iniciar un combate.");
    if (message[0]) ImGui::TextWrapped("%s", message);
    ImGui::Spacing();
}

static void pc_teams_preview(void) {
    if (!gPortPcTeamReady) return;
    PcBattle b;
    bool pending = Port_PcTeamsPreview(&b) != 0;
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * .5f, io.DisplaySize.y * .97f), ImGuiCond_Always, ImVec2(.5f,1));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * .88f, 0));
    ImGui::SetNextWindowBgAlpha(.94f);
    if (ImGui::Begin("Equipos PC##preview", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
        if (!pending) ImGui::TextUnformatted("F1: plantilla ampliada, trajes y equipos de hasta diez por relevos");
        else {
            ImGui::TextUnformatted("EQUIPOS PC POR RELEVOS - confirma el combate en el selector para aplicarlos");
            if (ImGui::BeginTable("preview_teams",2,ImGuiTableFlags_SizingStretchSame)) {
                for (int s=0;s<2;s++) {
                    ImGui::TableNextColumn();ImGui::Text("Equipo %d - %d integrantes",s+1,b.count[s]);
                    for (int i=0;i<b.count[s];i++) {
                        const char *name=Port_PcRosterName(b.fighter[s][i].chara);
                        ImGui::Text("%02d  %s  [T%d]",i+1,name&&*name?name:"Personaje",b.fighter[s][i].costume+1);
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
}
