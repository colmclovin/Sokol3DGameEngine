#define SOKOL_D3D11
#include "../External/Sokol/sokol_app.h"
#include "../External/Sokol/sokol_fetch.h"
#include "../External/Sokol/sokol_gfx.h"
#include "../External/Sokol/sokol_glue.h"
#include "../External/Sokol/sokol_log.h"
#include <stdio.h>
#include <windows.h>

// forward declare our project logger (defined in src/ThirdParty/SokolLog.cpp)
extern "C" void slog_to_debug(const char* tag,
                              uint32_t log_level,
                              uint32_t log_item_id,
                              const char* message_or_null,
                              uint32_t line_nr,
                              const char* filename_or_null,
                              void* user_data);

// Include HandmadeMath header only (implementation moved to a single TU)
#include "../External/HandmadeMath.h"

#include "../External/Imgui/imgui.h"
#include "../External/stb_image.h"
#include "../External/Assimp/include/assimp/Importer.hpp"
#include "../External/Assimp/include/assimp/postprocess.h"
#include "../External/Assimp/include/assimp/scene.h"
#include "src/Renderer/Renderer.h"
#include "src/Audio/AudioEngine.h"
#include "src/Model/ModelLoader.h"
#include "src/UI/UIManager.h"

// ECS + Player + GameState
#include "src/Game/ECS.h"
#include "src/Game/Player.h"
#include "src/Game/GameState.h"

// Quad geometry utilities
#include "src/Geometry/Quad.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <cmath>
#include <vector>

static Renderer renderer;
static AudioEngine audio;
static ModelLoader loader;
static UIManager ui;
static GameStateManager gameState;

// ECS + player/enemies/trees
static ECS ecs;
static PlayerController* player = nullptr;
static std::vector<EntityId> enemyEntities;
static std::vector<EntityId> treeEntities;
static std::vector<EntityId> UIEntities;
static EntityId hudQuadEntity = -1;
static EntityId billboardQuadEntity = -1;
static EntityId groundEntity = -1;

#define MAX_MODELS 16
Model3D models3D[MAX_MODELS];

static bool mouse_locked = true;

// Callback for when GUI visibility changes
void OnGuiVisibilityChanged(bool visible) {
    if (visible) {
        // GUI opened - unlock mouse and disable player input
        sapp_lock_mouse(false);
        sapp_show_mouse(true);
        if (player) {
            player->SetInputEnabled(false);
        }
        printf("GUI opened - mouse unlocked, camera frozen\n");
    } else {
        // GUI closed - restore mouse lock and enable player input
        if (mouse_locked) {
            sapp_lock_mouse(true);
            sapp_show_mouse(false);
        }
        if (player) {
            player->SetInputEnabled(true);
        }
        printf("GUI closed - mouse locked, camera active\n");
    }
}

