#include "ECS.h"
#include <cstdlib>
#include <cmath>
#include "../External/HandmadeMath.h"
ECS::ECS() = default;
ECS::~ECS() = default;

EntityId ECS::CreateEntity() {
    EntityId id = nextId_++;
    alive_.push_back(id);
    return id;
}

void ECS::DestroyEntity(EntityId id) {
    transforms_.erase(id);
    rigidbodies_.erase(id);
    ai_controllers_.erase(id);
    animators_.erase(id);
    mesh_for_entity_.erase(id);
    instance_for_entity_.erase(id);
    billboards_.erase(id);
    screen_spaces_.erase(id);
    lights_.erase(id);
    selectables_.erase(id);
    alive_.erase(std::remove(alive_.begin(), alive_.end(), id), alive_.end());
}

void ECS::AddTransform(EntityId id, const Transform& t) { transforms_[id] = t; }
bool ECS::HasTransform(EntityId id) const { return transforms_.find(id) != transforms_.end(); }
Transform* ECS::GetTransform(EntityId id) {
    auto it = transforms_.find(id);
    return (it != transforms_.end()) ? &it->second : nullptr;
}

void ECS::AddRigidbody(EntityId id, const Rigidbody& rb) { rigidbodies_[id] = rb; }
Rigidbody* ECS::GetRigidbody(EntityId id) {
    auto it = rigidbodies_.find(id);
    return (it != rigidbodies_.end()) ? &it->second : nullptr;
}

void ECS::AddAI(EntityId id, const AIController& ai) { ai_controllers_[id] = ai; }
AIController* ECS::GetAI(EntityId id) {
    auto it = ai_controllers_.find(id);
    return (it != ai_controllers_.end()) ? &it->second : nullptr;
}

void ECS::AddAnimator(EntityId id, const Animator& a) { animators_[id] = a; }
Animator* ECS::GetAnimator(EntityId id) {
    auto it = animators_.find(id);
    return (it != animators_.end()) ? &it->second : nullptr;
}

int ECS::AddRenderable(EntityId id, int meshId, Renderer& renderer) {
    if (meshId < 0) return -1;
    mesh_for_entity_[id] = meshId;
    // create instance in renderer and remember instance id
    int instId = renderer.AddInstance(meshId, transforms_.count(id) ? transforms_[id].ModelMatrix() : HMM_Mat4d(1.0f));
    instance_for_entity_[id] = instId;
    return instId;
}

bool ECS::HasRenderable(EntityId id) const {
    return instance_for_entity_.find(id) != instance_for_entity_.end();
}

int ECS::GetInstanceId(EntityId id) const {
    auto it = instance_for_entity_.find(id);
    return (it != instance_for_entity_.end()) ? it->second : -1;
}

int ECS::GetMeshId(EntityId id) const {
    auto it = mesh_for_entity_.find(id);
    return (it != mesh_for_entity_.end()) ? it->second : -1;
}

void ECS::RemoveRenderable(EntityId id, Renderer& renderer) {
    auto it = instance_for_entity_.find(id);
    if (it != instance_for_entity_.end()) {
        // FIXED: Actually remove the instance from the renderer
        int instanceId = it->second;
        renderer.RemoveInstance(instanceId);
        instance_for_entity_.erase(it);
    }
    mesh_for_entity_.erase(id);
}

// New Billboard Methods
void ECS::AddBillboard(EntityId id, const Billboard& b) {
    billboards_[id] = b;
}

Billboard* ECS::GetBillboard(EntityId id) {
    auto it = billboards_.find(id);
    return (it != billboards_.end()) ? &it->second : nullptr;
}

bool ECS::HasBillboard(EntityId id) const {
    return billboards_.find(id) != billboards_.end();
}

