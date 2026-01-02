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
    colliders_.erase(id);
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

void ECS::AddCollider(EntityId id, const Collider& col) { colliders_[id] = col; }
Collider* ECS::GetCollider(EntityId id) {
    auto it = colliders_.find(id);
    return (it != colliders_.end()) ? &it->second : nullptr;
}
bool ECS::HasCollider(EntityId id) const { return colliders_.find(id) != colliders_.end(); }

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
        int instanceId = it->second;
        renderer.RemoveInstance(instanceId);
        instance_for_entity_.erase(it);
    }
    mesh_for_entity_.erase(id);
}

// Billboard Methods
void ECS::AddBillboard(EntityId id, const Billboard& b) { billboards_[id] = b; }
Billboard* ECS::GetBillboard(EntityId id) {
    auto it = billboards_.find(id);
    return (it != billboards_.end()) ? &it->second : nullptr;
}
bool ECS::HasBillboard(EntityId id) const { return billboards_.find(id) != billboards_.end(); }

void ECS::UpdateBillboards(const hmm_vec3& cameraPosition) {
    for (auto& [id, billboard] : billboards_) {
        Transform* t = GetTransform(id);
        if (!t) continue;
        
        if (billboard.followTarget != -1) {
            Transform* targetTransform = GetTransform(billboard.followTarget);
            if (targetTransform) {
                t->position = HMM_AddVec3(targetTransform->position, billboard.offset);
            }
        }
        
        hmm_vec3 toCamera = HMM_SubtractVec3(cameraPosition, t->position);
        float length = sqrtf(toCamera.X * toCamera.X + toCamera.Y * toCamera.Y + toCamera.Z * toCamera.Z);
        
        if (length < 0.001f) continue;
        
        hmm_vec3 forward = HMM_Vec3(toCamera.X / length, toCamera.Y / length, toCamera.Z / length);
        
        if (billboard.lockY) {
            forward.Y = 0.0f;
            float horizontalLength = sqrtf(forward.X * forward.X + forward.Z * forward.Z);
            if (horizontalLength > 0.001f) {
                forward.X /= horizontalLength;
                forward.Z /= horizontalLength;
            } else {
                forward = HMM_Vec3(0.0f, 0.0f, 1.0f);
            }
        }
        
        hmm_vec3 worldUp = HMM_Vec3(0.0f, 1.0f, 0.0f);
        hmm_vec3 right = HMM_Cross(worldUp, forward);
        float rightLength = sqrtf(right.X * right.X + right.Y * right.Y + right.Z * right.Z);
        
        if (rightLength < 0.001f) {
            right = HMM_Vec3(1.0f, 0.0f, 0.0f);
        } else {
            right.X /= rightLength;
            right.Y /= rightLength;
            right.Z /= rightLength;
        }
        
        hmm_vec3 up = HMM_Cross(forward, right);
        hmm_vec3 negForward = HMM_Vec3(-forward.X, -forward.Y, -forward.Z);
        
        hmm_mat4 rotationMatrix;
        rotationMatrix.Elements[0][0] = right.X;
        rotationMatrix.Elements[0][1] = right.Y;
        rotationMatrix.Elements[0][2] = right.Z;
        rotationMatrix.Elements[0][3] = 0.0f;
        
        rotationMatrix.Elements[1][0] = up.X;
        rotationMatrix.Elements[1][1] = up.Y;
        rotationMatrix.Elements[1][2] = up.Z;
        rotationMatrix.Elements[1][3] = 0.0f;
        
        rotationMatrix.Elements[2][0] = negForward.X;
        rotationMatrix.Elements[2][1] = negForward.Y;
        rotationMatrix.Elements[2][2] = negForward.Z;
        rotationMatrix.Elements[2][3] = 0.0f;
        
        rotationMatrix.Elements[3][0] = t->position.X;
        rotationMatrix.Elements[3][1] = t->position.Y;
        rotationMatrix.Elements[3][2] = t->position.Z;
        rotationMatrix.Elements[3][3] = 1.0f;
        
        t->customMatrix = rotationMatrix;
        t->useCustomMatrix = true;
    }
}

// Screen Space Methods
void ECS::AddScreenSpace(EntityId id, const ScreenSpace& ss) { screen_spaces_[id] = ss; }
ScreenSpace* ECS::GetScreenSpace(EntityId id) {
    auto it = screen_spaces_.find(id);
    return (it != screen_spaces_.end()) ? &it->second : nullptr;
}
bool ECS::HasScreenSpace(EntityId id) const { return screen_spaces_.find(id) != screen_spaces_.end(); }

