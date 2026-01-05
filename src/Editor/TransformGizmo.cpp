#include "TransformGizmo.h"
#include <cstdio>
#include <cmath>

TransformGizmo::TransformGizmo()
    : m_ecs(nullptr)
    , m_renderer(nullptr)
    , m_mode(GizmoMode::None)
    , m_activeAxis(GizmoAxis::None)
    , m_isDragging(false)
    , m_arrowXMeshId(-1)
    , m_arrowYMeshId(-1)
    , m_arrowZMeshId(-1)
    , m_gizmoXEntity(-1)
    , m_gizmoYEntity(-1)
    , m_gizmoZEntity(-1)
{
}

TransformGizmo::~TransformGizmo() {
    DestroyGizmo();
}

void TransformGizmo::Init(ECS* ecs, Renderer* renderer) {
    m_ecs = ecs;
    m_renderer = renderer;
    printf("TransformGizmo initialized\n");
}

Model3D TransformGizmo::CreateArrowMesh(float length, float r, float g, float b) {
    Model3D arrow = {};
    
    // Create much thicker arrows with many more parallel lines
    const int linesPerAxis = 9; // More lines for better visibility
    const int arrowHeadLines = 8; // More lines for arrow head
    const int totalLines = linesPerAxis + arrowHeadLines;
    
    arrow.vertex_count = totalLines * 2;
    arrow.vertices = (Vertex*)malloc(arrow.vertex_count * sizeof(Vertex));
    
    arrow.index_count = totalLines * 2;
    arrow.indices = (uint16_t*)malloc(arrow.index_count * sizeof(uint16_t));
    
    int vIdx = 0;
    int iIdx = 0;
    
    // Create multiple parallel lines in a grid pattern for thickness
    float offset = 0.04f; // Larger offset for more visible thickness
    
    for (int line = 0; line < linesPerAxis; ++line) {
        float yOffset = 0.0f;
        float zOffset = 0.0f;
        
        // Create a 3x3 grid of lines
        int row = line / 3;
        int col = line % 3;
        yOffset = (row - 1) * offset;
        zOffset = (col - 1) * offset;
        
        // Start point
        arrow.vertices[vIdx].pos[0] = 0.0f;
        arrow.vertices[vIdx].pos[1] = yOffset;
        arrow.vertices[vIdx].pos[2] = zOffset;
        arrow.vertices[vIdx].normal[0] = 0.0f;
        arrow.vertices[vIdx].normal[1] = 1.0f;
        arrow.vertices[vIdx].normal[2] = 0.0f;
        arrow.vertices[vIdx].uv[0] = 0.0f;
        arrow.vertices[vIdx].uv[1] = 0.0f;
        arrow.vertices[vIdx].color[0] = r;
        arrow.vertices[vIdx].color[1] = g;
        arrow.vertices[vIdx].color[2] = b;
        arrow.vertices[vIdx].color[3] = 1.0f;
        
        // End point
        arrow.vertices[vIdx + 1].pos[0] = length;
        arrow.vertices[vIdx + 1].pos[1] = yOffset;
        arrow.vertices[vIdx + 1].pos[2] = zOffset;
        arrow.vertices[vIdx + 1].normal[0] = 0.0f;
        arrow.vertices[vIdx + 1].normal[1] = 1.0f;
        arrow.vertices[vIdx + 1].normal[2] = 0.0f;
        arrow.vertices[vIdx + 1].uv[0] = 0.0f;
        arrow.vertices[vIdx + 1].uv[1] = 0.0f;
        arrow.vertices[vIdx + 1].color[0] = r;
        arrow.vertices[vIdx + 1].color[1] = g;
        arrow.vertices[vIdx + 1].color[2] = b;
        arrow.vertices[vIdx + 1].color[3] = 1.0f;
        
        arrow.indices[iIdx] = vIdx;
        arrow.indices[iIdx + 1] = vIdx + 1;
        
        vIdx += 2;
        iIdx += 2;
    }
    
    // Create arrow head (cone shape)
    float arrowHeadStart = length * 0.8f; // Start earlier for larger head
    float arrowHeadSize = length * 0.2f;
    
    for (int i = 0; i < arrowHeadLines; ++i) {
        float angle = (float)i * (3.14159f * 2.0f / arrowHeadLines);
        float perpY = cosf(angle) * arrowHeadSize * 0.5f; // Larger cone
        float perpZ = sinf(angle) * arrowHeadSize * 0.5f;
        
        // From cone base to tip
        arrow.vertices[vIdx].pos[0] = arrowHeadStart;
        arrow.vertices[vIdx].pos[1] = perpY;
        arrow.vertices[vIdx].pos[2] = perpZ;
        arrow.vertices[vIdx].normal[0] = 0.0f;
        arrow.vertices[vIdx].normal[1] = 1.0f;
        arrow.vertices[vIdx].normal[2] = 0.0f;
        arrow.vertices[vIdx].uv[0] = 0.0f;
        arrow.vertices[vIdx].uv[1] = 0.0f;
        arrow.vertices[vIdx].color[0] = r;
        arrow.vertices[vIdx].color[1] = g;
        arrow.vertices[vIdx].color[2] = b;
        arrow.vertices[vIdx].color[3] = 1.0f;
        
        arrow.vertices[vIdx + 1].pos[0] = length;
        arrow.vertices[vIdx + 1].pos[1] = 0.0f;
        arrow.vertices[vIdx + 1].pos[2] = 0.0f;
        arrow.vertices[vIdx + 1].normal[0] = 0.0f;
        arrow.vertices[vIdx + 1].normal[1] = 1.0f;
        arrow.vertices[vIdx + 1].normal[2] = 0.0f;
        arrow.vertices[vIdx + 1].uv[0] = 0.0f;
        arrow.vertices[vIdx + 1].uv[1] = 0.0f;
        arrow.vertices[vIdx + 1].color[0] = r;
        arrow.vertices[vIdx + 1].color[1] = g;
        arrow.vertices[vIdx + 1].color[2] = b;
        arrow.vertices[vIdx + 1].color[3] = 1.0f;
        
        arrow.indices[iIdx] = vIdx;
        arrow.indices[iIdx + 1] = vIdx + 1;
        
        vIdx += 2;
        iIdx += 2;
    }
    
    arrow.has_texture = false;
    arrow.texture_data = nullptr;
    
    printf("Created arrow mesh with %d vertices, %d indices (color: %.1f, %.1f, %.1f)\n", 
           arrow.vertex_count, arrow.index_count, r, g, b);
    
    return arrow;
}

