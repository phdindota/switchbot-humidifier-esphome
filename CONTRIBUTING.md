# Contributing

Contributions are welcome after the repository owner selects a license. Until
then, use issues to share findings and ask before submitting substantial code.

## Good contributions

- scope or logic-analyzer captures that identify a signal without guessing;
- stock-firmware behavior matrices with exact starting conditions and timing;
- tests from additional W3902310 V07 units;
- evidence for other board revisions;
- local button and segment/icon mapping;
- confirmation of the individual GPIO12/GPIO22 loads;
- a fully timed stock filter-dry run;
- safety fixes that preserve fail-closed startup; and
- ESPHome compatibility fixes with complete build output.

## Evidence standard

For every hardware claim, state:

- model and mainboard revision;
- stock or ESPHome firmware revision;
- equipment and measurement point;
- initial water/fan/filter state;
- exact action taken;
- timestamped result; and
- whether the result was repeated.

Label a conclusion as observed, inferred, or unverified. Do not present a
component-name guess as a fact.

## Safety invariants

Changes must preserve these defaults:

- blower, wet outputs, buzzer, and refill output off during setup;
- wet system disarmed on a fresh install;
- wet outputs off for Empty/Unknown water;
- wet outputs off until startup delay and tach checks pass;
- GPIO12/GPIO22 fail off together;
- GPIO14 forced off and GPIO25 input-only unless the accessory is fully
  characterized;
- bounded buzzer operation; and
- shutdown/emergency stop force power/control outputs off.

Any proposal to weaken an invariant needs explicit evidence, a migration plan,
and a safe commissioning procedure.

## Reproducing the build

```sh
cp secrets.example.yaml secrets.yaml
```

Replace the API placeholder with a valid base64 key, then:

```sh
esphome config switchbot-humidifier.yaml
esphome compile switchbot-humidifier.yaml
```

Include ESPHome and ESP-IDF versions with failures. Keep formatting consistent
with the surrounding Python, C++, YAML, and Markdown.

## Privacy and copyright

Never commit or attach:

- raw stock firmware or extracted proprietary binaries;
- `secrets.yaml`;
- Wi-Fi/API/OTA credentials;
- device certificates or NVS dumps;
- unredacted MAC addresses, IP addresses, or SSIDs; or
- logs containing personal Home Assistant entity names unless the reporter
  knowingly chooses to share them.

Short, sanitized log excerpts and original measurements are preferred.

## Pull requests

Describe what changed, why the evidence supports it, how it was compiled, and
what was tested on hardware. Documentation-only changes should still verify
links, commands, and Markdown structure.
