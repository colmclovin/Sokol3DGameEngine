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
#include <unordered_map>
#include <algorithm>  // ADD THIS LINE for std::find
// Add at the top with other static variables (around line 47):
static int meshTreeId = -1;
static int meshEnemyId = -1;
static int meshPlayerId = -1;
static int meshGroundId = -1;

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
static std::vector<EntityId> lightEntities;
static EntityId hudQuadEntity = -1;
static EntityId billboardQuadEntity = -1;
static EntityId groundEntity = -1;

// Wireframe system for edit mode
static int wireframeYellowMeshId = -1; // Unselected wireframe (yellow)
static int wireframeOrangeMeshId = -1; // Selected wireframe (orange)
static std::unordered_map<EntityId, EntityId> entityWireframes; // Map entity -> wireframe entity

// Selection state
static EntityId selectedEntity = -1;

// Entity placement system for edit mode
static int placementMeshId = -1; // Which mesh to place
static bool placementMode = false; // Is placement mode active?
static EntityId ghostEntity = -1; // Preview entity while placing

#define MAX_MODELS 32  // Increased to accommodate wireframe meshes
Model3D models3D[MAX_MODELS];

static bool mouse_locked = true;

// Helper to update mouse/cursor state based on game mode
void UpdateCursorState() {
    if (gameState.IsEdit()) {
        // EDIT MODE: Always show cursor, unlock mouse
        sapp_lock_mouse(false);
        sapp_show_mouse(true);
        mouse_locked = false;
        if (player) {
            player->SetInputEnabled(!ui.IsGuiVisible()); // Disable input when GUI is visible
        }
    } else {
        // PLAYING MODE: Lock cursor when GUI is closed
        if (ui.IsGuiVisible()) {
            sapp_lock_mouse(false);
            sapp_show_mouse(true);
            mouse_locked = false;
            if (player) {
                player->SetInputEnabled(false);
            }
        } else {
            sapp_lock_mouse(true);
            sapp_show_mouse(false);
            mouse_locked = true;
            if (player) {
                player->SetInputEnabled(true);
            }
        }
    }
}

// Callback for when GUI visibility changes
void OnGuiVisibilityChanged(bool visible) {
    UpdateCursorState();
    printf("GUI %s\n", visible ? "opened" : "closed");
}

