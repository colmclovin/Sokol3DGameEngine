#pragma once

#include "../../include/Model.h"
#include "../Game/ECS.h"
#include "../Renderer/Renderer.h"
#include "../../External/HandmadeMath.h"

// Quad creation utilities
namespace QuadGeometry {

    // Create a horizontal ground quad (facing up, Y = 0)
    Model3D CreateGroundQuad(float size, float uvScale);

    // Create a vertical textured quad (facing forward, centered at origin)
    Model3D CreateTexturedQuad(float width, float height, const char* texturePath = nullptr);

    // Helper function to add a textured quad entity to ECS
    // Returns the created entity ID, or -1 on error
    EntityId AddTexturedQuadEntity(ECS& ecs, Renderer& renderer,
                                   float width, float height,
                                   hmm_vec3 position,
                                   float yaw, float pitch, float roll,
                                   const char* texturePath = nullptr);

} // namespace QuadGeometry