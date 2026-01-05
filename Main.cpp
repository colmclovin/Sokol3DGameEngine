// TODO: AI for enemies
// TODO: serialization system
// TODO: Level system?
// TODO: saving offsets, variables, controls to file, read and write them, etc.
#define SOKOL_D3D11
#include "../External/Sokol/sokol_app.h"
#include "../External/Sokol/sokol_fetch.h"
#include "../External/Sokol/sokol_gfx.h"
#include "../External/Sokol/sokol_glue.h"
#include "../External/Sokol/sokol_log.h"
#include <stdio.h>
#include <windows.h>

// Forward declare project logger
extern "C" void slog_to_debug(const char *tag,
        uint32_t log_level,
        uint32_t log_item_id,
        const char *message_or_null,
        uint32_t line_nr,
        const char *filename_or_null,
        void *user_data);

#include "../External/HandmadeMath.h"
#include "../External/Imgui/imgui.h"

// Core systems
#include "src/Audio/AudioEngine.h"
#include "src/Game/ECS.h"
#include "src/Game/GameState.h"
#include "src/Game/Player.h"
#include "src/Geometry/Quad.h"
#include "src/Model/ModelLoader.h"
#include "src/Model/ModelMetadata.h"
#include "src/Renderer/Renderer.h"
#include "src/UI/UIManager.h"

// Editor systems
#include "src/Editor/EditorUI.h"
#include "src/Editor/EntityPlacement.h"
#include "src/Editor/WireframeManager.h"
#include "src/Editor/TransformGizmo.h"  // ADDED
#include "src/Utilities/RaycastHelper.h"

#include <stdlib.h>
#include <time.h>
#include <vector>

// Mesh IDs
static int meshTreeId = -1;
static int meshEnemyId = -1;
static int meshPlayerId = -1;
static int meshGroundId = -1;

// Core systems
static Renderer renderer;
static AudioEngine audio;
static ModelLoader loader;
static UIManager ui;
static GameStateManager gameState;
static ECS ecs;
static PlayerController *player = nullptr;

// Editor systems
static EditorUI editorUI;
static WireframeManager wireframeManager;
static EntityPlacement entityPlacement;
static TransformGizmo transformGizmo;

// ADDED: Collision visualization toggle
static bool showCollisionShapes = false;

// Entity collections
static std::vector<EntityId> enemyEntities;
static std::vector<EntityId> treeEntities;
static std::vector<EntityId> UIEntities;
static std::vector<EntityId> lightEntities;
static EntityId hudQuadEntity = -1;
static EntityId billboardQuadEntity = -1;
static EntityId groundEntity = -1;

// Editor state
static EntityId selectedEntity = -1;
static int placementMeshId = -1;
static bool placementMode = false;
static EntityId ghostEntity = -1;
static GizmoMode currentGizmoMode = GizmoMode::None;
static bool gizmoEnabled = false; // ADDED: Toggle for gizmo system
static float lastMouseX = 0.0f; // ADDED: Track mouse position
static float lastMouseY = 0.0f; // ADDED: Track mouse position

#define MAX_MODELS 32
Model3D models3D[MAX_MODELS];

static bool mouse_locked = true;

