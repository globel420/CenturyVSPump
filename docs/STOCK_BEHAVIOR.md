# VGreen EVO stock behavior reference

The offline controller uses a 24-hour clock that begins at hour one whenever the ATOM starts. This intentionally does not use Home Assistant, Internet time, or an RTC. The EVO guide says the motor itself returns to hour one after a power interruption.

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
- Single press: 3450 RPM override for up to 24 hours.
- Double press: stop indefinitely while the 24-hour schedule continues advancing.
- Long hold: leave pause/override and return to the current schedule position.
- Triple press: report the selected profile with LED blinks.
- Freeze request: below 39°F, request 1725 RPM for one hour, then recheck hourly. If a previously valid cold sensor becomes stale during an active window, continue in one-hour windows until a fresh reading is above 39°F or protection is explicitly suspended.
- Power recovery: restart the selected profile at hour one.

Maintenance pause has higher priority than freeze logic so the motor cannot unexpectedly start while someone is servicing it. That also means freeze protection is deliberately suspended during maintenance pause. Pause stops motor commands but does not freeze or reset the current 24-hour schedule position.

## Source and uncertainty

Source: [VGreen EVO installation and user guide](https://electricmotorwarehouse.com/content/PDF/EVO%20Install%20Manual.pdf), schedule and operation pages 10–13.

The Gen3 protocol describes an estimated ambient-temperature sensor at page `0x00`, address `0x07`, scale 128. The controller treats it as an experimental Fahrenheit input with plausibility and freshness checks. Its units and behavior on the exact EVC165 must be verified against a trusted thermometer before relying on local freeze protection.
