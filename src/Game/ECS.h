#pragma once

#include "../../External/HandmadeMath.h"
#include "../Renderer/Renderer.h"
#include "Components.h"
#include "../../include/Model.h"
#include <unordered_map>
#include <vector>
#include <optional>

using EntityId = int;

// ============================================================================
// RAYCAST HIT RESULT - Declared BEFORE ECS class
// ============================================================================
struct RaycastHit {
    bool hit = false;
    EntityId entity = -1;
    hmm_vec3 point{0.0f, 0.0f, 0.0f};
    hmm_vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    int triangleIndex = -1;
};

// ============================================================================
// COLLISION INFO - Declared BEFORE ECS class
// ============================================================================
struct CollisionInfo {
    EntityId entityA;
    EntityId entityB;
    hmm_vec3 normal;
    float penetration;
    hmm_vec3 contactPoint;
};

// ============================================================================
// ECS CLASS
// ============================================================================
class ECS {
public:
    ECS();
    ~ECS();

    // Entity lifecycle
    EntityId CreateEntity();
    void DestroyEntity(EntityId id);

    // Components
    void AddTransform(EntityId id, const Transform& t);
    bool HasTransform(EntityId id) const;
    Transform* GetTransform(EntityId id);

    void AddRigidbody(EntityId id, const Rigidbody& rb);
    Rigidbody* GetRigidbody(EntityId id);
    
    void AddCollider(EntityId id, const Collider& col);
    Collider* GetCollider(EntityId id);
    bool HasCollider(EntityId id) const;

    void AddAI(EntityId id, const AIController& ai);
    AIController* GetAI(EntityId id);

    void AddAnimator(EntityId id, const Animator& a);
    Animator* GetAnimator(EntityId id);
    
    void AddBillboard(EntityId id, const Billboard& b);
    Billboard* GetBillboard(EntityId id);
    bool HasBillboard(EntityId id) const;
    
    void AddScreenSpace(EntityId id, const ScreenSpace& ss);
    ScreenSpace* GetScreenSpace(EntityId id);
    bool HasScreenSpace(EntityId id) const;

    void AddLight(EntityId entity, const Light& light);
    Light* GetLight(EntityId entity);
    void RemoveLight(EntityId entity);

    void AddSelectable(EntityId id, const Selectable& sel);
    Selectable* GetSelectable(EntityId id);
    bool HasSelectable(EntityId id) const;

    // Rendering linkage
    int AddRenderable(EntityId id, int meshId, Renderer& renderer);
    bool HasRenderable(EntityId id) const;
    int GetInstanceId(EntityId id) const;
    int GetMeshId(EntityId id) const;
    void RemoveRenderable(EntityId id, Renderer& renderer);

    // Systems
    void UpdateAI(float dt);
    void UpdatePhysics(float dt);
    void UpdateAnimation(float dt);
    void UpdateBillboards(const hmm_vec3& cameraPosition);
    void UpdateScreenSpace(float screenWidth, float screenHeight);
    void UpdateCollisions(float dt);
    bool CheckCollision(EntityId a, EntityId b, CollisionInfo* outInfo = nullptr);

    // ========================================================================
    // NEW RAYCAST SYSTEM
    // ========================================================================
    
    // Physics raycast (checks colliders only)
    RaycastHit RaycastPhysics(const hmm_vec3& origin, const hmm_vec3& direction, 
                              float maxDistance = 1000.0f, uint32_t layerMask = 0xFFFFFFFF);
    
    // Selection raycast (checks selectables) - NEW VERSION
    RaycastHit RaycastSelectionNew(const hmm_vec3& origin, const hmm_vec3& direction,
                                   float maxDistance = 1000.0f);
    
    // Placement raycast (finds surface for entity placement)
    RaycastHit RaycastPlacement(const hmm_vec3& origin, const hmm_vec3& direction,
                                float maxDistance = 1000.0f);
    
    // ========================================================================
    // COLLIDER CREATION HELPERS
    // ========================================================================
    
    // Create mesh collider from model data
    void CreateMeshCollider(EntityId entity, const Model3D& model);
    
    // Create plane collider (infinite ground)
    void CreatePlaneCollider(EntityId entity, const hmm_vec3& normal, float distance);
    
    // ========================================================================
    // LEGACY COMPATIBILITY (kept for existing code)
    // ========================================================================
    
    EntityId RaycastSelection(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float maxDistance = 1000.0f);
    hmm_vec3 GetPlacementPosition(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float distance = 10.0f);

    // Sync transforms to renderer
    void SyncToRenderer(Renderer& renderer);
    std::vector<EntityId> GetScreenSpaceEntities() const;
    std::vector<EntityId> AllEntities() const;

    const std::unordered_map<EntityId, Light>& GetLights() const { return lights_; }
    const std::unordered_map<EntityId, Transform>& GetTransforms() const { return transforms_; }
    const std::unordered_map<EntityId, Selectable>& GetSelectables() const { return selectables_; }
    const std::unordered_map<EntityId, Collider>& GetColliders() const { return colliders_; }
    const std::unordered_map<EntityId, Rigidbody>& GetRigidbodies() const { return rigidbodies_; } // ADDED

private:
    EntityId nextId_ = 1;
    std::vector<EntityId> alive_;

    std::unordered_map<EntityId, Transform> transforms_;
    std::unordered_map<EntityId, Rigidbody> rigidbodies_;
    std::unordered_map<EntityId, Collider> colliders_;
    std::unordered_map<EntityId, AIController> ai_controllers_;
    std::unordered_map<EntityId, Animator> animators_;
    std::unordered_map<EntityId, Billboard> billboards_;
    std::unordered_map<EntityId, ScreenSpace> screen_spaces_;
    std::unordered_map<EntityId, Light> lights_;
    std::unordered_map<EntityId, Selectable> selectables_;

    std::unordered_map<EntityId, int> mesh_for_entity_;
    std::unordered_map<EntityId, int> instance_for_entity_;
    
    // Collision helpers
    bool SphereVsSphere(const hmm_vec3& posA, float radiusA, const hmm_vec3& posB, float radiusB, CollisionInfo* outInfo);
    bool SphereVsBox(const hmm_vec3& spherePos, float radius, const hmm_vec3& boxPos, const hmm_vec3& boxHalfExtents, CollisionInfo* outInfo);
    bool SphereVsPlane(const hmm_vec3& spherePos, float radius, const hmm_vec3& planeNormal, float planeDistance, CollisionInfo* outInfo);
    void ResolveCollision(EntityId a, EntityId b, const CollisionInfo& info);
    
    // NEW: Ray-triangle intersection
    bool RayTriangleIntersect(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir,
                             const CollisionTriangle& tri, float* outDistance, hmm_vec3* outPoint);
    
    // NEW: Ray-mesh intersection
    RaycastHit RayMeshIntersect(EntityId entity, const hmm_vec3& rayOrigin, 
                                const hmm_vec3& rayDir, float maxDistance);
    
    // ADDED: Ray-box intersection (for AABB selection volumes)
    bool RayBoxIntersect(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir,
                        const hmm_vec3& boxMin, const hmm_vec3& boxMax,
                        float* outDistance);
};