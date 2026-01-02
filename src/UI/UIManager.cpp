// Provide sokol-imgui + sokol-debugtext implementations in exactly one TU
#define SOKOL_IMGUI_IMPL
#define SOKOL_DEBUGTEXT_IMPL
#define SOKOL_D3D11
#include "../../External/Imgui/imgui.h"            // must come before sokol_imgui.h
#include "../../External/Sokol/sokol_app.h"        // must come before sokol_imgui.h
#include "../../External/Sokol/sokol_gfx.h"
#include "../../External/Sokol/util/sokol_imgui.h"  // implementation included because SOKOL_IMGUI_IMPL defined
#include "../../External/Sokol/sokol_log.h"
#include "../../External/dbgui.h"
#include "../../External/sokol_debugtext.h"         // implementation included because SOKOL_DEBUGTEXT_IMPL defined

#include "UIManager.h"
#include <string.h>

bool UIManager::Setup() {
    sdtx_desc_t sdtx_desc = {};
    sdtx_desc.fonts[0] = sdtx_font_oric();
    sdtx_desc.logger.func = slog_func;
    sdtx_setup(&sdtx_desc);

    __dbgui_setup(sapp_sample_count());

    simgui_desc_t simgui_desc = {};
    simgui_desc.logger.func = slog_func;
    simgui_setup(&simgui_desc);

    return true;
}

void UIManager::NewFrame(int width, int height, float dt, float dpi_scale) {
    simgui_new_frame({ width, height, dt, dpi_scale });
    // sdtx: recording functions may be called at any time; no per-frame call required
}

void UIManager::Render() {
    // Render ImGui windows if visible
    if (gui_visible_) {
        ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debug GUI", &gui_visible_, ImGuiWindowFlags_None);

        // toggle demo window
        ImGui::Checkbox("Show ImGui Demo", &show_demo_window_);

        // debug-text controls
        if (ImGui::CollapsingHeader("DebugText (sdtx)")) {
            bool changed = false;
            float w = dbg_canvas_w_, h = dbg_canvas_h_;
            changed |= ImGui::SliderFloat("Canvas Width", &w, 64.0f, 4096.0f);
            changed |= ImGui::SliderFloat("Canvas Height", &h, 64.0f, 4096.0f);
            if (changed) SetDebugCanvas(w, h);

            float ox = dbg_origin_x_, oy = dbg_origin_y_;
            if (ImGui::DragFloat2("Origin (x,y)", &ox, 0.1f)) SetDebugOrigin(ox, oy);

            int cr = dbg_r_, cg = dbg_g_, cb = dbg_b_;
            if (ImGui::ColorEdit3("Text Color", reinterpret_cast<float*>(&cr))) {
                SetDebugColor((uint8_t)cr, (uint8_t)cg, (uint8_t)cb);
            }
        }

        // call registered callbacks
        if (!gui_callbacks_.empty()) {
            ImGui::Separator();
            ImGui::Text("Custom Panels:");
            for (const auto& cb : gui_callbacks_) {
                cb();
            }
        }

        ImGui::End();

        if (show_demo_window_) {
            ImGui::ShowDemoWindow(&show_demo_window_);
        }
    }

    // render simgui/dbgui and sdtx
    simgui_render();
    __dbgui_draw();
    sdtx_draw();
}

void UIManager::Shutdown() {
    __dbgui_shutdown();
    sdtx_shutdown();
    simgui_shutdown();
}

void UIManager::SetDebugCanvas(float w, float h) {
    dbg_canvas_w_ = w;
    dbg_canvas_h_ = h;
}

void UIManager::SetDebugOrigin(float x, float y) {
    dbg_origin_x_ = x;
    dbg_origin_y_ = y;
}

void UIManager::SetDebugColor(uint8_t r, uint8_t g, uint8_t b) {
    dbg_r_ = r; dbg_g_ = g; dbg_b_ = b;
}

void UIManager::DebugText(const char* text) {
    sdtx_canvas(dbg_canvas_w_, dbg_canvas_h_);
    sdtx_origin(dbg_origin_x_, dbg_origin_y_);
    sdtx_color3b(dbg_r_, dbg_g_, dbg_b_);
    sdtx_puts(text);
}

void UIManager::HandleEvent(const struct sapp_event* ev) {
    // toggle GUI on F1 press
    if (ev && ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (ev->key_code == SAPP_KEYCODE_F1) {
            gui_visible_ = !gui_visible_;
            return; // consume toggle (still forward to simgui if desired)
        }
    }

    // forward to simgui and dbgui
    simgui_handle_event(ev);
    __dbgui_event(ev);
}

void UIManager::ToggleGui() {
    gui_visible_ = !gui_visible_;
}

void UIManager::SetGuiVisible(bool visible) {
    gui_visible_ = visible;
}

bool UIManager::IsGuiVisible() const {
    return gui_visible_;
}

void UIManager::AddGuiCallback(const std::function<void()>& cb) {
    gui_callbacks_.push_back(cb);
}