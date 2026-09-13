#!/usr/bin/env python3
"""Transfer libsodium to Pi using pexpect — waits for prompt after each chunk."""
import pexpect, base64, os, time, sys, subprocess

SERIAL = "/dev/ttyACM0"
BAUD = 115200
LF = "/tmp/libsodium_extracted/usr/lib/aarch64-linux-gnu/libsodium.so.23.0.2"
RB = "/tmp/libsodium.so.b64"
RX = "/tmp/libsodium.so.23"
CHUNK_SIZE = 1000
TIMEOUT = 30

if not os.path.exists(LF):
    for root, dirs, files in os.walk("/tmp/libsodium_extracted"):
        for f in files:
            if "libsodium" in f and ".so" in f:
                LF = os.path.join(root, f)
                break

def die(msg):
    print("FAILED: " + msg)
    sys.exit(1)

ls = os.path.getsize(LF)
print("Local:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
tc = (len(b64) + CHUNK_SIZE - 1) // CHUNK_SIZE
print("Chunks:", tc)

subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"0\n", check=True)
time.sleep(2)
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"1\n", check=True)
time.sleep(5)

child = pexpect.spawn("screen " + SERIAL + " " + str(BAUD), timeout=TIMEOUT, env={"TERM": "vt100"})
child.logfile = sys.stdout.buffer

child.sendline("")
idx = child.expect(["login:", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: die("NO_LOGIN")

child.sendline("pi")
idx = child.expect(["Password:", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: die("NO_PASS")

child.sendline("raspberry")
idx = child.expect([r"\$\s*$", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: die("NO_SHELL")

print("LOGGED IN")
child.sendline("rm -f " + RB)
child.expect(r"\$\s*$", timeout=TIMEOUT)

for i in range(0, len(b64), CHUNK_SIZE):
    chunk = b64[i:i+CHUNK_SIZE]
    chunk_num = i // CHUNK_SIZE + 1
    child.sendline("echo -n '" + chunk + "' >> " + RB)
    idx = child.expect([r"\$\s*$", pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1: die("TIMEOUT " + str(chunk_num))
    if chunk_num % 50 == 0: print(" ", chunk_num, "/", tc)

print("Decoding...")
child.sendline("base64 -d " + RB + " > " + RX + " && chmod +x " + RX)
child.expect(r"\$\s*$", timeout=TIMEOUT)

child.sendline("stat -c %s " + RX)
child.expect(r"\$\s*$", timeout=TIMEOUT)
o = child.before.decode("utf-8", errors="replace")
print("Stat:", o)

sizes = [l.strip() for l in o.splitlines() if l.strip().isdigit()]
rs = int(sizes[-1]) if sizes else 0

if rs == ls:
    print("SUCCESS:", rs)
    child.sendline("sudo cp " + RX + " /usr/lib/aarch64-linux-gnu/libsodium.so.23 && ldconfig")
    child.expect(r"\$\s*$", timeout=TIMEOUT)
    child.sendline("ldd /tmp/tezos_shim")
    child.expect(r"\$\s*$", timeout=TIMEOUT)
    print("ldd:", child.before.decode("utf-8", errors="replace"))
else:
    print("MISMATCH:", ls, "vs", rs)

child.close()
print("DONE")