void ECS::UpdateBillboards(const hmm_vec3& cameraPosition) {
    for (auto& [id, billboard] : billboards_) {
        Transform* t = GetTransform(id);
        if (!t) continue;
        
        // If following a target, update position
        if (billboard.followTarget != -1) {
            Transform* targetTransform = GetTransform(billboard.followTarget);
            if (targetTransform) {
                t->position = HMM_AddVec3(targetTransform->position, billboard.offset);
            }
        }
        
        // Calculate direction to camera
        hmm_vec3 toCamera = HMM_SubtractVec3(cameraPosition, t->position);
        float length = sqrtf(toCamera.X * toCamera.X + toCamera.Y * toCamera.Y + toCamera.Z * toCamera.Z);
        
        if (length < 0.001f) continue; // Too close, skip
        
        // Normalize direction
        hmm_vec3 forward = HMM_Vec3(toCamera.X / length, toCamera.Y / length, toCamera.Z / length);
        
        if (billboard.lockY) {
            // Cylindrical billboard - only rotate around Y axis
            forward.Y = 0.0f;
            float horizontalLength = sqrtf(forward.X * forward.X + forward.Z * forward.Z);
            if (horizontalLength > 0.001f) {
                forward.X /= horizontalLength;
                forward.Z /= horizontalLength;
            } else {
                forward = HMM_Vec3(0.0f, 0.0f, 1.0f); // Default forward
            }
        }
        
        // Build a look-at rotation matrix
        // Forward = direction to camera
        // Up = world up (or local up for cylindrical)
        // Right = cross(up, forward)
        // Recalculate Up = cross(forward, right) to ensure orthogonality
        
        hmm_vec3 worldUp = HMM_Vec3(0.0f, 1.0f, 0.0f);
        
        // Calculate right vector (perpendicular to forward and up)
        hmm_vec3 right = HMM_Cross(worldUp, forward);
        float rightLength = sqrtf(right.X * right.X + right.Y * right.Y + right.Z * right.Z);
        
        if (rightLength < 0.001f) {
            // Forward is parallel to world up, choose arbitrary right
            right = HMM_Vec3(1.0f, 0.0f, 0.0f);
        } else {
            right.X /= rightLength;
            right.Y /= rightLength;
            right.Z /= rightLength;
        }
        
        // Recalculate up to ensure orthogonality
        hmm_vec3 up = HMM_Cross(forward, right);
        
        // Build rotation matrix from basis vectors
        // Note: We want the billboard to face the camera, so forward points AWAY from the object
        // The quad's default orientation has its normal pointing in +Z direction
        // So we need to negate the forward vector to make it face the camera
        hmm_vec3 negForward = HMM_Vec3(-forward.X, -forward.Y, -forward.Z);
        
        hmm_mat4 rotationMatrix;
        // Column 0: Right vector
        rotationMatrix.Elements[0][0] = right.X;
        rotationMatrix.Elements[0][1] = right.Y;
        rotationMatrix.Elements[0][2] = right.Z;
        rotationMatrix.Elements[0][3] = 0.0f;
        
        // Column 1: Up vector
        rotationMatrix.Elements[1][0] = up.X;
        rotationMatrix.Elements[1][1] = up.Y;
        rotationMatrix.Elements[1][2] = up.Z;
        rotationMatrix.Elements[1][3] = 0.0f;
        
        // Column 2: Forward vector (negated to face camera)
        rotationMatrix.Elements[2][0] = negForward.X;
        rotationMatrix.Elements[2][1] = negForward.Y;
        rotationMatrix.Elements[2][2] = negForward.Z;
        rotationMatrix.Elements[2][3] = 0.0f;
        
        // Column 3: Translation (position)
        rotationMatrix.Elements[3][0] = t->position.X;
        rotationMatrix.Elements[3][1] = t->position.Y;
        rotationMatrix.Elements[3][2] = t->position.Z;
        rotationMatrix.Elements[3][3] = 1.0f;
        
        // Set custom matrix and enable it
        t->customMatrix = rotationMatrix;
        t->useCustomMatrix = true;
    }
}

// New Screen Space Methods
void ECS::AddScreenSpace(EntityId id, const ScreenSpace& ss) {
    screen_spaces_[id] = ss;
}

ScreenSpace* ECS::GetScreenSpace(EntityId id) {
    auto it = screen_spaces_.find(id);
    return (it != screen_spaces_.end()) ? &it->second : nullptr;
}

