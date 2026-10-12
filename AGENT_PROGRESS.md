# Agent Progress

## Current Resume Point
- Last updated: 2026-10-12 14:22
- Active goal: Implement manual-equivalent controls on the M5Stack front button, standalone schedules and power recovery, plus optional HA hour selection and timing reset
- Current status: BLOCKED on user-supervised physical verification after three consecutive goal turns with the same missing readiness/observations. Software implementation is deployed and published: firmware d2c66e1, HA source a75c054, both dashboards and timing operations verified, GitHub CI passed both versions. Do not continue status-only checks without new user input or external evidence.
- Files touched: home_assistant/controls.json, home_assistant/dashboard_update.mjs, tools/ha_api.mjs, tests/test_home_assistant.mjs, CI, README, docs/HOME_ASSISTANT.md, CHANGELOG, AGENT_PROGRESS.md
- Commands run: tests/run_host_tests.sh PASS; node --test tests/test_home_assistant.mjs PASS; node --check tools/ha_api.mjs PASS; git diff --check PASS; native HA edits with API readbacks and semantic comparisons
- Validation state: Device metadata confirms 0.2.0-rc1 / ESPHome 2026.9.1. Automatic warm-start Hour 1/priming observed. Actual hour and phase follow HA selection/default reset; timing automation enabled and periodic triggers observed. Both dashboard readbacks exactly match reviewed payloads; no retired timing references in any available dashboard. Cold-power and physical button operation remain unverified.
- Next action: Await user's readiness/observations for a supervised cold power cycle and physical M5 button tests. Do not reinstall the running candidate or repeatedly rerun completed audits. With confirmation, observe the normal enclosure supply, no Reset press, and actual button/flow behavior; preserve diagnostics if it fails.
- Blockers: User presence is required for a true power-cycle, physical button and supervised hydraulic/offline checks. Experimental freeze sensor equivalence remains outside the verified feature set. No hardware test has been claimed complete.
- Do not repeat: Do not simplify the button mapping; do not hard-code 10:50 PM in firmware; do not publish configured firmware binaries or credentials. macOS credential helper stalled; scoped gh auth git-credential push succeeded. Browser raw-GitHub navigation was blocked; local reviewed candidate was used.

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

### 2026-10-12 10:29 - recover current firmware and button behavior
- Goal: Identify the exact newest VGreen EVO firmware and prepare a reliable physical-button repair
- Progress: Verified current source and newest local compiled image are ESPHome firmware 0.1.0 on vgreen-controller-dev commit a504b56; GitHub matches local; current button uses long, triple, double, and single-click actions; boot performs safe Stop and restores maintenance pause, default true
- Decisions: none recorded
- Files changed: AGENT_PROGRESS.md
- Commands/results: Spotlight and scoped file search; Git branch/commit comparison; GitHub repo/branch/actions inspection; SHA-256 and binary string fingerprints; source line inspection
- Validation: PASS: local HEAD equals GitHub vgreen-controller-dev a504b56; GitHub CI succeeded; newest local image is d55ad1e7...; no releases or GitHub artifacts; physical installed image and live button behavior not yet verified
- Next: Confirm desired one-button behavior, then simplify the YAML button logic and add host-level behavior tests before compiling
- Blockers: Need user choice for button action semantics; actual installed image and pump-side behavior require live device validation

### 2026-10-12 10:30 - identify likely cold-start gate
- Goal: Repair cold-power startup and simplify reliable physical-button control on the current autonomous firmware
- Progress: Confirmed user-provided GitHub branch is local HEAD a504b56. Reviewed startup and transport state machines: autonomous Run is gated on healthy RS485, recent recognized no-fault status, confirmed Stop, and settle time; early pump-unready timing is a plausible explanation for reset-after-power symptom but is not yet live-proven.
- Decisions: none recorded
- Files changed: AGENT_PROGRESS.md
- Commands/results: GitHub API/CLI branch verification; source review of VGreenEvoController and CenturyVSPump transport state machines
- Validation: PASS: source/remote identity verified. NOT YET VERIFIED: exact image currently flashed, cold-boot serial evidence, button input events, actual pump response.
- Next: Confirm simplified button contract, implement it, add tests; then capture supervised cold-power serial logs before changing startup retries.
- Blockers: Need desired button action semantics; live cold-boot diagnosis requires access to serial/logging without unsafe simultaneous USB and pump power.

