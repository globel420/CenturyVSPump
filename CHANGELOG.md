# Changes

## 0.2.0-rc1 — candidate; physical validation pending

- Replace ATOM GPIO39 multi-click YAML with a local, debounced button state machine. Implement hold-on-release, mode-specific actions, local profile programming and the manual's LED timing.
- Restore the saved profile and automatically start Hour 1 after every boot, without a saved Pause, Wi-Fi or Home Assistant requirement.
- Treat unknown motor status as missing authorization rather than a permanent motor fault; retain motion blocking until healthy status arrives. Recover when the motor itself reports a cleared fault.
- Add reset reason, uptime, button input/action, and schedule-position diagnostics.
- Add an optional phase-alignment action that preserves Pause/Override and does not repeatedly restart priming.
- Add controller and button unit tests alongside all 16 schedule tables; build on ESPHome 2026.9.0 and 2026.9.1.
- Public build artifacts use example credentials only. Device-configured binaries remain private.
- Deploy native HA Schedule Hour 1–24 selection, persistent user-selected timing, 11 PM default reset, and reconnect/periodic alignment. Verify real phase changes without changing the selected profile or running speed.
- Replace obsolete forced-B1/SMUD controls on both dashboards while preserving energy monitoring and private rollback snapshots.

The candidate is installed and its automatic Hour 1 software-restart behavior is observed. The reported cold-power Reset requirement is not declared fixed until tested on the powered enclosure and motor. Physical button checks remain pending and freeze temperature/protection remains experimental.