void TransformGizmo::CreateGizmoMeshes() {
    printf("Creating gizmo meshes...\n");
    
    // Create arrow meshes for each axis (longer - 8 units for better visibility)
    Model3D arrowX = CreateArrowMesh(8.0f, 1.0f, 0.0f, 0.0f); // Red X
    Model3D arrowY = CreateArrowMesh(8.0f, 0.0f, 1.0f, 0.0f); // Green Y
    Model3D arrowZ = CreateArrowMesh(8.0f, 0.0f, 0.0f, 1.0f); // Blue Z
    
    m_arrowXMeshId = m_renderer->AddMesh(arrowX);
    m_arrowYMeshId = m_renderer->AddMesh(arrowY);
    m_arrowZMeshId = m_renderer->AddMesh(arrowZ);
    
    // CHANGED: Mark as gizmo instead of just wireframe
    m_renderer->MarkMeshAsGizmo(m_arrowXMeshId, true);
    m_renderer->MarkMeshAsGizmo(m_arrowYMeshId, true);
    m_renderer->MarkMeshAsGizmo(m_arrowZMeshId, true);
    
    printf("Gizmo mesh IDs: X=%d, Y=%d, Z=%d\n", m_arrowXMeshId, m_arrowYMeshId, m_arrowZMeshId);
    
    free(arrowX.vertices);
    free(arrowX.indices);
    free(arrowY.vertices);
    free(arrowY.indices);
    free(arrowZ.vertices);
    free(arrowZ.indices);
}

