#!/usr/bin/env python3
import serial, base64, os, time, sys, subprocess

SERIAL = "/dev/ttyACM0"
BAUD = 115200

subprocess.run(["dpkg", "-x",
    "/var/cache/apt/archives/libsodium23_1.0.18-1+deb13u1_arm64.deb",
    "/tmp/ls"], check=True)

LF = None
for root, dirs, files in os.walk("/tmp/ls"):
    for f in files:
        if "libsodium" in f and ".so" in f:
            LF = os.path.join(root, f)
            break

ls = os.path.getsize(LF)
print("Local:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
tc = (len(b64) + 500 - 1) // 500
print("Chunks:", tc)

subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"],
    input=b"0\n", check=True)
time.sleep(2)
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"],
    input=b"1\n", check=True)
time.sleep(5)

ser = serial.Serial(SERIAL, BAUD, timeout=1)
ser.write(b"\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

ser.write(b"pi\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

ser.write(b"raspberry\r\n")
time.sleep(3)
ser.read(ser.in_waiting)

ser.write(b"echo OK\r\n")
time.sleep(1)
d = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
if "OK" not in d:
    print("No shell:", repr(d[:100]))
    sys.exit(1)

print("Shell OK")
ser.write(b"rm -f /tmp/ls.b64\r\n")
time.sleep(1)
ser.read(ser.in_waiting)

for i in range(0, len(b64), 500):
    chunk = b64[i:i+500]
    ser.write(b"echo -n '" + chunk.encode() + b"' >> /tmp/ls.b64\r\n")
    time.sleep(0.3)
    if (i//500+1) % 50 == 0:
        print(" ", i//500+1, "/", tc)
        ser.read(ser.in_waiting)

print("Decoding...")
ser.write(b"base64 -d /tmp/ls.b64 > /tmp/libsodium.so.23\r\n")
time.sleep(2)
ser.read(ser.in_waiting)

ser.write(b"stat -c %s /tmp/libsodium.so.23\r\n")
time.sleep(2)
o = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
print("Stat:", o)

sizes = [l.strip() for l in o.splitlines() if l.strip().isdigit()]
rs = int(sizes[-1]) if sizes else 0

if rs == ls:
    print("SUCCESS:", rs)
    ser.write(b"sudo cp /tmp/libsodium.so.23 /usr/lib/aarch64-linux-gnu/ && ldconfig\r\n")
    time.sleep(2)
    ser.read(ser.in_waiting)
else:
    print("MISMATCH:", ls, rs)

ser.close()
print("DONE")
