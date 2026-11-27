
#ifndef FALLBACK_CONTROLLER_H
#define FALLBACK_CONTROLLER_H

#include "fallback_types.h"

/*
 * Internal controller state.
 */
typedef struct {
    FallbackConfig config;

    double prev_accel_mps2;

    bool initialized;

} FallbackController;

/*
 * Initialize controller parameters and state.
 */
bool fallback_init(
    FallbackController *controller,
    const FallbackConfig *config,
    double initial_accel_mps2
);

/*
 * Calculate one fallback control command.
 */
ControlCommand fallback_step(
    FallbackController *controller,
    const VehicleState *vehicle,
    uint64_t now_ns
);

#endif
