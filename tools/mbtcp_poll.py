#!/usr/bin/env python3
import argparse
import socket
import struct
import time


def recv_all(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("socket closed")
        data += chunk
    return data


def build_request(tid, unit, addr, count):
    length = 6  # unit(1) + func(1) + addr(2) + count(2)
    mbap = struct.pack(">HHH", tid, 0, length)
    pdu = struct.pack(">BHH", 0x03, addr, count)
    return mbap + struct.pack(">B", unit) + pdu


def parse_response(payload):
    if len(payload) < 3:
        raise ValueError("short response")
    unit = payload[0]
    func = payload[1]
    if func & 0x80:
        exc = payload[2]
        return unit, func, exc, []
    if func != 0x03:
        raise ValueError("unexpected function: 0x%02X" % func)
    byte_count = payload[2]
    data = payload[3:3 + byte_count]
    regs = []
    for i in range(0, len(data), 2):
        regs.append((data[i] << 8) | data[i + 1])
    return unit, func, None, regs


def poll_once(sock, tid, unit, addr, count):
    req = build_request(tid, unit, addr, count)
    sock.sendall(req)
    header = recv_all(sock, 6)
    _tid, _pid, length = struct.unpack(">HHH", header)
    payload = recv_all(sock, length)
    return parse_response(payload)


def main():
    parser = argparse.ArgumentParser(description="Modbus TCP polling tester (FC03)")
    parser.add_argument("--ip", required=True, help="server ip, e.g. 192.168.0.177")
    parser.add_argument("--port", type=int, required=True, help="server port, e.g. 1503")
    parser.add_argument("--unit", type=int, default=1, help="unit id (default: 1)")
    parser.add_argument("--addr", type=int, default=0, help="start register (default: 0)")
    parser.add_argument("--count", type=int, default=3, help="register count (default: 3)")
    parser.add_argument("--interval", type=float, default=2.0, help="poll interval seconds")
    args = parser.parse_args()

    tid = 0
    while True:
        try:
            sock = socket.create_connection((args.ip, args.port), timeout=3)
            sock.settimeout(3)
            while True:
                tid = (tid + 1) & 0xFFFF
                unit, func, exc, regs = poll_once(sock, tid, args.unit, args.addr, args.count)
                ts = time.strftime("%F %T")
                if exc is not None:
                    print("[%s] unit=%d func=0x%02X exception=0x%02X" % (ts, unit, func, exc))
                else:
                    print("[%s] unit=%d regs=%s" % (ts, unit, regs))
                time.sleep(args.interval)
        except Exception as e:
            print("[WARN] %s, retry in 2s" % e)
            time.sleep(2)


if __name__ == "__main__":
    main()
