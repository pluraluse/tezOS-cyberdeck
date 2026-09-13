#!/usr/bin/env python3
import pexpect, base64, os, time, sys, subprocess

SERIAL = "/dev/ttyACM0"
BAUD = 115200
TIMEOUT = 30

LF = "/tmp/cj/usr/lib/aarch64-linux-gnu/libcjson.so.1.7.18"
RB = "/tmp/libcjson.so.b64"
RX = "/tmp/libcjson.so.1"

ls = os.path.getsize(LF)
print("Local:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
print("Base64:", len(b64))

child = pexpect.spawn("screen " + SERIAL + " " + str(BAUD), timeout=TIMEOUT, env={"TERM": "vt100"})
child.logfile = sys.stdout.buffer

child.sendline("")
idx = child.expect(["login:", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: sys.exit("NO_LOGIN")

child.sendline("pi")
idx = child.expect(["Password:", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: sys.exit("NO_PASS")

child.sendline("raspberry")
idx = child.expect([r"\$ ", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: sys.exit("NO_SHELL")

print("LOGGED IN")

child.sendline("rm -f " + RB)
child.expect(r"\$ ", timeout=TIMEOUT)

for i in range(0, len(b64), 1000):
    chunk = b64[i:i+1000]
    child.sendline("echo -n '" + chunk + "' >> " + RB)
    child.expect(r"\$ ", timeout=TIMEOUT)
    if (i//1000+1) % 50 == 0:
        print(" ", i//1000+1, "/", (len(b64)+999)//1000)

print("Decoding...")
child.sendline("base64 -d " + RB + " > " + RX + " && chmod +x " + RX)
child.expect(r"\$ ", timeout=TIMEOUT)

child.sendline("sudo cp " + RX + " /usr/lib/aarch64-linux-gnu/libcjson.so.1 && ldconfig")
idx = child.expect([r"\$ ", "[Pp]assword", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1:
    child.sendline("raspberry")
    child.expect(r"\$ ", timeout=TIMEOUT)
elif idx == 2:
    print("WARNING: sudo timed out")

child.sendline("ldd /tmp/tezos_shim")
child.expect(r"\$ ", timeout=TIMEOUT)
print(child.before.decode("utf-8", errors="replace"))

child.close()
print("DONE")
