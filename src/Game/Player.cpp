#include "Player.h"
#include <cmath>
#include <stdio.h>

static inline float clampf(float v, float a, float b) { return (v < a) ? a : (v > b ? b : v); }

// Helper function to lerp angles (handles wrapping at -PI/+PI)
static inline float LerpAngle(float from, float to, float t) {
    const float PI = 3.14159265359f;
    float delta = fmodf(to - from, 2.0f * PI);
    if (delta > PI) delta -= 2.0f * PI;
    if (delta < -PI) delta += 2.0f * PI;
    return from + delta * t;
}

PlayerController::PlayerController(ECS& ecs, Renderer& renderer, int meshId)
    : ecs_(ecs), renderer_(renderer), meshId_(meshId)
{
}

PlayerController::~PlayerController() {
    if (entityId_ != -1) {
        // leaving entity alive is fine; add Destroy if desired
    }
}

void PlayerController::Spawn(const hmm_vec3& pos) {
    entityId_ = ecs_.CreateEntity();
    Transform t;
    t.position = pos;
    t.yaw = 0.0f;
    ecs_.AddTransform(entityId_, t);
    Rigidbody rb;
    rb.mass = 1.0f;
    rb.affectedByGravity = true;
    ecs_.AddRigidbody(entityId_, rb);
    // Create render instance
    ecs_.AddRenderable(entityId_, meshId_, renderer_);
    
    // Initialize model yaw to match camera
    modelYaw_ = camYaw_;
    
    printf("Player spawned: camYaw_=%.2f, modelYaw_=%.2f\n", camYaw_, modelYaw_);
}

void PlayerController::HandleEvent(const sapp_event* ev) {
    if (!ev) return;
    switch (ev->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (ev->key_code == SAPP_KEYCODE_W) forward_ = true;
            if (ev->key_code == SAPP_KEYCODE_S) back_ = true;
            if (ev->key_code == SAPP_KEYCODE_A) left_ = true;
            if (ev->key_code == SAPP_KEYCODE_D) right_ = true;
            if (ev->key_code == SAPP_KEYCODE_LEFT_SHIFT) sprint_ = true;
            if (ev->key_code == SAPP_KEYCODE_ESCAPE) {
                // Toggle mouse lock/unlock with ESC key
                static bool mouse_locked = true;
                mouse_locked = !mouse_locked;
                sapp_lock_mouse(mouse_locked);
                sapp_show_mouse(!mouse_locked);
            }
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            if (ev->key_code == SAPP_KEYCODE_W) forward_ = false;
            if (ev->key_code == SAPP_KEYCODE_S) back_ = false;
            if (ev->key_code == SAPP_KEYCODE_A) left_ = false;
            if (ev->key_code == SAPP_KEYCODE_D) right_ = false;
            if (ev->key_code == SAPP_KEYCODE_LEFT_SHIFT) sprint_ = false;
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            {
                // When mouse is locked, use dx/dy directly from the event
                // These are provided by Sokol when the mouse is locked
                float dx = (float)ev->mouse_dx;
                float dy = (float)ev->mouse_dy;
                
                const float sens = 0.01f;
                camYaw_ += dx * sens;
                camPitch_ += -dy * sens;
                camPitch_ = clampf(camPitch_, -1.5f, 1.5f);
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            camDistance_ = clampf(camDistance_ - (float)ev->scroll_y * 0.5f, 2.0f, 200.0f); // Min distance of 2.0
            break;
        default: break;
    }
}

void PlayerController::Update(float dt) {
    Transform* t = ecs_.GetTransform(entityId_);
    if (!t) {
        printf("ERROR: No transform found for player entity!\n");
        return;
    }
    
    // Use camera yaw for movement direction
    hmm_vec3 forward_dir = HMM_Vec3(-sinf(camYaw_), 0.0f, cosf(camYaw_));
    hmm_vec3 right_dir = HMM_Vec3(cosf(camYaw_), 0.0f, sinf(camYaw_));
    float speed = moveSpeed_ * (sprint_ ? 2.0f : 1.0f);
    hmm_vec3 delta = HMM_Vec3(0.0f,0.0f,0.0f);
    if (forward_)  delta = HMM_AddVec3(delta, HMM_Vec3(forward_dir.X * speed * dt, forward_dir.Y * speed * dt, forward_dir.Z * speed * dt));
    if (back_)     delta = HMM_AddVec3(delta, HMM_Vec3(-forward_dir.X * speed * dt, -forward_dir.Y * speed * dt, -forward_dir.Z * speed * dt));
    if (left_)     delta = HMM_AddVec3(delta, HMM_Vec3(right_dir.X * speed * dt, right_dir.Y * speed * dt, right_dir.Z * speed * dt));
    if (right_)    delta = HMM_AddVec3(delta, HMM_Vec3(-right_dir.X * speed * dt, -right_dir.Y * speed * dt, -right_dir.Z * speed * dt));
    t->position = HMM_AddVec3(t->position, delta);

    // Smoothly lerp model rotation to match camera yaw
    modelYaw_ = LerpAngle(modelYaw_, camYaw_, rotationSpeed_ * dt);
    
    // Convert radians to degrees for the transform (HMM_Rotate expects degrees)
    t->yaw = modelYaw_ * (180.0f / 3.14159265359f);
    
    // Debug: Print the actual model matrix values
    static int debugCounter = 0;
    if (++debugCounter >= 60) {
        hmm_mat4 mat = t->ModelMatrix();
        printf("Player ModelMatrix row 0: [%.2f, %.2f, %.2f, %.2f] yaw=%.1f\n",
               mat.Elements[0][0], mat.Elements[0][1], mat.Elements[0][2], mat.Elements[0][3],
               t->yaw);
        debugCounter = 0;
    }
}

hmm_vec3 PlayerController::CameraPosition() const {
    Transform* t = ecs_.GetTransform(entityId_);
    hmm_vec3 player_pos = t ? t->position : HMM_Vec3(0,0,0);
    
    // Position camera BEHIND the player (offset by yaw)
    // This creates a third-person camera that orbits around the player
    float offsetX = camDistance_ * sinf(camYaw_);
    float offsetY = camDistance_ * sinf(camPitch_) + 1.5f; // Height relative to player
    float offsetZ = camDistance_ * -cosf(camYaw_) * cosf(camPitch_);
    
    return HMM_Vec3(player_pos.X + offsetX, player_pos.Y + offsetY, player_pos.Z + offsetZ);
}

hmm_mat4 PlayerController::GetViewMatrix() const {
    Transform* t = ecs_.GetTransform(entityId_);
    hmm_vec3 player_pos = t ? t->position : HMM_Vec3(0,0,0);
    
    // Camera looks directly at the player center (not ahead)
    hmm_vec3 look_target = HMM_Vec3(
        player_pos.X,
        player_pos.Y + 0.5f,  // Look at center of player (slightly above feet)
        player_pos.Z
    );
    
    hmm_vec3 cam = CameraPosition();
    return HMM_LookAt(cam, look_target, HMM_Vec3(0.0f,1.0f,0.0f));
}