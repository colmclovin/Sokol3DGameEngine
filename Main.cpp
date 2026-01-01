#define SOKOL_D3D11
#include <stdio.h>
#include <windows.h>

#include "../External/Sokol/sokol_app.h"
#include "../External/Sokol/sokol_fetch.h"
#include "../External/Sokol/sokol_gfx.h"
#include "../External/Sokol/sokol_glue.h"
#include "../External/Sokol/sokol_log.h"
#define DR_WAV_IMPLEMENTATION
#include "../External/Sokol/sokol_audio.h"
#include "../External/dr_wav.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "../External/sokol_debugtext.h"
#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_NO_SSE
#include "../External/HandmadeMath.h"
#include "../External/dbgui.h"
#include "Shader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../External/Imgui/imgui.h"
#include "../External/stb_image.h"
#define SOKOL_IMGUI_IMPL
#include "../External/Assimp/include/assimp/Importer.hpp"
#include "../External/Assimp/include/assimp/postprocess.h"
#include "../External/Assimp/include/assimp/scene.h"
#include "../External/Sokol/util/sokol_imgui.h"
#include <stdlib.h>
#include <stdint.h>
#include <time.h> // for seeding rand()
#include <algorithm> // std::shuffle
#include <cmath>
#include <memory.h>
#include <map>
#include <random>
#include <string>
#include <vector>
#include <string.h>


drwav wav;

#define MAX_QUADS 8 * 1024 * 10 // Allow up to 1024 quads
#define MAX_VERTICES (MAX_QUADS * 4)
#define MAX_INDICES (MAX_QUADS * 6)
#define MAX_BONES 100
#define MAX_BONES 100
#define MAX_BONE_NAME 64
#define MAX_BONE_MAPPING 64
#define MAX_MODELS 16
#define MAX_INSTANCES 1024
#define MAX_TOTAL_VERTICES (MAX_VERTICES)
#define MAX_TOTAL_INDICES (MAX_INDICES)

bool escape_not_pressed;
bool is_game_over = false;
float quad_colors[MAX_QUADS][4]; // Each is [r, g, b, a]
int quad_types[MAX_QUADS];
bool fullscreen = false;
bool moveLeft = false;
bool moveRight = false;
float camera_yaw = 0.0f; // horizontal angle (Y-axis)
float camera_pitch = 0.0f; // vertical angle (X-axis)
float camera_distance = 3.0f;

bool rotating = false;
float last_mouse_x = 0.0f;
float last_mouse_y = 0.0f;

float dt;
int width, height, channels;
unsigned char *icondata;
typedef enum {
    GAME_MENU,
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_RESET,
    GAME_OVER
} GameState;
GameState current_state = GAME_MENU;

struct {
    sg_pass_action pass_action;
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_pipeline pip;
    sg_bindings bind;
    vs_params_t vs_params;
    //fs_params_t fs_params;
    sg_pass pass_desc;
    float rx;
    float ry;
} state;

sg_image white_texture;
sg_image quad_textures[MAX_QUADS]; // Texture array per quad



char *read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = (char *)malloc(length + 1);
    fread(buffer, 1, length, file);
    fclose(file);
    buffer[length] = '\0';
    return buffer;
}

static float *audio_buffer = NULL;
static uint64_t total_samples = 0;
static uint64_t sample_index = 0;

// Audio stream callback
static void stream_cb(float *buffer, int num_frames, int num_channels) {
    if (!audio_buffer) return;

    int samples_needed = num_frames * num_channels;
    for (int i = 0; i < samples_needed; i++) {
        if (sample_index < total_samples) {
            buffer[i] = audio_buffer[sample_index++];
        } else {
            buffer[i] = 0.0f; // silence after audio ends
            sample_index = 0;
        }
    }
}

// Load and decode WAV into a float buffer
bool load_wav_file(const char *path) {
    
    if (!drwav_init_file(&wav, path, NULL)) {
        printf("Failed to load WAV: %s\n", path);
        return false;
    }

    total_samples = wav.totalPCMFrameCount * wav.channels;
    audio_buffer = (float *)malloc(total_samples * sizeof(float));
    if (!audio_buffer) {
        drwav_uninit(&wav);
        printf("Failed to allocate audio buffer\n");
        return false;
    }

    drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, audio_buffer);
    drwav_uninit(&wav);
    return true;
}

typedef struct {
    float pos[3];
    float uv[2];

} Vertex;
typedef struct {
    Vertex *vertices;
    uint16_t *indices;
    int vertex_count;
    int index_count;
} Model3D;
typedef struct {
    hmm_mat4 model3D_matrix;
    int model3D_index; // index into an array of `Model`
} Model3DInstance;

