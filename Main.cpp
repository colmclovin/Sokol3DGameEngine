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

// ECS + Player
#include "src/Game/ECS.h"
#include "src/Game/Player.h"

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

// ECS + player/enemies/trees
static ECS ecs;
static PlayerController* player = nullptr;
static std::vector<EntityId> enemyEntities;
static std::vector<EntityId> treeEntities;
static EntityId groundEntity = -1;

#define MAX_MODELS 16
Model3D models3D[MAX_MODELS];

// Helper function to create a ground quad
Model3D CreateGroundQuad(float size, float uvScale) {
    Model3D quad = {};
    quad.vertex_count = 4;
    quad.index_count = 6;
    
    quad.vertices = (Vertex*)malloc(sizeof(Vertex) * quad.vertex_count);
    quad.indices = (uint16_t*)malloc(sizeof(uint16_t) * quad.index_count);
    
    float half = size / 2.0f;
    
    // Ground quad vertices (facing up, Y = 0)
    // Bottom-left
    quad.vertices[0].pos[0] = -half; quad.vertices[0].pos[1] = 0.0f; quad.vertices[0].pos[2] = -half;
    quad.vertices[0].normal[0] = 0.0f; quad.vertices[0].normal[1] = 1.0f; quad.vertices[0].normal[2] = 0.0f;
    quad.vertices[0].uv[0] = 0.0f; quad.vertices[0].uv[1] = 0.0f;
    quad.vertices[0].color[0] = 0.3f; quad.vertices[0].color[1] = 0.6f; quad.vertices[0].color[2] = 0.3f; quad.vertices[0].color[3] = 1.0f;
    
    // Bottom-right
    quad.vertices[1].pos[0] = half; quad.vertices[1].pos[1] = 0.0f; quad.vertices[1].pos[2] = -half;
    quad.vertices[1].normal[0] = 0.0f; quad.vertices[1].normal[1] = 1.0f; quad.vertices[1].normal[2] = 0.0f;
    quad.vertices[1].uv[0] = uvScale; quad.vertices[1].uv[1] = 0.0f;
    quad.vertices[1].color[0] = 0.3f; quad.vertices[1].color[1] = 0.6f; quad.vertices[1].color[2] = 0.3f; quad.vertices[1].color[3] = 1.0f;
    
    // Top-right
    quad.vertices[2].pos[0] = half; quad.vertices[2].pos[1] = 0.0f; quad.vertices[2].pos[2] = half;
    quad.vertices[2].normal[0] = 0.0f; quad.vertices[2].normal[1] = 1.0f; quad.vertices[2].normal[2] = 0.0f;
    quad.vertices[2].uv[0] = uvScale; quad.vertices[2].uv[1] = uvScale;
    quad.vertices[2].color[0] = 0.3f; quad.vertices[2].color[1] = 0.6f; quad.vertices[2].color[2] = 0.3f; quad.vertices[2].color[3] = 1.0f;
    
    // Top-left
    quad.vertices[3].pos[0] = -half; quad.vertices[3].pos[1] = 0.0f; quad.vertices[3].pos[2] = half;
    quad.vertices[3].normal[0] = 0.0f; quad.vertices[3].normal[1] = 1.0f; quad.vertices[3].normal[2] = 0.0f;
    quad.vertices[3].uv[0] = 0.0f; quad.vertices[3].uv[1] = uvScale;
    quad.vertices[3].color[0] = 0.3f; quad.vertices[3].color[1] = 0.6f; quad.vertices[3].color[2] = 0.3f; quad.vertices[3].color[3] = 1.0f;
    
    // Indices (two triangles)
    quad.indices[0] = 0; quad.indices[1] = 1; quad.indices[2] = 2;
    quad.indices[3] = 0; quad.indices[4] = 2; quad.indices[5] = 3;
    
    return quad;
}

// Helper function to create a textured quad with position and rotation
Model3D CreateTexturedQuad(float width, float height, const char* texturePath = nullptr) {
    Model3D quad = {};
    quad.vertex_count = 4;
    quad.index_count = 6;
    
    quad.vertices = (Vertex*)malloc(sizeof(Vertex) * quad.vertex_count);
    quad.indices = (uint16_t*)malloc(sizeof(uint16_t) * quad.index_count);
    
    float halfW = width / 2.0f;
    float halfH = height / 2.0f;
    
    // Quad vertices (facing camera, centered at origin)
    // Bottom-left
    quad.vertices[0].pos[0] = -halfW; quad.vertices[0].pos[1] = -halfH; quad.vertices[0].pos[2] = 0.0f;
    quad.vertices[0].normal[0] = 0.0f; quad.vertices[0].normal[1] = 0.0f; quad.vertices[0].normal[2] = 1.0f;
    quad.vertices[0].uv[0] = 0.0f; quad.vertices[0].uv[1] = 1.0f;
    quad.vertices[0].color[0] = 1.0f; quad.vertices[0].color[1] = 1.0f; quad.vertices[0].color[2] = 1.0f; quad.vertices[0].color[3] = 1.0f;
    
    // Bottom-right
    quad.vertices[1].pos[0] = halfW; quad.vertices[1].pos[1] = -halfH; quad.vertices[1].pos[2] = 0.0f;
    quad.vertices[1].normal[0] = 0.0f; quad.vertices[1].normal[1] = 0.0f; quad.vertices[1].normal[2] = 1.0f;
    quad.vertices[1].uv[0] = 1.0f; quad.vertices[1].uv[1] = 1.0f;
    quad.vertices[1].color[0] = 1.0f; quad.vertices[1].color[1] = 1.0f; quad.vertices[1].color[2] = 1.0f; quad.vertices[1].color[3] = 1.0f;
    
    // Top-right
    quad.vertices[2].pos[0] = halfW; quad.vertices[2].pos[1] = halfH; quad.vertices[2].pos[2] = 0.0f;
    quad.vertices[2].normal[0] = 0.0f; quad.vertices[2].normal[1] = 0.0f; quad.vertices[2].normal[2] = 1.0f;
    quad.vertices[2].uv[0] = 1.0f; quad.vertices[2].uv[1] = 0.0f;
    quad.vertices[2].color[0] = 1.0f; quad.vertices[2].color[1] = 1.0f; quad.vertices[2].color[2] = 1.0f; quad.vertices[2].color[3] = 1.0f;
    
    // Top-left
    quad.vertices[3].pos[0] = -halfW; quad.vertices[3].pos[1] = halfH; quad.vertices[3].pos[2] = 0.0f;
    quad.vertices[3].normal[0] = 0.0f; quad.vertices[3].normal[1] = 0.0f; quad.vertices[3].normal[2] = 1.0f;
    quad.vertices[3].uv[0] = 0.0f; quad.vertices[3].uv[1] = 0.0f;
    quad.vertices[3].color[0] = 1.0f; quad.vertices[3].color[1] = 1.0f; quad.vertices[3].color[2] = 1.0f; quad.vertices[3].color[3] = 1.0f;
    
    // Indices (two triangles)
    quad.indices[0] = 0; quad.indices[1] = 1; quad.indices[2] = 2;
    quad.indices[3] = 0; quad.indices[4] = 2; quad.indices[5] = 3;
    
    // TODO: Load texture if path provided (when texture system is implemented)
    // For now, texturePath is ignored and vertex colors are used
    
    return quad;
}

