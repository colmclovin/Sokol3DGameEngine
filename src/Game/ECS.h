#pragma once

#include "../../External/HandmadeMath.h"
#include "../Renderer/Renderer.h"
#include "Components.h"
#include <unordered_map>
#include <vector>
#include <optional>

// Lightweight data-driven ECS (sparse maps per component).
// Not a high-performance packed ECS, but simple and clear to extend.

using EntityId = int;

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

    void AddAI(EntityId id, const AIController& ai);
    AIController* GetAI(EntityId id);

    void AddAnimator(EntityId id, const Animator& a);
    Animator* GetAnimator(EntityId id);

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

    // Sync transforms to renderer (call after systems)
    void SyncToRenderer(Renderer& renderer);

    // Iterate helper
    std::vector<EntityId> AllEntities() const;

private:
    EntityId nextId_ = 1;
    std::vector<EntityId> alive_;

    std::unordered_map<EntityId, Transform> transforms_;
    std::unordered_map<EntityId, Rigidbody> rigidbodies_;
    std::unordered_map<EntityId, AIController> ai_controllers_;
    std::unordered_map<EntityId, Animator> animators_;

    // rendering maps
    std::unordered_map<EntityId, int> mesh_for_entity_;
    std::unordered_map<EntityId, int> instance_for_entity_;
};