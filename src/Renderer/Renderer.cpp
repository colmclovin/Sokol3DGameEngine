#include "Renderer.h"
#include "../../../External/Sokol/sokol_log.h"
#include "../../../External/stb_image.h"
#include <stdio.h>
#include <string.h>

// Include the actual shader headers here (in the .cpp file only)
// The vs_params_t conflict is suppressed because ShaderCommon.h defined it first
#include "Shader3D.h"
#include "Shader2D.h"
#include "Shader3DLit.h"

#define MAX_VERTICES_STREAM (8 * 1024 * 10 * 4)
#define MAX_INDICES_STREAM  (8 * 1024 * 10 * 6)

Renderer::Renderer() noexcept
    : vbuf_()
    , ibuf_()
    , inst_vbuf_()
    , pip_3d_()
    , pip_3d_no_depth_()
    , pip_2d_()
    , pip_2d_no_depth_()
    , pip_3d_lit_()
    , pip_3d_lines_()
    , pip_3d_lines_no_depth_()  // ADDED
    , default_texture_()
    , texture_sampler_()
    , next_mesh_id_(1)
    , next_instance_id_(1)
    , gpu_buffers_dirty_(false)
{
    vbuf_.id = SG_INVALID_ID;
    ibuf_.id = SG_INVALID_ID;   
    inst_vbuf_.id = SG_INVALID_ID;
    pip_3d_.id  = SG_INVALID_ID;
    pip_3d_no_depth_.id = SG_INVALID_ID;
    pip_2d_.id = SG_INVALID_ID;
    pip_2d_no_depth_.id = SG_INVALID_ID;
    pip_3d_lit_.id = SG_INVALID_ID;
    pip_3d_lines_.id = SG_INVALID_ID;
    pip_3d_lines_no_depth_.id = SG_INVALID_ID;  // ADDED
    texture_sampler_.id = SG_INVALID_ID;

    memset(&bind_, 0, sizeof(bind_));
    memset(&pass_action_, 0, sizeof(pass_action_));
    memset(&pass_desc_, 0, sizeof(pass_desc_));
    vs_params_ = {};
}

Renderer::~Renderer() noexcept {
    Cleanup();
}

sg_image Renderer::create_texture_from_data(unsigned char* data, int width, int height, int channels) {
    sg_image_desc img_desc = {};
    img_desc.width = width;
    img_desc.height = height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8; // Always use RGBA8
    img_desc.data.subimage[0][0].ptr = data;
    img_desc.data.subimage[0][0].size = (size_t)(width * height * channels);
    img_desc.label = "texture";
    
    return sg_make_image(&img_desc);
}

