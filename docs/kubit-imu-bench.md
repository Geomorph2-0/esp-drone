# ESP-Drone MPU6050 attitude bench

**7 November update:** [INT23 candidate hardware checks](../../docs/hardware/HW123_INT23_ATTITUDE_2026-11-07.md)
completed slow/fast static runs; slow candidate restored. Current wiring uses
INT23. Historical legacy configuration and captures below use INT19.


**Current status — 5 November:** the slow static comparison and movement checks
have been captured. After the raw diagnostic exposed all-zero readings during
handling, a new connection/jumper test passed and a clean six-face capture produced
a candidate. The standalone correction trial was flashed and fresh face means
improved, but hand steadying left T08 inconclusive. Joseph authorized
[provisional ESP-Drone integration](kubit-hw123-integration.md) while support is
unavailable. A separate opt-in candidate now bypasses scalar normalization;
the original bench configuration remains the legacy baseline. See the
[tracker](../../docs/planning/PROJECT_TRACKER.md) and the dated results below.

This opt-in classic ESP32 diagnostic reuses ESP-Drone's MPU6050 driver, startup
gyro-bias calculation, acceleration magnitude scaling, hardware/software low-pass
filters, and `sensfusion6` Mahony implementation. It is a diagnostic task inside
ESP-Drone, not the full stabilizer/control loop. Output is through USB serial.

**Current diagnostic configuration, 4 November 2026:** the prepared local
`sdkconfig.imu-bench` and fresh bench defaults enable `KUBIT_IMU_BENCH_SLOW`.
This selects 100 kHz I2C and 100 Hz sensor sampling for the next reliability
comparison. Earlier captures below used 400 kHz / 1000 Hz. Motor inhibition and
wiring are unchanged. Full firmware startup/calibration may take about 15–20
seconds; leave the sensor stationary until calibration completes.

The portable safe profile remains selected. Motor, controller, battery-monitor,
LED, buzzer and extension-sensor initialization are skipped. Runtime arm queries
always return false in this bench mode. The dedicated build also selects no
radio. With `KUBIT_HW123_CANDIDATE` off, this test evaluates the existing single
magnitude scale. The separate candidate configuration applies provisional
per-axis correction; see its integration notes for limits and commands.

## Wiring and run

**7 November:** the current HW-123 candidate uses **INT → GPIO23** at Joseph’s
request. The table/commands below describe the legacy INT19 bench. Use the
[candidate build guide](kubit-hw123-integration.md) for the current soldered
assembly. The firmware prints its configured interrupt pin at startup.

Exit any serial monitor with Ctrl+], then unplug USB before adding the interrupt
wire. Keep motors, motor drivers and battery disconnected.

| ESP32 | HW-123 |
| --- | --- |
| 3V3 | VCC |
| GND | GND |
| GPIO21 | SDA |
| GPIO22 | SCL |
| GND | AD0 |
| GPIO19 | INT |

INT19 is required: the upstream driver waits for data-ready interrupts, unlike
the previous polling test. Leave XDA/XCL unconnected. This build preserves native
MPU axis ordering, matching the earlier bench readings; final airframe mounting
and orientation still need to be defined.

Reconnect USB, put the module flat and still with +Z upward, then run:

```bash
esp_idf55
cd ~/Documents/Projects/microDrone/esp-drone
idf.py -B build-imu-bench -D SDKCONFIG=sdkconfig.imu-bench -p /dev/ttyUSB0 -b 115200 flash monitor
```

The separate build/config paths preserve the existing portable Wi-Fi build.
Flashing replaces the standalone test on the ESP32. Keep the sensor stationary
during boot/self-test/calibration and for 10 seconds after the first attitude
line. Expected messages include:

```text
MPU6050 I2C connection [OK].
Keep flat and still: calibrating; motor/controller outputs disabled.
Calibration complete; Mahony attitude active (native sensor axes).
RPY[deg]=... gyro[dps]=... |a|=...
```