// Helper to update mouse/cursor state based on game mode
void UpdateCursorState() {
    if (gameState.IsEdit()) {
        sapp_lock_mouse(false);
        sapp_show_mouse(true);
        mouse_locked = false;
        if (player) {
            player->SetInputEnabled(!ui.IsGuiVisible());
        }
    } else {
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

void OnGuiVisibilityChanged(bool visible) {
    UpdateCursorState();
    printf("GUI %s\n", visible ? "opened" : "closed");
}

void init(void) {
#ifdef _WIN32
    AllocConsole();
    FILE *dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    printf("=== DEBUG CONSOLE ALLOCATED ===\n");
#endif

    sapp_lock_mouse(true);
    sapp_show_mouse(false);

    // Initialize core systems
    audio.Init();
    audio.LoadWav("assets/audio/TetrisTheme.wav");

    sg_desc desc = {};
    desc.environment = sglue_environment();
    desc.logger.func = slog_to_debug;
    sg_setup(&desc);

    ui.Setup();
    ui.SetGuiVisibilityCallback(OnGuiVisibilityChanged);

    // Initialize editor systems
    editorUI.Init(&ecs, &audio, &gameState);
    wireframeManager.Init(&ecs, &renderer);
    entityPlacement.Init(&ecs, &renderer, &treeEntities, &enemyEntities, &lightEntities);
    transformGizmo.Init(&ecs, &renderer);  // ADDED
    
    // Register ImGui callback
    ui.AddGuiCallback([]() {
        editorUI.RenderAudioControls();
        editorUI.RenderGameStateControls();

        if (gameState.IsEdit()) {
            ImGui::Separator();
            
            // ADDED: Collision visualization toggle
            if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
                editorUI.RenderCollisionVisualization(showCollisionShapes);
            }
            
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Entity Inspector", ImGuiTreeNodeFlags_DefaultOpen)) {
                editorUI.RenderEntityInspector(selectedEntity);
            }

            editorUI.RenderPlacementControls(placementMode, placementMeshId, meshTreeId, meshEnemyId);
        }

        if (player) {
            ImGui::Separator();
            player->GetCamera().RenderImGuiControls();
        }
    });

    // Initialize renderer
    printf("=== INITIALIZING RENDERER ===\n");
    renderer.Init();

    // Load models
    printf("\n=== LOADING MODELS ===\n");
    models3D[0] = loader.LoadModel("assets/models/cartoon_lowpoly_trees_blend.glb");
    models3D[1] = loader.LoadModel("assets/models/cube.glb");
    models3D[2] = loader.LoadModel("assets/models/test1.glb");
    models3D[3] = QuadGeometry::CreateGroundQuad(200.0f, 10.0f);

    printf("\n=== ADDING MESHES TO RENDERER ===\n");
    meshTreeId = renderer.AddMesh(models3D[0]);
    meshEnemyId = renderer.AddMesh(models3D[1]);
    meshPlayerId = renderer.AddMesh(models3D[2]);
    meshGroundId = renderer.AddMesh(models3D[3]);
    printf("Tree mesh ID: %d\n", meshTreeId);
    printf("Enemy mesh ID: %d\n", meshEnemyId);
    printf("Player mesh ID: %d\n", meshPlayerId);
    printf("Ground mesh ID: %d\n", meshGroundId);

    // Create wireframe meshes
    printf("\n=== CREATING WIREFRAME MESHES ===\n");
    wireframeManager.CreateWireframeMeshes();
    transformGizmo.CreateGizmoMeshes();  // ADDED
    
    // Create player
    printf("\n=== CREATING PLAYER ===\n");
    player = new PlayerController(ecs, renderer, meshPlayerId, gameState);
    player->Spawn(HMM_Vec3(0.0f, 2.0f, 0.0f)); // CHANGED: Spawn above ground, will fall with gravity

    // Create ground
    printf("\n=== CREATING GROUND ===\n");
    groundEntity = ecs.CreateEntity();
    Transform groundTransform;
    groundTransform.position = HMM_Vec3(0.0f, -4.0f, 0.0f);
    ecs.AddTransform(groundEntity, groundTransform);
    ecs.AddRenderable(groundEntity, meshGroundId, renderer);

    // Option 1: Simple plane collider (recommended for flat ground)
    ecs.CreatePlaneCollider(groundEntity, HMM_Vec3(0.0f, 1.0f, 0.0f), -4.0f);

    // Option 2: Mesh collider (for complex terrain)
    // ecs.CreateMeshCollider(groundEntity, models3D[3]);

    Selectable groundSel;
    groundSel.name = "Ground";
    groundSel.volumeType = SelectionVolumeType::Box;
    groundSel.boundingBoxMin = HMM_Vec3(-200.0f, -5.0f, -200.0f);
    groundSel.boundingBoxMax = HMM_Vec3(200.0f, -3.0f, 200.0f);
    groundSel.showWireframe = true;  // Don't show wireframe for ground
    ecs.AddSelectable(groundEntity, groundSel);

      // Spawn trees
    printf("\n=== SPAWNING TREES ===\n");
    treeEntities.clear();
    srand((unsigned int)time(nullptr));
    for (int i = 0; i < 2; ++i) {
        EntityId treeId = ecs.CreateEntity();
        Transform t;
        float x = ((float)rand() / RAND_MAX - 0.5f) * 200.0f;
        float z = ((float)rand() / RAND_MAX - 0.5f) * 200.0f;
        t.position = HMM_Vec3(x, 0.0f, z);

        // Apply tree model metadata
        const ModelMetadata *treeMetadata = &ModelRegistry::TREE_MODEL;
        t.pitch = treeMetadata->defaultRotation.X;
        t.yaw = treeMetadata->defaultRotation.Y;
        t.roll = treeMetadata->defaultRotation.Z;
        t.scale = treeMetadata->defaultScale;
        t.originOffset = treeMetadata->originOffset;

        ecs.AddTransform(treeId, t);
        ecs.AddRenderable(treeId, meshTreeId, renderer);

        // ADDED: Sphere collider for trees (static obstacle)
        Collider treeCollider;
        treeCollider.type = ColliderType::Sphere;
        treeCollider.radius = 2.0f; // Tree trunk radius
        treeCollider.isStatic = true;
        treeCollider.isTrigger = false;
        ecs.AddCollider(treeId, treeCollider);

        // CHANGED: Use sphere selection volume
        Selectable treeSel;
        treeSel.name = "Tree";
        treeSel.volumeType = SelectionVolumeType::Sphere;
        treeSel.boundingSphereRadius = 3.0f; // Slightly larger for easier selection
        treeSel.boundingRadius = 3.0f; // Backward compatibility
        treeSel.showWireframe = true;
        treeSel.canBeSelected = true;
        ecs.AddSelectable(treeId, treeSel);

        treeEntities.push_back(treeId);
    }
    printf("Spawned %zu trees\n", treeEntities.size());

          // Spawn enemies
    printf("\n=== SPAWNING ENEMIES ===\n");
    enemyEntities.clear();
    for (int i = 0; i < 2; ++i) {
        EntityId eid = ecs.CreateEntity();
        Transform t;
        float angle = (float)i * (3.14159f * 2.0f / 200.0f);
        float radius = 15.0f;
        t.position = HMM_Vec3(sinf(angle) * radius, 2.0f, cosf(angle) * radius); // CHANGED: Y=0 (above ground)
        t.yaw = angle + 3.14159f;
        ecs.AddTransform(eid, t);

        AIController ai;
        ai.state = AIState::Wander;
        ai.stateTimer = 3.0f + ((float)rand() / RAND_MAX) * 4.0f;
        ai.wanderTarget = HMM_AddVec3(t.position, HMM_Vec3(((float)rand() / RAND_MAX - 0.5f) * 10.0f, 2.0f, ((float)rand() / RAND_MAX - 0.5f) * 10.0f));
        ecs.AddAI(eid, ai);

        Animator anim;
        anim.currentClip = -1;
        anim.time = 0.0f;
        ecs.AddAnimator(eid, anim);

        ecs.AddRenderable(eid, meshEnemyId, renderer);

        // ADDED: Sphere collider for enemies (dynamic, affected by physics)
        Collider enemyCollider;
        enemyCollider.type = ColliderType::Sphere;
        enemyCollider.radius = 0.5f; // Enemy body radius
        enemyCollider.isStatic = false;
        enemyCollider.isTrigger = false;
        ecs.AddCollider(eid, enemyCollider);

        // ADDED: Rigidbody for enemies (so they can be pushed, affected by physics)
        Rigidbody enemyRb;
        enemyRb.mass = 50.0f; // Heavy enough to not be easily pushed
        enemyRb.affectedByGravity = true;
        enemyRb.drag = 0.5f; // Some air resistance
        enemyRb.bounciness = 0.0f; // Don't bounce
        enemyRb.isKinematic = false; // Affected by physics
        ecs.AddRigidbody(eid, enemyRb);

        // CHANGED: Use sphere selection volume
        Selectable enemySel;
        enemySel.name = "Enemy";
        enemySel.volumeType = SelectionVolumeType::Sphere;
        enemySel.boundingSphereRadius = 1.5f; // Larger for easier selection
        enemySel.boundingRadius = 1.5f; // Backward compatibility
        enemySel.showWireframe = true;
        enemySel.canBeSelected = true;
        ecs.AddSelectable(eid, enemySel);

        enemyEntities.push_back(eid);
    }
    printf("Spawned %zu enemies\n", enemyEntities.size());

    // Create HUD quad
    printf("\n=== CREATING HUD QUAD ===\n");
    models3D[4] = QuadGeometry::CreateTexturedQuad(0.2f, 0.2f, "assets/textures/pikman.png");
    int meshHUD = renderer.AddMesh(models3D[4]);

    if (meshHUD >= 0) {
        hudQuadEntity = ecs.CreateEntity();
        Transform hudTransform;
        hudTransform.position = HMM_Vec3(0.0f, 0.0f, 0.0f);
        ecs.AddTransform(hudQuadEntity, hudTransform);
        int hudInstanceId = ecs.AddRenderable(hudQuadEntity, meshHUD, renderer);
        renderer.SetInstanceScreenSpace(hudInstanceId, true);

        ScreenSpace ss;
        ss.screenPosition = HMM_Vec2(0.85f, 0.15f);
        ss.size = HMM_Vec2(0.1f, 0.1f);
        ss.depth = 0.0f;
        ecs.AddScreenSpace(hudQuadEntity, ss);
        UIEntities.push_back(hudQuadEntity);
    }

    // Create billboard quad
    printf("\n=== CREATING BILLBOARD QUAD ===\n");
    models3D[5] = QuadGeometry::CreateTexturedQuad(1.0f, 1.0f, nullptr);
    int meshBillboard = renderer.AddMesh(models3D[5]);

    if (meshBillboard >= 0 && player) {
        billboardQuadEntity = ecs.CreateEntity();
        Transform billboardTransform;
        billboardTransform.position = HMM_Vec3(0.0f, 2.0f, 0.0f);
        ecs.AddTransform(billboardQuadEntity, billboardTransform);
        ecs.AddRenderable(billboardQuadEntity, meshBillboard, renderer);

        Billboard billboardComp;
        billboardComp.followTarget = player->Entity();
        billboardComp.offset = HMM_Vec3(1.0f, 2.5f, 0.0f);
        billboardComp.lockY = false;
        ecs.AddBillboard(billboardQuadEntity, billboardComp);
        UIEntities.push_back(billboardQuadEntity);
    }

    // Create lights
    printf("\n=== CREATING LIGHTS ===\n");
    lightEntities.clear();
    for (int i = 0; i < 16; ++i) {
        EntityId lightId = ecs.CreateEntity();
        Transform t;
        float angle = (float)i * (3.14159f * 2.0f / 16.0f);
        float radius = 300.0f;
        t.position = HMM_Vec3(sinf(angle) * radius, 3.0f, cosf(angle) * radius);
        t.yaw = 0.0f;
        ecs.AddTransform(lightId, t);

        Light light;
        light.color = HMM_Vec3(1.0f, 0.8f, 0.6f);
        light.intensity = 10.0f;
        light.radius = 30.0f;
        light.enabled = true;
        ecs.AddLight(lightId, light);

        Selectable lightSel;
        lightSel.name = "Point Light";
        lightSel.boundingRadius = 2.0f;
        ecs.AddSelectable(lightId, lightSel);

        lightEntities.push_back(lightId);
    }
    printf("Created %zu lights\n", lightEntities.size());

    // Create sun
    printf("\n=== CREATING SUN ===\n");
    hmm_vec3 sunDirection = HMM_Vec3(0.0f, 1.0f, 0.0f);
    hmm_vec3 sunColor = HMM_Vec3(1.0f, 0.95f, 0.8f);
    float sunIntensity = 1.0f;
    renderer.SetSunLight(sunDirection, sunColor, sunIntensity);

    printf("\n=== INITIALIZATION COMPLETE ===\n");
    printf("Press TAB to toggle between PLAYING and EDIT modes\n");
    printf("Press F1 to open/close Debug GUI\n");
    printf("Press F11 to toggle fullscreen\n");
}

