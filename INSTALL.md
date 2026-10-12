# Install on the other computer

This is the handoff path for compiling and flashing the M5Stack ATOM Lite. Stop before the physical wiring section unless the exact VGreen EVO label, connector, and switch-bank photos have been checked.

## 1. Get this development branch

```bash
git clone --branch vgreen-controller-dev --single-branch https://github.com/globel420/CenturyVSPump.git
cd CenturyVSPump
```

If the repository already exists:

```bash
git fetch origin
git switch vgreen-controller-dev
git pull --ff-only
```

## 2. Add private settings

```bash
cp secrets.example.yaml secrets.yaml
```

Edit `secrets.yaml` with that location's Wi-Fi name/password and unique API, OTA, fallback-hotspot, and web credentials. Never commit `secrets.yaml`.

Generate the Home Assistant API key with:

```bash
openssl rand -base64 32
```

## 3. Install the pinned build tools

The simplest path on macOS or Linux uses `uv` and Python 3.13:

```bash
uv venv --python 3.13 .venv
uv pip install --python .venv/bin/python -r requirements-dev.txt
```

Validate and compile before connecting hardware:

```bash
.venv/bin/esphome config vgreen_evo_atom.yaml
.venv/bin/esphome compile vgreen_evo_atom.yaml
```

## 4. Flash the ATOM over USB

Keep the ATOM disconnected from the pump's RS485 connector and 12-volt supply while flashing over USB.

```bash
.venv/bin/esphome run vgreen_evo_atom.yaml
```

Select the USB serial device when prompted. The firmware synchronizes with the motor and automatically starts the saved profile at Hour 1; there is no persistent startup Pause. Compiling or flashing does not prove physical compatibility. Never install the public CI example image on the real controller; build the HA-managed device YAML using its existing private settings.

## 5. Physical verification gate

Before opening or wiring the pump:

1. Photograph the full motor label so `EVC165` and voltage are readable.
2. Photograph the complete switch bank with its printed ON direction visible.
3. Photograph the RS485 connector and every printed terminal label.
4. Turn off and lock out the breaker, then wait the full time required by the EVO guide before opening or moving anything.
5. Verify A/B polarity, ground, and supply using terminal labels and the correct manuals. Do not rely on wire colors.

The EVO guide uses both switches 3 and 4 ON to select Schedule Set D. That change disables native schedules, priming, freeze protection, and normal motor-button behavior, so do not make it until the photos and exact installation are reviewed.

## 6. First live test

- Keep the pump filled and hydraulically primed.
- Expect automatic start after communication is established. Be present with the hydraulic system ready.
- Observe boot, serial communication, priming where applicable, and actual RPM. Do not press Reset to make a failed cold start appear successful.
- Verify the experimental ambient-temperature value against a trusted thermometer before enabling local freeze behavior.
- Test one short, supervised run and confirm water flow, commanded RPM, actual RPM, stop behavior, and communication-fault handling.
- Test with Home Assistant and the router turned off for at least 30 minutes.

Do not leave the controller unattended until all of those checks pass. A software STOP is not an electrical lockout.

Test the M5 front-button sequences with Home Assistant unavailable. Verify that a button-selected profile is restored after power is reapplied and that the pump starts at Hour 1 without Reset. Observe the manual's minimum 60-second power-off interval for power-cycle tests. If the M5 has no LED or network presence before Reset, investigate its power/boot path; software unit tests do not establish that the enclosure power supply boots it reliably.
