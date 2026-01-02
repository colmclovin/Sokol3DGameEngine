#include "Camera.h"
#include "../../External/Imgui/imgui.h"
#include <cmath>

static inline float clampf(float v, float a, float b) { 
    return (v < a) ? a : (v > b ? b : v); 
}

Camera::Camera() {
}

void Camera::ProcessMouseMovement(float dx, float dy) {
    // FIXED: Use normal direction (removed inversion)
    yaw_ += dx * sensitivity_;  // Changed back from -= to +=
    pitch_ += -dy * sensitivity_;
    
    if (clampPitch_) {
        pitch_ = clampf(pitch_, minPitch_, maxPitch_);
    }
}

void Camera::ProcessMouseScroll(float scroll_delta) {
    distance_ = clampf(distance_ - scroll_delta * scrollSensitivity_, minDistance_, maxDistance_);
}

void Camera::UpdateFreeCamera(float dt, bool forward, bool back, bool left, bool right, bool up, bool down, bool sprint) {
    float speed = freeCameraSpeed_ * (sprint ? 2.0f : 1.0f);
    
    // Calculate movement directions based on camera orientation
    hmm_vec3 forward_dir = HMM_Vec3(-sinf(yaw_) * cosf(pitch_), sinf(pitch_), cosf(yaw_) * cosf(pitch_));
    hmm_vec3 right_dir = HMM_Vec3(cosf(yaw_), 0.0f, sinf(yaw_));
    hmm_vec3 up_dir = HMM_Vec3(0.0f, 1.0f, 0.0f); // World up vector
    
    hmm_vec3 movement = HMM_Vec3(0.0f, 0.0f, 0.0f);
    
    if (forward) {
        movement = HMM_AddVec3(movement, HMM_Vec3(
            forward_dir.X * speed * dt,
            forward_dir.Y * speed * dt,
            forward_dir.Z * speed * dt
        ));
    }
    if (back) {
        movement = HMM_AddVec3(movement, HMM_Vec3(
            -forward_dir.X * speed * dt,
            -forward_dir.Y * speed * dt,
            -forward_dir.Z * speed * dt
        ));
    }
    if (left) {
        // Fixed: Changed from negative to positive to correct left/right inversion
        movement = HMM_AddVec3(movement, HMM_Vec3(
            right_dir.X * speed * dt,
            right_dir.Y * speed * dt,
            right_dir.Z * speed * dt
        ));
    }
    if (right) {
        // Fixed: Changed from positive to negative to correct left/right inversion
        movement = HMM_AddVec3(movement, HMM_Vec3(
            -right_dir.X * speed * dt,
            -right_dir.Y * speed * dt,
            -right_dir.Z * speed * dt
        ));
    }
    if (up) {
        // Move straight up in world space
        movement = HMM_AddVec3(movement, HMM_Vec3(
            up_dir.X * speed * dt,
            up_dir.Y * speed * dt,
            up_dir.Z * speed * dt
        ));
    }
    if (down) {
        // Move straight down in world space
        movement = HMM_AddVec3(movement, HMM_Vec3(
            -up_dir.X * speed * dt,
            -up_dir.Y * speed * dt,
            -up_dir.Z * speed * dt
        ));
    }
    
    freeCameraPosition_ = HMM_AddVec3(freeCameraPosition_, movement);
}

hmm_vec3 Camera::GetPosition(const hmm_vec3& target_position) const {
    // Position camera BEHIND the target (offset by yaw and pitch)
    // This creates a third-person camera that orbits around the target
    float offsetX = distance_ * sinf(yaw_) * cosf(pitch_);
    float offsetY = distance_ * sinf(pitch_) + heightOffset_;
    float offsetZ = distance_ * -cosf(yaw_) * cosf(pitch_);
    
    return HMM_Vec3(
        target_position.X + offsetX, 
        target_position.Y + offsetY, 
        target_position.Z + offsetZ
    );
}