The printed gyro values are bias-corrected and low-pass filtered. They should
settle near zero while stationary. Roll/pitch should settle to a consistent
orientation and change smoothly with slow tilts; residual offsets may remain.
Return to the initial pose and check repeatability. Yaw is relative and can
drift without a heading reference; it is not a compass measurement.

Share the startup messages, 10 stationary attitude lines, and a few lines during
slow tilt and return. A sample timeout points to INT wiring, bus or power; the
task stops updating attitude rather than using old data. A self-test failure or
calibration timeout needs diagnosis before proceeding. The built-in sensor
self-test is limited to three attempts for this bench mode. Motor testing is a
separate stage.

## Reproduce the configuration

The prepared `sdkconfig.imu-bench` is local/ignored. For a fresh checkout with no
such file, use:

```bash
idf.py -B build-imu-bench -D SDKCONFIG=sdkconfig.imu-bench \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32;sdkconfig.defaults.imu-bench' \
  build
```

Defaults do not override existing saved configuration. The bench source enforces
the three sensor GPIOs at compile time. `CONFIG_KUBIT_IMU_BENCH` defaults off and
is available only for the portable safe profile on classic ESP32.

## Verification — 2 November 2026

- ESP-IDF v5.5 classic ESP32 build passed; bench application is 257808 bytes.
- The original portable Wi-Fi configuration also builds, with all previously
  saved enabled configuration values preserved and bench mode off.
- Host simulation of the diagnostic task passed initialization failure, missing
  samples, calibration timeout, normal fusion updates and excessive sample-gap
  cases. This does not simulate the physical MPU6050 or validate attitude accuracy.
- Linked bench ELF includes the sensor/fusion diagnostic but omits motor/PWM
  initialization and controller/stabilizer initialization. `motorsSetRatio` is
  retained by other firmware references but rejects use before motor initialization;
  bench arm queries additionally return false.
- Hardware flash, interrupt delivery, self-test, calibration and attitude results
  were pending at build verification; see the first hardware capture below.
  Existing unused sound-symbol and linker-group
  warnings do not prevent the builds.

## First hardware capture — 2 November 2026

The reported boot ran the bench image (ELF hash prefix `4abfb68da`, matching the
prepared bench ELF). It reached MPU6050 detection, the self-test gate, gyro-bias
completion at 5.042 seconds, and active Mahony updates at 6.044 seconds. INT-driven
sampling therefore worked. The supplied log contains 229 intact attitude lines.

For the stationary-looking interval 16–51 seconds, corrected gyro means were
(+0.0082, -0.0078, +0.0035) degrees/second; acceleration magnitude averaged
1.000 g with range 0.995–1.004 g. Roll settled around +0.50 degrees and pitch
around +0.66 degrees. The magnitude result reflects the driver's startup scaling,
not independent six-face calibration. The end of the capture begins to show
movement but does not establish tilt/return repeatability.

Five I2C NACK transactions occurred around 13.1, 26.2, 34.3, 37.8 and 47.6 seconds.
Readings resumed afterward, but bus reliability remains unresolved. The bus now
runs at 400 kHz versus the standalone test's 100 kHz. Check short, secure SDA/SCL,
ground and power connections with USB unplugged, then repeat a two-minute static
capture before progressing. The startup pull-up warning is emitted whenever
internal pull-ups are disabled; it does not measure the actual external resistors.
Do not assume it proves that onboard pull-ups are absent. Inspect the module's
pull-ups and supply if transaction failures persist.

The monitor also reported an ELF checksum mismatch and one line contains garbled
characters. Open the monitor with the matching bench build rather than a default
or older ELF:

```bash
idf.py -B build-imu-bench -D SDKCONFIG=sdkconfig.imu-bench -p /dev/ttyUSB0 monitor
```

No reflash is needed merely to select the matching monitor ELF. Persistent garbled
serial text warrants checking the USB cable/connection separately from the I2C bus.