bool Renderer::Init() {
    create_render_targets();

    // Create default white texture (1x1 white pixel)
    unsigned char white_pixel[4] = {255, 255, 255, 255};
    default_texture_ = create_texture_from_data(white_pixel, 1, 1, 4);

    // Create shared sampler for textures
    sg_sampler_desc sampler_desc = {};
    sampler_desc.min_filter = SG_FILTER_LINEAR;
    sampler_desc.mag_filter = SG_FILTER_LINEAR;
    sampler_desc.wrap_u = SG_WRAP_REPEAT;
    sampler_desc.wrap_v = SG_WRAP_REPEAT;
    texture_sampler_ = sg_make_sampler(&sampler_desc);

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

    // Create 3D shader for models (vertex colors + lighting)
    sg_shader shader_3d = sg_make_shader(Shader3D_shader_desc(sg_query_backend()));

    // Create 2D shader for textured quads
    sg_shader shader_2d = sg_make_shader(Shader2D_shader_desc(sg_query_backend()));

    // Setup common pipeline desc
    sg_pipeline_desc pip_desc = {};
    pip_desc.shader = shader_3d;  // Start with 3D shader
    
    // Configure buffer[0] stride for per-vertex data
    pip_desc.layout.buffers[0].stride = sizeof(Vertex); // 48 bytes
    
    // Per-vertex attributes (buffer 0)
    pip_desc.layout.attrs[ATTR_Shader3D_pos].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader3D_pos].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_Shader3D_pos].offset = 0;
    
    pip_desc.layout.attrs[ATTR_Shader3D_normal_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader3D_normal_in].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_Shader3D_normal_in].offset = 12;
    
    pip_desc.layout.attrs[ATTR_Shader3D_uv_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader3D_uv_in].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.layout.attrs[ATTR_Shader3D_uv_in].offset = 24;
    
    pip_desc.layout.attrs[ATTR_Shader3D_color_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader3D_color_in].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[ATTR_Shader3D_color_in].offset = 32;
    
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

    // Create 3D pipeline with depth testing
    pip_3d_ = sg_make_pipeline(&pip_desc);

    // Create 3D pipeline without depth (not used currently)
    sg_pipeline_desc pip_3d_no_depth_desc = pip_desc;
    pip_3d_no_depth_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    pip_3d_no_depth_desc.depth.write_enabled = false;
    pip_3d_no_depth_ = sg_make_pipeline(&pip_3d_no_depth_desc);

    // Create 2D pipeline for textured quads
    pip_desc.shader = shader_2d;
    
    // Update attribute indices for 2D shader
    pip_desc.layout.attrs[ATTR_Shader2D_pos].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader2D_pos].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_Shader2D_pos].offset = 0;
    
    pip_desc.layout.attrs[ATTR_Shader2D_normal_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader2D_normal_in].format = SG_VERTEXFORMAT_FLOAT3;
    pip_desc.layout.attrs[ATTR_Shader2D_normal_in].offset = 12;
    
    pip_desc.layout.attrs[ATTR_Shader2D_uv_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader2D_uv_in].format = SG_VERTEXFORMAT_FLOAT2;
    pip_desc.layout.attrs[ATTR_Shader2D_uv_in].offset = 24;
    
    pip_desc.layout.attrs[ATTR_Shader2D_color_in].buffer_index = 0;
    pip_desc.layout.attrs[ATTR_Shader2D_color_in].format = SG_VERTEXFORMAT_FLOAT4;
    pip_desc.layout.attrs[ATTR_Shader2D_color_in].offset = 32;
    
    pip_2d_ = sg_make_pipeline(&pip_desc);

    // Create 2D screen-space pipeline (no depth)
    sg_pipeline_desc pip_2d_screen_desc = pip_desc;
    pip_2d_screen_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
    pip_2d_screen_desc.depth.write_enabled = false;
    pip_2d_no_depth_ = sg_make_pipeline(&pip_2d_screen_desc);

    // Create 3D lit pipeline
    sg_pipeline_desc pip_lit_desc = pip_desc;
    pip_lit_desc.shader = sg_make_shader(Shader3DLit_shader_desc(sg_query_backend()));
    pip_3d_lit_ = sg_make_pipeline(&pip_lit_desc);

    // Create 3D line pipeline (for wireframes)
    printf("Creating line rendering pipeline...\n");
    sg_pipeline_desc pip_lines_desc = pip_desc;
    pip_lines_desc.shader = shader_3d;
    pip_lines_desc.primitive_type = SG_PRIMITIVETYPE_LINES;
    pip_lines_desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;  // Normal depth testing
    pip_lines_desc.depth.write_enabled = false;  // Don't write depth
    pip_3d_lines_ = sg_make_pipeline(&pip_lines_desc);
    
    // ADDED: Create 3D line pipeline for gizmos (NO depth test - always on top)
    printf("Creating gizmo line rendering pipeline (no depth test)...\n");
    sg_pipeline_desc pip_gizmo_desc = pip_lines_desc;
    pip_gizmo_desc.depth.compare = SG_COMPAREFUNC_ALWAYS;  // Always pass depth test
    pip_gizmo_desc.depth.write_enabled = false;  // Don't write depth
    pip_3d_lines_no_depth_ = sg_make_pipeline(&pip_gizmo_desc);

    // Initialize lighting params
    fs_params_ = {};
    fs_params_.ambient_data = HMM_Vec4(0.3f, 0.3f, 0.4f, 0.2f); // ambient color + intensity
    fs_params_.sun_data = HMM_Vec4(0.0f, -1.0f, 0.0f, 0.0f); // sun direction + intensity (disabled initially)
    fs_params_.sun_color_misc = HMM_Vec4(1.0f, 1.0f, 0.9f, 0.0f); // sun color + light count
    camera_pos_ = HMM_Vec3(0.0f, 0.0f, 0.0f);

    printf("Renderer: Initialized with 3D and 2D shader support\n");
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
    meta.has_texture = mesh.has_texture;
    meta.is_wireframe = false;
    meta.is_gizmo = false;  // ADDED
    
    // Create texture if provided
    if (mesh.has_texture && mesh.texture_data) {
        meta.texture = create_texture_from_data(mesh.texture_data, 
                                                mesh.texture_width, 
                                                mesh.texture_height, 
                                                mesh.texture_channels);
        printf("Renderer: Created texture %dx%d (%d channels) for mesh %d\n",
               mesh.texture_width, mesh.texture_height, mesh.texture_channels, meta.mesh_id);
    } else {
        meta.texture = default_texture_;
    }

    // Append vertices
    merged_vertices_.insert(merged_vertices_.end(), mesh.vertices, mesh.vertices + mesh.vertex_count);

    // Append indices with offset
    for (int i = 0; i < mesh.index_count; ++i) {
        merged_indices_.push_back((uint16_t)(mesh.indices[i] + meta.vertex_offset));
    }

    meshes_.emplace(meta.mesh_id, meta);
    model_data_[meta.mesh_id] = mesh;  // ADDED: Store full model data
    gpu_buffers_dirty_ = true;

    printf("Renderer: Added mesh %d (%d verts, %d indices, %s)\n", 
           meta.mesh_id, meta.vertex_count, meta.index_count,
           meta.has_texture ? "textured" : "vertex-color");
    
    return next_mesh_id_++;
}

