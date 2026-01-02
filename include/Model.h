#pragma once

#include "../External/HandmadeMath.h"
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
};

// Instance referencing a mesh loaded into the Renderer
struct ModelInstance {
    int mesh_id;           // mesh id returned by Renderer::AddMesh
    hmm_mat4 transform;    // model transform
};