## Follow-up capture — reported 3 November 2026

The capture contains two boots, separated by a ROM `POWERON_RESET` message.
The user confirmed that the reset/reconnection was deliberate and that a
multimeter is available. This capture does not establish a spontaneous supply
failure. The first boot has 189
attitude lines through 43.821 seconds and two NACK transactions. The second has
1317 attitude lines from 6.044 through 270.495 seconds and six NACK transactions,
at approximately 88.9, 154.5, 201.8, 216.3, 252.9 and 259.0 seconds. No checksum
mismatch, garbled text, diagnostic sample-timeout or sample-gap stop appears.
The printed attitude cadence continues across the failed transactions; this
does not mean every underlying 1 kHz sensor sample succeeded.

After 16 seconds in the second run, corrected gyro means are
(+0.0132, -0.0030, -0.0023) degrees/second, magnitude spans 0.995–1.005 g, and
pitch remains around +2.02 degrees. The final ten seconds show roll around
-0.112, pitch +2.010 and yaw -0.614 degrees. Pitch is stable, but physical level
alignment has not been measured, so the value is not proof of a calibration
fault or absolute attitude accuracy. The existing calibration/fusion path is
operating; intermittent bus reliability is still the unresolved next task.

Do not add another estimator or hard-code pitch correction based on this log.
Next measure module VCC-to-GND and idle SDA/SCL-to-GND with the SDA/SCL jumpers
disconnected at the ESP32 end (change wiring only with USB unplugged). Retain
3V3/VCC, common ground and grounded AD0 for this powered DC-voltage measurement.
The running firmware will report missing samples while the bus is disconnected;
that is expected. These DC measurements do not establish 400 kHz signal rise time.
Inspect module pull-ups, wiring and supply; if a lower bus-speed comparison is needed,
account for sensor sample
rate and transfer time rather than changing the bus clock alone.

**Corrected multimeter report, 3 November:** The user clarified that the initial
3.23 V supply reading and drifting SDA/SCL measurements were made on the ESP32
alone, with the HW-123 completely disconnected and unpowered. They do not
establish the module supply or pull-up condition; the earlier floating-module
interpretation is withdrawn.

For the repeat test, instructions were to power the HW-123 from ESP32 3V3/GND,
ground AD0, and leave SDA/SCL/INT/XDA/XCL disconnected. The user now reports the
module's SDA and SCL mostly steady at 3.22 V, occasionally 3.21 V. This supports
an effective idle pull-up path; 0.01 V variation is not concerning. It does not
measure pull-up resistance or prove acceptable rise time at 400 kHz. A new module
VCC reading was not supplied. Do not add resistors based on the invalid initial
measurements. Next inspect the pull-up paths with the module fully disconnected
and unpowered: resistance from SDA to VCC and SCL to VCC, noting that regulator
topology and parallel paths may make in-circuit readings ambiguous.

**Photo inspection, 3 November:** The supplied DT830L photo shows the dial at
the 2k/diode position, a lone overrange/open-circuit `1`, and a battery warning
icon. Replace its battery before relying on further measurements and use 20k
for nominal 2.2 kOhm resistors. The HW-123 component photo shows two resistors
marked `222` just above the MPU package, nominally 2.2 kOhm each. They are likely
I2C pull-ups, but exact connections are not established by the photo. Do not
add parallel resistors on this evidence. With the module fully disconnected and
unpowered, direct measurements across the two ends of each marked resistor on
the 20k range are more targeted than VCC-to-header measurements, although
parallel circuit paths can still lower readings. Do not desolder components.

## Slow comparison — 4 November 2026

The user replaced the meter battery and checked two loose 10 kOhm resistors,
both reading about 9.8 kOhm. The rising 600 kOhm–1.1 MOhm values were measured
between VCC and the bus header pins, not across the resistors. Direct resistor
measurements gave 2.2 kOhm. No parallel pull-ups were added. The user reported
completion of the jumper-check/reconnection step; individual wire readings
were not supplied.

