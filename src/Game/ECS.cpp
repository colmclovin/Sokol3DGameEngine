#include "ECS.h"
#include <cstdlib>
#include <cmath>
#include <map>  // ADDED: For std::map in ComputeBoneTransform
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
        
        // REMOVED: Old hardcoded ground collision at Y=0
        // This is now handled by UpdateCollisions() checking against the plane collider
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

bool ECS::CheckCollision(EntityId a, EntityId b, CollisionInfo *outInfo) {
    Collider *colA = GetCollider(a);
    Collider *colB = GetCollider(b);
    Transform *transA = GetTransform(a);
    Transform *transB = GetTransform(b);

    if (!colA || !colB || !transA || !transB) return false;

    // ADDED: Sphere vs Plane (most important for ground collision!)
    if (colA->type == ColliderType::Sphere && colB->type == ColliderType::Plane) {
        return SphereVsPlane(transA->position, colA->radius, colB->planeNormal, colB->planeDistance, outInfo);
    }
    if (colA->type == ColliderType::Plane && colB->type == ColliderType::Sphere) {
        bool result = SphereVsPlane(transB->position, colB->radius, colA->planeNormal, colA->planeDistance, outInfo);
        if (result && outInfo) {
            // Flip normal since we swapped order
            outInfo->normal = HMM_MultiplyVec3f(outInfo->normal, -1.0f);
            std::swap(outInfo->entityA, outInfo->entityB);
        }
        return result;
    }

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

bool ECS::SphereVsSphere(const hmm_vec3 &posA, float radiusA, const hmm_vec3 &posB, float radiusB, CollisionInfo *outInfo) {
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

bool ECS::SphereVsBox(const hmm_vec3 &spherePos, float radius, const hmm_vec3 &boxPos, const hmm_vec3 &boxHalfExtents, CollisionInfo *outInfo) {
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

// ADDED: Sphere vs Plane collision detection
bool ECS::SphereVsPlane(const hmm_vec3& spherePos, float radius, const hmm_vec3& planeNormal, float planeDistance, CollisionInfo* outInfo) {
    // Distance from sphere center to plane (signed distance)
    // Positive = above plane, Negative = below plane
    float distanceToPlane = HMM_DotVec3(spherePos, planeNormal) - planeDistance;
    
    // Check if sphere intersects plane
    if (distanceToPlane < radius) {
        if (outInfo) {
            // Penetration is how much the sphere has gone into the plane
            outInfo->penetration = radius - distanceToPlane;
            
            // Normal points away from plane (upward for ground)
            outInfo->normal = planeNormal;
            
            // Contact point on the plane surface
            outInfo->contactPoint = HMM_SubtractVec3(spherePos, HMM_MultiplyVec3f(planeNormal, distanceToPlane));
            
            //printf("DEBUG: Sphere-Plane collision | dist=%.3f, penetration=%.3f\n", distanceToPlane, outInfo->penetration);
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
    
    // ADDED: Clamp penetration to prevent explosion
    float clampedPenetration = fminf(info.penetration, 10.0f);  // Max 10 units correction per frame
    if (info.penetration > 10.0f) {
        //printf("WARNING: Large penetration %.2f clamped to 10.0\n", info.penetration);
    }
    
    // FIXED: Position correction along the collision normal
    hmm_vec3 correction = HMM_MultiplyVec3f(info.normal, clampedPenetration);
    
    if (!aStatic && !bStatic) {
        // Both dynamic - split the correction based on mass
        float totalMass = (rbA ? rbA->mass : 1.0f) + (rbB ? rbB->mass : 1.0f);
        float massA = rbA ? rbA->mass : 1.0f;
        float massB = rbB ? rbB->mass : 1.0f;
        
        float correctionA = massB / totalMass;
        float correctionB = massA / totalMass;
        
        // Move A along normal (away from B)
        transA->position = HMM_AddVec3(transA->position, HMM_MultiplyVec3f(correction, correctionA));
        transB->position = HMM_SubtractVec3(transB->position, HMM_MultiplyVec3f(correction, correctionB));
    } else if (!aStatic) {
        // FIXED: A is dynamic, B is static (e.g., sphere vs plane)
        // Move A away from B along the normal (push sphere UP out of ground)
        transA->position = HMM_AddVec3(transA->position, correction);
        
       //printf("Corrected entity A by %.3f along normal (%.2f, %.2f, %.2f)\n", 
              // clampedPenetration, info.normal.X, info.normal.Y, info.normal.Z);
    } else if (!bStatic) {
        // B is dynamic, A is static
        // Move B away from A along the normal
        transB->position = HMM_SubtractVec3(transB->position, correction);
    }
    
    // Velocity resolution (impulse-based)
    if (rbA && rbB && !aStatic && !bStatic) {
        hmm_vec3 relativeVelocity = HMM_SubtractVec3(rbB->velocity, rbA->velocity);
        float velocityAlongNormal = HMM_DotVec3(relativeVelocity, info.normal);
        
        if (velocityAlongNormal >= 0.0f) return; // Objects moving apart
        
        float restitution = (rbA->bounciness + rbB->bounciness) * 0.5f;
        float impulseScalar = -(1.0f + restitution) * velocityAlongNormal;
        impulseScalar /= (1.0f / rbA->mass + 1.0f / rbB->mass);
        
        hmm_vec3 impulse = HMM_MultiplyVec3f(info.normal, impulseScalar);
        
        rbA->velocity = HMM_SubtractVec3(rbA->velocity, HMM_MultiplyVec3f(impulse, 1.0f / rbA->mass));
        rbB->velocity = HMM_AddVec3(rbB->velocity, HMM_MultiplyVec3f(impulse, 1.0f / rbB->mass));
    } else if (rbA && !aStatic) {
        // A is dynamic, B is static - stop velocity along collision normal
        float velocityAlongNormal = HMM_DotVec3(rbA->velocity, info.normal);
        
        // Only correct if moving INTO the static object (downward for ground)
        if (velocityAlongNormal < 0.0f) {
            // FIXED: Stop all downward velocity (set Y component to 0 for ground collision)
            if (fabsf(info.normal.Y) > 0.9f) {  // Ground plane (normal mostly vertical)
                rbA->velocity.Y = 0.0f;  // Stop falling
               // printf("Stopped downward velocity\n");
            } else {
                // General case: reflect velocity
                hmm_vec3 normalVelocity = HMM_MultiplyVec3f(info.normal, velocityAlongNormal);
                rbA->velocity = HMM_SubtractVec3(rbA->velocity, HMM_MultiplyVec3f(normalVelocity, 1.0f + rbA->bounciness));
            }
        }
    } else if (rbB && !bStatic) {
        // B is dynamic, A is static
        float velocityAlongNormal = HMM_DotVec3(rbB->velocity, info.normal);
        
        // Only correct if moving INTO the static object
        if (velocityAlongNormal > 0.0f) {
            hmm_vec3 normalVelocity = HMM_MultiplyVec3f(info.normal, velocityAlongNormal);
            rbB->velocity = HMM_SubtractVec3(rbB->velocity, HMM_MultiplyVec3f(normalVelocity, 1.0f + rbB->bounciness));
        }
    }
}

// Add these helper functions before UpdateAnimation:
static hmm_vec3 LerpVec3(const hmm_vec3& a, float t, const hmm_vec3& b) {
    return HMM_Vec3(
        HMM_Lerp(a.X, t, b.X),
        HMM_Lerp(a.Y, t, b.Y),
        HMM_Lerp(a.Z, t, b.Z)
    );
}

static hmm_vec3 InterpolatePosition(const std::vector<PositionKey>& keys, float time) {
    if (keys.empty()) return HMM_Vec3(0, 0, 0);
    if (keys.size() == 1) return keys[0].value;
    
    // Find surrounding keyframes
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (time < keys[i + 1].time) {
            float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return LerpVec3(keys[i].value, t, keys[i + 1].value);
        }
    }
    return keys.back().value;
}

static hmm_quaternion InterpolateRotation(const std::vector<RotationKey>& keys, float time) {
    if (keys.empty()) return HMM_Quaternion(0, 0, 0, 1);
    if (keys.size() == 1) return keys[0].value;
    
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (time < keys[i + 1].time) {
            float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return HMM_Slerp(keys[i].value, t, keys[i + 1].value);  // FIXED: Use HMM_Slerp (lowercase 'l')
        }
    }
    return keys.back().value;
}

static hmm_vec3 InterpolateScale(const std::vector<ScaleKey>& keys, float time) {
    if (keys.empty()) return HMM_Vec3(1, 1, 1);
    if (keys.size() == 1) return keys[0].value;
    
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (time < keys[i + 1].time) {
            float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return LerpVec3(keys[i].value, t, keys[i + 1].value);
        }
    }
    return keys.back().value;
}

// Add this helper function to compute bone transforms recursively:
static void ComputeBoneTransform(
    int boneIndex,
    const Skeleton& skeleton,
    const std::map<std::string, hmm_mat4>& boneTransforms,
    const hmm_mat4& parentTransform,
    std::vector<hmm_mat4>& finalTransforms)
{
    const Bone& bone = skeleton.bones[boneIndex];
    
    // Get this bone's local transform from animation
    hmm_mat4 localTransform = HMM_Mat4d(1.0f);
    auto it = boneTransforms.find(bone.name);
    if (it != boneTransforms.end()) {
        localTransform = it->second;
    }
    
    // Combine with parent transform
    hmm_mat4 globalTransform = HMM_MultiplyMat4(parentTransform, localTransform);
    
    // Final transform = globalInverse * globalTransform * offsetMatrix
    hmm_mat4 finalTransform = HMM_MultiplyMat4(
        HMM_MultiplyMat4(skeleton.globalInverseTransform, globalTransform),
        bone.offsetMatrix
    );
    
    finalTransforms[boneIndex] = finalTransform;
    
    // Process children
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        if (skeleton.bones[i].parentIndex == boneIndex) {
            ComputeBoneTransform(i, skeleton, boneTransforms, globalTransform, finalTransforms);
        }
    }
}

// Now replace UpdateAnimation with this complete version:
void ECS::UpdateAnimation(float dt) {
    if (!renderer_) return;  // Need renderer to access model data
    
    for (auto &kv : animators_) {
        EntityId entityId = kv.first;
        Animator &animator = kv.second;
        
        if (!animator.playing || animator.currentClip < 0) continue;
        
        // Get the model for this entity
        int meshId = GetMeshId(entityId);
        if (meshId < 0) continue;
        
        const Model3D* model = renderer_->GetModelData(meshId);
        if (!model || !model->hasAnimations || animator.currentClip >= (int)model->animations.size()) {
            continue;
        }
        
        const AnimationClip& clip = model->animations[animator.currentClip];
        
        // Update time
        animator.time += dt * animator.speed;
        
        // Convert time to ticks
        float animationTime = animator.time * clip.ticksPerSecond;
        
        // Loop animation
        if (animator.loop && animationTime > clip.duration) {
            animationTime = fmodf(animationTime, clip.duration);
            animator.time = animationTime / clip.ticksPerSecond;
        } else if (!animator.loop && animationTime > clip.duration) {
            animationTime = clip.duration;
            animator.time = animationTime / clip.ticksPerSecond;
            animator.playing = false;
        }
        
        // Compute bone transforms from animation channels
        std::map<std::string, hmm_mat4> boneTransforms;
        
        for (const auto& channel : clip.channels) {
            // Interpolate position, rotation, scale
            hmm_vec3 position = InterpolatePosition(channel.positionKeys, animationTime);
            hmm_quaternion rotation = InterpolateRotation(channel.rotationKeys, animationTime);
            hmm_vec3 scale = InterpolateScale(channel.scaleKeys, animationTime);
            
            // Build transformation matrix: T * R * S
            hmm_mat4 translation = HMM_Translate(position);
            hmm_mat4 rotationMat = HMM_QuaternionToMat4(rotation);
            hmm_mat4 scaleMat = HMM_Scale(scale);
            
            hmm_mat4 localTransform = HMM_MultiplyMat4(
                HMM_MultiplyMat4(translation, rotationMat),
                scaleMat
            );
            
            boneTransforms[channel.boneName] = localTransform;
        }
        
        // Compute final bone transforms recursively
        if (!model->skeleton.bones.empty()) {
            animator.boneTransforms.resize(model->skeleton.bones.size());
            
            // Find root bones (parentIndex == -1) and compute hierarchy
            for (size_t i = 0; i < model->skeleton.bones.size(); ++i) {
                if (model->skeleton.bones[i].parentIndex == -1) {
                    ComputeBoneTransform(i, model->skeleton, boneTransforms, 
                                        HMM_Mat4d(1.0f), animator.boneTransforms);
                }
            }
        }
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
    float closestT = distance;
    bool foundHit = false;
    
    // Raycast against all selectable entities (including ground)
    for (const auto& [id, selectable] : selectables_) {
        Transform* t = GetTransform(id);
        if (!t) continue;
        
        // Get world position (accounting for origin offset)
        hmm_vec3 entityPos = t->GetWorldPosition();
        
        // Sphere intersection test (using bounding radius)
        hmm_vec3 oc = HMM_SubtractVec3(rayOrigin, entityPos);
        float a = HMM_DotVec3(rayDir, rayDir);
        float b = 2.0f * HMM_DotVec3(oc, rayDir);
        float c = HMM_DotVec3(oc, oc) - selectable.boundingRadius * selectable.boundingRadius;
        float discriminant = b * b - 4 * a * c;
        
        if (discriminant >= 0.0f) {
            float t1 = (-b - sqrtf(discriminant)) / (2.0f * a);
            if (t1 > 0.001f && t1 < closestT) {
                closestT = t1;
                foundHit = true;
            }
        }
    }
    
    // If we hit something, place at that point (slightly above the surface)
    if (foundHit) {
        placementPos = HMM_AddVec3(rayOrigin, HMM_MultiplyVec3f(rayDir, closestT));
        // Offset slightly along the ray to place above the surface
        placementPos = HMM_AddVec3(placementPos, HMM_MultiplyVec3f(rayDir, 0.1f));
    }
    
    return placementPos;
}

// ============================================================================
// NEW RAYCAST SYSTEM IMPLEMENTATION
// ============================================================================

void ECS::CreatePlaneCollider(EntityId entity, const hmm_vec3& normal, float distance) {
    Collider collider;
    collider.type = ColliderType::Plane;
    collider.planeNormal = normal;
    collider.planeDistance = distance;
    collider.isStatic = true;
    collider.useBroadPhase = false;  // Infinite planes don't need broad phase
    
    AddCollider(entity, collider);
    printf("Created plane collider: normal=(%.2f, %.2f, %.2f), distance=%.2f\n",
           normal.X, normal.Y, normal.Z, distance);
}

void ECS::CreateMeshCollider(EntityId entity, const Model3D& model) {
    Collider collider;
    collider.type = ColliderType::Mesh;
    collider.isStatic = true;
    
    collider.triangles.reserve(model.index_count / 3);
    
    hmm_vec3 boundsMin = HMM_Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    hmm_vec3 boundsMax = HMM_Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    
    for (int i = 0; i < model.index_count; i += 3) {
        CollisionTriangle tri;
        
        Vertex& v0 = model.vertices[model.indices[i + 0]];
        Vertex& v1 = model.vertices[model.indices[i + 1]];
        Vertex& v2 = model.vertices[model.indices[i + 2]];
        
        tri.v0 = HMM_Vec3(v0.pos[0], v0.pos[1], v0.pos[2]);
        tri.v1 = HMM_Vec3(v1.pos[0], v1.pos[1], v1.pos[2]);
        tri.v2 = HMM_Vec3(v2.pos[0], v2.pos[1], v2.pos[2]);
        
        hmm_vec3 edge1 = HMM_SubtractVec3(tri.v1, tri.v0);
        hmm_vec3 edge2 = HMM_SubtractVec3(tri.v2, tri.v0);
        tri.normal = HMM_NormalizeVec3(HMM_Cross(edge1, edge2));
        
        collider.triangles.push_back(tri);
        
        boundsMin = HMM_Vec3(
            fminf(boundsMin.X, fminf(tri.v0.X, fminf(tri.v1.X, tri.v2.X))),
            fminf(boundsMin.Y, fminf(tri.v0.Y, fminf(tri.v1.Y, tri.v2.Y))),
            fminf(boundsMin.Z, fminf(tri.v0.Z, fminf(tri.v1.Z, tri.v2.Z)))
        );
        boundsMax = HMM_Vec3(
            fmaxf(boundsMax.X, fmaxf(tri.v0.X, fmaxf(tri.v1.X, tri.v2.X))),
            fmaxf(boundsMax.Y, fmaxf(tri.v0.Y, fmaxf(tri.v1.Y, tri.v2.Y))),
            fmaxf(boundsMax.Z, fmaxf(tri.v0.Z, fmaxf(tri.v1.Z, tri.v2.Z)))
        );
    }
    
    collider.meshBoundsMin = boundsMin;
    collider.meshBoundsMax = boundsMax;
    
    AddCollider(entity, collider);
    
    printf("Created mesh collider: %zu triangles, bounds: [%.1f, %.1f, %.1f] to [%.1f, %.1f, %.1f]\n",
           collider.triangles.size(),
           boundsMin.X, boundsMin.Y, boundsMin.Z,
           boundsMax.X, boundsMax.Y, boundsMax.Z);
}

bool ECS::RayTriangleIntersect(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir,
                               const CollisionTriangle& tri, float* outDistance, hmm_vec3* outPoint) {
    const float EPSILON = 0.0000001f;
    
    hmm_vec3 edge1 = HMM_SubtractVec3(tri.v1, tri.v0);
    hmm_vec3 edge2 = HMM_SubtractVec3(tri.v2, tri.v0);
    hmm_vec3 h = HMM_Cross(rayDir, edge2);
    float a = HMM_DotVec3(edge1, h);
    
    if (a > -EPSILON && a < EPSILON) {
        return false;
    }
    
    float f = 1.0f / a;
    hmm_vec3 s = HMM_SubtractVec3(rayOrigin, tri.v0);
    float u = f * HMM_DotVec3(s, h);
    
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    
    hmm_vec3 q = HMM_Cross(s, edge1);
    float v = f * HMM_DotVec3(rayDir, q);
    
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    
    float t = f * HMM_DotVec3(edge2, q);
    
    if (t > EPSILON) {
        if (outDistance) *outDistance = t;
        if (outPoint) *outPoint = HMM_AddVec3(rayOrigin, HMM_MultiplyVec3f(rayDir, t));
        return true;
    }
    
    return false;
}

RaycastHit ECS::RayMeshIntersect(EntityId entity, const hmm_vec3& rayOrigin, 
                                 const hmm_vec3& rayDir, float maxDistance) {
    RaycastHit hit;
    hit.distance = maxDistance;
    
    Collider* collider = GetCollider(entity);
    Transform* transform = GetTransform(entity);
    
    if (!collider || collider->type != ColliderType::Mesh || !transform) {
        return hit;
    }
    
    // NOTE: For now, assume mesh is not transformed (world space triangles)
    // This works for static ground meshes at the origin
    // For rotated/scaled meshes, we'd need proper matrix inverse
    
    // Check all triangles (already in world space)
    for (size_t i = 0; i < collider->triangles.size(); ++i) {
        // Transform triangle vertices to world space
        hmm_mat4 modelMatrix = transform->ModelMatrix();
        
        // Transform vertices
        hmm_vec4 v0_4 = HMM_MultiplyMat4ByVec4(modelMatrix, HMM_Vec4(collider->triangles[i].v0.X, collider->triangles[i].v0.Y, collider->triangles[i].v0.Z, 1.0f));
        hmm_vec4 v1_4 = HMM_MultiplyMat4ByVec4(modelMatrix, HMM_Vec4(collider->triangles[i].v1.X, collider->triangles[i].v1.Y, collider->triangles[i].v1.Z, 1.0f));
        hmm_vec4 v2_4 = HMM_MultiplyMat4ByVec4(modelMatrix, HMM_Vec4(collider->triangles[i].v2.X, collider->triangles[i].v2.Y, collider->triangles[i].v2.Z, 1.0f));
        
        CollisionTriangle worldTri;
        worldTri.v0 = HMM_Vec3(v0_4.X, v0_4.Y, v0_4.Z);
        worldTri.v1 = HMM_Vec3(v1_4.X, v1_4.Y, v1_4.Z);
        worldTri.v2 = HMM_Vec3(v2_4.X, v2_4.Y, v2_4.Z);
        
        // Recalculate normal in world space
        hmm_vec3 edge1 = HMM_SubtractVec3(worldTri.v1, worldTri.v0);
        hmm_vec3 edge2 = HMM_SubtractVec3(worldTri.v2, worldTri.v0);
        worldTri.normal = HMM_NormalizeVec3(HMM_Cross(edge1, edge2));
        
        float dist;
        hmm_vec3 point;
        
        if (RayTriangleIntersect(rayOrigin, rayDir, worldTri, &dist, &point)) {
            if (dist < hit.distance) {
                hit.hit = true;
                hit.entity = entity;
                hit.distance = dist;
                hit.triangleIndex = (int)i;
                hit.normal = worldTri.normal;
                hit.point = point;
            }
        }
    }
    
    return hit;
}

RaycastHit ECS::RaycastPhysics(const hmm_vec3& origin, const hmm_vec3& direction, 
                               float maxDistance, uint32_t layerMask) {
    RaycastHit closestHit;
    closestHit.distance = maxDistance;
    
    for (const auto& [entityId, collider] : colliders_) {
        // Layer filtering
        if ((collider.collisionLayer & layerMask) == 0) {
            continue;
        }
        
        Transform* t = GetTransform(entityId);
        if (!t) continue;
        
        RaycastHit hit;
        
        switch (collider.type) {
            case ColliderType::Plane: {
                float denom = HMM_DotVec3(direction, collider.planeNormal);
                if (fabsf(denom) > 0.0001f) {
                    float t_param = (collider.planeDistance - HMM_DotVec3(origin, collider.planeNormal)) / denom;
                    if (t_param >= 0.0f && t_param < closestHit.distance) {
                        hit.hit = true;
                        hit.entity = entityId;
                        hit.point = HMM_AddVec3(origin, HMM_MultiplyVec3f(direction, t_param));
                        hit.normal = collider.planeNormal;
                        hit.distance = t_param;
                        closestHit = hit;
                    }
                }
                break;
            }
            
            case ColliderType::Mesh: {
                hit = RayMeshIntersect(entityId, origin, direction, closestHit.distance);
                if (hit.hit && hit.distance < closestHit.distance) {
                    closestHit = hit;
                }
                break;
            }
            
            case ColliderType::Sphere: {
                hmm_vec3 oc = HMM_SubtractVec3(origin, t->GetWorldPosition());
                float a = HMM_DotVec3(direction, direction);
                float b = 2.0f * HMM_DotVec3(oc, direction);
                float c = HMM_DotVec3(oc, oc) - collider.radius * collider.radius;
                float discriminant = b * b - 4 * a * c;
                
                if (discriminant >= 0.0f) {
                    float t1 = (-b - sqrtf(discriminant)) / (2.0f * a);
                    if (t1 > 0.001f && t1 < closestHit.distance) {
                        hit.hit = true;
                        hit.entity = entityId;
                        hit.distance = t1;
                        hit.point = HMM_AddVec3(origin, HMM_MultiplyVec3f(direction, t1));
                        hit.normal = HMM_NormalizeVec3(HMM_SubtractVec3(hit.point, t->GetWorldPosition()));
                        closestHit = hit;
                    }
                }
                break;
            }
            
            default:
                break;
        }
    }
    
    return closestHit;
}

// Add this helper function before RaycastSelection:
bool ECS::RayBoxIntersect(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir,
                          const hmm_vec3& boxMin, const hmm_vec3& boxMax,
                          float* outDistance) {
    // AABB (Axis-Aligned Bounding Box) ray intersection using slab method
    hmm_vec3 invDir = HMM_Vec3(1.0f / rayDir.X, 1.0f / rayDir.Y, 1.0f / rayDir.Z);
    
    float t1 = (boxMin.X - rayOrigin.X) * invDir.X;
    float t2 = (boxMax.X - rayOrigin.X) * invDir.X;
    float t3 = (boxMin.Y - rayOrigin.Y) * invDir.Y;
    float t4 = (boxMax.Y - rayOrigin.Y) * invDir.Y;
    float t5 = (boxMin.Z - rayOrigin.Z) * invDir.Z;
    float t6 = (boxMax.Z - rayOrigin.Z) * invDir.Z;
    
    float tmin = fmaxf(fmaxf(fminf(t1, t2), fminf(t3, t4)), fminf(t5, t6));
    float tmax = fminf(fminf(fmaxf(t1, t2), fmaxf(t3, t4)), fmaxf(t5, t6));
    
    // If tmax < 0, ray is intersecting AABB, but the whole AABB is behind us
    if (tmax < 0.0f) {
        return false;
    }
    
    // If tmin > tmax, ray doesn't intersect AABB
    if (tmin > tmax) {
        return false;
    }
    
    // If tmin < 0, we are inside the box, use tmax instead
    float t = (tmin < 0.0f) ? tmax : tmin;
    
    if (outDistance) {
        *outDistance = t;
    }
    
    return true;
}

RaycastHit ECS::RaycastSelectionNew(const hmm_vec3& origin, const hmm_vec3& direction,
                                    float maxDistance) {
    RaycastHit closestHit;
    closestHit.distance = maxDistance;
    
    for (const auto& [entityId, selectable] : selectables_) {
        if (!selectable.canBeSelected) continue;
        
        Transform* t = GetTransform(entityId);
        if (!t) continue;
        
        RaycastHit hit;
        
        switch (selectable.volumeType) {
            case SelectionVolumeType::Sphere: {
                hmm_vec3 oc = HMM_SubtractVec3(origin, t->GetWorldPosition());
                float a = HMM_DotVec3(direction, direction);
                float b = 2.0f * HMM_DotVec3(oc, direction);
                float c = HMM_DotVec3(oc, oc) - selectable.boundingSphereRadius * selectable.boundingSphereRadius;
                float discriminant = b * b - 4 * a * c;
                
                if (discriminant >= 0.0f) {
                    float t1 = (-b - sqrtf(discriminant)) / (2.0f * a);
                    if (t1 > 0.001f && t1 < closestHit.distance) {
                        hit.hit = true;
                        hit.entity = entityId;
                        hit.distance = t1;
                        hit.point = HMM_AddVec3(origin, HMM_MultiplyVec3f(direction, t1));
                        hit.normal = HMM_NormalizeVec3(HMM_SubtractVec3(hit.point, t->GetWorldPosition()));
                        closestHit = hit;
                    }
                }
                break;
            }
            
            case SelectionVolumeType::Box: {
                // ADDED: Box selection volume support
                // Transform box bounds to world space
                hmm_vec3 worldMin = HMM_AddVec3(t->position, selectable.boundingBoxMin);
                hmm_vec3 worldMax = HMM_AddVec3(t->position, selectable.boundingBoxMax);
                
                float dist;
                if (RayBoxIntersect(origin, direction, worldMin, worldMax, &dist)) {
                    if (dist > 0.001f && dist < closestHit.distance) {
                        hit.hit = true;
                        hit.entity = entityId;
                        hit.distance = dist;
                        hit.point = HMM_AddVec3(origin, HMM_MultiplyVec3f(direction, dist));
                        
                        // Calculate normal based on which face was hit
                        hmm_vec3 localPoint = HMM_SubtractVec3(hit.point, t->position);
                        hmm_vec3 center = HMM_MultiplyVec3f(HMM_AddVec3(selectable.boundingBoxMin, selectable.boundingBoxMax), 0.5f);
                        hmm_vec3 toPoint = HMM_SubtractVec3(localPoint, center);
                        
                        // Determine which axis has the largest component (which face)
                        float absX = fabsf(toPoint.X);
                        float absY = fabsf(toPoint.Y);
                        float absZ = fabsf(toPoint.Z);
                        
                        if (absX > absY && absX > absZ) {
                            hit.normal = HMM_Vec3(toPoint.X > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
                        } else if (absY > absZ) {
                            hit.normal = HMM_Vec3(0.0f, toPoint.Y > 0 ? 1.0f : -1.0f, 0.0f);
                        } else {
                            hit.normal = HMM_Vec3(0.0f, 0.0f, toPoint.Z > 0 ? 1.0f : -1.0f);
                        }
                        
                        closestHit = hit;
                    }
                }
                break;
            }
            
            case SelectionVolumeType::Mesh: {
                if (selectable.useMeshColliderForPicking) {
                    hit = RayMeshIntersect(entityId, origin, direction, closestHit.distance);
                    if (hit.hit && hit.distance < closestHit.distance) {
                        closestHit = hit;
                    }
                }
                break;
            }
            
            default:
                break;
        }
    }
    
    return closestHit;
}
RaycastHit ECS::RaycastPlacement(const hmm_vec3 &origin, const hmm_vec3 &direction,
        float maxDistance) {
    // Use physics raycast for placement (checks colliders)
    return RaycastPhysics(origin, direction, maxDistance);
}