Model3D models3D[MAX_MODELS]; // Up to 16 unique meshes
Model3DInstance instances3D[MAX_INSTANCES]; // Up to 1024 placed objects
int instance_count = 0;



Model3D load_glb_model(const char *path) {
    Model3D model = {};

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);
    if (!scene || !scene->HasMeshes()) {
        printf("Assimp Error: %s\n", importer.GetErrorString());
        return model;
    }

    const aiMesh *mesh = scene->mMeshes[0];
    model.vertex_count = mesh->mNumVertices;
    model.vertices = (Vertex *)malloc(sizeof(Vertex) * model.vertex_count);

    int total_indices = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        total_indices += mesh->mFaces[i].mNumIndices;
    }
    model.index_count = total_indices;
    model.indices = (uint16_t *)malloc(sizeof(uint16_t) * model.index_count);

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        aiVector3D pos = mesh->mVertices[i];
        aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
        model.vertices[i].pos[0] = pos.x;
        model.vertices[i].pos[1] = pos.y;
        model.vertices[i].pos[2] = pos.z;
        model.vertices[i].uv[0] = uv.x;
        model.vertices[i].uv[1] = uv.y;
    }

    int idx = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        const aiFace &face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            model.indices[idx++] = (uint16_t)face.mIndices[j];
        }
    }
    return model;
}

void create_render_targets() {
    // Default pass action
    state.pass_action = {};
    state.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    state.pass_action.colors[0].clear_value = { 0.4f, 0.3f, 0.2f, 1.0f }; // clears to black
    state.pass_desc = {};
    state.pass_desc.action = state.pass_action;
    state.pass_desc.swapchain = sglue_swapchain();

    //uint32_t white_pixel = 0xFFFFFFFF; // RGBA (255,255,255,255)
    //int width = 1;
    //int height = 1;
    //sg_image_desc tilemap_desc = {};
    //tilemap_desc.width = width;
    //tilemap_desc.height = height;
    //tilemap_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    //tilemap_desc.data.subimage[0][0].ptr = &white_pixel;
    //tilemap_desc.data.subimage[0][0].size = sizeof(white_pixel);
    //white_texture = sg_make_image(&tilemap_desc);
    //state.bind.images[IMG_tex] = white_texture;

    sg_buffer_desc vbuf_desc = {};
    vbuf_desc.usage = SG_USAGE_STREAM;
    vbuf_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    vbuf_desc.size = MAX_VERTICES * sizeof(Vertex);
    state.vbuf = sg_make_buffer(&vbuf_desc);
    state.bind.vertex_buffers[0] = state.vbuf;

    sg_buffer_desc ibuf_desc = {};
    ibuf_desc.usage = SG_USAGE_STREAM;
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.size = MAX_INDICES * sizeof(uint16_t);
    state.ibuf = sg_make_buffer(&ibuf_desc);
    state.bind.index_buffer = state.ibuf;

    //// Sampler
    //sg_sampler_desc sampler_desc = {};
    //sampler_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    //sampler_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    //sampler_desc.border_color = SG_BORDERCOLOR_OPAQUE_BLACK;
    //state.bind.samplers[SMP_smp] = sg_make_sampler(&sampler_desc);

    // Shader and pipeline
    sg_shader quad_shader = sg_make_shader(Shader_shader_desc(sg_query_backend()));

    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = quad_shader;
    pip_desc.layout.attrs[ATTR_Shader_pos].format = SG_VERTEXFORMAT_FLOAT3; // Position
    pip_desc.layout.attrs[ATTR_Shader_uv_in].format = SG_VERTEXFORMAT_FLOAT2; // UV
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pip_desc.cull_mode = SG_CULLMODE_NONE;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.label = "quad_pipeline";

    // Enable blending for transparency
    pip_desc.colors[0].blend.enabled = true;
    pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors[0].blend.op_rgb = SG_BLENDOP_ADD;
    pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
    pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors[0].blend.op_alpha = SG_BLENDOP_ADD;

    state.pip = sg_make_pipeline(&pip_desc);
}

