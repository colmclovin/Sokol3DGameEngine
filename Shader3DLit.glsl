@ctype mat4 hmm_mat4
@ctype vec2 hmm_vec2
@ctype vec4 hmm_vec4

@vs Shader3DLit_vs
layout(binding=0) uniform vs_params {
    mat4 mvp;
    mat4 model; 
    float is_screen_space; // 1.0 for screen-space HUD, 0.0 for world-space
};

in vec3 pos;
in vec3 normal_in;
in vec2 uv_in;
in vec4 color_in;

// Instance data (4x vec4 = mat4)
in vec4 inst_row0;
in vec4 inst_row1;
in vec4 inst_row2;
in vec4 inst_row3;

out vec2 uv;
out vec3 world_pos;       // World-space position for lighting
out vec3 world_normal;
out vec4 vertex_color;

void main() {
    mat4 inst_transform = mat4(inst_row0, inst_row1, inst_row2, inst_row3);
    vec4 world_position = inst_transform * vec4(pos, 1.0);
    
    gl_Position = mvp * world_position;
    world_pos = world_position.xyz;
    
    // Transform normal to world space (assumes uniform scaling)
    mat3 normal_matrix = mat3(inst_transform);
    world_normal = normalize(normal_matrix * normal_in);
    
    uv = uv_in;
    vertex_color = color_in;
}
@end

@fs Shader3DLit_fs
// Maximum number of lights
#define MAX_LIGHTS 8

layout(binding=1) uniform fs_params {
    // Light data packed into vec4 arrays
    // light_data[i] = vec4(position.xyz, intensity)
    vec4 light_data[MAX_LIGHTS];
    // light_colors[i] = vec4(color.rgb, radius)
    vec4 light_colors[MAX_LIGHTS];
    // ambient_data = vec4(ambient_color.rgb, ambient_intensity)
    vec4 ambient_data;
    // sun_data = vec4(direction.xyz, intensity)
    vec4 sun_data;
    // sun_color_misc = vec4(color.rgb, light_count)
    vec4 sun_color_misc;
};

in vec2 uv;
in vec3 world_pos;
in vec3 world_normal;
in vec4 vertex_color;

out vec4 frag_color;

void main() {
    vec3 normal = normalize(world_normal);
    vec3 ambient_color = ambient_data.xyz;
    float ambient_intensity = ambient_data.w;
    vec3 ambient = ambient_color * ambient_intensity;
    vec3 lighting = ambient;
    
    // Add directional sun light
    vec3 sun_dir = normalize(sun_data.xyz);
    float sun_intensity = sun_data.w;
    vec3 sun_color = sun_color_misc.xyz;
    
    if (sun_intensity > 0.0) {
        float sun_diff = max(dot(normal, sun_dir), 0.0);
        lighting += sun_color * sun_intensity * sun_diff;
    }
    
    int light_count = int(sun_color_misc.w);
    
    // Calculate lighting from each point light
    for (int i = 0; i < light_count && i < MAX_LIGHTS; i++) {
        vec3 light_pos = light_data[i].xyz;
        float light_intensity = light_data[i].w;
        vec3 light_color = light_colors[i].xyz;
        float light_radius = light_colors[i].w;
        
        vec3 light_dir = light_pos - world_pos;
        float distance = length(light_dir);
        
        // Skip if outside light radius
        if (distance > light_radius) continue;
        
        light_dir = normalize(light_dir);
        
        // Diffuse lighting
        float diff = max(dot(normal, light_dir), 0.0);
        
        // Attenuation (inverse square with radius)
        float attenuation = 1.0 - smoothstep(0.0, light_radius, distance);
        attenuation *= attenuation; // Square for more dramatic falloff
        
        // Add light contribution
        vec3 light_contribution = light_color * light_intensity * diff * attenuation;
        lighting += light_contribution;
    }
    
    // Apply lighting to vertex color
    frag_color = vec4(vertex_color.rgb * lighting, vertex_color.a);
}
@end

@program Shader3DLit Shader3DLit_vs Shader3DLit_fs