#include "EditorUI.h"
#include "../Game/Player.h"
#include "../../External/Imgui/imgui.h"

EditorUI::EditorUI()
    : ecs(nullptr)
    , audio(nullptr)
    , gameState(nullptr)
    , m_ecs(nullptr)
    , m_audio(nullptr)
    , m_gameState(nullptr)
    , m_player(nullptr)
    , m_selectedPlacementType(0)
    , m_fpsAccumulator(0.0f)
    , m_fpsFrameCount(0)
    , m_currentFPS(0.0f)
{
}

EditorUI::~EditorUI() {
}

void EditorUI::Init(ECS* ecsPtr, AudioEngine* audioPtr, GameStateManager* gameStatePtr) {
    m_ecs = ecsPtr;
    m_audio = audioPtr;
    m_gameState = gameStatePtr;
    ecs = ecsPtr;
    audio = audioPtr;
    gameState = gameStatePtr;
}

void EditorUI::RenderAudioControls() {
    ImGui::Text("Audio Controls:");
    if (m_audio->IsPlaying()) {
        if (ImGui::Button("Stop")) {
            m_audio->Stop();
        }
    } else {
        if (ImGui::Button("Play")) {
            m_audio->Play();
        }
    }
    float vol = m_audio->GetVolume();
    if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f)) {
        m_audio->SetVolume(vol);
    }
}

void EditorUI::RenderGameStateControls() {
    ImGui::Separator();
    ImGui::Text("Game State:");
    const char* currentMode = m_gameState->IsPlaying() ? "PLAYING" : "EDIT";
    ImGui::Text("Current Mode: %s", currentMode);
    ImGui::Text("Press TAB to toggle modes");
    if (ImGui::Button("Toggle Mode (TAB)")) {
        m_gameState->ToggleMode();
    }
}

