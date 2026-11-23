# Kubit four-motor bench profile (ESP-Drone)

Status, 18 November 2026 (Africa/Lagos): **flashed and run props-off (user-reported).** On 14 Nov it was flashed and booted with the motor supply off (E37). On 16 Nov all four motors ran from scripts, and the serial stop, setpoint watchdog and 5 s run cap were each observed once (E40–E43). No capture file is saved, and current, temperature, phone-app control and propeller load are untested. **Propellers stay OFF.** This profile is not a flight configuration.
(Earlier, 14 November: "No motor has run.")

## What it is

This is the first ESP-Drone image that drives Kubit's motors. Earlier motor
work used standalone bench images (`bench-tests/esp32-one-motor`,
`esp32-milestone-demo`). This profile runs the real ESP-Drone path: sensors →
estimator → controller → power distribution → motors, controlled from the
phone app or cfclient over the ESP32 SoftAP.

It uses the classic-ESP32 `TARGET_ESPLANE_V1` code path. Its MPU-6050 axis
handling is the same as in the HW-123 attitude bench. It does **not** apply
the HW-123 correction candidate; ESP-Drone's own gyro-bias and accelerometer
scale estimation is used instead.

## Pins

| Function | GPIO | Notes |
| --- | --- | --- |
| M1 front-right | 13 | Left header, USB end. CCW motor (white/black wires) |
| M2 rear-right | 32 | Left header, antenna end. CW motor (red/blue) |
| M3 rear-left | 19 | Right header, antenna end (just below SDA21). CCW motor |
| M4 front-left | 4 | Right header, USB end. CW motor |
| HW-123 SDA / SCL / INT | 21 / 22 / 23 | As currently soldered |
| Status LEDs | 2 | Onboard LED. ESPLANE's red-LED default was GPIO23 |
| Battery ADC | 35 | No divider fitted yet, so reported voltage is meaningless |

The layout follows the user's 13 Nov request: two motor pins per header, one
toward each end, so motor leads stay short and easy to solder. It assumes the
board sits face up with **USB forward** and the antenna at the rear (user choice, 14 Nov). `cad/kubit-frame` still has USB at the rear and needs flipping to match. If the board is mounted another way, the M1–M4 positions
change. The order is ESP-Drone's X layout. Check spin directions in T24.

Header positions are from the common 38-pin DevKitC pinout. Check them against
the silkscreen on the actual board before soldering.

- Right header, antenna end first: GND, 23, 22, TX, RX, 21, GND, **19**, 18,
  5, 17, 16, **4**, 0, 2, 15, …
- Left header, antenna end first: 3V3, EN, 36, 39, 34, 35, **32**, 33, 25, 26,
  27, 14, 12, GND, **13**, …

The motor pins avoid strapping pins (0, 2, 5, 12, 15), input-only pins
(34–39), the IMU pins (21/22/23) and GPIO14, which toggles during boot. GPIO25,
the earlier one-motor bench channel, is not used here, so that channel's
wiring moves to one of the four pins above. The motor bench option also turns
off ESP-Drone's VL53L1X and PMW3901 probes, because Kubit has neither deck.
That frees their default bus pins (12–17), including GPIO13. Each channel must copy the GPIO25 circuit: a 150 Ω
gate resistor, a 10 kΩ gate–source pull-down, and a flyback diode across the
motor. Each channel also needs its own reset off-state check (T21) before it
gets motor power. Confirm the motor positions and spin directions against the
frame before any restrained test.

## Bench safety layer (`CONFIG_KUBIT_MOTOR_BENCH`)

These limits are implemented in `components/drivers/general/motors/motors.c`,
the single point every motor command passes through.

- **Duty cap:** every motor command is clamped to `KUBIT_MOTOR_MAX_PERCENT`
  (default 30 %).
- **Run cap:** if any motor runs continuously for longer than
  `KUBIT_MOTOR_MAX_RUN_MS` (default 5000 ms), all motors latch off.
- **Serial stop:** sending `s` on the USB serial port (UART0, 115200 baud)
  latches all motors off. If the UART stop cannot be installed, motors latch
  off at boot.
