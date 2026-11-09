#ifndef TEST_PHYSICS_H
#define TEST_PHYSICS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 position;
    Vec3 velocity;
    float mass;
    float radius;
    float drag_coefficient;
    float restitution;
    float friction;
} BallSimulationState;

// Initializes a standard cricket ball simulation state
void cricket_ball_init(BallSimulationState* ball, float release_speed_kmh, float pitch_angle_deg);

// Steps the simulation forward by delta_time (seconds) using RK4
void cricket_ball_step(BallSimulationState* ball, float dt);

// Runs a self-contained ballistic validation test and verifies pitch bounce on ARM64
bool cricket_physics_run_validation_test(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_PHYSICS_H
