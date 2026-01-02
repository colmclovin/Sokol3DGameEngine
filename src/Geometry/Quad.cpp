#include "Quad.h"
#include "../../External/stb_image.h"
#include <stdlib.h>
#include <stdio.h>

namespace QuadGeometry {

Model3D CreateGroundQuad(float size, float uvScale) {
    Model3D quad = {};
    quad.vertex_count = 4;
    quad.index_count = 6;
    quad.has_texture = false;
    quad.texture_data = nullptr;
    
    quad.vertices = (Vertex*)malloc(sizeof(Vertex) * quad.vertex_count);
    quad.indices = (uint16_t*)malloc(sizeof(uint16_t) * quad.index_count);
    
    float half = size / 2.0f;
    
    // Ground quad vertices (facing up, Y = 0)
    // Bottom-left
    quad.vertices[0].pos[0] = -half; quad.vertices[0].pos[1] = 0.0f; quad.vertices[0].pos[2] = -half;
    quad.vertices[0].normal[0] = 0.0f; quad.vertices[0].normal[1] = 1.0f; quad.vertices[0].normal[2] = 0.0f;
    quad.vertices[0].uv[0] = 0.0f; quad.vertices[0].uv[1] = 0.0f;
    quad.vertices[0].color[0] = 0.3f; quad.vertices[0].color[1] = 0.6f; quad.vertices[0].color[2] = 0.3f; quad.vertices[0].color[3] = 1.0f;
    
    // Bottom-right
    quad.vertices[1].pos[0] = half; quad.vertices[1].pos[1] = 0.0f; quad.vertices[1].pos[2] = -half;
    quad.vertices[1].normal[0] = 0.0f; quad.vertices[1].normal[1] = 1.0f; quad.vertices[1].normal[2] = 0.0f;
    quad.vertices[1].uv[0] = uvScale; quad.vertices[1].uv[1] = 0.0f;
    quad.vertices[1].color[0] = 0.3f; quad.vertices[1].color[1] = 0.6f; quad.vertices[1].color[2] = 0.3f; quad.vertices[1].color[3] = 1.0f;
    
    // Top-right
    quad.vertices[2].pos[0] = half; quad.vertices[2].pos[1] = 0.0f; quad.vertices[2].pos[2] = half;
    quad.vertices[2].normal[0] = 0.0f; quad.vertices[2].normal[1] = 1.0f; quad.vertices[2].normal[2] = 0.0f;
    quad.vertices[2].uv[0] = uvScale; quad.vertices[2].uv[1] = uvScale;
    quad.vertices[2].color[0] = 0.3f; quad.vertices[2].color[1] = 0.6f; quad.vertices[2].color[2] = 0.3f; quad.vertices[2].color[3] = 1.0f;
    
    // Top-left
    quad.vertices[3].pos[0] = -half; quad.vertices[3].pos[1] = 0.0f; quad.vertices[3].pos[2] = half;
    quad.vertices[3].normal[0] = 0.0f; quad.vertices[3].normal[1] = 1.0f; quad.vertices[3].normal[2] = 0.0f;
    quad.vertices[3].uv[0] = 0.0f; quad.vertices[3].uv[1] = uvScale;
    quad.vertices[3].color[0] = 0.3f; quad.vertices[3].color[1] = 0.6f; quad.vertices[3].color[2] = 0.3f; quad.vertices[3].color[3] = 1.0f;
    
    // Indices (two triangles)
    quad.indices[0] = 0; quad.indices[1] = 1; quad.indices[2] = 2;
    quad.indices[3] = 0; quad.indices[4] = 2; quad.indices[5] = 3;
    
    return quad;
}

Model3D CreateTexturedQuad(float width, float height, const char* texturePath) {
    Model3D quad = {};
    quad.vertex_count = 4;
    quad.index_count = 6;
    quad.has_texture = false;
    quad.texture_data = nullptr;
    
    quad.vertices = (Vertex*)malloc(sizeof(Vertex) * quad.vertex_count);
    quad.indices = (uint16_t*)malloc(sizeof(uint16_t) * quad.index_count);
    
    float halfW = width / 2.0f;
    float halfH = height / 2.0f;
    
    // Quad vertices (facing camera, centered at origin)
    // Bottom-left
    quad.vertices[0].pos[0] = -halfW; quad.vertices[0].pos[1] = -halfH; quad.vertices[0].pos[2] = 0.0f;
    quad.vertices[0].normal[0] = 0.0f; quad.vertices[0].normal[1] = 0.0f; quad.vertices[0].normal[2] = 1.0f;
    quad.vertices[0].uv[0] = 0.0f; quad.vertices[0].uv[1] = 1.0f;
    quad.vertices[0].color[0] = 1.0f; quad.vertices[0].color[1] = 1.0f; quad.vertices[0].color[2] = 1.0f; quad.vertices[0].color[3] = 1.0f;
    
    // Bottom-right
    quad.vertices[1].pos[0] = halfW; quad.vertices[1].pos[1] = -halfH; quad.vertices[1].pos[2] = 0.0f;
    quad.vertices[1].normal[0] = 0.0f; quad.vertices[1].normal[1] = 0.0f; quad.vertices[1].normal[2] = 1.0f;
    quad.vertices[1].uv[0] = 1.0f; quad.vertices[1].uv[1] = 1.0f;
    quad.vertices[1].color[0] = 1.0f; quad.vertices[1].color[1] = 1.0f; quad.vertices[1].color[2] = 1.0f; quad.vertices[1].color[3] = 1.0f;
    
    // Top-right
    quad.vertices[2].pos[0] = halfW; quad.vertices[2].pos[1] = halfH; quad.vertices[2].pos[2] = 0.0f;
    quad.vertices[2].normal[0] = 0.0f; quad.vertices[2].normal[1] = 0.0f; quad.vertices[2].normal[2] = 1.0f;
    quad.vertices[2].uv[0] = 1.0f; quad.vertices[2].uv[1] = 0.0f;
    quad.vertices[2].color[0] = 1.0f; quad.vertices[2].color[1] = 1.0f; quad.vertices[2].color[2] = 1.0f; quad.vertices[2].color[3] = 1.0f;
    
    // Top-left
    quad.vertices[3].pos[0] = -halfW; quad.vertices[3].pos[1] = halfH; quad.vertices[3].pos[2] = 0.0f;
    quad.vertices[3].normal[0] = 0.0f; quad.vertices[3].normal[1] = 0.0f; quad.vertices[3].normal[2] = 1.0f;
    quad.vertices[3].uv[0] = 0.0f; quad.vertices[3].uv[1] = 0.0f;
    quad.vertices[3].color[0] = 1.0f; quad.vertices[3].color[1] = 1.0f; quad.vertices[3].color[2] = 1.0f; quad.vertices[3].color[3] = 1.0f;
    
    // Indices (two triangles)
    quad.indices[0] = 0; quad.indices[1] = 1; quad.indices[2] = 2;
    quad.indices[3] = 0; quad.indices[4] = 2; quad.indices[5] = 3;
    
    // Load texture if path provided
    if (texturePath != nullptr) {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1); // Flip Y axis for OpenGL/D3D coordinate system
        
        unsigned char* data = stbi_load(texturePath, &width, &height, &channels, 0);
        
        if (data) {
            quad.has_texture = true;
            quad.texture_width = width;
            quad.texture_height = height;
            quad.texture_channels = channels;
            
            // Convert to RGBA if necessary
            if (channels == 3) {
                // RGB -> RGBA conversion
                quad.texture_channels = 4;
                quad.texture_data = (unsigned char*)malloc(width * height * 4);
                for (int i = 0; i < width * height; ++i) {
                    quad.texture_data[i * 4 + 0] = data[i * 3 + 0]; // R
                    quad.texture_data[i * 4 + 1] = data[i * 3 + 1]; // G
                    quad.texture_data[i * 4 + 2] = data[i * 3 + 2]; // B
                    quad.texture_data[i * 4 + 3] = 255;              // A
                }
                stbi_image_free(data);
            } else if (channels == 4) {
                // Already RGBA, just copy
                size_t data_size = width * height * 4;
                quad.texture_data = (unsigned char*)malloc(data_size);
                memcpy(quad.texture_data, data, data_size);
                stbi_image_free(data);
            } else {
                printf("WARNING: Unsupported texture format (%d channels) for '%s'\n", channels, texturePath);
                stbi_image_free(data);
                quad.has_texture = false;
            }
            
            printf("Loaded texture: %s (%dx%d, %d channels)\n", texturePath, width, height, channels);
        } else {
            printf("ERROR: Failed to load texture: %s\n", texturePath);
            printf("STB Error: %s\n", stbi_failure_reason());
        }
    }
    
