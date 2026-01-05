#include "EntityPlacement.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

EntityPlacement::EntityPlacement()
    : m_ecs(nullptr)
    , m_renderer(nullptr)
    , m_treeEntities(nullptr)
    , m_enemyEntities(nullptr)
    , m_lightEntities(nullptr)
    , m_ghostEntity(-1)
    , m_ghostMeshId(-1)
{
}

EntityPlacement::~EntityPlacement() {
    DestroyGhostPreview();
}

void EntityPlacement::Init(ECS* ecs, Renderer* renderer,
                           std::vector<EntityId>* treeEntities,
                           std::vector<EntityId>* enemyEntities,
                           std::vector<EntityId>* lightEntities) {
    m_ecs = ecs;
    m_renderer = renderer;
    m_treeEntities = treeEntities;
    m_enemyEntities = enemyEntities;
    m_lightEntities = lightEntities;
}

const ModelMetadata* EntityPlacement::GetModelMetadata(int meshId, int meshTreeId, int meshEnemyId) {
    if (meshId == meshTreeId) {
        return &ModelRegistry::TREE_MODEL;
    } else if (meshId == meshEnemyId) {
        return &ModelRegistry::ENEMY_MODEL;
    }
    return nullptr;
}

void EntityPlacement::CreateGhostPreview(int placementMeshId, int meshTreeId, int meshEnemyId) {
    DestroyGhostPreview();
    
    if (placementMeshId == -1) {
        // For lights, don't create a visual ghost (or create a simple indicator)
        printf("Ghost preview for light (no mesh)\n");
        return;
    }
    
    m_ghostEntity = m_ecs->CreateEntity();
    m_ghostMeshId = placementMeshId;
    
    Transform t;
    // FIXED: Start at world origin on the ground instead of far away
    t.position = HMM_Vec3(0.0f, -4.0f, 0.0f);  // Start at ground level (Y=-4 is ground plane)
    
    // Apply model metadata if available
    const ModelMetadata* metadata = GetModelMetadata(placementMeshId, meshTreeId, meshEnemyId);
    if (metadata) {
        t.pitch = metadata->defaultRotation.X;
        t.yaw = metadata->defaultRotation.Y;
        t.roll = metadata->defaultRotation.Z;
        t.scale = metadata->defaultScale;
        t.originOffset = metadata->originOffset;
        printf("  Applied metadata: scale=(%.2f, %.2f, %.2f), offset=(%.2f, %.2f, %.2f)\n",
               t.scale.X, t.scale.Y, t.scale.Z,
               t.originOffset.X, t.originOffset.Y, t.originOffset.Z);
    }
    
    m_ecs->AddTransform(m_ghostEntity, t);
    int instanceId = m_ecs->AddRenderable(m_ghostEntity, placementMeshId, *m_renderer);
    
    // Verify the instance was created
    if (instanceId < 0) {
        printf("ERROR: Failed to create ghost preview renderable! Instance ID: %d\n", instanceId);
    } else {
        printf("Created ghost preview entity %d with mesh %d (instance %d) at ground level\n", 
               m_ghostEntity, placementMeshId, instanceId);
    }
    
    // Force immediate sync so it appears immediately
    m_ecs->SyncToRenderer(*m_renderer);
    printf("  Ghost preview synced to renderer\n");
}

void EntityPlacement::UpdateGhostPreview(const hmm_vec3& position) {
    if (m_ghostEntity == -1) {
  //      printf("WARNING: UpdateGhostPreview called but ghost entity is -1\n");
        return;
    }
    
    Transform* t = m_ecs->GetTransform(m_ghostEntity);
    if (t) {
        t->position = position;
        // Debug output (only occasionally to avoid spam)
        static int updateCount = 0;
        if (updateCount++ % 60 == 0) {  // Print every 60 frames
            printf("Ghost preview at (%.2f, %.2f, %.2f)\n", position.X, position.Y, position.Z);
        }
    } else {
        printf("ERROR: Ghost entity %d has no Transform component!\n", m_ghostEntity);
    }
}

void EntityPlacement::DestroyGhostPreview() {
    if (m_ghostEntity != -1) {
        m_ecs->RemoveRenderable(m_ghostEntity, *m_renderer);
        m_ecs->DestroyEntity(m_ghostEntity);
        printf("Destroyed ghost preview entity %d\n", m_ghostEntity);
        m_ghostEntity = -1;
        m_ghostMeshId = -1;
    }
}

