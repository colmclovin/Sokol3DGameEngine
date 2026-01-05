#include "WireframeManager.h"
#include "../../include/Model.h"
#include <cmath>
#include <vector>

WireframeManager::WireframeManager()
    : m_ecs(nullptr)
    , m_renderer(nullptr)
    , m_wireframeYellowMeshId(-1)
    , m_wireframeOrangeMeshId(-1)
    , ecs(nullptr)
    , renderer(nullptr)
    , wireframeYellowMeshId(-1)
    , wireframeOrangeMeshId(-1)
    , collisionSphereMeshId(-1)
    , collisionBoxMeshId(-1)
    , collisionPlaneMeshId(-1)
{
}

WireframeManager::~WireframeManager() {
    DestroyAllWireframes();
}

void WireframeManager::Init(ECS* ecsPtr, Renderer* rendererPtr) {
    this->ecs = ecsPtr;
    this->renderer = rendererPtr;
    this->m_ecs = ecsPtr;
    this->m_renderer = rendererPtr;
}

// ADDED: Implementation of CreateWireframeCube
void WireframeManager::CreateWireframeCube(float size, hmm_vec3 color, int& outMeshId) {
    Model3D mesh = {};
    mesh.vertex_count = 8;
    mesh.index_count = 24; // 12 edges * 2 vertices per edge
    mesh.vertices = (Vertex*)malloc(sizeof(Vertex) * 8);
    mesh.indices = (uint16_t*)malloc(sizeof(uint16_t) * 24);
    
    float halfSize = size * 0.5f;
    
    // 8 corners of the cube
    float positions[][3] = {
        {-halfSize, -halfSize, -halfSize},
        { halfSize, -halfSize, -halfSize},
        { halfSize,  halfSize, -halfSize},
        {-halfSize,  halfSize, -halfSize},
        {-halfSize, -halfSize,  halfSize},
        { halfSize, -halfSize,  halfSize},
        { halfSize,  halfSize,  halfSize},
        {-halfSize,  halfSize,  halfSize}
    };
    
    for (int i = 0; i < 8; i++) {
        mesh.vertices[i].pos[0] = positions[i][0];
        mesh.vertices[i].pos[1] = positions[i][1];
        mesh.vertices[i].pos[2] = positions[i][2];
        mesh.vertices[i].color[0] = color.X;
        mesh.vertices[i].color[1] = color.Y;
        mesh.vertices[i].color[2] = color.Z;
        mesh.vertices[i].color[3] = 1.0f; // Fully opaque
        mesh.vertices[i].normal[0] = 0.0f;
        mesh.vertices[i].normal[1] = 1.0f;
        mesh.vertices[i].normal[2] = 0.0f;
        mesh.vertices[i].uv[0] = 0.0f;
        mesh.vertices[i].uv[1] = 0.0f;
    }
    
    // 12 edges of the cube (as line pairs)
    uint16_t edges[] = {
        0, 1, 1, 2, 2, 3, 3, 0, // Bottom face
        4, 5, 5, 6, 6, 7, 7, 4, // Top face
        0, 4, 1, 5, 2, 6, 3, 7  // Vertical edges
    };
    memcpy(mesh.indices, edges, sizeof(edges));
    
    outMeshId = renderer->AddMesh(mesh);
    renderer->MarkMeshAsWireframe(outMeshId);
}

void WireframeManager::CreateWireframeMeshes() {
    CreateWireframeCube(2.0f, HMM_Vec3(1.0f, 1.0f, 0.0f), wireframeYellowMeshId);
    CreateWireframeCube(2.0f, HMM_Vec3(1.0f, 0.5f, 0.0f), wireframeOrangeMeshId);
    
    m_wireframeYellowMeshId = wireframeYellowMeshId;
    m_wireframeOrangeMeshId = wireframeOrangeMeshId;
    
    // ADDED: Create collision visualization meshes
    CreateCollisionSphereMesh();
    CreateCollisionBoxMesh();
    CreateCollisionPlaneMesh();
    
    printf("Yellow wireframe mesh ID (lines): %d\n", wireframeYellowMeshId);
    printf("Orange wireframe mesh ID (lines): %d\n", wireframeOrangeMeshId);
    printf("Collision sphere mesh ID: %d\n", collisionSphereMeshId);
    printf("Collision box mesh ID: %d\n", collisionBoxMeshId);
    printf("Collision plane mesh ID: %d\n", collisionPlaneMeshId);
}

