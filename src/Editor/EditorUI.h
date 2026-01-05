#pragma once
#include "../Game/ECS.h"
#include "../Audio/AudioEngine.h"
#include "../Game/GameState.h"

// Forward declare PlayerController
class PlayerController;

class EditorUI {
public:
    EditorUI();
    ~EditorUI();
    
    void Init(ECS* ecs, AudioEngine* audio, GameStateManager* gameState);
    void SetPlayer(PlayerController* player) { m_player = player; }
    void RenderAudioControls();
    void RenderGameStateControls();
    void RenderEntityInspector(EntityId selectedEntity);
    void RenderPlacementControls(bool& placementMode, int& placementMeshId, int meshTreeId, int meshEnemyId);
    
    // ADDED: Collision visualization toggle
    void RenderCollisionVisualization(bool& showCollisions);
    
    // ADDED: Global performance stats
    void RenderPerformanceStats(float deltaTime);

    int GetSelectedPlacementType() const { return m_selectedPlacementType; }

private:
    ECS* ecs = nullptr;
    AudioEngine* audio = nullptr;
    GameStateManager* gameState = nullptr;
    
    // Member variables with m_ prefix (used in implementation)
    ECS* m_ecs = nullptr;
    AudioEngine* m_audio = nullptr;
    GameStateManager* m_gameState = nullptr;
    PlayerController* m_player = nullptr;
    int m_selectedPlacementType = 0;
    
    // ADDED: FPS tracking
    float m_fpsAccumulator = 0.0f;
    int m_fpsFrameCount = 0;
    float m_currentFPS = 0.0f;
    
    // ADDED: Helper to render player-specific inspector
    void RenderPlayerInspector(EntityId playerId);
    
    // ADDED: Helper to render enemy-specific inspector
    void RenderEnemyInspector(EntityId enemyId);
    
    // ADDED: Helper to render tree-specific inspector
    void RenderTreeInspector(EntityId treeId);
};