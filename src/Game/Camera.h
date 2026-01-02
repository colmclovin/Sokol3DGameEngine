#pragma once

#include "../../External/HandmadeMath.h"
#include "../Game/ECS.h"

class Camera {
public:
    Camera();
    ~Camera() = default;

    // Update camera rotation based on mouse input
    void ProcessMouseMovement(float dx, float dy);
    
    // Update camera distance based on scroll input
    void ProcessMouseScroll(float scroll_delta);
    
    // Update free camera position (Edit mode)
    void UpdateFreeCamera(float dt, bool forward, bool back, bool left, bool right, bool up, bool down, bool sprint);
    
    // Get the view matrix for rendering
    hmm_mat4 GetViewMatrix(const hmm_vec3& target_position) const;
    
    // Get the view matrix for free camera (Edit mode)
    hmm_mat4 GetFreeCameraViewMatrix() const;
    
    // Get the camera world position
    hmm_vec3 GetPosition(const hmm_vec3& target_position) const;
    
    // Get free camera position (Edit mode)
    hmm_vec3 GetFreeCameraPosition() const { return freeCameraPosition_; }
    
    // Get camera forward direction (2D, for player movement on ground)
    hmm_vec3 GetForwardDirection() const;
    
    // Get camera forward direction (3D, includes pitch - for raycasting)
    hmm_vec3 GetForward3D() const;
    
    // Get camera right direction (for player movement)
    hmm_vec3 GetRightDirection() const;
    
    // Get current yaw/pitch
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }
    
    // Set free camera position (used when entering Edit mode)
    void SetFreeCameraPosition(const hmm_vec3& position) { freeCameraPosition_ = position; }
    
    // Set yaw and pitch (used when entering Edit mode to match look-at direction)
    void SetYawPitch(float yaw, float pitch) { yaw_ = yaw; pitch_ = pitch; }
    
    // Render ImGui controls for camera parameters
    void RenderImGuiControls();

    // Camera configuration (public for direct access if needed)
    float sensitivity_ = 0.01f;      // Mouse look sensitivity
    float scrollSensitivity_ = 0.5f; // Mouse scroll sensitivity
    float distance_ = 6.0f;          // Distance from target
    float minDistance_ = 2.0f;       // Minimum zoom distance
    float maxDistance_ = 200.0f;     // Maximum zoom distance
    float heightOffset_ = 1.5f;      // Height offset above target
    float lookAtOffset_ = 0.5f;      // Look-at target offset (Y-axis)
    float minPitch_ = -1.5f;         // Minimum pitch angle
    float maxPitch_ = 1.5f;          // Maximum pitch angle
    bool clampPitch_ = false;        // Enable/disable pitch clamping
    float freeCameraSpeed_ = 10.0f;  // Free camera movement speed

private:
    float yaw_ = 0.0f;
    float pitch_ = 0.2f;
    
    // Free camera position (used in Edit mode)
    hmm_vec3 freeCameraPosition_ = HMM_Vec3(0.0f, 2.0f, 10.0f);
};