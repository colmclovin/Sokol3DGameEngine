#pragma once
#include "../../External/HandmadeMath.h"
#include "../Game/ECS.h"
#include "../Renderer/Renderer.h"
#include "../Model/ModelMetadata.h"
#include <vector>

class EntityPlacement {
public:
    EntityPlacement();
    ~EntityPlacement();
    
    void Init(ECS* ecs, Renderer* renderer, 
              std::vector<EntityId>* treeEntities,
              std::vector<EntityId>* enemyEntities,
              std::vector<EntityId>* lightEntities);
    
    // Ghost preview management
    void CreateGhostPreview(int placementMeshId, int meshTreeId, int meshEnemyId);
    void UpdateGhostPreview(const hmm_vec3& position);
    void DestroyGhostPreview();
    EntityId GetGhostEntity() const { return m_ghostEntity; }
    
    // Entity placement
    void PlaceEntity(hmm_vec3 position, int placementMeshId, int meshTreeId, int meshEnemyId);
    void DeleteEntity(EntityId entityId);
    
private:
    ECS* m_ecs;
    Renderer* m_renderer;
    std::vector<EntityId>* m_treeEntities;
    std::vector<EntityId>* m_enemyEntities;
    std::vector<EntityId>* m_lightEntities;
    
    EntityId m_ghostEntity;
    int m_ghostMeshId;
    
    // Helper to get model metadata based on mesh ID
    const ModelMetadata* GetModelMetadata(int meshId, int meshTreeId, int meshEnemyId);
};