bool ECS::HasScreenSpace(EntityId id) const {
    return screen_spaces_.find(id) != screen_spaces_.end();
}

void ECS::UpdateScreenSpace(float screenWidth, float screenHeight) {
    for (auto& [id, screenSpace] : screen_spaces_) {
        Transform* t = GetTransform(id);
        if (!t) continue;
        
        // Convert normalized screen coordinates (0-1) to NDC (-1 to 1)
        float x = (screenSpace.screenPosition.X - 0.5f) * 2.0f;
        float y = (0.5f - screenSpace.screenPosition.Y) * 2.0f;
        
        // Calculate size in NDC
        float sizeInNDC_X = screenSpace.size.X * 2.0f;
        float sizeInNDC_Y = screenSpace.size.Y * 2.0f;
        
        // Scale from quad size (0.2) to desired NDC size
        float scaleX = sizeInNDC_X / 0.2f;
        float scaleY = sizeInNDC_Y / 0.2f;
        
        // HandmadeMath uses COLUMN-MAJOR matrices
        // So Elements[column][row]
        hmm_mat4 screenMatrix = HMM_Mat4d(1.0f);
        
        // Column 0: X axis (scale X)
        screenMatrix.Elements[0][0] = scaleX;
        screenMatrix.Elements[1][0] = 0.0f;
        screenMatrix.Elements[2][0] = 0.0f;
        screenMatrix.Elements[3][0] = x; // Translation X
        
        // Column 1: Y axis (scale Y)
        screenMatrix.Elements[0][1] = 0.0f;
        screenMatrix.Elements[1][1] = scaleY;
        screenMatrix.Elements[2][1] = 0.0f;
        screenMatrix.Elements[3][1] = y; // Translation Y
        
        // Column 2: Z axis
        screenMatrix.Elements[0][2] = 0.0f;
        screenMatrix.Elements[1][2] = 0.0f;
        screenMatrix.Elements[2][2] = 1.0f;
        screenMatrix.Elements[3][2] = 0.0f; // Translation Z
        
        // Column 3: W component (homogeneous coordinate)
        screenMatrix.Elements[0][3] = 0.0f;
        screenMatrix.Elements[1][3] = 0.0f;
        screenMatrix.Elements[2][3] = 0.0f;
        screenMatrix.Elements[3][3] = 1.0f; // This ensures w=1.0 after transformation
        
        t->customMatrix = screenMatrix;
        t->useCustomMatrix = true;
    }
}

std::vector<EntityId> ECS::GetScreenSpaceEntities() const {
    std::vector<EntityId> result;
    for (const auto& [id, _] : screen_spaces_) {
        result.push_back(id);
    }
    return result;
}

std::vector<EntityId> ECS::AllEntities() const { return alive_; }

// -- Systems ---------------------------------------------------------------

static inline float randf() { return (float)std::rand() / (float)RAND_MAX; }

// Helper to smoothly interpolate angles (handles wrapping at -PI/+PI)
static inline float LerpAngle(float from, float to, float t) {
    const float PI = 3.14159265359f;
    float delta = fmodf(to - from, 2.0f * PI);
    if (delta > PI) delta -= 2.0f * PI;
    if (delta < -PI) delta += 2.0f * PI;
    return from + delta * t;
}

void ECS::UpdateAI(float dt) {
    for (auto &kv : ai_controllers_) {
        EntityId id = kv.first;
        AIController &ai = kv.second;
        Transform* t = GetTransform(id);
        if (!t) continue;

        ai.stateTimer -= dt;
        switch (ai.state) {
            case AIState::Idle:
                if (ai.stateTimer <= 0.0f) {
                    ai.state = AIState::Wander;
                    ai.stateTimer = 3.0f + randf() * 4.0f;
                    ai.wanderTarget = HMM_AddVec3(t->position, HMM_Vec3((randf()-0.5f)*10.0f,0.0f,(randf()-0.5f)*10.0f));
                }
                break;
            case AIState::Wander: {
                hmm_vec3 dir = HMM_SubtractVec3(ai.wanderTarget, t->position);
                float dx = dir.X; float dz = dir.Z;
                float dist = sqrtf(dx*dx + dz*dz);
                if (dist > 0.1f) {
                    float speed = 1.0f;
                    t->position = HMM_AddVec3(t->position, HMM_Vec3(dx/dist*speed*dt, 0.0f, dz/dist*speed*dt));
                    
                    // Smooth rotation instead of instant snap
                    float targetYaw = atan2f(dx, dz);
                    float rotationSpeed = 5.0f; // radians per second (adjust for faster/slower turning)
                    t->yaw = LerpAngle(t->yaw, targetYaw, rotationSpeed * dt);
                } else {
                    ai.state = AIState::Idle;
                    ai.stateTimer = 1.0f + randf()*2.0f;
                }
                break;
            }
            case AIState::Chase:
                // placeholder (needs target reference)
                break;
        }
    }
}