void WireframeManager::CreateCollisionSphereMesh() {
    const int segments = 16;
    const int rings = 8;
    const int numVertices = (segments + 1) * (rings + 1);
    const int numIndices = segments * rings * 2;
    
    Model3D mesh = {};
    mesh.vertex_count = numVertices;
    mesh.index_count = numIndices;
    mesh.vertices = (Vertex*)malloc(sizeof(Vertex) * numVertices);
    mesh.indices = (uint16_t*)malloc(sizeof(uint16_t) * numIndices);
    
    // Create wireframe sphere (latitude/longitude lines)
    int vIdx = 0;
    for (int ring = 0; ring <= rings; ring++) {
        float phi = (float)ring / rings * HMM_PI;
        for (int seg = 0; seg <= segments; seg++) {
            float theta = (float)seg / segments * 2.0f * HMM_PI;
            
            float x = sinf(phi) * cosf(theta);
            float y = cosf(phi);
            float z = sinf(phi) * sinf(theta);
            
            mesh.vertices[vIdx].pos[0] = x;
            mesh.vertices[vIdx].pos[1] = y;
            mesh.vertices[vIdx].pos[2] = z;
            mesh.vertices[vIdx].color[0] = 1.0f;
            mesh.vertices[vIdx].color[1] = 0.0f;
            mesh.vertices[vIdx].color[2] = 0.0f;
            mesh.vertices[vIdx].color[3] = 0.3f; // Semi-transparent red
            mesh.vertices[vIdx].normal[0] = 0.0f;
            mesh.vertices[vIdx].normal[1] = 1.0f;
            mesh.vertices[vIdx].normal[2] = 0.0f;
            mesh.vertices[vIdx].uv[0] = 0.0f;
            mesh.vertices[vIdx].uv[1] = 0.0f;
            vIdx++;
        }
    }
    
    // Create line indices
    int idx = 0;
    for (int ring = 0; ring < rings; ring++) {
        for (int seg = 0; seg < segments; seg++) {
            int curr = ring * (segments + 1) + seg;
            int next = curr + segments + 1;
            
            // Vertical line
            mesh.indices[idx++] = curr;
            mesh.indices[idx++] = next;
        }
    }
    
    collisionSphereMeshId = renderer->AddMesh(mesh);
    renderer->MarkMeshAsWireframe(collisionSphereMeshId);
}

void WireframeManager::CreateCollisionBoxMesh() {
    Model3D mesh = {};
    mesh.vertex_count = 8;
    mesh.index_count = 24; // 12 edges * 2 vertices
    mesh.vertices = (Vertex*)malloc(sizeof(Vertex) * 8);
    mesh.indices = (uint16_t*)malloc(sizeof(uint16_t) * 24);
    
    // 8 corners of unit cube
    float positions[][3] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    };
    
    for (int i = 0; i < 8; i++) {
        mesh.vertices[i].pos[0] = positions[i][0];
        mesh.vertices[i].pos[1] = positions[i][1];
        mesh.vertices[i].pos[2] = positions[i][2];
        mesh.vertices[i].color[0] = 1.0f;
        mesh.vertices[i].color[1] = 0.0f;
        mesh.vertices[i].color[2] = 0.0f;
        mesh.vertices[i].color[3] = 0.3f; // Semi-transparent red
        mesh.vertices[i].normal[0] = 0.0f;
        mesh.vertices[i].normal[1] = 1.0f;
        mesh.vertices[i].normal[2] = 0.0f;
        mesh.vertices[i].uv[0] = 0.0f;
        mesh.vertices[i].uv[1] = 0.0f;
    }
    
    // 12 edges of the cube
    uint16_t edges[] = {
        0, 1, 1, 2, 2, 3, 3, 0, // Bottom face
        4, 5, 5, 6, 6, 7, 7, 4, // Top face
        0, 4, 1, 5, 2, 6, 3, 7  // Vertical edges
    };
    memcpy(mesh.indices, edges, sizeof(edges));
    
    collisionBoxMeshId = renderer->AddMesh(mesh);
    renderer->MarkMeshAsWireframe(collisionBoxMeshId);
}