void Renderer::RemoveMesh(int meshId) {
    auto it = meshes_.find(meshId);
    if (it == meshes_.end()) return;
    
    // Destroy texture if it's not the default texture
    if (it->second.has_texture && it->second.texture.id != default_texture_.id) {
        sg_destroy_image(it->second.texture);
    }
    
    meshes_.erase(it);
    gpu_buffers_dirty_ = true;
}

int Renderer::AddInstance(int meshId, const hmm_mat4& transform) {
    if (meshes_.find(meshId) == meshes_.end()) return -1;
    ModelInstance inst;
    inst.mesh_id = meshId;
    inst.transform = transform;
    inst.active = true; // Mark instance as active by default
    instances_.push_back(inst);
    return (int)instances_.size() - 1;
}

void Renderer::UpdateInstanceTransform(int instanceId, const hmm_mat4& transform) {
    if (instanceId < 0 || instanceId >= (int)instances_.size()) return;
    instances_[instanceId].transform = transform;
}

void Renderer::RemoveInstance(int instanceId) {
    if (instanceId < 0 || instanceId >= (int)instances_.size()) return;
    printf("Renderer: Marking instance %d as inactive (mesh_id=%d)\n", 
           instanceId, instances_[instanceId].mesh_id);
    instances_[instanceId].active = false;  // Mark as inactive instead of erasing
    // Also remove from screenSpaceInstances if present
    screenSpaceInstances_.erase(instanceId);
}

