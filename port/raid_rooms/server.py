"""Private BT3 Raid room directory and UDP relay. No official service credentials.

Default is loopback for development. Public use requires an explicitly configured
reachable server and UDP firewall rule; running this on a player's PC alone does
not make that PC reachable through CGNAT. See README.md.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import secrets
import socket
import struct
import time

HEADER = struct.Struct("!4sBBHI16s8sBBH")
PROFILE = struct.Struct("!QBB32s")
MAGIC, VERSION = b"BT3R", 1
CREATE, JOIN, ACCEPT, DATA, PING, PONG, LEAVE, ERROR, RESUME = range(1, 10)
MAX_PAYLOAD = 1360
ALPHABET = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ"


def pack(op, request=0, token=bytes(16), code=b"", source=0, target=0, payload=b""):
    return HEADER.pack(MAGIC, VERSION, op, 0, request, token,
                       code.ljust(8, b"\0"), source, target, len(payload)) + payload


def unpack(data):
    if len(data) < HEADER.size or len(data) > HEADER.size + MAX_PAYLOAD:
        return None
    magic, version, op, reserved, request, token, code, source, target, size = HEADER.unpack_from(data)
    if magic != MAGIC or version != VERSION or reserved or size != len(data) - HEADER.size:
        return None
    return op, request, token, code.rstrip(b"\0"), source, target, data[HEADER.size:]


@dataclass
class Peer:
    address: tuple
    token: bytes
    name: str
    seen: float
    window: float = 0
    packets: int = 0


@dataclass
class Room:
    fingerprint: int
    protocol: int
    capacity: int
    created: float
    peers: dict[int, Peer] = field(default_factory=dict)


class Directory:
    def __init__(self, max_rooms=256, ttl=60):
        self.rooms: dict[bytes, Room] = {}
        self.max_rooms, self.ttl = max_rooms, ttl
        self.rates = {}

    def expire(self, now):
        for code, room in list(self.rooms.items()):
            # Host loss closes the room; never silently elect another host.
            if now - room.peers[0].seen > self.ttl or now - room.created > 43200:
                del self.rooms[code]
        self.rates = {ip: value for ip, value in self.rates.items() if now - value[0] < 10}

    def handle(self, data, address, now=None):
        """Return bounded (address, datagram) replies. Transport-independent for tests."""
        now = time.monotonic() if now is None else now
        parsed = unpack(data)
        if not parsed:
            return []
        op, request, token, code, source, target, payload = parsed
        if not any(token):
            return []
        # Bound unauthenticated work and reflected errors per IP.
        stamp, count = self.rates.get(address[0], (now, 0))
        if now - stamp >= 1:
            stamp, count = now, 0
        self.rates[address[0]] = stamp, count + 1
        if count >= 32768:
            return []

        def error(text):
            # Never reply with an amplification payload to a tiny datagram.
            msg = text.encode("utf-8")[:min(120, len(payload))]
            return [(address, pack(ERROR, request, token, code, payload=msg))]

        def accepted(room, slot):
            return [(address, pack(ACCEPT, request, token, code, slot,
                                   payload=bytes((slot, room.capacity))))]

        if op in (CREATE, JOIN, RESUME):
            if len(payload) != PROFILE.size:
                return []
            fingerprint, protocol, capacity, raw_name = PROFILE.unpack(payload)
            name = raw_name.split(b"\0", 1)[0].decode("utf-8", errors="replace")
            if op == CREATE:
                # Retried create requests must return the SAME room/code.
                for existing, room in self.rooms.items():
                    host = room.peers[0]
                    if host.token == token:
                        host.address = address
                        code = existing
                        host.seen = now
                        return accepted(room, 0)
                if not 2 <= capacity <= 6 or not protocol:
                    return error("Capacidad no valida")
                if len(self.rooms) >= self.max_rooms or sum(
                    r.peers[0].address[0] == address[0] for r in self.rooms.values()
                ) >= 16:
                    return error("Servicio ocupado; reintenta")
                while True:
                    code = "".join(secrets.choice(ALPHABET) for _ in range(6)).encode("ascii")
                    if code not in self.rooms:
                        break
                room = Room(fingerprint, protocol, capacity, now)
                room.peers[0] = Peer(address, token, name, now)
                self.rooms[code] = room
                return accepted(room, 0)
            room = self.rooms.get(code)
            if not room:
                return error("Sala inexistente o cerrada")
            if room.protocol != protocol:
                return error("Versiones diferentes")
            if room.fingerprint != fingerprint:
                return error("Los mods no coinciden")
            if op == RESUME:
                peer = room.peers.get(source)
                if not peer or peer.token != token:
                    return error("Sesion de sala no valida")
                peer.address, peer.seen = address, now
                return accepted(room, source)
            for slot, peer in room.peers.items():
                if peer.token == token:
                    if peer.address != address:
                        return []
                    peer.seen = now
                    return accepted(room, slot)
            free = next((i for i in range(1, room.capacity) if i not in room.peers), None)
            if free is None:
                return error("Sala llena")
            room.peers[free] = Peer(address, token, name, now)
            return accepted(room, free)

        room = self.rooms.get(code)
        if not room:
            return error("Sala cerrada")
        peer = room.peers.get(source)
        if not peer or peer.token != token or peer.address != address:
            return []
        peer.seen = now
        if now - peer.window >= 1:
            peer.window, peer.packets = now, 0
        peer.packets += 1
        # The host forwards all six input streams to five guests (up to 1800
        # packets/s at 60 Hz), so a one-client 1024 limit would stall a full raid.
        if peer.packets > 4096:
            return []
        if op == PING:
            return [(address, pack(PONG, request, token, code, source))]
        if op == LEAVE:
            if source == 0:
                del self.rooms[code]
                return [(p.address, pack(ERROR, 0, p.token, code, payload=b"El anfitrion cerro la sala"))
                        for slot, p in room.peers.items() if slot != 0]
            # Do not recycle a departed player's slot during an active battle.
            # The game detects missing input and returns everyone to the menu.
            return []
        if op != DATA or not payload or (source != 0 and target != 0):
            return []
        other = room.peers.get(target)
        if not other or now - other.seen > self.ttl:
            return []
        return [(other.address, pack(DATA, 0, other.token, code, source, target, payload))]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=29741)
    ap.add_argument("--max-rooms", type=int, default=256)
    args = ap.parse_args()
    directory = Directory(args.max_rooms)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((args.bind, args.port))
        sock.settimeout(.5)
        if hasattr(socket, "SIO_UDP_CONNRESET"):
            try:
                sock.ioctl(socket.SIO_UDP_CONNRESET, False)
            except OSError:
                pass
        print(f"BT3 Raid rooms listening on {args.bind}:{sock.getsockname()[1]}", flush=True)
        try:
            while True:
                directory.expire(time.monotonic())
                try:
                    data, address = sock.recvfrom(2048)
                except (socket.timeout, ConnectionResetError, OSError):
                    continue
                try:
                    replies = directory.handle(data, address)
                except Exception:
                    continue
                for destination, reply in replies:
                    try:
                        sock.sendto(reply, destination)
                    except (ConnectionResetError, OSError):
                        pass
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