void WireframeManager::CreateCollisionPlaneMesh() {
    const int gridSize = 10; // 10x10 grid
    const float cellSize = 2.0f;
    const int numLines = (gridSize + 1) * 2;
    const int numVertices = numLines * 2;
    
    Model3D mesh = {};
    mesh.vertex_count = numVertices;
    mesh.index_count = numVertices;
    mesh.vertices = (Vertex*)malloc(sizeof(Vertex) * numVertices);
    mesh.indices = (uint16_t*)malloc(sizeof(uint16_t) * numVertices);
    
    float halfSize = gridSize * cellSize * 0.5f;
    
    int vIdx = 0;
    // Horizontal lines
    for (int i = 0; i <= gridSize; i++) {
        float z = -halfSize + i * cellSize;
        mesh.vertices[vIdx].pos[0] = -halfSize;
        mesh.vertices[vIdx].pos[1] = 0.0f;
        mesh.vertices[vIdx].pos[2] = z;
        mesh.vertices[vIdx].color[0] = 1.0f;
        mesh.vertices[vIdx].color[1] = 0.0f;
        mesh.vertices[vIdx].color[2] = 0.0f;
        mesh.vertices[vIdx].color[3] = 0.3f;
        mesh.vertices[vIdx].normal[0] = 0.0f;
        mesh.vertices[vIdx].normal[1] = 1.0f;
        mesh.vertices[vIdx].normal[2] = 0.0f;
        mesh.vertices[vIdx].uv[0] = 0.0f;
        mesh.vertices[vIdx].uv[1] = 0.0f;
        mesh.indices[vIdx] = vIdx;
        vIdx++;
        
        mesh.vertices[vIdx].pos[0] = halfSize;
        mesh.vertices[vIdx].pos[1] = 0.0f;
        mesh.vertices[vIdx].pos[2] = z;
        mesh.vertices[vIdx].color[0] = 1.0f;
        mesh.vertices[vIdx].color[1] = 0.0f;
        mesh.vertices[vIdx].color[2] = 0.0f;
        mesh.vertices[vIdx].color[3] = 0.3f;
        mesh.vertices[vIdx].normal[0] = 0.0f;
        mesh.vertices[vIdx].normal[1] = 1.0f;
        mesh.vertices[vIdx].normal[2] = 0.0f;
        mesh.vertices[vIdx].uv[0] = 0.0f;
        mesh.vertices[vIdx].uv[1] = 0.0f;
        mesh.indices[vIdx] = vIdx;
        vIdx++;
    }
    
    // Vertical lines
    for (int i = 0; i <= gridSize; i++) {
        float x = -halfSize + i * cellSize;
        mesh.vertices[vIdx].pos[0] = x;
        mesh.vertices[vIdx].pos[1] = 0.0f;
        mesh.vertices[vIdx].pos[2] = -halfSize;
        mesh.vertices[vIdx].color[0] = 1.0f;
        mesh.vertices[vIdx].color[1] = 0.0f;
        mesh.vertices[vIdx].color[2] = 0.0f;
        mesh.vertices[vIdx].color[3] = 0.3f;
        mesh.vertices[vIdx].normal[0] = 0.0f;
        mesh.vertices[vIdx].normal[1] = 1.0f;
        mesh.vertices[vIdx].normal[2] = 0.0f;
        mesh.vertices[vIdx].uv[0] = 0.0f;
        mesh.vertices[vIdx].uv[1] = 0.0f;
        mesh.indices[vIdx] = vIdx;
        vIdx++;
        
        mesh.vertices[vIdx].pos[0] = x;
        mesh.vertices[vIdx].pos[1] = 0.0f;
        mesh.vertices[vIdx].pos[2] = halfSize;
        mesh.vertices[vIdx].color[0] = 1.0f;
        mesh.vertices[vIdx].color[1] = 0.0f;
        mesh.vertices[vIdx].color[2] = 0.0f;
        mesh.vertices[vIdx].color[3] = 0.3f;
        mesh.vertices[vIdx].normal[0] = 0.0f;
        mesh.vertices[vIdx].normal[1] = 1.0f;
        mesh.vertices[vIdx].normal[2] = 0.0f;
        mesh.vertices[vIdx].uv[0] = 0.0f;
        mesh.vertices[vIdx].uv[1] = 0.0f;
        mesh.indices[vIdx] = vIdx;
        vIdx++;
    }
    
    collisionPlaneMeshId = renderer->AddMesh(mesh);
    renderer->MarkMeshAsWireframe(collisionPlaneMeshId);
}

