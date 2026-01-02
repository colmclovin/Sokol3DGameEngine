#pragma once

#include "Model.h"
#include <string>

                  class ModelLoader {
public:
    ModelLoader() noexcept = default;
    ~ModelLoader() noexcept = default;

    // Loads a model using Assimp. Returns Model3D (caller must free vertices/indices with free()).
    Model3D LoadModel(const char *path);
};