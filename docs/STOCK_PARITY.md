# Stock-firmware parity

“Working” and “identical to stock” are different milestones. This matrix
separates binary-derived behavior, real-device validation, product-document
behavior, and remaining gaps.

## Core hardware

| Behavior | Basis | ESPHome status |
|---|---|---|
| GPIO2 blower enable | Stock-image disassembly + hardware | Implemented and validated |
| GPIO17 blower frequency command | Stock-image disassembly + hardware | Implemented and validated |
| 125/250/400/550 Hz levels | Stock-image mode table | Implemented; level 2 validated |
| GPIO34 tach input | Stock-image GPIO setup + hardware | Implemented and validated |
| GPIO12/GPIO22 paired wet outputs | Stock-image state path + wet-filter test | Implemented and validated as a pair |
| Eight-second wet-start delay | Stock task timing + runtime log | Implemented and validated |
| Empty/low water classification | Stock key-map function + hardware | Implemented and validated |
| Medium/high classification | Stock key-map function | Implemented; not yet physically range-tested |
| Panel brightness rails | Stock LEDC configuration | Implemented; visual parity not fully recorded |
| GPIO16 buzzer | Stock LEDC setup/callers | Safe one-shot only; stock patterns missing |
| GPIO27 RGB LEDs | Hardware/stock setup | Exposed as a light; stock meanings missing |

## Operating behavior

| Behavior | Status and caveat |
|---|---|
| Manual four-speed humidification | Working through ESPHome/HA |
| Filter drying starts after a wet run | Implemented and observed |
| Filter-dry wet outputs remain off | Implemented |
| Filter-dry duration | Configured to 55 min; current SwitchBot docs say 70 min; unresolved |
| Target humidity 40–70%, default 60% | Implemented in YAML using an HA sensor |
| Temperature-dependent Auto target | Implemented in YAML; not derived end-to-end from hardware tests |
| Sleep step-down | Implemented in YAML; not validated against every stock transition |
| 70% over-humidity stop | Implemented in YAML when paired humidity is available |
| 240-hour filter reminder | Implemented using wet-output runtime |
| Power-loss/restoration behavior | Basic output-safe startup implemented; full stock state matrix not replicated |

## Deliberate replacements

| Stock feature | Replacement |
|---|---|
| SwitchBot mobile app/cloud | Home Assistant and native ESPHome API |
| Stock OTA | ESPHome OTA |
| Stock Wi-Fi provisioning | ESPHome fallback AP/captive portal |
| BLE/Matter integration | Not included |
| Proprietary telemetry | ESPHome diagnostics |

## Not yet reconstructed

- local front-panel button assignments and actions;
- segment/icon RAM map and display rendering;
- complete buzzer melodies and alert cadence;
- exact RGB color/animation state machine;
- child lock;
- all warning/error codes;
- tank-removed and tilt behavior;
- exact tach thresholds, retries, and failure timing;
- exact V07 filter-dry duration;
- optional S10/auto-refill IR and solenoid protocol;
- every persistence, schedule, and power-recovery rule;
- factory diagnostics and production test modes.

## Safety additions that are not stock-parity claims

The port adds a first-install `Wet System Armed` gate, a configurable tach
startup timeout, an emergency `Stop All Hardware` button, and a bounded buzzer
test. These make commissioning safer but are ESPHome-specific behavior.

## Current conclusion

The project reproduces enough high-confidence behavior to run the blower,
detect water, circulate water, wet the filter, and dry the filter on the tested
V07 unit. It should be described as a **working core port with fail-closed
commissioning**, not an exact stock-firmware replica.

Closing the parity gap requires controlled captures from stock firmware and
repeatable tests on more than one unit. See [CONTRIBUTING.md](../CONTRIBUTING.md).
