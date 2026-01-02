#pragma once

#include "../Game/ECS.h"
#include "../Game/Camera.h"
#include "../Game/GameState.h"
#include "../../External/HandmadeMath.h"
#include "../../../External/Sokol/sokol_app.h"
#include <cstdint>

// Player controller implemented as an ECS-driven actor.
// Creates an entity in ECS and manipulates its transform/rigidbody.

class PlayerController {
public:
    PlayerController(ECS& ecs, Renderer& renderer, int meshId, GameStateManager& gameState);
    ~PlayerController();

    // spawn player at position
    void Spawn(const hmm_vec3& pos);

    // input events
    void HandleEvent(const sapp_event* ev);

    // per-frame update
    void Update(float dt);

    // camera helpers
    hmm_mat4 GetViewMatrix() const;
    hmm_vec3 CameraPosition() const;

    EntityId Entity() const { return entityId_; }

    // Access to camera for ImGui controls
    Camera& GetCamera() { return camera_; }
    
    // Enable/disable input processing (used when GUI is open)
    void SetInputEnabled(bool enabled) { inputEnabled_ = enabled; }

private:
    ECS& ecs_;
    Renderer& renderer_;
    GameStateManager& gameState_;
    int meshId_;
    EntityId entityId_ = -1;

    // Camera system
    Camera camera_;

    // model rotation (lerped)
    float modelYaw_ = 0.0f;

    // input state
    bool forward_ = false, back_ = false, left_ = false, right_ = false, sprint_ = false;
    bool up_ = false, down_ = false; // For Edit mode vertical movement
    bool inputEnabled_ = true; // Input processing enabled/disabled

    float moveSpeed_ = 6.0f;
    float rotationSpeed_ = 5.0f; // How fast the model rotates to match camera
};