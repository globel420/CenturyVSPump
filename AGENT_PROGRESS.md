# Agent Progress

## Current Resume Point
- Last updated: 2026-08-28 20:33
- Active goal: Publish autonomous M5Stack firmware that mimics VGreen EVO stock behavior while Home Assistant remains optional
- Current status: Firmware and documentation complete; final validation passed; ready to commit and push vgreen-controller-dev
- Files touched: .gitignore, README.md, INSTALL.md, docs/STOCK_BEHAVIOR.md, components/centuryvspump/, components/vgreen_evo_controller/, tests/, vgreen_evo_atom.yaml, secrets.example.yaml, requirements-dev.txt, .github/workflows/validate.yml
- Commands run: tests/run_host_tests.sh; .venv/bin/esphome config vgreen_evo_atom.yaml; .venv/bin/esphome compile vgreen_evo_atom.yaml; git diff --check; redacted secret-signature scan
- Validation state: PASS: all 16 schedule profiles and boundaries; ESPHome 2026.8.1 config and full firmware compile; no compiler warnings; 1,022,927-byte image, 55.7 percent app flash, 28.1 percent DRAM; no secret signatures in commit candidates.
- Next action: Stage reviewed source files, commit Add autonomous VGreen EVO controller, push origin vgreen-controller-dev, verify remote commit, then hand off INSTALL.md to the other computer.
- Blockers: Physical EVC165 label, switch bank, RS485 polarity, ambient-register units, dry-run protection, and real motor behavior still require photos and supervised hardware validation.
- Do not repeat: Do not claim physical pump validation from compilation. Do not change DIP switches or wiring without exact labeled photos and breaker lockout. Do not commit secrets.yaml, build output, or temporary research files.

## Chapter Log

### 2026-08-28 19:31 - offline stock behavior controller kickoff
- Goal: Implement autonomous M5Stack firmware for the VGreen EVO EVC165 that mimics useful stock behavior and keeps Home Assistant optional
- Progress: Created vgreen-controller-dev from main; confirmed target is local schedules, priming, freeze handling, manual control, and power/network-loss recovery
- Decisions: none recorded
- Files changed: AGENT_PROGRESS.md
- Commands/results: git branch verified vgreen-controller-dev tracks origin; live repository and prior device findings reviewed
- Validation: No firmware validation yet
- Next: Inspect CenturyVSPump and existing standalone ATOM firmware; verify official EVC165 behavior and define fail-safe state machine
- Blockers: Physical wiring, DIP changes, and pump validation remain blocked until exact model label and connector/switch orientation are visually confirmed

### 2026-08-28 19:40 - stock behavior verified and architecture selected
- Goal: Implement autonomous M5Stack control that mimics VGreen EVO stock schedules while Home Assistant remains optional
- Progress: Verified Schedule Sets A/B, 3-minute 3450 RPM priming, 39F one-hour 1725 RPM freeze behavior, override/pause semantics, and hour-one restart after power loss. Selected boot-relative on-device clock and separate controller state machine. Found motor ambient temperature at sensor page 0 address 0x07 with scale 128 as a candidate local freeze source.
- Decisions: none recorded
- Files changed: AGENT_PROGRESS.md
- Commands/results: Rendered and inspected EVO manual schedule tables; inspected Gen3 protocol sensor tables; reviewed ESPHome API/WiFi offline timeout behavior
- Validation: Manual tables visually checked; no code/build validation yet
- Next: Implement pure stock schedule engine, ESPHome autonomous controller, bus hardening, and exact example configuration
- Blockers: EVC165 support for Gen3 serial commands and ambient sensor remains bench/physical validation only; wiring and DIP changes still require device photos

### 2026-08-28 19:59 - autonomous firmware implementation and schedule tests
- Goal: Implement and publish autonomous M5Stack firmware that mimics VGreen EVO stock schedules while Home Assistant remains optional
- Progress: Added all A1-A8 and B1-B8 stock profiles, offline monotonic controller, maintenance pause, override, priming, freeze input policy, reference ATOM firmware, install/safety documentation, pinned ESPHome tooling, and CI validation.
- Decisions: Use boot-relative 24-hour schedule; first flash defaults to persistent maintenance pause; freeze is opt-in until actual temperature register validation; control commands never write motor configuration.
- Files changed: .gitignore, README.md, INSTALL.md, docs/STOCK_BEHAVIOR.md, components/vgreen_evo_controller/, tests/, vgreen_evo_atom.yaml, secrets.example.yaml, requirements-dev.txt, .github/workflows/validate.yml
- Commands/results: Installed ESPHome 2026.8.1; esphome config vgreen_evo_atom.yaml passed; tests/run_host_tests.sh passed.
- Validation: All 16 stock profiles tested at every second with strict C++ warnings; ESPHome configuration schema is valid. Full ESP32 link compile awaits final transport APIs.
- Next: Finish confirmed Set Demand then Go transport sequence and motor fault API; run full ESPHome compile, inspect diff, commit, push, and verify GitHub branch.
- Blockers: Physical EVC165 wiring, DIP, ambient temperature units, and pump behavior require exact device photos and supervised hardware testing on the other computer.

### 2026-08-28 20:33 - autonomous firmware validated and ready to publish
- Goal: Publish autonomous M5Stack firmware that mimics VGreen EVO stock behavior while Home Assistant remains optional
- Progress: Implemented all A1-A8 and B1-B8 schedules, ACK-gated startup and motion control, local pause/override/resume, verified-status and motor-fault gates, conservative opt-in freeze handling, M5Stack reference YAML, install guide, and CI compile workflow.
- Decisions: Use boot-relative 24-hour phase; stock Pause leaves phase advancing; first flash defaults to persistent maintenance pause; Run is confirmed only after Set Demand and Go ACKs; startup requires known no-fault status plus confirmed Stop; unknown status fails closed; freeze remains opt-in.
- Files changed: .gitignore, README.md, INSTALL.md, docs/STOCK_BEHAVIOR.md, components/centuryvspump/, components/vgreen_evo_controller/, tests/, vgreen_evo_atom.yaml, secrets.example.yaml, requirements-dev.txt, .github/workflows/validate.yml
- Commands/results: tests/run_host_tests.sh; .venv/bin/esphome config vgreen_evo_atom.yaml; .venv/bin/esphome compile vgreen_evo_atom.yaml; git diff --check; redacted secret-signature scan
- Validation: PASS: all 16 schedule profiles and boundaries; ESPHome 2026.8.1 config and full firmware compile; no compiler warnings; 1,022,927-byte image, 55.7 percent app flash, 28.1 percent DRAM; no secret signatures in commit candidates.
- Next: Stage reviewed source files, commit Add autonomous VGreen EVO controller, push origin vgreen-controller-dev, verify remote commit, then hand off INSTALL.md to the other computer.
- Blockers: Physical EVC165 label, switch bank, RS485 polarity, ambient-register units, dry-run protection, and real motor behavior still require photos and supervised hardware validation.
