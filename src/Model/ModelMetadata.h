#pragma once
#include "../../External/HandmadeMath.h"

// Model metadata - stores per-model configuration like origin offsets
struct ModelMetadata {
    hmm_vec3 originOffset;
    hmm_vec3 defaultScale;
    hmm_vec3 defaultRotation; // euler angles in degrees (pitch, yaw, roll)
};

// Global model metadata registry
// Add entries here for each model type that needs special configuration
namespace ModelRegistry {
    // Tree model from cartoon_lowpoly_trees_blend.glb
    inline const ModelMetadata TREE_MODEL = {
        .originOffset = HMM_Vec3(-19.500f, 2.400f, 2.200f),
        .defaultScale = HMM_Vec3(1.0f, 1.0f, 1.0f),
        .defaultRotation = HMM_Vec3(90.0f, 0.0f, 0.0f) // pitch, yaw, roll
    };
    
    // Enemy model
    inline const ModelMetadata ENEMY_MODEL = {
        .originOffset = HMM_Vec3(0.0f, 0.0f, 0.0f),
        .defaultScale = HMM_Vec3(1.0f, 1.0f, 1.0f),
        .defaultRotation = HMM_Vec3(0.0f, 0.0f, 0.0f)
    };
    
    // Player model
    inline const ModelMetadata PLAYER_MODEL = {
        .originOffset = HMM_Vec3(0.0f, 0.0f, 0.0f),
        .defaultScale = HMM_Vec3(1.0f, 1.0f, 1.0f),
        .defaultRotation = HMM_Vec3(0.0f, 0.0f, 0.0f)
    };
}