void frame(void) {
    float dt = (float)sapp_frame_duration();
    const int width = sapp_width();
    const int height = sapp_height();
    ui.NewFrame(width, height, dt, sapp_dpi_scale());

    // Debug text
    ui.SetDebugCanvas(sapp_widthf(), sapp_heightf());
    ui.SetDebugOrigin(2.0f, 2.0f);
    ui.SetDebugColor(255, 255, 0);
    const char *modeText = gameState.IsPlaying() ? "PLAYING" : "EDIT";
    char debugText[512];  // Increased buffer size
    const char* gizmoModeText = "";
    if (gizmoEnabled) {
        if (currentGizmoMode == GizmoMode::Translate) gizmoModeText = " | TRANSLATE";
        else if (currentGizmoMode == GizmoMode::Rotate) gizmoModeText = " | ROTATE";
        else if (currentGizmoMode == GizmoMode::Scale) gizmoModeText = " | SCALE";
        else gizmoModeText = " | GIZMO ON";
    }
    
    snprintf(debugText, sizeof(debugText),
            "Trees Demo - WASD to move, Mouse to look, T to toggle gizmo, G/R/S for mode, TAB for play/edit\nCurrent: %s%s%s%s",
            modeText,
            (gameState.IsEdit() && selectedEntity != -1) ? " | Entity Selected" : "",
            gizmoModeText,
            placementMode ? " | PLACEMENT ACTIVE" : "");
    ui.DebugText(debugText);

    // Wireframe management
    static EntityId previousSelection = -1;
    static bool wasInEditMode = false;
    bool isEditMode = gameState.IsEdit();
    bool modeChanged = (isEditMode != wasInEditMode);

    if (!isEditMode) {
        wireframeManager.DestroyAllWireframes();
        transformGizmo.DestroyGizmo();  // ADDED
        entityPlacement.DestroyGhostPreview();  // ADDED
        previousSelection = -1;
        placementMode = false;
    }

    // Update player
    if (player) player->Update(dt);

    // Run ECS systems
    if (gameState.IsPlaying()) {
        ecs.UpdateAI(dt);
        ecs.UpdatePhysics(dt);
        ecs.UpdateCollisions(dt); // ADDED: Enable collision detection
        ecs.UpdateAnimation(dt);
    }

    if (player) {
        ecs.UpdateBillboards(player->CameraPosition());
    }
    ecs.UpdateScreenSpace((float)width, (float)height);

    // Update wireframes and gizmo in edit mode
    if (isEditMode) {
        bool selectionChanged = (selectedEntity != previousSelection);
        bool forceUpdate = selectionChanged || modeChanged;
        wireframeManager.UpdateWireframes(selectedEntity, forceUpdate);
        
        // ADDED: Update collision visualization
        wireframeManager.UpdateCollisionVisuals(showCollisionShapes);
        
        // CHANGED: Update transform gizmo (only if enabled)
        if (gizmoEnabled && selectedEntity != -1 && !placementMode) {
            transformGizmo.SetMode(currentGizmoMode);
            transformGizmo.UpdateGizmo(selectedEntity, player ? player->CameraPosition() : HMM_Vec3(0, 0, 0));
        } else {
            transformGizmo.DestroyGizmo();
        }
        
        previousSelection = selectedEntity;
        
        // Update ghost preview in placement mode
        if (placementMode && player) {
            hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width() / (float)sapp_height(), 0.01f, 1000.0f);
            hmm_mat4 view = player->GetViewMatrix();
            
            hmm_vec3 rayOrigin = player->CameraPosition();
            hmm_vec3 rayDir = RaycastHelper::ScreenToWorldRay(lastMouseX, lastMouseY, sapp_width(), sapp_height(), view, proj);
            
            RaycastHit hit = ecs.RaycastPlacement(rayOrigin, rayDir, 1000.0f);
            if (hit.hit) {
                entityPlacement.UpdateGhostPreview(hit.point);  // CHANGED: Only pass position
            }
        }
    }

    wasInEditMode = isEditMode;

    // Sync transforms
    ecs.SyncToRenderer(renderer);

    // Compute view-projection
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    hmm_mat4 proj = HMM_Perspective(60.0f, w / h, 0.01f, 1000.0f);
    hmm_mat4 view = player ? player->GetViewMatrix() : HMM_LookAt(HMM_Vec3(0.0f, 2.0f, 50.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // Gather lights
    std::vector<hmm_vec3> lightPositions;
    std::vector<hmm_vec3> lightColors;
    std::vector<float> lightIntensities;
    std::vector<float> lightRadii;

    const auto &lights = ecs.GetLights();
    const auto &transforms = ecs.GetTransforms();

    for (const auto &[entityId, light] : lights) {
        if (!light.enabled) continue;
        auto transIt = transforms.find(entityId);
        if (transIt != transforms.end()) {
            lightPositions.push_back(transIt->second.GetWorldPosition());  // CHANGED: Use GetWorldPosition()
            lightColors.push_back(light.color);
            lightIntensities.push_back(light.intensity);
            lightRadii.push_back(light.radius);
        }
    }

    renderer.SetLights(lightPositions, lightColors, lightIntensities, lightRadii);
    renderer.SetCameraPosition(player ? player->CameraPosition() : HMM_Vec3(0, 0, 0));

    hmm_mat4 ortho = HMM_Mat4d(1.0f);

    renderer.BeginPass();
    renderer.Render(view_proj);
    renderer.RenderScreenSpace(ortho);
    
    // CHANGED: Render wireframes and gizmos separately
    if (isEditMode) {
        renderer.RenderWireframes(view_proj);  // Selection boxes WITH depth testing
        renderer.RenderGizmos(view_proj);       // ADDED: Gizmos WITHOUT depth testing (always on top)
    }
    
    ui.Render();  // UI renders LAST
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

static void input(const sapp_event *ev) {
    if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        renderer.OnResize();
    }

    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_F11) {
        sapp_toggle_fullscreen();
        return;
    }

    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_TAB) {
        gameState.ToggleMode();
        UpdateCursorState();
        return;
    }
    
    // CHANGED: Toggle gizmo system with T key - also reset gizmo mode
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_T && gameState.IsEdit() && selectedEntity != -1) {
        gizmoEnabled = !gizmoEnabled;
        if (!gizmoEnabled) {
            currentGizmoMode = GizmoMode::None;
            transformGizmo.HandleMouseUp(); // Release any active drag
        }
        printf("Gizmo system: %s\n", gizmoEnabled ? "ENABLED" : "DISABLED");
        return;
    }
    
    // CHANGED: Gizmo mode hotkeys (only when gizmo enabled)
    if (gameState.IsEdit() && selectedEntity != -1 && gizmoEnabled) {
        if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_G) {
            transformGizmo.HandleMouseUp(); // Release drag before mode change
            currentGizmoMode = (currentGizmoMode == GizmoMode::Translate) ? GizmoMode::None : GizmoMode::Translate;
            printf("Gizmo mode: %s\n", currentGizmoMode == GizmoMode::Translate ? "TRANSLATE" : "NONE");
            return;
        }
        if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_R) {
            transformGizmo.HandleMouseUp(); // Release drag before mode change
            currentGizmoMode = (currentGizmoMode == GizmoMode::Rotate) ? GizmoMode::None : GizmoMode::Rotate;
            printf("Gizmo mode: %s\n", currentGizmoMode == GizmoMode::Rotate ? "ROTATE" : "NONE");
            return;
        }
        if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_S) {
            transformGizmo.HandleMouseUp(); // Release drag before mode change
            currentGizmoMode = (currentGizmoMode == GizmoMode::Scale) ? GizmoMode::None : GizmoMode::Scale;
            printf("Gizmo mode: %s\n", currentGizmoMode == GizmoMode::Scale ? "SCALE" : "NONE");
            return;
        }
    }
    
    // ADDED: Track mouse position
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        lastMouseX = ev->mouse_x;
        lastMouseY = ev->mouse_y;
    }

    // CHANGED: Entity selection in edit mode - DON'T deselect if gizmo is enabled
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT && gameState.IsEdit()) {
        if (!ui.IsGuiVisible() || !ImGui::GetIO().WantCaptureMouse) {
            if (player) {
                hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width() / (float)sapp_height(), 0.01f, 1000.0f);
                hmm_mat4 view = player->GetViewMatrix();

                hmm_vec3 rayOrigin = player->CameraPosition();
                hmm_vec3 rayDir = RaycastHelper::ScreenToWorldRay(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height(), view, proj);

                // CHANGED: Check gizmo interaction first (only if gizmo enabled)
                if (gizmoEnabled && currentGizmoMode != GizmoMode::None && selectedEntity != -1) {
                    if (transformGizmo.HandleMouseDown(selectedEntity, rayOrigin, rayDir)) {
                        return; // Gizmo handled the click
                    }
                    // ADDED: If gizmo is enabled but we didn't click on it, don't change selection
                    printf("Gizmo enabled - ignoring selection change\n");
                    return;
                }

                // FIXED: Use NEW raycast function that supports all selection volume types
                RaycastHit hit = ecs.RaycastSelectionNew(rayOrigin, rayDir, 1000.0f);
                EntityId hitEntity = hit.entity;

                if (selectedEntity != -1) {
                    Selectable *prevSel = ecs.GetSelectable(selectedEntity);
                    if (prevSel) prevSel->isSelected = false;
                }

                selectedEntity = hitEntity;

                if (selectedEntity != -1) {
                    Selectable *sel = ecs.GetSelectable(selectedEntity);
                    if (sel) {
                        sel->isSelected = true;
                        printf("Selected entity %d (%s) at distance %.2f\n", selectedEntity, sel->name, hit.distance);
                    }
                } else {
                    printf("Deselected entity\n");
                }
            }
        }
    }
    
    // CHANGED: Mouse drag for gizmo (only if gizmo enabled AND dragging)
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE && gameState.IsEdit() && selectedEntity != -1 && gizmoEnabled) {
        if (currentGizmoMode != GizmoMode::None && player) {
            hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width() / (float)sapp_height(), 0.01f, 1000.0f);
            hmm_mat4 view = player->GetViewMatrix();
            
            hmm_vec3 rayOrigin = player->CameraPosition();
            hmm_vec3 rayDir = RaycastHelper::ScreenToWorldRay(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height(), view, proj);
            
            transformGizmo.HandleMouseDrag(selectedEntity, rayOrigin, rayDir, ev->mouse_dx, ev->mouse_dy);
            
            // ADDED: Don't process player input while dragging gizmo
            if (transformGizmo.IsDragging()) {
                return;
            }
        }
    }
    
    // Mouse up for gizmo
    if (ev->type == SAPP_EVENTTYPE_MOUSE_UP && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        transformGizmo.HandleMouseUp();
    }

    // Placement mode
    if (gameState.IsEdit() && placementMode) {
        if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_ESCAPE) {
            placementMode = false;
            entityPlacement.DestroyGhostPreview();
            return;
        }

        if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            if (!ui.IsGuiVisible() || !ImGui::GetIO().WantCaptureMouse) {
                if (player) {
                    hmm_mat4 proj = HMM_Perspective(60.0f, (float)sapp_width() / (float)sapp_height(), 0.01f, 1000.0f);
                    hmm_mat4 view = player->GetViewMatrix();

                    hmm_vec3 rayOrigin = player->CameraPosition();
                    hmm_vec3 rayDir = RaycastHelper::ScreenToWorldRay(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height(), view, proj);
                    
                    // FIXED: Use RaycastPlacement instead of GetPlacementPosition
                    RaycastHit hit = ecs.RaycastPlacement(rayOrigin, rayDir, 1000.0f);
                    
                    if (hit.hit) {
                        printf("Placing entity at raycast hit: (%.2f, %.2f, %.2f)\n", 
                               hit.point.X, hit.point.Y, hit.point.Z);
                        entityPlacement.PlaceEntity(hit.point, placementMeshId, meshTreeId, meshEnemyId);
                    } else {
                        // Fallback: place at fixed distance if no hit
                        hmm_vec3 fallbackPos = HMM_AddVec3(rayOrigin, HMM_MultiplyVec3f(rayDir, 10.0f));
                        printf("No raycast hit, placing at fallback: (%.2f, %.2f, %.2f)\n", 
                               fallbackPos.X, fallbackPos.Y, fallbackPos.Z);
                        entityPlacement.PlaceEntity(fallbackPos, placementMeshId, meshTreeId, meshEnemyId);
                    }

                    // Create wireframe for newly placed entity
                    EntityId newEntity = -1;
                    const auto &selectables = ecs.GetSelectables();
                    for (const auto &[entityId, selectable] : selectables) {
                        if (newEntity == -1 || entityId > newEntity) {
                            newEntity = entityId;
                        }
                    }
                    if (newEntity != -1) {
                        wireframeManager.CreateOrUpdateWireframe(newEntity, false);
                    }
                }
            }
        }
    }

