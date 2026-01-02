#include "Renderer.h"
#include "../../../External/Sokol/sokol_log.h"
#include <stdio.h>
#include <string.h>

#define MAX_VERTICES_STREAM (8 * 1024 * 10 * 4)
#define MAX_INDICES_STREAM  (8 * 1024 * 10 * 6)

Renderer::Renderer() noexcept
    : vbuf_()
    , ibuf_()
    , inst_vbuf_()
    , pip_()
    , next_mesh_id_(1)
    , next_instance_id_(1)
    , gpu_buffers_dirty_(false)
{
    vbuf_.id = SG_INVALID_ID;
    ibuf_.id = SG_INVALID_ID;   
    inst_vbuf_.id = SG_INVALID_ID;
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

    // Vertex buffer
    sg_buffer_desc vbuf_desc = {};
    vbuf_desc.usage = SG_USAGE_STREAM;
    vbuf_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    vbuf_desc.size = MAX_VERTICES_STREAM * sizeof(Vertex);
    vbuf_ = sg_make_buffer(&vbuf_desc);
    bind_.vertex_buffers[0] = vbuf_;

    // Index buffer
    sg_buffer_desc ibuf_desc = {};
    ibuf_desc.usage = SG_USAGE_STREAM;
    ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    ibuf_desc.size = MAX_INDICES_STREAM * sizeof(uint16_t);
    ibuf_ = sg_make_buffer(&ibuf_desc);
    bind_.index_buffer = ibuf_;

    // Placeholder instance buffer
    sg_buffer_desc inst_desc = {};
    inst_desc.usage = SG_USAGE_STREAM;
    inst_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    inst_desc.size = sizeof(hmm_mat4);
    inst_vbuf_ = sg_make_buffer(&inst_desc);
    bind_.vertex_buffers[1] = inst_vbuf_;

    // Shader + pipeline
    sg_shader quad_shader = sg_make_shader(Shader_shader_desc(sg_query_backend()));
    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = quad_shader;
    
    // Configure buffer[0] stride for per-vertex data
    pip_desc.layout.buffers[0].stride = sizeof(Vertex); // 48 bytes
    
    // Per-vertex attributes (buffer 0)
    pip_desc.layout.attrs[ATTR_Shader_pos].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader_pos].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_Shader_pos].offset = 0;
    
    pip_desc.layout.attrs[ATTR_Shader_normal_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader_normal_in].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_Shader_normal_in].offset = 12;
    
    pip_desc.layout.attrs[ATTR_Shader_uv_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader_uv_in].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.layout.attrs[ATTR_Shader_uv_in].offset = 24;
    
    pip_desc.layout.attrs[ATTR_Shader_color_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader_color_in].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[ATTR_Shader_color_in].offset = 32;
    
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pip_desc.cull_mode = SG_CULLMODE_NONE;
    pip_desc.index_type = SG_INDEXTYPE_UINT16;
    
    // Enable depth testing
    pip_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pip_desc.depth.write_enabled = true;
    
    pip_desc.colors[0].blend.enabled = true;
    pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors[0].blend.op_rgb = SG_BLENDOP_ADD;
    pip_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
    pip_desc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pip_desc.colors[0].blend.op_alpha = SG_BLENDOP_ADD;

    // Per-instance attributes (mat4 = 4x vec4) from buffer 1
    pip_desc.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;
    pip_desc.layout.buffers[1].stride = (int)sizeof(hmm_mat4);

    pip_desc.layout.attrs[4].buffer_index = 1;
    pip_desc.layout.attrs[4].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[4].offset = 0;

    pip_desc.layout.attrs[5].buffer_index = 1;
    pip_desc.layout.attrs[5].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[5].offset = 16;

    pip_desc.layout.attrs[6].buffer_index = 1;
    pip_desc.layout.attrs[6].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[6].offset = 32;

    pip_desc.layout.attrs[7].buffer_index = 1;
    pip_desc.layout.attrs[7].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[7].offset = 48;

    pip_ = sg_make_pipeline(&pip_desc);

    printf("Renderer: Initialized (vertex-color only mode)\n");
    return true;
}

