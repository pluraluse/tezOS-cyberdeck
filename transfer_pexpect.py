#!/usr/bin/env python3
import pexpect
import base64
import sys
import os

SERIAL_DEVICE = '/dev/ttyACM0'
BAUD = 115200
LOCAL_FILE = '/home/qwen/projects/tezos-cyberdeck/src/shim-linux/tezos_shim_arm64'
REMOTE_B64 = '/tmp/tezos_shim.b64'
REMOTE_BIN = '/tmp/tezos_shim'
CHUNK_SIZE = 1000
TIMEOUT = 30

def die(msg):
    print("FAILED: " + msg, file=sys.stderr)
    sys.exit(1)

local_size = os.path.getsize(LOCAL_FILE)
print("Local file: " + LOCAL_FILE + " (" + str(local_size) + " bytes)")

with open(LOCAL_FILE, 'rb') as f:
    b64_data = base64.b64encode(f.read()).decode('ascii')
total_chunks = (len(b64_data) + CHUNK_SIZE - 1) // CHUNK_SIZE
print("Base64-encoded: " + str(len(b64_data)) + " chars, " + str(total_chunks) + " chunks")

child = pexpect.spawn('screen ' + SERIAL_DEVICE + ' ' + str(BAUD), timeout=TIMEOUT)
child.logfile = sys.stdout.buffer

try:
    child.sendline('')
    idx = child.expect(['login:', pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        die("timed out waiting for login prompt")

    child.sendline('pi')
    idx = child.expect(['Password:', pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        die("timed out waiting for password prompt")

    child.sendline('raspberry')
    idx = child.expect([r'\$\s*$', pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        die("timed out waiting for shell prompt after login -- wrong password or login failed")

    print("Logged in. Starting transfer...")

    child.sendline('rm -f ' + REMOTE_B64)
    child.expect(r'\$\s*$', timeout=TIMEOUT)

    for i in range(0, len(b64_data), CHUNK_SIZE):
        chunk = b64_data[i:i+CHUNK_SIZE]
        chunk_num = i // CHUNK_SIZE + 1
        child.sendline("echo -n '" + chunk + "' >> " + REMOTE_B64)
        idx = child.expect([r'\$\s*$', pexpect.TIMEOUT], timeout=TIMEOUT)
        if idx == 1:
            die("timed out on chunk " + str(chunk_num) + "/" + str(total_chunks))
        if chunk_num % 50 == 0:
            print("  " + str(chunk_num) + "/" + str(total_chunks) + " chunks sent")

    print("All chunks sent. Decoding on the Pi...")
    child.sendline('base64 -d ' + REMOTE_B64 + ' > ' + REMOTE_BIN + ' && chmod +x ' + REMOTE_BIN)
    idx = child.expect([r'\$\s*$', pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        die("timed out waiting for base64 decode to complete")

    child.sendline('stat -c %s ' + REMOTE_BIN)
    idx = child.expect([r'\$\s*$', pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        die("timed out waiting for remote file size check")

    output = child.before.decode('utf-8', errors='replace')
    print("Remote stat output: " + str(output))

    remote_size_str = [line.strip() for line in output.splitlines() if line.strip().isdigit()]
    if not remote_size_str:
        die("could not parse remote file size from output: " + repr(output))
    remote_size = int(remote_size_str[-1])

    if remote_size == local_size:
        print("SUCCESS: remote file size (" + str(remote_size) + ") matches local (" + str(local_size) + ") exactly.")
    else:
        die("SIZE MISMATCH: local=" + str(local_size) + " bytes, remote=" + str(remote_size) + " bytes")

finally:
    child.close()
