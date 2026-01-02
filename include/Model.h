#pragma once

#include "../External/HandmadeMath.h"
#include "../External/Sokol/sokol_gfx.h"
#include <cstdint>

// Vertex with position, normal, UV (for future use), and color from material
struct Vertex {
    float pos[3]; // 12 bytes
    float normal[3]; // 12 bytes
    float uv[2]; // 8 bytes
    float color[4]; // 16 bytes
}; // Total: 48 bytes

struct Model3D {
    Vertex* vertices;   // owned by loader (caller responsible for free)
    uint16_t* indices;  // owned by loader (caller responsible for free)
    int vertex_count;
    int index_count;

    // Texture data (optional)
    unsigned char* texture_data; // Raw texture pixels (RGBA)
    int texture_width;
    int texture_height;
    int texture_channels;
    bool has_texture;
};

// Instance referencing a mesh loaded into the Renderer
struct ModelInstance {
    int mesh_id;           // mesh id returned by Renderer::AddMesh
    hmm_mat4 transform;    // model transform
};

// Light component
struct Light {
    hmm_vec3 color;        // RGB color
    float intensity;       // Light intensity (0-1)
    float radius;          // Light radius/range
    bool enabled;          // Is light active
};