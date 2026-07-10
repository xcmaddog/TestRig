"""
Push program.dmc to the Galil DMC-4080 over telnet (port 23).

Usage:
    python3 push_program.py

Sends ER (erase) -> DL (download) -> each line, paced -> \\ (terminator)
-> BP (burn to non-volatile) -> RS (reset, re-runs #AUTO) -> LS (verify).

Prints the controller's response after every step so a rejected line
(marked with a leading '?') is visible immediately instead of silently
getting skipped.
"""

import socket
import time

GALIL_IP = "192.168.42.100"
GALIL_PORT = 23
PROGRAM_PATH = "/home/testrig/Documents/TestStation/program.dmc"
LINE_DELAY_S = 0.05   # bumped up from 0.02 for margin; lower if this feels slow


def send(sock, text, wait=0.3):
    sock.sendall(text.encode())
    time.sleep(wait)
    try:
        return sock.recv(4096).decode(errors="replace")
    except socket.timeout:
        return ""


def main():
    with open(PROGRAM_PATH, "r") as f:
        program_lines = [
            line.strip() for line in f.read().splitlines()
            if line.strip() and not line.strip().startswith(";")
        ]

    s = socket.socket()
    s.settimeout(2.0)
    s.connect((GALIL_IP, GALIL_PORT))
    time.sleep(0.5)
    try:
        print("[banner]", s.recv(1024).decode(errors="replace"))
    except socket.timeout:
        pass

    print("[ER]", send(s, "ER\r"))
    print("[DL]", send(s, "DL\r"))

    for line in program_lines:
        resp = send(s, line + "\r", wait=LINE_DELAY_S)
        if "?" in resp:
            print(f"!! REJECTED: {line!r} -> {resp!r}")
        else:
            print(f"   ok: {line}")

    print("[terminator]", send(s, "\\\r", wait=0.5))
    print("[BP]", send(s, "BP\r", wait=1.0))
    print("[RS]", send(s, "RS\r", wait=1.0))
    time.sleep(0.5)  # give the controller a moment to actually reset
    print("[LS]", send(s, "LS\r", wait=0.5))

    s.close()


if __name__ == "__main__":
    main()