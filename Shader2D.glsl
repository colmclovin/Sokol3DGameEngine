@ctype mat4 hmm_mat4
@ctype vec2 hmm_vec2
@vs Shader2D_vs
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
out vec4 vertex_color;

void main() {
    mat4 inst_mat = mat4(inst_row0, inst_row1, inst_row2, inst_row3);
    vec4 transformed = inst_mat * vec4(pos, 1.0);
    
    // If screen-space (is_screen_space > 0.5), use transformed position directly (already in NDC)
    // Otherwise, apply MVP transformation for world-space 2D rendering
    if (is_screen_space > 0.5) {
        gl_Position = transformed;
    } else {
        gl_Position = mvp * transformed;
    }
    
    uv = uv_in;
    vertex_color = color_in;
}
@end

@fs Shader2D_fs
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec2 uv;
in vec4 vertex_color;

out vec4 frag_color;

void main() {
    // Sample texture and multiply by vertex color (for tinting)
    vec4 tex_color = texture(sampler2D(tex, smp), uv);
    frag_color = tex_color * vertex_color;
}
@end

@program Shader2D Shader2D_vs Shader2D_fs