if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_P && gameState.IsEdit()) {
        placementMode = !placementMode;

        if (placementMode) {
            // FIXED: Set placementMeshId based on the currently selected type in EditorUI
            int selectedType = editorUI.GetSelectedPlacementType(); // You'll need to add this getter
            if (selectedType == 0)
                placementMeshId = meshTreeId;
            else if (selectedType == 1)
                placementMeshId = meshEnemyId;
            else
                placementMeshId = -1;

            entityPlacement.CreateGhostPreview(placementMeshId, meshTreeId, meshEnemyId);
            printf("Placement mode: ON (meshId=%d)\n", placementMeshId);
        } else {
            entityPlacement.DestroyGhostPreview();
            printf("Placement mode: OFF\n");
        }

        return;
    }

    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_DELETE && gameState.IsEdit()) {
        if (selectedEntity != -1) {
            wireframeManager.DestroyWireframe(selectedEntity);
            entityPlacement.DeleteEntity(selectedEntity);
            selectedEntity = -1;
            // ADDED: Disable gizmo when entity is deleted
            gizmoEnabled = false;
            currentGizmoMode = GizmoMode::None;
        }
        return;
    }

    ui.HandleEvent(ev);

    // CHANGED: Player input disabled when gizmo is active OR dragging
    if (player) {
        bool gizmoDragging = gizmoEnabled && currentGizmoMode != GizmoMode::None && transformGizmo.IsDragging();
        bool shouldHandleInput = !ui.IsGuiVisible() && !gizmoDragging;
        if (shouldHandleInput) {
            player->HandleEvent(ev);
        }
    }
}

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
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
    desc.fullscreen = false;
    return desc;
}