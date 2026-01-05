#include "EditorUI.h"
#include "../../External/Imgui/imgui.h"

EditorUI::EditorUI()
    : ecs(nullptr)
    , audio(nullptr)
    , gameState(nullptr)
    , m_ecs(nullptr)
    , m_audio(nullptr)
    , m_gameState(nullptr)
    , m_selectedPlacementType(0)
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

void EditorUI::RenderEntityInspector(EntityId selectedEntity) {
    if (selectedEntity == -1) {
        ImGui::Text("No entity selected");
        ImGui::Text("Click on an entity in Edit mode to select it");
        return;
    }
    
    Selectable* sel = m_ecs->GetSelectable(selectedEntity);
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
            ImGui::DragFloat3("Scale", &t->scale.X, 0.01f, 0.01f, 100.0f); // ADDED
            ImGui::DragFloat3("Origin Offset", &t->originOffset.X, 0.1f); // ADDED
            ImGui::Separator();
            ImGui::DragFloat("Yaw", &t->yaw, 1.0f);
            ImGui::DragFloat("Pitch", &t->pitch, 1.0f);
            ImGui::DragFloat("Roll", &t->roll, 1.0f);
            
            // Display world position
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
        }
    }
    
    // AI component
    AIController* ai = m_ecs->GetAI(selectedEntity);
    if (ai) {
        if (ImGui::CollapsingHeader("AI Controller")) {
            const char* stateNames[] = { "Idle", "Wander", "Chase" };
            int currentState = (int)ai->state;
            if (ImGui::Combo("State", &currentState, stateNames, 3)) {
                ai->state = (AIState)currentState;
            }
            ImGui::DragFloat("State Timer", &ai->stateTimer, 0.1f);
            ImGui::DragFloat3("Wander Target", &ai->wanderTarget.X, 0.1f);
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
    
    // ADDED: Gizmo controls
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