#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
"""Plain-HTTP fixture server for feature tests: fixture_server.py MODE PORT

Modes: ok metno fallback imperial 500 404 slow chunked trunc garbage hangup badjson, and w:CODE:ISDAY:TEMP (a forecast with that WMO code)
`ok` honours the temperature_unit/wind_speed_unit query parameters like the real API (converting the imperial fixture);
`imperial` ignores them, like a server that does not understand them.
Not shipped to maul; it only runs on the build machine."""
import socketserver, sys, time

FIXDIR = __file__.rsplit('/', 1)[0] + '/fixtures/'
FIX = open(FIXDIR + 'openmeteo-ok.json', 'rb').read()
MET = open(FIXDIR + 'metno-ok.json', 'rb').read()
mode, port = sys.argv[1], int(sys.argv[2])

def om_body(path, cur=None, convert=False):
    """Open-Meteo answer for this request path. Unit labels follow the query; with convert the imperial fixture's numbers
    are converted too, otherwise the numbers are served as they are (the w: modes pass their own temperature)."""
    import json
    d = json.loads(FIX)
    metric, kmh = 'temperature_unit=celsius' in path, 'wind_speed_unit=kmh' in path
    if cur:
        d['current'].update(cur)
    if not (metric or kmh) and not cur:
        return FIX
    if metric:
        if convert:
            d['current']['temperature_2m'] = round((d['current']['temperature_2m'] - 32) / 1.8, 1)
        d['current_units']['temperature_2m'] = '\u00b0C'
    if kmh:
        if convert:
            d['current']['wind_speed_10m'] = round(d['current']['wind_speed_10m'] * 1.609344, 1)
        d['current_units']['wind_speed_10m'] = 'km/h'
    return json.dumps(d, ensure_ascii=False).encode('utf-8')

class H(socketserver.StreamRequestHandler):
    def handle(self):
        request_line = self.rfile.readline().decode('latin-1')
        path = request_line.split(' ')[1] if request_line.count(' ') >= 2 else ''
        while self.rfile.readline().strip():   # discard the request headers
            pass
        w = self.wfile.write
        if mode == 'ok':
            body = om_body(path, convert=True)
            w(b'HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n' % len(body) + body)
        elif mode == 'imperial':    # ignores the unit parameters
            w(b'HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n' % len(FIX) + FIX)
        elif mode == 'metno':
            w(b'HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n' % len(MET) + MET)
        elif mode == 'fallback':    # Open-Meteo's path is broken, MET Norway's works
            if path.startswith('/v1/forecast'):
                w(b'HTTP/1.0 500 Internal Server Error\r\n\r\noops')
            else:
                w(b'HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n' % len(MET) + MET)
        elif mode.startswith('w:'):
            _, code, day, temp = mode.split(':')
            body = om_body(path, {'weather_code': int(code), 'is_day': int(day), 'temperature_2m': float(temp)})
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
