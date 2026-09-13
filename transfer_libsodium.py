#!/usr/bin/env python3
import serial, base64, os, time, sys

LF = "/var/cache/apt/archives/libsodium23_1.0.18-1+deb13u1_arm64.deb"
RB = "/tmp/libsodium23.deb.b64"
RX = "/tmp/libsodium23.deb"
cs = 1000

ls = os.path.getsize(LF)
print("Local:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
tc = (len(b64) + cs - 1) // cs
print("Chunks:", tc)

ser = serial.Serial("/dev/ttyACM0", 115200, timeout=1)
ser.write(b"rm -f " + RB.encode() + b"\r\n")
time.sleep(1)
ser.read(ser.in_waiting)

for i in range(0, len(b64), cs):
    chunk = b64[i:i+cs]
    ser.write(b"echo -n " + repr(chunk).encode() + b" >> " + RB.encode() + b"\r\n")
    time.sleep(0.5)
    if (i//cs+1) % 50 == 0:
        print(" ", i//cs+1, "/", tc)
        ser.read(ser.in_waiting)

print("Decoding...")
ser.write(b"base64 -d " + RB.encode() + b" > " + RX.encode() + b"\r\n")
time.sleep(3)
ser.read(ser.in_waiting)

ser.write(b"stat -c %s " + RX.encode() + b"\r\n")
time.sleep(2)
o = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
print("Remote:", o)

ser.close()
print("DONE")
