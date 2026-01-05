#include "ModelLoader.h"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <map>

// Helper: Convert Assimp matrix to HandmadeMath matrix
static hmm_mat4 AssimpToHMM(const aiMatrix4x4& m) {
    hmm_mat4 result;
    result.Elements[0][0] = m.a1; result.Elements[0][1] = m.b1; result.Elements[0][2] = m.c1; result.Elements[0][3] = m.d1;
    result.Elements[1][0] = m.a2; result.Elements[1][1] = m.b2; result.Elements[1][2] = m.c2; result.Elements[1][3] = m.d2;
    result.Elements[2][0] = m.a3; result.Elements[2][1] = m.b3; result.Elements[2][2] = m.c3; result.Elements[2][3] = m.d3;
    result.Elements[3][0] = m.a4; result.Elements[3][1] = m.b4; result.Elements[3][2] = m.c4; result.Elements[3][3] = m.d4;
    return result;
}

// Helper: Convert Assimp quaternion to HandmadeMath quaternion
static hmm_quaternion AssimpToHMMQuat(const aiQuaternion& q) {
    return HMM_Quaternion(q.x, q.y, q.z, q.w);
}

// Helper: Convert Assimp vector to HandmadeMath vector
static hmm_vec3 AssimpToHMMVec3(const aiVector3D& v) {
    return HMM_Vec3(v.x, v.y, v.z);
}

// Helper: sRGB to linear conversion
static float srgb_to_linear(float c) {
    if (c <= 0.04045f) {
        return c / 12.92f;
    } else {
        return powf((c + 0.055f) / 1.055f, 2.4f);
    }
}

// Helper: Simple matrix inverse for transforms (assumes uniform scale)
static hmm_mat4 InverseTransform(const hmm_mat4& m) {
    hmm_mat4 result = HMM_Mat4d(1.0f);
    
    // Extract rotation part (3x3 upper-left)
    // For orthogonal matrices (rotations), inverse = transpose
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result.Elements[i][j] = m.Elements[j][i];  // Transpose
        }
    }
    
    // Invert translation: T' = -R^T * T
    hmm_vec3 translation = HMM_Vec3(m.Elements[3][0], m.Elements[3][1], m.Elements[3][2]);
    hmm_vec3 invTranslation = HMM_Vec3(
        -(result.Elements[0][0] * translation.X + result.Elements[0][1] * translation.Y + result.Elements[0][2] * translation.Z),
        -(result.Elements[1][0] * translation.X + result.Elements[1][1] * translation.Y + result.Elements[1][2] * translation.Z),
        -(result.Elements[2][0] * translation.X + result.Elements[2][1] * translation.Y + result.Elements[2][2] * translation.Z)
    );
    
    result.Elements[3][0] = invTranslation.X;
    result.Elements[3][1] = invTranslation.Y;
    result.Elements[3][2] = invTranslation.Z;
    
    return result;
}

// Helper: Process skeleton hierarchy recursively
static void ProcessBoneHierarchy(const aiNode* node, int parentIndex, Skeleton& skeleton, 
                                  std::map<std::string, int>& boneMapping) {
    std::string nodeName = node->mName.C_Str();
    
    // Check if this node is a bone
    auto it = boneMapping.find(nodeName);
    if (it != boneMapping.end()) {
        int boneIndex = it->second;
        skeleton.bones[boneIndex].parentIndex = parentIndex;
        parentIndex = boneIndex; // This bone becomes parent for children
    }
    
    // Process children
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        ProcessBoneHierarchy(node->mChildren[i], parentIndex, skeleton, boneMapping);
    }
}