void init(void) {
    // Allocate console for debug output (Windows only)
    #ifdef _WIN32
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    printf("=== DEBUG CONSOLE ALLOCATED ===\n");
    #endif

    // Capture and lock the cursor
    sapp_lock_mouse(true);
    sapp_show_mouse(false);

    // audio
    audio.Init();
    audio.LoadWav("assets/audio/TetrisTheme.wav");

    // sokol
    sg_desc desc = {};
    desc.environment = sglue_environment();
    // use our custom logger implementation (avoid clash with Sokol's default slog_func)
    desc.logger.func = slog_to_debug;
    sg_setup(&desc);

    // UI
    ui.Setup();
    
    // Set callback for GUI visibility changes (IMPORTANT - THIS WAS MISSING!)
    ui.SetGuiVisibilityCallback(OnGuiVisibilityChanged);

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
        
        // Game State controls
        ImGui::Separator();
        ImGui::Text("Game State:");
        const char* currentMode = gameState.IsPlaying() ? "PLAYING" : "EDIT";
        ImGui::Text("Current Mode: %s", currentMode);
        ImGui::Text("Press TAB to toggle modes");
        if (ImGui::Button("Toggle Mode (TAB)")) {
            gameState.ToggleMode();
        }
        
        // Camera controls
        if (player) {
            ImGui::Separator();
            player->GetCamera().RenderImGuiControls();
        }
    });

    // renderer
    printf("=== INITIALIZING RENDERER ===\n");
    renderer.Init();

    // models via ModelLoader
    printf("\n=== LOADING MODELS ===\n");
    printf("Loading tree model...\n");
    models3D[0] = loader.LoadModel("assets/models/cartoon_lowpoly_trees_blend.glb");
    printf("Loading enemy model...\n");
    models3D[1] = loader.LoadModel("assets/models/cube.glb");
    printf("\nLoading player model...\n");
    models3D[2] = loader.LoadModel("assets/models/test1.glb");
    
    // Create procedural ground quad using Quad utility
    printf("\nCreating ground quad...\n");
    models3D[3] = QuadGeometry::CreateGroundQuad(5000.0f, 10.0f); // 5000x5000 size, UV tiled 10x
   

    printf("\n=== ADDING MESHES TO RENDERER ===\n");
    int meshTree = renderer.AddMesh(models3D[0]);
    printf("Tree mesh ID: %d\n", meshTree);
    
    int meshEnemy = renderer.AddMesh(models3D[1]);
    printf("Enemy mesh ID: %d\n", meshEnemy);

    int meshPlayer = renderer.AddMesh(models3D[2]);
    printf("Player mesh ID: %d\n", meshPlayer);
    
    int meshGround = renderer.AddMesh(models3D[3]);
    printf("Ground mesh ID: %d\n", meshGround);

    if (meshTree < 0) {
        printf("ERROR: Failed to add tree mesh!\n");
    }
    if (meshPlayer < 0) {
        printf("ERROR: Failed to add player mesh!\n");
    }
    if (meshGround < 0) {
        printf("ERROR: Failed to add ground mesh!\n");
    }


        // Create player (ECS-driven)
    printf("\n=== CREATING UI ===\n");
    player = new PlayerController(ecs, renderer, meshPlayer, gameState);
    player->Spawn(HMM_Vec3(0.0f, 0.0f, 0.0f));
    printf("Player spawned at (0, 0, 0)\n");

    // Create ground entity (static, no AI)
    printf("\n=== CREATING GROUND ===\n");
    groundEntity = ecs.CreateEntity();
    Transform groundTransform;
    groundTransform.position = HMM_Vec3(0.0f, -4.0f, 0.0f);
    groundTransform.yaw = 0.0f;
    ecs.AddTransform(groundEntity, groundTransform);
    ecs.AddRenderable(groundEntity, meshGround, renderer);
    printf("Ground created\n");





    // Spawn trees randomly across the map
    printf("\n=== SPAWNING TREES ===\n");
    treeEntities.clear();
    srand((unsigned int)time(nullptr));
    for (int i = 0; i < 100; ++i) {
        EntityId treeId = ecs.CreateEntity();
        Transform t;
        
        // Random position within a large area
        float x = ((float)rand() / RAND_MAX - 0.5f) * 2000.0f;
        float z = ((float)rand() / RAND_MAX - 0.5f) * 2000.0f;
        t.position = HMM_Vec3(x, 0.0f, z);
        t.yaw = 0.0f;
        t.pitch = 90.0f; // 90 DEGREES to stand trees upright
        t.roll = 0.0f;
        
        ecs.AddTransform(treeId, t);
        ecs.AddRenderable(treeId, meshTree, renderer);
        treeEntities.push_back(treeId);
    }
    printf("Spawned %zu trees\n", treeEntities.size());

    // spawn wandering enemies
    printf("\n=== SPAWNING ENEMIES ===\n");
    enemyEntities.clear();
    for (int i = 0; i < 10; ++i) {
        EntityId eid = ecs.CreateEntity();
        Transform t;
        float angle = (float)i * (3.14159f * 2.0f / 10.0f);
        float radius = 15.0f;
        t.position = HMM_Vec3(sinf(angle) * radius, 0.0f, cosf(angle) * radius);
        t.yaw = angle + 3.14159f;
        ecs.AddTransform(eid, t);
        
        // give them AI and animator
        AIController ai;
        ai.state = AIState::Wander;
        ai.stateTimer = 3.0f + ((float)rand() / RAND_MAX) * 4.0f;
        ai.wanderTarget = HMM_AddVec3(t.position, HMM_Vec3(((float)rand()/RAND_MAX - 0.5f)*10.0f, 0.0f, ((float)rand()/RAND_MAX - 0.5f)*10.0f));
        
        ecs.AddAI(eid, ai);
        Animator anim;
        anim.currentClip = -1;
        anim.time = 0.0f;
        ecs.AddAnimator(eid, anim);
        ecs.AddRenderable(eid, meshEnemy, renderer);
        enemyEntities.push_back(eid);
    }
    printf("Spawned %zu enemies\n", enemyEntities.size());
    
    // === CREATE HUD QUAD ===
    printf("\n=== CREATING HUD QUAD ===\n");
    printf("Loading texture from: assets/textures/HUD.jpeg\n");
    models3D[4] = QuadGeometry::CreateTexturedQuad(0.2f, 0.2f,"assets/textures/pikman.png");
    printf("Texture loaded: has_texture=%d, width=%d, height=%d\n", 
           models3D[4].has_texture, models3D[4].texture_width, models3D[4].texture_height);
    int meshHUD = renderer.AddMesh(models3D[4]);
    
    if (meshHUD >= 0) {
        hudQuadEntity = ecs.CreateEntity();
        Transform hudTransform;
        hudTransform.position = HMM_Vec3(0.0f, 0.0f, 0.0f); // Will be set by UpdateScreenSpace
        hudTransform.yaw = 0.0f;
        hudTransform.pitch = 0.0f;
        hudTransform.roll = 0.0f;
        
        ecs.AddTransform(hudQuadEntity, hudTransform);
        int hudInstanceId = ecs.AddRenderable(hudQuadEntity, meshHUD, renderer);
        
        // Mark as screen-space
        renderer.SetInstanceScreenSpace(hudInstanceId, true);
        
        // Add screen-space component (top-right corner)
        ScreenSpace ss;
        ss.screenPosition = HMM_Vec2(0.85f, 0.15f); // Top-right corner
        ss.size = HMM_Vec2(0.1f, 0.1f);
        ss.depth = 0.0f;
        ecs.AddScreenSpace(hudQuadEntity, ss);
        
        UIEntities.push_back(hudQuadEntity);
        printf("HUD quad created in screen space at (%.2f, %.2f)\n", 
               ss.screenPosition.X, ss.screenPosition.Y);
    }
    
    // === CREATE BILLBOARD QUAD (follows player) ===
    printf("\n=== CREATING BILLBOARD QUAD ===\n");
    models3D[5] = QuadGeometry::CreateTexturedQuad(1.0f, 1.0f, nullptr); // Larger quad for billboard
    int meshBillboard = renderer.AddMesh(models3D[5]);
    
    if (meshBillboard >= 0 && player) {
        billboardQuadEntity = ecs.CreateEntity();
        Transform billboardTransform;
        // Initial position (will be updated to follow player)
        billboardTransform.position = HMM_Vec3(0.0f, 2.0f, 0.0f);
        billboardTransform.yaw = 0.0f;
        billboardTransform.pitch = 0.0f;
        billboardTransform.roll = 0.0f;
        
        ecs.AddTransform(billboardQuadEntity, billboardTransform);
        ecs.AddRenderable(billboardQuadEntity, meshBillboard, renderer);
        
        // Add billboard component to make it follow player and face camera
        Billboard billboardComp;
        billboardComp.followTarget = player->Entity(); // Follow the player
        billboardComp.offset = HMM_Vec3(1.0f, 2.5f, 0.0f); // Hover 2.5 units above player
        billboardComp.lockY = false; // Cylindrical billboard (only rotate around Y)
        
        ecs.AddBillboard(billboardQuadEntity, billboardComp);
        UIEntities.push_back(billboardQuadEntity);
        printf("Billboard quad created, following player with offset (%.2f, %.2f, %.2f)\n",
               billboardComp.offset.X, billboardComp.offset.Y, billboardComp.offset.Z);
    }
    
    // === CREATE LIGHTS ===
    printf("\n=== CREATING LIGHTS ===\n");
    std::vector<EntityId> lightEntities;

    // Create several point lights around the scene
    for (int i = 0; i < 16; ++i) {
        EntityId lightId = ecs.CreateEntity();
        Transform t;
        float angle = (float)i * (3.14159f * 2.0f / 4.0f);
        float radius = 25.0f;
        t.position = HMM_Vec3(sinf(angle) * radius, 3.0f, cosf(angle) * radius);
        t.yaw = 0.0f;
        
        ecs.AddTransform(lightId, t);
        
        Light light;
        light.color = HMM_Vec3(1.0f, 0.8f, 0.6f); // Warm white
        light.intensity = 10.0f;
        light.radius = 30.0f;
        light.enabled = true;
        
        ecs.AddLight(lightId, light);
        lightEntities.push_back(lightId);
        
        printf("Light %d created at (%.1f, %.1f, %.1f)\n", i, t.position.X, t.position.Y, t.position.Z);
    }

    printf("Created %zu lights\n", lightEntities.size());
    
    // === CREATE SUN ===
    printf("\n=== CREATING SUN ===\n");
    // Sun pointing down and slightly from the side (like afternoon sun)
    hmm_vec3 sunDirection = HMM_Vec3(0.0f, 1.0f, 0.0f); // Direction the sun is pointing
    hmm_vec3 sunColor = HMM_Vec3(1.0f, 0.95f, 0.8f); // Warm sunlight
    float sunIntensity = 1.0f; // Strong sun
    renderer.SetSunLight(sunDirection, sunColor, sunIntensity);
    printf("Sun created: direction=(%.2f, %.2f, %.2f), intensity=%.2f\n",
           sunDirection.X, sunDirection.Y, sunDirection.Z, sunIntensity);
    
    printf("\n=== INITIALIZATION COMPLETE ===\n");
    printf("Press TAB to toggle between PLAYING and EDIT modes\n");
    printf("Press F1 to open/close Debug GUI\n");
    printf("Press F11 to toggle fullscreen\n\n");
}

