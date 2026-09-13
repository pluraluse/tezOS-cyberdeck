#!/usr/bin/env python3
import pexpect
import base64
import sys
import os
import time
import subprocess

SERIAL_DEVICE = "/dev/ttyACM0"
BAUD = 115200
LOCAL_FILE = "/tmp/libsodium_extracted/usr/lib/aarch64-linux-gnu/libsodium.so.23.0.2"
if not os.path.exists(LOCAL_FILE):
    for root, dirs, files in os.walk("/tmp/libsodium_extracted"):
        for f in files:
            if "libsodium" in f and ".so" in f:
                LOCAL_FILE = os.path.join(root, f)
                break

REMOTE_B64 = "/tmp/libsodium.so.b64"
REMOTE_SO = "/tmp/libsodium.so.23"
CHUNK_SIZE = 500
TIMEOUT = 60

def die(msg):
    print("FAILED: " + msg, file=sys.stderr)
    sys.exit(1)

local_size = os.path.getsize(LOCAL_FILE)
print("Local: {} bytes".format(local_size))

with open(LOCAL_FILE, "rb") as f:
    b64_data = base64.b64encode(f.read()).decode("ascii")
total_chunks = (len(b64_data) + CHUNK_SIZE - 1) // CHUNK_SIZE
print("Base64: {} chars, {} chunks".format(len(b64_data), total_chunks))

# Reset USB first
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"0\n", check=True)
time.sleep(2)
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"1\n", check=True)
time.sleep(5)

# Wait for serial device
for _ in range(10):
    if os.path.exists(SERIAL_DEVICE):
        break
    time.sleep(2)

child = pexpect.spawn("screen " + SERIAL_DEVICE + " " + str(BAUD), timeout=TIMEOUT, env={"TERM": "vt100"})
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
    child.sendline("echo -n '" + chunk + "' >> " + REMOTE_B64)
    idx = child.expect([r"\$\s*$", pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        die("timed out on chunk {}/{}".format(chunk_num, total_chunks))
    if chunk_num % 50 == 0:
        print("  {}/{} chunks sent".format(chunk_num, total_chunks))

print("All chunks sent. Decoding...")
child.sendline("base64 -d " + REMOTE_B64 + " > " + REMOTE_SO + " && chmod +x " + REMOTE_SO)
idx = child.expect([r"\$\s*$", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1:
    die("timed out waiting for decode")

child.sendline("stat -c %s " + REMOTE_SO)
idx = child.expect([r"\$\s*$", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1:
    die("timed out waiting for stat")

output = child.before.decode("utf-8", errors="replace")
print("Stat output: " + str(output))
sizes = [l.strip() for l in output.splitlines() if l.strip().isdigit()]
if not sizes:
    die("could not parse size from: " + repr(output))
remote_size = int(sizes[-1])

if remote_size == local_size:
    print("SUCCESS: {} bytes matches".format(remote_size))
    # Install
    child.sendline("sudo cp " + REMOTE_SO + " /usr/lib/aarch64-linux-gnu/libsodium.so.23 && ldconfig")
    idx = child.expect([r"\$\s*$", pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        print("WARNING: install timed out")
else:
    die("SIZE MISMATCH: local={} remote={}".format(local_size, remote_size))

child.close()
print("DONE")
