# Optional Home Assistant schedule controls

Status: firmware **0.2.0-rc1 (ESPHome 2026.9.1)** is installed. The native hour selector, timing script, reconnect automation and both dashboard copies were deployed and live-checked on 2026-10-12. The M5's button, profile saving, schedules and boot recovery are implemented locally and never call HA. Physical button and cold-power verification are still pending.

The reusable native configuration is in [`home_assistant/controls.json`](../home_assistant/controls.json). It is a reference for HA's helper, script and automation editors, not a package to paste into `configuration.yaml`. The default helper value is a one-time initialization, not a startup reset. Instance-specific full dashboard exports and configured firmware remain private and ignored by Git.

## Interface

- `set_schedule_phase(seconds)` accepts 0–86399, where Hour H begins at `(H - 1) * 3600`.
- The installed node name `vgreen-pump` normally exposes `esphome.vgreen_pump_set_schedule_phase`. Discover the actual action after connection; do not assume a candidate is installed because its source compiled.
- Keep existing `Schedule Hour`, selected-set/profile, Pause, Override and motor-state entities. New diagnostics provide position seconds, input/action, uptime and reset reason.
- Changing phase preserves Pause, Override, profile and motor fault checks. Scheduled off cancels a running prime; other phase corrections preserve its deadline.

## HA data and UI contract

Create a native persistent time-only `input_datetime.pool_pump_hour_one_start` helper, initialized **once** to `23:00:00`. Do not reset an existing helper on HA startup. Its stored value is the active daily timing reference.

Create a native Template Select helper named **Schedule Hour** with options `1` through `24`. Its state comes from the existing actual schedule-hour sensor; its select action calls a single queued synchronization script. Updating the sensor must never invoke the select action. This uses HA's native more-info dropdown and requires no new dashboard plugin.

When H is selected, calculate `new_reference = (local_seconds_now - (H - 1) * 3600) % 86400`, store that reference, and align the device immediately. It continues through the rest of the cycle; subsequent reconnects retain the chosen timing. A **Reset to Default Timing** button stores `23:00:00` and aligns to the current position under that reference, without changing the profile.

Show these separately on the pump page:

- Selected schedule and actual current hour, together with the existing schedule charts.
- `Default timing: Hour 1 is 11:00 PM–midnight (Pacific).`
- The active Hour 1 start time, which changes after a manual hour adjustment.
- Pause/Override mode so the scheduled speed is not confused with the motor's current command.

For B1 on default timing: 11 PM–1 AM 3450 RPM, 1–5 AM 2850, 5–9 AM 2250, 9 AM–5 PM 1550, 5–11 PM off. All profiles use the same 1-based hour mapping; selection never forces B1.

## Synchronization

Use one queued script for manual selection, default reset and background alignment. Compute time when the queued action executes. Validate the reference and current device availability; the UI shows actual device state rather than optimistic success.

Trigger background alignment on HA startup, device availability returning, and every 15 minutes. Wait up to two minutes for both the actual hour and new schedule-position diagnostic to become available; then call the discovered native phase action. If the device or action is unavailable, the automation fails without changing motor modes. Calculate phase as `(local_seconds_now - reference_seconds) % 86400`. HA uses `America/Los_Angeles`, including daylight-saving changes. Without HA, the M5 continues its elapsed-time cycle; after a complete reboot it begins Hour 1 until a valid alignment arrives.

Time alignment must never call Resume, clear Pause, clear a motor fault, select B1, or repeatedly press Restart Schedule. A physical button profile change remains the user's selection when HA returns.

## Migration and rollback

1. Export the live ESPHome YAML, Pool Pump dashboard, relevant helper definitions and SMUD automations. Keep private backups outside Git.
2. Preserve the device hostname, credentials, board, UART wiring and existing entity identities. Apply the reviewed reference changes to the HA-managed `vgreen-pump.yaml`, pointing external components at the tested immutable commit.
3. Build using the existing private HA secrets; install only when the pump is ready for supervised automatic startup.
4. Confirm the new firmware version and action are live, then create the native helper, Template Select, script, reset button and reconnection automation through HA's configuration APIs/UI.
5. Remove the SMUD-OPTIMIZED switch from the page and disable the obsolete forced-B1/nightly-reset and fixed-time peak-guard automations. Preserve energy/billing helpers and unrelated controls. Archive their exports for rollback; do not delete their history.
6. Verify actual phase and RPM before considering each operation successful. Check selection under another profile, Pause, Override, disconnect/reconnect and power restoration.

Rollback the firmware and HA timing controls together. A compiled reference image with public example credentials is not a device rollback image.

## Verified live results

- Device metadata reports 0.2.0-rc1 / ESPHome 2026.9.1. After installation, the M5 automatically started Hour 1 and priming, without a Reset or Run command. Reset reason was `software via esp_restart`: this is not evidence of a successful cold power cycle.
- Selecting Hour 15 on the pump page changed the real device hour to 15 and saved a new daily reference. The selected B1 profile and approximately 1550 RPM were preserved.
- Reset to Default Timing restored `23:00:00` and the correct current Hour 16, rather than jumping to Hour 1. Reported schedule-position seconds matched the reference within the diagnostic reporting interval.
- Background synchronization is enabled; the 2:15 PM periodic-sync trace finished successfully with the timing script executed. Reconnect and HA-restart triggers are configured but have not yet been exercised by a controlled outage.
- Pool Pump and Home Operations readbacks exactly matched their reviewed changes. All available dashboard configs were checked for the retired controls and old 10:45/10:50 scheduling guidance; none remained. The two obsolete SMUD pump automations are disabled, not deleted. Energy/billing content and rollback exports were preserved.
- Physical one/two/three-press, hold/programming, offline operation and enclosure cold-power tests still require observation at the pump. Host tests are not substitutes for these checks.

## Local preparation tools

`tools/ha_api.mjs` is a small Node.js 22+ REST/WebSocket client. It reads a token from a supplied private environment file and requires `--apply` for mutations. Access is still limited by that token's HA account; the deployed instance's read-only account was used only for readback, while native administrator editors applied changes. Do not increase an account's permissions to bypass a denied operation.

`home_assistant/dashboard_update.mjs` prepares a narrow dashboard update from a fresh JSON export without contacting HA. It removes only the retired pump scheduling controls and guidance, adds the new timing cards and preserves unrelated content. Review its output, apply it through HA's supported editor/API, and compare a new server readback before considering the change complete. Output files are created privately and never overwrite an existing backup.
