# Provisional HW-123 integration — 5 November 2026

Joseph authorized proceeding with the existing readings because hands-free
support is not currently available. T08 remains inconclusive; its unresolved
accuracy/repeatability checks do not block this T09 software experiment.
[Fresh results](../../docs/hardware/HW123_VALIDATION_2026-11-05.md) show all six
means improved. They do not establish calibration accuracy or flight readiness.

## Implementation

`CONFIG_KUBIT_HW123_CANDIDATE` defaults off and depends on the existing
`KUBIT_IMU_BENCH` portable-safe classic ESP32 diagnostic. A source guard also
rejects candidate compilation without bench mode. The dedicated candidate
configuration selects no radio and retains motor/controller inhibition.

The MPU6050 remains at +/-16 g and +/-2000 degrees/second. The candidate path
checks those range bits after configuration, including transport status, and
stops initialization on mismatch/read failure. Its native-axis acceleration
pipeline is:

```text
signed raw counts × (32 / 65536) g/count
  → (raw_g − axis_offset_g) / axis_scale
  → existing mounting alignment
  → existing acceleration low-pass filter
  → bench Mahony attitude estimator
```

Offsets are (+0.03817, +0.01150, +0.05154) g; scales are
(1.00415, 0.99530, 1.02034). These are the existing 5 November candidate,
not a new fit to hand-steadied validation data. No fixed gyro offset is added:
upstream startup gyro-bias estimation and gyro filtering remain in use.
The candidate bypasses both accumulation and application of startup `accScale`
to avoid double normalization. Legacy configurations retain their scalar and
axis mappings.

The conversion arithmetic is range-aware, but the coefficients were measured at
+/-2 g. Physical bias/scale transfer to +/-16 g remains untested; arithmetic
agreement cannot validate that assumption. These coefficients belong only to
the measured HW-123, not another HW-123/GY-521 or a flight airframe.

All IMU bench configurations now reject the known all-six-motion-channel-zero
payload before gyro calibration, acceleration correction, filtering or queue
publication. Temperature bytes are ignored; individual zero axes remain valid.
The bench attitude consumer stops after observing any invalid-payload count;
continuous invalids cause its existing two-second fresh-sample timeout. This
detects the observed fault pattern, not every possible stuck/corrupt sensor.
Normal flight-path sensor fault handling remains T06/T31 work.

Counters now print `ok`, `failed`, and `invalid`. `ok` means successful I2C
transactions and includes `invalid`; valid streaming payloads equal `ok-invalid`.
They are not counts of internal sensor acquisitions or proof that every data-ready
interrupt was serviced. Failed reads are excluded; existing small-gap tolerance
remains, so a continuing attitude stream does not establish a fault-free bus.

Each queued bench sample now carries raw acceleration from the same acquisition
as its processed data. Output adds `raw_acc[g]` (native, unfiltered g) and
`acc[g]` (post-correction/normalization, alignment and filtering). Compare
stationary windows after filter settling, not individual moving samples.

## Build and later hardware session

The original portable Wi-Fi build and original bench configuration remain
separate. Create the candidate with:

```bash
esp_idf55
cd /home/rbj/Documents/Projects/microDrone/esp-drone
idf.py -B build-hw123-candidate -D SDKCONFIG=sdkconfig.hw123-candidate \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32;sdkconfig.defaults.imu-bench;sdkconfig.defaults.hw123-candidate' \
  build
```

Defaults seed a fresh configuration; they do not override a saved one. Verify
`KUBIT_IMU_BENCH`, `KUBIT_IMU_BENCH_SLOW`, `KUBIT_HW123_CANDIDATE`,
`TARGET_PORTABLE_SAFE`, and `TRANSPORT_NONE` are enabled, and
`CONFIG_MPU_PIN_INT=23` (7 November soldering-convenience change).

The 5 November software work did not flash the board. On 7 November the
INT23 candidate was built/flashed and tested in slow and fast stationary modes;
see the [hardware report](../../docs/hardware/HW123_INT23_ATTITUDE_2026-11-07.md).
The slow candidate is now restored on the board.
For a future USB-only bench session, keep motors/drivers/battery disconnected.
The ESP-Drone bench additionally requires INT → GPIO23; SDA21/SCL22 and other
wiring remain as documented in [the attitude bench guide](kubit-imu-bench.md).
Unplug USB before changing wiring. Then:

```bash
idf.py -B build-hw123-candidate -D SDKCONFIG=sdkconfig.hw123-candidate \
  -p /dev/ttyUSB0 -b 115200 flash monitor
```

Confirm range readback, provisional-candidate banner, 100 kHz / 100 Hz mode,
gyro calibration and counters. Keep still during startup calibration. Preserve
the full log. A hand-steadied session can check startup, basic attitude response
and communication; it must not close T08's stationary accuracy/repeatability test.
Do not retune coefficients to compensate for hand motion or unmeasured tilt.

After slow-mode results are reviewed, use the candidate's `menuconfig` to disable
`KUBIT_IMU_BENCH_SLOW` and rebuild for the 400 kHz / 1 kHz diagnostic. This changes
sample rate and filters together. Record static and motion captures, counters,
resets, invalid payloads and sample-gap stops. Intended flight-rate reliability
remains a hardware requirement; slow-mode success does not establish it.

## Verification scope

`python3 tests/test_hw123_pipeline.py` extracts the actual driver processing
function and compiles it with instrumented host stubs under C11 warnings-as-errors
and undefined-behavior sanitizer. It exercises candidate and legacy paths,
six signed faces at +/-16 g quantization, signed extrema, scale bypass, gyro-bias
subtraction, axis mapping and alignment/filter call order. Payload checks cover
temperature-only zeros and individual nonzero motion bytes; a source-order check
ensures rejection precedes processing/publication. Hardware, actual filters,
gyro-bias convergence and task scheduling are not simulated by these tests.

All three ESP-IDF v5.5 builds passed: candidate, original bench and portable Wi-Fi.
The linked candidate ELF contains the bench/fusion/sensor task and excludes motor,
PWM, controller and stabilizer initialization plus scalar-normalization state.
The clean build retained existing unused sound-symbol and I2C const-qualifier
warnings. [Verification record and hashes](../../docs/hardware/evidence/2026-11-05/t09-software/verification.json)
link the saved logs and configuration snapshots. No hardware acceptance is
inferred from a build, and no image was flashed in this implementation session.

## 7 November interrupt-pin revision

Joseph selected GPIO23 for soldering convenience. The candidate defaults and
saved candidate configuration now use INT23; source guards accept 19 or 23 and
startup/timeout messages report the configured pin. The legacy bench remains
INT19. GPIO23 is unused in TARGET_PORTABLE_SAFE; the ESP-Lane V1 red-LED
assignment belongs to an inactive profile. (Update, 13–14 Nov: the
[motor-bench profile](kubit-motor-bench.md) uses the ESPLANE_V1 path and moves the LEDs to
GPIO2 so that INT23 stays free.) SDA21/SCL22 remain unchanged.
Build verification does not confirm the physical solder joint or interrupt delivery.

**7 November runtime update:** Joseph reported soldering INT23 and authorized
unattended checks. Both modes calibrated and streamed with zero reported
failed/invalid reads; five-minute static windows preserved. Fast serviced rate
was about 989.6/s, not proven lossless 1 kHz. Slow image restored after comparison;
controlled motion/return, supported edges and flight acceptance remain pending.