void TransformGizmo::UpdateGizmo(EntityId selectedEntity, const hmm_vec3& cameraPos) {
    if (selectedEntity == -1 || m_mode == GizmoMode::None) {
        DestroyGizmo();
        return;
    }
    
    Transform* t = m_ecs->GetTransform(selectedEntity);
    if (!t) {
        DestroyGizmo();
        return;
    }
    
    // Option 1: Use entity position directly (gizmo at base)
    hmm_vec3 gizmoPos = t->position;
    
    // Option 2: Use rotated world position (gizmo at true center)
    // hmm_vec3 gizmoPos = t->GetWorldPosition();
    
    printf("Updating gizmo at pos: (%.2f, %.2f, %.2f)\n", gizmoPos.X, gizmoPos.Y, gizmoPos.Z);
    
    // Create or update gizmo entities
    if (m_gizmoXEntity == -1) {
        m_gizmoXEntity = m_ecs->CreateEntity();
        Transform gizmoTransform;
        gizmoTransform.position = gizmoPos;
        gizmoTransform.yaw = 0.0f;    // FIXED: No rotation needed - arrow already points along X
        gizmoTransform.pitch = 0.0f;
        gizmoTransform.roll = 0.0f;
        m_ecs->AddTransform(m_gizmoXEntity, gizmoTransform);
        m_ecs->AddRenderable(m_gizmoXEntity, m_arrowXMeshId, *m_renderer);
        printf("Created X gizmo entity %d (RED pointing RIGHT +X) at (%.2f, %.2f, %.2f)\n", 
               m_gizmoXEntity, gizmoPos.X, gizmoPos.Y, gizmoPos.Z);
    }
    
    if (m_gizmoYEntity == -1) {
        m_gizmoYEntity = m_ecs->CreateEntity();
        Transform gizmoTransform;
        gizmoTransform.position = gizmoPos;
        gizmoTransform.yaw = 0.0f;
        gizmoTransform.pitch = 0.0f;
        gizmoTransform.roll = 90.0f; // FIXED: Roll +90 to point Y-axis up (was -90)
        m_ecs->AddTransform(m_gizmoYEntity, gizmoTransform);
        m_ecs->AddRenderable(m_gizmoYEntity, m_arrowYMeshId, *m_renderer);
        printf("Created Y gizmo entity %d (GREEN pointing UP +Y) at (%.2f, %.2f, %.2f)\n", 
               m_gizmoYEntity, gizmoPos.X, gizmoPos.Y, gizmoPos.Z);
    }
    
    if (m_gizmoZEntity == -1) {
        m_gizmoZEntity = m_ecs->CreateEntity();
        Transform gizmoTransform;
        gizmoTransform.position = gizmoPos;
        gizmoTransform.yaw = -90.0f;  // FIXED: Yaw -90 to point Z-axis forward
        gizmoTransform.pitch = 0.0f;   // FIXED: No pitch needed
        gizmoTransform.roll = 0.0f;
        m_ecs->AddTransform(m_gizmoZEntity, gizmoTransform);
        m_ecs->AddRenderable(m_gizmoZEntity, m_arrowZMeshId, *m_renderer);
        printf("Created Z gizmo entity %d (BLUE pointing FORWARD +Z) at (%.2f, %.2f, %.2f)\n", 
               m_gizmoZEntity, gizmoPos.X, gizmoPos.Y, gizmoPos.Z);
    }
    
    // Update gizmo positions to follow the selected entity
    Transform* gizmoXTransform = m_ecs->GetTransform(m_gizmoXEntity);
    Transform* gizmoYTransform = m_ecs->GetTransform(m_gizmoYEntity);
    Transform* gizmoZTransform = m_ecs->GetTransform(m_gizmoZEntity);
    
    if (gizmoXTransform) gizmoXTransform->position = gizmoPos;
    if (gizmoYTransform) gizmoYTransform->position = gizmoPos;
    if (gizmoZTransform) gizmoZTransform->position = gizmoPos;
}

GizmoAxis TransformGizmo::RaycastGizmo(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, const hmm_vec3& gizmoPosition) {
    // Ray-line distance calculation for each axis
    float threshold = 1.0f; // Increased threshold for easier clicking
    float closestDist = threshold;
    GizmoAxis closestAxis = GizmoAxis::None;
    
    // Test X axis (red) - points along (1, 0, 0)
    hmm_vec3 xDir = HMM_NormalizeVec3(HMM_Vec3(1.0f, 0.0f, 0.0f));
    float distX = RayToLineDistance(rayOrigin, rayDir, gizmoPosition, xDir, 8.0f);
    if (distX < closestDist) {
        closestDist = distX;
        closestAxis = GizmoAxis::X;
    }
    
    // Test Y axis (green) - points along (0, 1, 0)
    hmm_vec3 yDir = HMM_NormalizeVec3(HMM_Vec3(0.0f, 1.0f, 0.0f));
    float distY = RayToLineDistance(rayOrigin, rayDir, gizmoPosition, yDir, 8.0f);
    if (distY < closestDist) {
        closestDist = distY;
        closestAxis = GizmoAxis::Y;
    }
    
    // Test Z axis (blue) - points along (0, 0, 1)
    hmm_vec3 zDir = HMM_NormalizeVec3(HMM_Vec3(0.0f, 0.0f, 1.0f));
    float distZ = RayToLineDistance(rayOrigin, rayDir, gizmoPosition, zDir, 8.0f);
    if (distZ < closestDist) {
        closestDist = distZ;
        closestAxis = GizmoAxis::Z;
    }
    
    if (closestAxis != GizmoAxis::None) {
        const char* axisName = closestAxis == GizmoAxis::X ? "X (RED)" : 
                              closestAxis == GizmoAxis::Y ? "Y (GREEN)" : "Z (BLUE)";
        printf("Raycast hit axis: %s (dist: %.3f)\n", axisName, closestDist);
    }
    
    return closestAxis;
}

