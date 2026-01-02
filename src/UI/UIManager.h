#pragma once
#define SOKOL_D3D11
#include <cstdint>
#include <functional>
#include <vector>

class UIManager {
public:
    bool Setup();
    void NewFrame(int width, int height, float dt, float dpi_scale);
    void Render(); // call inside sokol pass
    void Shutdown();

    // Debug text helpers (sdtx)
    void SetDebugCanvas(float w, float h);
    void SetDebugOrigin(float x, float y);
    void SetDebugColor(uint8_t r, uint8_t g, uint8_t b);
    void DebugText(const char* text);

    // GUI control
    void HandleEvent(const struct sapp_event* ev); // Forward events (and toggles F1)
    void ToggleGui();
    void SetGuiVisible(bool visible);
    bool IsGuiVisible() const;

    // Register a callback to draw custom ImGui widgets (called when GUI visible)
    void AddGuiCallback(const std::function<void()>& cb);

private:
    // internal state
    bool gui_visible_ = false;
    bool show_demo_window_ = false;

    // debug-text state
    float dbg_canvas_w_ = 640.0f;
    float dbg_canvas_h_ = 480.0f;
    float dbg_origin_x_ = 0.0f;
    float dbg_origin_y_ = 0.0f;
    uint8_t dbg_r_ = 255, dbg_g_ = 255, dbg_b_ = 255;

    std::vector<std::function<void()>> gui_callbacks_;
};