void ECS::UpdatePhysics(float dt) {
    const hmm_vec3 gravity = HMM_Vec3(0.0f, -9.81f, 0.0f);
    for (auto &kv : rigidbodies_) {
        EntityId id = kv.first;
        Rigidbody &rb = kv.second;
        Transform* t = GetTransform(id);
        if (!t) continue;
        if (rb.affectedByGravity) {
            rb.velocity = HMM_AddVec3(rb.velocity, HMM_Vec3(gravity.X * dt, gravity.Y * dt, gravity.Z * dt));
        }
        t->position = HMM_AddVec3(t->position, HMM_Vec3(rb.velocity.X * dt, rb.velocity.Y * dt, rb.velocity.Z * dt));
        if (t->position.Y < 0.0f) {
            t->position.Y = 0.0f;
            rb.velocity.Y = 0.0f;
        }
    }
}

void ECS::UpdateAnimation(float dt) {
    for (auto &kv : animators_) {
        Animator &a = kv.second;
        a.time += dt;
        if (a.time > 10.0f) a.time = 0.0f;
    }
}

void ECS::SyncToRenderer(Renderer& renderer) {
    static int syncCounter = 0;
    for (EntityId id : alive_) {
        auto it = instance_for_entity_.find(id);
        if (it == instance_for_entity_.end()) continue;
        int instId = it->second;
        Transform* t = GetTransform(id);
        if (t && instId >= 0) {
            hmm_mat4 modelMatrix = t->ModelMatrix();
            renderer.UpdateInstanceTransform(instId, modelMatrix);
        }
    }
}

void ECS::AddLight(EntityId entity, const Light& light) {
    lights_[entity] = light;
}

Light* ECS::GetLight(EntityId entity) {
    auto it = lights_.find(entity);
    return (it != lights_.end()) ? &it->second : nullptr;
}

void ECS::RemoveLight(EntityId entity) {
    lights_.erase(entity);
}

// Selectable component methods
void ECS::AddSelectable(EntityId id, const Selectable& sel) {
    selectables_[id] = sel;
}

Selectable* ECS::GetSelectable(EntityId id) {
    auto it = selectables_.find(id);
    return (it != selectables_.end()) ? &it->second : nullptr;
}

bool ECS::HasSelectable(EntityId id) const {
    return selectables_.find(id) != selectables_.end();
}

// Ray-sphere intersection for entity picking
EntityId ECS::RaycastSelection(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float maxDistance) {
    EntityId closestEntity = -1;
    float closestDist = maxDistance;
    
    for (const auto& [id, selectable] : selectables_) {
        Transform* t = GetTransform(id);
        if (!t) continue;
        
        // Ray-sphere intersection
        hmm_vec3 oc = HMM_SubtractVec3(rayOrigin, t->position);
        float a = HMM_DotVec3(rayDir, rayDir);
        float b = 2.0f * HMM_DotVec3(oc, rayDir);
        float c = HMM_DotVec3(oc, oc) - selectable.boundingRadius * selectable.boundingRadius;
        float discriminant = b * b - 4 * a * c;
        
        if (discriminant >= 0.0f) {
            float t1 = (-b - sqrtf(discriminant)) / (2.0f * a);
            if (t1 > 0.0f && t1 < closestDist) {
                closestDist = t1;
                closestEntity = id;
            }
        }
    }
    
    return closestEntity;
}