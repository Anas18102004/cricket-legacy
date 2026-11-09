#include "test_physics.h"
#include <math.h>
#include <android/log.h>

#define LOG_TAG "CricketPhysics"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const float GRAVITY = 9.80665f;
static const float AIR_DENSITY = 1.225f;

void cricket_ball_init(BallSimulationState* ball, float release_speed_kmh, float pitch_angle_deg) {
    if (!ball) return;

    float speed_ms = release_speed_kmh * (1000.0f / 3600.0f);
    float angle_rad = pitch_angle_deg * (M_PI / 180.0f);

    // Standard cricket pitch geometry: bowler release at z = 20.12m, y = 2.1m (over arm release)
    ball->position.x = 0.0f;
    ball->position.y = 2.1f;
    ball->position.z = 20.12f;

    ball->velocity.x = 0.0f;
    ball->velocity.y = -sinf(angle_rad) * speed_ms;
    ball->velocity.z = -cosf(angle_rad) * speed_ms;

    ball->mass = 0.160f; // 160 grams
    ball->radius = 0.036f; // 36mm radius
    ball->drag_coefficient = 0.35f;
    ball->restitution = 0.58f; // Hard pitch restitution
    ball->friction = 0.28f;
}

static Vec3 compute_acceleration(const BallSimulationState* ball, Vec3 vel) {
    Vec3 acc;
    float v_mag = sqrtf(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
    float area = M_PI * ball->radius * ball->radius;
    
    // F_drag = 0.5 * rho * v^2 * Cd * A
    float drag_mag = 0.5f * AIR_DENSITY * v_mag * v_mag * ball->drag_coefficient * area;
    float drag_acc = (v_mag > 0.0001f) ? (drag_mag / ball->mass) / v_mag : 0.0f;

    acc.x = -vel.x * drag_acc;
    acc.y = -GRAVITY - (vel.y * drag_acc);
    acc.z = -vel.z * drag_acc;

    return acc;
}

void cricket_ball_step(BallSimulationState* ball, float dt) {
    if (!ball) return;

    // Numerical integration (Euler / Verlet substep)
    Vec3 a = compute_acceleration(ball, ball->velocity);

    ball->position.x += ball->velocity.x * dt + 0.5f * a.x * dt * dt;
    ball->position.y += ball->velocity.y * dt + 0.5f * a.y * dt * dt;
    ball->position.z += ball->velocity.z * dt + 0.5f * a.z * dt * dt;

    ball->velocity.x += a.x * dt;
    ball->velocity.y += a.y * dt;
    ball->velocity.z += a.z * dt;

    // Pitch collision at y = 0.036m (ball radius contacting surface)
    if (ball->position.y <= ball->radius && ball->velocity.y < 0.0f) {
        ball->position.y = ball->radius;
        ball->velocity.y = -ball->velocity.y * ball->restitution;
        ball->velocity.x *= (1.0f - ball->friction);
        ball->velocity.z *= (1.0f - ball->friction);
        LOGI("Pitch impact registered! New Vy: %.2f m/s, Vz: %.2f m/s", ball->velocity.y, ball->velocity.z);
    }
}

bool cricket_physics_run_validation_test(void) {
    BallSimulationState ball;
    cricket_ball_init(&ball, 140.0f, 4.5f); // 140 km/h delivery at 4.5 deg downward tilt

    LOGI("Starting ARM64 Ballistic Validation Test: 140 km/h release...");
    float dt = 1.0f / 120.0f; // 120Hz physics rate
    bool bounce_occurred = false;

    for (int step = 0; step < 120; ++step) {
        float prev_y = ball.position.y;
        cricket_ball_step(&ball, dt);

        if (prev_y <= ball.radius + 0.001f && ball.velocity.y > 0.0f) {
            bounce_occurred = true;
        }

        if (ball.position.z <= 0.0f) {
            LOGI("Ball reached batsman crease at step %d: Pos=(%.2f, %.2f, %.2f), Speed=%.1f km/h",
                 step, ball.position.x, ball.position.y, ball.position.z,
                 sqrtf(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y + ball.velocity.z * ball.velocity.z) * 3.6f);
            break;
        }
    }

    if (!bounce_occurred) {
        LOGE("Physics validation FAILED: Ball reached crease without pitch bounce!");
        return false;
    }

    LOGI("ARM64 Ballistic Validation Test PASSED successfully!");
    return true;
}
