#pragma once

#include "../../External/HandmadeMath.h"
#include <cstdint>

// Basic transform
struct Transform {
    hmm_vec3 position{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;   // rotation around Y (in degrees)
    float pitch = 0.0f; // rotation around X (in degrees)
    float roll = 0.0f; // rotation around Z (in degrees)
    hmm_mat4 ModelMatrix() const {
        hmm_mat4 t = HMM_Translate(position);
        // HMM_Rotate expects angles in DEGREES, not radians!
        hmm_mat4 rx = HMM_Rotate(pitch, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 ry = HMM_Rotate(yaw, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 rz = HMM_Rotate(roll, HMM_Vec3(0.0f, 0.0f, 1.0f));
        
        // Build rotation matrix step by step
        hmm_mat4 rot = HMM_MultiplyMat4(HMM_MultiplyMat4(rx, ry), rz); // Rx * Ry * Rz
        
        // Apply translation last
        return HMM_MultiplyMat4(t, rot);
    }
};

// Simple rigidbody (kinematic support)
struct Rigidbody {
    hmm_vec3 velocity{0.0f, 0.0f, 0.0f};
    float mass = 1.0f;
    bool affectedByGravity = true;
};

// Very small AI controller state
enum class AIState : uint8_t {
    Idle = 0,
    Wander,
    Chase
};

struct AIController {
    AIState state = AIState::Idle;
    float stateTimer = 0.0f;
    // wander target or other per-AI data
    hmm_vec3 wanderTarget{0.0f,0.0f,0.0f};
};

// Animation stub (time and current clip id)
struct Animator {
    int currentClip = -1;
    float time = 0.0f;
};