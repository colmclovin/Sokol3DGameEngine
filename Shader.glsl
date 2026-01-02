@ctype mat4 hmm_mat4
@ctype vec2 hmm_vec2

@vs Shader_vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
};

layout(location=0) in vec3 pos;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec2 uv_in;
layout(location=3) in vec4 color_in;

// per-instance model matrix (locations 4..7)
layout(location=4) in vec4 inst_row0;
layout(location=5) in vec4 inst_row1;
layout(location=6) in vec4 inst_row2;
layout(location=7) in vec4 inst_row3;

out vec2 uv;
out vec3 world_normal;
out vec4 vertex_color;

void main() {
    mat4 instance_model = mat4(inst_row0, inst_row1, inst_row2, inst_row3);
    gl_Position = mvp * instance_model * vec4(pos, 1.0);
    uv = uv_in;
    world_normal = mat3(instance_model) * normal_in;
    vertex_color = color_in;
}
@end

@fs Shader_fs
in vec2 uv;
in vec3 world_normal;
in vec4 vertex_color;

layout(binding=1) uniform fs_params {
    vec4 color;
    int  type;
};

layout(binding=2) uniform texture2D albedo_tex;
layout(binding=3) uniform sampler albedo_smp;

out vec4 frag_color;

void main() {
    // Simple directional lighting
    vec3 light_dir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 normal = normalize(world_normal);
    
    float ndotl = max(dot(normal, light_dir), 0.0);
    
    float ambient = 0.4;
    float diffuse = 0.6;
    float lighting = ambient + diffuse * ndotl;
    
    // Use vertex color (from material) with lighting
    vec3 lit_color = vertex_color.rgb * lighting;
    frag_color = vec4(lit_color, vertex_color.a);
}
@end

@program Shader Shader_vs Shader_fs