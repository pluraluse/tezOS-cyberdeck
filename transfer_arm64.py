import os, time, select, base64

# Read binary
with open("/home/qwen/projects/tezos-cyberdeck/src/shim-linux/tezos_shim_arm64", "rb") as f:
    data = f.read()

# Encode as base64
b64 = base64.b64encode(data).decode("ascii")

# Split into chunks
chunk_size = 1000
chunks = [b64[i:i+chunk_size] for i in range(0, len(b64), chunk_size)]

print(f"Binary: {len(data)} bytes, Base64: {len(b64)} chars")
print(f"Total chunks: {len(chunks)}")

fd = os.open("/dev/ttyACM0", os.O_RDWR | os.O_NOCTTY)

def read():
    r, _, _ = select.select([fd], [], [], 2)
    if r: return os.read(fd, 4096).decode("utf-8", errors="replace")
    return ""

def cmd(s):
    os.write(fd, (s + "\r\n").encode())
    time.sleep(0.5)
    return read()

# Login
print("=== Login ===")
print(cmd("pi"))
time.sleep(1)
print(cmd("raspberry"))
time.sleep(1)

# Clear prompt
read()

# Start transfer
print("=== Transfer ===")
for i, chunk in enumerate(chunks):
    cmd(f"echo -n {chunk} >> /tmp/tezos_shim.b64")
    if (i+1) % 100 == 0:
        print(f"Sent {i+1}/{len(chunks)} chunks")

print("All chunks sent")

# Decode
print("=== Decode ===")
print(cmd("base64 -d /tmp/tezos_shim.b64 > /tmp/tezos_shim"))
print(cmd("chmod +x /tmp/tezos_shim"))

# Verify
print("=== Verify ===")
print(cmd("ls -la /tmp/tezos_shim"))
print(cmd("file /tmp/tezos_shim"))

os.close(fd)
