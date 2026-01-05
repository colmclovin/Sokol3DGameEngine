#include "RaycastHelper.h"

namespace RaycastHelper {
    hmm_vec3 ScreenToWorldRay(float mouseX, float mouseY, int screenWidth, int screenHeight,
                              const hmm_mat4& viewMatrix, const hmm_mat4& projMatrix) {
        float ndcX = (2.0f * mouseX) / screenWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * mouseY) / screenHeight;
        
        hmm_vec3 right = HMM_Vec3(viewMatrix.Elements[0][0], viewMatrix.Elements[1][0], viewMatrix.Elements[2][0]);
        hmm_vec3 up = HMM_Vec3(viewMatrix.Elements[0][1], viewMatrix.Elements[1][1], viewMatrix.Elements[2][1]);
        hmm_vec3 forward = HMM_Vec3(-viewMatrix.Elements[0][2], -viewMatrix.Elements[1][2], -viewMatrix.Elements[2][2]);
        
        float fovY = 2.0f * HMM_ATANF(1.0f / projMatrix.Elements[1][1]);
        float aspectRatio = projMatrix.Elements[1][1] / projMatrix.Elements[0][0];
        
        float halfFovY = fovY * 0.5f;
        float halfFovX = HMM_ATANF(HMM_TANF(halfFovY) * aspectRatio);
        
        float tanHalfFovX = HMM_TANF(halfFovX);
        float tanHalfFovY = HMM_TANF(halfFovY);
        
        hmm_vec3 rayDir = HMM_AddVec3(
            HMM_AddVec3(
                forward,
                HMM_MultiplyVec3f(right, ndcX * tanHalfFovX)),
            HMM_MultiplyVec3f(up, ndcY * tanHalfFovY));
        
        return HMM_NormalizeVec3(rayDir);
    }
}