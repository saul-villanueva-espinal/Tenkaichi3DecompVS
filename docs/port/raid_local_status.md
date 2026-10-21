# Local raid validation — 2026-10-21

The official v0.1.38 source is integrated with the existing character packages,
roster, costume routing and raid changes. Builds use the portable local LLVM 22
toolchain. This checkpoint is local only; it has not been uploaded to GitHub.

Validated: five real model previews, separate depth clearing, staggered group
framing, unobstructed footer, room-to-main-menu return through the native menu
with the user's saved boss costume and stage, and eleven-fighter battle-result
teardown. Unit harnesses for raid configuration, four/five/six-peer UDP,
relays, costume routing and battle rules pass. The 535 baseline character files
match their original SHA-256 hashes.

The full six-player raid is **not yet validated for online play**. One six-process
run passed 6,120 network ticks with all eleven fighters matching. Repetitions,
including a graphical host, still detected positional differences. Digest
validation remains enabled; do not mask the differences or describe transport
unit tests as a successful full match. BT3_COOP_TEST_SEED is an opt-in regression
seed for reproducing simulation paths; the UDP session nonce stays independent.

The ordinary authorized official v0.1.38 executable is installed separately with
clean original data for the official duel service. It successfully created a
server room. A custom build's version number does not authorize it for official
quick matchmaking. The modded raid uses its separate direct UDP connection and
requires matching builds and character data on all peers.

Detailed test paths and the next diagnostic step are recorded in
`AVANCE_JEFE_FINAL.md` in the surrounding local workspace.
