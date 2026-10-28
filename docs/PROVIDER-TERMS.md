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
* Every request identifies the app as `weather/VERSION (contact: https://github.com/jackwillis/weather)` unless the user configures a contact override. New installs try Open-Meteo first, then MET; saving off skips MET.
* Data is **CC BY 4.0** (credit, link to the licence, indicate changes).
* Limit: 20 requests/second per application in aggregate; coordinates must have **at most 4 decimals** (5+ returns 403).
  Weather stores coordinates as fixed-point E4, so it complies by construction.
* Clients cache MET responses, reuse them without a request until `Expires`, then send `If-Modified-Since` with the exact saved `Last-Modified`. A valid 304 keeps the forecast and replaces its expiry from that response. A later Open-Meteo success preserves the MET record in the same file.
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
  before the time in the `Expires` response header**. `If-Modified-Since` landed in #146; `Expires` is now parsed and persisted by #152. Native acceptance remains open.
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
- [x] Conditional requests (`If-Modified-Since`) for MET Norway: #146. Expiry implementation: #152; native verification remains open.
- [ ] Jack accepts this summary (the issue is labelled `needs-owner`).

### Implementation limits and release review (#152)
The above describes successful 200/304 forecast reuse, not a blanket provider-terms acceptance. Re-read [the official terms](https://api.met.no/doc/TermsOfService) before release. The current terms also discuss redirects, gzip, traffic spreading and aggregate application traffic; this change does not implement a redirect/decompression client or establish fleet-scale compliance. Native behavior and owner acceptance remain release gates. Cache deletion, corruption, a query/provider-selection change, or concurrent independent writers can discard the saved metadata; the cache is not a cross-process traffic coordinator.
