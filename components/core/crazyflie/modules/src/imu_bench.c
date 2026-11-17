#include "sdkconfig.h"
#ifdef CONFIG_KUBIT_IMU_BENCH
#include <math.h>
#include <inttypes.h>
#include "FreeRTOS.h"
#include "task.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "imu_bench.h"
#include "sensors.h"
#include "sensors_mpu6050_hm5883L_ms5611.h"
#include "sensfusion6.h"
#include "static_mem.h"
#include "system.h"

#if CONFIG_I2C0_PIN_SDA != 21 || CONFIG_I2C0_PIN_SCL != 22 || (CONFIG_MPU_PIN_INT != 19 && CONFIG_MPU_PIN_INT != 23)
#error "Kubit IMU bench requires SDA21, SCL22 and INT19 or INT23"
#endif

static const char *TAG = "imu_bench";
#ifdef CONFIG_KUBIT_IMU_BENCH_SLOW
#define BENCH_SETTLE_US 3000000
#define BENCH_MAX_DT 0.1f
#else
#define BENCH_SETTLE_US 1000000
#define BENCH_MAX_DT 0.02f
#endif
STATIC_MEM_TASK_ALLOC(imuBenchTask, STABILIZER_TASK_STACKSIZE);

static void imuBenchTask(void *arg)
{
    (void)arg;
    sensorData_t data = {0};
    mpu6050BenchSample_t sample = {0};
    uint64_t previous = 0;
    int64_t calibratedAt = 0, lastPrint = 0, lastStats = 0;
    bool ready = false;
    systemWaitStart();
    const int64_t started = esp_timer_get_time();
    ESP_LOGI(TAG, "Wiring: SDA%d SCL%d INT%d", CONFIG_I2C0_PIN_SDA, CONFIG_I2C0_PIN_SCL, CONFIG_MPU_PIN_INT);
#ifdef CONFIG_KUBIT_IMU_BENCH_SLOW
    ESP_LOGI(TAG, "Slow comparison: I2C=100000 Hz, samples=100 Hz; allow about 20 seconds for calibration.");
#else
    ESP_LOGI(TAG, "Fast comparison: I2C=400000 Hz, samples=1000 Hz.");
#endif
    ESP_LOGI(TAG, "Keep flat and still: calibrating; motor/controller outputs disabled.");
#ifdef CONFIG_KUBIT_HW123_CANDIDATE
    ESP_LOGW(TAG, "Provisional 5 November HW-123 correction: +/-16 g transfer unvalidated; startup accScale bypassed; gyro bias retained.");
#else
    ESP_LOGI(TAG, "Acceleration uses upstream startup scalar normalization (candidate disabled).");
#endif
    for (;;) {
        if (!sensorsMpu6050ReadBenchSample(&sample, 2000)) {
            uint32_t ok, failed, invalid;
            sensorsMpu6050BenchReadCounts(&ok, &failed, &invalid);
            ESP_LOGE(TAG, "Stopped counts: ok=%" PRIu32 " failed=%" PRIu32 " invalid=%" PRIu32, ok, failed, invalid);
            ESP_LOGE(TAG, "No fresh IMU samples. Unplug USB; check INT%d, SDA%d, SCL%d and power. Reset after fixing.", CONFIG_MPU_PIN_INT, CONFIG_I2C0_PIN_SDA, CONFIG_I2C0_PIN_SCL);
            break;
        }
        data = sample.data;
        const int64_t now = esp_timer_get_time();
        uint32_t ok, failed, invalid;
        sensorsMpu6050BenchReadCounts(&ok, &failed, &invalid);
        if (invalid) {
            ESP_LOGE(TAG, "Invalid motion payload detected; stopping attitude updates. ok=%" PRIu32 " failed=%" PRIu32 " invalid=%" PRIu32, ok, failed, invalid);
            break;
        }
        if (now - lastStats >= 5000000) {
            ESP_LOGI(TAG, "Bus reads: ok=%" PRIu32 " failed=%" PRIu32 " invalid=%" PRIu32, ok, failed, invalid);
            lastStats = now;
        }
        const float dt = previous ? (data.interruptTimestamp - previous) * 1e-6f : 0;
        previous = data.interruptTimestamp;
        if (!sensorsAreCalibrated()) {
            if (now - started > 30000000) {
                ESP_LOGE(TAG, "Calibration timed out. Support the module flat and still, then reset.");
                break;
            }
            continue;
        }
        // Allow LPFs (and legacy 200-sample acceleration scaling, if used) to settle.
        if (!calibratedAt) calibratedAt = now;
        if (now - calibratedAt < BENCH_SETTLE_US) continue;
        if (!ready) {
            sensfusion6Init();
            ESP_LOGI(TAG, "Calibration complete; Mahony attitude active (native sensor axes).");
            ready = true;
        }
        if (dt <= 0 || dt > BENCH_MAX_DT) {
            ESP_LOGE(TAG, "IMU sample gap %.4f s; stopping attitude updates. Reset to retry.", dt);
            break;
        }
        sensfusion6UpdateQ(data.gyro.x, data.gyro.y, data.gyro.z,
                         data.acc.x, data.acc.y, data.acc.z, dt);
        if (now - lastPrint >= 200000) {
            float roll, pitch, yaw;
            sensfusion6GetEulerRPY(&roll, &pitch, &yaw);
            ESP_LOGI(TAG, "RPY[deg]=%+.2f,%+.2f,%+.2f gyro[dps]=%+.2f,%+.2f,%+.2f |a|=%.3f acc[g]=%+.5f,%+.5f,%+.5f raw_acc[g]=%+.5f,%+.5f,%+.5f",
                     roll, pitch, yaw, data.gyro.x, data.gyro.y, data.gyro.z,
                     sqrtf(data.acc.x*data.acc.x + data.acc.y*data.acc.y + data.acc.z*data.acc.z),
                     data.acc.x, data.acc.y, data.acc.z,
                     sample.rawAccG.x, sample.rawAccG.y, sample.rawAccG.z);
            lastPrint = now;
        }
    }
    vTaskDelete(NULL);
}

bool imuBenchInit(void)
{
    sensorsInit();
    if (!sensorsTest()) {
        ESP_LOGE(TAG, "MPU6050 initialization/self-test failed; attitude task not started.");
        return false;
    }
    STATIC_MEM_TASK_CREATE(imuBenchTask, imuBenchTask, "IMU_BENCH", NULL, STABILIZER_TASK_PRI);
    return true;
}
#endif
