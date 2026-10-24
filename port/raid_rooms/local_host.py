"""Start/check this PC's private room service before launching the game.

This fixes host-local routing. It does NOT make a home connection reachable
through upstream routers or CGNAT. No firewall/router rules are modified.
"""
from pathlib import Path
import argparse
import os
import secrets
import socket
import struct
import subprocess
import sys
import time

HEADER = struct.Struct("!4sBBHI16s8sBBH")


def probe(endpoint, create=False):
    host, port = endpoint.rsplit(":", 1)
    token = secrets.token_bytes(16)
    profile = struct.pack("!QBB32s", 0, 1, 2, b"connection-check")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(.7)
        packet = HEADER.pack(b"BT3R", 1, 1 if create else 2, 0, 1, token,
                             bytes(8) if create else b"ZZZZZZ\0\0", 0, 0, len(profile)) + profile
        try:
            sock.sendto(packet, (host, int(port)))
            data, _ = sock.recvfrom(1500)
        except OSError:
            return None
        if len(data) < HEADER.size or data[:4] != b"BT3R" or data[12:28] != token:
            return None
        if not create:
            return "responding"
        if data[5] != 3:
            return None
        code = data[28:36].rstrip(b"\0").decode("ascii")
        # Delete only our short-lived diagnostic room, never another room.
        sock.sendto(HEADER.pack(b"BT3R", 1, 7, 0, 2, token,
                               data[28:36], 0, 0, 0), (host, int(port)))
        return code


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--launch-game", action="store_true")
    ap.add_argument("--foreground", action="store_true")
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[2]
    workspace = repo.parent
    game = workspace / "Tenkaichi3Decomp"
    local = "127.0.0.1:29741"
    if args.foreground:
        if probe(local):
            print("El servidor local ya esta abierto. No es necesario abrir otro.")
            return
        os.execv(sys.executable, [sys.executable, str(Path(__file__).with_name("server.py")),
                                 "--bind", "0.0.0.0", "--port", "29741"])
    if not probe(local):
        logs = workspace / "local-build-logs"
        logs.mkdir(exist_ok=True)
        with (logs / "raid-server.txt").open("a", encoding="utf-8") as log:
            subprocess.Popen([sys.executable, str(Path(__file__).with_name("server.py")),
                              "--bind", "0.0.0.0", "--port", "29741"], cwd=repo,
                             stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                             creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not probe(local):
            time.sleep(.1)
    code = probe(local, create=True)
    if not code:
        raise SystemExit("El servidor local no respondio. Revisa local-build-logs/raid-server.txt.")
    print(f"Servidor local comprobado: creo y confirmo una sala de prueba {code} (ya cerrada).")
    print("El codigo de tu partida se genera dentro de Jefe Final > Crear sala.")
    print("IMPORTANTE: conectividad desde otras casas aun requiere acceso al UDP 29741")
    print("a traves de los routers anteriores a esta PC. Esto no se habilita por iniciar Python.")
    if args.launch_game:
        if not (game / "Tenkaichi3Decomp.exe").is_file():
            raise SystemExit("No se encuentra el ejecutable del juego.")
        env = os.environ | {"BT3_ROOM_HOST_SERVER": local, "BT3_GPU_API": "gl"}
        subprocess.Popen([str(game / "Tenkaichi3Decomp.exe")], cwd=game, env=env)


if __name__ == "__main__":
    main()
