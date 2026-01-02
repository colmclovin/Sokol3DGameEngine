#pragma once

enum class GameMode {
    Playing,
    Edit
};

class GameStateManager {
public:
    GameStateManager() = default;
    ~GameStateManager() = default;

    void SetMode(GameMode mode) { currentMode_ = mode; }
    GameMode GetMode() const { return currentMode_; }
    bool IsPlaying() const { return currentMode_ == GameMode::Playing; }
    bool IsEdit() const { return currentMode_ == GameMode::Edit; }
    
    void ToggleMode() {
        currentMode_ = (currentMode_ == GameMode::Playing) ? GameMode::Edit : GameMode::Playing;
    }

private:
    GameMode currentMode_ = GameMode::Playing;
};