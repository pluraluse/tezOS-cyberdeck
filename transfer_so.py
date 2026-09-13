#!/usr/bin/env python3
"""Transfer just libsodium.so.23 to the Pi."""
import serial, base64, os, time, sys, subprocess

SERIAL = "/dev/ttyACM0"
BAUD = 115200

# Extract just the .so file from the .deb
subprocess.run([
    "dpkg", "-x",
    "/var/cache/apt/archives/libsodium23_1.0.18-1+deb13u1_arm64.deb",
    "/tmp/libsodium_extracted"
], check=True)

LF = "/tmp/libsodium_extracted/usr/lib/aarch64-linux-gnu/libsodium.so.23.0.2"
if not os.path.exists(LF):
    # Find the actual file
    for root, dirs, files in os.walk("/tmp/libsodium_extracted"):
        for f in files:
            if "libsodium" in f and ".so" in f:
                LF = os.path.join(root, f)
                break

RB = "/tmp/libsodium.so.b64"
RX = "/tmp/libsodium.so.23"
cs = 1000

ls = os.path.getsize(LF)
print("Local library file:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
tc = (len(b64) + cs - 1) // cs
print("Chunks:", tc)

ser = serial.Serial(SERIAL, BAUD, timeout=1)
ser.write(b"\r\nls -la /tmp/libsodium.so.23 2>/dev/null; rm -f /tmp/libsodium.so.b64\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

for i in range(0, len(b64), cs):
    chunk = b64[i:i+cs]
    ser.write(b"echo -n '" + chunk.encode() + b"' >> /tmp/libsodium.so.b64\r\n")
    time.sleep(0.5)
    if (i//cs+1) % 50 == 0:
        print(" ", i//cs+1, "/", tc)
        ser.read(ser.in_waiting)

print("Decoding...")
ser.write(b"base64 -d /tmp/libsodium.so.b64 > /tmp/libsodium.so.23\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

ser.write(b"sudo cp /tmp/libsodium.so.23 /usr/lib/aarch64-linux-gnu/libsodium.so.23\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

ser.write(b"ldconfig\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

ser.write(b"ldd /tmp/tezos_shim\r\n")
time.sleep(3)
o = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
print("ldd:", o)

ser.close()
print("DONE")