// ADDED: Global performance statistics panel
void EditorUI::RenderPerformanceStats(float deltaTime) {
    if (ImGui::CollapsingHeader("Performance Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        // FPS calculation (update every 0.5 seconds)
        m_fpsAccumulator += deltaTime;
        m_fpsFrameCount++;
        
        if (m_fpsAccumulator >= 0.5f) {
            m_currentFPS = (float)m_fpsFrameCount / m_fpsAccumulator;
            m_fpsAccumulator = 0.0f;
            m_fpsFrameCount = 0;
        }
        
        // Display FPS with color coding
        ImVec4 fpsColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
        if (m_currentFPS < 30.0f) {
            fpsColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
        } else if (m_currentFPS < 60.0f) {
            fpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
        }
        
        ImGui::TextColored(fpsColor, "FPS: %.1f", m_currentFPS);
        ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
        ImGui::Text("Delta Time: %.4f s", deltaTime);
        
        ImGui::Separator();
        
        // Entity counts
        std::vector<EntityId> allEntities = m_ecs->AllEntities();
        ImGui::Text("Total Entities: %zu", allEntities.size());
        
        // Count by type
        int playerCount = 0;
        int enemyCount = 0;
        int treeCount = 0;
        int lightCount = 0;
        int otherCount = 0;
        
        for (EntityId id : allEntities) {
            Selectable* sel = m_ecs->GetSelectable(id);
            if (sel) {
                if (strcmp(sel->name, "Player") == 0) playerCount++;
                else if (strcmp(sel->name, "Enemy") == 0) enemyCount++;
                else if (strcmp(sel->name, "Tree") == 0) treeCount++;
                else if (strcmp(sel->name, "Point Light") == 0) lightCount++;
                else otherCount++;
            } else {
                otherCount++;
            }
        }
        
        ImGui::Indent();
        ImGui::Text("Players: %d", playerCount);
        ImGui::Text("Enemies: %d", enemyCount);
        ImGui::Text("Trees: %d", treeCount);
        ImGui::Text("Lights: %d", lightCount);
        ImGui::Text("Other: %d", otherCount);
        ImGui::Unindent();
        
        ImGui::Separator();
        
        // Physics statistics
        const auto& colliders = m_ecs->GetColliders();
        const auto& rigidbodies = m_ecs->GetRigidbodies();
        
        ImGui::Text("Colliders: %zu", colliders.size());
        ImGui::Text("Rigidbodies: %zu", rigidbodies.size());
        
        // Camera info (if player exists)
        if (m_player) {
            ImGui::Separator();
            hmm_vec3 camPos = m_player->CameraPosition();
            ImGui::Text("Camera Position:");
            ImGui::Text("  X: %.2f  Y: %.2f  Z: %.2f", camPos.X, camPos.Y, camPos.Z);
        }
    }
}

// ADDED: Player-specific inspector panel
void EditorUI::RenderPlayerInspector(EntityId playerId) {
    if (!m_player) return;
    
    Selectable* sel = m_ecs->GetSelectable(playerId);
    if (!sel) return;
    
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.6f, 0.8f, 0.8f));
    
    ImGui::Text("Entity ID: %d", playerId);
    ImGui::Text("Type: PLAYER");
    ImGui::Separator();
    
    // === MOVEMENT & PHYSICS ===
    if (ImGui::CollapsingHeader("Player Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        hmm_vec3 velocity = m_player->GetVelocity();
        float horizontalSpeed = m_player->GetCurrentSpeed();
        
        // Display current state
        ImGui::Text("Speed: %.2f m/s", horizontalSpeed);
        ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity.X, velocity.Y, velocity.Z);
        
        // Velocity magnitude
        float totalSpeed = sqrtf(velocity.X * velocity.X + velocity.Y * velocity.Y + velocity.Z * velocity.Z);
        ImGui::Text("Total Speed: %.2f m/s", totalSpeed);
        
        // Grounded state
        bool isGrounded = m_player->IsGrounded();
        ImGui::TextColored(isGrounded ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.5f, 0, 1), 
                          "Grounded: %s", isGrounded ? "YES" : "NO");
        
        ImGui::Separator();
        
        // Editable movement parameters
        float moveSpeed = m_player->GetMoveSpeed();
        if (ImGui::SliderFloat("Walk Speed", &moveSpeed, 1.0f, 20.0f, "%.1f m/s")) {
            m_player->SetMoveSpeed(moveSpeed);
        }
        
        ImGui::Text("Sprint Speed: %.1f m/s (2x Walk)", moveSpeed * 2.0f);
        
        float jumpForce = m_player->GetJumpForce();
        if (ImGui::SliderFloat("Jump Force", &jumpForce, 5.0f, 30.0f, "%.1f")) {
            m_player->SetJumpForce(jumpForce);
        }
    }
    
    // === PHYSICS PROPERTIES ===
    Rigidbody* rb = m_ecs->GetRigidbody(playerId);
    if (rb) {
        if (ImGui::CollapsingHeader("Physics Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.1f, 100.0f, "%.1f kg");
            ImGui::DragFloat("Drag", &rb->drag, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Bounciness", &rb->bounciness, 0.01f, 0.0f, 1.0f, "%.2f");
            
            if (ImGui::Checkbox("Affected By Gravity", &rb->affectedByGravity)) {
                printf("Player gravity: %s\n", rb->affectedByGravity ? "ON" : "OFF");
            }
            
            ImGui::Separator();
            ImGui::Text("NOTE: Gravity = -9.81 m/s²");
        }
    }
    
    // === COLLISION INFO ===
    Collider* collider = m_ecs->GetCollider(playerId);
    if (collider) {
        if (ImGui::CollapsingHeader("Collision Info")) {
            const char* colliderTypeNames[] = { "Sphere", "Box", "Capsule", "Mesh", "Plane" };
            ImGui::Text("Type: %s", colliderTypeNames[(int)collider->type]);
            
            if (collider->type == ColliderType::Sphere) {
                ImGui::DragFloat("Radius", &collider->radius, 0.01f, 0.1f, 5.0f, "%.2f m");
            }
            
            ImGui::Checkbox("Is Trigger", &collider->isTrigger);
            ImGui::Text("Is Static: %s", collider->isStatic ? "YES" : "NO");
        }
    }
    
    // === TRANSFORM ===
    Transform* t = m_ecs->GetTransform(playerId);
    if (t) {
        if (ImGui::CollapsingHeader("Transform")) {
            ImGui::DragFloat3("Position", &t->position.X, 0.1f);
            ImGui::DragFloat("Yaw", &t->yaw, 1.0f);
            
            // Display world position
            hmm_vec3 worldPos = t->GetWorldPosition();
            ImGui::Text("World Position: (%.2f, %.2f, %.2f)", worldPos.X, worldPos.Y, worldPos.Z);
        }
    }
    
    ImGui::PopStyleColor();
}

// ADDED: Enemy-specific inspector panel
void EditorUI::RenderEnemyInspector(EntityId enemyId) {
    Selectable* sel = m_ecs->GetSelectable(enemyId);
    if (!sel) return;
    
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
    
    ImGui::Text("Entity ID: %d", enemyId);
    ImGui::Text("Type: ENEMY");
    ImGui::Separator();
    
    // === AI STATE ===
    AIController* ai = m_ecs->GetAI(enemyId);
    if (ai) {
        if (ImGui::CollapsingHeader("AI Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* stateNames[] = { "Idle", "Wander", "Chase" };
            int currentState = (int)ai->state;
            
            // Display current state with color
            ImVec4 stateColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            if (ai->state == AIState::Idle) stateColor = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
            else if (ai->state == AIState::Wander) stateColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            else if (ai->state == AIState::Chase) stateColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            
            ImGui::TextColored(stateColor, "Current State: %s", stateNames[currentState]);
            
            if (ImGui::Combo("Override State", &currentState, stateNames, 3)) {
                ai->state = (AIState)currentState;
            }
            
            ImGui::DragFloat("State Timer", &ai->stateTimer, 0.1f, 0.0f, 10.0f, "%.1f s");
            ImGui::DragFloat3("Wander Target", &ai->wanderTarget.X, 0.1f);
            
            // Display distance to wander target
            Transform* t = m_ecs->GetTransform(enemyId);
            if (t) {
                hmm_vec3 toTarget = HMM_SubtractVec3(ai->wanderTarget, t->position);
                float distance = sqrtf(toTarget.X * toTarget.X + toTarget.Z * toTarget.Z);
                ImGui::Text("Distance to Target: %.2f m", distance);
            }
        }
    }
    
    // === PHYSICS & COLLISION ===
    Rigidbody* rb = m_ecs->GetRigidbody(enemyId);
    if (rb) {
        if (ImGui::CollapsingHeader("Physics Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Display current velocity
            float speed = sqrtf(rb->velocity.X * rb->velocity.X + rb->velocity.Z * rb->velocity.Z);
            ImGui::Text("Speed: %.2f m/s", speed);
            ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", rb->velocity.X, rb->velocity.Y, rb->velocity.Z);
            
            ImGui::Separator();
            
            // Editable properties
            ImGui::DragFloat("Mass", &rb->mass, 1.0f, 1.0f, 500.0f, "%.0f kg");
            ImGui::DragFloat("Drag", &rb->drag, 0.01f, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat("Bounciness", &rb->bounciness, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::Checkbox("Affected By Gravity", &rb->affectedByGravity);
            ImGui::Checkbox("Kinematic", &rb->isKinematic);
        }
    }
    
    Collider* collider = m_ecs->GetCollider(enemyId);
    if (collider) {
        if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* colliderTypeNames[] = { "Sphere", "Box", "Capsule", "Mesh", "Plane" };
            int currentType = (int)collider->type;
            
            ImGui::Text("Type: %s", colliderTypeNames[currentType]);
            
            if (collider->type == ColliderType::Sphere) {
                ImGui::DragFloat("Radius", &collider->radius, 0.01f, 0.1f, 10.0f, "%.2f m");
            } else if (collider->type == ColliderType::Box) {
                ImGui::DragFloat3("Half Extents", &collider->boxHalfExtents.X, 0.1f);
            }
            
            ImGui::Separator();
            ImGui::Checkbox("Is Trigger", &collider->isTrigger);
            ImGui::Checkbox("Is Static", &collider->isStatic);
        }
    }
    
    // === TRANSFORM ===
    Transform* t = m_ecs->GetTransform(enemyId);
    if (t) {
        if (ImGui::CollapsingHeader("Transform")) {
            ImGui::DragFloat3("Position", &t->position.X, 0.1f);
            ImGui::DragFloat("Yaw", &t->yaw, 1.0f);
            
            hmm_vec3 worldPos = t->GetWorldPosition();
            ImGui::Text("World Position: (%.2f, %.2f, %.2f)", worldPos.X, worldPos.Y, worldPos.Z);
        }
    }
    
    ImGui::PopStyleColor();
}

// ADDED: Tree-specific inspector panel
void EditorUI::RenderTreeInspector(EntityId treeId) {
    Selectable* sel = m_ecs->GetSelectable(treeId);
    if (!sel) return;
    
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.8f, 0.2f, 0.8f));
    
    ImGui::Text("Entity ID: %d", treeId);
    ImGui::Text("Type: TREE (Static)");
    ImGui::Separator();
    
    // === COLLISION INFO ===
    Collider* collider = m_ecs->GetCollider(treeId);
    if (collider) {
        if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* colliderTypeNames[] = { "Sphere", "Box", "Capsule", "Mesh", "Plane" };
            ImGui::Text("Type: %s", colliderTypeNames[(int)collider->type]);
            
            if (collider->type == ColliderType::Sphere) {
                ImGui::DragFloat("Radius", &collider->radius, 0.01f, 0.1f, 10.0f, "%.2f m");
            }
            
            ImGui::Separator();
            ImGui::Text("Is Static: %s", collider->isStatic ? "YES" : "NO");
            ImGui::Text("Is Trigger: %s", collider->isTrigger ? "YES" : "NO");
        }
    }
    
    // === SELECTION VOLUME ===
    if (ImGui::CollapsingHeader("Selection Volume")) {
        const char* volumeTypeNames[] = { "Sphere", "Box", "Mesh", "Icon" };
        ImGui::Text("Type: %s", volumeTypeNames[(int)sel->volumeType]);
        
        if (sel->volumeType == SelectionVolumeType::Sphere) {
            ImGui::DragFloat("Selection Radius", &sel->boundingSphereRadius, 0.1f, 0.5f, 20.0f, "%.1f m");
        }
        
        ImGui::Checkbox("Show Wireframe", &sel->showWireframe);
        ImGui::Checkbox("Can Be Selected", &sel->canBeSelected);
    }
    
    // === TRANSFORM ===
    Transform* t = m_ecs->GetTransform(treeId);
    if (t) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &t->position.X, 0.1f);
            ImGui::DragFloat3("Scale", &t->scale.X, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat3("Origin Offset", &t->originOffset.X, 0.1f);
            ImGui::Separator();
            ImGui::DragFloat("Yaw", &t->yaw, 1.0f);
            ImGui::DragFloat("Pitch", &t->pitch, 1.0f);
            ImGui::DragFloat("Roll", &t->roll, 1.0f);
            
            hmm_vec3 worldPos = t->GetWorldPosition();
            ImGui::Text("World Position: (%.2f, %.2f, %.2f)", worldPos.X, worldPos.Y, worldPos.Z);
        }
    }
    
    ImGui::PopStyleColor();
}

