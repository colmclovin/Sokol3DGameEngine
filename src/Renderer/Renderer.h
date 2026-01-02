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

    bool Init();
    void Cleanup();

    // Mesh management
    int AddMesh(const Model3D& mesh);
    void RemoveMesh(int meshId);

    // Instance management
    int AddInstance(int meshId, const hmm_mat4& transform);
    void UpdateInstanceTransform(int instanceId, const hmm_mat4& transform);
    void RemoveInstance(int instanceId);

    void OnResize();

    // Render API
    void BeginPass();
    void Render(const hmm_mat4& mvp);
    void EndPass();

private:
    struct MeshMeta {
        int mesh_id;
        int vertex_offset;
        int index_offset;
        int vertex_count;
        int index_count;
    };

    struct InstanceBufferInfo {
        sg_buffer buffer;
        size_t capacity;  // in number of instances
    };

    void create_render_targets();
    void rebuild_gpu_buffers();

    // Sokol resources
    sg_buffer vbuf_;
    sg_buffer ibuf_;
    sg_buffer inst_vbuf_;
    std::unordered_map<int, InstanceBufferInfo> mesh_instance_bufs_;
    sg_pipeline pip_;
    sg_bindings bind_;
    sg_pass_action pass_action_;
    sg_pass pass_desc_;

    // CPU-side storage
    std::vector<Vertex> merged_vertices_;
    std::vector<uint16_t> merged_indices_;

    // Cached per-frame data (reused to avoid allocations)
    std::unordered_map<int, std::vector<hmm_mat4>> mesh_instances_cache_;

    // Bookkeeping
    std::unordered_map<int, MeshMeta> meshes_;
    std::vector<ModelInstance> instances_;
    int next_mesh_id_;
    int next_instance_id_;

    vs_params_t vs_params_;

    bool gpu_buffers_dirty_;
};