void Renderer::SetInstanceScreenSpace(int instanceId, bool isScreenSpace) {
    if (instanceId < 0 || instanceId >= (int)instances_.size()) return;
    
    if (isScreenSpace) {
        screenSpaceInstances_.insert(instanceId);
    } else {
        screenSpaceInstances_.erase(instanceId);
    }
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

void Renderer::render_instances(const hmm_mat4& view_proj, const std::set<int>& instanceFilter, bool exclude, bool use2DShader) {
    // Reuse cached map
    mesh_instances_cache_.clear();
    
    // Group instances by mesh, filtering based on screen-space status AND active state
    for (size_t i = 0; i < instances_.size(); ++i) {
        // ADDED: Skip inactive instances
        if (!instances_[i].active) continue;
        
        bool isScreenSpace = (screenSpaceInstances_.find((int)i) != screenSpaceInstances_.end());
        bool shouldInclude = exclude ? !isScreenSpace : isScreenSpace;
        
        if ((instanceFilter.empty() && shouldInclude) || 
            (!instanceFilter.empty() && (instanceFilter.find((int)i) != instanceFilter.end()) == !exclude)) {
            const ModelInstance& inst = instances_[i];
            mesh_instances_cache_[inst.mesh_id].push_back(inst.transform);
        }
    }

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
            if (buf_info.buffer.id != SG_INVALID_ID) {
                sg_destroy_buffer(buf_info.buffer);
            }
            
            size_t new_capacity = needed_instances * 3 / 2;
            if (new_capacity < 64) new_capacity = 64;
            
            sg_buffer_desc inst_desc = {};
            inst_desc.usage = SG_USAGE_STREAM;
            inst_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
            inst_desc.size = (size_t)(new_capacity * sizeof(hmm_mat4));
            buf_info.buffer = sg_make_buffer(&inst_desc);
            buf_info.capacity = new_capacity;
        }

        const uint32_t data_size = (uint32_t)(instances_for_mesh.size() * sizeof(hmm_mat4));
        sg_update_buffer(buf_info.buffer, { .ptr = instances_for_mesh.data(), .size = data_size });
        
        bind_.vertex_buffers[1] = buf_info.buffer;
        
        // Bind texture only for 2D shader (textured quads)
        if (use2DShader) {
            bind_.images[IMG_tex] = meta.texture;
            bind_.samplers[SMP_smp] = texture_sampler_;
        }
        
        sg_apply_bindings(&bind_);

        // Copy matrix data to the uniform - memcpy the float array directly
        vs_params_.mvp = view_proj;
        vs_params_.is_screen_space = use2DShader ? 1.0f : 0.0f; // Set flag for screen-space rendering
        sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params_));

        if (meta.index_count > 0) {
            sg_draw(meta.index_offset, meta.index_count, instance_count);
        }

        bind_.vertex_buffers[1] = inst_vbuf_;
    }

    sg_apply_bindings(&bind_);
}

void Renderer::Render(const hmm_mat4& view_proj) {
    sg_apply_pipeline(pip_3d_lit_);
    
    // Apply vertex shader uniforms
    sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params_));
    
    // Apply fragment shader params for lighting (UB_fs_params is slot 1)
    sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params_));
    
    // CHANGED: Exclude screen-space instances AND wireframe meshes
    // Group instances by mesh, excluding screen-space and wireframe meshes
    mesh_instances_cache_.clear();
    
    for (size_t i = 0; i < instances_.size(); ++i) {
        if (!instances_[i].active) continue;
        
        bool isScreenSpace = (screenSpaceInstances_.find((int)i) != screenSpaceInstances_.end());
        if (isScreenSpace) continue;  // Skip screen-space instances
        
        // ADDED: Skip wireframe meshes
        int meshId = instances_[i].mesh_id;
        if (wireframeMeshes_.find(meshId) != wireframeMeshes_.end()) {
            continue;  // Skip wireframe instances in main render
        }
        
        const ModelInstance& inst = instances_[i];
        mesh_instances_cache_[inst.mesh_id].push_back(inst.transform);
    }

    // Render all non-wireframe, non-screen-space meshes
    for (auto& m : meshes_) {
        const MeshMeta& meta = m.second;
        
        // Skip wireframe meshes in main render
        if (meta.is_wireframe) continue;
        
        auto it = mesh_instances_cache_.find(meta.mesh_id);
        if (it == mesh_instances_cache_.end()) continue;
        
        const std::vector<hmm_mat4>& instances_for_mesh = it->second;
        int instance_count = (int)instances_for_mesh.size();
        if (instance_count <= 0) continue;

        // Get or create instance buffer with growth strategy
        InstanceBufferInfo& buf_info = mesh_instance_bufs_[meta.mesh_id];
        size_t needed_instances = instances_for_mesh.size();

        if (buf_info.buffer.id == SG_INVALID_ID || buf_info.capacity < needed_instances) {
            if (buf_info.buffer.id != SG_INVALID_ID) {
                sg_destroy_buffer(buf_info.buffer);
            }
            
            size_t new_capacity = needed_instances * 3 / 2;
            if (new_capacity < 64) new_capacity = 64;
            
            sg_buffer_desc inst_desc = {};
            inst_desc.usage = SG_USAGE_STREAM;
            inst_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
            inst_desc.size = (size_t)(new_capacity * sizeof(hmm_mat4));
            buf_info.buffer = sg_make_buffer(&inst_desc);
            buf_info.capacity = new_capacity;
        }

        const uint32_t data_size = (uint32_t)(instances_for_mesh.size() * sizeof(hmm_mat4));
        sg_update_buffer(buf_info.buffer, { .ptr = instances_for_mesh.data(), .size = data_size });
        
        bind_.vertex_buffers[1] = buf_info.buffer;
        sg_apply_bindings(&bind_);

        vs_params_.mvp = view_proj;
        vs_params_.is_screen_space = 0.0f;
        sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params_));

        if (meta.index_count > 0) {
            sg_draw(meta.index_offset, meta.index_count, instance_count);
        }

        bind_.vertex_buffers[1] = inst_vbuf_;
    }

    sg_apply_bindings(&bind_);
}

