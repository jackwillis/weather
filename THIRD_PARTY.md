# Third-party notices

Weather itself is licensed GPL-3.0-or-later (`LICENSE`). Everything below is compatible with that licence.

The `weather` binaries statically link BearSSL. Its license requires this notice to
accompany copies, so it ships in every release tarball and binary package.

## BearSSL (https://bearssl.org/)

Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>

Permission is hereby granted, free of charge, to any person obtaining 
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be 
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Trust anchors

`src/tls/anchors.h` is generated from `src/tls/roots.pem`: the public ISRG Root X1 and X2 certificates (Internet Security
Research Group; Open-Meteo, api.weather.gov) and HARICA TLS RSA Root CA 2021 (Hellenic Academic and Research
Institutions CA; api.met.no), converted with BearSSL's `brssl ta`. Certificates are public data, not licensed software.

## Data

Forecasts come from Open-Meteo (https://open-meteo.com) and, optionally, MET Norway (https://api.met.no, Locationforecast 2.0).
Check both services' terms of use and attribution requirements before distributing widely (TODO: verify; the output already
names the source). MET Norway's terms ask every client to identify itself with a contact: this is the `contact` setting, and
the source is skipped until it is set.