### 2026-10-12 10:32 - inspect live Home Assistant pump page
- Goal: Repair cold-power startup and simplify reliable physical-button control using the authoritative Home Assistant firmware configuration
- Progress: Verified device is online and running firmware 0.1.0 built with ESPHome 2026.9.0. Authoritative config is vgreen-pump.yaml and pulls globel420/CenturyVSPump ref vgreen-controller-dev. Current live state: communication connected, no motor fault, schedule B1 running at target/actual 1550 RPM, maintenance pause off, max-RPM override off. Physical ATOM button is internal and has no HA entity/history. Device Builder offers 2026.9.1 update but it was not applied.
- Decisions: none recorded
- Files changed: AGENT_PROGRESS.md
- Commands/results: Read-only Home Assistant Pool Pump dashboard, Advanced tab, device page, and ESPHome Builder configuration inspection
- Validation: PASS: live device metadata, entity states, YAML filename, Git source ref, board, UART pins, pump address, and online status visibly confirmed. NOT VERIFIED: raw button press events and cold-power startup log.
- Next: Choose the simplified physical-button behavior, then update a safe working copy of vgreen-pump.yaml and component tests before any compile or flash
- Blockers: Home Assistant cannot observe the internal GPIO button directly; raw button verification requires temporary diagnostics/logging or a controlled physical test.

### 2026-10-12 10:43 - plan manual behavior and HA clock sync
- Goal: Plan a VGreen EVO firmware release that matches owner-manual button and power-recovery behavior, resynchronizes schedule phase from Home Assistant time, and stays published on GitHub
- Progress: Manual verified: power restore starts hour one; one press starts 3450 RPM override up to 24h; two presses enter indefinite Pause while schedule advances; three presses report profile; 3s hold exits Pause/Override, and from normal operation enters Schedule Set Mode. ESPHome homeassistant time plus on_time_sync can correct device phase when API reconnects. Live HA currently uses a later schedule anchor than the checked-in dashboard YAML, so anchor must be resolved before implementation.
- Decisions: none recorded
- Files changed: AGENT_PROGRESS.md
- Commands/results: Reviewed VGreen EVO manual sections 4 and 8; reviewed live HA state/config and local SMUD automation; reviewed official ESPHome Home Assistant time/on_time_sync documentation
- Validation: PASS: manual semantics and supported HA-native time synchronization path confirmed. NOT STARTED: implementation, tests, compile, CI, release, OTA, physical validation.
- Next: Confirm Hour 1 wall-clock anchor, then implement the approved plan in source and tests; push only after local validation
- Blockers: Live HA automation indicates 10:50 PM while checked-in dashboard YAML says 10:45 PM; choose one anchor or make it a configurable substitution with 10:50 PM as current live default.

### 2026-10-12 10:43 - treat current button decoder as failed
- Goal: Plan and deliver a manual-faithful, physically verifiable VGreen EVO button and power-recovery firmware release
- Progress: Do not preserve current YAML on_multi_click logic. Its 50-350 ms cadence is too narrow for reliable human input and long-hold context is incomplete versus the owner manual. Plan now begins with raw GPIO39 press/release diagnostics and a tested C++ button state machine before pump actions are trusted.
- Decisions: none recorded
- Files changed: AGENT_PROGRESS.md
- Commands/results: No new diagnostic command; recorded user-observed live failure against inspected source timing
- Validation: USER EVIDENCE: current button behavior fails in physical use. ROOT CAUSE NOT YET PROVEN: GPIO actuation, electrical input, debounce, click timing, and action dispatch must be isolated.
- Next: Approve plan and 10:50 PM Hour 1 anchor; then implement raw button diagnostics/state machine, host tests, boot recovery, HA time sync, and GitHub release workflow.
- Blockers: Need Hour 1 anchor confirmation; physical button test requires user present and pump-safe conditions.