hmm_mat4 Camera::GetViewMatrix(const hmm_vec3& target_position) const {
    // Camera looks at the target with offset
    hmm_vec3 look_target = HMM_Vec3(
        target_position.X,
        target_position.Y + lookAtOffset_,
        target_position.Z
    );
    
    hmm_vec3 cam_pos = GetPosition(target_position);
    return HMM_LookAt(cam_pos, look_target, HMM_Vec3(0.0f, 1.0f, 0.0f));
}

hmm_mat4 Camera::GetFreeCameraViewMatrix() const {
    // Calculate look-at target based on camera rotation
    hmm_vec3 forward = HMM_Vec3(
        -sinf(yaw_) * cosf(pitch_),
        sinf(pitch_),
        cosf(yaw_) * cosf(pitch_)
    );
    
    hmm_vec3 look_target = HMM_AddVec3(freeCameraPosition_, forward);
    
    return HMM_LookAt(freeCameraPosition_, look_target, HMM_Vec3(0.0f, 1.0f, 0.0f));
}

hmm_vec3 Camera::GetForwardDirection() const {
    return HMM_Vec3(-sinf(yaw_), 0.0f, cosf(yaw_));
}

hmm_vec3 Camera::GetRightDirection() const {
    // FIXED: Negate to match inverted yaw direction
    return HMM_Vec3(-cosf(yaw_), 0.0f, -sinf(yaw_)); // Added negative signs
}

hmm_vec3 Camera::GetForward3D() const {
    // Returns the full 3D forward direction including pitch (for raycasting)
    return HMM_Vec3(
        -sinf(yaw_) * cosf(pitch_),
        sinf(pitch_),
        cosf(yaw_) * cosf(pitch_)
    );
}

void Camera::RenderImGuiControls() {
    if (ImGui::CollapsingHeader("Camera Settings")) {
        ImGui::SliderFloat("Mouse Sensitivity", &sensitivity_, 0.001f, 0.1f, "%.3f");
        ImGui::SliderFloat("Scroll Sensitivity", &scrollSensitivity_, 0.1f, 2.0f, "%.1f");
        
        ImGui::Separator();
        ImGui::Text("Distance & Zoom");
        ImGui::SliderFloat("Distance", &distance_, minDistance_, maxDistance_, "%.1f");
        ImGui::SliderFloat("Min Distance", &minDistance_, 1.0f, 50.0f, "%.1f");
        ImGui::SliderFloat("Max Distance", &maxDistance_, 10.0f, 500.0f, "%.1f");
        
        ImGui::Separator();
        ImGui::Text("Position & Look-At");
        ImGui::SliderFloat("Height Offset", &heightOffset_, -5.0f, 10.0f, "%.1f");
        ImGui::SliderFloat("Look-At Offset (Y)", &lookAtOffset_, -2.0f, 5.0f, "%.1f");
        ImGui::SliderFloat("Free Camera Speed", &freeCameraSpeed_, 1.0f, 50.0f, "%.1f");
        
        ImGui::Separator();
        ImGui::Text("Rotation");
        ImGui::Text("Yaw: %.2f  Pitch: %.2f", yaw_, pitch_);
        ImGui::Checkbox("Clamp Pitch", &clampPitch_);
        if (clampPitch_) {
            ImGui::SliderFloat("Min Pitch", &minPitch_, -3.14f, 0.0f, "%.2f");
            ImGui::SliderFloat("Max Pitch", &maxPitch_, 0.0f, 3.14f, "%.2f");
        }
        
        ImGui::Separator();
        ImGui::Text("Free Camera Position");
        ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f", 
                    freeCameraPosition_.X, freeCameraPosition_.Y, freeCameraPosition_.Z);
        
        if (ImGui::Button("Reset Camera")) {
            yaw_ = 0.0f;
            pitch_ = 0.2f;
            distance_ = 6.0f;
            sensitivity_ = 0.01f;
            scrollSensitivity_ = 0.5f;
            heightOffset_ = 1.5f;
            lookAtOffset_ = 0.5f;
            freeCameraSpeed_ = 10.0f;
        }
    }
}