void init(void) {
    saudio_desc ad = { 0 };
    ad.stream_cb = stream_cb;
    ad.sample_rate = 44100;
    ad.num_channels = 2;
    saudio_setup(&ad);

    if (!load_wav_file("assets/audio/TetrisTheme.wav")) {
        printf("Audio load failed!\n");
    }

    sg_desc desc = {};
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    sdtx_desc_t sdtx_desc = {};
    sdtx_desc.fonts[0] = sdtx_font_oric();
    sdtx_desc.logger.func = slog_func;
    sdtx_setup(&sdtx_desc);

    __dbgui_setup(sapp_sample_count());

    // Sokol ImGui setup
    simgui_desc_t simgui_desc = {};
    simgui_desc.logger.func = slog_func;
    simgui_setup(&simgui_desc);

    create_render_targets();
    int cube_model_index = 0;
    int character_model_index = 1;

    models3D[cube_model_index] = load_glb_model("assets/models/cube.glb");
    models3D[character_model_index] = load_glb_model("assets/models/test1.glb");


}
int total_vertices = 0;
int total_indices = 0;
int model_count = 1; // set after loading your cube and character models
Vertex merged_vertices[MAX_TOTAL_VERTICES];
uint16_t merged_indices[MAX_TOTAL_INDICES];
void upload_models_to_stream_buffer() {
    total_vertices = 0;
    total_indices = 0;

    for (int i = 0; i < model_count; i++) {
        Model3D *model = &models3D[i];
        int vertex_offset = total_vertices;

        // Copy vertices
        for (int v = 0; v < model->vertex_count; v++) {
            merged_vertices[total_vertices++] = model->vertices[v];
        }

        // Copy indices with offset
        for (int idx = 0; idx < model->index_count; idx++) {
            merged_indices[total_indices++] = model->indices[idx] + vertex_offset;
        }
    }

    // Upload to GPU stream buffer
    sg_update_buffer(state.vbuf, { .ptr = merged_vertices, .size = total_vertices * sizeof(Vertex) });
    sg_update_buffer(state.ibuf, { .ptr = merged_indices, .size = total_indices * sizeof(uint16_t) });
}