### 2026-10-12 10:44 - clarify exact EVO button emulation
- Goal: Make the M5Stack button emulate the VGreen EVO motor button while separately improving power recovery and HA schedule-phase synchronization
- Progress: User explicitly wants the M5Stack button to behave like the EVO button, not a simplified stop/resume mapping. Preserve manual context-sensitive behavior: 1 press Override, 2 Pause, 3 Schedule Check, long hold exits Pause/Override or enters Schedule Set from Operating, with 1-8 presses plus long hold to select a schedule.
- Decisions: none recorded
- Files changed: AGENT_PROGRESS.md
- Commands/results: No implementation command; recorded clarified functional contract
- Validation: Requirement clarified against owner-manual sections 4.1-4.5. Physical button decoder remains unverified.
- Next: Confirm the HA Hour 1 anchor, then implement and test exact EVO button emulation plus separate boot/time-sync changes.
- Blockers: Hour 1 anchor remains 10:50 PM live versus 10:45 PM checked-in YAML.

### 2026-10-12 11:34 - implement clarified standalone controls
- Goal: M5Stack front button replaces the EVO button using the EVO manual as the control reference, including when HA never connects.
- Progress: Requirements settled: HA hour dropdown 1-24 keeps manually selected timing; Reset to Default Timing restores an 11 PM Pacific reference; remove the SMUD switch and forced-B1 scheduling.
- Decisions: Firmware owns button interpretation, LED patterns, saved profile, priming and schedules. HA timing correction is optional and must not release Pause or Override. Preserve motor/communication checks. Cold-start root cause remains unknown.
- Files changed: AGENT_PROGRESS.md
- Commands/results: Source/manual inspection; no new firmware compiled or flashed
- Validation: Hardware button, cold-start and freeze behavior remain unverified
- Next: Implement local behavior and meaningful host tests, then build and publish reviewed source
- Blockers: No live pump changes until the built candidate is ready for supervised validation

### 2026-10-12 11:50 - local candidate tested and compiled
- Goal: Deliver standalone manual controls and automatic Hour 1 startup, with optional HA phase correction.
- Progress: Added GPIO39 decoder, complete local Schedule Set workflow and LED patterns, local saved-profile callback, automatic boot without persisted Pause, reset/uptime/button diagnostics, and phase API. Unknown status no longer creates a motor fault; recognized motor fault clearing allows resynchronization. Corrected phase-to-off during priming.
- Decisions: Keep freeze experimental. No physical repair claim from host tests. Firmware images built with example credentials are reference artifacts only. Live YAML backup is private under .cache/recovery/ and matches the source copied from the HA editor (9753 characters, fingerprint verified).
- Files changed: components/, vgreen_evo_atom.yaml, tests/, CI, requirements, documentation and example build credentials
- Commands/results: All three host test suites PASS; ESPHome 2026.9.1 schema and full compile PASS; git diff --check PASS
- Validation: Reference OTA SHA256 fc82388d756ae6c9d76e1b468d5390859a7539bb467ceed0f4d7600e9eb6e745. No OTA/USB flash or live pump/HA control change.
- Next: Push and verify CI; build the HA-managed candidate with private secrets; supervise boot/button tests; then apply the HA hour picker/reset UI and retire obsolete SMUD controls.
- Blockers: Actual enclosure cold boot, pump response and experimental freeze behavior remain unverified. HA controls are specified but not deployed.

### 2026-10-12 12:25 - GitHub release and private HA image ready
- Goal: Prepare the exact device candidate for supervised installation and power-restoration testing.
- Progress: Published d2c66e1d13b694c52d5a09414d42381920304ea1 and prerelease v0.2.0-rc1. CI run 38294500074 passed both ESPHome versions and retained reference-only binaries/checksums. Saved the fingerprint-verified HA-managed candidate pinned to that commit, preserving device identity and all secret references.
- Decisions: Compile/download-only path used in ESPHome Builder. No upload, restart, power cycle or motor command was issued. The user's physical-readiness response is pending; do not infer approval from elapsed time.
- Files changed: HA-managed vgreen-pump.yaml source; private source backup/candidate under .cache/; AGENT_PROGRESS.md
- Commands/results: Live HA Validate reported Configuration is valid; full private compile reported Successfully compiled program, config hash 0xf474cd77, 997335-byte image, DRAM 50312 bytes. Source remains pinned to firmware commit d2c66e1 even if later documentation commits change branch HEAD.
- Validation: Device-page readback still reports installed 0.1.0 on ESPHome 2026.9.0, schedule running, connected, actual/target 1550 RPM. Candidate is built, not installed or physically verified.
- Next: Use the prepared private image for supervised installation; observe cold power with no Reset press, all front-button sequences, profile retention, then finish HA controls described in docs/HOME_ASSISTANT.md.
- Blockers: Awaiting readiness confirmation for the live pump test. Experimental freeze behavior remains unverified. HA dropdown/default-reset UI and removal of obsolete SMUD controls are still outstanding.

