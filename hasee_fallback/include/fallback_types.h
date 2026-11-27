
#ifndef FALLBACK_TYPES_H
#define FALLBACK_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Control command ownership.
 */
typedef enum {
    CMD_SOURCE_NONE = 0,
    CMD_SOURCE_AI,
    CMD_SOURCE_FALLBACK
} CommandSource;

/*
 * Independent vehicle state.
 * Initially supplied by the vehicle simulator.
 */
typedef struct {
    double speed_mps;
    double accel_mps2;
    double distance_m;

    bool valid;

} VehicleState;

/*
 * Command sent to the actuator interface.
 */
typedef struct {
    double accel_mps2;

    bool hold_stop;
    bool valid;

    CommandSource source;

    uint64_t timestamp_ns;

} ControlCommand;

/*
 * Fallback controller configuration.
 */
typedef struct {
    double control_period_s;

    double decel_limit_mps2;

    double jerk_limit_mps3;

    double stop_speed_mps;

} FallbackConfig;

#endif