void ECS::UpdateScreenSpace(float screenWidth, float screenHeight) {
    for (auto& [id, screenSpace] : screen_spaces_) {
        Transform* t = GetTransform(id);
        if (!t) continue;
        
        float x = (screenSpace.screenPosition.X - 0.5f) * 2.0f;
        float y = (0.5f - screenSpace.screenPosition.Y) * 2.0f;
        
        float sizeInNDC_X = screenSpace.size.X * 2.0f;
        float sizeInNDC_Y = screenSpace.size.Y * 2.0f;
        
        float scaleX = sizeInNDC_X / 0.2f;
        float scaleY = sizeInNDC_Y / 0.2f;
        
        hmm_mat4 screenMatrix = HMM_Mat4d(1.0f);
        
        screenMatrix.Elements[0][0] = scaleX;
        screenMatrix.Elements[1][0] = 0.0f;
        screenMatrix.Elements[2][0] = 0.0f;
        screenMatrix.Elements[3][0] = x;
        
        screenMatrix.Elements[0][1] = 0.0f;
        screenMatrix.Elements[1][1] = scaleY;
        screenMatrix.Elements[2][1] = 0.0f;
        screenMatrix.Elements[3][1] = y;
        
        screenMatrix.Elements[0][2] = 0.0f;
        screenMatrix.Elements[1][2] = 0.0f;
        screenMatrix.Elements[2][2] = 1.0f;
        screenMatrix.Elements[3][2] = 0.0f;
        
        screenMatrix.Elements[0][3] = 0.0f;
        screenMatrix.Elements[1][3] = 0.0f;
        screenMatrix.Elements[2][3] = 0.0f;
        screenMatrix.Elements[3][3] = 1.0f;
        
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
                    
                    float targetYaw = atan2f(dx, dz);
                    float rotationSpeed = 5.0f;
                    t->yaw = LerpAngle(t->yaw, targetYaw, rotationSpeed * dt);
                } else {
                    ai.state = AIState::Idle;
                    ai.stateTimer = 1.0f + randf()*2.0f;
                }
                break;
            }
            case AIState::Chase:
                break;
        }
    }
}

void ECS::UpdatePhysics(float dt) {
    const hmm_vec3 gravity = HMM_Vec3(0.0f, -9.81f, 0.0f);
    
    // Apply gravity and integrate velocity
    for (auto &kv : rigidbodies_) {
        EntityId id = kv.first;
        Rigidbody &rb = kv.second;
        Transform* t = GetTransform(id);
        if (!t || rb.isKinematic) continue;
        
        // Apply gravity
        if (rb.affectedByGravity) {
            rb.velocity = HMM_AddVec3(rb.velocity, HMM_MultiplyVec3f(gravity, dt));
        }
        
        // Apply drag
        if (rb.drag > 0.0f) {
            float dragFactor = 1.0f - (rb.drag * dt);
            if (dragFactor < 0.0f) dragFactor = 0.0f;
            rb.velocity = HMM_MultiplyVec3f(rb.velocity, dragFactor);
        }
        
        // Integrate position
        t->position = HMM_AddVec3(t->position, HMM_MultiplyVec3f(rb.velocity, dt));
        
        // Simple ground collision (Y = 0)
        if (t->position.Y < 0.0f) {
            t->position.Y = 0.0f;
            
            // Apply bounciness
            if (rb.bounciness > 0.0f) {
                rb.velocity.Y = -rb.velocity.Y * rb.bounciness;
                // Stop bouncing if velocity is too small
                if (fabsf(rb.velocity.Y) < 0.1f) {
                    rb.velocity.Y = 0.0f;
                }
            } else {
                rb.velocity.Y = 0.0f;
            }
        }
    }
}

void ECS::UpdateCollisions(float dt) {
    // Broad phase: Check all pairs with colliders
    std::vector<EntityId> collidableEntities;
    for (const auto& [id, _] : colliders_) {
        collidableEntities.push_back(id);
    }
    
    // Narrow phase: Check collisions and resolve
    for (size_t i = 0; i < collidableEntities.size(); ++i) {
        for (size_t j = i + 1; j < collidableEntities.size(); ++j) {
            EntityId a = collidableEntities[i];
            EntityId b = collidableEntities[j];
            
            Collider* colA = GetCollider(a);
            Collider* colB = GetCollider(b);
            
            // Check collision layers
            if ((colA->collisionMask & colB->collisionLayer) == 0 ||
                (colB->collisionMask & colA->collisionLayer) == 0) {
                continue; // Layers don't match
            }
            
            CollisionInfo info;
            if (CheckCollision(a, b, &info)) {
                // Skip if either is a trigger (no physics response)
                if (!colA->isTrigger && !colB->isTrigger) {
                    ResolveCollision(a, b, info);
                }
            }
        }
    }
}

