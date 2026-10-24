#!/usr/bin/env python3
"""Plain-HTTP fixture server for feature tests: fixture_server.py MODE PORT

Modes: ok 500 404 slow chunked trunc garbage hangup badjson, and w:CODE:ISDAY:TEMP (a forecast with that WMO code)
Not shipped to maul; it only runs on the build machine."""
import socketserver, sys, time

FIX = open(__file__.rsplit('/', 1)[0] + '/fixtures/openmeteo-ok.json', 'rb').read()
mode, port = sys.argv[1], int(sys.argv[2])

def weather(code, is_day, temp):
    import json
    d = json.loads(FIX)
    d['current'].update({'weather_code': int(code), 'is_day': int(is_day), 'temperature_2m': float(temp)})
    return json.dumps(d).encode()

class H(socketserver.StreamRequestHandler):
    def handle(self):
        while self.rfile.readline().strip():   # discard the request headers
            pass
        w = self.wfile.write
        if mode == 'ok':
            w(b'HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n' % len(FIX) + FIX)
        elif mode.startswith('w:'):
            _, code, day, temp = mode.split(':')
            body = weather(code, day, temp)
            w(b'HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n' % len(body) + body)
        elif mode == '500':
            w(b'HTTP/1.0 500 Internal Server Error\r\n\r\noops')
        elif mode == '404':
            w(b'HTTP/1.0 404 Not Found\r\n\r\n{}')
        elif mode == 'slow':
            time.sleep(30)
        elif mode == 'chunked':
            w(b'HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}\r\n0\r\n\r\n')
        elif mode == 'trunc':
            w(b'HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n' % len(FIX) + FIX[:60])
        elif mode == 'garbage':
            w(b'\x00\x01 this is not http \xff\r\n\r\n')
        elif mode == 'badjson':
            w(b'HTTP/1.0 200 OK\r\n\r\n{"current": [1,2')
        # hangup: say nothing, close

class S(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

S(('127.0.0.1', port), H).serve_forever()
