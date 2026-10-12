# VGreen EVO stock behavior reference

The controller uses a local 24-hour clock that begins at Hour 1 whenever the ATOM starts. It does not require Home Assistant, Internet time, or an RTC. An optional HA action can align its phase after reconnection. Without that connection, it continues locally. Displayed Hour 1 is elapsed 0–1 h and Hour 24 is elapsed 23–24 h; the tables below use elapsed-hour boundaries.

## Schedule Set A

| Profile | 24-hour stages | Prime |
| --- | --- | --- |
| A1 | 0–2: 3450; 2–4: 2750; 4–10: 1750; 10–12: 1150; 12–24: stopped | Yes |
| A2 | 0–2: 3450; 2–4: 2850; 4–10: 1850; 10–12: 1250; 12–24: stopped | Yes |
| A3 | 0–4: 3450; 4–8: 1750; 8–12: 1150; 12–24: stopped | Yes |
| A4 | 0–2: 3250; 2–10: 1150; 10–12: 3250; 12–24: stopped | Yes |
| A5 | 0–24: 1725 | Yes |
| A6 | 0–24: 1100 | No |
| A7 | 0–24: 1725 | No |
| A8 | 0–24: 3450 | No |

## Schedule Set B

| Profile | 24-hour stages | Prime |
| --- | --- | --- |
| B1 | 0–2: 3450; 2–6: 2850; 6–10: 2250; 10–18: 1550; 18–24: stopped | Yes |
| B2 | 0–2: 3350; 2–6: 2750; 6–10: 2150; 10–18: 1450; 18–24: stopped | Yes |
| B3 | 0–2: 3250; 2–6: 2650; 6–10: 1950; 10–18: 1350; 18–24: stopped | Yes |
| B4 | 0–4: 3150; 4–8: 2550; 8–12: 1850; 12–18: 1250; 18–24: stopped | Yes |
| B5 | 0–4: 3050; 4–8: 2450; 8–12: 1750; 12–18: 1150; 18–24: stopped | Yes |
| B6 | 0–4: 2950; 4–8: 2350; 8–12: 1650; 12–18: 1050; 18–24: stopped | Yes |
| B7 | 0–2: 3450; 2–6: 2850; 6–10: 2250; 10–24: 1550 | Yes |
| B8 | 0–4: 2950; 4–10: 2350; 10–18: 1650; 18–24: 1050 | Yes |

## Other emulated behavior

- Priming: 3450 RPM for three minutes for profiles marked Yes.
- From Operating Mode: one press requests a 3450 RPM override for up to 24 hours; two presses pause while the schedule advances; three presses report the profile with LED blinks.
- Hold at least three seconds and release: leave Pause/Override, or enter Schedule Set Mode from Operating Mode.
- Schedule Set Mode stops the motor. Count 1–8 presses, hold three seconds and release to save the profile locally. Inactivity for 45 seconds abandons the selection. The existing cycle position is preserved when choosing a profile.
- LED: normal steady on; Override 2500 ms on/125 ms off; Pause 125 ms on/2500 ms off; Schedule Set 250 ms on/off; Schedule Check three seconds on, profile-count half-second flashes, three seconds off, then normal operation.
- Freeze request: below 39°F, request 1725 RPM for one hour, then recheck hourly. If a previously valid cold sensor becomes stale during an active window, continue in one-hour windows until a fresh reading is above 39°F or protection is explicitly suspended.
- Power recovery: clear temporary modes and automatically restart the saved profile at Hour 1 after the motor is ready. No HA connection or side-Reset press is required.

Pause has priority over freeze logic during the current boot. It stops motor commands but does not freeze the schedule clock. A power cycle clears Pause; use the electrical disconnect/lockout for maintenance. The experimental freeze policy has not been established as equivalent to the native motor's protection and remains opt-in.

## Source and uncertainty

Source: [VGreen EVO installation and user guide](https://electricmotorwarehouse.com/content/PDF/EVO%20Install%20Manual.pdf), button operation pages 7–9 and 14, schedules 10–13, power recovery 24.

The Gen3 protocol describes an estimated ambient-temperature sensor at page `0x00`, address `0x07`, scale 128. The controller treats it as an experimental Fahrenheit input with plausibility and freshness checks. Its units and behavior on the exact EVC165 must be verified against a trusted thermometer before relying on local freeze protection.
