#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""TLS fixture server for test/tls_local.sh:  tls_server.py PORTS_FILE REQUESTS_FILE NAME:CHAIN.pem:KEY.pem ...

One listening socket per NAME on an ephemeral port of 127.0.0.1, each presenting its own certificate chain.
When every socket is listening it writes 'NAME PORT' lines to PORTS_FILE. Each time a complete HTTP request arrives over
a finished handshake it appends NAME to REQUESTS_FILE and answers 200 {} : so a name that never appears there was
refused by the client before any request bytes were sent. Not shipped; it only runs on the build machine."""
import socket, ssl, sys, threading, warnings

ports_file, requests_file = sys.argv[1], sys.argv[2]
lock = threading.Lock()
BODY = b'{}'
REPLY = b'HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n' % len(BODY) + BODY


def serve(name, ctx, sock):
    while True:
        conn, _ = sock.accept()
        threading.Thread(target=handle, args=(name, ctx, conn), daemon=True).start()


def handle(name, ctx, conn):
    try:
        conn.settimeout(10)
        tls = ctx.wrap_socket(conn, server_side=True)
        data = b''
        while b'\r\n\r\n' not in data:
            chunk = tls.recv(4096)
            if not chunk:
                return
            data += chunk
        with lock:
            with open(requests_file, 'a') as f:
                f.write(name + '\n')
        tls.sendall(REPLY)
        tls.close()
    except (ssl.SSLError, OSError):
        pass            # the client refused the handshake: that is the point of most cases


socks = []
for spec in sys.argv[3:]:
    name, chain, key = spec.split(':')
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(chain, key)
    if name in ("legacy", "cbc"):
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", DeprecationWarning)
            version = ssl.TLSVersion.TLSv1_1 if name == "legacy" else ssl.TLSVersion.TLSv1_2
            ctx.minimum_version = ctx.maximum_version = version
        ctx.set_ciphers("ECDHE-ECDSA-AES128-SHA:@SECLEVEL=0")
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('127.0.0.1', 0))
    s.listen(8)
    socks.append((name, s))
    threading.Thread(target=serve, args=(name, ctx, s), daemon=True).start()
open(requests_file, 'a').close()
with open(ports_file + '.tmp', 'w') as f:
    for name, s in socks:
        f.write('%s %d\n' % (name, s.getsockname()[1]))
import os
os.rename(ports_file + '.tmp', ports_file)
threading.Event().wait()
