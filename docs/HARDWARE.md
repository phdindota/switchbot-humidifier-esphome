# Hardware notes

## Validated target

| Item | Tested value |
|---|---|
| Product | SwitchBot Evaporative Humidifier 2 |
| Model | W3902310 |
| Mainboard | HUMIDIFIER 2 MAINBOARD V07 |
| Board date | 2024 01 31 |
| MCU | ESP32-D0WD V3, revision v3.1, QFN-48 |
| Crystal | 40 MHz |
| Flash | 4 MB SPI, DIO, 40 MHz |
| Unit-under-test MAC | `88:57:21:44:42:0C` |
| Stock framework | ESP-IDF v5.0.2 |
| Stock project | `WoHumi2` |
| Stock developer string | `zkk` |

Results are from one V07 board. Do not assume another revision has the same
pinout without checking.

![Complete V07 board and J3](images/mainboard-j3.jpg)

![ESP32-D0WD V3 and nearby circuitry](images/esp32-closeup.jpg)

The included JPEGs contain no EXIF or GPS profile. They are provided as
hardware-reference photographs.

## J3 programming header

The J3 pinout was established with voltage measurements, ROM boot-mode tests,
UART logs, a successful 4 MB read, and successful firmware installation.

```text
Front of board, component side

        left column          right column
        ┌───────────────┐
        │ TP8   EN      │ TP13  GPIO0
        │ TP12  ESP RX  │ TP10  ESP TX
        │ TP9   3V3     │ TP41  GND
        └───────────────┘
```

| Adapter | J3 | Meaning |
|---|---|---|
| TXD | TP12 | Adapter transmit to ESP32 receive |
| RXD | TP10 | Adapter receive from ESP32 transmit |
| GND | TP41 | Common logic ground |
| RTS | TP13 | GPIO0 in the confirmed test setup |

TP8 is `EN` and can be pulsed low to reset. TP9 is the board's 3.3 V rail but
was not part of the confirmed adapter wiring; never connect adapter 5 V.

The direct RTS-to-GPIO0 arrangement worked in the original test setup.
Control-line polarity and driver behavior vary, and no two-transistor
DevKit-style auto-reset circuit has been confirmed at J3. If automatic entry
fails, hold GPIO0 low while resetting EN manually.

## Recovered GPIO map

| GPIO | Recovered role | Port behavior |
|---:|---|---|
| 2 | Blower run/enable | Asserted only after GPIO17 speed setup succeeds |
| 4 | AIP/TM1668-compatible panel clock | Bit-banged |
| 12 | Wet auxiliary A | Switched with GPIO22 after interlocks |
| 13 | Panel brightness rail | 40 kHz PWM |
| 14 | Optional auto-refill PWM | Forced off |
| 15 | Panel strobe | Bit-banged |
| 16 | Piezo buzzer | Off by default; bounded one-shot only |
| 17 | Blower speed signal | 50% duty at 125/250/400/550 Hz |
| 21 | Inverted panel brightness rail | 40 kHz PWM |
| 22 | Wet auxiliary B | Switched with GPIO12 after interlocks |
| 25 | Optional auto-refill sense/drive-low | Input-only |
| 26 | Panel data/key-scan | Bidirectional bit-bang |
| 27 | Three addressable RGB LEDs | Exposed as an ESPHome light |
| 32 | Output with no normal stock writer found | Held low |
| 33 | Product/variant ADC classifier | Not used as water level |
| 34 | Blower tachometer | Pulse counter input |

GPIO5, GPIO18, GPIO19, and GPIO23 belong to optional 38 kHz RMT paths associated
with the refill/robot subsystem and are untouched. GPIO36 is not the stock
water-level input.

## Recovered control signals

### Blower

The stock path uses GPIO17 as a 50%-duty frequency command and GPIO2 as enable:

| Level | User-facing name | GPIO17 |
|---:|---|---:|
| 1 | Quiet | 125 Hz |
| 2 | Low | 250 Hz |
| 3 | Medium | 400 Hz |
| 4 | High | 550 Hz |

GPIO34 reports tachometer pulses. On the validated level-2 test, the reading
settled near 14,976 pulses/min. That value is an observed signal rate, not a
claim about shaft RPM or pulses per revolution.

### Wet subsystem

The stock firmware toggles GPIO12 and GPIO22 together in ordinary
humidification and keeps them both low during filter drying. The port therefore
names them `Wet auxiliary A/B`; it does not guess which physical load is the
circulation pump and which is the UV load.

The stock task waits approximately eight seconds after blower startup before
asserting the pair. The ESPHome port also requires:

- the first valid panel sample;
- a water level other than `Empty` or `Unknown`;
- non-zero tach feedback above the configured minimum;
- no tach fault; and
- explicit `Wet System Armed` permission.

### Panel and water level

The panel connection uses GPIO15/4/26 for STB/CLK/DIO. The firmware sends
`0x42`, reads five LSB-first key bytes, validates reserved bits, and compresses
four bits from each byte into the stock 20-bit mask.

Water decoding follows the recovered precedence:

```text
mask bit 7 set       -> Empty
otherwise bit 18 set -> Low
otherwise bit 17 clear -> Medium
otherwise            -> High
```

An all-zero key mask is therefore a valid stock `Medium` classification. It is
not proof of bus acknowledgement.

## PCB revision warning

Before trying this component on any board other than V07:

1. photograph both sides;
2. verify ESP32 model and supply voltage;
3. trace or measure J3;
4. confirm safe-state levels on every output;
5. leave wet outputs disarmed; and
6. open an issue with the new board evidence rather than guessing.

See [RE_EVIDENCE.md](../RE_EVIDENCE.md) for binary provenance and confidence
levels.
