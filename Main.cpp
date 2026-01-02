#define SOKOL_D3D11
#include "../External/Sokol/sokol_app.h"
#include "../External/Sokol/sokol_fetch.h"
#include "../External/Sokol/sokol_gfx.h"
#include "../External/Sokol/sokol_glue.h"
#include "../External/Sokol/sokol_log.h"
#include <stdio.h>
#include <windows.h>

// Include HandmadeMath header only (implementation moved to a single TU)
#include "../External/HandmadeMath.h"

#include "Shader.h"
#include "../External/Imgui/imgui.h"
#include "../External/stb_image.h"
#include "../External/Assimp/include/assimp/Importer.hpp"
#include "../External/Assimp/include/assimp/postprocess.h"
#include "../External/Assimp/include/assimp/scene.h"
#include "src/Renderer/Renderer.h"
#include "src/Audio/AudioEngine.h"
#include "src/Model/ModelLoader.h"
#include "src/UI/UIManager.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <cmath>

static Renderer renderer;
static AudioEngine audio;
static ModelLoader loader;
static UIManager ui;

#define MAX_MODELS 16
Model3D models3D[MAX_MODELS];

int cubeInstanceIds[5] = { -1, -1, -1, -1, -1 };
int characterInstanceId = -1;

void init(void) {
    // audio
    audio.Init();
    audio.LoadWav("assets/audio/TetrisTheme.wav");

    // sokol
    sg_desc desc = {};
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    // UI
    ui.Setup();

    // register a simple ImGui callback (you can add more / modify)
    ui.AddGuiCallback([](){
        ImGui::Separator();
        ImGui::Text("Example Controls:");
        static float floatVal = 0.5f;
        ImGui::SliderFloat("Example Float", &floatVal, 0.0f, 1.0f);
        static int intVal = 5;
        ImGui::InputInt("Example Int", &intVal);
        static bool toggle = true;
        ImGui::Checkbox("Example Toggle", &toggle);

        // Audio controls
        ImGui::Separator();
        ImGui::Text("Audio Controls:");
        if (audio.IsPlaying()) {
            if (ImGui::Button("Stop")) {
                audio.Stop();
            }
        } else {
            if (ImGui::Button("Play")) {
                audio.Play();
            }
        }
        float vol = audio.GetVolume();
        if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f)) {
            audio.SetVolume(vol);
        }
    });

    // renderer
    renderer.Init();

    // models via ModelLoader
    models3D[0] = loader.LoadModel("assets/models/cube.glb");
    models3D[1] = loader.LoadModel("assets/models/test1.glb");

    int meshCube = renderer.AddMesh(models3D[0]);
    int meshChar = renderer.AddMesh(models3D[1]);

    // create instances
    for (int i = 0; i < 5; ++i) {
        float x = (i - 2) * 3.0f;
        hmm_mat4 t = HMM_Translate(HMM_Vec3(x, 0.0f, 0.0f));
        cubeInstanceIds[i] = renderer.AddInstance(meshCube, t);
    }
    characterInstanceId = renderer.AddInstance(meshChar, HMM_Translate(HMM_Vec3(8.0f,0.0f,0.0f)));
}

void frame(void) {
    float dt = (float)sapp_frame_duration();
    const int width = sapp_width();
    const int height = sapp_height();
    ui.NewFrame(width, height, dt, sapp_dpi_scale());

    // show debug text via UIManager
    // use full window virtual canvas so characters have expected size
    ui.SetDebugCanvas(sapp_widthf(), sapp_heightf()); // virtual canvas = window size
    ui.SetDebugOrigin(2.0f, 2.0f); // origin in character cells (small offset from top-left)
    ui.SetDebugColor(255, 255, 0); // yellow
    ui.DebugText("Banana");

    // compute view-proj
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    hmm_mat4 proj = HMM_Perspective(60.0f, w/h, 0.01f, 100.0f);
    hmm_vec3 cam_pos = HMM_Vec3(0.0f, 2.0f, 50.0f);
    hmm_vec3 target = HMM_Vec3(0.0f, 0.0f, 0.0f);
    hmm_vec3 up = HMM_Vec3(0.0f,1.0f,0.0f);
    hmm_mat4 view = HMM_LookAt(cam_pos, target, up);
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // animate instances (example)
    static float elapsed = 0.0f;
    elapsed += dt;
    for (int i = 0; i < 5; ++i) {
        int id = cubeInstanceIds[i];
        if (id < 0) continue;
        float baseX = (i - 2) * 3.0f;
        float bobZ = sinf(elapsed * 1.5f + i) * 2.0f;
        float bobY = sinf(elapsed * 2.0f + i) * 0.5f;
        float angle = elapsed * 1.2f + (float)i * 0.6f;
        hmm_mat4 trans = HMM_Translate(HMM_Vec3(baseX, bobY, bobZ));
        hmm_mat4 rot = HMM_Rotate(angle, HMM_Vec3(0.0f,1.0f,0.0f));
        renderer.UpdateInstanceTransform(id, HMM_MultiplyMat4(trans, rot));
    }

    renderer.BeginPass();
    renderer.Render(view_proj);
    ui.Render();
    renderer.EndPass();
    sg_commit();
}

void cleanup(void) {
    ui.Shutdown();
    audio.Shutdown();
    renderer.Cleanup();

    for (int i = 0; i < MAX_MODELS; ++i) {
        if (models3D[i].vertices) free(models3D[i].vertices);
        if (models3D[i].indices) free(models3D[i].indices);
    }
}

static void input(const sapp_event* ev) {
    if (ev->type == SAPP_EVENTTYPE_RESIZED) renderer.OnResize();
    // forward events to UI manager
    ui.HandleEvent(ev);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 1920;
    desc.height = 1080;
    desc.window_title = "GAME";
    desc.sample_count = 4;
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
}