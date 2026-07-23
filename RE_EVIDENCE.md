# Reverse-engineering evidence

## Source image

| Item | Recovered value |
|---|---|
| Flash SHA-256 | `8d6907bcb1f92a10f27b2cf8b0cb965c20481887a5ba1c567f0c11d1a9b97641` |
| Raw flash size | 4 MiB |
| Target | Original ESP32, 4 MiB flash |
| Project | `WoHumi2` |
| Version | `V0_7_release-45-ga223e69-dirty` |
| Stock build | 2024-10-16 17:13:19 |
| Stock framework | ESP-IDF 5.0.2 |
| App descriptor ELF SHA-256 | `17b459d52fea87ded1ac35d13a9adc168a027ff7a1beaa711feb7a80e0a04ef5` |

The application used for disassembly is `ota_0` at flash offset `0x40000`.

## Partition table

| Label | Type/subtype | Offset | Size |
|---|---|---:|---:|
| `esp_secure_cert` | custom | `0x00d000` | `0x002000` |
| `nvs` | data/NVS | `0x010000` | `0x024000` |
| `nvs_keys` | data/NVS keys | `0x034000` | `0x001000` |
| `otadata` | data/OTA | `0x035000` | `0x002000` |
| `phy_init` | data/PHY | `0x037000` | `0x001000` |
| `fctry` | data/NVS | `0x038000` | `0x006000` |
| `keydata` | custom | `0x03e000` | `0x002000` |
| `ota_0` | app/OTA 0 | `0x040000` | `0x1e0000` |
| `ota_1` | app/OTA 1 | `0x220000` | `0x1e0000` |

## Recovered GPIO map

| GPIO | Recovered role | Confidence / port behavior |
|---:|---|---|
| 2 | Blower run/enable | High; asserted with the GPIO17 speed signal |
| 4 | AIP/TM1668-compatible panel clock | High; bit-banged stock descriptor |
| 12 | Wet auxiliary A | High as a paired output; individual load identity unresolved |
| 13 | Panel brightness rail | High; HS LEDC channel 0, timer 0, 40 kHz |
| 14 | Optional auto-refill PWM | High; HS LEDC channel 1, timer 2, 20 kHz; forced off in port |
| 15 | Panel strobe | High; bit-banged stock descriptor |
| 16 | Piezo buzzer | High; HS LEDC channel 6, timer 1, 4 kHz |
| 17 | Blower speed signal | High; LS LEDC channel 2, timer 2, 50% duty |
| 21 | Inverted panel brightness rail | High; HS LEDC channel 7, timer 0, 40 kHz |
| 22 | Wet auxiliary B | High as a paired output; individual load identity unresolved |
| 25 | Optional auto-refill solenoid sense/drive-low | High; input-only in port |
| 26 | Panel data/key-scan line | High; bit-banged stock descriptor |
| 27 | Three addressable RGB status LEDs | High |
| 32 | Stock-configured digital output with no normal writer found | Held low |
| 33 | ADC product/variant classifier | Medium-high; returns hardware codes 5–12, not exposed as water level |
| 34 | Blower tachometer input | High; stock config uses a negative-edge input |

GPIO5, GPIO18, GPIO19, and GPIO23 are used by 38 kHz RMT transmitters in the
optional auto-refill/robot subsystem and are untouched. **GPIO36 is not the
stock water-level ADC.**

## Blower reconstruction

Stock setup places GPIO17 on low-speed LEDC timer 2/channel 2. The normal start
wrapper sets the requested timer frequency, applies 50% duty, and asserts
GPIO2. Stop removes the duty and clears GPIO2.

The mode dispatcher maps:

| Stock mode | Enum | GPIO17 frequency | Wet pair |
|---|---:|---:|---|
| High | 1 | 550 Hz | delayed on |
| Medium | 2 | 400 Hz | delayed on |
| Low | 3 | 250 Hz | delayed on |
| Quiet | 4 | 125 Hz | delayed on |
| Filter Dry | 8 | 250 Hz | always off |

The ordinary wet path counts 400 iterations of a task that delays two 10 ms
RTOS ticks. It then asserts GPIO12 and GPIO22 together. The port translates
that to an 8-second non-blocking delay.

The old port instead treated GPIO14 as the motor drive and GPIO12/GPIO22 as
motor enable/reset. That omitted the real GPIO2 enable and explains why the
fan did not spin.

## Buzzer correction

GPIO16 is configured as high-speed LEDC channel 6/timer 1 at 4 kHz. The normal
`on` wrapper applies 50% duty, and the only stock caller uses it for timed beep
patterns. The former custom component left that channel at 50% while attempting
to run a fictitious pump, producing the reported continuous loud tone.

This port initializes it at 0% and clamps every requested beep to 20–500 ms.
The normal fan and wet paths never call the buzzer.

## Panel and water-level reconstruction

Stock initializes a three-wire panel descriptor with STB/CLK/DIO pins
15/4/26. Its key-scan function sends command `0x42`, reads five LSB-first
bytes, rejects values with either reserved high bit set, and packs bits
0/3/1/4 from each byte into a 20-bit key mask. The recovered water classifier
applies this precedence:

```text
if key bit 7 is set:       EMPTY (0)
else if key bit 18 is set: LOW (1)
else if key bit 17 is 0:   MEDIUM (2)
else:                      HIGH (3)
```

