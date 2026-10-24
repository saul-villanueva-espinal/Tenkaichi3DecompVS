"""Native input sync under relay latency/loss, guest loss and cancellation."""
from pathlib import Path
import argparse
import heapq
import os
import random
import shutil
import socket
import subprocess
import tempfile
import threading
import time

root = Path(__file__).resolve().parents[2]
os.environ['PATH'] = str(root.parent/'local-toolchain/ucrt64/bin') + os.pathsep + os.environ['PATH']
parser = argparse.ArgumentParser()
parser.add_argument('--public', help='Optional actual relay host:port, not a simulation')
args = parser.parse_args()

class Proxy:
    def __init__(self, remote):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('127.0.0.1', 0))
        self.sock.settimeout(.002)
        self.endpoint = '127.0.0.1:' + str(self.sock.getsockname()[1])
        self.remote = remote
        self.stop = False
        self.thread = threading.Thread(target=self.loop)
        self.thread.start()

    def loop(self):
        queue, clients, seq = [], {}, 0
        rng = random.Random(20261023)
        while not self.stop:
            try:
                data, sender = self.sock.recvfrom(2048)
                token = data[12:28]
                if sender == self.remote:
                    target = clients.get(token)
                else:
                    clients[token] = sender
                    target = self.remote
                # About 100 ms RTT to relay, ~200 ms client->host->client.
                # Lose occasional INPUT datagrams, never CREATE/session control.
                coop_input = len(data)>52 and data[48:52] == (5).to_bytes(4,'big')
                if target and not (coop_input and rng.randrange(200)==0):
                    seq += 1
                    heapq.heappush(queue, (time.monotonic()+.05+rng.uniform(0,.01),seq,target,data))
            except (socket.timeout, ConnectionResetError):
                pass
            while queue and queue[0][0] <= time.monotonic():
                _,_,target,data = heapq.heappop(queue)
                self.sock.sendto(data,target)

    def close(self):
        self.stop=True
        self.thread.join(timeout=2)
        self.sock.close()

def run(exe, count, endpoint, option=None):
    base = {k:v for k,v in os.environ.items() if not k.startswith(('BT3_COOP','BT3_ROOM','FIXTURE'))}
    config = [3,count,0,count,1]+sum(([i*2 if side==0 else 4,0] for side in range(2) for i in range(10)),[])
    config += [125,150,150,2,25,162500,200,50,30,14,134,2]
    base.update(BT3_ROOM_SERVER=endpoint,BT3_ROOM_HOST_SERVER=endpoint,
                BT3_COOP_CONFIG=','.join(map(str,config)),FIXTURE_RESTART='1',FIXTURE_PACE_MS='10')
    if option:base[option]='1'
    peers=[]
    start=time.monotonic()
    try:
        host=subprocess.Popen([exe,'--relay-setup'],env=base|{'FIXTURE_HOST':'1'},stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        peers.append(host)
        line=host.stdout.readline().strip()
        assert line.startswith('CODE='),line
        code=line[5:]
        for player in range(1,count):
            p=subprocess.Popen([exe,'--relay-setup',code],env=base|{'FIXTURE_HERO':str(player*2)},stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            peers.append(p)
            assert p.stdout.readline().strip()==f'JOINED={player}'
        play_started=time.monotonic()
        for player,p in enumerate(peers):
            out,err=p.communicate(timeout=40)
            assert p.returncode==0,(player,out,err)
            expected=('intentional guest departure' if option=='FIXTURE_DROP_GUEST' and player==count-1 else
                      'cancellation recovered' if option=='FIXTURE_CANCEL_WAIT' else
                      'remaining peers continued' if option=='FIXTURE_DROP_GUEST' else 'distinct pads match')
            assert expected in out,(player,out,err)
        duration=time.monotonic()-start
        if not option: assert time.monotonic()-play_started<12, f'300 paced ticks too slow: {duration:.2f}s'
        print(f'coop_resilience: {count} clients, {option or "latency/loss"}, {duration:.2f}s PASS',flush=True)
    finally:
        for p in peers:
            if p.poll() is None:
                if os.name=='nt':subprocess.run(['taskkill','/PID',str(p.pid),'/T','/F'],capture_output=True)
                else:p.kill()
                p.communicate(timeout=5)

with tempfile.TemporaryDirectory() as tmp:
    exe=str(Path(tmp)/('peer.exe' if os.name=='nt' else 'peer'))
    subprocess.run([shutil.which('cc') or shutil.which('gcc'),'-std=c11','-D_POSIX_C_SOURCE=200809L','-Iport/include',
                    'port/tests/coop_peer_test.c','port/src/gs/coop_net.c','port/src/gs/coop_rooms.c',
                    'port/src/plat_pc_battle.c','port/src/plat_tournament.c','-lpthread','-o',exe]+
                   (['-lws2_32'] if os.name=='nt' else []),cwd=root,check=True)
    service=None
    if args.public:
        host,port=args.public.rsplit(':',1)
        remote=(socket.gethostbyname(host),int(port))
    else:
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
            s.bind(('127.0.0.1',0));port=s.getsockname()[1]
        remote=('127.0.0.1',port)
        service=subprocess.Popen([shutil.which('python') or 'python','port/raid_rooms/server.py','--port',str(port)],cwd=root,stdout=subprocess.PIPE,text=True)
        assert 'listening' in service.stdout.readline()
    try:
        for count,option in ((2,None),(6,None),(4,'FIXTURE_DROP_GUEST'),(4,'FIXTURE_CANCEL_WAIT')):
            proxy=Proxy(remote) if not args.public else None
            try:run(exe,count,proxy.endpoint if proxy else f'{remote[0]}:{remote[1]}',option)
            finally:
                if proxy:proxy.close()
    finally:
        if service:service.terminate();service.wait(timeout=5)
