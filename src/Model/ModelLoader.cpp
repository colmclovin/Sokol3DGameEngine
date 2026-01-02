#include "ModelLoader.h"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

Model3D ModelLoader::LoadModel(const char *path) {
    Model3D model = {};
    printf("ModelLoader: Loading model from: %s\n", path);
    
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path,
        aiProcess_Triangulate | 
        aiProcess_JoinIdenticalVertices | 
        aiProcess_FlipUVs | 
        aiProcess_CalcTangentSpace);
    
    if (!scene || !scene->HasMeshes()) {
        printf("ModelLoader: Failed to load model: %s\n", importer.GetErrorString());
        return model;
    }
    
    printf("  - Meshes: %d\n", scene->mNumMeshes);
    printf("  - Materials: %d\n", scene->mNumMaterials);
    
    // Count total vertices and indices across ALL meshes
    int total_vertex_count = 0;
    int total_index_count = 0;
    for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx) {
        const aiMesh* mesh = scene->mMeshes[meshIdx];
        total_vertex_count += mesh->mNumVertices;
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            total_index_count += mesh->mFaces[i].mNumIndices;
        }
    }
    
    printf("  - Total Vertices: %d\n", total_vertex_count);
    printf("  - Total Indices: %d\n", total_index_count);
    
    model.vertex_count = total_vertex_count;
    model.index_count = total_index_count;
    model.vertices = (Vertex *)malloc(sizeof(Vertex) * total_vertex_count);
    model.indices = (uint16_t *)malloc(sizeof(uint16_t) * total_index_count);
    
    int vertex_offset = 0;
    int index_offset = 0;
    
    // sRGB to linear conversion helper
    auto srgb_to_linear = [](float c) -> float {
        if (c <= 0.04045f) {
            return c / 12.92f;
        } else {
            return powf((c + 0.055f) / 1.055f, 2.4f);
        }
    };
    
    // Process each mesh
    for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx) {
        const aiMesh *mesh = scene->mMeshes[meshIdx];
        printf("  - Mesh %d: %d verts, material %d\n", 
               meshIdx, mesh->mNumVertices, mesh->mMaterialIndex);
        
        // Get material color for this mesh
        aiColor4D materialColor(1.0f, 1.0f, 1.0f, 1.0f); // default white
        if (scene->HasMaterials() && mesh->mMaterialIndex < scene->mNumMaterials) {
            const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            
            // Try PBR base color first, then legacy diffuse
            if (AI_SUCCESS != aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &materialColor)) {
                if (AI_SUCCESS != aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &materialColor)) {
                    materialColor = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
                }
            }
            
            // Convert from sRGB to linear for proper lighting
            materialColor.r = srgb_to_linear(materialColor.r);
            materialColor.g = srgb_to_linear(materialColor.g);
            materialColor.b = srgb_to_linear(materialColor.b);
            
            printf("    Color (linear): (%.2f, %.2f, %.2f, %.2f)\n", 
                   materialColor.r, materialColor.g, materialColor.b, materialColor.a);
        }
        
        // Copy vertices with material color baked in
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            aiVector3D pos = mesh->mVertices[i];
            aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
            aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
            
            int vIdx = vertex_offset + i;
            model.vertices[vIdx].pos[0] = pos.x;
            model.vertices[vIdx].pos[1] = pos.y;
            model.vertices[vIdx].pos[2] = pos.z;
            
            model.vertices[vIdx].normal[0] = normal.x;
            model.vertices[vIdx].normal[1] = normal.y;
            model.vertices[vIdx].normal[2] = normal.z;
            
            model.vertices[vIdx].uv[0] = uv.x;
            model.vertices[vIdx].uv[1] = uv.y;
            
            // Bake material color into vertex
            model.vertices[vIdx].color[0] = materialColor.r;
            model.vertices[vIdx].color[1] = materialColor.g;
            model.vertices[vIdx].color[2] = materialColor.b;
            model.vertices[vIdx].color[3] = materialColor.a;
        }
        
        // Copy indices with vertex offset
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace &face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                model.indices[index_offset++] = (uint16_t)(face.mIndices[j] + vertex_offset);
            }
        }
        
        vertex_offset += mesh->mNumVertices;
    }
    
    printf("ModelLoader: Model loaded successfully\n");
    return model;
}