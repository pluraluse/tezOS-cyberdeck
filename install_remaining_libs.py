#!/usr/bin/env python3
"""Install libcjson, libqrencode, libzbar on Pi."""
import pexpect, base64, os, time, sys, subprocess

SERIAL = "/dev/ttyACM0"
BAUD = 115200
TIMEOUT = 30

debs = {
    "libcjson": "/var/cache/apt/archives/libcjson1_1.7.18-3.1+deb13u1_arm64.deb",
    "libqrencode": "/var/cache/apt/archives/libqrencode4_4.1.1-2_arm64.deb",
    "libzbar": "/var/cache/apt/archives/libzbar0t64_0.23.93-8_arm64.deb",
}

subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"0\n", check=True)
time.sleep(2)
subprocess.run(["sudo", "tee", "/sys/bus/usb/devices/3-2/authorized"], input=b"1\n", check=True)
time.sleep(5)

child = pexpect.spawn("screen " + SERIAL + " " + str(BAUD), timeout=TIMEOUT, env={"TERM": "vt100"})
child.logfile = sys.stdout.buffer

child.sendline("")
idx = child.expect(["login:", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: sys.exit("NO_LOGIN")

child.sendline("pi")
idx = child.expect(["Password:", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: sys.exit("NO_PASS")

child.sendline("raspberry")
idx = child.expect([r"\$ ", pexpect.TIMEOUT], timeout=TIMEOUT)
if idx == 1: sys.exit("NO_SHELL")

print("LOGGED IN")

for name, deb_path in debs.items():
    print(f"Processing {name}...")
    # Extract .so from deb
    extract_dir = f"/tmp/{name}_extracted"
    os.makedirs(extract_dir, exist_ok=True)
    subprocess.run(["dpkg", "-x", deb_path, extract_dir], check=True)
    
    # Find .so file
    so_file = None
    for root, dirs, files in os.walk(extract_dir):
        for f in files:
            if ".so" in f:
                so_file = os.path.join(root, f)
                break
    
    if not so_file:
        print(f"  No .so found in {name}")
        continue
    
    local_size = os.path.getsize(so_file)
    print(f"  {so_file}: {local_size} bytes")
    
    with open(so_file, "rb") as f:
        b64_data = base64.b64encode(f.read()).decode("ascii")
    
    # Find the target filename from the .so
    so_basename = os.path.basename(so_file)
    remote_path = f"/tmp/{so_basename}"
    remote_b64 = f"/tmp/{so_basename}.b64"
    
    # Transfer
    child.sendline(f"rm -f {remote_b64}")
    child.expect(r"\$ ", timeout=TIMEOUT)
    
    CHUNK_SIZE = 1000
    for i in range(0, len(b64_data), CHUNK_SIZE):
        chunk = b64_data[i:i+CHUNK_SIZE]
        child.sendline(f"echo -n '{chunk}' >> {remote_b64}")
        child.expect(r"\$ ", timeout=TIMEOUT)
        if (i//CHUNK_SIZE+1) % 50 == 0:
            print(f"  {i//CHUNK_SIZE+1}/{(len(b64_data)+CHUNK_SIZE-1)//CHUNK_SIZE}")
    
    print(f"  Decoding...")
    child.sendline(f"base64 -d {remote_b64} > {remote_path} && chmod +x {remote_path}")
    child.expect(r"\$ ", timeout=TIMEOUT)
    
    # Find install path
    install_dir = os.path.dirname(so_file.replace(extract_dir, ""))
    install_path = f"/usr/lib/aarch64-linux-gnu/{so_basename}"
    
    child.sendline(f"sudo cp {remote_path} {install_path} && ldconfig")
    idx = child.expect([r"\$ ", "[Pp]assword", pexpect.TIMEOUT], timeout=TIMEOUT)
    if idx == 1:
        child.sendline("raspberry")
        child.expect(r"\$ ", timeout=TIMEOUT)
    elif idx == 2:
        print(f"  WARNING: install timed out")
    
    print(f"  Installed to {install_path}")

# Final check
print("Final ldd check...")
child.sendline("ldd /tmp/tezos_shim")
child.expect(r"\$ ", timeout=TIMEOUT)
print(child.before.decode("utf-8", errors="replace"))

child.close()
print("DONE")