void Renderer::create_render_targets() {
    pass_action_ = {};
    pass_action_.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action_.colors[0].clear_value = { 0.4f, 0.3f, 0.6f, 1.0f };
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

    // Append vertices
    merged_vertices_.insert(merged_vertices_.end(), mesh.vertices, mesh.vertices + mesh.vertex_count);

    // Append indices with offset
    for (int i = 0; i < mesh.index_count; ++i) {
        merged_indices_.push_back((uint16_t)(mesh.indices[i] + meta.vertex_offset));
    }

    meshes_.emplace(meta.mesh_id, meta);
    gpu_buffers_dirty_ = true;

    printf("Renderer: Added mesh %d (%d verts, %d indices)\n", 
           meta.mesh_id, meta.vertex_count, meta.index_count);
    
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

void Renderer::BeginPass() {
    if (gpu_buffers_dirty_) rebuild_gpu_buffers();
    sg_begin_pass(&pass_desc_);
}

void Renderer::Render(const hmm_mat4& view_proj) {
    // Reuse cached map instead of creating new one each frame
    mesh_instances_cache_.clear();
    
    // Group instances by mesh
    for (const ModelInstance& inst : instances_) {
        mesh_instances_cache_[inst.mesh_id].push_back(inst.transform);
    }

    sg_apply_pipeline(pip_);

    for (auto& m : meshes_) {
        const MeshMeta& meta = m.second;
        auto it = mesh_instances_cache_.find(meta.mesh_id);
        if (it == mesh_instances_cache_.end()) continue;
        
        const std::vector<hmm_mat4>& instances_for_mesh = it->second;
        int instance_count = (int)instances_for_mesh.size();
        if (instance_count <= 0) continue;

        // Get or create instance buffer with growth strategy
        InstanceBufferInfo& buf_info = mesh_instance_bufs_[meta.mesh_id];
        size_t needed_instances = instances_for_mesh.size();

        if (buf_info.buffer.id == SG_INVALID_ID || buf_info.capacity < needed_instances) {
            // Destroy old buffer if exists
            if (buf_info.buffer.id != SG_INVALID_ID) {
                sg_destroy_buffer(buf_info.buffer);
            }
            
            // Allocate with 1.5x growth factor to reduce future reallocations
            size_t new_capacity = needed_instances * 3 / 2;
            if (new_capacity < 64) new_capacity = 64; // minimum capacity
            
            sg_buffer_desc inst_desc = {};
            inst_desc.usage = SG_USAGE_STREAM;
            inst_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
            inst_desc.size = (size_t)(new_capacity * sizeof(hmm_mat4));
            buf_info.buffer = sg_make_buffer(&inst_desc);
            buf_info.capacity = new_capacity;
        }

        // Update buffer data
        const uint32_t data_size = (uint32_t)(instances_for_mesh.size() * sizeof(hmm_mat4));
        sg_update_buffer(buf_info.buffer, { .ptr = instances_for_mesh.data(), .size = data_size });
        
        bind_.vertex_buffers[1] = buf_info.buffer;
        sg_apply_bindings(&bind_);

        vs_params_.mvp = view_proj;
        sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params_));

        if (meta.index_count > 0) {
            sg_draw(meta.index_offset, meta.index_count, instance_count);
        }

        bind_.vertex_buffers[1] = inst_vbuf_;
    }

    sg_apply_bindings(&bind_);
}

void Renderer::EndPass() {
    sg_end_pass();
}

void Renderer::OnResize() {
    create_render_targets();
}

void Renderer::Cleanup() {
    if (vbuf_.id != SG_INVALID_ID) { sg_destroy_buffer(vbuf_); vbuf_.id = SG_INVALID_ID; }
    if (ibuf_.id != SG_INVALID_ID) { sg_destroy_buffer(ibuf_); ibuf_.id = SG_INVALID_ID; }
    if (inst_vbuf_.id != SG_INVALID_ID) { sg_destroy_buffer(inst_vbuf_); inst_vbuf_.id = SG_INVALID_ID; }
    for (auto &kv : mesh_instance_bufs_) {
        if (kv.second.buffer.id != SG_INVALID_ID) { 
            sg_destroy_buffer(kv.second.buffer); 
        }
    }
    mesh_instance_bufs_.clear();
    mesh_instances_cache_.clear();
    if (pip_.id != SG_INVALID_ID)  { sg_destroy_pipeline(pip_); pip_.id = SG_INVALID_ID; }
    merged_vertices_.clear();
    merged_indices_.clear();
    meshes_.clear();
    instances_.clear();
    
}