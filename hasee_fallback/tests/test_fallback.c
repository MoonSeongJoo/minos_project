
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "fallback_controller.h"

#define CONTROL_DT     0.02
#define STEP_NS        20000000ULL

#define TRIGGER_STEP   150U
#define TOTAL_STEPS    1000U

#define EPSILON        0.000001

/*
 * Apply a command to a simulated vehicle.
 */
static void vehicle_apply_command(
    VehicleState *vehicle,
    const ControlCommand *cmd)
{
    assert(vehicle != NULL);
    assert(cmd != NULL);
    assert(cmd->valid);

    double prev_speed =
        vehicle->speed_mps;

    double next_speed;

    if (cmd->hold_stop) {

        next_speed = 0.0;

    } else {

        next_speed = fmax(
            0.0,
            prev_speed +
            cmd->accel_mps2 * CONTROL_DT
        );
    }

    /*
     * Trapezoidal integration.
     */
    vehicle->distance_m +=
        (prev_speed + next_speed) *
        0.5 * CONTROL_DT;

    vehicle->speed_mps =
        next_speed;

    vehicle->accel_mps2 =
        cmd->accel_mps2;
}

/*
 * Check invalid input handling.
 */
static void test_invalid_inputs(
    const FallbackConfig *config)
{
    FallbackController controller = {0};

    VehicleState vehicle = {
        .speed_mps = 10.0,
        .accel_mps2 = 0.0,
        .distance_m = 0.0,
        .valid = true
    };

    ControlCommand cmd;

    /* Controller not initialized. */
    cmd = fallback_step(
        &controller,
        &vehicle,
        0
    );

    assert(!cmd.valid);

    /* Invalid configuration. */
    FallbackConfig bad_config = *config;

    bad_config.control_period_s = 0.0;

    assert(!fallback_init(
        &controller,
        &bad_config,
        0.0
    ));

    /* Valid initialization. */
    assert(fallback_init(
        &controller,
        config,
        0.0
    ));

    /* Invalid vehicle data. */
    vehicle.valid = false;

    cmd = fallback_step(
        &controller,
        &vehicle,
        0
    );

    assert(!cmd.valid);

    /* NaN speed. */
    vehicle.valid = true;
    vehicle.speed_mps = NAN;

    cmd = fallback_step(
        &controller,
        &vehicle,
        0
    );

    assert(!cmd.valid);

    /* Negative speed. */
    vehicle.speed_mps = -1.0;

    cmd = fallback_step(
        &controller,
        &vehicle,
        0
    );

    assert(!cmd.valid);

    fprintf(
        stderr,
        "PASS: Invalid input handling\n"
    );
}

int main(void)
{
    /*
     * Step 1. Controller configuration.
     */
    FallbackConfig config = {
        .control_period_s = CONTROL_DT,
        .decel_limit_mps2 = 2.0,
        .jerk_limit_mps3 = 2.0,
        .stop_speed_mps = 0.0
    };

    test_invalid_inputs(&config);

    /*
     * Step 2. Initialize simulated vehicle.
     */
    VehicleState vehicle = {
        .speed_mps = 60.0 / 3.6,
        .accel_mps2 = 0.0,
        .distance_m = 0.0,
        .valid = true
    };

    /*
     * Step 3. Initialize fallback controller.
     */
    FallbackController controller = {0};

    if (!fallback_init(
            &controller,
            &config,
            vehicle.accel_mps2)) {

        fprintf(
            stderr,
            "FAIL: Controller initialization\n"
        );

        return EXIT_FAILURE;
    }

    fprintf(
        stderr,
        "PASS: Controller initialization\n"
    );

    bool fallback_started = false;
    bool stopped = false;

    double fallback_start_distance = 0.0;
    double stop_time = 0.0;
    double stop_distance = 0.0;

    /*
     * Step 4. CSV header.
     */
    printf(
        "time_s,speed_kmh,accel_mps2,"
        "distance_m,source,hold_stop\n"
    );

    /*
     * Step 5. Run 20 ms simulation loop.
     */
    for (unsigned int step = 0;
         step <= TOTAL_STEPS;
         step++) {

        double t =
            step * CONTROL_DT;

        uint64_t now_ns =
            (uint64_t)step * STEP_NS;

        /*
         * Normal AI command stub.
         */
        ControlCommand cmd = {
            .accel_mps2 = 0.0,
            .hold_stop = false,
            .valid = true,
            .source = CMD_SOURCE_AI,
            .timestamp_ns = now_ns
        };

        double prev_accel =
            vehicle.accel_mps2;

        /*
         * Trigger fallback at 3 seconds.
         */
        if (step >= TRIGGER_STEP) {

            if (!fallback_started) {

                fallback_started = true;

                fallback_start_distance =
                    vehicle.distance_m;

                fprintf(
                    stderr,
                    "EVENT: Fallback trigger "
                    "t=%.3f s\n",
                    t
                );
            }

            /*
             * Generate fallback command.
             */
            cmd = fallback_step(
                &controller,
                &vehicle,
                now_ns
            );

            /*
             * Verify output validity.
             */
            assert(cmd.valid);

            assert(
                cmd.source ==
                CMD_SOURCE_FALLBACK
            );

            /*
             * Verify acceleration range.
             */
            assert(
                cmd.accel_mps2 >=
                -config.decel_limit_mps2
                - EPSILON
            );

            assert(
                cmd.accel_mps2 <= EPSILON
            );

            /*
             * Verify jerk limitation
             * during the deceleration phase.
             */
            if (!cmd.hold_stop) {

                double actual_delta =
                    fabs(
                        cmd.accel_mps2 -
                        prev_accel
                    );

                double max_delta =
                    config.jerk_limit_mps3 *
                    CONTROL_DT;

                assert(
                    actual_delta <=
                    max_delta + EPSILON
                );
            }
        }

        /*
         * Record first hold-stop command.
         */
        if (cmd.hold_stop && !stopped) {

            stopped = true;

            stop_time = t;

            stop_distance =
                vehicle.distance_m;

            fprintf(
                stderr,
                "EVENT: Hold stop "
                "t=%.3f s\n",
                t
            );
        }

        /*
         * Log current state and command
         * before integrating the next step.
         */
        printf(
            "%.3f,%.6f,%.6f,%.6f,%s,%d\n",
            t,

            vehicle.speed_mps * 3.6,

            cmd.accel_mps2,

            vehicle.distance_m,

            cmd.source == CMD_SOURCE_AI ?
                "AI_STUB" : "FALLBACK",

            cmd.hold_stop ? 1 : 0
        );

        /*
         * Apply control command.
         */
        vehicle_apply_command(
            &vehicle,
            &cmd
        );

        /*
         * Verify vehicle state.
         */
        assert(vehicle.speed_mps >= 0.0);

        if (stopped)
            assert(vehicle.speed_mps == 0.0);
    }

    /*
     * Step 6. Final verification.
     */
    assert(fallback_started);

    assert(stopped);

    assert(vehicle.speed_mps == 0.0);

    fprintf(
        stderr,
        "RESULT: Stop time = %.3f s\n",
        stop_time
    );

    fprintf(
        stderr,
        "RESULT: Fallback reaction "
        "to stop = %.3f s\n",
        stop_time -
        TRIGGER_STEP * CONTROL_DT
    );

    fprintf(
        stderr,
        "RESULT: Fallback distance "
        "= %.3f m\n",
        stop_distance -
        fallback_start_distance
    );

    fprintf(
        stderr,
        "PASS: Fallback deceleration "
        "and hold stop\n"
    );

    return EXIT_SUCCESS;
}
