"""Room membership, invitation retries, data isolation and expiration."""
import importlib.util
from pathlib import Path
import sys
import unittest

spec = importlib.util.spec_from_file_location("raid_room_server", Path(__file__).resolve().parents[1] / "raid_rooms/server.py")
server = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = server
spec.loader.exec_module(server)


class DirectoryTests(unittest.TestCase):
    def setUp(self):
        self.directory = server.Directory()
        self.host = ("127.0.0.1", 31000)
        self.token = b"h" * 16
        self.profile = server.PROFILE.pack(123, 7, 6, b"SAUL")
        self.create = server.pack(server.CREATE, 1, self.token, payload=self.profile)
        response = self.directory.handle(self.create, self.host, 10)[0][1]
        self.code = server.unpack(response)[3]

    def join(self, slot, fingerprint=123, code=None):
        token = bytes([slot]) * 16
        profile = server.PROFILE.pack(fingerprint, 7, 6, b"AMIGO")
        request = server.pack(server.JOIN, slot, token, code or self.code, payload=profile)
        response = self.directory.handle(request, ("127.0.0.1", 31000 + slot), 11)
        return server.unpack(response[0][1])

    def test_create_retry_keeps_invitation(self):
        reply = self.directory.handle(self.create, self.host, 11)[0][1]
        self.assertEqual(server.unpack(reply)[3], self.code)
        self.assertEqual(len(self.directory.rooms), 1)

    def test_six_members_and_full_room(self):
        for slot in range(1, 6):
            reply = self.join(slot)
            self.assertEqual(reply[0], server.ACCEPT)
            self.assertEqual(reply[-1], bytes((slot, 6)))
            self.assertEqual(self.join(slot)[-1], bytes((slot, 6)))
        self.assertEqual(self.join(6)[0], server.ERROR)

    def test_mismatched_mods_and_missing_code(self):
        self.assertIn(b"mods", self.join(1, fingerprint=456)[-1])
        self.assertEqual(self.join(1, code=b"ZZZZZZ")[0], server.ERROR)
        self.assertEqual(len(self.directory.rooms[self.code].peers), 1)

    def test_authenticated_relay_and_forgery_rejection(self):
        self.join(1)
        raw = b"actual BT3 input datagram"
        data = server.pack(server.DATA, 0, bytes([1]) * 16, self.code, 1, 0, raw)
        replies = self.directory.handle(data, ("127.0.0.1", 31001), 12)
        self.assertEqual(replies[0][0], self.host)
        parsed = server.unpack(replies[0][1])
        self.assertEqual(parsed[2], self.token)
        self.assertEqual(parsed[-1], raw)
        self.assertEqual(self.directory.handle(data, ("127.0.0.1", 32000), 12), [])
        forged = server.pack(server.DATA, 0, b"x" * 16, self.code, 1, 0, raw)
        self.assertEqual(self.directory.handle(forged, ("127.0.0.1", 31001), 12), [])
        # Guests communicate with the authoritative host, never each other.
        direct = server.pack(server.DATA, 0, bytes([1]) * 16, self.code, 1, 2, raw)
        self.assertEqual(self.directory.handle(direct, ("127.0.0.1", 31001), 12), [])

    def test_room_isolation_and_expiration(self):
        self.join(1)
        other_token = b"o" * 16
        other = server.pack(server.CREATE, 1, other_token, payload=self.profile)
        response = self.directory.handle(other, ("127.0.0.1", 32000), 11)[0][1]
        other_code = server.unpack(response)[3]
        data = server.pack(server.DATA, 0, self.token, other_code, 0, 1, b"x")
        self.assertEqual(self.directory.handle(data, self.host, 12), [])
        self.directory.expire(72)
        self.assertEqual(self.directory.rooms, {})

    def test_malformed_datagrams_and_capacity(self):
        for data in (b"", b"BT3R", self.create[:-1], self.create + b"x", bytes(2000)):
            self.assertEqual(self.directory.handle(data, self.host, 12), [])
        bad = server.PROFILE.pack(123, 7, 7, b"name")
        request = server.pack(server.CREATE, 2, b"b" * 16, payload=bad)
        self.assertEqual(server.unpack(self.directory.handle(request, self.host, 12)[0][1])[0], server.ERROR)

    def test_host_closes_room_for_all(self):
        self.join(1)
        close = server.pack(server.LEAVE, 2, self.token, self.code)
        replies = self.directory.handle(close, self.host, 12)
        self.assertNotIn(self.code, self.directory.rooms)
        self.assertEqual(replies[0][0], ("127.0.0.1", 31001))
        self.assertEqual(server.unpack(replies[0][1])[0], server.ERROR)

    def test_six_player_host_input_budget(self):
        self.join(1)
        packet = server.pack(server.DATA, 0, self.token, self.code, 0, 1, bytes(400))
        # One host forwards 6 input sources to 5 guests, 60 times per second.
        for _ in range(1800):
            self.assertEqual(len(self.directory.handle(packet, self.host, 12)), 1)

    def test_resume_host_and_guest_after_process_restart(self):
        self.join(1)
        for slot, token, address in ((0, self.token, ("127.0.0.1", 41000)),
                                     (1, bytes([1]) * 16, ("127.0.0.1", 41001))):
            request = server.pack(server.RESUME, 9, token, self.code, slot, payload=self.profile)
            reply = self.directory.handle(request, address, 12)[0][1]
            self.assertEqual(server.unpack(reply)[0], server.ACCEPT)
            self.assertEqual(server.unpack(reply)[3], self.code)
            self.assertEqual(self.directory.rooms[self.code].peers[slot].address, address)
        self.assertEqual(len(self.directory.rooms), 1)
        wrong = server.pack(server.RESUME, 9, b"x" * 16, self.code, 0, payload=self.profile)
        self.assertEqual(server.unpack(self.directory.handle(wrong, self.host, 12)[0][1])[0], server.ERROR)


if __name__ == "__main__":
    unittest.main()