void Renderer::RenderScreenSpace(const hmm_mat4& orthoProj) {
    if (screenSpaceInstances_.empty()) return;
    
    sg_apply_pipeline(pip_2d_no_depth_);  // Use 2D shader for HUD
    render_instances(orthoProj, screenSpaceInstances_, false, true); // 2D rendering with textures
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
    
    // Destroy textures
    for (auto& [id, meta] : meshes_) {
        if (meta.has_texture && meta.texture.id != default_texture_.id && meta.texture.id != SG_INVALID_ID) {
            sg_destroy_image(meta.texture);
        }
    }
    
    if (default_texture_.id != SG_INVALID_ID) {
        sg_destroy_image(default_texture_);
        default_texture_.id = SG_INVALID_ID;
    }
    
    if (texture_sampler_.id != SG_INVALID_ID) {
        sg_destroy_sampler(texture_sampler_);
        texture_sampler_.id = SG_INVALID_ID;
    }
    
    for (auto &kv : mesh_instance_bufs_) {
        if (kv.second.buffer.id != SG_INVALID_ID) { 
            sg_destroy_buffer(kv.second.buffer); 
        }
    }
    mesh_instance_bufs_.clear();
    mesh_instances_cache_.clear();
    
    if (pip_3d_.id != SG_INVALID_ID)  { sg_destroy_pipeline(pip_3d_); pip_3d_.id = SG_INVALID_ID; }
    if (pip_3d_no_depth_.id != SG_INVALID_ID) { sg_destroy_pipeline(pip_3d_no_depth_); pip_3d_no_depth_.id = SG_INVALID_ID; }
    if (pip_2d_.id != SG_INVALID_ID)  { sg_destroy_pipeline(pip_2d_); pip_2d_.id = SG_INVALID_ID; }
    if (pip_2d_no_depth_.id != SG_INVALID_ID) { sg_destroy_pipeline(pip_2d_no_depth_); pip_2d_no_depth_.id = SG_INVALID_ID; }
    
    if (pip_3d_lines_.id != SG_INVALID_ID) { 
        sg_destroy_pipeline(pip_3d_lines_); 
        pip_3d_lines_.id = SG_INVALID_ID; 
    }
    
    // ADDED
    if (pip_3d_lines_no_depth_.id != SG_INVALID_ID) { 
        sg_destroy_pipeline(pip_3d_lines_no_depth_); 
        pip_3d_lines_no_depth_.id = SG_INVALID_ID; 
    }
    
    model_data_.clear();  // ADDED: Clear model data
    
    merged_vertices_.clear();
    merged_indices_.clear();
    meshes_.clear();
    instances_.clear();
    screenSpaceInstances_.clear();
}

void Renderer::SetLights(const std::vector<hmm_vec3> &positions,
        const std::vector<hmm_vec3> &colors,
        const std::vector<float> &intensities,
        const std::vector<float> &radii) {
    int count = (int)positions.size();
    if (count > 512) count = 512; // CHANGED: Match shader array size

    // Pack light data into vec4 arrays
    for (int i = 0; i < count; i++) {
        fs_params_.light_data[i] = HMM_Vec4(positions[i].X, positions[i].Y, positions[i].Z, intensities[i]);
        fs_params_.light_colors[i] = HMM_Vec4(colors[i].X, colors[i].Y, colors[i].Z, radii[i]);
    }

    fs_params_.sun_color_misc.W = (float)count;
}

void Renderer::SetAmbientLight(const hmm_vec3& color, float intensity) {
    // ambient_data = vec4(color.rgb, intensity)
    fs_params_.ambient_data = HMM_Vec4(color.X, color.Y, color.Z, intensity);
}

