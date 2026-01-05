#pragma once
#include "../Game/ECS.h"
#include "../Audio/AudioEngine.h"
#include "../Game/GameState.h"

class EditorUI {
public:
    EditorUI();
    ~EditorUI();
    
    void Init(ECS* ecs, AudioEngine* audio, GameStateManager* gameState);
    void RenderAudioControls();
    void RenderGameStateControls();
    void RenderEntityInspector(EntityId selectedEntity);
    void RenderPlacementControls(bool& placementMode, int& placementMeshId, int meshTreeId, int meshEnemyId);
    
    // ADDED: Collision visualization toggle
    void RenderCollisionVisualization(bool& showCollisions);

    int GetSelectedPlacementType() const { return m_selectedPlacementType; }


private:
    ECS* ecs = nullptr;
    AudioEngine* audio = nullptr;
    GameStateManager* gameState = nullptr;
    
    // Member variables with m_ prefix (used in implementation)
    ECS* m_ecs = nullptr;
    AudioEngine* m_audio = nullptr;
    GameStateManager* m_gameState = nullptr;
    int m_selectedPlacementType = 0;
};