### 2026-10-12 13:15 - firmware live and phase correction verified
- Goal: Complete the approved implementation, including HA controls and runtime validation.
- Progress: Created persistent time helper at 23:00:00 and queued Pool Pump Timing script through the administrator UI. Installed the fingerprint-matched private candidate; new runtime diagnostics appeared, motor entered Hour 1/priming at about 3450 RPM without a Reset or Run command. Invoked the timing routine and confirmed Hour 15 with actual 1550 RPM on B1.
- Decisions: API credential located in the managed store is read-only; use it for allowed observations/backups and the existing administrator browser for configuration. An unsupported has_service Jinja expression was caught and replaced by availability of the actual new schedule-position sensor before the script was exercised. Do not infer cold-power success from an OTA software restart.
- Files changed: tools/ha_api.mjs, home_assistant/controls.json; live input_datetime.pool_pump_hour_one_start and script.pool_pump_timing; private dashboard backups in .cache/recovery/
- Commands/results: Local parse of 20 templates PASS; HA editor saved the script; native phase action discovered as esphome.vgreen_pump_set_schedule_phase; live action moved the reported phase to Hour 15 and actual speed to 1550 RPM.
- Validation: New diagnostics include schedule_position_seconds, controller_uptime, controller_reset_reason and last_controller_button_action under the existing backyard_vgreen_pool_pump prefix. Reset reason reports software via esp_restart. Actual GPIO/LED observations and true cold power remain pending.
- Next: Finish the dropdown flow currently open in the administrator browser, add reset/clock sync, retire old SMUD controls, publish source; then verify physical sequences and power restoration.
- Blockers: Physical triple-press observation was requested asynchronously; no response recorded yet. Freeze remains experimental.

### 2026-10-12 14:16 - native HA controls deployed and verified
- Goal: Finish the HA implementation while keeping autonomous startup and physical proof boundaries explicit.
- Progress: Created the actual-state 1–24 Template Select and reconnect/quarter-hour automation. Added hour selection, default reset and 11 PM note to Pool Pump and Home Operations. Disabled both obsolete pump SMUD automations; removed their controls, stale 10:45/10:50 text and B1-only phase guidance. Preserved energy, billing, unrelated diagnostics and private rollback exports.
- Decisions: The normal M5 boot contains no Reset/HA/Wi-Fi prerequisite; motor health gates remain. Inspected the installed ESPHome version's Wi-Fi/API setup code: neither blocks setup waiting for a connection. No physical cold-start repair claim is made from software evidence. Existing legacy helper entities/history are retained for rollback and no longer drive scheduling.
- Files changed: home_assistant/, tools/ha_api.mjs, tests/test_home_assistant.mjs, CI, README, docs/HOME_ASSISTANT.md, CHANGELOG, AGENT_PROGRESS.md
- Commands/results: Both server dashboard readbacks exactly equal reviewed payloads; all available dashboards have zero retired timing references. Host C++ suites and three HA structural/migration tests PASS; JS syntax and diff checks PASS. Device page confirms 0.2.0-rc1 / ESPHome 2026.9.1.
- Validation: Selecting Hour 15 updated the actual M5 hour and saved a new reference. Reset restored 23:00:00 and current Hour 16. B1, normal running mode and approximately 1550 RPM persisted. Reported phase matched the 11 PM reference within a second at its report time. Periodic sync has triggered. Saved a private screenshot of the finished controls.
- Next: Publish and verify GitHub checks, then collect user-supervised physical button, cold-power and offline results. The explicit power-test readiness question is awaiting an answer.
- Blockers: No actual front-button action has been observed; last action still none. Reset reason is still software via esp_restart, not a cold-power observation. Supervised hydraulic and experimental freeze behavior remain unverified.

