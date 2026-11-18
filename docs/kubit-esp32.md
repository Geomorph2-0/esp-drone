# Kubit classic ESP32 setup

The selected board is an **ESP32-D0WD-V3, 38-pin WROOM-32 DevKit**, using
ESP-IDF **v5.5** and the `esp32` target. The available board reports chip revision
v3.1 and 4 MB flash. The separate [ESP32/MPU6050 bench check](../../docs/hardware/ESP32_MPU6050_BRINGUP.md)
has booted and recorded HW-123 sensor readings (2 November 2026).
The opt-in [ESP-Drone attitude bench](kubit-imu-bench.md) has also run on hardware,
historically using INT19 with motor/controller initialization disabled. The
7 November soldered HW-123 candidate now selects INT23 for soldering convenience.
Current next task: [candidate attitude/range bench](kubit-hw123-integration.md)
for user-assisted tilt/return: INT23 and static slow/fast behavior are now verified
([hardware report](../../docs/hardware/HW123_INT23_ATTITUDE_2026-11-07.md)); [standalone recheck results](../../docs/hardware/HW123_VALIDATION_2026-11-07.md) are saved.
See the [master tracker](../../docs/planning/PROJECT_TRACKER.md) for status. **Update, 18 Nov:** the board has since
been flashed with the [motor-bench profile](kubit-motor-bench.md) (4 MB flash, motors on
GPIO13/32/19/4, ESPLANE_V1 path); the portable diagnostic configuration described below is
the 31 Oct–7 Nov baseline and is no longer the installed image. Flight configuration remains pending.

## Current configuration

`sdkconfig.defaults.esp32` loads automatically for the `esp32` target after
the shared defaults.

| Setting | Value |
| --- | --- |
| Processor | Classic ESP32, dual core, 240 MHz |
| Hardware profile | Portable diagnostic profile; board outputs disabled |
| Transport | Native Wi-Fi / CRTP |
| ESP-NOW and legacy ESPlane app | Disabled |
| Position-hold and assisted command conversion | Disabled |
| Startup motor spin and buzzer | Disabled |
| External PSRAM | Disabled |
| Flash | DIO, 40 MHz, conservative 2 MB configuration (the motor-bench profile uses 4 MB; the board reports 4 MB) |
| Partition layout | Single factory application, 1 MiB app partition |
| Console | UART0, 115200 baud, through the USB-to-UART bridge |

The 2 MB setting is a provisional build limit, not detected flash capacity.
Check the module marking or flash ID before flashing. Configure additional
capacity once verified. These settings do not tune flight PID gains or establish
compatibility with the 8520 motors.

## Build

Open a fresh terminal outside cfclient's virtual environment:

```bash
esp_idf55
cd ~/Documents/Projects/microDrone/esp-drone
idf.py build
```

The local alias activates `~/.espressif/v5.5/esp-idf/export.sh`.
For a fresh checkout, or when switching from another chip, run
`idf.py set-target esp32` before building. That command clears the build and
regenerates `sdkconfig`; preserve configuration changes first. Do not copy the
S3 `sdkconfig` into the ESP32 project. Editing defaults does not override values
already saved in an existing `sdkconfig`.

The earlier S3 configuration and flash artifacts are archived in
`../../firmware-archives/esp32s3-diagnostic-2026-10-31/` relative to this document
(the archive is alongside the repository).

## Hardware integration still required

By default, the portable profile skips sensor/stabilization, motor, LED, buzzer
and battery monitoring initialization. Its GPIO defaults are placeholders, not
a wiring diagram. The optional `KUBIT_IMU_BENCH` enables only the MPU6050 path
and a diagnostic Mahony attitude task with explicitly configured pins. Flight
control remains disabled; Wi-Fi testing is separate from this no-radio bench.

Use the upstream **ESPlane V1 / classic ESP32** profile as the reference for
the next hardware configuration. Its MPU-6050 defaults are SDA GPIO21, SCL
GPIO22 and interrupt GPIO19; its motor outputs are GPIO4, GPIO33, GPIO32 and
GPIO25. These are reference-board assignments only. Kubit's own assignments (motors
GPIO13/32/19/4, INT23) are in [kubit-motor-bench.md](kubit-motor-bench.md). Confirm the DevKit's pinout,
boot constraints and actual circuit before using them. Changing hardware
profiles also requires reviewing every saved GPIO setting in `menuconfig`.

Before enabling the hardware profile:

1. Confirm the board revision, input supply and flash capacity.
2. Finalize MPU-6050 power, address, I2C pull-ups, interrupt and orientation.
3. Map the four AO3400 channels, motor order/direction and gate pull-downs.
   Keep startup motor spin disabled during bring-up.
4. Review unused extension sensors, LEDs, buzzer and battery voltage sensing;
   the reference drone PCB's circuits are not automatically present on a DevKit.
5. Validate 8520 motor PWM and response. The upstream menu offers 715/720
   presets; neither is a validated 8520 preset. Determine PID gains, motor
   behavior and battery thresholds from the assembled hardware.
6. Test sensor readings, off-state behavior, Wi-Fi loss and motor stopping before
   progressing from propeller-free bench tests to restrained flight tests.

The planned initial flight mode remains stabilized manual control with the
MPU-6050. Automatic altitude/position hold requires additional sensors.

## Build verification — 31 October 2026

- Upstream ESP-Drone base: `9d5a4f8`; local changes are the classic ESP32 defaults
  and documentation.
- SDK: ESP-IDF v5.5; compiler: Xtensa GCC 14.2.0.
- `idf.py set-target esp32` and `idf.py build` completed successfully.
- Generated configuration and flash arguments both select `esp32`.
- Application: `build/ESPDrone.bin`, size `0xbadd0` bytes; 27% of the 1 MiB
  application partition remains free.
- Existing non-fatal warnings remain: I2C const qualifiers, unused sound symbols
  with the buzzer disabled, and linker group termination.
- At this 31 October verification, no hardware tests had been performed. Later
  sensor/attitude results are linked above; Wi-Fi/motor/flight acceptance remains open.

The primary Markdown plan/handoff include this update. Previously generated
PDF plan exports remain historical snapshots and have not been regenerated.
