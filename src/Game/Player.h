#pragma once

#include "../Game/ECS.h"
#include "../../External/HandmadeMath.h"
#include "../../../External/Sokol/sokol_app.h"
#include <cstdint>

// Player controller implemented as an ECS-driven actor.
// Creates an entity in ECS and manipulates its transform/rigidbody.

class PlayerController {
public:
    PlayerController(ECS& ecs, Renderer& renderer, int meshId);
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

private:
    ECS& ecs_;
    Renderer& renderer_;
    int meshId_;
    EntityId entityId_ = -1;

    // local camera params
    float camDistance_ = 6.0f;
    float camPitch_ = 0.2f;
    float camYaw_ = 0.0f;

    // model rotation (lerped)
    float modelYaw_ = 0.0f;

    // input state
    bool forward_ = false, back_ = false, left_ = false, right_ = false, sprint_ = false;
    bool right_mouse_down_ = false;
    float last_mouse_x_ = 0.0f, last_mouse_y_ = 0.0f;

    float moveSpeed_ = 6.0f;
    float rotationSpeed_ = 5.0f; // How fast the model rotates to match camera
};