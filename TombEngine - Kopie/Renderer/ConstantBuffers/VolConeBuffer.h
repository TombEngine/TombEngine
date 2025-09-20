#pragma once
#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

namespace TEN::Renderer::ConstantBuffers
{
    struct CVolCone
    {
        Matrix ConeViewToLocal; // View -> ConeLocal
        Vector3 ConeColor; float Intensity;
        float ConeLen; float ConeR0; float ConeR1; float G;
        // 16-byte aligned already
    };
}
