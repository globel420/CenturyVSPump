# Changes

## Dashboard card 1.0.1 — live 2026-10-12

- Enable the approved local module and replace only the pump overview; verify the saved server configuration and unchanged Advanced view.
- Correct the internal sparkline's SVG sizing so it stays below the RPM reading, then update the resource cache version.
- Verify live phone and desktop layouts, real history hydration, both original chart images, current-position movement and next-change timing. Open and cancel Pause, Override, hour and profile confirmations; six control/timing states and their change timestamps remain identical.
- No firmware flash, HA restart, helper/automation change or motor command was part of this UI rollout. Preserve a private pre-rollout dashboard export for exact UI rollback.

## Dashboard card 1.0.0 — implementation prepared 2026-10-12

- Add a local, phone-first HA pump card matching the approved light/teal design, with actual-RPM dial and internal history line, next scheduled speed change, moving schedule marker, existing manual controls, all 16 profiles and the original schedule images.
- Use real HA state/history. Exclude communication gaps from observed run time, label incomplete energy estimates and disable remote actions on stale, disconnected or unknown/faulted controller state.
- Require confirmation for pump/timing/profile actions. Keep existing native entities and timing scripts; loading the card sends no pump command.
- Add an overview-only migration, rollback instructions and ten passing HA/card test cases, including 180 schedule-boundary checks. Activation and live rendered validation are tracked separately from this implementation checkpoint.

## 0.2.0-rc1 — installed; core field test accepted 2026-10-12

- Replace ATOM GPIO39 multi-click YAML with a local, debounced button state machine. Implement hold-on-release, mode-specific actions, local profile programming and the manual's LED timing.
- Restore the saved profile and automatically start Hour 1 after every boot, without a saved Pause, Wi-Fi or Home Assistant requirement.
- Treat unknown motor status as missing authorization rather than a permanent motor fault; retain motion blocking until healthy status arrives. Recover when the motor itself reports a cleared fault.
- Add reset reason, uptime, button input/action, and schedule-position diagnostics.
- Add an optional phase-alignment action that preserves Pause/Override and does not repeatedly restart priming.
- Add controller and button unit tests alongside all 16 schedule tables; build on ESPHome 2026.9.0 and 2026.9.1.
- Public build artifacts use example credentials only. Device-configured binaries remain private.
- Deploy native HA Schedule Hour 1–24 selection, persistent user-selected timing, 11 PM default reset, and reconnect/periodic alignment. Verify real phase changes without changing the selected profile or running speed.
- Replace obsolete forced-B1/SMUD controls on both dashboards while preserving energy monitoring and private rollback snapshots.

The user accepted the installation after a real shared-supply power cycle started the pump, fresh diagnostics reported a power-on event, and HA corrected Hour 1 to Hour 18. Physical Override/resume and Pause/resume, the three-minute prime, and the scheduled 5 PM B1 stop were also observed. This records one successful cold-start trial, not universal power-supply reliability. Triple-press report, physical programming/persistence and isolated offline operation remain outside this session's field-verified coverage; freeze remains experimental. No additional flash or version change was made after acceptance.