float TransformGizmo::RayToLineDistance(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, 
                                         const hmm_vec3& lineOrigin, const hmm_vec3& lineDir, 
                                         float lineLength) {
    // Calculate the closest distance between a ray and a line segment
    hmm_vec3 w = HMM_SubtractVec3(rayOrigin, lineOrigin);
    float a = HMM_DotVec3(rayDir, rayDir);
    float b = HMM_DotVec3(rayDir, lineDir);
    float c = HMM_DotVec3(lineDir, lineDir);
    float d = HMM_DotVec3(rayDir, w);
    float e = HMM_DotVec3(lineDir, w);
    
    float denom = a * c - b * b;
    if (fabsf(denom) < 0.0001f) {
        return 1000.0f;
    }
    
    float t = (b * e - c * d) / denom;
    float s = (a * e - b * d) / denom;
    
    s = fmaxf(0.0f, fminf(lineLength, s));
    
    hmm_vec3 pointOnRay = HMM_AddVec3(rayOrigin, HMM_MultiplyVec3f(rayDir, t));
    hmm_vec3 pointOnLine = HMM_AddVec3(lineOrigin, HMM_MultiplyVec3f(lineDir, s));
    
    return HMM_LengthVec3(HMM_SubtractVec3(pointOnRay, pointOnLine));
}

bool TransformGizmo::IntersectRayPlane(const hmm_vec3& rayOrigin, const hmm_vec3& rayDir,
                                        const hmm_vec3& planePoint, const hmm_vec3& planeNormal,
                                        float& t) {
    float denom = HMM_DotVec3(planeNormal, rayDir);
    if (fabsf(denom) < 0.0001f) {
        return false; // Ray parallel to plane
    }
    
    hmm_vec3 p0l0 = HMM_SubtractVec3(planePoint, rayOrigin);
    t = HMM_DotVec3(p0l0, planeNormal) / denom;
    return t >= 0.0f;
}

bool TransformGizmo::HandleMouseDown(EntityId selectedEntity, const hmm_vec3& rayOrigin, const hmm_vec3& rayDir) {
    if (m_mode == GizmoMode::None || selectedEntity == -1) {
        return false;
    }
    
    Transform* t = m_ecs->GetTransform(selectedEntity);
    if (!t) return false;
    
    // FIXED: Use entity position directly (same as UpdateGizmo)
    hmm_vec3 gizmoPos = t->position;
    m_activeAxis = RaycastGizmo(rayOrigin, rayDir, gizmoPos);
    
    if (m_activeAxis != GizmoAxis::None) {
        m_isDragging = true;
        m_dragStartPos = t->position;
        m_dragStartScale = t->scale;
        m_dragStartRotation = t->yaw;
        
        hmm_vec3 axisDir;
        switch (m_activeAxis) {
            case GizmoAxis::X: axisDir = HMM_Vec3(1.0f, 0.0f, 0.0f); break;
            case GizmoAxis::Y: axisDir = HMM_Vec3(0.0f, 1.0f, 0.0f); break;
            case GizmoAxis::Z: axisDir = HMM_Vec3(0.0f, 0.0f, 1.0f); break;
            default: axisDir = HMM_Vec3(0.0f, 1.0f, 0.0f); break;
        }
        
        hmm_vec3 camToGizmo = HMM_NormalizeVec3(HMM_SubtractVec3(gizmoPos, rayOrigin));
        m_dragPlaneNormal = HMM_NormalizeVec3(HMM_Cross(axisDir, HMM_Cross(camToGizmo, axisDir)));
        
        float t_intersect;
        if (IntersectRayPlane(rayOrigin, rayDir, gizmoPos, m_dragPlaneNormal, t_intersect)) {
            m_dragStartPoint = HMM_AddVec3(rayOrigin, HMM_MultiplyVec3f(rayDir, t_intersect));
        }
        
        printf("Started dragging axis %d at pos (%.2f, %.2f, %.2f)\n", 
               (int)m_activeAxis, gizmoPos.X, gizmoPos.Y, gizmoPos.Z);
        return true;
    }
    
    return false;
}

