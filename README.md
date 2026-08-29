# VGreen EVO offline controller

This branch turns an M5Stack ATOM Lite with an Atomic RS485 Base into a local controller for a Century/Regal VGreen EVO motor. The controller runs the selected stock-style schedule on the ATOM itself. Home Assistant, Wi-Fi, Internet access, and wall-clock time are optional.

The intended first target is the VGreen EVO EVC165. The implementation keeps the original `centuryvspump` ESPHome component and adds a local state machine that replaces the useful behavior disabled when an EVO is placed in Schedule Set D.

## What it does

- Includes all eight Schedule A and all eight Schedule B profiles from the EVO guide.
- Restarts at hour one after a controller power cycle, matching the EVO's documented power-recovery behavior.
- Primes at 3450 RPM for three minutes on eligible stopped-to-running schedule transitions.
- Supports a 3450 RPM override with a 24-hour timeout, an indefinite maintenance pause, resume, and schedule restart.
- Reasserts the desired run state locally so a Home Assistant outage does not stop control.
- Latches a reported motor fault and requires a verified manual resume before any restart.
- Disables ESPHome's Wi-Fi and native-API disconnect reboot timers.
- Uses an authenticated local web page plus optional encrypted Home Assistant API access.
- Never writes the motor's configuration or flash registers.

Control priority is: maintenance pause, communication safety, latched motor fault, freeze request, manual override, priming, then the active schedule.

The first flash deliberately restores into **Maintenance Pause**. After the physical installation is verified, turn that switch off once from Home Assistant or the local web page. That commissioning choice is saved; later power cycles safely synchronize with the pump and then restart the selected schedule at hour one.

That hour-one restart is tied to the ATOM controller reboot. If only the motor or RS485 link drops while the ATOM stays powered, the controller keeps its current 24-hour phase and safely resynchronizes before sending another run request.

## Validation status

| Area | Status |
| --- | --- |
| Schedule tables and boundaries | Host unit tested |
| ESPHome configuration | Compiled with the pinned version in `requirements-dev.txt` |
| ESPHome 2026.7+ response layout | Supported |
| EVC165 serial control | Reported working by current upstream field testing; not yet verified on this user's pump |
| Motor ambient-temperature register | Implemented as an experimental freeze input; must be verified on the actual EVC165 |
| Wiring, switch positions, dry-run protection, and real pump behavior | Physical verification still required |

Do not treat a successful compile as proof that the actual pump is safe to run. The first live test must verify the exact motor label, connector labels, switch orientation, serial replies, direction/flow, and temperature reading.

## Safety boundary

Pool equipment contains line voltage and a pump can be damaged by running dry.

- Turn off and lock out the breaker before opening the motor cover. Follow the EVO guide's required wait time before touching switches or terminals.
- Photograph the motor label, current switch bank, and labeled RS485 connector before changing anything. Match terminal labels, not remembered wire colors.
- Flash the ATOM over USB while it is disconnected from pump power and RS485 wiring.
- Fill and prime the hydraulic system before any command that can start the motor.
- The ATOM button and web controls are software controls, not an emergency disconnect.
- Schedule Set D disables the motor's native schedules, priming, freeze protection, and normal button behavior. Do not select Set D until this controller and its temperature policy are ready for the physical test.

## Start here

Follow [INSTALL.md](INSTALL.md). The complete reference configuration is [vgreen_evo_atom.yaml](vgreen_evo_atom.yaml), with credentials kept in an untracked `secrets.yaml` copied from [secrets.example.yaml](secrets.example.yaml).

The exact stock schedule data and behavior assumptions are documented in [docs/STOCK_BEHAVIOR.md](docs/STOCK_BEHAVIOR.md).

Run the host schedule tests with `tests/run_host_tests.sh`. Validate and compile the complete ESP32 firmware with the commands in `INSTALL.md`.

## Repository layout

- `components/centuryvspump/` — hardened Gen3 serial transport and ESPHome entities.
- `components/vgreen_evo_controller/` — offline schedule and behavior state machine.
- `tests/` — host tests for every stock schedule boundary.
- `vgreen_evo_atom.yaml` — M5Stack ATOM Lite reference firmware.
- `example_century_vs_pump.yaml` — original minimal upstream example, retained for reference.

## Sources and upstream credit

This work builds on [gazoodle/CenturyVSPump](https://github.com/gazoodle/CenturyVSPump). Protocol behavior comes from the bundled Gen3 EPC document and is cross-checked against the [VGreen EVO installation and user guide](https://electricmotorwarehouse.com/content/PDF/EVO%20Install%20Manual.pdf). ESPHome 2026.7+ response handling follows the hardware-tested findings in [upstream PR #20](https://github.com/gazoodle/CenturyVSPump/pull/20), and current EVC/EVO command ordering is corroborated by [upstream issue #17](https://github.com/gazoodle/CenturyVSPump/issues/17).

Use this project at your own risk. It sends real motor commands over an unsupported interface and has not yet completed physical validation on the target installation.
