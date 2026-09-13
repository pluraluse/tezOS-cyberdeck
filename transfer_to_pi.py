#!/usr/bin/env python3
"""
Transfer tezos_shim_arm64 to a Raspberry Pi over a serial console
(/dev/ttyACM0), by logging in with pi/raspberry, base64-encoding the
binary locally, and streaming it over in 1000-char chunks via
`echo -n "<chunk>" >> /tmp/tezos_shim.b64`.
"""

import os
import sys
import time
import select
import termios
import base64

SERIAL_DEV = "/dev/ttyACM0"
BINARY_PATH = "/home/qwen/projects/tezos-cyberdeck/src/shim-linux/tezos_shim_arm64"
REMOTE_B64 = "/tmp/tezos_shim.b64"
REMOTE_BIN = "/tmp/tezos_shim"
CHUNK_SIZE = 1000
BAUD = termios.B115200
INTER_CMD_SLEEP = 0.5

log_lines = []


def log(msg):
    print(msg, flush=True)
    log_lines.append(msg)


def open_serial(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    # cfmakeraw-equivalent
    iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs
    iflag &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP |
               termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON)
    oflag &= ~termios.OPOST
    lflag &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)
    cflag &= ~(termios.CSIZE | termios.PARENB)
    cflag |= termios.CS8 | termios.CREAD | termios.CLOCAL
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW,
                       [iflag, oflag, cflag, lflag, BAUD, BAUD, cc])
    return fd


def read_available(fd, timeout=0.5):
    """Read whatever is available within timeout, return bytes (possibly empty)."""
    data = b""
    end = time.time() + timeout
    while True:
        remaining = end - time.time()
        if remaining <= 0:
            break
        r, _, _ = select.select([fd], [], [], remaining)
        if fd in r:
            try:
                chunk = os.read(fd, 4096)
            except BlockingIOError:
                break
            if not chunk:
                break
            data += chunk
        else:
            break
    return data


def wait_for(fd, patterns, overall_timeout=20.0):
    """Read from fd until one of patterns (bytes) appears in the accumulated buffer.
    Returns (matched_pattern, full_buffer_text) or (None, buffer_text) on timeout."""
    buf = b""
    deadline = time.time() + overall_timeout
    while time.time() < deadline:
        buf += read_available(fd, timeout=0.3)
        for p in patterns:
            if p in buf:
                text = buf.decode(errors="replace")
                log(text)
                return p, text
    text = buf.decode(errors="replace")
    log(text)
    return None, text


def send(fd, s):
    os.write(fd, s.encode())


def main():
    if not os.path.exists(BINARY_PATH):
        log(f"ERROR: binary not found at {BINARY_PATH}")
        sys.exit(1)

    log(f"Opening {SERIAL_DEV} ...")
    fd = open_serial(SERIAL_DEV)

    # Nudge the console in case it's sitting idle
    send(fd, "\r\n")
    time.sleep(INTER_CMD_SLEEP)

    log("Waiting for login prompt...")
    matched, _ = wait_for(fd, [b"login:"], overall_timeout=20.0)
    if matched is None:
        log("ERROR: never saw login: prompt")
        os.close(fd)
        sys.exit(1)

    send(fd, "pi\r")
    time.sleep(INTER_CMD_SLEEP)

    log("Waiting for Password prompt...")
    matched, _ = wait_for(fd, [b"Password:"], overall_timeout=20.0)
    if matched is None:
        log("ERROR: never saw Password: prompt")
        os.close(fd)
        sys.exit(1)

    send(fd, "raspberry\r")
    time.sleep(INTER_CMD_SLEEP)

    log("Waiting for shell prompt...")
    matched, _ = wait_for(fd, [b"$ ", b"# "], overall_timeout=20.0)
    if matched is None:
        log("ERROR: never saw shell prompt after login")
        os.close(fd)
        sys.exit(1)

    log("Logged in. Clearing any stale remote b64 file...")
    send(fd, f": > {REMOTE_B64}\r")
    time.sleep(INTER_CMD_SLEEP)
    wait_for(fd, [b"$ ", b"# "], overall_timeout=10.0)

    with open(BINARY_PATH, "rb") as f:
        raw = f.read()
    b64_data = base64.b64encode(raw).decode()
    total_len = len(b64_data)
    chunks = [b64_data[i:i + CHUNK_SIZE] for i in range(0, total_len, CHUNK_SIZE)]
    log(f"Binary size: {len(raw)} bytes, base64 size: {total_len} chars, "
        f"{len(chunks)} chunks of up to {CHUNK_SIZE} chars.")

    for idx, chunk in enumerate(chunks, 1):
        cmd = f'echo -n "{chunk}" >> {REMOTE_B64}\r'
        send(fd, cmd)
        time.sleep(INTER_CMD_SLEEP)
        matched, _ = wait_for(fd, [b"$ ", b"# "], overall_timeout=15.0)
        if matched is None:
            log(f"WARNING: no prompt seen after chunk {idx}/{len(chunks)}")
        if idx % 25 == 0 or idx == len(chunks):
            log(f"-- sent chunk {idx}/{len(chunks)} --")

    log("All chunks sent. Decoding on remote side...")
    send(fd, f"base64 -d {REMOTE_B64} > {REMOTE_BIN} && chmod +x {REMOTE_BIN}\r")
    time.sleep(INTER_CMD_SLEEP)
    wait_for(fd, [b"$ ", b"# "], overall_timeout=20.0)

    log("Verifying...")
    send(fd, f"ls -la {REMOTE_BIN} && file {REMOTE_BIN}\r")
    time.sleep(INTER_CMD_SLEEP)
    wait_for(fd, [b"$ ", b"# "], overall_timeout=20.0)

    os.close(fd)
    log("Done.")


if __name__ == "__main__":
    main()