- **No motor beeps:** propeller-test tones and startup tones are disabled.
- **Clearing a latched stop:** only a reset clears it.
- **Inherited ESP-Drone behaviour:** with no setpoint for 2 s, ESP-Drone
  commands zero thrust. This does **not** apply when `motorPowerSet.enable=1`,
  which bypasses the setpoint path; only the 5 s run cap and the serial stop bound
  those runs. The startup motor test is disabled.

Hardware status (user-reported, props off): serial `s` latches off on this image; the setpoint watchdog zeroed the motors about 2.06 s after the last setpoint (E41); the 5 s run cap latched off a 7 s run (E42, hand-timed 5.5-6.5 s). Props stay off. Before relying on any of them, check
each one with the motor supply current-limited and the props off.

## Phone bench image (22 Nov 2026): serial console and phone control

Status: built, flashed and run by the user on 22 Nov (user-reported: serial commands, Wi-Fi/phone control and motor control worked; E45). No capture is saved; propeller state, current, temperature and any flight result are unrecorded. **Props stay OFF.** Not a flight configuration.

**Why the phone could not drive motors before:** the Kubit image boots disarmed (`system.c`), ESP-Drone only calls power distribution when armed, and the phone apps never send an arm command. Phone thrust was dropped in `stabilizer.c`.

`CONFIG_KUBIT_PHONE_BENCH` (depends on the motor bench) adds:

- A UART0 console, 115200 baud, line-based:

| Command | Effect |
| --- | --- |
| `a` | Arm; also clears a run-cap latch (never a serial-stop latch) |
| `d` | Disarm, motors 0, duty cap back to default |
| `m <1-4> <0-65535>` | Set one motor (still clamped by the cap) |
| `all <0-65535>` | Set all four motors |
| `c <percent>` | Set the duty cap, 5 up to `KUBIT_MOTOR_CAP_CEILING_PERCENT` (default 50) |
| `?` | Print state |
| `s` | Latch all motors off at once (reset to clear); works mid-line |

- Phone thrust sent equally to all four motors once armed (no roll/pitch/yaw mixing).
- Unchanged: 30 % default cap (resets on boot and on `d`), 5 s run cap, ~2 s link-loss stop, no motor beeps. Re-arming with `a` never restarts old serial values.

Build (new directory; never run `set-target` on an existing one):

```bash
esp_idf55
cd /home/rbj/Documents/Projects/microDrone/esp-drone
idf.py -B build-kubit-phone-bench -D SDKCONFIG=sdkconfig.kubit-phone-bench \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32;sdkconfig.defaults.kubit-motor-bench;sdkconfig.defaults.kubit-phone-bench' \
  build
```

Checks before commit: motor-bench profile rebuilt and `tests/test_hw123_pipeline.py` passed (user-run, 23 Nov).

## Sound self-test fix (14 Nov)

With `BUZZER_ON=n` the startup sound self-test failed, so `systemStart()` never ran and the
first cfclient connection caused a reboot loop (port-4 queue full assert, E37). `system.c`
now runs `soundTest()` only when `CONFIG_BUZZER_ON` is set (`system.c` around lines 251–254;
commit `2c4fed2`). The image dated 14 Nov 13:44 includes this fix. The user then flashed it and
reports cfclient connects and shows values; this was not captured.

## Build

```bash
esp_idf55
cd /home/rbj/Documents/Projects/microDrone/esp-drone
idf.py -B build-kubit-motor-bench -D SDKCONFIG=sdkconfig.kubit-motor-bench \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32;sdkconfig.defaults.kubit-motor-bench' \
  build
```

Defaults seed a fresh configuration only. Verify `CONFIG_TARGET_ESPLANE_V1`,
`CONFIG_MOTOR0x_PIN`, `CONFIG_KUBIT_MOTOR_*` and the 4 MB flash size in
`sdkconfig.kubit-motor-bench`.

