# Changelog

This project uses date-based development revisions while the hardware mapping
is still experimental.

## 2026.07.23-r3

- Corrected the panel scan to read five stock-format bytes.
- Reproduced the stock four-bits-per-byte compression into a 20-bit key mask.
- Implemented the recovered Empty/Low/Medium/High water classifier.
- Exposed water level and panel mask diagnostics.
- Compiled successfully with ESPHome 2026.7.1 and ESP-IDF 5.5.5.
- Validated level-2 blower, tach feedback, empty/low sensing, delayed wet
  outputs, and real filter wetting on a V07 unit.

## 2026.07.23-r2

- Corrected ESP-IDF 5.5 LEDC frequency handling: `ledc_set_freq()` returns
  `ESP_OK` on success and `ledc_get_freq()` supplies the read-back frequency.
- Added actual-frequency startup logging.

## 2026.07.23-r1

- Replaced the incorrect early fan/pump model with the recovered stock paths.
- Mapped blower control to GPIO2 enable plus GPIO17 50%-duty frequency.
- Identified GPIO16 as the piezo buzzer and made it off by default.
- Treated GPIO12/GPIO22 as paired wet auxiliaries.
- Added the stock-derived eight-second startup delay.
- Added wet-system arming, tach safety, emergency stop, and bounded beep.
- Disabled unverified S10/auto-refill outputs.

## Documentation release

- Added confirmed J3 pinout and board photographs.
- Added full backup, first-flash, commissioning, OTA, and restore instructions.
- Added explicit stock-parity and validation matrices.
- Documented the unresolved 55-minute versus published 70-minute filter-dry
  duration.
- Added safety, contribution, issue-reporting, and CI files for publication.
