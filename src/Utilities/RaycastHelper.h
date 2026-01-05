#pragma once
#include "../External/HandmadeMath.h"

namespace RaycastHelper {
    hmm_vec3 ScreenToWorldRay(float mouseX, float mouseY, int screenWidth, int screenHeight,
                              const hmm_mat4& viewMatrix, const hmm_mat4& projMatrix);
}