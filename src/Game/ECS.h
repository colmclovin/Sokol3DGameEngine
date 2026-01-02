#pragma once

#include "../../External/HandmadeMath.h"
#include "../Renderer/Renderer.h"
#include "Components.h"
#include "../../include/Model.h" // Include for Light struct definition
#include <unordered_map>
#include <vector>
#include <optional>

// Lightweight data-driven ECS (sparse maps per component).
// Not a high-performance packed ECS, but simple and clear to extend.

using EntityId = int;

// Collision info structure
struct CollisionInfo {
    EntityId entityA;
    EntityId entityB;
    hmm_vec3 normal;      // Collision normal (from A to B)
    float penetration;    // How deep the collision is
    hmm_vec3 contactPoint; // Point of contact
};

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
    // Adds a renderable component (meshId) and creates instance via renderer.
    // Returns instance id or -1 on failure.
    int AddRenderable(EntityId id, int meshId, Renderer& renderer);
    bool HasRenderable(EntityId id) const;
    int GetInstanceId(EntityId id) const;
    int GetMeshId(EntityId id) const;
    void RemoveRenderable(EntityId id, Renderer& renderer);

    // Systems (data-driven)
    void UpdateAI(float dt);
    void UpdatePhysics(float dt);
    void UpdateAnimation(float dt);
    void UpdateBillboards(const hmm_vec3& cameraPosition);
    void UpdateScreenSpace(float screenWidth, float screenHeight);
    
    // Collision detection
    void UpdateCollisions(float dt);
    bool CheckCollision(EntityId a, EntityId b, CollisionInfo* outInfo = nullptr);

    // Selection system
    EntityId RaycastSelection(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float maxDistance = 1000.0f);
    
    // Placement system for edit mode
    hmm_vec3 GetPlacementPosition(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float distance = 10.0f);

    // Sync transforms to renderer (call after systems)
    void SyncToRenderer(Renderer& renderer);
    
    // Get all screen-space entities for separate rendering
    std::vector<EntityId> GetScreenSpaceEntities() const;

    // Iterate helper
    std::vector<EntityId> AllEntities() const;

    const std::unordered_map<EntityId, Light>& GetLights() const { return lights_; }
    const std::unordered_map<EntityId, Transform>& GetTransforms() const { return transforms_; }
    const std::unordered_map<EntityId, Selectable>& GetSelectables() const { return selectables_; }
    const std::unordered_map<EntityId, Collider>& GetColliders() const { return colliders_; }

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

    // rendering maps
    std::unordered_map<EntityId, int> mesh_for_entity_;
    std::unordered_map<EntityId, int> instance_for_entity_;
    
    // Collision helpers
    bool SphereVsSphere(const hmm_vec3& posA, float radiusA, const hmm_vec3& posB, float radiusB, CollisionInfo* outInfo);
    bool SphereVsBox(const hmm_vec3& spherePos, float radius, const hmm_vec3& boxPos, const hmm_vec3& boxHalfExtents, CollisionInfo* outInfo);
    void ResolveCollision(EntityId a, EntityId b, const CollisionInfo& info);
};