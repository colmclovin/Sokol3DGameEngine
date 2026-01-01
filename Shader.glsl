@ctype mat4 hmm_mat4
@ctype vec2 hmm_vec2

@vs Shader_vs
layout(binding=0) uniform vs_params {
    mat4 mvp;

};

in vec3 pos;
in vec2 uv_in;


out vec2 uv;

void main() {

        gl_Position = mvp * vec4(pos, 1.0);

    uv = uv_in;
}
@end

@fs Shader_fs
in vec2 uv;

layout(binding=1) uniform fs_params {
    vec4 color;
    int  type;
};



out vec4 frag_color;

void main() {

    frag_color = vec4(1.0, 0.0, 0.0, 1.0);
}
@end

@program Shader Shader_vs Shader_fs