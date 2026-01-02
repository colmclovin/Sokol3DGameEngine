#include "Player.h"
#include <cmath>
#include <stdio.h>

// Helper function to lerp angles (handles wrapping at -PI/+PI)
static inline float LerpAngle(float from, float to, float t) {
    const float PI = 3.14159265359f;
    float delta = fmodf(to - from, 2.0f * PI);
    if (delta > PI) delta -= 2.0f * PI;
    if (delta < -PI) delta += 2.0f * PI;
    return from + delta * t;
}

PlayerController::PlayerController(ECS& ecs, Renderer& renderer, int meshId, GameStateManager& gameState)
    : ecs_(ecs), renderer_(renderer), meshId_(meshId), gameState_(gameState)
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
    
    // FIXED: Initialize model yaw with inverted camera yaw
    modelYaw_ = -camera_.GetYaw(); // Added negative sign
    
    printf("Player spawned: camYaw=%.2f, modelYaw=%.2f\n", camera_.GetYaw(), modelYaw_);
}

void PlayerController::HandleEvent(const sapp_event* ev) {
    if (!ev || !inputEnabled_) return; // Skip input processing if disabled
    
    switch (ev->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (ev->key_code == SAPP_KEYCODE_W) forward_ = true;
            if (ev->key_code == SAPP_KEYCODE_S) back_ = true;
            if (ev->key_code == SAPP_KEYCODE_A) left_ = true;
            if (ev->key_code == SAPP_KEYCODE_D) right_ = true;
            
            // In Edit mode: Shift = down, in Playing mode: Shift = sprint
            if (ev->key_code == SAPP_KEYCODE_LEFT_SHIFT) {
                if (gameState_.IsEdit()) {
                    down_ = true;
                } else {
                    sprint_ = true;
                }
            }
            
            // Spacebar = up in Edit mode
            if (ev->key_code == SAPP_KEYCODE_SPACE && gameState_.IsEdit()) {
                up_ = true;
            }
            
            if (ev->key_code == SAPP_KEYCODE_ESCAPE) {
                // Toggle mouse lock/unlock with ESC key
                static bool mouse_locked = true;
                mouse_locked = !mouse_locked;
                sapp_lock_mouse(mouse_locked);
                sapp_show_mouse(!mouse_locked);
            }
            // Toggle between Playing and Edit mode with TAB key
            if (ev->key_code == SAPP_KEYCODE_TAB) {
                gameState_.ToggleMode();
                
                if (gameState_.IsEdit()) {
                    // Entering Edit mode - calculate yaw/pitch from camera looking at player
                    Transform* t = ecs_.GetTransform(entityId_);
                    if (t) {
                        // Get the current third-person camera position
                        hmm_vec3 camPos = camera_.GetPosition(t->position);
                        
                        // Get the look-at target (where the camera is looking in Playing mode)
                        hmm_vec3 lookTarget = HMM_Vec3(
                            t->position.X,
                            t->position.Y + camera_.lookAtOffset_,
                            t->position.Z
                        );
                        
                        // Calculate the direction vector from camera to target
                        hmm_vec3 direction = HMM_SubtractVec3(lookTarget, camPos);
                        float length = sqrtf(direction.X * direction.X + direction.Y * direction.Y + direction.Z * direction.Z);
                        
                        if (length > 0.001f) {
                            // Normalize
                            direction.X /= length;
                            direction.Y /= length;
                            direction.Z /= length;
                            
                            // Calculate yaw and pitch from the direction vector
                            // yaw: horizontal angle (rotation around Y axis)
                            // pitch: vertical angle (up/down)
                            float newYaw = atan2f(-direction.X, direction.Z);
                            float newPitch = asinf(direction.Y);
                            
                            // Update camera rotation to match the look-at direction
                            camera_.SetYawPitch(newYaw, newPitch);
                        }
                        
                        // Set the free camera position
                        camera_.SetFreeCameraPosition(camPos);
                        
                        printf("=== EDIT MODE === Camera at (%.2f, %.2f, %.2f) yaw=%.2f pitch=%.2f\n", 
                               camPos.X, camPos.Y, camPos.Z,
                               camera_.GetYaw(), camera_.GetPitch());
                    }
                } else {
                    // Returning to Playing mode
                    printf("=== PLAYING MODE ===\n");
                }
            }
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            if (ev->key_code == SAPP_KEYCODE_W) forward_ = false;
            if (ev->key_code == SAPP_KEYCODE_S) back_ = false;
            if (ev->key_code == SAPP_KEYCODE_A) left_ = false;
            if (ev->key_code == SAPP_KEYCODE_D) right_ = false;
            
            // Handle shift key up for both modes
            if (ev->key_code == SAPP_KEYCODE_LEFT_SHIFT) {
                sprint_ = false;
                down_ = false;
            }
            
            // Handle spacebar up
            if (ev->key_code == SAPP_KEYCODE_SPACE) {
                up_ = false;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            {
                // Mouse movement always controls camera rotation
                float dx = (float)ev->mouse_dx;
                float dy = (float)ev->mouse_dy;
                camera_.ProcessMouseMovement(dx, dy);
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            // Scroll only affects distance in Playing mode
            if (gameState_.IsPlaying()) {
                camera_.ProcessMouseScroll((float)ev->scroll_y);
            }
            break;
        default: break;
    }
}

void PlayerController::Update(float dt) {
    if (!inputEnabled_) return; // Skip update if input disabled
    
    Transform* t = ecs_.GetTransform(entityId_);
    if (!t) {
        printf("ERROR: No transform found for player entity!\n");
        return;
    }
    
    if (gameState_.IsPlaying()) {
        // PLAYING MODE: Move player, camera follows
        hmm_vec3 forward_dir = camera_.GetForwardDirection();
        hmm_vec3 right_dir = camera_.GetRightDirection();
        float speed = moveSpeed_ * (sprint_ ? 2.0f : 1.0f);
        hmm_vec3 delta = HMM_Vec3(0.0f, 0.0f, 0.0f);
        
        // FIXED: Corrected direction signs for proper left/right movement
        if (forward_)  delta = HMM_AddVec3(delta, HMM_Vec3(forward_dir.X * speed * dt, forward_dir.Y * speed * dt, forward_dir.Z * speed * dt));
        if (back_)     delta = HMM_AddVec3(delta, HMM_Vec3(-forward_dir.X * speed * dt, -forward_dir.Y * speed * dt, -forward_dir.Z * speed * dt));
        if (left_)     delta = HMM_AddVec3(delta, HMM_Vec3(-right_dir.X * speed * dt, -right_dir.Y * speed * dt, -right_dir.Z * speed * dt)); // FIXED: Changed + to -
        if (right_)    delta = HMM_AddVec3(delta, HMM_Vec3(right_dir.X * speed * dt, right_dir.Y * speed * dt, right_dir.Z * speed * dt));     // FIXED: Changed - to +
        
        t->position = HMM_AddVec3(t->position, delta);

        // FIXED: Invert camera yaw for player rotation to face the correct direction
        float targetYaw = -camera_.GetYaw(); // Added negative sign
        modelYaw_ = LerpAngle(modelYaw_, targetYaw, rotationSpeed_ * dt);
        
        // Convert radians to degrees for the transform (HMM_Rotate expects degrees)
        t->yaw = modelYaw_ * (180.0f / 3.14159265359f);
    } else {
        // EDIT MODE: Move camera freely, player stays still
        camera_.UpdateFreeCamera(dt, forward_, back_, left_, right_, up_, down_, sprint_);
    }
}

hmm_vec3 PlayerController::CameraPosition() const {
    if (gameState_.IsEdit()) {
        return camera_.GetFreeCameraPosition();
    }
    
    Transform* t = ecs_.GetTransform(entityId_);
    hmm_vec3 player_pos = t ? t->position : HMM_Vec3(0, 0, 0);
    return camera_.GetPosition(player_pos);
}

hmm_mat4 PlayerController::GetViewMatrix() const {
    if (gameState_.IsEdit()) {
        return camera_.GetFreeCameraViewMatrix();
    }
    
    Transform* t = ecs_.GetTransform(entityId_);
    hmm_vec3 player_pos = t ? t->position : HMM_Vec3(0, 0, 0);
    return camera_.GetViewMatrix(player_pos);
}