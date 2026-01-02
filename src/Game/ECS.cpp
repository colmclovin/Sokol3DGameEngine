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
        // Note: Renderer has no RemoveInstance by id in current API; implement later if needed.
        instance_for_entity_.erase(it);
    }
    mesh_for_entity_.erase(id);
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
            
            // Debug: Print player entity sync (entity ID 1)
            if (++syncCounter >= 60 && id == 1) {
                printf("ECS::SyncToRenderer - Entity %d, InstanceId %d, yaw=%.1f deg\n", 
                       id, instId, t->yaw);
                syncCounter = 0;
            }
        }
    }
}