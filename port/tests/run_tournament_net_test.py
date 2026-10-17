"""Run every bracket round over the actual UDP transport, with 4/8/16 peers."""
from pathlib import Path
import os,shutil,socket,subprocess,tempfile,time
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    exe=str(Path(tmp)/('tourney.exe' if os.name=='nt' else 'tourney'))
    subprocess.run([shutil.which('cc') or shutil.which('gcc'),'-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra',
                    '-Iport/include','port/tests/tournament_peer_test.c','port/src/gs/coop_net.c','port/src/plat_pc_battle.c',
                    'port/src/plat_tournament.c','-lpthread']+(['-lws2_32'] if os.name=='nt' else [])+['-o',exe],cwd=root,check=True)
    for count in [4,8,16]:
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
            sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        base={k:v for k,v in os.environ.items() if not k.startswith('BT3_COOP') and not k.startswith('BT3_TOURNAMENT')}
        peers=[]
        try:
            for i in range(count):
                config=[0,2,4,1,1]+sum(([i*2 if s==0 else 4,0] for s in range(2) for j in range(10)),[])
                env=base|{'BT3_TOURNAMENT':str(count),'BT3_COOP_CONFIG':','.join(map(str,config)),
                          'BT3_COOP_HOST' if i==0 else 'BT3_COOP_JOIN':str(port) if i==0 else f'127.0.0.1:{port}'}
                peers.append(subprocess.Popen([exe],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True))
                if i==0:time.sleep(.1)
            champions=[]
            for p in peers:
                out,err=p.communicate(timeout=90)
                assert p.returncode==0,out+err
                assert 'spectators isolated' in out,out+err
                champions.append(out.split('champion ')[-1].split(',')[0])
            assert len(set(champions))==1,champions
            print(f'tournament_test: {count} UDP peers, complete bracket, role changes, duplicate result guard and spectator isolation pass')
        finally:
            for p in peers:
                if p.poll() is None:p.kill();p.communicate()
