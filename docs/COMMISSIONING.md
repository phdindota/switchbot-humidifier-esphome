# First-run commissioning

Commission the appliance in stages. The goal is to prove every prerequisite
before GPIO12/GPIO22 can energize.

Keep the unit attended throughout. `Stop All Hardware` is the software stop;
unplugging mains is the definitive stop.

## Stage 0: startup

- Tank empty.
- `Wet System Armed`: off.
- `Automatic Filter Drying`: on is acceptable; no dry cycle can start until a
  wet run has occurred.
- Fan entity: off.

Expected:

- no continuous buzzer;
- blower stopped;
- wet auxiliaries off;
- S10/auto-refill outputs inactive;
- component log ends with `wet outputs are DISARMED`.

Stop if any load starts unexpectedly.

## Stage 1: empty-water scan

With the tank empty:

- `Water Level` must become `Empty`;
- `Water Empty` must be on;
- `Panel Key Mask` should update once per second in Home Assistant;
- the component log should show a five-byte scan and 20-bit mask when the value
  changes.

Do not treat an all-zero key mask as a communication acknowledgement; stock
decodes it as `Medium`.

If the result is `Unknown`, `Low`, `Medium`, or `High` while definitely empty,
leave the wet system disarmed and investigate the panel bus.

## Stage 2: blower-only test

With the tank still empty and wet system disarmed:

1. Select `Quiet`.
2. Turn the fan on at speed 1.
3. Confirm the physical blower spins.
4. Confirm `Blower Signal Frequency` reports 125 Hz.
5. Within 15 seconds, confirm `Blower Tachometer` is well above the configured
   30 pulses/min floor.
6. Confirm `Wet Auxiliaries Active` remains off.
7. Repeat at speeds 2, 3, and 4.

Expected signal frequencies:

| Speed | Name | Frequency |
|---:|---|---:|
| 1 | Quiet | 125 Hz |
| 2 | Low | 250 Hz |
| 3 | Medium | 400 Hz |
| 4 | High | 550 Hz |

The frequency sensor reports the commanded/read-back LEDC frequency. The
tachometer proves the blower actually moved.

If tach feedback is absent after the startup timeout, the component sets
`Blower Tach Fault` and forces operating outputs off.

## Stage 3: fill and recheck water level

Turn the fan off, fill the tank according to the appliance instructions, and
wait for the scan to settle.

Expected:

- `Water Level` changes away from `Empty`;
- `Water Empty` turns off; and
- raw panel bytes/mask change as the physical level changes.

The tested unit reported `Low` after filling enough for the supervised wet
test.

## Stage 4: supervised wet test

Only after stages 1–3 pass:

1. Check for dry surfaces and correct filter installation.
2. Turn `Wet System Armed` on.
3. Start `Low`/speed 2.
4. Confirm the blower starts immediately at 250 Hz.
5. Confirm tach feedback rises.
6. Confirm `Wet Auxiliaries Active` remains off during the startup delay.
7. At approximately eight seconds, confirm it turns on.
8. Verify normal circulation wets the filter and inspect for leaks.

The validated log showed:

```text
09:09:13.812  Blower started: speed 2, requested 250 Hz, actual 250 Hz
09:09:18.061  Blower Tachometer: 5496 pulses/min (ramping)
09:09:21.827  Wet auxiliaries enabled after stock startup delay
09:09:23.064  Blower Tachometer: 15324 pulses/min
09:09:28.057  Blower Tachometer: 14976 pulses/min
```

The wet-enable event occurred 8.015 seconds after the blower-start log.

## Stage 5: stop and filter dry

Turn the Home Assistant fan off after the wet system has run.

Expected:

- GPIO12/GPIO22 and `Wet Auxiliaries Active` turn off immediately;
- the HA fan entity says off;
- `Filter Dry Running` turns on; and
- the physical blower continues at level 2/250 Hz.

That continued blower operation is deliberate. Filter drying has a separate
state from the HA fan entity. Press `Stop All Hardware` to abort it.

The present YAML uses 55 minutes. SwitchBot's current published documentation
describes 70 minutes at 20 °C; the exact duration of the recovered unit has not
yet been timed to completion.

## Stage 6: reboot checks

After successful supervised operation:

- reboot with the fan off and confirm no load starts unexpectedly;
- disconnect and restore power once, then repeat the check;
- confirm water state and tach still update;
- decide whether to leave `Wet System Armed` restored on or turn it off between
  tests; and
- confirm the **Stop All Hardware** button remains reachable.

## What to record for an issue

Include:

- exact model and mainboard revision;
- ESPHome and runtime versions;
- requested fan speed;
- five raw panel bytes/mask and decoded water state;
- requested/actual blower frequency;
- tach readings over the first 20 seconds;
- whether wet outputs were armed and active; and
- timestamped logs from before the event through shutdown.

Remove Wi-Fi names, IPs, API/OTA credentials, MAC addresses, and any stock
flash image before publishing.
