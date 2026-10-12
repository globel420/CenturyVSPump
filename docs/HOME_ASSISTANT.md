# Optional Home Assistant schedule controls

Status: firmware action implemented and compiled; live HA control migration and physical deployment are pending. The M5's front button, profile saving, schedules and boot recovery work locally and never call HA.

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

Trigger background alignment on HA startup, device availability returning, and every 15 minutes. If the action has not been registered yet, wait up to two minutes for it before failing visibly. Calculate phase as `(local_seconds_now - reference_seconds) % 86400`. HA uses `America/Los_Angeles`, including daylight-saving changes. Without HA, the M5 continues its elapsed-time cycle; after a complete reboot it begins Hour 1 until a valid alignment arrives.

Time alignment must never call Resume, clear Pause, clear a motor fault, select B1, or repeatedly press Restart Schedule. A physical button profile change remains the user's selection when HA returns.

## Migration and rollback

1. Export the live ESPHome YAML, Pool Pump dashboard, relevant helper definitions and SMUD automations. Keep private backups outside Git.
2. Preserve the device hostname, credentials, board, UART wiring and existing entity identities. Apply the reviewed reference changes to the HA-managed `vgreen-pump.yaml`, pointing external components at the tested immutable commit.
3. Build using the existing private HA secrets; install only when the pump is ready for supervised automatic startup.
4. Confirm the new firmware version and action are live, then create the native helper, Template Select, script, reset button and reconnection automation through HA's configuration APIs/UI.
5. Remove the SMUD-OPTIMIZED switch from the page and disable the obsolete forced-B1/nightly-reset and fixed-time peak-guard automations. Preserve energy/billing helpers and unrelated controls. Archive their exports for rollback; do not delete their history.
6. Verify actual phase and RPM before considering each operation successful. Check selection under another profile, Pause, Override, disconnect/reconnect and power restoration.

Rollback the firmware and HA timing controls together. A compiled reference image with public example credentials is not a device rollback image.
