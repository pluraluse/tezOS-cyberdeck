import os, time, select, base64

# Read binary
with open("/home/qwen/projects/tezos-cyberdeck/src/shim-linux/tezos_shim_arm64", "rb") as f:
    data = f.read()

b64 = base64.b64encode(data).decode("ascii")
chunk_size = 1000
chunks = [b64[i:i+chunk_size] for i in range(0, len(b64), chunk_size)]

print(f"Binary: {len(data)} bytes, Base64: {len(b64)} chars, Chunks: {len(chunks)}")

fd = os.open("/dev/ttyACM0", os.O_RDWR | os.O_NOCTTY)
os.system("stty -F /dev/ttyACM0 115200 raw -echo")

def read():
    r, _, _ = select.select([fd], [], [], 3)
    if r: return os.read(fd, 4096).decode("utf-8", errors="replace")
    return ""

def cmd(s):
    os.write(fd, (s + "\r\n").encode())
    time.sleep(0.5)
    return read()

# Wait for login prompt
print("Waiting for login...")
for _ in range(10):
    out = read()
    if "login" in out.lower():
        print("Found login prompt")
        break
    time.sleep(1)

# Send username
print("Sending username...")
out = cmd("pi")
print(out)

# Wait for password prompt
print("Waiting for password...")
for _ in range(5):
    out = read()
    if "password" in out.lower():
        print("Found password prompt")
        break
    time.sleep(1)

# Send password
print("Sending password...")
out = cmd("raspberry")
print(out)

# Wait for shell
print("Waiting for shell...")
for _ in range(10):
    out = read()
    if "$" in out or "#" in out:
        print("Got shell!")
        break
    time.sleep(1)

# Transfer
print("Starting transfer...")
for i, chunk in enumerate(chunks):
    cmd(f"echo -n {chunk} >> /tmp/tezos_shim.b64")
    if (i+1) % 100 == 0:
        print(f"Sent {i+1}/{len(chunks)}")

print("All chunks sent")

# Decode
print("Decoding...")
out = cmd("base64 -d /tmp/tezos_shim.b64 > /tmp/tezos_shim")
print(out)
out = cmd("chmod +x /tmp/tezos_shim")
print(out)

# Verify
print("Verifying...")
out = cmd("ls -la /tmp/tezos_shim")
print(out)
out = cmd("file /tmp/tezos_shim")
print(out)

os.close(fd)
print("DONE")
