#pragma once

#include "../../External/HandmadeMath.h"
#include <cstdint>

// Forward declare EntityId (defined in ECS.h)
using EntityId = int;

// Basic transform
struct Transform {
    hmm_vec3 position{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;   // rotation around Y (in degrees)
    float pitch = 0.0f; // rotation around X (in degrees)
    float roll = 0.0f; // rotation around Z (in degrees)
    
    // Optional: Custom matrix for billboards (bypasses Euler angles)
    bool useCustomMatrix = false;
    hmm_mat4 customMatrix = HMM_Mat4d(1.0f);
    
    hmm_mat4 ModelMatrix() const {
        if (useCustomMatrix) {
            // Use custom matrix directly (for billboards)
            return customMatrix;
        }
        
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

// Billboard component - makes entity always face the camera
struct Billboard {
    EntityId followTarget = -1; // Entity to follow (e.g., player)
    hmm_vec3 offset{0.0f, 0.0f, 0.0f}; // Offset from target position
    bool lockY = true; // Only rotate around Y axis (cylindrical billboard)
};

// Screen-space component for HUD elements
// Position is in normalized screen coordinates: (0,0) = top-left, (1,1) = bottom-right
struct ScreenSpace {
    hmm_vec2 screenPosition{0.0f, 0.0f}; // Position in screen space (0-1 range)
    hmm_vec2 size{0.1f, 0.1f}; // Size in screen space (0-1 range)
    float depth = 0.0f; // Depth for layering (0 = front, 1 = back)
};

// NOTE: Light struct is defined in include/Model.h - use that instead of redefining here

// Selectable component - marks entity as selectable in edit mode
struct Selectable {
    bool isSelected = false;
    float boundingRadius = 1.0f; // Radius for picking sphere
    const char* name = "Entity"; // Display name in inspector
};