void EditorUI::RenderEntityInspector(EntityId selectedEntity) {
    if (selectedEntity == -1) {
        ImGui::Text("No entity selected");
        ImGui::Text("Click on an entity in Edit mode to select it");
        return;
    }
    
    // ADDED: Check entity type and render appropriate inspector
    Selectable* sel = m_ecs->GetSelectable(selectedEntity);
    if (sel) {
        if (m_player && selectedEntity == m_player->Entity()) {
            RenderPlayerInspector(selectedEntity);
            return;
        } else if (strcmp(sel->name, "Enemy") == 0) {
            RenderEnemyInspector(selectedEntity);
            return;
        } else if (strcmp(sel->name, "Tree") == 0) {
            RenderTreeInspector(selectedEntity);
            return;
        }
    }
    
    // Fallback: Generic inspector for lights and other entities
    if (!sel) {
        ImGui::Text("Selected entity has no Selectable component");
        return;
    }
    
    ImGui::Text("Entity ID: %d", selectedEntity);
    ImGui::Text("Name: %s", sel->name);
    ImGui::Separator();
    
    // Transform component
    Transform* t = m_ecs->GetTransform(selectedEntity);
    if (t) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &t->position.X, 0.1f);
            ImGui::DragFloat3("Scale", &t->scale.X, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat3("Origin Offset", &t->originOffset.X, 0.1f);
            ImGui::Separator();
            ImGui::DragFloat("Yaw", &t->yaw, 1.0f);
            ImGui::DragFloat("Pitch", &t->pitch, 1.0f);
            ImGui::DragFloat("Roll", &t->roll, 1.0f);
            
            hmm_vec3 worldPos = t->GetWorldPosition();
            ImGui::Text("World Position: (%.2f, %.2f, %.2f)", worldPos.X, worldPos.Y, worldPos.Z);
        }
    }
    
    // Light component
    Light* light = m_ecs->GetLight(selectedEntity);
    if (light) {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Color", &light->color.X);
            ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Radius", &light->radius, 0.5f, 0.1f, 1000.0f);
            ImGui::Checkbox("Enabled", &light->enabled);
        }
    }
    
    // Rigidbody component
    Rigidbody* rb = m_ecs->GetRigidbody(selectedEntity);
    if (rb) {
        if (ImGui::CollapsingHeader("Rigidbody")) {
            ImGui::DragFloat3("Velocity", &rb->velocity.X, 0.1f);
            ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.1f, 1000.0f);
            ImGui::Checkbox("Affected By Gravity", &rb->affectedByGravity);
            ImGui::DragFloat("Drag", &rb->drag, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("Bounciness", &rb->bounciness, 0.01f, 0.0f, 1.0f);
        }
    }
    
    // Collider component
    Collider* collider = m_ecs->GetCollider(selectedEntity);
    if (collider) {
        if (ImGui::CollapsingHeader("Collider")) {
            const char* colliderTypeNames[] = { "Sphere", "Box", "Capsule", "Mesh", "Plane" };
            int currentType = (int)collider->type;
            if (ImGui::Combo("Type", &currentType, colliderTypeNames, 5)) {
                collider->type = (ColliderType)currentType;
            }
            
            if (collider->type == ColliderType::Sphere) {
                ImGui::DragFloat("Radius", &collider->radius, 0.01f, 0.1f, 10.0f);
            } else if (collider->type == ColliderType::Box) {
                ImGui::DragFloat3("Half Extents", &collider->boxHalfExtents.X, 0.1f);
            }
            
            ImGui::Checkbox("Is Trigger", &collider->isTrigger);
            ImGui::Checkbox("Is Static", &collider->isStatic);
        }
    }
    
    // Billboard component
    Billboard* billboard = m_ecs->GetBillboard(selectedEntity);
    if (billboard) {
        if (ImGui::CollapsingHeader("Billboard")) {
            ImGui::InputInt("Follow Target ID", &billboard->followTarget);
            ImGui::DragFloat3("Offset", &billboard->offset.X, 0.1f);
            ImGui::Checkbox("Lock Y Axis", &billboard->lockY);
        }
    }
    
    // Screen Space component
    ScreenSpace* ss = m_ecs->GetScreenSpace(selectedEntity);
    if (ss) {
        if (ImGui::CollapsingHeader("Screen Space")) {
            ImGui::DragFloat2("Screen Position", &ss->screenPosition.X, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat2("Size", &ss->size.X, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Depth", &ss->depth, 0.01f, 0.0f, 1.0f);
        }
    }
    
    ImGui::Separator();
    if (ImGui::Button("Deselect")) {
        sel->isSelected = false;
    }
}

void EditorUI::RenderPlacementControls(bool& placementMode, int& placementMeshId, int meshTreeId, int meshEnemyId) {
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Entity Placement")) {
        ImGui::Text("Press 'G' to grab/place selected entity");
        ImGui::Text("Press 'Delete' to delete selected entity");
        
        const char* entityTypes[] = { "Tree", "Enemy", "Light" };
        ImGui::Combo("Entity Type", &m_selectedPlacementType, entityTypes, 3);
        
        if (ImGui::Button("Start Placement Mode (P)")) {
            placementMode = !placementMode;
            if (placementMode) {
                if (m_selectedPlacementType == 0) placementMeshId = meshTreeId;
                else if (m_selectedPlacementType == 1) placementMeshId = meshEnemyId;
                else placementMeshId = -1;
            }
        }
        
        if (placementMode) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "PLACEMENT MODE ACTIVE");
            ImGui::Text("Click to place, ESC to cancel");
        }
    }
    
    // Gizmo controls
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Transform Gizmo")) {
        ImGui::Text("Keyboard Shortcuts:");
        ImGui::BulletText("T - Toggle Gizmo On/Off");
        ImGui::BulletText("G - Translate Mode");
        ImGui::BulletText("R - Rotate Mode");
        ImGui::BulletText("S - Scale Mode");
    }
}

void EditorUI::RenderCollisionVisualization(bool& showCollisions) {
    if (ImGui::Checkbox("Show Collision Shapes", &showCollisions)) {
        printf("Collision visualization: %s\n", showCollisions ? "ON" : "OFF");
    }
    
    if (showCollisions) {
        ImGui::Indent();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Red = Collision Shapes");
        ImGui::Unindent();
    }
}