    return quad;
}

EntityId AddTexturedQuadEntity(ECS& ecs, Renderer& renderer,
                               float width, float height,
                               hmm_vec3 position,
                               float yaw, float pitch, float roll,
                               const char* texturePath) {
    // Create the quad mesh
    Model3D quadModel = CreateTexturedQuad(width, height, texturePath);
    int meshId = renderer.AddMesh(quadModel);
    
    // Free the temporary model data (renderer has copied it)
    if (quadModel.vertices) {
        free(quadModel.vertices);
    }
    if (quadModel.indices) {
        free(quadModel.indices);
    }
    if (quadModel.texture_data) {
        free(quadModel.texture_data); // Free texture data after renderer has copied it
    }
    
    if (meshId < 0) {
        printf("ERROR: Failed to add quad mesh to renderer!\n");
        return -1;
    }
    
    // Create entity with transform
    EntityId quadEntity = ecs.CreateEntity();
    Transform t;
    t.position = position;
    t.yaw = yaw;
    t.pitch = pitch;
    t.roll = roll;
    
    ecs.AddTransform(quadEntity, t);
    ecs.AddRenderable(quadEntity, meshId, renderer);
    
    printf("Textured quad entity created at (%.2f, %.2f, %.2f) with rotation (yaw=%.2f, pitch=%.2f, roll=%.2f)\n",
           position.X, position.Y, position.Z, yaw, pitch, roll);
    
    return quadEntity;
}

} // namespace QuadGeometry