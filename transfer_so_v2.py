#!/usr/bin/env python3
"""Transfer libsodium to Pi using the same pexpect approach as transfer_final.py"""
import pexpect, base64, os, time, sys, subprocess

SERIAL = "/dev/ttyACM0"
BAUD = 115200

# Find the .so file
LF = None
for root, dirs, files in os.walk("/tmp/libsodium_extracted"):
    for f in files:
        if "libsodium" in f and ".so" in f:
            LF = os.path.join(root, f)
            break

if not LF:
    print("No libsodium .so found")
    sys.exit(1)

REMOTE_B64 = "/tmp/libsodium.so.b64"
REMOTE_SO = "/tmp/libsodium.so.23"
CHUNK_SIZE = 1000
TIMEOUT = 30

def die(msg):
    print("FAILED: " + msg, file=sys.stderr)
    sys.exit(1)

local_size = os.path.getsize(LF)
print("Local: {} bytes".format(local_size))

with open(LF, "rb") as f:
    b64_data = base64.b64encode(f.read()).decode("ascii")
total_chunks = (len(b64_data) + CHUNK_SIZE - 1) // CHUNK_SIZE
print("Base64: {} chars, {} chunks".format(len(b64_data), total_chunks))

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
idx = child.expect([r"\$ ", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: die("NO_SHELL")

print("LOGGED IN")

child.sendline("rm -f " + REMOTE_B64)
child.expect(r"\$ ", timeout=TIMEOUT)

for i in range(0, len(b64_data), CHUNK_SIZE):
    chunk = b64_data[i:i+CHUNK_SIZE]
    chunk_num = i // CHUNK_SIZE + 1
    child.sendline("echo -n '" + chunk + "' >> " + REMOTE_B64)
    idx = child.expect([r"\$ ", pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        die("TIMEOUT chunk {}/{}".format(chunk_num, total_chunks))
    if chunk_num % 50 == 0:
        print("  {}/{}".format(chunk_num, total_chunks))

print("All chunks sent. Decoding...")
child.sendline("base64 -d " + REMOTE_B64 + " > " + REMOTE_SO)
child.expect(r"\$ ", timeout=TIMEOUT)

child.sendline("stat -c %s " + REMOTE_SO)
child.expect(r"\$ ", timeout=TIMEOUT)
output = child.before.decode("utf-8", errors="replace")
print("Stat: " + str(output))
sizes = [l.strip() for l in output.splitlines() if l.strip().isdigit()]
remote_size = int(sizes[-1]) if sizes else 0

if remote_size == local_size:
    print("SUCCESS: {} bytes".format(remote_size))
    
    # Install with sudo — handle password prompt
    child.sendline("sudo cp " + REMOTE_SO + " /usr/lib/aarch64-linux-gnu/libsodium.so.23")
    idx = child.expect([r"\$ ", "[Pp]assword", pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:  # password prompt
        child.sendline("raspberry")
        child.expect(r"\$ ", timeout=TIMEOUT)
    elif idx == 2:
        print("WARNING: sudo timed out")
    
    child.sendline("ldconfig")
    child.expect(r"\$ ", timeout=TIMEOUT)
    
    # Test
    child.sendline("ldd /tmp/tezos_shim")
    child.expect(r"\$ ", timeout=TIMEOUT)
    print("ldd: " + child.before.decode("utf-8", errors="replace"))
else:
    die("SIZE MISMATCH: local={} remote={}".format(local_size, remote_size))

child.close()
print("DONE")
