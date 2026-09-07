import pty, os, sys, time, select, subprocess
outpath = sys.argv[1]; cmds = sys.argv[2:]
# Per-command settle time (seconds). Default 4 keeps the historical pacing; set WGCONSOLE_PUMP=1 for
# long ladders of cheap commands (e.g. 61x ".eratalents learn") that would otherwise take minutes each.
PUMP = float(os.environ.get("WGCONSOLE_PUMP", "4"))
out = open(outpath, "wb", buffering=0)
master, slave = pty.openpty()
p = subprocess.Popen(["docker","attach","ac-worldserver"],
                     stdin=slave, stdout=slave, stderr=slave, close_fds=True)
os.close(slave)
def pump(secs):
    end=time.time()+secs
    while time.time()<end:
        r,_,_=select.select([master],[],[],0.3)
        if r:
            try: d=os.read(master,65536)
            except OSError: return
            if d: out.write(d)
pump(5)
for c in cmds:
    if c.startswith("wait:"):
        out.write(b"\n>>> WAIT "+c.encode()+b"\n"); pump(int(c[5:])); continue
    out.write(b"\n>>> SENDING: "+c.encode()+b"\n")
    os.write(master,(c+"\n").encode()); pump(PUMP)
pump(2)
os.write(master,b"\x10\x11"); time.sleep(1.5)
try: p.wait(timeout=6)
except Exception: p.kill()
out.write(b"\n>>> DETACHED\n"); out.close()