void frame(void) {
    float dt = (float)sapp_frame_duration();
    const int width = sapp_width();
    const int height = sapp_height();
    ui.NewFrame(width, height, dt, sapp_dpi_scale());

    // show debug text via UIManager
    ui.SetDebugCanvas(sapp_widthf(), sapp_heightf());
    ui.SetDebugOrigin(2.0f, 2.0f);
    ui.SetDebugColor(255, 255, 0);
    const char* modeText = gameState.IsPlaying() ? "PLAYING" : "EDIT";
    char debugText[256];
    snprintf(debugText, sizeof(debugText), 
             "Trees Demo - WASD to move, Mouse to look, Scroll to zoom, TAB to toggle mode, F1 for GUI, F11 for fullscreen\nCurrent: %s", 
             modeText);
    ui.DebugText(debugText);

    // update player
    if (player) player->Update(dt);

    // run ECS systems
    if (gameState.IsPlaying()) {
        ecs.UpdateAI(dt);
        ecs.UpdatePhysics(dt);
        ecs.UpdateAnimation(dt);
    }
    
    // Update billboards
    if (player) {
        ecs.UpdateBillboards(player->CameraPosition());
    }
    
    // Update screen-space positions
    ecs.UpdateScreenSpace((float)width, (float)height);

    // sync transforms to renderer
    ecs.SyncToRenderer(renderer);

    // compute view-proj for 3D rendering
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    hmm_mat4 proj = HMM_Perspective(60.0f, w/h, 0.01f, 1000.0f);
    hmm_mat4 view;
    if (player) {
        view = player->GetViewMatrix();
    } else {
        hmm_vec3 cam_pos = HMM_Vec3(0.0f, 2.0f, 50.0f);
        hmm_vec3 target = HMM_Vec3(0.0f, 0.0f, 0.0f);
        view = HMM_LookAt(cam_pos, target, HMM_Vec3(0.0f,1.0f,0.0f));
    }
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    
    // Gather active lights and pass to renderer
    std::vector<hmm_vec3> lightPositions;
    std::vector<hmm_vec3> lightColors;
    std::vector<float> lightIntensities;
    std::vector<float> lightRadii;

    const auto& lights = ecs.GetLights();
    const auto& transforms = ecs.GetTransforms();

    for (const auto& [entityId, light] : lights) {
        if (!light.enabled) continue;
        
        auto transIt = transforms.find(entityId);
        if (transIt != transforms.end()) {
            lightPositions.push_back(transIt->second.position);
            lightColors.push_back(light.color);
            lightIntensities.push_back(light.intensity);
            lightRadii.push_back(light.radius);
        }
    }

    renderer.SetLights(lightPositions, lightColors, lightIntensities, lightRadii);
    renderer.SetCameraPosition(player ? player->CameraPosition() : HMM_Vec3(0, 0, 0));

    // Create identity projection for screen-space (quad is already in NDC)
    hmm_mat4 ortho = HMM_Mat4d(1.0f); // Identity - no transformation

    renderer.BeginPass();
    renderer.Render(view_proj); // Now with lighting!
    renderer.RenderScreenSpace(ortho); // Render HUD with identity projection
    ui.Render();
    renderer.EndPass();
    sg_commit();
}