Model3D ModelLoader::LoadModel(const char *path) {
    Model3D model = {};
    model.hasAnimations = false;
    
    printf("ModelLoader: Loading model from: %s\n", path);
    
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path,
        aiProcess_Triangulate | 
        aiProcess_JoinIdenticalVertices | 
        aiProcess_FlipUVs | 
        aiProcess_CalcTangentSpace |
        aiProcess_LimitBoneWeights);  // Limit to 4 bones per vertex
    
    if (!scene || !scene->HasMeshes()) {
        printf("ModelLoader: Failed to load model: %s\n", importer.GetErrorString());
        return model;
    }
    
    printf("  - Meshes: %d\n", scene->mNumMeshes);
    printf("  - Materials: %d\n", scene->mNumMaterials);
    printf("  - Animations: %d\n", scene->mNumAnimations);
    
    // Count total vertices and indices
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
    
    // Bone mapping (bone name -> bone index)
    std::map<std::string, int> boneMapping;
    int boneCount = 0;
    
    // Process each mesh
    for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx) {
        const aiMesh *mesh = scene->mMeshes[meshIdx];
        printf("  - Mesh %d: %d verts, %d bones\n", 
               meshIdx, mesh->mNumVertices, mesh->mNumBones);
        
        // Get material color
        aiColor4D materialColor(1.0f, 1.0f, 1.0f, 1.0f);
        if (scene->HasMaterials() && mesh->mMaterialIndex < scene->mNumMaterials) {
            const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            if (AI_SUCCESS != aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &materialColor)) {
                if (AI_SUCCESS != aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &materialColor)) {
                    materialColor = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
                }
            }
            materialColor.r = srgb_to_linear(materialColor.r);
            materialColor.g = srgb_to_linear(materialColor.g);
            materialColor.b = srgb_to_linear(materialColor.b);
        }
        
        // Initialize bone weights to zero
        std::vector<BoneWeight> vertexBoneWeights(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            for (int j = 0; j < 4; ++j) {
                vertexBoneWeights[i].boneIds[j] = 0;
                vertexBoneWeights[i].weights[j] = 0.0f;
            }
        }
        
        // Process bones for this mesh
        if (mesh->HasBones()) {
            model.hasAnimations = true;
            
            for (unsigned int boneIdx = 0; boneIdx < mesh->mNumBones; ++boneIdx) {
                const aiBone* bone = mesh->mBones[boneIdx];
                std::string boneName = bone->mName.C_Str();
                
                int globalBoneIndex;
                if (boneMapping.find(boneName) == boneMapping.end()) {
                    // New bone
                    globalBoneIndex = boneCount++;
                    boneMapping[boneName] = globalBoneIndex;
                    
                    Bone newBone;
                    newBone.name = boneName;
                    newBone.offsetMatrix = AssimpToHMM(bone->mOffsetMatrix);
                    newBone.parentIndex = -1;  // Will be set later
                    model.skeleton.bones.push_back(newBone);
                } else {
                    globalBoneIndex = boneMapping[boneName];
                }
                
                // Assign bone weights to vertices
                for (unsigned int weightIdx = 0; weightIdx < bone->mNumWeights; ++weightIdx) {
                    const aiVertexWeight& weight = bone->mWeights[weightIdx];
                    unsigned int vertexId = weight.mVertexId;
                    float weightValue = weight.mWeight;
                    
                    // Find empty slot (we limit to 4 bones per vertex)
                    for (int slot = 0; slot < 4; ++slot) {
                        if (vertexBoneWeights[vertexId].weights[slot] == 0.0f) {
                            vertexBoneWeights[vertexId].boneIds[slot] = globalBoneIndex;
                            vertexBoneWeights[vertexId].weights[slot] = weightValue;
                            break;
                        }
                    }
                }
            }
        }
        
        // Copy vertices with bone weights
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
            
            model.vertices[vIdx].color[0] = materialColor.r;
            model.vertices[vIdx].color[1] = materialColor.g;
            model.vertices[vIdx].color[2] = materialColor.b;
            model.vertices[vIdx].color[3] = materialColor.a;
            
            // FIXED: Copy bone weights (or use defaults for non-animated models)
            for (int j = 0; j < 4; ++j) {
                model.vertices[vIdx].boneIds[j] = vertexBoneWeights[i].boneIds[j];
                model.vertices[vIdx].boneWeights[j] = vertexBoneWeights[i].weights[j];
            }
            
            // ADDED: For non-animated models, ensure first bone weight is 1.0 (identity transform)
            // This prevents the vertex from being transformed by invalid bone data
            if (!mesh->HasBones()) {
                model.vertices[vIdx].boneIds[0] = 0;
                model.vertices[vIdx].boneWeights[0] = 1.0f;
                for (int j = 1; j < 4; ++j) {
                    model.vertices[vIdx].boneIds[j] = 0;
                    model.vertices[vIdx].boneWeights[j] = 0.0f;
                }
            }
            
            // Padding (for alignment)
            for (int j = 0; j < 4; ++j) {
                model.vertices[vIdx].padding[j] = 0.0f;
            }
        }
        
        // Copy indices
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace &face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                model.indices[index_offset++] = (uint16_t)(face.mIndices[j] + vertex_offset);
            }
        }
        
        vertex_offset += mesh->mNumVertices;
    }
    
    // Build bone hierarchy
    if (model.hasAnimations && !model.skeleton.bones.empty()) {
        model.skeleton.globalInverseTransform = AssimpToHMM(scene->mRootNode->mTransformation);
        model.skeleton.globalInverseTransform = InverseTransform(model.skeleton.globalInverseTransform);
        ProcessBoneHierarchy(scene->mRootNode, -1, model.skeleton, boneMapping);
        printf("  - Loaded %zu bones\n", model.skeleton.bones.size());
    }
    
    // Load animations
    if (scene->HasAnimations()) {
        for (unsigned int animIdx = 0; animIdx < scene->mNumAnimations; ++animIdx) {
            const aiAnimation* anim = scene->mAnimations[animIdx];
            AnimationClip clip;
            clip.name = anim->mName.C_Str();
            clip.duration = (float)anim->mDuration;
            clip.ticksPerSecond = (float)(anim->mTicksPerSecond != 0 ? anim->mTicksPerSecond : 25.0);
            
            printf("  - Animation %d: '%s' (%.2f sec, %.0f ticks/sec, %d channels)\n",
                   animIdx, clip.name.c_str(), clip.duration / clip.ticksPerSecond, 
                   clip.ticksPerSecond, anim->mNumChannels);
            
            for (unsigned int chanIdx = 0; chanIdx < anim->mNumChannels; ++chanIdx) {
                const aiNodeAnim* channel = anim->mChannels[chanIdx];
                AnimationChannel animChannel;
                animChannel.boneName = channel->mNodeName.C_Str();
                
                // Position keys
                for (unsigned int keyIdx = 0; keyIdx < channel->mNumPositionKeys; ++keyIdx) {
                    PositionKey key;
                    key.time = (float)channel->mPositionKeys[keyIdx].mTime;
                    key.value = AssimpToHMMVec3(channel->mPositionKeys[keyIdx].mValue);
                    animChannel.positionKeys.push_back(key);
                }
                
                // Rotation keys
                for (unsigned int keyIdx = 0; keyIdx < channel->mNumRotationKeys; ++keyIdx) {
                    RotationKey key;
                    key.time = (float)channel->mRotationKeys[keyIdx].mTime;
                    key.value = AssimpToHMMQuat(channel->mRotationKeys[keyIdx].mValue);
                    animChannel.rotationKeys.push_back(key);
                }
                
                // Scale keys
                for (unsigned int keyIdx = 0; keyIdx < channel->mNumScalingKeys; ++keyIdx) {
                    ScaleKey key;
                    key.time = (float)channel->mScalingKeys[keyIdx].mTime;
                    key.value = AssimpToHMMVec3(channel->mScalingKeys[keyIdx].mValue);
                    animChannel.scaleKeys.push_back(key);
                }
                
                clip.channels.push_back(animChannel);
            }
            
            model.animations.push_back(clip);
        }
    }
    
    printf("ModelLoader: Model loaded successfully\n");
    return model;
}