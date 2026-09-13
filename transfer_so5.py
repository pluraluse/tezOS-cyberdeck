#!/usr/bin/env python3
import serial, base64, os, time, sys

SERIAL = "/dev/ttyACM0"
BAUD = 115200
LF = "/tmp/libsodium_extracted/usr/lib/aarch64-linux-gnu/libsodium.so.23.0.2"
RB = "/tmp/libsodium.so.b64"
RX = "/tmp/libsodium.so.23"
CHUNK_SIZE = 1000
TIMEOUT = 30

# Find the .so file
if not os.path.exists(LF):
    for root, dirs, files in os.walk("/tmp/libsodium_extracted"):
        for f in files:
            if "libsodium" in f and ".so" in f:
                LF = os.path.join(root, f)
                break

def die(msg):
    print("FAILED: " + msg)
    sys.exit(1)

def read(ser, timeout=5):
    data = b""
    end = time.time() + timeout
    while time.time() < end:
        if ser.in_waiting:
            data += ser.read(ser.in_waiting)
            end = time.time() + 0.5
        else:
            time.sleep(0.1)
    return data.decode("utf-8", errors="replace")

def cmd(ser, s, wait=1):
    ser.write((s + "\r\n").encode())
    time.sleep(wait)
    return read(ser)

ls = os.path.getsize(LF)
print("Local:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
tc = (len(b64) + CHUNK_SIZE - 1) // CHUNK_SIZE
print("Chunks:", tc)

# Reset USB first
import subprocess
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"0\n", check=True)
time.sleep(2)
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"1\n", check=True)
time.sleep(5)

ser = serial.Serial(SERIAL, BAUD, timeout=1)
print("Serial opened")

# Wake up
ser.write(b"\r\n")
time.sleep(1)
text = read(ser, 3)
print("Initial:", repr(text[:100]))

# Login
text = cmd(ser, "pi", 2)
print("After user:", repr(text[:100]))

text = cmd(ser, "raspberry", 3)
print("After pass:", repr(text[:100]))

if "$" not in text and "#" not in text:
    more = read(ser, 5)
    text += more
    if "$" not in text and "#" not in text:
        die("NO_SHELL. Got: " + repr(text[:300]))

print("LOGGED IN")
cmd(ser, "rm -f " + RB, 1)

print("Starting transfer...")
for i in range(0, len(b64), CHUNK_SIZE):
    chunk = b64[i:i+CHUNK_SIZE]
    chunk_num = i // CHUNK_SIZE + 1
    cmd(ser, "echo -n '" + chunk + "' >> " + RB, 0.5)
    if chunk_num % 50 == 0:
        print(" ", chunk_num, "/", tc)

print("Decoding...")
cmd(ser, "base64 -d " + RB + " > " + RX + " && chmod +x " + RX, 3)

text = cmd(ser, "stat -c %s " + RX, 2)
print("Stat:", text)
sizes = [l.strip() for l in text.splitlines() if l.strip().isdigit()]
rs = int(sizes[-1]) if sizes else 0

if rs == ls:
    print("SUCCESS:", rs)
    cmd(ser, "sudo cp " + RX + " /usr/lib/aarch64-linux-gnu/libsodium.so.23 && ldconfig", 3)
    text = cmd(ser, "ldd /tmp/tezos_shim", 3)
    print("ldd:", text)
else:
    print("MISMATCH:", ls, "vs", rs)

ser.close()
print("DONE")
