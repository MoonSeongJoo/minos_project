
#include "fallback_controller.h"

#include <math.h>
#include <stddef.h>

/*
 * Initialize the fallback controller.
 */
bool fallback_init(
    FallbackController *controller,
    const FallbackConfig *config,
    double initial_accel_mps2)
{
    if (controller == NULL)
        return false;

    controller->initialized = false;

    if (config == NULL)
        return false;

    /*
     * Reject NaN and infinity.
     */
    if (!isfinite(config->control_period_s) ||
        !isfinite(config->decel_limit_mps2) ||
        !isfinite(config->jerk_limit_mps3) ||
        !isfinite(config->stop_speed_mps) ||
        !isfinite(initial_accel_mps2)) {

        return false;
    }

    /*
     * Validate configuration ranges.
     */
    if (config->control_period_s <= 0.0 ||
        config->decel_limit_mps2 <= 0.0 ||
        config->jerk_limit_mps3 <= 0.0 ||
        config->stop_speed_mps < 0.0) {

        return false;
    }

    /*
     * Store controller configuration.
     */
    controller->config = *config;

    controller->prev_accel_mps2 =
        initial_accel_mps2;

    controller->initialized = true;

    return true;
}

/*
 * Calculate the next fallback control command.
 */
ControlCommand fallback_step(
    FallbackController *controller,
    const VehicleState *vehicle,
    uint64_t now_ns)
{
    /*
     * Default output: invalid command.
     */
    ControlCommand cmd = {
        .accel_mps2 = 0.0,
        .hold_stop = false,
        .valid = false,
        .source = CMD_SOURCE_FALLBACK,
        .timestamp_ns = now_ns
    };

    /*
     * Step 1. Validate input.
     */
    if (controller == NULL ||
        vehicle == NULL ||
        !controller->initialized ||
        !vehicle->valid ||
        !isfinite(vehicle->speed_mps) ||
        vehicle->speed_mps < 0.0) {

        return cmd;
    }

    const FallbackConfig *cfg =
        &controller->config;

    /*
     * Step 2. Check full-stop condition.
     */
    if (vehicle->speed_mps <=
        cfg->stop_speed_mps) {

        cmd.accel_mps2 = 0.0;

        cmd.hold_stop = true;

        cmd.valid = true;

        controller->prev_accel_mps2 =
            0.0;

        return cmd;
    }

    /*
     * Step 3. Calculate target acceleration.
     */
    double target_accel =
        -cfg->decel_limit_mps2;

    /*
     * Step 4. Calculate jerk limitation.
     */
    double max_delta =
        cfg->jerk_limit_mps3 *
        cfg->control_period_s;

    double delta =
        target_accel -
        controller->prev_accel_mps2;

    /*
     * Step 5. Clamp acceleration change.
     */
    delta = fmax(
        -max_delta,
        fmin(max_delta, delta)
    );

    /*
     * Step 6. Generate new command.
     */
    cmd.accel_mps2 =
        controller->prev_accel_mps2 +
        delta;

    cmd.valid = true;

    /*
     * Step 7. Save internal state
     * for the next control cycle.
     */
    controller->prev_accel_mps2 =
        cmd.accel_mps2;

    return cmd;
}
