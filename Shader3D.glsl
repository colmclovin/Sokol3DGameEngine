@ctype mat4 hmm_mat4
@ctype vec2 hmm_vec2

@vs Shader3D_vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
    mat4 model; 
    float is_screen_space; // 1.0 for screen-space HUD, 0.0 for world-space
};

in vec3 pos;
in vec3 normal_in;
in vec2 uv_in;
in vec4 color_in;
in vec4 inst_row0;
in vec4 inst_row1;
in vec4 inst_row2;
in vec4 inst_row3;

out vec2 uv;
out vec3 world_normal;
out vec4 vertex_color;

void main() {
    mat4 inst_mat = mat4(inst_row0, inst_row1, inst_row2, inst_row3);
    // FIXED: Correct order - matrix * matrix * vector
    gl_Position = mvp * inst_mat * vec4(pos, 1.0);
    uv = uv_in;
    // FIXED: Correct order for normal transformation
    world_normal = mat3(inst_row0.xyz, inst_row1.xyz, inst_row2.xyz) * normal_in;
    vertex_color = color_in;
}
@end

@fs Shader3D_fs
in vec2 uv;
in vec3 world_normal;
in vec4 vertex_color;

out vec4 frag_color;

void main() {
    // Simple directional lighting
    vec3 light_dir = normalize(vec3(0.5, 1.0, 0.3));
    float ndotl = max(dot(normalize(world_normal), light_dir), 0.0);
    float light = 0.4 + 0.6 * ndotl;
    
    // Apply lighting to vertex color
    vec3 color = vertex_color.rgb * light;
    
    frag_color = vec4(color, vertex_color.a);
}
@end

@program Shader3D Shader3D_vs Shader3D_fs