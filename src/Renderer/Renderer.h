#pragma once

#include "../../include/Model.h"
#include "../../../External/Sokol/sokol_gfx.h"
#include "../../../External/Sokol/sokol_glue.h"
#include "Shader.h"
#include <unordered_map>
#include <vector>

class Renderer {
public:
    Renderer() noexcept;
    ~Renderer() noexcept;

    bool Init(); // create buffers + pipeline
    void Cleanup();

    // Mesh management
    int AddMesh(const Model3D& mesh);
    void RemoveMesh(int meshId);

    // Instance management
    int AddInstance(int meshId, const hmm_mat4& transform);
    void UpdateInstanceTransform(int instanceId, const hmm_mat4& transform);
    void RemoveInstance(int instanceId);

    // Call on resize
    void OnResize();

    // Render API:
    // BeginPass / Render / EndPass allow UI (simgui, sdtx) to be called inside same pass.
    void BeginPass();
    void Render(const hmm_mat4& mvp); // issue mesh draw calls - must be called after BeginPass()
    void EndPass();

private:
    struct MeshMeta {
        int mesh_id;
        int vertex_offset;
        int index_offset;
        int vertex_count;
        int index_count;
    };

    void create_render_targets();
    void rebuild_gpu_buffers(); // re-uploads merged CPU arrays to GPU buffers

    // sokol resources
    sg_buffer vbuf_;
    sg_buffer ibuf_;
    sg_pipeline pip_;
    sg_bindings bind_;
    sg_pass_action pass_action_;
    sg_pass pass_desc_;

    // merged CPU-side storage
    std::vector<Vertex> merged_vertices_;
    std::vector<uint16_t> merged_indices_;

    // bookkeeping
    std::unordered_map<int, MeshMeta> meshes_;
    std::vector<ModelInstance> instances_;
    int next_mesh_id_;
    int next_instance_id_;

    vs_params_t vs_params_; // from Shader.h

    // Simple resource maps (skeleton)
    std::unordered_map<int, sg_image> textures_;
    std::unordered_map<int, int> materials_; // placeholder

    // pipeline state dirty
    bool gpu_buffers_dirty_;
};