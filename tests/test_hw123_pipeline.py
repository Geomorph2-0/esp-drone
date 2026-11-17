#!/usr/bin/env python3
"""Compile the actual acceleration/gyro processing function with host stubs.

Exercises candidate and legacy axis/scale paths. Hardware, real gyro-bias
estimation, alignment and filters are not simulated; their call order is checked.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "components/core/crazyflie/hal/src/sensors_mpu6050_hm5883L_ms5611.c"
INCLUDE = ROOT / "components/core/crazyflie/hal/interface"


def function(text, signature):
    start = text.index(signature + "\n{")
    body = text.index("{", start)
    depth = 1
    end = body + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


HARNESS = r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "hw123_bench_candidate.h"
typedef struct { float x,y,z; } Axis3f;
typedef struct { int16_t x,y,z; } Axis3i16;
typedef int lpf2pData;
static Axis3i16 accelRaw, gyroRaw;
static Axis3f gyroBias;
static struct { Axis3f acc,gyro; } sensorData;
static bool gyroBiasFound;
static float accScale = 2.0f;
static int biasCalls, scaleCalls, stage;
static lpf2pData gyroLpf[3], accLpf[3];
#define SENSORS_G_PER_LSB_CFG (1.0f/2048.0f)
#define SENSORS_DEG_PER_LSB_CFG (4000.0f/65536.0f)
bool processGyroBias(int16_t x,int16_t y,int16_t z,Axis3f *out) {
    (void)x; (void)y; (void)z;
    biasCalls++; *out=(Axis3f){1,2,3}; return true;
}
bool processAccScale(int16_t x,int16_t y,int16_t z) {
    (void)x; (void)y; (void)z; scaleCalls++; return true;
}
void applyAxis3fLpf(lpf2pData *filter, Axis3f *in) {
    (void)in;
    if (filter == gyroLpf) { assert(stage == 0); stage = 1; }
    else { assert(filter == accLpf && stage == 2); stage = 3; }
}
void sensorsAccAlignToGravity(Axis3f *in, Axis3f *out) {
    assert(stage == 1); stage = 2; *out = *in;
}
/* PROCESS_FUNCTION */
static void word(uint8_t *p,int16_t v) { p[0]=(uint16_t)v>>8; p[1]=(uint16_t)v; }
static void near(float actual,float expected,float tolerance) {
    assert(fabsf(actual-expected) < tolerance);
}
static void run(uint8_t b[14]) { stage=0; processAccGyroMeasurements(b); assert(stage==3); }
int main(void) {
    uint8_t b[14]={0};
    assert(!hw123BenchMotionPayloadValid(b));
    b[6]=0xff; b[7]=0xff;
    assert(!hw123BenchMotionPayloadValid(b));
    for (int i=0;i<14;i++) {
        if(i==6 || i==7) continue;
        b[i]=1; assert(hw123BenchMotionPayloadValid(b)); b[i]=0;
    }
    word(b,2048); word(b+2,-4096); word(b+4,1024);
    word(b+8,10); word(b+10,-20); word(b+12,30);
    run(b);
#ifdef CONFIG_KUBIT_HW123_CANDIDATE
    near(sensorData.acc.x, (1.0f-0.03817f)/1.00415f, 0.00001f);
    near(sensorData.acc.y, (-2.0f-0.01150f)/0.99530f, 0.00001f);
    near(sensorData.acc.z, (0.5f-0.05154f)/1.02034f, 0.00001f);
    assert(scaleCalls==0 && accScale==2.0f); /* scalar must be bypassed */
    const float offset[3]={0.03817f,0.01150f,0.05154f};
    const float scale[3]={1.00415f,0.99530f,1.02034f};
    for(int axis=0;axis<3;axis++) for(int sign=-1;sign<=1;sign+=2) {
        for(int k=0;k<3;k++) word(b+2*k,(int16_t)lroundf((offset[k]+(axis==k?sign*scale[k]:0))*2048));
        run(b);
        near(sensorData.acc.x,axis==0?sign:0,0.0005f);
        near(sensorData.acc.y,axis==1?sign:0,0.0005f);
        near(sensorData.acc.z,axis==2?sign:0,0.0005f);
    }
    word(b,INT16_MIN); word(b+2,INT16_MAX); run(b);
    near(sensorData.acc.x,(-16.0f-offset[0])/scale[0],0.00001f);
    near(sensorData.acc.y,(32767/2048.0f-offset[1])/scale[1],0.00001f);
#elif defined(CONFIG_KUBIT_IMU_BENCH)
    near(sensorData.acc.x,0.5f,0.00001f);
    near(sensorData.acc.y,-1.0f,0.00001f);
    near(sensorData.acc.z,0.25f,0.00001f);
    assert(scaleCalls==1);
#else
    near(sensorData.acc.x,1.0f,0.00001f); /* swapped Y then inverted X */
    near(sensorData.acc.y,0.5f,0.00001f);
    near(sensorData.acc.z,0.25f,0.00001f);
    assert(scaleCalls==1);
#endif
#ifdef CONFIG_KUBIT_IMU_BENCH
    near(sensorData.gyro.x,9*SENSORS_DEG_PER_LSB_CFG,0.00001f);
    near(sensorData.gyro.y,-22*SENSORS_DEG_PER_LSB_CFG,0.00001f);
#else
    near(sensorData.gyro.x,21*SENSORS_DEG_PER_LSB_CFG,0.00001f);
    near(sensorData.gyro.y,8*SENSORS_DEG_PER_LSB_CFG,0.00001f);
#endif
    near(sensorData.gyro.z,27*SENSORS_DEG_PER_LSB_CFG,0.00001f);
    assert(biasCalls>0 && gyroBiasFound);
    return 0;
}
'''


def main():
    text = SOURCE.read_text()
    body = function(text, "void processAccGyroMeasurements(const uint8_t *buffer)")
    # Verify invalid data is gated before calibration and queue publication.
    task = function(text, "static void sensorsTask(void *param)")
    gate = task.index("if (!hw123BenchMotionPayloadValid(buffer))")
    process = task.index("processAccGyroMeasurements(&(buffer[0]))")
    assert gate < task.index("continue;", gate) < process < task.index("xQueueOverwrite")
    with tempfile.TemporaryDirectory(prefix="kubit-pipeline-") as tmp:
        source = Path(tmp) / "test.c"
        source.write_text(HARNESS.replace("/* PROCESS_FUNCTION */", body))
        for name, defines in (
            ("candidate", ["CONFIG_KUBIT_IMU_BENCH", "CONFIG_KUBIT_HW123_CANDIDATE"]),
            ("legacy-bench", ["CONFIG_KUBIT_IMU_BENCH"]),
            ("legacy-board", []),
        ):
            binary = Path(tmp) / name
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined", *["-D"+d for d in defines],
                            "-I", str(INCLUDE), str(source), "-lm", "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
            print(name + ": passed", flush=True)


if __name__ == "__main__":
    main()
