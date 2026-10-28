# Data-provider terms and attribution (issue #73)

Read on 2026-10-26 from the providers' published terms. Re-read before every release; terms change. This is a
summary for planning, not legal advice, and the providers' own pages govern.

## Open-Meteo (default source) — https://open-meteo.com/en/terms
* Data is licensed **CC BY 4.0**: credit Open-Meteo as the source, link the licence, say if changes were made.
  The terms give no mandatory wording beyond CC BY.
* Free tier is for **non-commercial** use (personal projects, no ads, no subscriptions). Free limits: 600 calls/minute,
  5,000/hour, 10,000/day, 300,000/month. No API key is used or needed.
* Weather defaults to a 15-minute refresh per running copy, far below the limits for any single user.
* Consequence: Weather must stay free of ads/subscriptions to remain on the free tier; a future paid or hosted
  distribution would need a commercial plan or another source.

## MET Norway Locationforecast 2.0 (optional source) — https://api.met.no/doc/TermsOfService
* Every request must identify the application with a **User-Agent containing contact information** (an email or
  website). Unidentified clients can be blocked without warning. Weather sends `weather/VERSION (contact: ...)` and
  **skips the source until the user sets `contact`**. Never invent a contact for the user.
* Data is **CC BY 4.0** (credit, link to the licence, indicate changes).
* Limit: 20 requests/second per application in aggregate; coordinates must have **at most 4 decimals** (5+ returns 403).
  Weather stores coordinates as fixed-point E4, so it complies by construction.
* Clients must **cache and use `If-Modified-Since`** with the previous `Last-Modified` value. **Weather does not send
  `If-Modified-Since` yet** (there is no HTTP conditional-request code). At 15-minute intervals this is a courtesy, but
  it is a stated requirement: tracked as a follow-up, see "Open items".
* For apps with many users the limit is aggregate, so heavy distribution should not poll more often than documented.

### Re-check 2026-10-28 for #152 (MET Norway identification and caching)
Source pages: https://api.met.no/doc/TermsOfService (terms last updated 26 June 2020 per the page) and the Locationforecast 2.0 documentation.
Read through a summarising fetch tool, so the wording below is paraphrased; re-read the pages themselves before a release.
* **Identification:** requests need a User-Agent with the application name (and optionally version) plus "a company email address or a link to
  the company website where we can contact you". The terms' own examples include `AcmeWeatherApp/0.9 github.com/acmeweatherapp`, so a public
  project repository URL is an accepted form of contact. "Fake or random strings" are grounds for a permanent ban, and the API documentation
  says requests with a prohibited or missing User-Agent get 403. Decision (issue #150): default to the project URL, with the user's `contact`
  as an override. Never invent contact details.
* **Caching:** use any cache headers; send `If-Modified-Since` identical to the previous `Last-Modified`; and **do not repeat a request
  before the time in the `Expires` response header**. `If-Modified-Since` landed in #146; **`Expires` is not honoured yet**, and #152 must finish it
  before MET becomes an automatic fallback.
* **Many installations:** the 20 requests/second limit is per application, counting all installations together. At one request per 15 minutes
  per copy that is roughly 18,000 simultaneously active copies. Beyond a lot of traffic MET asks for a caching proxy; the project is nowhere near that.
  A shared User-Agent also means one badly behaved build could get every copy blocked, so keep the rate low and honour `Expires`.
* **Attribution and coordinates:** unchanged: CC BY 4.0 credit with a licence link and a note of changes; coordinates truncated to 4 decimals.

## Attribution used by Weather
The README credits both sources and licences. Every forecast already names its source and timestamp in the output
(`(open-meteo 2026-10-24T05:00)`). Showing the credit inside the X11 and Mac OS 9 About boxes is not done yet.

## NWS (api.weather.gov)
Not in scope for 0.0.1 (PR #16 unmerged). Its terms must be read before v0.1 (#45).

## Open items
- [ ] About boxes (xweather, Mac OS 9) show the credit and licence link. OS 9 needs a hardware layout check.
- [ ] `man/weather.1` mentions the CC BY 4.0 credit.
- [x] Conditional requests (`If-Modified-Since`) for MET Norway: #146. Still open: honour `Expires` (#152).
- [ ] Jack accepts this summary (the issue is labelled `needs-owner`).
