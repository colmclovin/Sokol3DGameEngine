#pragma once

#include "Components.h"
#include <optional>

// Simple entity container
struct Entity {
    int id = -1;
    int meshId = -1;      // mesh lookup in Renderer
    int instanceId = -1;  // renderer instance id
    bool isStatic = false;

    Transform transform;
    std::optional<Rigidbody> rb;
    std::optional<AIController> ai;
    std::optional<Animator> animator;
};