#!/usr/bin/env python3
import serial
import base64
import os
import time
import sys

SERIAL = "/dev/ttyACM1"
BAUD = 115200
LF = "/home/qwen/projects/tezos-cyberdeck/src/shim-linux/tezos_shim_arm64"
RB = "/tmp/tezos_shim.b64"
RX = "/tmp/tezos_shim"
CHUNK_SIZE = 1000
TIMEOUT = 30

def die(msg):
    print("FAILED: " + msg)
    sys.exit(1)

def read(ser, timeout=5):
    """Read all available data from serial with timeout."""
    data = b""
    end = time.time() + timeout
    while time.time() < end:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting)
            data += chunk
            end = time.time() + 0.5  # extend timeout if data is coming
        else:
            time.sleep(0.1)
    return data.decode("utf-8", errors="replace")

def waitfor(ser, patterns, timeout=30):
    """Wait for one of the patterns, return (index, buffer)."""
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        if ser.in_waiting:
            buf += ser.read(ser.in_waiting)
            text = buf.decode("utf-8", errors="replace")
            for i, pat in enumerate(patterns):
                if pat in text:
                    return i, text
        time.sleep(0.1)
    return -1, buf.decode("utf-8", errors="replace")

def cmd(ser, s, wait=1):
    """Send command and read response."""
    ser.write((s + "\r\n").encode())
    time.sleep(wait)
    return read(ser)

ls = os.path.getsize(LF)
print("Local:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
tc = (len(b64) + CHUNK_SIZE - 1) // CHUNK_SIZE
print("Chunks:", tc)

ser = serial.Serial(SERIAL, BAUD, timeout=1)
print("Serial opened")

# Wake up console
ser.write(b"\r\n")
time.sleep(1)
text = read(ser, 3)
print("Initial:", repr(text[:200]))

# Send username
text = cmd(ser, "pi", 2)
print("After user:", repr(text[:200]))

# Send password
text = cmd(ser, "raspberry", 3)
print("After pass:", repr(text[:200]))

# Check for shell prompt
if "$" not in text and "#" not in text:
    # Maybe need to wait more
    more = read(ser, 5)
    text += more
    if "$" not in text and "#" not in text:
        die("NO_SHELL. Got: " + repr(text[:500]))

print("LOGGED IN")

# Clear any partial file
cmd(ser, "rm -f " + RB, 1)

# Transfer
print("Starting transfer...")
for i in range(0, len(b64), CHUNK_SIZE):
    chunk = b64[i:i+CHUNK_SIZE]
    chunk_num = i // CHUNK_SIZE + 1
    cmd(ser, "echo -n '" + chunk + "' >> " + RB, 0.5)
    if chunk_num % 50 == 0:
        print(" ", chunk_num, "/", tc)
        # Read any prompt output
        text = read(ser, 0.5)

print("All chunks sent. Decoding...")
cmd(ser, "base64 -d " + RB + " > " + RX + " && chmod +x " + RX, 3)

# Verify
text = cmd(ser, "stat -c %s " + RX, 2)
print("Stat:", repr(text))
sizes = [l.strip() for l in text.splitlines() if l.strip().isdigit()]
rs = int(sizes[-1]) if sizes else 0

if rs == ls:
    print("SUCCESS:", rs, "matches", ls)
else:
    print("MISMATCH:", ls, "vs", rs)

ser.close()
print("DONE")