// FIXED: Changed signature to match header (pass by value, not reference)
void EntityPlacement::PlaceEntity(hmm_vec3 position, int placementMeshId, int meshTreeId, int meshEnemyId) {
    if (placementMeshId == meshTreeId) {
        // Spawn tree
        EntityId treeId = m_ecs->CreateEntity();
        Transform t;
        t.position = position;
        
        const ModelMetadata* treeMetadata = &ModelRegistry::TREE_MODEL;
        t.pitch = treeMetadata->defaultRotation.X;
        t.yaw = treeMetadata->defaultRotation.Y;
        t.roll = treeMetadata->defaultRotation.Z;
        t.scale = treeMetadata->defaultScale;
        t.originOffset = treeMetadata->originOffset;

        m_ecs->AddTransform(treeId, t);
        m_ecs->AddRenderable(treeId, meshTreeId, *m_renderer);

        // ADDED: Tree collider
        Collider treeCollider;
        treeCollider.type = ColliderType::Sphere;
        treeCollider.radius = 2.0f;
        treeCollider.isStatic = true;
        treeCollider.isTrigger = false;
        m_ecs->AddCollider(treeId, treeCollider);

        Selectable treeSel;
        treeSel.name = "Tree";
        treeSel.volumeType = SelectionVolumeType::Sphere;
        treeSel.boundingSphereRadius = 3.0f;
        treeSel.boundingRadius = 3.0f;
        treeSel.showWireframe = true;
        treeSel.canBeSelected = true;
        m_ecs->AddSelectable(treeId, treeSel);

        m_treeEntities->push_back(treeId);
        // FIXED: Corrected printf format string (removed extra %d)
        printf("Placed TREE at (%.2f, %.2f, %.2f) with entity ID %d\n", 
               position.X, position.Y, position.Z, treeId);
    }
    else if (placementMeshId == meshEnemyId) {
        // Spawn enemy
        EntityId enemyId = m_ecs->CreateEntity();
        Transform t;
        t.position = position;
        t.yaw = 0.0f;
        m_ecs->AddTransform(enemyId, t);

        AIController ai;
        ai.state = AIState::Wander;
        ai.stateTimer = 3.0f + ((float)rand() / RAND_MAX) * 4.0f;
        ai.wanderTarget = HMM_AddVec3(position, HMM_Vec3(((float)rand() / RAND_MAX - 0.5f) * 10.0f, 0.0f, ((float)rand() / RAND_MAX - 0.5f) * 10.0f));
        m_ecs->AddAI(enemyId, ai);

        Animator anim;
        anim.currentClip = -1;
        anim.time = 0.0f;
        m_ecs->AddAnimator(enemyId, anim);

        m_ecs->AddRenderable(enemyId, meshEnemyId, *m_renderer);

        // ADDED: Enemy collider
        Collider enemyCollider;
        enemyCollider.type = ColliderType::Sphere;
        enemyCollider.radius = 0.5f;
        enemyCollider.isStatic = false;
        enemyCollider.isTrigger = false;
        m_ecs->AddCollider(enemyId, enemyCollider);

        // ADDED: Enemy rigidbody
        Rigidbody enemyRb;
        enemyRb.mass = 50.0f;
        enemyRb.affectedByGravity = true;
        enemyRb.drag = 0.5f;
        enemyRb.bounciness = 0.0f;
        enemyRb.isKinematic = false;
        m_ecs->AddRigidbody(enemyId, enemyRb);

        Selectable enemySel;
        enemySel.name = "Enemy";
        enemySel.volumeType = SelectionVolumeType::Sphere;
        enemySel.boundingSphereRadius = 1.5f;
        enemySel.boundingRadius = 1.5f;
        enemySel.showWireframe = true;
        enemySel.canBeSelected = true;
        m_ecs->AddSelectable(enemyId, enemySel);

        m_enemyEntities->push_back(enemyId);
        printf("Placed ENEMY at (%.2f, %.2f, %.2f) with entity ID %d\n", 
               position.X, position.Y, position.Z, enemyId);
    }
}

void EntityPlacement::DeleteEntity(EntityId entityId) {
    printf("Deleting entity %d\n", entityId);
    
    m_ecs->RemoveRenderable(entityId, *m_renderer);
    m_ecs->DestroyEntity(entityId);
    
    auto treeIt = std::find(m_treeEntities->begin(), m_treeEntities->end(), entityId);
    if (treeIt != m_treeEntities->end()) {
        m_treeEntities->erase(treeIt);
        printf("  Removed from treeEntities\n");
    }
    
    auto enemyIt = std::find(m_enemyEntities->begin(), m_enemyEntities->end(), entityId);
    if (enemyIt != m_enemyEntities->end()) {
        m_enemyEntities->erase(enemyIt);
        printf("  Removed from enemyEntities\n");
    }
    
    auto lightIt = std::find(m_lightEntities->begin(), m_lightEntities->end(), entityId);
    if (lightIt != m_lightEntities->end()) {
        m_lightEntities->erase(lightIt);
        printf("  Removed from lightEntities\n");
    }
    
    printf("Entity deletion complete\n");
}