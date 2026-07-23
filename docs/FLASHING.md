# Detailed flashing and recovery

This document expands the install sequence in the root README. Read
[SAFETY.md](../SAFETY.md) first.

## What flashing changes

The first ESPHome install overwrites the stock bootloader, partition table,
application, and data areas used by SwitchBot. The original app/cloud/BLE/Matter
behavior will not remain available. A verified full-flash backup is the only
recovery artifact this project can use.

Do not publish a stock backup. Apart from copyright concerns, it may contain
unit-specific NVS, credentials, certificates, calibration, and identifiers.

## Required equipment

- 3.3 V USB-UART adapter with TX, RX, GND, and preferably RTS
- insulated leads or a secured programming fixture
- computer with ESPHome 2026.7.0 or newer
- esptool v4 or v5
- a safe, isolated way to power the board during serial access
- multimeter for orientation and voltage checks

No adapter 5 V lead is used.

## J3 wiring

```text
Front of board, component side

        left column          right column
        ┌───────────────┐
        │ TP8   EN      │ TP13  GPIO0
        │ TP12  ESP RX  │ TP10  ESP TX
        │ TP9   3V3     │ TP41  GND
        └───────────────┘
```

```text
Adapter TXD -> TP12
Adapter RXD -> TP10
Adapter GND -> TP41
Adapter RTS -> TP13     # confirmed test arrangement
```

See [HARDWARE.md](HARDWARE.md) and the
[mainboard photograph](images/mainboard-j3.jpg).

## Enter the ESP32 ROM bootloader

GPIO0 must be low at reset:

1. Hold TP13/GPIO0 low.
2. Pull TP8/EN low briefly.
3. Release EN.
4. Release GPIO0.

If RTS is attached to GPIO0, esptool may control it, but J3 does not have a
confirmed complete auto-reset circuit. Be ready to pulse EN manually.

A successful ROM-loader connection normally reports the ESP32 type, crystal,
MAC, and flash ID:

```sh
esptool.py --chip esp32 --port /dev/ttyUSB0 flash_id
```

For esptool v5:

```sh
esptool --chip esp32 --port /dev/ttyUSB0 flash-id
```

If synchronization fails:

- swap/check TX and RX;
- verify common ground;
- confirm the adapter is 3.3 V logic;
- retry the GPIO0/EN sequence;
- close any serial monitor using the port; and
- lower the baud rate.

## Make and verify the stock backup

The flash is 4 MB (`0x400000`). Read the entire address range twice.

esptool v4:

```sh
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  read_flash 0x000000 0x400000 wohumi2-backup-a.bin

esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  read_flash 0x000000 0x400000 wohumi2-backup-b.bin
```

esptool v5:

```sh
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  read-flash 0x000000 0x400000 wohumi2-backup-a.bin

esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  read-flash 0x000000 0x400000 wohumi2-backup-b.bin
```

Verify:

```sh
wc -c wohumi2-backup-a.bin wohumi2-backup-b.bin
sha256sum wohumi2-backup-a.bin wohumi2-backup-b.bin
cmp wohumi2-backup-a.bin wohumi2-backup-b.bin
```

Expected size for each file:

```text
4194304 bytes
```

Matching hashes and a successful `cmp` protect against a flaky serial read.
Keep the hash beside an offline copy.

## Configure ESPHome

```sh
cp secrets.example.yaml secrets.yaml
```

Replace every placeholder. The API key must be a valid base64-encoded 32-byte
key. Generate one with:

```sh
openssl rand -base64 32
```

Set the optional Home Assistant sensor IDs in the YAML substitutions. If the
paired meter is unavailable, manual speed modes still work but target/auto
logic cannot measure the room.

Validate before compiling:

```sh
esphome config switchbot-humidifier.yaml
esphome compile switchbot-humidifier.yaml
```

## First serial upload

The simplest supported path is:

```sh
esphome run switchbot-humidifier.yaml --device /dev/ttyUSB0
```

If a Dashboard or CLI build supplies a merged `firmware.factory.bin`, the
manual v4 upload is:

```sh
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x0 firmware.factory.bin
```

The esptool v5 spelling is:

```sh
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB \
  0x0 firmware.factory.bin
```

The offset `0x0` is correct only for the merged factory image. ESP-IDF component
images have different offsets; use ESPHome's generated flash command if you do
not have the merged image.

After the write:

1. release/disconnect GPIO0;
2. reset EN;
3. open logs at 115200 baud or with `esphome logs`;
4. confirm the runtime announces the W3902310/V07 component; and
5. continue with [COMMISSIONING.md](COMMISSIONING.md).

## Later OTA uploads

After Wi-Fi and the native API are working:

```sh
esphome run switchbot-humidifier.yaml
```

Keep the serial header accessible until the port has been stable through
multiple boots and power failures.

## Restore the stock image

Use only the verified full backup from this exact appliance:

```sh
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  write_flash 0x000000 wohumi2-backup-a.bin
```

For esptool v5:

```sh
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  write-flash 0x000000 wohumi2-backup-a.bin
```

Writing a complete backup covers the full 4 MB address space; a separate erase
is not required. When finished, release GPIO0 and reset EN.

## Common failures

### `Failed to connect` or no serial response

The ESP32 is not in download mode, TX/RX are wrong, another application owns
the port, or the board is not powered correctly. Recheck GPIO0-low-at-reset.

### Upload fails partway through

Retry at a lower baud, such as `115200`. Check supply stability and lead length.

### Upload succeeds but the appliance does not boot

Confirm that a merged factory binary was written at `0x0`, that DIO/40 MHz/4 MB
were selected, and that GPIO0 was released before reset.

### ESPHome boots but the blower does not run

Do not arm the wet system. Capture startup and fan logs, then verify that the
runtime reports an actual GPIO17 frequency and that GPIO34 tach pulses appear.

### Continuous loud tone

Press `Stop All Hardware` and disconnect power. GPIO16 is the buzzer. The
current component never uses it during normal fan/wet operation and clamps the
test button to a short one-shot; a continuous tone indicates the wrong build,
wrong pin mapping, or a fault.

## Primary references

- [Espressif boot-mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html)
- [Espressif esptool v4 basic commands](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/esptool/basic-commands.html)
- [Espressif esptool latest flashing guide](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/flashing-firmware.html)
- [ESPHome command-line guide](https://esphome.io/guides/getting_started_command_line/)