bool ECS::CheckCollision(EntityId a, EntityId b, CollisionInfo* outInfo) {
    Collider* colA = GetCollider(a);
    Collider* colB = GetCollider(b);
    Transform* transA = GetTransform(a);
    Transform* transB = GetTransform(b);
    
    if (!colA || !colB || !transA || !transB) return false;
    
    // Sphere vs Sphere
    if (colA->type == ColliderType::Sphere && colB->type == ColliderType::Sphere) {
        return SphereVsSphere(transA->position, colA->radius, transB->position, colB->radius, outInfo);
    }
    
    // Sphere vs Box
    if (colA->type == ColliderType::Sphere && colB->type == ColliderType::Box) {
        return SphereVsBox(transA->position, colA->radius, transB->position, colB->boxHalfExtents, outInfo);
    }
    if (colA->type == ColliderType::Box && colB->type == ColliderType::Sphere) {
        bool result = SphereVsBox(transB->position, colB->radius, transA->position, colA->boxHalfExtents, outInfo);
        if (result && outInfo) {
            // Flip normal since we swapped order
            outInfo->normal = HMM_MultiplyVec3f(outInfo->normal, -1.0f);
            std::swap(outInfo->entityA, outInfo->entityB);
        }
        return result;
    }
    
    return false;
}

bool ECS::SphereVsSphere(const hmm_vec3& posA, float radiusA, const hmm_vec3& posB, float radiusB, CollisionInfo* outInfo) {
    hmm_vec3 diff = HMM_SubtractVec3(posB, posA);
    float distSq = HMM_DotVec3(diff, diff);
    float radiusSum = radiusA + radiusB;
    
    if (distSq < radiusSum * radiusSum) {
        if (outInfo) {
            float dist = sqrtf(distSq);
            outInfo->normal = (dist > 0.001f) ? HMM_MultiplyVec3f(diff, 1.0f / dist) : HMM_Vec3(0.0f, 1.0f, 0.0f);
            outInfo->penetration = radiusSum - dist;
            outInfo->contactPoint = HMM_AddVec3(posA, HMM_MultiplyVec3f(outInfo->normal, radiusA - outInfo->penetration * 0.5f));
        }
        return true;
    }
    return false;
}

bool ECS::SphereVsBox(const hmm_vec3& spherePos, float radius, const hmm_vec3& boxPos, const hmm_vec3& boxHalfExtents, CollisionInfo* outInfo) {
    // Find closest point on box to sphere
    hmm_vec3 closestPoint;
    closestPoint.X = fmaxf(boxPos.X - boxHalfExtents.X, fminf(spherePos.X, boxPos.X + boxHalfExtents.X));
    closestPoint.Y = fmaxf(boxPos.Y - boxHalfExtents.Y, fminf(spherePos.Y, boxPos.Y + boxHalfExtents.Y));
    closestPoint.Z = fmaxf(boxPos.Z - boxHalfExtents.Z, fminf(spherePos.Z, boxPos.Z + boxHalfExtents.Z));
    
    hmm_vec3 diff = HMM_SubtractVec3(spherePos, closestPoint);
    float distSq = HMM_DotVec3(diff, diff);
    
    if (distSq < radius * radius) {
        if (outInfo) {
            float dist = sqrtf(distSq);
            outInfo->normal = (dist > 0.001f) ? HMM_MultiplyVec3f(diff, 1.0f / dist) : HMM_Vec3(0.0f, 1.0f, 0.0f);
            outInfo->penetration = radius - dist;
            outInfo->contactPoint = closestPoint;
        }
        return true;
    }
    return false;
}

