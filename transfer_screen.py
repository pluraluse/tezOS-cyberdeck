#!/usr/bin/env python3
import pexpect, base64, os, subprocess, sys

SERIAL = "/dev/ttyACM1"
LF = "/home/qwen/projects/tezos-cyberdeck/src/shim-linux/tezos_shim_arm64"
RB = "/tmp/tezos_shim.b64"
RX = "/tmp/tezos_shim"
cs = 1000
t = 30

def die(msg):
    print("FAILED: " + msg)
    sys.exit(1)

ls = os.path.getsize(LF)
print("Local:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
tc = (len(b64) + cs - 1) // cs
print("Chunks:", tc)

child = pexpect.spawn("screen " + SERIAL + " 115200", timeout=t, env={"TERM": "vt100"})
child.logfile = sys.stdout.buffer

child.sendline("")
i = child.expect(["login:", pexpect.TIMEOUT], timeout=t)
if i == 1: die("NO_LOGIN")

child.sendline("pi")
i = child.expect(["Password:", pexpect.TIMEOUT], timeout=t)
if i == 1: die("NO_PASS")

child.sendline("raspberry")
i = child.expect([r"\$ ", pexpect.TIMEOUT], timeout=t)
if i == 1: die("NO_SHELL")

print("LOGGED IN")
child.sendline("rm -f " + RB)
child.expect(r"\$ ", timeout=t)

for i in range(0, len(b64), cs):
    chunk = b64[i:i+cs]
    child.sendline("echo -n '" + chunk + "' >> " + RB)
    j = child.expect([r"\$ ", pexpect.TIMEOUT], timeout=t)
    if j == 1: die("TIMEOUT chunk " + str(i//cs+1) + "/" + str(tc))
    if (i//cs+1) % 50 == 0: print(" ", i//cs+1, "/", tc)

print("Decoding...")
child.sendline("base64 -d " + RB + " > " + RX + " && chmod +x " + RX)
child.expect(r"\$ ", timeout=t)

child.sendline("stat -c %s " + RX)
child.expect(r"\$ ", timeout=t)
o = child.before.decode("utf-8", errors="replace")
sizes = [l.strip() for l in o.splitlines() if l.strip().isdigit()]
rs = int(sizes[-1]) if sizes else 0

if rs == ls: print("SUCCESS:", rs)
else: print("MISMATCH:", ls, "vs", rs)

child.close()
print("DONE")