void WireframeManager::UpdateCollisionVisuals(bool showCollisions) {
    if (!showCollisions) {
        DestroyAllCollisionVisuals();
        return;
    }
    
    // Create or update collision visuals for all entities with colliders
    const auto& colliders = ecs->GetColliders();
    for (const auto& [entityId, collider] : colliders) {
        CreateOrUpdateCollisionVisual(entityId);
    }
    
    // Remove visuals for entities that no longer have colliders
    std::vector<EntityId> toRemove;
    for (const auto& [entityId, instanceId] : activeCollisionVisuals) {
        if (colliders.find(entityId) == colliders.end()) {
            toRemove.push_back(entityId);
        }
    }
    for (EntityId id : toRemove) {
        DestroyCollisionVisual(id);
    }
}

void WireframeManager::CreateOrUpdateCollisionVisual(EntityId entity) {
    Collider* collider = ecs->GetCollider(entity);
    Transform* transform = ecs->GetTransform(entity);
    if (!collider || !transform) return;
    
    // Destroy existing visual
    DestroyCollisionVisual(entity);
    
    int meshId = -1;
    hmm_mat4 visualMatrix;
    
    switch (collider->type) {
        case ColliderType::Sphere:
            meshId = collisionSphereMeshId;
            {
                // Create transform matrix for sphere
                hmm_mat4 scale = HMM_Scale(HMM_Vec3(collider->radius * 2.0f, collider->radius * 2.0f, collider->radius * 2.0f));
                hmm_mat4 translation = HMM_Translate(transform->position);
                visualMatrix = HMM_MultiplyMat4(translation, scale);
            }
            break;
            
        case ColliderType::Box:
            meshId = collisionBoxMeshId;
            {
                // Create transform matrix for box using half extents
                hmm_vec3 fullSize = HMM_Vec3(
                    collider->boxHalfExtents.X * 2.0f,
                    collider->boxHalfExtents.Y * 2.0f,
                    collider->boxHalfExtents.Z * 2.0f
                );
                hmm_mat4 scale = HMM_Scale(fullSize);
                hmm_mat4 translation = HMM_Translate(transform->position);
                visualMatrix = HMM_MultiplyMat4(translation, scale);
            }
            break;
            
        case ColliderType::Plane:
            meshId = collisionPlaneMeshId;
            {
                // Create transform matrix for plane
                hmm_mat4 scale = HMM_Scale(HMM_Vec3(1.0f, 1.0f, 1.0f));
                hmm_vec3 planePos = HMM_Vec3(0.0f, collider->planeDistance, 0.0f);
                hmm_mat4 translation = HMM_Translate(planePos);
                visualMatrix = HMM_MultiplyMat4(translation, scale);
            }
            break;
            
        default:
            return;
    }
    
    if (meshId >= 0) {
        int instanceId = renderer->AddInstance(meshId, visualMatrix);
        activeCollisionVisuals[entity] = instanceId;
    }
}

void WireframeManager::DestroyCollisionVisual(EntityId entity) {
    auto it = activeCollisionVisuals.find(entity);
    if (it != activeCollisionVisuals.end()) {
        renderer->RemoveInstance(it->second);
        activeCollisionVisuals.erase(it);
    }
}

void WireframeManager::DestroyAllCollisionVisuals() {
    for (const auto& [entityId, instanceId] : activeCollisionVisuals) {
        renderer->RemoveInstance(instanceId);
    }
    activeCollisionVisuals.clear();
}

