#pragma once

#include "../External/HandmadeMath.h"
#include <cstdint>

// Shared vertex and model definitions used by both Main.cpp and Renderer.
struct Vertex {
    float pos[3];
    float uv[2];
};

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