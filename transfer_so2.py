#!/usr/bin/env python3
"""Fast serial transfer of libsodium to Pi."""
import serial, base64, os, time, sys, subprocess

SERIAL = "/dev/ttyACM0"
BAUD = 115200

# Extract just the .so file from the .deb
subprocess.run([
    "dpkg", "-x",
    "/var/cache/apt/archives/libsodium23_1.0.18-1+deb13u1_arm64.deb",
    "/tmp/libsodium_extracted"
], check=True)

LF = None
for root, dirs, files in os.walk("/tmp/libsodium_extracted"):
    for f in files:
        if "libsodium" in f and ".so" in f:
            LF = os.path.join(root, f)
            break

if not LF:
    print("Could not find libsodium .so file")
    sys.exit(1)

ls = os.path.getsize(LF)
print("Local library file:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
print("Base64:", len(b64), "chars")

ser = serial.Serial(SERIAL, BAUD, timeout=1)

# Wake up and login
ser.write(b"\r\n")
time.sleep(1)
ser.read(ser.in_waiting)

ser.write(b"pi\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

ser.write(b"raspberry\r\n")
time.sleep(3)
ser.read(ser.in_waiting)

# Check for shell
ser.write(b"echo SHELL_OK\r\n")
time.sleep(1)
data = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
if "SHELL_OK" not in data:
    print("No shell, got:", repr(data[:200]))
    ser.close()
    sys.exit(1)

print("Shell OK")

# Start receiving file on Pi side
ser.write(b"base64 -d > /tmp/libsodium.so.23 << 'B64EOF'\r\n")
time.sleep(1)

# Send all base64 data as fast as possible
ser.write(b64.encode())
time.sleep(1)

# End the here document
ser.write(b"\r\nB64EOF\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

# Verify
ser.write(b"stat -c %s /tmp/libsodium.so.23\r\n")
time.sleep(2)
o = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
print("Remote size:", o)

# Install
ser.write(b"sudo cp /tmp/libsodium.so.23 /usr/lib/aarch64-linux-gnu/libsodium.so.23 && ldconfig\r\n")
time.sleep(3)
ser.read(ser.in_waiting)

# Test
ser.write(b"ldd /tmp/tezos_shim\r\n")
time.sleep(3)
o = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
print("ldd:", o)

ser.close()
print("DONE")