13 Nov result (pins revised 14 Nov to 13/32/19/4, USB forward): build passed, with
17 % of the app partition free. A build from a fresh `--recurse-submodules` clone at commit
`2c4fed2` also passed on 17 Nov (see `GIT_HISTORY_RECOVERY.md` in the repository root). The existing
`build/` and `build-hw123-candidate/` profiles also rebuilt, and
`tests/test_hw123_pipeline.py` passed. That is a software result only.


## Four-channel wiring (as intended, not inspected)

Source: the one-channel circuit in
[ONE_MOTOR_TEST_WIRING.html](../../docs/hardware/ONE_MOTOR_TEST_WIRING.html) and
[MOTOR_CHANNEL_BRINGUP](../../docs/hardware/MOTOR_CHANNEL_BRINGUP_2026-11-07.md), repeated per
channel. **This is the intended design, not a record of what is soldered.** No photo,
continuity or resistance inspection of the four channels is recorded after the 14 Nov
repair (E38, E39); there is no schematic yet (tracker T15).

Per channel (M1 GPIO13, M2 GPIO32, M3 GPIO19, M4 GPIO4), AO3400 N-channel MOSFET:

| Net | Intended connection | Confirmed? |
| --- | --- | --- |
| Gate drive | ESP32 GPIO → 150 Ω → AO3400 gate | **No.** 0 Ω gate-to-GPIO was read on all four channels on 14 Nov (E38); no later reading recorded |
| Gate pull-down | 10 kΩ gate → source (ground) | Reported for the GPIO25 channel only (E32) |
| Load | Motor between +BAT and AO3400 drain | Motors respond (E39–E43, user-reported) |
| Flyback diode | Across the motor, cathode to +BAT, anode to drain | **Partly.** M3 diode was reversed and corrected (E38/E39); M1, M2, M4 orientation not recorded |
| Source | AO3400 source to the common ground | Not recorded |
| Supply decoupling | 100 nF and 330 µF/10 V across the motor supply (per the one-channel sheet) | Not recorded for four channels |

Shared nets:

- Battery: bare 1S Li-ion cell on the bench (E38). The BETAFPV 550 mAh battery is selected but not used here. Fuse (about 1–1.5 A) and switch in the positive lead: **required by `bench-tests/esp32-four-motor/README.md`, not confirmed fitted.** The only record is the `--fuse-fitted` flag the user passed to the link-timeout script (E41).
- ESP32 power: USB only on the bench (3V3 powers the HW-123); motor supply separate. A common ground between the motor supply and the ESP32 is intended (the AO3400 sources and gate pull-downs need it) but not recorded as inspected.
- HW-123: 3V3, GND, SDA GPIO21, SCL GPIO22, INT GPIO23 (E16, E23).
- Battery-voltage ADC GPIO35: no divider fitted.

## Suggested first hardware session (ask before flashing)

Progress on 16 Nov (props off, user-reported): step 2 (flash) was done by the user (E37) and step 3 was partly done (cfclient connected, user-reported; the phone app was not tested). Step 1's diode and current-limited supply are not confirmed: the fuse is unrecorded and the M3 diode was reversed (E38). The three stops in step 4 were each observed once on the ESP-Drone image (E40–E42). The phone-app link (T32/T33) and the 2 s link timeout via a closed phone app are untested. Steps 4 and 5 below remain the reference method; the reset off-state (T21) is still open for all four pins.

1. Fit the flyback diode and use a current-limited supply. Keep the props off
   and the motor supply **off while flashing**.
2. Flash this image over `/dev/ttyUSB0`. It replaces whatever image is on the
   board, currently a standalone bench image.
3. With the motor supply still off, test the link: connect a phone to the
   ESP-Drone SoftAP, open the app and check telemetry (T32/T33). Or use
   cfclient instead (T34).
4. Move the proven bench channel from GPIO25 to one of the four motor pins.
   With only that channel wired and the supply on, apply small thrust from the
   app. Check that each of the three stops works: serial `s`, the 5 s run cap,
   and closing the app (2 s link timeout).
5. Add the other three channels one at a time, repeating the reset off-state
   check for each.