void frame(void) {


    dt = (float)sapp_frame_duration();

    const int width = sapp_width();
    const int height = sapp_height();
    simgui_new_frame({ width, height, sapp_frame_duration(), sapp_dpi_scale() });
    static float f = 0.0f;




    char buf[32];
    char buf1[32];

    sdtx_canvas(200, 200); // set up screen resolution for text
    sdtx_origin(9.5, 10); // position in character grid (2,2) pixels from top-left
    sdtx_color3b(255, 255, 255); // optional: white text
    sprintf(buf, "HELLO");
    sdtx_puts(buf);
    switch (current_state) {

    case GAME_MENU:

        break;

    case GAME_PLAYING:

        break;

    case GAME_PAUSED:

        break;

    case GAME_RESET:

        sdtx_canvas(200, 200); // set up screen resolution for text
        sdtx_origin(9.5, 10); // position in character grid (2,2) pixels from top-left
        sdtx_color3b(255, 255, 255); // optional: white text
        // sprintf(buf, "GAME OVER!! \n \n SCORE: %d",score);
        sdtx_puts(buf);

        break;
    }



Vertex cube_vertices[] = {
        // Front face
         {{ -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f }},
         {{ 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f }},
         {{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }},
         {{ -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }},

        // Back face
         {{ 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f }},
         {{ -1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f }},
         {{ -1.0f, 1.0f, -1.0f }, { 1.0f, 1.0f }},
         {{ 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f }}
    };

uint16_t cube_indices[] = {
    // Front face
    0,
    1,
    2,
    2,
    3,
    0,
    // Right face
    1,
    4,
    7,
    7,
    2,
    1,
    // Back face
    4,
    5,
    6,
    6,
    7,
    4,
    // Left face
    5,
    0,
    3,
    3,
    6,
    5,
    // Top face
    3,
    2,
    7,
    7,
    6,
    3,
    // Bottom face
    5,
    4,
    1,
    1,
    0,
    5,
};

    // Update buffers
//sg_update_buffer(state.vbuf, {
//                                     .ptr = Cube.vertices,
//                                     .size = sizeof(Vertex) * Cube.vertex_count });
//sg_update_buffer(state.ibuf, {
//                                     .ptr = Cube.indices,
//                                     .size = sizeof(uint16_t) * Cube.index_count });
//sg_update_buffer(state.vbuf, SG_RANGE(cube_vertices));
//sg_update_buffer(state.ibuf, SG_RANGE(cube_indices));
upload_models_to_stream_buffer();

    // OutputDebugStringA("Test!\n");
    if (sg_query_buffer_state(state.vbuf) != SG_RESOURCESTATE_VALID) {
        OutputDebugStringA("ERROR: Vertex buffer is INVALID!\n");
    }
    if (sg_query_buffer_state(state.ibuf) != SG_RESOURCESTATE_VALID) {
        OutputDebugStringA("ERROR: Index buffer is INVALID!\n");
    }


const float w = sapp_widthf();
    const float h = sapp_heightf();
    const float t = (float)(sapp_frame_duration() * 60.0f);

    // Projection matrix
    hmm_mat4 proj = HMM_Perspective(60.0f, w / h, 0.01f, 100.0f);

    // Camera position farther back and slightly above
    hmm_vec3 cam_pos = HMM_Vec3(0.0f, 2.0f, 50.0f);

    // Look at the center of the model (slightly above ground)
    hmm_vec3 target = HMM_Vec3(0.0f, 0.0f, 0.0f);

    // Standard "up" vector
    hmm_vec3 up = HMM_Vec3(0.0f, 1.0f, 0.0f);

    // View matrix
    hmm_mat4 view = HMM_LookAt(cam_pos, target, up);

    // Combined view-projection
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);

    // Animate model rotation
    state.rx += 0.2f * t;
    state.ry += 0.2f * t;
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));

    // Final model transform
    hmm_mat4 model = HMM_MultiplyMat4(rxm, rym);
    //hmm_mat4 model = HMM_Scale(HMM_Vec3(5.0f, 5.0f, 5.0f));
    // Final MVP matrix
    state.vs_params.mvp = HMM_MultiplyMat4(view_proj, model);


    // Render
    sg_begin_pass(&state.pass_desc);
    sg_apply_pipeline(state.pip); // Forces pipeline rebind before every sprite
    sg_apply_bindings(&state.bind);

    sg_apply_uniforms(UB_vs_params, SG_RANGE(state.vs_params));

    sg_draw(0, total_indices, 1);
    simgui_render();
    __dbgui_draw();
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    __dbgui_shutdown();
    sdtx_shutdown();
    sg_shutdown();
    simgui_shutdown();
    saudio_shutdown();
   // free(Cube.vertices);
   // free(Cube.indices);
}
static void input(const sapp_event *ev) {
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN) {
        switch (ev->key_code) {
        case SAPP_KEYCODE_W:

            break;
        case SAPP_KEYCODE_UP:

            break;
        case SAPP_KEYCODE_S:
            // try_rotate(&CurrentTetrimino, -1);  // Counter-Clockwise

            break;
        case SAPP_KEYCODE_DOWN:
            // try_rotate(&CurrentTetrimino, -1);  // Counter-Clockwise

            break;
        case SAPP_KEYCODE_A:
            moveLeft = true;
            break;

        case SAPP_KEYCODE_LEFT:
            moveLeft = true;
            break;
        case SAPP_KEYCODE_D:
            moveRight = true;
            break;
        case SAPP_KEYCODE_RIGHT:
            moveRight = true;
            break;
        case SAPP_KEYCODE_SPACE:

            break;
        case SAPP_KEYCODE_R:
            current_state = GAME_RESET;
            break;
        case SAPP_KEYCODE_C:

        case SAPP_KEYCODE_ESCAPE:

            if (current_state == GAME_PLAYING) {
                current_state = GAME_PAUSED;
            }
            if ((current_state == GAME_PAUSED && escape_not_pressed) || current_state == GAME_MENU || current_state == GAME_OVER) {
                sapp_request_quit();
            }
            escape_not_pressed = false;
            break;
        case SAPP_KEYCODE_F11: sapp_toggle_fullscreen(); break;
        default: break;
        }
    }
    if (ev->type == SAPP_EVENTTYPE_KEY_UP) {
        switch (ev->key_code) {
        case SAPP_KEYCODE_S:

            break;
        case SAPP_KEYCODE_DOWN:

            break;
        case SAPP_KEYCODE_ESCAPE:
            escape_not_pressed = true;
            break;
        }
    }
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        rotating = true;
        last_mouse_x = ev->mouse_x;
        last_mouse_y = ev->mouse_y;
    }
    if (ev->type == SAPP_EVENTTYPE_MOUSE_UP && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        rotating = false;
    }
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE && rotating) {
        float dx = ev->mouse_x - last_mouse_x;
        float dy = ev->mouse_y - last_mouse_y;
        last_mouse_x = ev->mouse_x;
        last_mouse_y = ev->mouse_y;

        camera_yaw += dx * 0.01f; // adjust sensitivity
        camera_pitch += dy * 0.01f;

        // Clamp pitch to avoid flipping
        const float max_pitch = HMM_ToRadians(89.0f);
        if (camera_pitch > max_pitch) camera_pitch = max_pitch;
        if (camera_pitch < -max_pitch) camera_pitch = -max_pitch;
    }

    if (ev->type == SAPP_EVENTTYPE_RESIZED) {
        create_render_targets();
    }
    simgui_handle_event(ev);
    __dbgui_event(ev);
};
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
    desc.fullscreen = false;
    desc.sample_count = 4;
    desc.window_title = "Tetris!";
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
};
