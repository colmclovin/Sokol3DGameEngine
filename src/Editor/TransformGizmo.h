#pragma once
#include "../../External/HandmadeMath.h"
#include "../Game/ECS.h"
#include "../Renderer/Renderer.h"

enum class GizmoMode {
    None,
    Translate,
    Rotate,
    Scale
};

enum class GizmoAxis {
    None = 0,
    X = 1,
    Y = 2,
    Z = 3
};

class TransformGizmo {
public:
    TransformGizmo();
    ~TransformGizmo();
    
    void Init(ECS* ecs, Renderer* renderer);
    void CreateGizmoMeshes();
    
    // Update gizmo for selected entity
    void UpdateGizmo(EntityId selectedEntity, const hmm_vec3& cameraPos);
    
    // Handle mouse interactions
    bool HandleMouseDown(EntityId selectedEntity, const hmm_vec3& rayOrigin, const hmm_vec3& rayDir);
    void HandleMouseDrag(EntityId selectedEntity, const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float deltaX, float deltaY);
    void HandleMouseUp();
    
    // Render gizmo
    void RenderGizmo(const hmm_mat4& viewProj);
    
    // Mode control
    void SetMode(GizmoMode mode) { m_mode = mode; }
    GizmoMode GetMode() const { return m_mode; }
    
    // Check if gizmo is active
    bool IsActive() const { return m_mode != GizmoMode::None; }
    bool IsDragging() const { return m_isDragging; } // ADDED
    
    // Destroy gizmo entities
    void DestroyGizmo();
    
private:
    ECS* m_ecs;
    Renderer* m_renderer;
    
    GizmoMode m_mode;
    GizmoAxis m_activeAxis;
    bool m_isDragging;
    
    // Gizmo mesh IDs
    int m_arrowXMeshId;
    int m_arrowYMeshId;
    int m_arrowZMeshId;
    
    // Gizmo entity IDs
    EntityId m_gizmoXEntity;
    EntityId m_gizmoYEntity;
    EntityId m_gizmoZEntity;
    
    // Drag state
    hmm_vec3 m_dragStartPos;
    hmm_vec3 m_dragStartScale;
    float m_dragStartRotation;
    hmm_vec3 m_dragStartPoint;
    hmm_vec3 m_dragPlaneNormal;
    
    // Helper functions
    Model3D CreateArrowMesh(float length, float r, float g, float b);
    GizmoAxis RaycastGizmo(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, const hmm_vec3& gizmoPosition);
    float RayToLineDistance(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, 
                            const hmm_vec3& lineOrigin, const hmm_vec3& lineDir, float lineLength);
    bool IntersectRayPlane(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir,
                           const hmm_vec3& planePoint, const hmm_vec3& planeNormal, float& t);
};