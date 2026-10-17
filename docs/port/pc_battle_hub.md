# PC roster, relay teams and cooperative Raid

Open **F1 → Tag Team / Modos Extra → Plantilla y combates PC**. Every installed original form or added package has an independent roster entry. Search by name or ID, select a team and member, then choose the fighter and costume. Duplicate fighters are allowed. Uninstalled packages are reserved capacity, not selectable fictional characters.

The PC battle builder starts Duel, Training, simultaneous Tag Team, Raid or ten-versus-ten relay fights from the main menu. The first two sides are distinct teams. Raid has one CPU boss and up to four human allies. The host selects the cooperative lineup. Each costume is checked against available files before starting.

## Ten-member relay selection in existing team modes

In the Duel or Ultimate Battle team selector, open F1 and build teams in **Duelo** or **Relevos 10 vs 10**. Use **Usar estos equipos en el modo actual**. Closing F1 displays both complete lineups, including all ten fighters and their costumes. Confirm the battle in the underlying selector. Its CPU difficulty and mode rules remain in force.

The battle keeps its original five native member slots. A defeated, inactive bench slot is replaced by the next member from the PC queue using the engine's `BtlMember_Replace`. The active fighter is never overwritten. The first bank and reserve queue reset before a rematch reloads models. This preserves PS2 save and replay layouts. The ten-member queue itself is a PC match configuration; it is not represented in old PS2 replay files.

Simultaneous Tag Team and Raid retain their own actor limits and do not consume a relay override. Training is one against one. Story battles keep their scripted participants. Tournament entrants are individual fighters, with the expanded character roster available in their selection screens.

## Expanded roster

Duel/Training, team selection, tournament entry/bracket and Ultimate Battle selectors use separate PC buffers. Original structures keep their binary layout. The grid builder flattens original and added transformations into independent cells and translates saved item sets back to the original grouped cell. Evolution Z retains its original customization layout; it does not write added characters into the original 97 saved customization entries.

`port/include/port_limits.h` owns `PORT_TOTAL_CHARAS` (default 290). A larger build can override it, for example `-DPORT_TOTAL_CHARAS=512`. Package IDs, aliases, virtual file namespaces and grid buffers derive from that setting. The tests build grids at 290 and 512 and verify non-overlapping virtual file ranges. Increasing a build setting still requires testing actual packages and memory usage; it does not install more fighters.

## Cooperative Raid over direct UDP

Choose **Raid cooperativo**, two to four players, one ally per player and a boss. From the main menu, **Crear sala Raid** restarts into a fresh cooperative session. Other PCs choose **Unirse a sala Raid** with the host's reachable IPv4 address and UDP port (default 29740). The selected player count is fixed for the session. Direct internet connections require the host's UDP port to be reachable; room codes and the existing two-player matchmaking relay are separate.

The session compares gameplay-data and installed-package fingerprints before accepting players. Package hashes include resolved links, stats, models, animations, effects, member blocks and replacements. The host assigns input slots and relays them to everyone. Each copy simulates the boss from the same seed and configuration. Three input frames of delay and sixteen frames of redundant input provide delivery without prediction. A missing peer times out; a detected state mismatch stops the session. There is no reconnect, host migration or rollback in this cooperative transport.

Every peer uses its own keyboard/controller 1 for its assigned player. The first implementation shares the cooperative split-screen layouts. Photo mode and changes to gameplay speed/cheats are disabled during cooperative sessions. Graphics, sound and local control bindings remain local.

## Validation

`run_modes_test.sh` runs the existing mode/asset checks, the real grid and duplicate filters, the PC configuration/request boundary, a four-process UDP session, and the actual relay/refill functions through twenty KOs. Mismatched package fingerprints are rejected. A separate Windows job executes the UDP test with native Winsock peers.

Full executable smoke-test results and deployment hashes are recorded in the workspace delivery report. Code fixtures do not certify every installed mod, every UI screen or a geographically remote four-player match.
