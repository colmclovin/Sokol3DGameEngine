#pragma once

#include "../../External/HandmadeMath.h"
#include <cstdint>
#include <vector>

// Forward declare EntityId (defined in ECS.h)
using EntityId = int;

// ============================================================================
// TRANSFORM COMPONENT
// ============================================================================
struct Transform {
    hmm_vec3 position{0.0f, 0.0f, 0.0f};
    hmm_vec3 scale{1.0f, 1.0f, 1.0f};
    hmm_vec3 originOffset{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    
    bool useCustomMatrix = false;
    hmm_mat4 customMatrix = HMM_Mat4d(1.0f);
    
    hmm_mat4 ModelMatrix() const {
        if (useCustomMatrix) {
            return customMatrix;
        }
        
        hmm_mat4 originTranslation = HMM_Translate(originOffset);
        hmm_mat4 s = HMM_Scale(scale);
        
        hmm_mat4 rx = HMM_Rotate(pitch, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 ry = HMM_Rotate(yaw, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 rz = HMM_Rotate(roll, HMM_Vec3(0.0f, 0.0f, 1.0f));
        
        hmm_mat4 rot = HMM_MultiplyMat4(HMM_MultiplyMat4(rx, ry), rz);
        
        hmm_mat4 t = HMM_Translate(position);
        hmm_mat4 temp = HMM_MultiplyMat4(s, originTranslation);
        temp = HMM_MultiplyMat4(rot, temp);
        return HMM_MultiplyMat4(t, temp);
    }
    
    hmm_vec3 GetWorldPosition() const {
        hmm_mat4 rx = HMM_Rotate(pitch, HMM_Vec3(1.0f, 0.0f, 0.0f));
        hmm_mat4 ry = HMM_Rotate(yaw, HMM_Vec3(0.0f, 1.0f, 0.0f));
        hmm_mat4 rz = HMM_Rotate(roll, HMM_Vec3(0.0f, 0.0f, 1.0f));
        hmm_mat4 rot = HMM_MultiplyMat4(HMM_MultiplyMat4(rx, ry), rz);
        
        hmm_vec4 offset4 = HMM_Vec4(originOffset.X, originOffset.Y, originOffset.Z, 0.0f);
        hmm_vec4 rotatedOffset4 = HMM_MultiplyMat4ByVec4(rot, offset4);
        hmm_vec3 rotatedOffset = HMM_Vec3(rotatedOffset4.X, rotatedOffset4.Y, rotatedOffset4.Z);
        
        return HMM_AddVec3(position, rotatedOffset);
    }
};

// ============================================================================
// COLLISION SYSTEM
// ============================================================================

enum class ColliderType : uint8_t {
    Sphere = 0,
    Box,
    Capsule,
    Mesh,        // Complex geometry (terrain, buildings)
    Plane        // Infinite plane (simple ground)
};

// Triangle for mesh collider
struct CollisionTriangle {
    hmm_vec3 v0, v1, v2;  // Vertices in local space
    hmm_vec3 normal;       // Pre-calculated normal
};

struct Collider {
    ColliderType type = ColliderType::Sphere;
    
    // === Primitive shapes ===
    float radius = 1.0f;
    hmm_vec3 boxHalfExtents{1.0f, 1.0f, 1.0f};
    float capsuleHeight = 2.0f;
    float capsuleRadius = 0.5f;
    
    // === Mesh collider ===
    std::vector<CollisionTriangle> triangles;
    hmm_vec3 meshBoundsMin{0.0f, 0.0f, 0.0f};
    hmm_vec3 meshBoundsMax{0.0f, 0.0f, 0.0f};
    
    // === Plane collider ===
    hmm_vec3 planeNormal{0.0f, 1.0f, 0.0f};
    float planeDistance = 0.0f;
    
    // === Collision properties ===
    bool isTrigger = false;
    bool isStatic = false;
    uint32_t collisionMask = 0xFFFFFFFF;
    uint32_t collisionLayer = 0x00000001;
    bool useBroadPhase = true;
};

struct Rigidbody {
    hmm_vec3 velocity{0.0f, 0.0f, 0.0f};
    float mass = 1.0f;
    bool affectedByGravity = true;
    bool isKinematic = false;
    float drag = 0.1f;
    float bounciness = 0.0f;
};

// ============================================================================
// SELECTION SYSTEM
// ============================================================================

enum class SelectionVolumeType : uint8_t {
    Sphere = 0,
    Box,
    Mesh,
    Icon
};

struct Selectable {
    bool isSelected = false;
    const char* name = "Entity";
    
    // === Selection volume ===
    SelectionVolumeType volumeType = SelectionVolumeType::Sphere;
    
    // Sphere volume
    float boundingSphereRadius = 1.0f;
    float boundingRadius = 1.0f;  // KEPT: Backward compatibility alias
    
    // Box volume
    hmm_vec3 boundingBoxMin{-1.0f, -1.0f, -1.0f};
    hmm_vec3 boundingBoxMax{1.0f, 1.0f, 1.0f};
    
    // Mesh volume
    bool useMeshColliderForPicking = false;
    
    // Icon picking
    hmm_vec2 iconScreenSize{32.0f, 32.0f};
    
    // === Editor properties ===
    bool showWireframe = true;
    bool canBeSelected = true;
    int editorLayer = 0;
};

// ============================================================================
// AI, ANIMATION, BILLBOARD, SCREEN SPACE
// ============================================================================

enum class AIState : uint8_t {
    Idle = 0,
    Wander,
    Chase
};

struct AIController {
    AIState state = AIState::Idle;
    float stateTimer = 0.0f;
    hmm_vec3 wanderTarget{0.0f, 0.0f, 0.0f};
};

struct Animator {
    int currentClip = -1;
    float time = 0.0f;
};

struct Billboard {
    EntityId followTarget = -1;
    hmm_vec3 offset{0.0f, 0.0f, 0.0f};
    bool lockY = true;
};

struct ScreenSpace {
    hmm_vec2 screenPosition{0.0f, 0.0f};
    hmm_vec2 size{0.1f, 0.1f};
    float depth = 0.0f;
};

// ============================================================================
// GHOST PREVIEW
// ============================================================================

struct GhostPreview {
    bool isGhost = true;
    float opacity = 0.5f;
    bool snapToSurface = true;
    bool snapToGrid = false;
    float gridSize = 1.0f;
    bool alignToSurfaceNormal = false;
    hmm_vec3 surfaceNormal{0.0f, 1.0f, 0.0f};
};