void Renderer::SetCameraPosition(const hmm_vec3& position) {
    // Store camera position for future use (not currently used in shader)
    camera_pos_ = position;
}

void Renderer::SetSunLight(const hmm_vec3& direction, const hmm_vec3& color, float intensity) {
    // Normalize the direction vector
    float len = sqrtf(direction.X * direction.X + direction.Y * direction.Y + direction.Z * direction.Z);
    hmm_vec3 normalized = direction;
    if (len > 0.001f) {
        normalized.X /= len;
        normalized.Y /= len;
        normalized.Z /= len;
    }
    
    // sun_data = vec4(direction.xyz, intensity)
    fs_params_.sun_data = HMM_Vec4(normalized.X, normalized.Y, normalized.Z, intensity);
    
    // sun_color_misc.xyz = color (sun_color_misc.w is light_count, already set)
    fs_params_.sun_color_misc.X = color.X;
    fs_params_.sun_color_misc.Y = color.Y;
    fs_params_.sun_color_misc.Z = color.Z;
}

void Renderer::MarkMeshAsWireframe(int meshId, bool isWireframe) {
    auto it = meshes_.find(meshId);
    if (it != meshes_.end()) {
        it->second.is_wireframe = isWireframe;
        if (isWireframe) {
            wireframeMeshes_.insert(meshId);
            printf("Marked mesh %d as wireframe\n", meshId);
        } else {
            wireframeMeshes_.erase(meshId);
        }
    }
}

// ADDED: New method to mark mesh as gizmo
void Renderer::MarkMeshAsGizmo(int meshId, bool isGizmo) {
    auto it = meshes_.find(meshId);
    if (it != meshes_.end()) {
        it->second.is_gizmo = isGizmo;
        if (isGizmo) {
            gizmoMeshes_.insert(meshId);
            // Gizmos are also wireframes
            it->second.is_wireframe = true;
            wireframeMeshes_.insert(meshId);
            printf("Marked mesh %d as gizmo (renders on top)\n", meshId);
        } else {
            gizmoMeshes_.erase(meshId);
        }
    }
}

void Renderer::RenderWireframes(const hmm_mat4& view_proj) {
    if (wireframeMeshes_.empty()) return;
    
    // Apply line rendering pipeline WITH depth testing
    sg_apply_pipeline(pip_3d_lines_);
    
    // Build instance cache for wireframe meshes (excluding gizmos)
    std::unordered_map<int, std::vector<hmm_mat4>> wireframe_cache;
    
    for (size_t i = 0; i < instances_.size(); ++i) {
        if (!instances_[i].active) continue;
        
        int meshId = instances_[i].mesh_id;
        
        // Skip if not a wireframe mesh
        if (wireframeMeshes_.find(meshId) == wireframeMeshes_.end()) {
            continue;
        }
        
        // ADDED: Skip gizmo meshes (they render separately)
        if (gizmoMeshes_.find(meshId) != gizmoMeshes_.end()) {
            continue;
        }
        
        const ModelInstance& inst = instances_[i];
        wireframe_cache[inst.mesh_id].push_back(inst.transform);
    }
    
    // Render wireframe meshes (selection boxes) with depth testing
    for (auto& m : meshes_) {
        const MeshMeta& meta = m.second;
        
        if (!meta.is_wireframe || meta.is_gizmo) continue;  // CHANGED: Skip gizmos
        
        auto it = wireframe_cache.find(meta.mesh_id);
        if (it == wireframe_cache.end() || it->second.empty()) continue;
        
        const std::vector<hmm_mat4>& instances_for_mesh = it->second;
        int instance_count = (int)instances_for_mesh.size();
        
        // Get or create instance buffer
        InstanceBufferInfo& buf_info = mesh_instance_bufs_[meta.mesh_id];
        size_t needed_instances = instances_for_mesh.size();

        if (buf_info.buffer.id == SG_INVALID_ID || buf_info.capacity < needed_instances) {
            if (buf_info.buffer.id != SG_INVALID_ID) {
                sg_destroy_buffer(buf_info.buffer);
            }
            
            size_t new_capacity = needed_instances * 3 / 2;
            if (new_capacity < 64) new_capacity = 64;
            
            sg_buffer_desc inst_desc = {};
            inst_desc.usage = SG_USAGE_STREAM;
            inst_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
            inst_desc.size = (size_t)(new_capacity * sizeof(hmm_mat4));
            buf_info.buffer = sg_make_buffer(&inst_desc);
            buf_info.capacity = new_capacity;
        }

        const uint32_t data_size = (uint32_t)(instances_for_mesh.size() * sizeof(hmm_mat4));
        sg_update_buffer(buf_info.buffer, { .ptr = instances_for_mesh.data(), .size = data_size });
        
        bind_.vertex_buffers[1] = buf_info.buffer;
        sg_apply_bindings(&bind_);
        
        vs_params_.mvp = view_proj;
        vs_params_.is_screen_space = 0.0f;
        sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params_));
        
        if (meta.index_count > 0) {
            sg_draw(meta.index_offset, meta.index_count, instance_count);
        }
    }
    
    bind_.vertex_buffers[1] = inst_vbuf_;
    sg_apply_bindings(&bind_);
}

