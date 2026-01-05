#pragma once

#include "../../include/Model.h"
#include "../../../External/Sokol/sokol_gfx.h"
#include "../../../External/Sokol/sokol_glue.h"
#include "../../../External/HandmadeMath.h"

//#include "Shader3D.h"
#include "Shader2D.h"
#include "Shader3DLit.h"

#include <unordered_map>
#include <vector>
#include <set>

class Renderer {
public:
    Renderer() noexcept;
    ~Renderer() noexcept;

    bool Init();
    void Cleanup();

    // Mesh management
    int AddMesh(const Model3D &mesh);
    void RemoveMesh(int meshId);

    // ADDED: Mark a mesh as wireframe (uses line rendering)
    void MarkMeshAsWireframe(int meshId, bool isWireframe = true);

    // ADDED: Mark a mesh as a gizmo (renders with no depth test)
    void MarkMeshAsGizmo(int meshId, bool isGizmo = true);

    // Instance management
    int AddInstance(int meshId, const hmm_mat4 &transform);
    void UpdateInstanceTransform(int instanceId, const hmm_mat4 &transform);
    void RemoveInstance(int instanceId);

    void SetInstanceScreenSpace(int instanceId, bool isScreenSpace);

    // Add method to set bone transforms for an instance
    void SetInstanceBoneTransforms(int instanceId, const std::vector<hmm_mat4> &boneTransforms);

    void OnResize();

    // Render API
    void BeginPass();
    void Render(const hmm_mat4 &mvp);
    void RenderScreenSpace(const hmm_mat4 &orthoProj);
    void RenderWireframes(const hmm_mat4 &view_proj); // Wireframes with depth test
    void RenderGizmos(const hmm_mat4 &view_proj); // ADDED: Gizmos without depth test
    void EndPass();

    void SetLights(const std::vector<hmm_vec3> &positions,
            const std::vector<hmm_vec3> &colors,
            const std::vector<float> &intensities,
            const std::vector<float> &radii);

    void SetAmbientLight(const hmm_vec3 &color, float intensity);
    void SetCameraPosition(const hmm_vec3 &position);
    void SetSunLight(const hmm_vec3 &direction, const hmm_vec3 &color, float intensity);

    // ADDED: Get model data for animation (returns nullptr if invalid meshId)
    const Model3D *GetModelData(int meshId) const;

private:
    struct MeshMeta {
        int mesh_id;
        int vertex_offset;
        int index_offset;
        int vertex_count;
        int index_count;
        sg_image texture;
        bool has_texture;
        bool is_wireframe; // Flag to identify wireframe meshes
        bool is_gizmo; // ADDED: Flag to identify gizmo meshes
    };

    struct InstanceBufferInfo {
        sg_buffer buffer;
        size_t capacity;
    };

    void create_render_targets();
    void rebuild_gpu_buffers();
    void render_instances(const hmm_mat4 &view_proj, const std::set<int> &instanceFilter, bool exclude, bool use2DShader);
    sg_image create_texture_from_data(unsigned char *data, int width, int height, int channels);

    // Sokol resources
    sg_buffer vbuf_;
    sg_buffer ibuf_;
    sg_buffer inst_vbuf_;
    std::unordered_map<int, InstanceBufferInfo> mesh_instance_bufs_;

    // Pipelines
    sg_pipeline pip_3d_;
    sg_pipeline pip_3d_no_depth_;
    sg_pipeline pip_2d_;
    sg_pipeline pip_2d_no_depth_;
    sg_pipeline pip_3d_lit_;
    sg_pipeline pip_3d_lines_; // Line rendering with depth test
    sg_pipeline pip_3d_lines_no_depth_; // ADDED: Line rendering without depth test (for gizmos)

    sg_bindings bind_;
    sg_pass_action pass_action_;
    sg_pass pass_desc_;

    sg_image default_texture_;
    sg_sampler texture_sampler_;

    // CPU-side storage
    std::vector<Vertex> merged_vertices_;
    std::vector<uint16_t> merged_indices_;

    // Cached per-frame data
    std::unordered_map<int, std::vector<hmm_mat4>> mesh_instances_cache_;

    // Bookkeeping
    std::unordered_map<int, MeshMeta> meshes_;
    std::vector<ModelInstance> instances_;
    std::set<int> screenSpaceInstances_;
    std::set<int> wireframeMeshes_; // Track which meshes are wireframes
    std::set<int> gizmoMeshes_; // ADDED: Track which meshes are gizmos
    int next_mesh_id_;
    int next_instance_id_;

    vs_params_t vs_params_;
    fs_params_t fs_params_;

    hmm_vec3 camera_pos_;

    bool gpu_buffers_dirty_;

    // ADDED: Store full model data for animation
    std::unordered_map<int, Model3D> model_data_;
};