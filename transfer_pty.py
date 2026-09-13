#!/usr/bin/env python3
import pexpect, base64, sys, os, subprocess

SERIAL_DEVICE = "/dev/ttyACM0"
BAUD = 115200
LOCAL_FILE = "/home/qwen/projects/tezos-cyberdeck/src/shim-linux/tezos_shim_arm64"
REMOTE_B64 = "/tmp/tezos_shim.b64"
REMOTE_BIN = "/tmp/tezos_shim"
CHUNK_SIZE = 1000
TIMEOUT = 30

def die(msg):
    print("FAILED: " + msg, file=sys.stderr)
    sys.exit(1)

local_size = os.path.getsize(LOCAL_FILE)
print("Local file: " + LOCAL_FILE + " (" + str(local_size) + " bytes)")

with open(LOCAL_FILE, "rb") as f:
    b64_data = base64.b64encode(f.read()).decode("ascii")
total_chunks = (len(b64_data) + CHUNK_SIZE - 1) // CHUNK_SIZE
print("Base64: " + str(len(b64_data)) + " chars, " + str(total_chunks) + " chunks")

fd = os.open(SERIAL_DEVICE, os.O_RDWR | os.O_NOCTTY)
subprocess.run(["stty", "-F", SERIAL_DEVICE, "115200", "raw", "-echo"])
child = pexpect.fdpexpect.fdspawn(fd, timeout=TIMEOUT)
child.logfile = sys.stdout.buffer

child.sendline("")
idx = child.expect(["login:", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1:
    die("timed out waiting for login prompt")

child.sendline("pi")
idx = child.expect(["Password:", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1:
    die("timed out waiting for password prompt")

child.sendline("raspberry")
idx = child.expect([r"\$\s*$", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1:
    die("timed out waiting for shell prompt")

print("Logged in. Starting transfer...")
child.sendline("rm -f " + REMOTE_B64)
child.expect(r"\$\s*$", timeout=TIMEOUT)

for i in range(0, len(b64_data), CHUNK_SIZE):
    chunk = b64_data[i:i+CHUNK_SIZE]
    chunk_num = i // CHUNK_SIZE + 1
    child.sendline("echo -n  + chunk +  >> " + REMOTE_B64)
    idx = child.expect([r"\$\s*$", pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        die("timed out on chunk " + str(chunk_num) + "/" + str(total_chunks))
    if chunk_num % 50 == 0:
        print("  " + str(chunk_num) + "/" + str(total_chunks))

print("All chunks sent. Decoding...")
child.sendline("base64 -d " + REMOTE_B64 + " > " + REMOTE_BIN + " && chmod +x " + REMOTE_BIN)
child.expect(r"\$\s*$", timeout=TIMEOUT)

child.sendline("stat -c %s " + REMOTE_BIN)
child.expect(r"\$\s*$", timeout=TIMEOUT)
output = child.before.decode("utf-8", errors="replace")
sizes = [l.strip() for l in output.splitlines() if l.strip().isdigit()]
remote_size = int(sizes[-1]) if sizes else 0

if remote_size == local_size:
    print("SUCCESS: remote=" + str(remote_size) + " matches local=" + str(local_size))
else:
    die("MISMATCH: local=" + str(local_size) + " remote=" + str(remote_size))
child.close()
print("DONE")