### 2026-10-12 14:20 - GitHub verified; software handoff ready
- Goal: Finish publication and hand off only the remaining physical validation.
- Progress: Pushed a75c054f2d8653318302b920da8fb4c43651f7e6 to vgreen-controller-dev. Updated the existing v0.2.0-rc1 release notes to show installation and HA deployment while retaining prerelease status and the original firmware tag. The HA 2:15 PM periodic-sync trace shows Finished, with the timing script executed successfully.
- Decisions: No additional flash, Reset command, power cycle or unrelated infrastructure change. Preserve user-owned .DS_Store files. Keep the goal incomplete until the required physical observations exist.
- Files changed: AGENT_PROGRESS.md; public release notes updated, private configured firmware and recovery snapshots excluded.
- Commands/results: GitHub run 38309407282 PASS on ESPHome 2026.9.0 and 2026.9.1, including C++ tests, HA tests, full builds, checksums and reference artifacts. Remote branch SHA matched a75c054. Release readback confirms installed status and pending physical tests.
- Validation: Software and HA work is verified. Hardware readiness question has not been answered; physical-button diagnostic still records no action and observed reset reason remains a software restart. True cold-power behavior is not known.
- Next: User-supervised cold-power and front-button checks. Do not infer success from code, build, network availability or the prior warm start.
- Blockers: User presence and safe physical access to the operating pump are needed. This handoff does not claim 100 percent manual/hardware equivalence, cold-power repair or verified freeze protection.

### 2026-10-12 14:21 - hardware-readiness blocker revalidated
- Goal: Complete the remaining real power-restoration and M5 button checks without substituting software tests for physical evidence.
- Progress: The previous goal turn made substantive progress by deploying, publishing and verifying the HA controls and CI. This continuation made no implementation progress: a fresh readback confirms the same unresolved physical-test condition.
- Decisions: No Reset, flash, motor command or new automation was issued. The readiness request is unanswered. The same blocker has now been present for two consecutive goal turns, including the prior completion handoff; leave the goal active until the required blocked-audit threshold is reached or the user can test.
- Files changed: AGENT_PROGRESS.md only; this is a checkpoint, not additional implementation work.
- Commands/results: Read-only device snapshot at 21:21:33 UTC: schedule running, approximately 1550 RPM, software via esp_restart, last physical button action none, uptime approximately 4260 seconds. Worktree remains otherwise unchanged apart from pre-existing user .DS_Store files.
- Validation: No evidence of a cold power cycle or physical button action. All previously completed software checks remain completed; no background build or test process is still running.
- Next: User-supervised power and physical-button observations. If the same condition is confirmed in the next goal turn with no safe work available, mark the goal blocked instead of continuing repeated status-only turns.
- Blockers: User presence/readiness at the pump and direct observation of startup, buttons and safe water flow.

### 2026-10-12 14:22 - blocked audit satisfied
- Goal: Preserve the complete implementation scope, including real cold-power and physical-button verification.
- Progress: The previous goal turn was no progress, not a verified wait. This read-only continuation reconfirmed the same blocker; no live process is pending and no further safe software action supplies the missing hardware evidence.
- Decisions: The physical-readiness blocker has repeated for three consecutive goal turns. Mark the goal blocked, not complete. Stop automatic status-only checks until the user supplies readiness or observations. No pump control or restart was issued.
- Files changed: AGENT_PROGRESS.md only; publish this handoff without triggering duplicate builds.
- Commands/results: Fresh device readback at 21:22:13 UTC remains schedule running at approximately 1550 RPM, reset reason software via esp_restart, last physical button action none. User readiness response is still absent.
- Validation: Software checks and deployment are complete; true enclosure power-on, physical button sequences and supervised offline/flow behavior remain unproven.
- Next: When the user is at the pump and confirms it is ready to run, perform the documented supervised power-cycle and M5 front-button checks without pressing Reset to mask a startup failure.
- Blockers: Physical access and user observation. No additional source change or software reboot can verify this requirement remotely.
