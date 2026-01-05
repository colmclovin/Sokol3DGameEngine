#pragma once

#include "../External/HandmadeMath.h"
#include "../External/Sokol/sokol_gfx.h"
#include <cstdint>
#include <vector>
#include <string>

// ADDED: Bone weights for skinning (per vertex, max 4 bones per vertex)
struct BoneWeight {
    int boneIds[4];     // Bone indices (into skeleton bone array)
    float weights[4];   // Weight for each bone (sum should be 1.0)
};

// ADDED: Keyframe for position
struct PositionKey {
    float time;
    hmm_vec3 value;
};

// ADDED: Keyframe for rotation (quaternion)
struct RotationKey {
    float time;
    hmm_quaternion value;  // FIXED: Use hmm_quaternion instead of hmm_quat
};

// ADDED: Keyframe for scale
struct ScaleKey {
    float time;
    hmm_vec3 value;
};

// ADDED: Animation channel (one bone's animation over time)
struct AnimationChannel {
    std::string boneName;
    std::vector<PositionKey> positionKeys;
    std::vector<RotationKey> rotationKeys;
    std::vector<ScaleKey> scaleKeys;
};

// ADDED: Animation clip (collection of channels)
struct AnimationClip {
    std::string name;
    float duration;
    float ticksPerSecond;
    std::vector<AnimationChannel> channels;
};

// ADDED: Bone in skeleton hierarchy
struct Bone {
    std::string name;
    hmm_mat4 offsetMatrix;  // Transform from mesh space to bone space
    int parentIndex;        // Index of parent bone (-1 for root)
};

// ADDED: Skeleton (bone hierarchy)
struct Skeleton {
    std::vector<Bone> bones;
    hmm_mat4 globalInverseTransform;  // Root transform inverse
};

// Vertex with position, normal, UV, color, and bone weights
struct Vertex {
    float pos[3];           // 12 bytes
    float normal[3];        // 12 bytes
    float uv[2];            // 8 bytes
    float color[4];         // 16 bytes
    // ADDED: Bone skinning data (48 bytes)
    int boneIds[4];         // 16 bytes
    float boneWeights[4];   // 16 bytes
    float padding[4];       // 16 bytes (align to 16-byte boundary)
}; // Total: 96 bytes

struct Model3D {
    Vertex* vertices;
    uint16_t* indices;
    int vertex_count;
    int index_count;

    // Texture data (optional)
    unsigned char* texture_data;
    int texture_width;
    int texture_height;
    int texture_channels;
    bool has_texture;
    
    // ADDED: Animation data
    bool hasAnimations;
    Skeleton skeleton;
    std::vector<AnimationClip> animations;
};

// Instance referencing a mesh loaded into the Renderer
struct ModelInstance {
    int mesh_id;
    hmm_mat4 transform;
    bool active = true;
};

// Light component
struct Light {
    hmm_vec3 color;
    float intensity;
    float radius;
    bool enabled;
};