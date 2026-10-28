#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Slow relay for test/tls_local.sh:  tls_proxy.py PORTS_FILE TARGET_PORT NAME:DELAY ...

One listening socket per NAME on an ephemeral port of 127.0.0.1. Each connection is relayed to 127.0.0.1:TARGET_PORT:
client bytes go through at once, but server bytes are passed on ONE BYTE AT A TIME with DELAY seconds between them
(0 = at full speed). So every read the client does returns quickly, yet the answer, the TLS handshake included,
takes very long: a trickling server. Writes 'NAME PORT' lines to PORTS_FILE when ready. Not shipped."""
import os, socket, sys, threading, time

ports_file, target = sys.argv[1], int(sys.argv[2])


def pump_fast(src, dst):
    try:
        while True:
            data = src.recv(4096)
            if not data:
                break
            dst.sendall(data)
    except OSError:
        pass
    for s in (src, dst):
        try:
            s.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass


def pump_slow(src, dst, delay):
    try:
        while True:
            data = src.recv(4096)
            if not data:
                break
            for i in range(len(data)):
                dst.sendall(data[i:i + 1])
                if delay:
                    time.sleep(delay)
    except OSError:
        pass
    for s in (src, dst):
        try:
            s.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass


def serve(listener, delay):
    while True:
        client, _ = listener.accept()
        try:
            server = socket.create_connection(('127.0.0.1', target), timeout=10)
        except OSError:
            client.close()
            continue
        threading.Thread(target=pump_fast, args=(client, server), daemon=True).start()
        threading.Thread(target=pump_slow, args=(server, client, delay), daemon=True).start()


listeners = []
for spec in sys.argv[3:]:
    name, delay = spec.split(':')
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('127.0.0.1', 0))
    s.listen(8)
    listeners.append((name, s))
    threading.Thread(target=serve, args=(s, float(delay)), daemon=True).start()
with open(ports_file + '.tmp', 'w') as f:
    for name, s in listeners:
        f.write('%s %d\n' % (name, s.getsockname()[1]))
os.rename(ports_file + '.tmp', ports_file)
threading.Event().wait()