This is why an all-zero scan is `Medium` in stock behavior. Runtime revision
`2026.07.23-r3` corrects the initial port's four-byte flat packing and exposes
the decoded level and stock-compressed 20-bit key mask. Wet hardware remains
disarmed by default for first commissioning.

The 0x42 transaction and display command set are compatible with the
AIP1668/TM1668 family. The glass-specific segment/icon map has not been inferred
from the binary with enough confidence to drive it safely.

## Panel rail levels

The recovered firmware applies these 40 kHz PWM levels:

| Setting | GPIO13 | GPIO21 effective level |
|---|---:|---:|
| Normal | 100% | 50% |
| Dim | 27% | 2% |
| Off | 0% | 0% |

GPIO21 is inverted, so its LEDC duty is the complement of the effective level.

## Optional auto-refill subsystem

The state machine previously described as a pump-feedback controller is tied to
the optional auto-refill/S10 path:

- GPIO25 alternates between input sensing and output-low states;
- GPIO14 is the associated 20 kHz PWM output;
- four 38 kHz RMT channels form the external IR/robot interface; and
- diagnostic strings include `SOL Low Cnt` and `SOL High Cnt`.

It is not needed for ordinary humidification. Because its exact electrical
topology and paired accessory protocol are not yet validated, this port forces
the PWM off, keeps GPIO25 input-only, and never emits the IR patterns.

## Product behavior used by the YAML

The YAML implements four levels, a default 60% target within a 40–70% range,
temperature-dependent Auto targets, Sleep step-down, automatic filter drying,
and the 240-hour filter reminder. Higher-level modes use optional Home
Assistant room-temperature and room-humidity entities because this appliance
does not expose an onboard room climate sensor to the port.

These YAML automations are a practical reconstruction, not a claim that every
transition and persistence rule is identical to stock.

The component currently defaults to a 55-minute filter-dry interval.
SwitchBot's current support page describes a 70-minute countdown at 20 °C. A
complete timed dry cycle from the recovered `V0_7` unit has not yet been
captured, so exact duration parity remains unresolved. The interval is
configurable in YAML.

References:

- [SwitchBot user-manual index](https://www.switch-bot.com/pages/switchbot-user-manual)
- [SwitchBot Evaporative Humidifier product page](https://www.switch-bot.com/products/switchbot-evaporative-humidifier)
- [SwitchBot filter-dry documentation](https://support.switch-bot.com/hc/en-us/articles/20575946790679-Filter-Dry-Function-For-SwitchBot-Evaporative-Humidifier)
- [SwitchBot care and 240-hour reminder](https://support.switch-bot.com/hc/en-us/articles/20579856621463-Care-and-Maintenance-For-SwitchBot-Evaporative-Humidifier)
- [FCC W3902310 internal photographs](https://fccid.io/2AKXB-W3902310/Internal-Photos/Internal-photos-6909672)

## Build verification

Runtime revision `2026.07.23-r3` includes the blower timer fix from r2 and the
stock five-byte panel-key packing described above. For ESP-IDF 5.5:
`ledc_set_freq()` is checked as an `esp_err_t`, followed by an independent
`ledc_get_freq()` readback. The earlier runtime treated the successful
`ESP_OK == 0` result as a failed/zero frequency and therefore never asserted
the blower enable.

The complete project was validated and compiled on 2026-07-23:

| Item | Result |
|---|---|
| ESPHome | 2026.7.1 |
| Framework | ESP-IDF 5.5.5 |
| Configuration validation | Passed |
| C++ compilation and link | Passed |
| Application binary | `0xe8d10` bytes |
| Total image size | 953,503 bytes |
| App-partition free space | 48% |
| DRAM use | 49,060 / 180,736 bytes (27.1%) |
| IRAM use | 80,655 / 131,072 bytes (61.5%) |

The build proves API, schema, compiler, and linker compatibility. It does not
replace electrical validation on the V07 mainboard.

## Hardware validation

On 2026-07-23, runtime r3 was installed on one W3902310/V07 unit. The observed
results were:

- the empty tank decoded as `Empty`;
- a filled tank decoded as `Low`;
- the physical blower ran;
- speed 2 requested and read back 250 Hz;
- tach feedback ramped from 5,496 to 15,324 pulses/min and settled near
  14,976–14,988 pulses/min;
- the wet pair enabled 8.015 seconds after the blower-start log; and
- circulation wetted the physical filter.

These results validate the core path on that unit. They do not establish shaft
RPM, UV effectiveness, other board revisions, or complete stock parity. See
[`docs/VALIDATION.md`](docs/VALIDATION.md) for the sanitized log excerpt and
[`docs/STOCK_PARITY.md`](docs/STOCK_PARITY.md) for the remaining matrix.

## Remaining hardware-validation items

- Resolve which of GPIO12/GPIO22 is circulation and which is UV by board trace
  or scoped load behavior. Stock behavior does not require that distinction
  because it switches them together.
- Time a complete filter-dry run on both stock and ESPHome firmware to resolve
  the current 55-minute configuration versus SwitchBot's published 70 minutes.
- Map the local button bits and display segment/icon RAM.
- Capture the exact RGB status colors and animations.
- Decode the optional auto-refill/S10 IR protocol only if that accessory is in
  scope.
