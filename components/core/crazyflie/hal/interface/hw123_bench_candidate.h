#ifndef HW123_BENCH_CANDIDATE_H
#define HW123_BENCH_CANDIDATE_H

#include <stdbool.h>
#include <stdint.h>

/* Provisional 5 Nov 2026 coefficients for Joseph's HW-123, measured at +/-2 g.
 * Arithmetic supports physical g at +/-16 g; cross-range accuracy is UNTESTED.
 * Native sensor axes only. Bench-only; never combine with startup accScale.
 */
static inline float hw123BenchCorrectG(float raw_g, unsigned axis)
{
    static const float offset_g[3] = {0.03817f, 0.01150f, 0.05154f};
    static const float scale[3] = {1.00415f, 0.99530f, 1.02034f};
    return (raw_g - offset_g[axis]) / scale[axis];
}

/* Reject the observed all-six-motion-channel-zero pattern; ignore temperature.
 * This is not a complete sensor-health check. Individual zero axes are valid.
 */
static inline bool hw123BenchMotionPayloadValid(const uint8_t data[14])
{
    for (unsigned i = 0; i < 14; ++i) {
        if (i != 6 && i != 7 && data[i] != 0) return true;
    }
    return false;
}

#endif
