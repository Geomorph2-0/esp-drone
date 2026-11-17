# Kubit four-motor bench profile (ESP-Drone)

Status, 14 November 2026 (Africa/Lagos): **flashed and booted (user-reported, motor supply off).** No motor has run. **Propellers stay OFF.** This profile is
not a flight configuration.

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
17 % of the app partition free. The existing
`build/` and `build-hw123-candidate/` profiles also rebuilt, and
`tests/test_hw123_pipeline.py` passed. That is a software result only.

## Suggested first hardware session (ask before flashing)

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
