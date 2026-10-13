# VGreen EVO offline controller — 0.2.0-rc1

This branch turns an M5Stack ATOM Lite with an Atomic RS485 Base into a local controller for a Century/Regal VGreen EVO motor. The controller runs the selected stock-style schedule on the ATOM itself. Home Assistant, Wi-Fi, Internet access, and wall-clock time are optional.

The intended first target is the VGreen EVO EVC165. The implementation keeps the original `centuryvspump` ESPHome component and adds a local state machine that replaces the useful behavior disabled when an EVO is placed in Schedule Set D.

## What it does

- Includes all eight Schedule A and all eight Schedule B profiles from the EVO guide.
- Restarts at hour one after a controller power cycle, matching the EVO's documented power-recovery behavior.
- Primes at 3450 RPM for three minutes on eligible stopped-to-running schedule transitions.
- Supports a 3450 RPM override with a 24-hour timeout, an indefinite maintenance pause, resume, and schedule restart.
- Reasserts the desired run state locally so a Home Assistant outage does not stop control.
- Stops for a motor fault and resumes only after the motor reports a recognized, fresh no-fault status. Unknown startup responses block motion without creating a permanent software fault.
- Disables ESPHome's Wi-Fi and native-API disconnect reboot timers.
- Uses an authenticated local web page plus optional encrypted Home Assistant API access.
- Never writes the motor's configuration or flash registers.

Control priority is: local Schedule Set mode or Pause, communication safety, motor fault, freeze request, manual override, priming, then the active schedule.

**Powering the M5 starts the saved schedule at Hour 1 automatically.** There is no saved startup Pause and no requirement to press Reset, connect Wi-Fi, or connect Home Assistant. Run follows a confirmed Stop and fresh, healthy pump status; the controller retries while the pump is waking up. Every reboot clears temporary Pause/Override/programming modes. The pump must be ready to run before installing or powering this firmware.

That hour-one restart is tied to the ATOM controller reboot. If only the motor or RS485 link drops while the ATOM stays powered, the controller keeps its current 24-hour phase and safely resynchronizes before sending another run request.

## Use the M5Stack front button

The **large front button on the ATOM** replaces the EVO button for these controls. The small side Reset button is not an operating control. Everything in this table runs on the M5 with no Home Assistant connection.

| Current mode | Button input | Result |
| --- | --- | --- |
| Operating | One press | 3450 RPM Override, up to 24 hours |
| Operating | Two presses | Indefinite Pause; schedule time continues |
| Operating | Three presses | LED reports the selected profile 1–8 |
| Operating | Hold at least 3 seconds, then release | Schedule Set mode; motor stops |
| Pause or Override | Hold at least 3 seconds, then release | Return to the current schedule position |
| Schedule Set | Press 1–8 times; hold at least 3 seconds and release | Save that profile locally, then report it with the LED |
| Schedule Set | No input for 45 seconds | Keep the previous profile and return to operation |

The decoder waits 500 ms after the last short press to distinguish one/two/three presses. Holds act on release. A button held during boot must be released before it can issue an action. Indicator timing follows manual sections 4.2–4.5; red and amber are additional fault/link diagnostic colors. `Controller Button` and `Last Controller Button Action` expose input diagnostics without participating in control.

## Optional Home Assistant timing

The `set_schedule_phase` native API action accepts an integer `seconds` from 0 through 86399. Hour H begins at `(H - 1) * 3600`. It changes only the schedule clock: Pause, Override and fault checks remain in force. A correction into a scheduled-off period stops priming; repeated corrections do not restart a running prime.

The deployed HA controls use a 1–24 hour dropdown and **Reset to Default Timing**. Default Hour 1 begins at **11:00 PM Pacific**; a manual hour selection keeps a new timing reference. The 11 PM reference belongs to Home Assistant, not the standalone firmware. See [Home Assistant controls](docs/HOME_ASSISTANT.md) for reusable configuration, live results and the remaining physical checks.

## Validation status

| Area | Status |
| --- | --- |
| Schedule tables and boundaries | Host unit tested |
| Manual button, LED patterns, startup and phase correction | Host tested; controller tests use an explicitly separate transport double |
| ESPHome configuration | Compiled with the pinned version in `requirements-dev.txt` |
| ESPHome 2026.7+ response layout | Supported |
| Installed candidate | 0.2.0-rc1 / ESPHome 2026.9.1; a supervised shared-supply power cycle started Hour 1, followed by HA alignment |
| EVC165 serial control | User-observed run/stop behavior corroborated by live RPM, connection and fault readings |
| HA hour selector/default reset | Real device phase changes and timing-reference persistence verified; profile and running speed preserved |
| Field acceptance, 2026-10-12 | Automatic cold start, Override/resume, Pause/resume, three-minute prime and scheduled 5 PM B1 stop observed; user concluded testing as working |
| Additional field coverage | Triple-press report, physical programming/persistence and isolated offline operation were not exercised to completion; their implementation remains host-tested, not field-verified in this session |
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

Run all host tests with `tests/run_host_tests.sh`. Validate and compile the complete ESP32 firmware with the commands in `INSTALL.md`. CI builds ESPHome 2026.9.0 and 2026.9.1. CI binaries contain public example credentials and are **reference-only, not images for the installed controller**.

## Repository layout

- `components/centuryvspump/` — hardened Gen3 serial transport and ESPHome entities.
- `components/vgreen_evo_controller/` — offline schedule and behavior state machine.
- `tests/` — host tests for every stock schedule boundary.
- `vgreen_evo_atom.yaml` — M5Stack ATOM Lite reference firmware.
- `home_assistant/` — native HA controls and narrowly scoped dashboard migration tool.
- `tools/ha_api.mjs` — credential-file-based HA readback/configuration client.
- `example_century_vs_pump.yaml` — original minimal upstream example, retained for reference.

## Sources and upstream credit

This work builds on [gazoodle/CenturyVSPump](https://github.com/gazoodle/CenturyVSPump). Protocol behavior comes from the bundled Gen3 EPC document and is cross-checked against the [VGreen EVO installation and user guide](https://electricmotorwarehouse.com/content/PDF/EVO%20Install%20Manual.pdf). ESPHome 2026.7+ response handling follows the hardware-tested findings in [upstream PR #20](https://github.com/gazoodle/CenturyVSPump/pull/20), and current EVC/EVO command ordering is corroborated by [upstream issue #17](https://github.com/gazoodle/CenturyVSPump/issues/17).

Use this project at your own risk. It sends real motor commands over an unsupported interface. The accepted field test covers the behaviors listed above, not universal power-supply reliability, electrical certification or experimental freeze protection.

## UI design follow-up

A separate, non-live Superdesign preview is being reviewed for iPhone 17 and desktop. Priorities are large Pause, Override and Resume Schedule controls, a compact RPM dial with a history line, the retained schedule-position graphic, clear telemetry provenance and the original Schedule A/B reference images. The preview does not send pump commands and has not replaced the live Home Assistant dashboard. Private design state and captured test data remain outside Git.
