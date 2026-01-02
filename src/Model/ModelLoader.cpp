#include "ModelLoader.h"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <cstdio>
#include <cstdlib>

   Model3D
   ModelLoader::LoadModel(const char *path) {
    Model3D model = {};
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs);
    if (!scene || !scene->HasMeshes()) {
        printf("ModelLoader: Assimp load failed: %s\n", importer.GetErrorString());
        return model;
    }
    const aiMesh *mesh = scene->mMeshes[0];
    model.vertex_count = (int)mesh->mNumVertices;
    model.vertices = (Vertex *)malloc(sizeof(Vertex) * model.vertex_count);
    int total_indices = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        total_indices += mesh->mFaces[i].mNumIndices;
    model.index_count = total_indices;
    model.indices = (uint16_t *)malloc(sizeof(uint16_t) * model.index_count);

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        aiVector3D pos = mesh->mVertices[i];
        aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
        model.vertices[i].pos[0] = pos.x;
        model.vertices[i].pos[1] = pos.y;
        model.vertices[i].pos[2] = pos.z;
        model.vertices[i].uv[0] = uv.x;
        model.vertices[i].uv[1] = uv.y;
    }
    int idx = 0;
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace &face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            model.indices[idx++] = (uint16_t)face.mIndices[j];
        }
    }
    return model;
}