void TransformGizmo::HandleMouseDrag(EntityId selectedEntity, const hmm_vec3& rayOrigin, const hmm_vec3& rayDir, float deltaX, float deltaY) {
    if (!m_isDragging || m_activeAxis == GizmoAxis::None || selectedEntity == -1) {
        return;
    }
    
    Transform* t = m_ecs->GetTransform(selectedEntity);
    if (!t) return;
    
    hmm_vec3 gizmoPos = t->position;
    
    if (m_mode == GizmoMode::Translate) {
        float t_intersect;
        if (IntersectRayPlane(rayOrigin, rayDir, gizmoPos, m_dragPlaneNormal, t_intersect)) {
            hmm_vec3 currentPoint = HMM_AddVec3(rayOrigin, HMM_MultiplyVec3f(rayDir, t_intersect));
            hmm_vec3 delta = HMM_SubtractVec3(currentPoint, m_dragStartPoint);
            
            hmm_vec3 axisDir;
            switch (m_activeAxis) {
                case GizmoAxis::X: axisDir = HMM_Vec3(1.0f, 0.0f, 0.0f); break;
                case GizmoAxis::Y: axisDir = HMM_Vec3(0.0f, 1.0f, 0.0f); break;
                case GizmoAxis::Z: axisDir = HMM_Vec3(0.0f, 0.0f, 1.0f); break;
                default: return;
            }
            
            float axisDelta = HMM_DotVec3(delta, axisDir);
            hmm_vec3 axisMovement = HMM_MultiplyVec3f(axisDir, axisDelta);
            
            t->position = HMM_AddVec3(m_dragStartPos, axisMovement);
            
            m_dragStartPoint = currentPoint;
            m_dragStartPos = t->position;
        }
    } else {
        float sensitivity = 0.1f;
        
        switch (m_mode) {
            case GizmoMode::Rotate:
                if (m_activeAxis == GizmoAxis::X) {
                    t->pitch += deltaY * sensitivity * 10.0f;  // Red arrow (X-axis) controls pitch (rotation around X)
                }
                else if (m_activeAxis == GizmoAxis::Y) {
                    t->roll += deltaX * sensitivity * 10.0f;  // Green arrow (Y-axis) controls yaw (rotation around Y)
                }
                else if (m_activeAxis == GizmoAxis::Z) {
                    t->yaw += deltaX * sensitivity * 10.0f;  // Blue arrow (Z-axis) controls roll (rotation around Z)
                }
                break;
                
            case GizmoMode::Scale:
                {
                    float scaleDelta = deltaY * sensitivity;
                    if (m_activeAxis == GizmoAxis::X) {
                        t->scale.X += scaleDelta;  // Red arrow controls X scale
                        t->scale.X = fmaxf(0.1f, t->scale.X);
                    }
                    else if (m_activeAxis == GizmoAxis::Y) {
                        t->scale.Z -= scaleDelta;  // Green arrow controls Y scale
                        t->scale.Z = fmaxf(0.1f, t->scale.Z);
                    }
                    else if (m_activeAxis == GizmoAxis::Z) {
                        t->scale.Y -= scaleDelta;  // Blue arrow controls Z scale
                        t->scale.Y = fmaxf(0.1f, t->scale.Y);
                    }
                }
                break;
                
            default:
                break;
        }
    }
}

void TransformGizmo::HandleMouseUp() {
    m_isDragging = false;
    m_activeAxis = GizmoAxis::None;
    printf("Stopped dragging\n");
}

void TransformGizmo::RenderGizmo(const hmm_mat4& viewProj) {
    // Gizmos are rendered as part of normal wireframe rendering
}

void TransformGizmo::DestroyGizmo() {
    if (m_gizmoXEntity != -1) {
        m_ecs->RemoveRenderable(m_gizmoXEntity, *m_renderer);
        m_ecs->DestroyEntity(m_gizmoXEntity);
        printf("Destroyed X gizmo entity\n");
        m_gizmoXEntity = -1;
    }
    
    if (m_gizmoYEntity != -1) {
        m_ecs->RemoveRenderable(m_gizmoYEntity, *m_renderer);
        m_ecs->DestroyEntity(m_gizmoYEntity);
        printf("Destroyed Y gizmo entity\n");
        m_gizmoYEntity = -1;
    }
    
    if (m_gizmoZEntity != -1) {
        m_ecs->RemoveRenderable(m_gizmoZEntity, *m_renderer);
        m_ecs->DestroyEntity(m_gizmoZEntity);
        printf("Destroyed Z gizmo entity\n");
        m_gizmoZEntity = -1;
    }
}