void WireframeManager::CreateOrUpdateWireframe(EntityId entityId, bool isSelected) {
    Selectable* sel = m_ecs->GetSelectable(entityId);
    Transform* t = m_ecs->GetTransform(entityId);
    if (!sel || !t) {
        printf("WARNING: Entity %d missing Selectable or Transform component\n", entityId);
        return;
    }
    
    EntityId wireframeEntity = -1;
    auto it = m_entityWireframes.find(entityId);
    
    if (it == m_entityWireframes.end()) {
        // Create new wireframe
        wireframeEntity = m_ecs->CreateEntity();
        m_entityWireframes[entityId] = wireframeEntity;
        
        Transform wireTransform;
        wireTransform.position = t->position;
        m_ecs->AddTransform(wireframeEntity, wireTransform);
        
        int meshId = isSelected ? m_wireframeOrangeMeshId : m_wireframeYellowMeshId;
        m_ecs->AddRenderable(wireframeEntity, meshId, *m_renderer);
        
        printf("Created wireframe entity %d for entity %d (mesh: %d, selected: %d)\n", 
               wireframeEntity, entityId, meshId, isSelected);
    } else {
        wireframeEntity = it->second;
    }
    
    // Update wireframe transform
    Transform* wireTransform = m_ecs->GetTransform(wireframeEntity);
    if (wireTransform) {
        wireTransform->position = t->position;
        
        // Scale wireframe to match bounding sphere
        float size = sel->boundingRadius * 2.0f * 1.15f;  // Slightly larger than bounding radius
        hmm_mat4 translation = HMM_Translate(t->position);
        hmm_mat4 scale = HMM_Scale(HMM_Vec3(size / 2.0f, size / 2.0f, size / 2.0f));
        wireTransform->customMatrix = HMM_MultiplyMat4(translation, scale);
        wireTransform->useCustomMatrix = true;
        
        // Debug: Print transform info
        static int debugCounter = 0;
        if (debugCounter++ < 5) {  // Only print first 5 to avoid spam
            printf("  Wireframe %d: pos=(%.1f,%.1f,%.1f), size=%.1f, radius=%.1f\n",
                   entityId, t->position.X, t->position.Y, t->position.Z, size, sel->boundingRadius);
        }
    }
    
    // Update mesh if selection state changed
    int currentMeshId = m_ecs->GetMeshId(wireframeEntity);
    int desiredMeshId = isSelected ? m_wireframeOrangeMeshId : m_wireframeYellowMeshId;
    if (currentMeshId != desiredMeshId) {
        printf("Updating wireframe %d mesh from %d to %d (selected: %d)\n", 
               entityId, currentMeshId, desiredMeshId, isSelected);
        m_ecs->RemoveRenderable(wireframeEntity, *m_renderer);
        m_ecs->AddRenderable(wireframeEntity, desiredMeshId, *m_renderer);
    }
}

void WireframeManager::DestroyAllWireframes() {
    for (auto& [entityId, wireframeEntity] : m_entityWireframes) {
        printf("  Destroying wireframe entity %d for entity %d\n", wireframeEntity, entityId);
        m_ecs->RemoveRenderable(wireframeEntity, *m_renderer);
        m_ecs->DestroyEntity(wireframeEntity);
    }
    m_entityWireframes.clear();
}

void WireframeManager::DestroyWireframe(EntityId entityId) {
    auto it = m_entityWireframes.find(entityId);
    if (it != m_entityWireframes.end()) {
        EntityId wireframeEntity = it->second;
        printf("Destroying wireframe entity %d for entity %d\n", wireframeEntity, entityId);
        m_ecs->RemoveRenderable(wireframeEntity, *m_renderer);
        m_ecs->DestroyEntity(wireframeEntity);
        m_entityWireframes.erase(it);
    }
}

void WireframeManager::UpdateWireframes(EntityId selectedEntity, bool forceUpdate) {
    const auto& selectables = m_ecs->GetSelectables();
    
    for (const auto& [entityId, selectable] : selectables) {
        bool isSelected = (entityId == selectedEntity);
        
        if (forceUpdate) {
            auto it = m_entityWireframes.find(entityId);
            if (it != m_entityWireframes.end()) {
                EntityId oldWireframe = it->second;
                m_ecs->RemoveRenderable(oldWireframe, *m_renderer);
                m_ecs->DestroyEntity(oldWireframe);
                m_entityWireframes.erase(it);
                printf("  Force-removed wireframe for entity %d\n", entityId);
            }
        }
        
        CreateOrUpdateWireframe(entityId, isSelected);
    }
}