void ECS::ResolveCollision(EntityId a, EntityId b, const CollisionInfo& info) {
    Rigidbody* rbA = GetRigidbody(a);
    Rigidbody* rbB = GetRigidbody(b);
    Transform* transA = GetTransform(a);
    Transform* transB = GetTransform(b);
    Collider* colA = GetCollider(a);
    Collider* colB = GetCollider(b);
    
    if (!transA || !transB || !colA || !colB) return;
    
    // Determine if objects can move
    bool aStatic = colA->isStatic || (rbA && rbA->isKinematic);
    bool bStatic = colB->isStatic || (rbB && rbB->isKinematic);
    
    if (aStatic && bStatic) return; // Both static, no resolution
    
    // Position correction (separate objects)
    float totalMass = 0.0f;
    float massA = (rbA && !aStatic) ? rbA->mass : 0.0f;
    float massB = (rbB && !bStatic) ? rbB->mass : 0.0f;
    totalMass = massA + massB;
    
    if (totalMass < 0.001f) totalMass = 1.0f; // Avoid division by zero
    
    float correctionA = (!aStatic) ? (massB / totalMass) : 0.0f;
    float correctionB = (!bStatic) ? (massA / totalMass) : 0.0f;
    
    hmm_vec3 correction = HMM_MultiplyVec3f(info.normal, info.penetration);
    
    if (!aStatic) {
        transA->position = HMM_SubtractVec3(transA->position, HMM_MultiplyVec3f(correction, correctionA));
    }
    if (!bStatic) {
        transB->position = HMM_AddVec3(transB->position, HMM_MultiplyVec3f(correction, correctionB));
    }
    
    // Velocity resolution (impulse-based)
    if (rbA && rbB && !aStatic && !bStatic) {
        hmm_vec3 relativeVelocity = HMM_SubtractVec3(rbB->velocity, rbA->velocity);
        float velocityAlongNormal = HMM_DotVec3(relativeVelocity, info.normal);
        
        if (velocityAlongNormal < 0.0f) return; // Objects moving apart
        
        float restitution = (rbA->bounciness + rbB->bounciness) * 0.5f;
        float impulseScalar = -(1.0f + restitution) * velocityAlongNormal;
        impulseScalar /= (1.0f / massA + 1.0f / massB);
        
        hmm_vec3 impulse = HMM_MultiplyVec3f(info.normal, impulseScalar);
        
        rbA->velocity = HMM_SubtractVec3(rbA->velocity, HMM_MultiplyVec3f(impulse, 1.0f / massA));
        rbB->velocity = HMM_AddVec3(rbB->velocity, HMM_MultiplyVec3f(impulse, 1.0f / massB));
    } else if (rbA && !aStatic) {
        // A is dynamic, B is static - reflect A's velocity
        float velocityAlongNormal = HMM_DotVec3(rbA->velocity, info.normal);
        if (velocityAlongNormal < 0.0f) {
            hmm_vec3 reflection = HMM_MultiplyVec3f(info.normal, velocityAlongNormal * (1.0f + rbA->bounciness));
            rbA->velocity = HMM_SubtractVec3(rbA->velocity, reflection);
        }
    } else if (rbB && !bStatic) {
        // B is dynamic, A is static - reflect B's velocity
        float velocityAlongNormal = HMM_DotVec3(rbB->velocity, info.normal);
        if (velocityAlongNormal > 0.0f) {
            hmm_vec3 reflection = HMM_MultiplyVec3f(info.normal, velocityAlongNormal * (1.0f + rbB->bounciness));
            rbB->velocity = HMM_AddVec3(rbB->velocity, reflection);
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

void ECS::AddLight(EntityId entity, const Light& light) { lights_[entity] = light; }
Light* ECS::GetLight(EntityId entity) {
    auto it = lights_.find(entity);
    return (it != lights_.end()) ? &it->second : nullptr;
}
void ECS::RemoveLight(EntityId entity) { lights_.erase(entity); }

void ECS::AddSelectable(EntityId id, const Selectable& sel) { selectables_[id] = sel; }
Selectable* ECS::GetSelectable(EntityId id) {
    auto it = selectables_.find(id);
    return (it != selectables_.end()) ? &it->second : nullptr;
}
bool ECS::HasSelectable(EntityId id) const { return selectables_.find(id) != selectables_.end(); }

EntityId ECS::RaycastSelection(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float maxDistance) {
    EntityId closestEntity = -1;
    float closestDist = maxDistance;
    
    for (const auto& [id, selectable] : selectables_) {
        Transform* t = GetTransform(id);
        if (!t) continue;
        
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

hmm_vec3 ECS::GetPlacementPosition(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float distance) {
    // Default: place at fixed distance along ray
    hmm_vec3 placementPos = HMM_AddVec3(rayOrigin, HMM_MultiplyVec3f(rayDir, distance));
    
    // Try to snap to ground (Y = 0) or existing colliders
    float bestT = distance;
    
    // Check ground plane intersection (Y = 0)
    if (fabsf(rayDir.Y) > 0.001f) {
        float t = -rayOrigin.Y / rayDir.Y;
        if (t > 0.0f && t < bestT) {
            bestT = t;
            placementPos = HMM_AddVec3(rayOrigin, HMM_MultiplyVec3f(rayDir, t));
        }
    }
    
    return placementPos;
}