#include "Renderer.h"
#include "../../../External/Sokol/sokol_log.h"
#include <stdio.h>
#include <string.h>

#define MAX_VERTICES_STREAM (8 * 1024 * 10 * 4) // conservative
#define MAX_INDICES_STREAM  (8 * 1024 * 10 * 6)

Renderer::Renderer() noexcept
    : vbuf_()
    , ibuf_()
    , pip_()
    , next_mesh_id_(1)
    , next_instance_id_(1)
    , gpu_buffers_dirty_(false)
{
    vbuf_.id = SG_INVALID_ID;
    ibuf_.id = SG_INVALID_ID;   
    pip_.id  = SG_INVALID_ID;

    memset(&bind_, 0, sizeof(bind_));
    memset(&pass_action_, 0, sizeof(pass_action_));
    memset(&pass_desc_, 0, sizeof(pass_desc_));
    vs_params_ = {};
}

Renderer::~Renderer() noexcept {
    Cleanup();
}

bool Renderer::Init() {
    create_render_targets();

    // create vertex stream buffer
    sg_buffer_desc vbuf_desc = {};
    vbuf_desc.usage = SG_USAGE_STREAM;
    vbuf_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    vbuf_desc.size = MAX_VERTICES_STREAM * sizeof(Vertex);
    vbuf_ = sg_make_buffer(&vbuf_desc);
    bind_.vertex_buffers[0] = vbuf_;

    // index buffer
    sg_buffer_desc ibuf_desc = {};
    ibuf_desc.usage = SG_USAGE_STREAM;
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.size = MAX_INDICES_STREAM * sizeof(uint16_t);
    ibuf_ = sg_make_buffer(&ibuf_desc);
    bind_.index_buffer = ibuf_;

    // shader + pipeline
    sg_shader quad_shader = sg_make_shader(Shader_shader_desc(sg_query_backend()));
    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = quad_shader;
    pip_desc.layout.attrs[ATTR_Shader_pos].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_Shader_uv_in].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pip_desc.cull_mode = SG_CULLMODE_NONE;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    pip_desc.colors[0].blend.enabled = true;
    pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors[0].blend.op_rgb = SG_BLENDOP_ADD;
    pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
    pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors[0].blend.op_alpha = SG_BLENDOP_ADD;
    pip_ = sg_make_pipeline(&pip_desc);

    return true;
}

void Renderer::create_render_targets() {
    pass_action_ = {};
    pass_action_.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action_.colors[0].clear_value = { 0.4f, 0.3f, 0.2f, 1.0f };
    pass_desc_ = {};
    pass_desc_.action = pass_action_;
    pass_desc_.swapchain = sglue_swapchain();
}

int Renderer::AddMesh(const Model3D& mesh) {
    if (mesh.vertex_count <= 0 || mesh.index_count <= 0) return -1;

    MeshMeta meta;
    meta.mesh_id = next_mesh_id_;
    meta.vertex_offset = (int)merged_vertices_.size();
    meta.index_offset = (int)merged_indices_.size();
    meta.vertex_count = mesh.vertex_count;
    meta.index_count = mesh.index_count;

    // append vertices
    merged_vertices_.insert(merged_vertices_.end(), mesh.vertices, mesh.vertices + mesh.vertex_count);

    // append indices with offset
    for (int i = 0; i < mesh.index_count; ++i) {
        merged_indices_.push_back((uint16_t)(mesh.indices[i] + meta.vertex_offset));
    }

    meshes_.emplace(meta.mesh_id, meta);
    gpu_buffers_dirty_ = true;
    return next_mesh_id_++;
}

void Renderer::RemoveMesh(int meshId) {
    auto it = meshes_.find(meshId);
    if (it == meshes_.end()) return;
    meshes_.erase(it);
    gpu_buffers_dirty_ = true;
}

int Renderer::AddInstance(int meshId, const hmm_mat4& transform) {
    if (meshes_.find(meshId) == meshes_.end()) return -1;
    ModelInstance inst;
    inst.mesh_id = meshId;
    inst.transform = transform;
    instances_.push_back(inst);
    return (int)instances_.size() - 1;
}

void Renderer::UpdateInstanceTransform(int instanceId, const hmm_mat4& transform) {
    if (instanceId < 0 || instanceId >= (int)instances_.size()) return;
    instances_[instanceId].transform = transform;
}

void Renderer::RemoveInstance(int instanceId) {
    if (instanceId < 0 || instanceId >= (int)instances_.size()) return;
    instances_.erase(instances_.begin() + instanceId);
}

void Renderer::rebuild_gpu_buffers() {
    if (!merged_vertices_.empty()) {
        sg_update_buffer(vbuf_, { .ptr = merged_vertices_.data(), .size = (uint32_t)(merged_vertices_.size() * sizeof(Vertex)) });
    }
    if (!merged_indices_.empty()) {
        sg_update_buffer(ibuf_, { .ptr = merged_indices_.data(), .size = (uint32_t)(merged_indices_.size() * sizeof(uint16_t)) });
    }
    gpu_buffers_dirty_ = false;
}

// Begin the frame render pass (call before Render and UI draws)
void Renderer::BeginPass() {
    // ensure gpu buffers uploaded before drawing
    if (gpu_buffers_dirty_) rebuild_gpu_buffers();
    sg_begin_pass(&pass_desc_);
    // apply default pipeline state so UI draw code doesn't need to rebind if desired
    // (Renderer::Render will re-apply its pipeline)
}

// Draw meshes only (must be called between BeginPass and EndPass)
// The parameter `view_proj` should be the combined view * projection matrix.
// Each instance contains its own model transform and will be multiplied with view_proj.
void Renderer::Render(const hmm_mat4& view_proj) {
    // ensure GPU buffers bound
    sg_apply_pipeline(pip_);
    sg_apply_bindings(&bind_);

    // Draw each instance separately so we can set per-instance MVP
    for (const ModelInstance& inst : instances_) {
        auto it = meshes_.find(inst.mesh_id);
        if (it == meshes_.end()) continue;
        const MeshMeta& meta = it->second;

        // compute per-instance MVP = view_proj * model_transform
        vs_params_.mvp = HMM_MultiplyMat4(view_proj, inst.transform);

        // apply uniform and draw the mesh subset
        sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params_));
        if (meta.index_count > 0) {
            sg_draw(meta.index_offset, meta.index_count, 1);
        }
    }
}

// End the frame render pass (call after Render and after UI draws)
void Renderer::EndPass() {
    sg_end_pass();
    // caller (Main.cpp) should call sg_commit() once after EndPass
}

void Renderer::OnResize() {
    create_render_targets();
}

void Renderer::Cleanup() {
    if (vbuf_.id != SG_INVALID_ID) { sg_destroy_buffer(vbuf_); vbuf_.id = SG_INVALID_ID; }
    if (ibuf_.id != SG_INVALID_ID) { sg_destroy_buffer(ibuf_); ibuf_.id = SG_INVALID_ID; }
    if (pip_.id != SG_INVALID_ID)  { sg_destroy_pipeline(pip_); pip_.id = SG_INVALID_ID; }
    merged_vertices_.clear();
    merged_indices_.clear();
    meshes_.clear();
    instances_.clear();
    textures_.clear();
    materials_.clear();
}