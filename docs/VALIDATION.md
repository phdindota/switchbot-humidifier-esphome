# Validation record

## Software build

Runtime revision `2026.07.23-r3` was validated and compiled on 2026-07-23.

| Item | Result |
|---|---|
| ESPHome | 2026.7.1 |
| Framework | ESP-IDF 5.5.5 |
| Configuration validation | Passed |
| C++ compilation | Passed |
| Link | Passed |
| ESP32 image generation | Passed |
| Application binary | `0xe8d10` bytes |
| Total image | 953,503 bytes |
| App-partition free space | 48% |
| DRAM | 49,060 / 180,736 bytes (27.1%) |
| IRAM | 80,655 / 131,072 bytes (61.5%) |

The earlier line:

```text
ninja: build stopped: subcommand failed
```

was not itself the root error. The failing build revision contained C++ source
placement/channel-definition problems. Those were corrected before the r3
compile summarized above. Successful bootloader generation alone is not a
complete build; the final application link and image-generation result are the
relevant checks.

## Real-device validation

Target:

```text
SwitchBot Evaporative Humidifier 2, W3902310
HUMIDIFIER 2 MAINBOARD V07, dated 2024 01 31
ESP32-D0WD V3
```

Confirmed on the unit under test:

- UART access and full 4 MB stock backup;
- ESPHome boot after serial installation;
- no Secure Boot or flash encryption blocking installation;
- empty-tank state reported as `Empty`;
- filled state reported as `Low`;
- physical blower operation;
- level-2 requested/read-back frequency of 250 Hz;
- tach ramp from 5,496 to 15,324 pulses/min;
- stable tach around 14,976–14,988 pulses/min;
- wet outputs remained off until 8.015 seconds after blower start;
- circulation wetted the filter; and
- the blower continued after humidification stopped, consistent with the
  implemented filter-dry state.

Relevant sanitized log excerpt:

```text
[09:09:13.812][I][switchbot_humidifier:281]:
  Blower started: speed 2, requested 250 Hz, actual 250 Hz
[09:09:18.061][S][sensor]:
  'Blower Tachometer' >> 5496 pulses/min
[09:09:21.827][I][switchbot_humidifier:315]:
  Wet auxiliaries enabled after stock startup delay
[09:09:23.064][S][sensor]:
  'Blower Tachometer' >> 15324 pulses/min
[09:09:28.057][S][sensor]:
  'Blower Tachometer' >> 14976 pulses/min
```

## Not validated by that run

- full physical tests of speed levels 1, 3, and 4;
- medium/high tank-level transitions;
- the complete automatic dry-cycle duration;
- exact individual identity of GPIO12 and GPIO22;
- UV effectiveness;
- local buttons, display, and stock alerts;
- child lock, tilt, or tank-removal safety;
- target/auto/sleep behavior against a calibrated room meter;
- optional S10/auto-refill hardware;
- long-duration reliability or multiple V07 units.

## How to reproduce

Use the staged procedure in [COMMISSIONING.md](COMMISSIONING.md). Record
timestamped logs starting before the fan command and continuing at least 20
seconds. Do not publish stock firmware or device credentials.

Build success demonstrates schema, API, compiler, linker, and image
compatibility. It does not establish electrical safety, feature parity, or
fitness for unattended use.
