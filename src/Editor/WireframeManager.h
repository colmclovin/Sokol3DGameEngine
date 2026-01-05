#pragma once

#include "../Game/ECS.h"
#include "../Renderer/Renderer.h"
#include <unordered_map>

class WireframeManager {
public:
    WireframeManager();
    ~WireframeManager();
    
    void Init(ECS* ecs, Renderer* renderer);
    void CreateWireframeMeshes();
    void UpdateWireframes(EntityId selectedEntity, bool forceUpdate);
    void CreateOrUpdateWireframe(EntityId entity, bool isSelected);
    void DestroyWireframe(EntityId entity);
    void DestroyAllWireframes();
    
    // ADDED: Collision visualization
    void UpdateCollisionVisuals(bool showCollisions);
    void CreateOrUpdateCollisionVisual(EntityId entity);
    void DestroyCollisionVisual(EntityId entity);
    void DestroyAllCollisionVisuals();

private:
    ECS* m_ecs = nullptr;
    Renderer* m_renderer = nullptr;
    
    // Old-style member names (used in existing code)
    ECS* ecs = nullptr;
    Renderer* renderer = nullptr;
    
    int m_wireframeYellowMeshId = -1;
    int m_wireframeOrangeMeshId = -1;
    
    int wireframeYellowMeshId = -1;
    int wireframeOrangeMeshId = -1;
    
    // ADDED: Collision visualization meshes
    int collisionSphereMeshId = -1;
    int collisionBoxMeshId = -1;
    int collisionPlaneMeshId = -1;
    
    // ADDED: Track collision visuals separately
    std::unordered_map<EntityId, int> activeCollisionVisuals;
    
    // ADDED: Track wireframes for entities
    std::unordered_map<EntityId, EntityId> m_entityWireframes;
    
    void CreateWireframeCube(float size, hmm_vec3 color, int& outMeshId);
    
    // ADDED: Create collision shape meshes
    void CreateCollisionSphereMesh();
    void CreateCollisionBoxMesh();
    void CreateCollisionPlaneMesh();
};