void cleanup(void) {
    ui.Shutdown();
    audio.Shutdown();
    renderer.Cleanup();

    if (player) {
        delete player;
        player = nullptr;
    }

    // Simplified cleanup - only free vertex/index data
    for (int i = 0; i < MAX_MODELS; ++i) {
        if (models3D[i].vertices) {
            free(models3D[i].vertices);
            models3D[i].vertices = nullptr;
        }
        if (models3D[i].indices) {
            free(models3D[i].indices);
            models3D[i].indices = nullptr;
        }
    }
}

static void input(const sapp_event* ev) {
    // Handle resize events - this triggers on fullscreen toggle too
    if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        printf("Window resized to %dx%d - updating renderer...\n", ev->window_width, ev->window_height);
        renderer.OnResize();
    }
    
    // Handle F11 for fullscreen toggle
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_F11) {
        sapp_toggle_fullscreen();
        printf("Fullscreen toggled\n");
        // Note: The SAPP_EVENTTYPE_RESIZED event will be triggered automatically
        // and will call renderer.OnResize(), so we don't need to do it here
        return; // Consume the event
    }
    
    // forward events to UI manager
    ui.HandleEvent(ev);
    
    // forward to player for input handling (IMPORTANT - ONLY IF GUI NOT VISIBLE!)
    if (player && !ui.IsGuiVisible()) {
        player->HandleEvent(ev);
    }
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
    desc.window_title = "GAME - Trees & Ground Demo";
    desc.sample_count = 4;
    desc.icon.sokol_default = true;
    desc.logger.func = slog_to_debug;
    desc.fullscreen = false; // Start in windowed mode
    return desc;
}