// Helper to render entity inspector panel
void RenderEntityInspector() {
    if (selectedEntity == -1) {
        ImGui::Text("No entity selected");
        ImGui::Text("Click on an entity in Edit mode to select it");
        return;
    }
    
    Selectable* sel = ecs.GetSelectable(selectedEntity);
    if (!sel) {
        ImGui::Text("Selected entity has no Selectable component");
        return;
    }
    
    ImGui::Text("Entity ID: %d", selectedEntity);
    ImGui::Text("Name: %s", sel->name);
    ImGui::Separator();
    
    // Transform component
    Transform* t = ecs.GetTransform(selectedEntity);
    if (t) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &t->position.X, 0.1f);
            ImGui::DragFloat("Yaw", &t->yaw, 1.0f);
            ImGui::DragFloat("Pitch", &t->pitch, 1.0f);
            ImGui::DragFloat("Roll", &t->roll, 1.0f);
        }
    }
    
    // Light component
    Light* light = ecs.GetLight(selectedEntity);
    if (light) {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Color", &light->color.X);
            ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Radius", &light->radius, 0.5f, 0.1f, 1000.0f);
            ImGui::Checkbox("Enabled", &light->enabled);
        }
    }
    
    // Rigidbody component
    Rigidbody* rb = ecs.GetRigidbody(selectedEntity);
    if (rb) {
        if (ImGui::CollapsingHeader("Rigidbody")) {
            ImGui::DragFloat3("Velocity", &rb->velocity.X, 0.1f);
            ImGui::DragFloat("Mass", &rb->mass, 0.1f, 0.1f, 1000.0f);
            ImGui::Checkbox("Affected By Gravity", &rb->affectedByGravity);
        }
    }
    
    // AI component
    AIController* ai = ecs.GetAI(selectedEntity);
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
    Billboard* billboard = ecs.GetBillboard(selectedEntity);
    if (billboard) {
        if (ImGui::CollapsingHeader("Billboard")) {
            ImGui::InputInt("Follow Target ID", &billboard->followTarget);
            ImGui::DragFloat3("Offset", &billboard->offset.X, 0.1f);
            ImGui::Checkbox("Lock Y Axis", &billboard->lockY);
        }
    }
    
    // Screen Space component
    ScreenSpace* ss = ecs.GetScreenSpace(selectedEntity);
    if (ss) {
        if (ImGui::CollapsingHeader("Screen Space")) {
            ImGui::DragFloat2("Screen Position", &ss->screenPosition.X, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat2("Size", &ss->size.X, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Depth", &ss->depth, 0.01f, 0.0f, 1.0f);
        }
    }
    
    ImGui::Separator();
    if (ImGui::Button("Deselect")) {
        selectedEntity = -1;
        // Clear selection flag
        if (sel) sel->isSelected = false;
    }
}

// Helper to create a wireframe box mesh (using thin triangles for lines)
Model3D CreateWireframeBox(float sizeX, float sizeY, float sizeZ, float r, float g, float b) {
    Model3D box = {};
    
    // We'll create thin quads for each edge to simulate lines
    // 12 edges * 4 vertices per quad = 48 vertices
    // 12 edges * 6 indices per quad = 72 indices
    box.vertex_count = 48;
    box.vertices = (Vertex*)malloc(48 * sizeof(Vertex));
    
    box.index_count = 72;
    box.indices = (uint16_t*)malloc(72 * sizeof(uint16_t));
    
    float hx = sizeX * 0.5f;
    float hy = sizeY * 0.5f;
    float hz = sizeZ * 0.5f;
    float lineWidth = 0.05f; // Width of the "line" quads (made thicker for visibility)
    
    // Define 8 corners of the box
    hmm_vec3 corners[8] = {
        HMM_Vec3(-hx, -hy, -hz), // 0: bottom-front-left
        HMM_Vec3( hx, -hy, -hz), // 1: bottom-front-right
        HMM_Vec3( hx,  hy, -hz), // 2: top-front-right
        HMM_Vec3(-hx,  hy, -hz), // 3: top-front-left
        HMM_Vec3(-hx, -hy,  hz), // 4: bottom-back-left
        HMM_Vec3( hx, -hy,  hz), // 5: bottom-back-right
        HMM_Vec3( hx,  hy,  hz), // 6: top-back-right
        HMM_Vec3(-hx,  hy,  hz)  // 7: top-back-left
    };
    
    // Define 12 edges as pairs of corner indices
    int edges[12][2] = {
        // Bottom face
        {0, 1}, {1, 5}, {5, 4}, {4, 0},
        // Top face
        {3, 2}, {2, 6}, {6, 7}, {7, 3},
        // Vertical edges
        {0, 3}, {1, 2}, {5, 6}, {4, 7}
    };
    
    int vIdx = 0;
    int iIdx = 0;
    
    // Create a thin quad for each edge
    for (int e = 0; e < 12; ++e) {
        hmm_vec3 p1 = corners[edges[e][0]];
        hmm_vec3 p2 = corners[edges[e][1]];
        
        // Calculate edge direction and perpendicular
        hmm_vec3 edgeDir = HMM_NormalizeVec3(HMM_SubtractVec3(p2, p1));
        hmm_vec3 up = HMM_Vec3(0.0f, 1.0f, 0.0f);
        hmm_vec3 perp = HMM_NormalizeVec3(HMM_Cross(edgeDir, up));
        
        // If perpendicular is zero (edge is vertical), use different perpendicular
        if (HMM_LengthVec3(perp) < 0.001f) {
            perp = HMM_Vec3(1.0f, 0.0f, 0.0f);
        }
        
        // Create 4 vertices for a thin quad representing the line
        for (int i = 0; i < 4; ++i) {
            Vertex& v = box.vertices[vIdx + i];
            
            // Determine position based on quad corner
            hmm_vec3 basePos = (i < 2) ? p1 : p2;
            hmm_vec3 offset = HMM_Vec3(
                perp.X * lineWidth * ((i % 2 == 0) ? -1.0f : 1.0f),
                perp.Y * lineWidth * ((i % 2 == 0) ? -1.0f : 1.0f),
                perp.Z * lineWidth * ((i % 2 == 0) ? -1.0f : 1.0f)
            );
            hmm_vec3 finalPos = HMM_AddVec3(basePos, offset);
            
            v.pos[0] = finalPos.X;
            v.pos[1] = finalPos.Y;
            v.pos[2] = finalPos.Z;
            
            v.normal[0] = perp.X;
            v.normal[1] = perp.Y;
            v.normal[2] = perp.Z;
            
            v.uv[0] = 0.0f;
            v.uv[1] = 0.0f;
            
            // Use provided color
            v.color[0] = r;
            v.color[1] = g;
            v.color[2] = b;
            v.color[3] = 1.0f;
        }
        
        // Create indices for the quad (2 triangles)
        box.indices[iIdx++] = (uint16_t)(vIdx + 0);
        box.indices[iIdx++] = (uint16_t)(vIdx + 1);
        box.indices[iIdx++] = (uint16_t)(vIdx + 2);
        
        box.indices[iIdx++] = (uint16_t)(vIdx + 2);
        box.indices[iIdx++] = (uint16_t)(vIdx + 3);
        box.indices[iIdx++] = (uint16_t)(vIdx + 0);
        
        vIdx += 4;
    }
    
    box.has_texture = false;
    box.texture_data = nullptr;
    
    return box;
}

// Helper to create or update wireframe for an entity
void CreateOrUpdateWireframe(EntityId entityId, bool isSelected) {
    Selectable* sel = ecs.GetSelectable(entityId);
    Transform* t = ecs.GetTransform(entityId);
    if (!sel || !t) return;
    
    // Check if wireframe already exists
    EntityId wireframeEntity = -1;
    auto it = entityWireframes.find(entityId);
    
    if (it == entityWireframes.end()) {
        // Create new wireframe entity
        wireframeEntity = ecs.CreateEntity();
        entityWireframes[entityId] = wireframeEntity;
        
        // Add transform and renderable
        Transform wireTransform;
        wireTransform.position = t->position;
        ecs.AddTransform(wireframeEntity, wireTransform);
        
        // Use appropriate color mesh
        int meshId = isSelected ? wireframeOrangeMeshId : wireframeYellowMeshId;
        ecs.AddRenderable(wireframeEntity, meshId, renderer);
    } else {
        wireframeEntity = it->second;
    }
    
    // Update wireframe transform to match entity
    Transform* wireTransform = ecs.GetTransform(wireframeEntity);
    if (wireTransform) {
        wireTransform->position = t->position;
        
        // Scale to match bounding radius
        float size = sel->boundingRadius * 2.0f * 1.15f; // Slightly larger
        hmm_mat4 translation = HMM_Translate(t->position);
        hmm_mat4 scale = HMM_Scale(HMM_Vec3(size / 2.0f, size / 2.0f, size / 2.0f));
        wireTransform->customMatrix = HMM_MultiplyMat4(translation, scale);
        wireTransform->useCustomMatrix = true;
    }
    
    // Update mesh if selection changed
    int currentMeshId = ecs.GetMeshId(wireframeEntity);
    int desiredMeshId = isSelected ? wireframeOrangeMeshId : wireframeYellowMeshId;
    if (currentMeshId != desiredMeshId) {
        // Remove old renderable and add new one with different color
        ecs.RemoveRenderable(wireframeEntity, renderer);
        ecs.AddRenderable(wireframeEntity, desiredMeshId, renderer);
    }
}

// Helper to properly destroy wireframe entities when switching to playing mode
void DestroyAllWireframes() {
    printf("DestroyAllWireframes called - destroying %zu wireframes\n", entityWireframes.size());
    
    for (auto& [entityId, wireframeEntity] : entityWireframes) {
        printf("  Destroying wireframe entity %d for entity %d\n", wireframeEntity, entityId);
        
        // Get the instance ID before destroying
        int instanceId = ecs.GetInstanceId(wireframeEntity);
        
        // Remove from renderer first
        ecs.RemoveRenderable(wireframeEntity, renderer);
        
        // Destroy the entity from ECS
        ecs.DestroyEntity(wireframeEntity);
        
        printf("    Removed instance %d, destroyed entity %d\n", instanceId, wireframeEntity);
    }
    entityWireframes.clear();
    printf("DestroyAllWireframes complete - map cleared\n");
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

    // Start in playing mode with locked cursor
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
    
    // Set callback for GUI visibility changes
    ui.SetGuiVisibilityCallback(OnGuiVisibilityChanged);

    // register a simple ImGui callback (you can add more / modify)
    ui.AddGuiCallback([](){
        // Audio controls
        //ImGui::Separator();
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
            UpdateCursorState(); // Update cursor when mode changes
        }
        
        // Entity Inspector (EDIT MODE ONLY)
        if (gameState.IsEdit()) {
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Entity Inspector", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderEntityInspector();
            }
            
            // Entity Placement (EDIT MODE ONLY) - ADD THIS BLOCK
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Entity Placement")) {
                ImGui::Text("Press 'G' to grab/place selected entity");
                ImGui::Text("Press 'Delete' to delete selected entity");
                
                const char* entityTypes[] = { "Tree", "Enemy", "Light" };
                static int selectedType = 0;
                ImGui::Combo("Entity Type", &selectedType, entityTypes, 3);
                
                if (ImGui::Button("Start Placement Mode (P)")) {
                    placementMode = !placementMode;
                    if (placementMode) {
                        // Set mesh based on selection
                        if (selectedType == 0) placementMeshId = meshTreeId; // Tree - USE ACTUAL ID
                        else if (selectedType == 1) placementMeshId = meshEnemyId; // Enemy - USE ACTUAL ID
                        else placementMeshId = -1; // Light (no mesh)
                    }
                }
                
                if (placementMode) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "PLACEMENT MODE ACTIVE");
                    ImGui::Text("Click to place, ESC to cancel");
                }
            }
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
    meshTreeId = renderer.AddMesh(models3D[0]);
    printf("Tree mesh ID: %d\n", meshTreeId);
    
    meshEnemyId = renderer.AddMesh(models3D[1]);
    printf("Enemy mesh ID: %d\n", meshEnemyId);

    meshPlayerId = renderer.AddMesh(models3D[2]);
    printf("Player mesh ID: %d\n", meshPlayerId);
    
    meshGroundId = renderer.AddMesh(models3D[3]);
    printf("Ground mesh ID: %d\n", meshGroundId);

    if (meshTreeId < 0) {
        printf("ERROR: Failed to add tree mesh!\n");
    }
    if (meshPlayerId < 0) {
        printf("ERROR: Failed to add player mesh!\n");
    }
    if (meshGroundId < 0) {
        printf("ERROR: Failed to add ground mesh!\n");
    }

    // Create wireframe meshes (two colors: yellow for unselected, orange for selected)
    printf("\n=== CREATING WIREFRAME MESHES ===\n");
    models3D[6] = CreateWireframeBox(2.0f, 2.0f, 2.0f, 1.0f, 1.0f, 0.0f); // Yellow
    wireframeYellowMeshId = renderer.AddMesh(models3D[6]);
    printf("Yellow wireframe mesh ID: %d\n", wireframeYellowMeshId);
    
    models3D[7] = CreateWireframeBox(2.0f, 2.0f, 2.0f, 1.0f, 0.5f, 0.0f); // Orange
    wireframeOrangeMeshId = renderer.AddMesh(models3D[7]);
    printf("Orange wireframe mesh ID: %d\n", wireframeOrangeMeshId);

    // Create player (ECS-driven)
    printf("\n=== CREATING PLAYER ===\n");
    player = new PlayerController(ecs, renderer, meshPlayerId, gameState);
    player->Spawn(HMM_Vec3(0.0f, 0.0f, 0.0f));
    printf("Player spawned at (0, 0, 0)\n");

    // Create ground entity (static, no AI)
    printf("\n=== CREATING GROUND ===\n");
    groundEntity = ecs.CreateEntity();
    Transform groundTransform;
    groundTransform.position = HMM_Vec3(0.0f, -4.0f, 0.0f);
    groundTransform.yaw = 0.0f;
    ecs.AddTransform(groundEntity, groundTransform);
    ecs.AddRenderable(groundEntity, meshGroundId, renderer);
    
    // Add Selectable to ground (FIXED: Reduced bounding radius)
    Selectable groundSel;
    groundSel.name = "Ground";
    groundSel.boundingRadius = 10.0f; // CHANGED from 50.0f to 10.0f
    ecs.AddSelectable(groundEntity, groundSel);
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
        ecs.AddRenderable(treeId, meshTreeId, renderer);
        
        // Add Selectable to trees
        Selectable treeSel;
        treeSel.name = "Tree";
        treeSel.boundingRadius = 3.0f; // Approximate size
        ecs.AddSelectable(treeId, treeSel);
        
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
        ecs.AddRenderable(eid, meshEnemyId, renderer);
        
        // Add Selectable to enemies
        Selectable enemySel;
        enemySel.name = "Enemy";
        enemySel.boundingRadius = 1.5f;
        ecs.AddSelectable(eid, enemySel);
        
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
    lightEntities.clear();

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
        
        // Add Selectable to lights
        Selectable lightSel;
        lightSel.name = "Point Light";
        lightSel.boundingRadius = 2.0f;
        ecs.AddSelectable(lightId, lightSel);
        
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
    printf("Press F11 to toggle fullscreen\n");
    printf("In EDIT mode, all selectable entities show yellow wireframes\n");
    printf("Selected entities show orange wireframes\n\n");
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
             "Trees Demo - WASD to move, Mouse to look, Scroll to zoom, TAB to toggle mode, F1 for GUI, F11 for fullscreen\nCurrent: %s%s", 
             modeText,
             (gameState.IsEdit() && selectedEntity != -1) ? " | Entity Selected" : "");
    ui.DebugText(debugText);

    // FIXED: Proper wireframe management with mode tracking
    static EntityId previousSelection = -1;
    static bool wasInEditMode = false;
    
    bool modeChanged = (gameState.IsEdit() != wasInEditMode);
    bool isEditMode = gameState.IsEdit();
    
    // CRITICAL: Destroy wireframes BEFORE any ECS updates
    if (!isEditMode && !entityWireframes.empty()) {
        printf("=== PLAYING MODE: Destroying wireframes ===\n");
        DestroyAllWireframes();
        previousSelection = -1;
    }
    
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
    
    // Update wireframes only in edit mode
    if (isEditMode) {
        const auto& selectables = ecs.GetSelectables();
        bool selectionChanged = (selectedEntity != previousSelection);
        
        for (const auto& [entityId, selectable] : selectables) {
            bool isSelected = (entityId == selectedEntity);
            bool forceUpdate = (selectionChanged && (entityId == selectedEntity || entityId == previousSelection)) || modeChanged;
            
            if (forceUpdate) {
                auto it = entityWireframes.find(entityId);
                if (it != entityWireframes.end()) {
                    EntityId oldWireframe = it->second;
                    ecs.RemoveRenderable(oldWireframe, renderer);
                    ecs.DestroyEntity(oldWireframe);
                    entityWireframes.erase(it);
                }
            }
            
            CreateOrUpdateWireframe(entityId, isSelected);
        }
        
        previousSelection = selectedEntity;
    }
    
    wasInEditMode = isEditMode;

    // Sync transforms to renderer
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

    // TEMPORARY DEBUG - Remove after testing
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) { // Print once per second (at 60fps)
        printf("\n=== LIGHT DEBUG (Frame %d) ===\n", frameCount);
        printf("Total light entities: %zu\n", lights.size());
        printf("Total transform entities: %zu\n", transforms.size());
    }

    int activeLightCount = 0;
    for (const auto& [entityId, light] : lights) {
        if (!light.enabled) {
            if (frameCount % 60 == 0) {
                printf("  Light %d: DISABLED\n", entityId);
            }
            continue;
        }
        
        auto transIt = transforms.find(entityId);
        if (transIt != transforms.end()) {
            lightPositions.push_back(transIt->second.position);
            lightColors.push_back(light.color);
            lightIntensities.push_back(light.intensity);
            lightRadii.push_back(light.radius);
            activeLightCount++;
            
            if (frameCount % 60 == 0) {
                printf("  Light %d: pos=(%.1f,%.1f,%.1f) color=(%.2f,%.2f,%.2f) intensity=%.1f radius=%.1f\n",
                       entityId,
                       transIt->second.position.X, transIt->second.position.Y, transIt->second.position.Z,
                       light.color.X, light.color.Y, light.color.Z,
                       light.intensity, light.radius);
            }
        } else {
            if (frameCount % 60 == 0) {
                printf("  Light %d: ERROR - No transform found!\n", entityId);
            }
        }
    }

    if (frameCount % 60 == 0) {
        printf("Active lights passed to renderer: %d\n", activeLightCount);
        if (player) {
            hmm_vec3 camPos = player->CameraPosition();
            printf("Camera position: (%.1f, %.1f, %.1f)\n", camPos.X, camPos.Y, camPos.Z);
        }
        printf("=== END LIGHT DEBUG ===\n\n");
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

// Helper to convert mouse screen position to world ray
hmm_vec3 ScreenToWorldRay(float mouseX, float mouseY, int screenWidth, int screenHeight,
        const hmm_mat4 &viewMatrix, const hmm_mat4 &projMatrix) {
    // Convert mouse position to normalized device coordinates (NDC)
    float ndcX = (2.0f * mouseX) / screenWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / screenHeight; // Flip Y

    // Extract camera basis vectors from view matrix (column-major)
    // In a view matrix: right = column 0, up = column 1, forward = -column 2
    // But we need WORLD space vectors, not view space
    // The inverse view matrix has: right = row 0, up = row 1, back = row 2

    // For HandmadeMath column-major matrices:
    // Elements[col][row]
    // So to get row 0: Elements[0][0], Elements[1][0], Elements[2][0]
    hmm_vec3 right = HMM_Vec3(viewMatrix.Elements[0][0], viewMatrix.Elements[1][0], viewMatrix.Elements[2][0]);
    hmm_vec3 up = HMM_Vec3(viewMatrix.Elements[0][1], viewMatrix.Elements[1][1], viewMatrix.Elements[2][1]);
    hmm_vec3 forward = HMM_Vec3(-viewMatrix.Elements[0][2], -viewMatrix.Elements[1][2], -viewMatrix.Elements[2][2]);

    // Extract FOV from projection matrix
    float fovY = 2.0f * HMM_ATANF(1.0f / projMatrix.Elements[1][1]);
    float aspectRatio = projMatrix.Elements[1][1] / projMatrix.Elements[0][0];

    // Calculate half-angles
    float halfFovY = fovY * 0.5f;
    float halfFovX = HMM_ATANF(HMM_TANF(halfFovY) * aspectRatio);

    // Calculate ray direction
    // Start with forward direction, then offset by mouse position
    float tanHalfFovX = HMM_TANF(halfFovX);
    float tanHalfFovY = HMM_TANF(halfFovY);

    hmm_vec3 rayDir = HMM_AddVec3(
            HMM_AddVec3(
                    forward,
                    HMM_MultiplyVec3f(right, ndcX * tanHalfFovX)),
            HMM_MultiplyVec3f(up, ndcY * tanHalfFovY));

    return HMM_NormalizeVec3(rayDir);
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
        return; // Consume the event
    }
    
    // Handle TAB key for mode switching
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_TAB) {
        gameState.ToggleMode();
        UpdateCursorState(); // Update cursor state when mode changes
        printf("Mode switched to %s\n", gameState.IsEdit() ? "EDIT" : "PLAYING");
        return; // Consume the event
    }
    
    // Handle mouse clicks for entity selection (EDIT MODE ONLY)
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT && gameState.IsEdit()) {
        // Check if mouse is over ImGui window
        if (ui.IsGuiVisible() && ImGui::GetIO().WantCaptureMouse) {
            // Let ImGui handle the click (user is clicking on GUI)
        } else {
            // Perform entity selection with proper mouse-to-world ray
            if (player) {
                // Get screen dimensions
                int screenWidth = sapp_width();
                int screenHeight = sapp_height();
                
                // Get mouse position (in screen coordinates)
                float mouseX = ev->mouse_x;
                float mouseY = ev->mouse_y;
                
                // Get camera matrices
                hmm_mat4 proj = HMM_Perspective(60.0f, (float)screenWidth / (float)screenHeight, 0.01f, 1000.0f);
                hmm_mat4 view = player->GetViewMatrix();
                
                // Calculate ray from camera through mouse cursor
                hmm_vec3 rayOrigin = player->CameraPosition();
                hmm_vec3 rayDir = ScreenToWorldRay(mouseX, mouseY, screenWidth, screenHeight, view, proj);
                
                printf("Mouse ray: origin=(%.2f, %.2f, %.2f) dir=(%.2f, %.2f, %.2f)\n",
                       rayOrigin.X, rayOrigin.Y, rayOrigin.Z,
                       rayDir.X, rayDir.Y, rayDir.Z);
                
                // Perform raycast
                EntityId hitEntity = ecs.RaycastSelection(rayOrigin, rayDir, 1000.0f);
                
                // Always clear previous selection first
                if (selectedEntity != -1) {
                    Selectable* prevSel = ecs.GetSelectable(selectedEntity);
                    if (prevSel) prevSel->isSelected = false;
                }
                
                // Set new selection
                selectedEntity = hitEntity;
                
                if (selectedEntity != -1) {
                    Selectable* sel = ecs.GetSelectable(selectedEntity);
                    if (sel) {
                        sel->isSelected = true;
                        printf("Selected entity %d (%s)\n", selectedEntity, sel->name);
                    }
                } else {
                    printf("No entity selected (mouse ray missed all objects)\n");
                }
            }
        }
    }
    
    // Handle placement mode (EDIT MODE)
    if (gameState.IsEdit() && placementMode) {
        if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_ESCAPE) {
            placementMode = false;
            if (ghostEntity != -1) {
                ecs.DestroyEntity(ghostEntity);
                ghostEntity = -1;
            }
            return;
        }
        
        if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            if (!ui.IsGuiVisible() || !ImGui::GetIO().WantCaptureMouse) {
                // Place entity at cursor position
                if (player) {
                    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width() / (float)sapp_height(), 0.01f, 1000.0f);
                    hmm_mat4 view = player->GetViewMatrix();
                    
                    hmm_vec3 rayOrigin = player->CameraPosition();
                    hmm_vec3 rayDir = ScreenToWorldRay(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height(), view, proj);
                    
                    hmm_vec3 position = ecs.GetPlacementPosition(rayOrigin, rayDir, 10.0f);
                    
                    // Create entity
                    EntityId newEntity = ecs.CreateEntity();
                    Transform t;
                    t.position = position;
                    
                    // Configure entity based on placement type
                    if (placementMeshId == meshTreeId) {  // COMPARE TO meshTreeId, not 0!
                        // TREE
                        t.yaw = 0.0f;
                        t.pitch = 90.0f; // Stand tree upright
                        t.roll = 0.0f;
                        ecs.AddTransform(newEntity, t);
                        
                        // Verify transform was added
                        if (!ecs.HasTransform(newEntity)) {
                            printf("ERROR: Failed to add transform to entity %d\n", newEntity);
                        }
                        
                        int instanceId = ecs.AddRenderable(newEntity, meshTreeId, renderer);
                        printf("  Tree meshId=%d, instance ID: %d\n", meshTreeId, instanceId);
                        
                        if (instanceId < 0) {
                            printf("ERROR: Failed to create renderer instance for tree entity %d!\n", newEntity);
                            printf("  Check renderer capacity\n");
                        }
                        
                        // Add selectable
                        Selectable treeSel;
                        treeSel.name = "Tree";
                        treeSel.boundingRadius = 3.0f;
                        ecs.AddSelectable(newEntity, treeSel);
                        
                        // Add collider
                        Collider col;
                        col.type = ColliderType::Sphere;
                        col.radius = 3.0f;
                        col.isStatic = true;
                        ecs.AddCollider(newEntity, col);
                        
                        // Add to tracking vector
                        treeEntities.push_back(newEntity);
                        
                        // Create wireframe immediately (only if renderable succeeded)
                        if (instanceId >= 0) {
                            CreateOrUpdateWireframe(newEntity, false);
                        }
                        
                        printf("Placed TREE at (%.2f, %.2f, %.2f) with entity ID %d\n", position.X, position.Y, position.Z, newEntity);
                        
                    } else if (placementMeshId == meshEnemyId) {  // COMPARE TO meshEnemyId, not 1!
                        // ENEMY
                        t.yaw = 0.0f;
                        t.pitch = 0.0f;
                        t.roll = 0.0f;
                        ecs.AddTransform(newEntity, t);
                        
                        int instanceId = ecs.AddRenderable(newEntity, meshEnemyId, renderer);
                        printf("  Enemy meshId=%d, instance ID: %d\n", meshEnemyId, instanceId);
                        
                        if (instanceId < 0) {
                            printf("ERROR: Failed to create renderer instance for enemy entity %d!\n", newEntity);
                        }
                        
                        // Add AI and animator
                        AIController ai;
                        ai.state = AIState::Wander;
                        ai.stateTimer = 3.0f + ((float)rand() / RAND_MAX) * 4.0f;
                        ai.wanderTarget = HMM_AddVec3(t.position, HMM_Vec3(((float)rand()/RAND_MAX - 0.5f)*10.0f, 0.0f, ((float)rand()/RAND_MAX - 0.5f)*10.0f));
                        ecs.AddAI(newEntity, ai);
                        
                        Animator anim;
                        anim.currentClip = -1;
                        anim.time = 0.0f;
                        ecs.AddAnimator(newEntity, anim);
                        
                        // Add selectable
                        Selectable enemySel;
                        enemySel.name = "Enemy";
                        enemySel.boundingRadius = 1.5f;
                        ecs.AddSelectable(newEntity, enemySel);
                        
                        // Add collider
                        Collider col;
                        col.type = ColliderType::Sphere;
                        col.radius = 1.5f;
                        ecs.AddCollider(newEntity, col);
                        
                        // Add to tracking vector
                        enemyEntities.push_back(newEntity);
                        
                        // Create wireframe immediately (only if renderable succeeded)
                        if (instanceId >= 0) {
                            CreateOrUpdateWireframe(newEntity, false);
                        }
                        
                        printf("Placed ENEMY at (%.2f, %.2f, %.2f) with entity ID %d\n", position.X, position.Y, position.Z, newEntity);
                        
                    } else if (placementMeshId == -1) {
                        // LIGHT (no mesh)
                        printf("DEBUG: Placing light (placementMeshId=-1)\n");
                        
                        t.yaw = 0.0f;
                        
                        // IMPROVED: Always place lights at a reasonable height (3 units above ground)
                        position.Y = position.Y + 3.0f; // Raise light 3 units above placement point
                        t.position = position;
                        
                        ecs.AddTransform(newEntity, t);
                        
                        Light light;
                        light.color = HMM_Vec3(1.0f, 0.8f, 0.6f); // Warm white
                        light.intensity = 10.0f;
                        light.radius = 30.0f;
                        light.enabled = true;
                        ecs.AddLight(newEntity, light);
                        
                        // Add selectable
                        Selectable lightSel;
                        lightSel.name = "Point Light";
                        lightSel.boundingRadius = 2.0f;
                        ecs.AddSelectable(newEntity, lightSel);
                        
                        // Add to tracking vector
                        lightEntities.push_back(newEntity);
                        
                        // Create wireframe immediately
                        CreateOrUpdateWireframe(newEntity, false);
                        
                        printf("Placed LIGHT at (%.2f, %.2f, %.2f) with entity ID %d\n", position.X, position.Y, position.Z, newEntity);
                    } else {
                        // Unknown placement type
                        printf("ERROR: Unknown placementMeshId=%d (expected %d for tree, %d for enemy, -1 for light)\n", 
                               placementMeshId, meshTreeId, meshEnemyId);
                    }
                    
                    // Sync the new entity to the renderer immediately
                    ecs.SyncToRenderer(renderer);
                }
            }
        }
    }

    // Handle 'P' key to toggle placement mode
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_P && gameState.IsEdit()) {
        placementMode = !placementMode;
        printf("Placement mode: %s\n", placementMode ? "ON" : "OFF");
        return;
    }

    // Handle Delete key to delete selected entity
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_DELETE && gameState.IsEdit()) {
        if (selectedEntity != -1) {
            printf("Deleting entity %d\n", selectedEntity);
            
            // CRITICAL: Remove wireframe FIRST before destroying the entity
            auto wireIt = entityWireframes.find(selectedEntity);
            if (wireIt != entityWireframes.end()) {
                EntityId wireframeEntity = wireIt->second;
                printf("  Also removing wireframe entity %d\n", wireframeEntity);
                ecs.RemoveRenderable(wireframeEntity, renderer);
                ecs.DestroyEntity(wireframeEntity);
                entityWireframes.erase(wireIt);
            }
            
            // Now destroy the actual entity
            ecs.RemoveRenderable(selectedEntity, renderer);
            ecs.DestroyEntity(selectedEntity);
            
            // Remove from tracking vectors
            auto treeIt = std::find(treeEntities.begin(), treeEntities.end(), selectedEntity);
            if (treeIt != treeEntities.end()) {
                treeEntities.erase(treeIt);
                printf("  Removed from treeEntities\n");
            }
            
            auto enemyIt = std::find(enemyEntities.begin(), enemyEntities.end(), selectedEntity);
            if (enemyIt != enemyEntities.end()) {
                enemyEntities.erase(enemyIt);
                printf("  Removed from enemyEntities\n");
            }
            
            auto lightIt = std::find(lightEntities.begin(), lightEntities.end(), selectedEntity);
            if (lightIt != lightEntities.end()) {
                lightEntities.erase(lightIt);
                printf("  Removed from lightEntities\n");
            }
            
            selectedEntity = -1;
            printf("Entity deletion complete\n");
        }
        return;
    }
    
    // forward events to UI manager
    ui.HandleEvent(ev);
    
    // forward to player for input handling
    if (player) {
        bool shouldHandleInput = gameState.IsEdit() ? !ui.IsGuiVisible() : !ui.IsGuiVisible();
        if (shouldHandleInput) {
            player->HandleEvent(ev);
        }
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