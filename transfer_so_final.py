#!/usr/bin/env python3
import serial, base64, os, time, sys, subprocess

SERIAL = "/dev/ttyACM0"
BAUD = 115200
LF = "/tmp/libsodium_extracted/usr/lib/aarch64-linux-gnu/libsodium.so.23.0.2"
RB = "/tmp/libsodium.so.b64"
RX = "/tmp/libsodium.so.23"
CHUNK_SIZE = 1000

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

def waitfor(ser, target, timeout=30):
    buf = ""
    end = time.time() + timeout
    while time.time() < end:
        ser.write(b"\r\n")
        time.sleep(0.5)
        buf += read(ser, 1)
        if target in buf:
            return True
    return False

ls = os.path.getsize(LF)
print("Local:", ls)

with open(LF, "rb") as f:
    b64 = base64.b64encode(f.read()).decode("ascii")
tc = (len(b64) + CHUNK_SIZE - 1) // CHUNK_SIZE
print("Chunks:", tc)

# Reset USB
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"0\n", check=True)
time.sleep(2)
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"1\n", check=True)
time.sleep(5)

ser = serial.Serial(SERIAL, BAUD, timeout=1)
print("Serial opened")

# Wait for login
print("Waiting for login prompt...")
if not waitfor(ser, "login:", 30):
    die("No login prompt")

print("Sending pi...")
ser.write(b"pi\r\n")
time.sleep(2)
text = read(ser, 3)
print("After user:", repr(text[:100]))

if "password" not in text.lower():
    more = read(ser, 3)
    text += more
    if "password" not in text.lower():
        die("No password prompt. Got: " + repr(text[:200]))

print("Sending password...")
ser.write(b"raspberry\r\n")
time.sleep(3)
text = read(ser, 5)
print("After pass:", repr(text[:100]))

if "$" not in text and "#" not in text:
    more = read(ser, 5)
    text += more
    if "$" not in text and "#" not in text:
        die("NO_SHELL. Got: " + repr(text[:300]))

print("LOGGED IN")
ser.write(b"rm -f " + RB.encode() + b"\r\n")
time.sleep(1)
read(ser, 2)

print("Starting transfer...")
for i in range(0, len(b64), CHUNK_SIZE):
    chunk = b64[i:i+CHUNK_SIZE]
    chunk_num = i // CHUNK_SIZE + 1
    ser.write(b"echo -n '" + chunk.encode() + b"' >> " + RB.encode() + b"\r\n")
    time.sleep(0.5)
    if chunk_num % 50 == 0:
        print(" ", chunk_num, "/", tc)
        read(ser, 1)

print("Decoding...")
ser.write(b"base64 -d " + RB.encode() + b" > " + RX.encode() + b" && chmod +x " + RX.encode() + b"\r\n")
time.sleep(3)
read(ser, 2)

ser.write(b"stat -c %s " + RX.encode() + b"\r\n")
time.sleep(2)
o = read(ser, 3)
print("Stat:", o)

sizes = [l.strip() for l in o.splitlines() if l.strip().isdigit()]
rs = int(sizes[-1]) if sizes else 0

if rs == ls:
    print("SUCCESS:", rs)
    # Handle sudo password prompt
    ser.write(b"sudo cp " + RX.encode() + b" /usr/lib/aarch64-linux-gnu/libsodium.so.23 && ldconfig\r\n")
    time.sleep(3)
    text = read(ser, 5)
    if "password" in text.lower():
        ser.write(b"raspberry\r\n")
        time.sleep(3)
        read(ser, 5)
    ser.write(b"ldd /tmp/tezos_shim\r\n")
    time.sleep(3)
    print("ldd:", read(ser, 3))
else:
    print("MISMATCH:", ls, "vs", rs)

ser.close()
print("DONE")
