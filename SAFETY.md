# Safety

This project controls a mains-powered appliance that deliberately moves water.
The reverse-engineered port is experimental and has not been certified by
SwitchBot or an electrical-safety laboratory.

## Electrical safety

- Unplug mains before opening the enclosure, moving the board, or attaching or
  removing UART leads.
- J3 is a 3.3 V logic interface. Never apply 5 V to its signal or supply pins.
- Do not assume the low-voltage board is isolated from mains merely because it
  measures 3.3 V.
- Do not connect a grounded computer or non-isolated oscilloscope to an
  energized appliance unless the power topology has been verified and you know
  how to do so safely.
- A USB-UART adapter's 3.3 V output may not be able to power the mainboard and
  attached loads. TP9 is identified as 3.3 V, but powering the appliance
  through TP9 is not part of the validated wiring.
- Secure serial leads against slips and shorts before applying any power.
- If safe powered access cannot be arranged with isolated/current-limited
  equipment, have a qualified technician perform the flash.

## Water and mechanical safety

- Perform the first blower tests with the tank empty and `Wet System Armed`
  off.
- Fill only after empty-water detection and blower/tach feedback have been
  confirmed.
- Keep the unit upright and watch the complete first wet test.
- Disconnect power immediately if there is a leak, stalled blower, abnormal
  smell, heating, smoke, repeated reset, continuous buzzer, or unexpected pump
  operation.
- Keep guards and covers fitted whenever practical; a spinning blower can cause
  injury.

## Firmware limitations

The port includes conservative interlocks, but it does not reproduce every
stock safety behavior. In particular:

- tilt behavior has not been validated;
- local buttons and child lock are not implemented;
- the complete display and warning-code system is not implemented;
- exact stock fault thresholds and retry timing are not known;
- the GPIO12/GPIO22 loads are operated as the paired wet outputs seen in the
  stock firmware, but their individual circulation/UV identities are not yet
  proven;
- the S10/auto-refill accessory path is disabled.

`Stop All Hardware` is an emergency software control, not a substitute for
disconnecting mains. Firmware can crash or a semiconductor can fail.

## Recovery

Create two matching, full 4 MB flash backups before overwriting the device.
Store them privately and off-device. A raw backup may contain unique
certificates, NVS data, calibration values, Wi-Fi history, identifiers, and the
device MAC. Do not publish it or attach it to an issue.

Flashing this firmware may void warranties and may make the SwitchBot app,
cloud, BLE, or Matter integrations unavailable until the original image is
restored.
