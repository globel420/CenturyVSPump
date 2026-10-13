# Optional Home Assistant schedule controls

Status: firmware **0.2.0-rc1 (ESPHome 2026.9.1)** is installed. Native hour selection, timing reset and synchronization were deployed and live-checked on 2026-10-12. The user accepted the installation after the supervised cold-power, basic-button, priming and scheduled-stop tests below. Local controller behavior remains independent of HA by design; unexercised physical paths are listed separately rather than claimed verified.

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

- Device metadata reports 0.2.0-rc1 / ESPHome 2026.9.1. After the initial software-restart observation, the user performed a supervised power cycle through the normal shared pump/M5 supply, with a timed off interval of at least 60 seconds. The user restored power under instructions not to press Reset and confirmed the pump started. Fresh diagnostics reported `power-on event`, restarted uptime, healthy communication and approximately 3450 RPM priming.
- Recorded history shows Hour 1 at 4:48:32 PM Pacific and HA-corrected Hour 18 at 4:48:33 PM, with B1 preserved. This is one successful real power-restoration trial, not a claim about all supply conditions.
- Selecting Hour 15 on the pump page changed the real device hour to 15 and saved a new daily reference. The selected B1 profile and approximately 1550 RPM were preserved.
- Reset to Default Timing restored `23:00:00` and the correct current Hour 16, rather than jumping to Hour 1. Reported schedule-position seconds matched the reference within the diagnostic reporting interval.
- Background synchronization is enabled; the 2:15 PM periodic-sync trace finished successfully, and M5 reconnect synchronization was exercised in the later power-cycle test. An HA-server restart was not part of the session.
- Pool Pump and Home Operations readbacks exactly matched their reviewed changes. All available dashboard configs were checked for the retired controls and old 10:45/10:50 scheduling guidance; none remained. The two obsolete SMUD pump automations are disabled, not deleted. Energy/billing content and rollback exports were preserved.
- Physical input events corroborate single-press Override, hold/release out of Override, double-press Pause (actual/target zero RPM) and hold/release from Pause (3450 RPM prime). The user observed slowdown, stop and restart. No remote motor command substituted for a button action.
- The resumed prime lasted from 4:56:42 to 4:59:42 PM Pacific, then actual speed settled to 1550 RPM. At 5 PM, B1 entered Hour 19 and stopped with Pause off. The user confirmed that stop and explicitly concluded the test as working.
- Coverage not claimed: the attempted triple-press report recorded a single-press Override, so a blink alone was not counted as a passed Schedule Check. Physical programming/persistence, isolated offline operation, an HA-server restart and experimental freeze behavior were not exercised to completion. Testing ended at the user's request.

## Local preparation tools

### Phone-first pump dashboard card

Deployment status: **card 1.0.1 is live on the pump overview**, verified on 2026-10-12 after the user's explicit module-activation approval. The server configuration exactly matches the reviewed candidate, and the Advanced view matches the pre-rollout export. Phone (402px) and desktop (1440px) renders, live/history data, full-size chart assets and cancel-only action confirmations were checked. No physical motor action was issued during these UI checks.

[`home_assistant/vgreen-pump-card.js`](../home_assistant/vgreen-pump-card.js) implements the approved light/teal layout as a local custom card, without a framework, third-party JavaScript, embedded credentials or a cloud runtime. It uses HA's documented [custom-card interface](https://developers.home-assistant.io/docs/frontend/custom-ui/custom-card/) and [frontend data methods](https://developers.home-assistant.io/docs/frontend/data/).

- The dial displays the actual motor RPM, with an actual-RPM history line inside it. The full graph covers the last 24 hours and distinguishes actual from commanded RPM. Disconnected intervals remain gaps; observed run time is not claimed to cover missing data.
- The next scheduled speed-change time and destination RPM/Off come from the real controller phase and selected profile. The schedule bar retains its current-position marker. All 16 profile tables are tested against `stock_schedule.h`.
- Pause, Override and Resume call the existing native switch/button entities only after confirmation. Hour selection and default reset retain the deployed native timing workflow. The combined profile selector updates the existing family/profile entities sequentially; a partial failure is reported, not hidden.
- Missing, stale, disconnected or faulted controller data disables remote actions. A never-pressed button's `unknown` state is valid; it is not confused with an unavailable button.
- Power and billing energy remain explicitly labeled RPM-based estimates, including the existing history-coverage warning. The original full-size A/B charts and a local-button reference are retained. Loading or refreshing the card sends no motor command.

Install/update through the supported HA editor and [dashboard resource UI](https://developers.home-assistant.io/docs/frontend/custom-ui/registering-resources/):

1. Save a fresh dashboard export privately. Do not replace it with an older design snapshot.
2. Place the reviewed card at `www/vgreen-pump-card.js` and register `/local/vgreen-pump-card.js?v=1.0.1` as a JavaScript module. Only code belongs in this publicly served location; never put credentials or private telemetry exports there.
3. Prepare an overview-only candidate with `node home_assistant/pump_dashboard.mjs BEFORE.json CANDIDATE.json`. This preserves the Advanced view and does not change helpers, automations or firmware.
4. Review and save the candidate in HA's raw dashboard editor, then reload the page. Re-read the server configuration and compare it with the reviewed candidate; check the actual rendered card and live telemetry separately.
5. Validate phone and desktop layouts. Open and cancel confirmation dialogs if testing the UI without pump supervision; do not confirm a motor action just to test a button.

Rollback this UI change by restoring the saved dashboard export through the same editor. Leave firmware, entities and timing helpers alone. The new resource can remain unused, or be removed separately after confirming no dashboard references it. No HA restart or firmware flash is required when `www` already exists.

The entity mapping defaults to this controller's existing entity IDs. Another installation may override individual keys in the card's `entities` configuration. Never use a numeric fallback of zero for an unavailable RPM sensor.

`tools/ha_api.mjs` is a small Node.js 22+ REST/WebSocket client. It reads a token from a supplied private environment file and requires `--apply` for mutations. Access is still limited by that token's HA account; the deployed instance's read-only account was used only for readback, while native administrator editors applied changes. Do not increase an account's permissions to bypass a denied operation.

`home_assistant/dashboard_update.mjs` prepares a narrow dashboard update from a fresh JSON export without contacting HA. It removes only the retired pump scheduling controls and guidance, adds the new timing cards and preserves unrelated content. Review its output, apply it through HA's supported editor/API, and compare a new server readback before considering the change complete. Output files are created privately and never overwrite an existing backup.