void Renderer::RenderGizmos(const hmm_mat4& view_proj) {
    if (gizmoMeshes_.empty()) return;
    
    // Apply line rendering pipeline WITHOUT depth testing (always on top)
    sg_apply_pipeline(pip_3d_lines_no_depth_);
    
    // Build instance cache for ONLY gizmo meshes
    std::unordered_map<int, std::vector<hmm_mat4>> gizmo_cache;
    
    for (size_t i = 0; i < instances_.size(); ++i) {
        if (!instances_[i].active) continue;
        
        int meshId = instances_[i].mesh_id;
        
        // Only include gizmo meshes
        if (gizmoMeshes_.find(meshId) == gizmoMeshes_.end()) {
            continue;
        }
        
        const ModelInstance& inst = instances_[i];
        gizmo_cache[inst.mesh_id].push_back(inst.transform);
    }
    
    // Render gizmo meshes with no depth test (always visible)
    for (auto& m : meshes_) {
        const MeshMeta& meta = m.second;
        
        if (!meta.is_gizmo) continue;
        
        auto it = gizmo_cache.find(meta.mesh_id);
        if (it == gizmo_cache.end() || it->second.empty()) continue;
        
        const std::vector<hmm_mat4>& instances_for_mesh = it->second;
        int instance_count = (int)instances_for_mesh.size();
        
        // Get or create instance buffer
        InstanceBufferInfo& buf_info = mesh_instance_bufs_[meta.mesh_id];
        size_t needed_instances = instances_for_mesh.size();

        if (buf_info.buffer.id == SG_INVALID_ID || buf_info.capacity < needed_instances) {
            if (buf_info.buffer.id != SG_INVALID_ID) {
                sg_destroy_buffer(buf_info.buffer);
            }
            
            size_t new_capacity = needed_instances * 3 / 2;
            if (new_capacity < 64) new_capacity = 64;
            
            sg_buffer_desc inst_desc = {};
            inst_desc.usage = SG_USAGE_STREAM;
            inst_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
            inst_desc.size = (size_t)(new_capacity * sizeof(hmm_mat4));
            buf_info.buffer = sg_make_buffer(&inst_desc);
            buf_info.capacity = new_capacity;
        }

        const uint32_t data_size = (uint32_t)(instances_for_mesh.size() * sizeof(hmm_mat4));
        sg_update_buffer(buf_info.buffer, { .ptr = instances_for_mesh.data(), .size = data_size });
        
        bind_.vertex_buffers[1] = buf_info.buffer;
        sg_apply_bindings(&bind_);
        
        vs_params_.mvp = view_proj;
        vs_params_.is_screen_space = 0.0f;
        sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params_));
        
        if (meta.index_count > 0) {
            sg_draw(meta.index_offset, meta.index_count, instance_count);
        }
    }
    
    bind_.vertex_buffers[1] = inst_vbuf_;
    sg_apply_bindings(&bind_);
}

// Add this implementation:
const Model3D* Renderer::GetModelData(int meshId) const {
    auto it = model_data_.find(meshId);
    if (it != model_data_.end()) {
        return &it->second;
    }
    return nullptr;
}