The optional slow mode changes these settings together:

| Setting | Original bench | Slow comparison |
| --- | --- | --- |
| I2C clock | 400 kHz | 100 kHz |
| MPU sample rate | 1000 Hz | 100 Hz (divider 9) |
| MPU gyro DLPF | 98 Hz | 20 Hz (accelerometer 21 Hz) |
| Software gyro/acceleration LPF | 80/30 Hz at 1000 Hz sampling | 20/20 Hz at 100 Hz sampling |
| Post-gyro-calibration settling | 1 second | 3 seconds |
| Maximum fusion sample gap | 20 ms | 100 ms |

The same 1024-sample gyro-bias window now takes about 10.24 seconds, and the
200-sample acceleration scale takes about 2 seconds. The extended settling
period accommodates that. Mahony integration continues to use actual sample
timestamps. Register timing follows the [MPU6050 register map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf).
Both modes print cumulative `Bus reads: ok=... failed=...` every five seconds
while samples arrive. These counters cover streaming reads after sensor setup,
not boot-time register transactions. Failed reads remain excluded from fusion.

Use the flash/monitor command above. Check the boot line says `100000 Hz` and
the diagnostic banner says `samples=100 Hz`. Leave the module motionless for
five minutes after calibration, then share startup, all errors and the final
counter/attitude lines. A clean result would support a speed/load-sensitive
problem; it would not isolate clock speed alone or establish flight readiness,
since the transfer rate and filtering have also changed.

To return to the fast diagnostic for a later comparison, use
`idf.py -B build-imu-bench -D SDKCONFIG=sdkconfig.imu-bench menuconfig`, open
ESPDrone Config and disable `Kubit bench: 100 kHz I2C and 100 Hz sensor sampling`.
Rebuild/flash with the same bench command. Normal firmware configurations keep
this option off; do not use the slow diagnostic as a flight-control configuration.

Verification on 4 November: the slow bench firmware built successfully with
ESP-IDF v5.5 (258048-byte application). Host checks of both diagnostic modes
passed initialization failure, missing samples, calibration timeout, successful
fusion and excessive sample-gap cases. The bench ELF contains the attitude task
and fusion function, but excludes motor, controller, stabilizer and LEDC PWM
initialization functions. The normal firmware also built successfully with bench
mode disabled. At build verification the physical comparison was pending; the later results follow.


## Later hardware captures — 4–5 November 2026

The slow static capture continued 306.5 seconds after calibration and ended at
31,585 successful reads / zero failed, without NACKs or sample-gap stops. One
text line was garbled. Tilt/return and roll-pose captures ended at 11,044/0 and
10,543/0 respectively. Attitude tracked movement, but resting magnitude depended
on orientation (~0.90–0.94 g upright versus ~1 g flat). The driver uses one scalar
normalization, not independent accelerometer axis calibration.

The subsequent raw six-face test had 167 all-zero logged rows associated with
handling; the user confirmed deliberate resets. On 5 November the next raw run
had 1,236 rows spanning 247 seconds, including reported jumper tugging, with no
all-zero rows, reported read failures or subsequent boots. The exact repair is
not recorded; a stable capture does not identify the original faulty contact.

A replacement six-face capture yielded 788 readable rows and the
[5 November candidate](../../docs/hardware/HW123_CALIBRATION_2026-11-05.md).
Two corrupted lines were excluded and +Z was noisy/tilted. The
[evidence index](../../docs/planning/PROJECT_EVIDENCE.md) links preserved captures.
The standalone +/-2 g candidate subsequently ran a fresh raw/corrected trial;
hand steadying left validation inconclusive. The new opt-in +/-16 g integration
is a provisional experiment with scalar normalization bypassed, not an accepted
cross-range calibration. The 400 kHz / 1 kHz configuration still needs reliability
regression after the connection check. See [current integration notes](kubit-hw123-integration.md).