// Helper function to add a textured quad entity to ECS
EntityId AddTexturedQuadEntity(ECS& ecs, Renderer& renderer, 
                               float width, float height,
                               hmm_vec3 position, 
                               float yaw, float pitch, float roll,
                               const char* texturePath = nullptr) {
    // Create the quad mesh
    static int quadModelIndex = 4; // Start after existing models
    if (quadModelIndex >= MAX_MODELS) {
        printf("ERROR: Too many models! Increase MAX_MODELS\n");
        return -1;
    }
    
    models3D[quadModelIndex] = CreateTexturedQuad(width, height, texturePath);
    int meshId = renderer.AddMesh(models3D[quadModelIndex]);
    quadModelIndex++;
    
    if (meshId < 0) {
        printf("ERROR: Failed to add quad mesh to renderer!\n");
        return -1;
    }
    
    // Create entity with transform
    EntityId quadEntity = ecs.CreateEntity();
    Transform t;
    t.position = position;
    t.yaw = yaw;
    t.pitch = pitch;
    t.roll = roll;
    
    ecs.AddTransform(quadEntity, t);
    ecs.AddRenderable(quadEntity, meshId, renderer);
    
    printf("Textured quad entity created at (%.2f, %.2f, %.2f) with rotation (yaw=%.2f, pitch=%.2f, roll=%.2f)\n",
           position.X, position.Y, position.Z, yaw, pitch, roll);
    
    return quadEntity;
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
    
    // Create procedural ground quad
    printf("\nCreating ground quad...\n");
    models3D[3] = CreateGroundQuad(5000.0f, 10.0f); // 100x100 size, UV tiled 10x

    printf("\n=== ADDING MESHES TO RENDERER ===\n");
    int meshTree = renderer.AddMesh(models3D[0]);
    printf("Tree mesh ID: %d\n", meshTree);
    
    int meshEnemy = renderer.AddMesh(models3D[1]);
    printf("Enemy mesh ID: %d\n", meshTree);

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
    printf("\n=== CREATING PLAYER ===\n");
    // Use cube instead of test1.glb for better visibility
    player = new PlayerController(ecs, renderer, meshEnemy); // Use the cube model
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
        
        // Random position within a 80x80 area
        float x = ((float)rand() / RAND_MAX - 0.5f) * 2000.0f;
        float z = ((float)rand() / RAND_MAX - 0.5f) * 2000.0f;
        t.position = HMM_Vec3(x, 0.0f, z);
        t.yaw = 0.0f; // Random Y rotation in DEGREES
        t.pitch = 90.0f; // 90 DEGREES to stand trees upright
        t.roll = 0.0f;
        
        printf("Tree %d: pitch=%.4f, yaw=%.4f, roll=%.4f\n", i, t.pitch, t.yaw, t.roll);
        
        ecs.AddTransform(treeId, t);
        ecs.AddRenderable(treeId, meshTree, renderer);
        treeEntities.push_back(treeId);
        
        printf("Tree %d spawned at (%.2f, 0, %.2f)\n", i, x, z);
    }

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
        printf("Enemy %d spawned at (%.2f, 0, %.2f)\n", i, t.position.X, t.position.Z);
    }
    
    printf("\n=== INITIALIZATION COMPLETE ===\n\n");
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
    ui.DebugText("Trees Demo - WASD to move, Mouse to look, Scroll to zoom");

    // update player (process input -> move transform)
    if (player) player->Update(dt);

    // run ECS systems (AI, physics, animation)
    ecs.UpdateAI(dt);
    ecs.UpdatePhysics(dt);
    ecs.UpdateAnimation(dt);

    // sync transforms to renderer instances
    ecs.SyncToRenderer(renderer);

    // compute view-proj using player's camera
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
    if (ev->type == SAPP_EVENTTYPE_RESIZED) renderer.OnResize();
    // forward events to UI manager
    ui.HandleEvent(ev);
    // forward to player for input handling
    if (player) player->HandleEvent(ev);
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
    return desc;
}