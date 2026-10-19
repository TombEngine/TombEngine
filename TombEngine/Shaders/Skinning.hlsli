#ifndef TEN_SKINNING
#define TEN_SKINNING

// Bone blending of the frontend (Shaders/Math.hlsli), reading bones from a palette slice of the
// frame bone buffer. Classic mode blends rotations as quaternions and keeps the first bone translation.

#define SKINNING_NONE    0 // Instance world matrix only.
#define SKINNING_RIGID   1 // First bone index, weight ignored.
#define SKINNING_FULL    2 // Linear matrix blend.
#define SKINNING_CLASSIC 3 // Quaternion rotation blend.

#define BONE_WEIGHTS_MAX 4

float4 RotationMatrixToQuaternion(float3x3 m)
{
    float trace = m[0].x + m[1].y + m[2].z;

    bool tracePositive = trace > 0.0f;
    float s0 = sqrt(trace + 1.0f) * 2.0f;
    float4 q0 = float4((m[2].y - m[1].z) / s0, (m[0].z - m[2].x) / s0, (m[1].x - m[0].y) / s0, 0.25f * s0);

    bool cond1 = (m[0].x > m[1].y) && (m[0].x > m[2].z);
    float s1 = sqrt(1.0f + m[0].x - m[1].y - m[2].z) * 2.0f;
    float4 q1 = float4(0.25f * s1, (m[0].y + m[1].x) / s1, (m[0].z + m[2].x) / s1, (m[2].y - m[1].z) / s1);

    bool cond2 = m[1].y > m[2].z;
    float s2 = sqrt(1.0f + m[1].y - m[0].x - m[2].z) * 2.0f;
    float4 q2 = float4((m[0].y + m[1].x) / s2, 0.25f * s2, (m[1].z + m[2].y) / s2, (m[0].z - m[2].x) / s2);

    float s3 = sqrt(1.0f + m[2].z - m[0].x - m[1].y) * 2.0f;
    float4 q3 = float4((m[0].z + m[2].x) / s3, (m[1].z + m[2].y) / s3, 0.25f * s3, (m[1].x - m[0].y) / s3);

    float4 q = tracePositive ? q0 : (cond1 ? q1 : (cond2 ? q2 : q3));
    return normalize(q);
}

float3x3 QuaternionToRotationMatrix(float4 q)
{
    float x2 = q.x + q.x;
    float y2 = q.y + q.y;
    float z2 = q.z + q.z;
    float xx = q.x * x2;
    float yy = q.y * y2;
    float zz = q.z * z2;
    float xy = q.x * y2;
    float xz = q.x * z2;
    float yz = q.y * z2;
    float wx = q.w * x2;
    float wy = q.w * y2;
    float wz = q.w * z2;

    float3x3 m;
    m[0] = float3(1.0f - (yy + zz), xy - wz, xz + wy);
    m[1] = float3(xy + wz, 1.0f - (xx + zz), yz - wx);
    m[2] = float3(xz - wy, yz + wx, 1.0f - (xx + yy));
    return m;
}

// FXC ignores the row-major packing option for a matrix used directly as structured buffer element,
// so bones are wrapped in a struct.
struct BoneTransform
{
    float4x4 Transform;
};

float4x4 BlendBones(TEN_RAW_BUFFER_TYPE(BoneTransform) bones, uint boneOffset, uint4 boneIndices, float4 boneWeights, uint mode)
{
    if (mode == SKINNING_RIGID)
        return TEN_LOAD_RAW(BoneTransform, bones, boneOffset + boneIndices.x).Transform;

    if (mode == SKINNING_CLASSIC)
    {
        float4 blendedQuaternion = float4(0.0f, 0.0f, 0.0f, 0.0f);

        [unroll]
        for (int i = 0; i < BONE_WEIGHTS_MAX; i++)
        {
            float4 q = RotationMatrixToQuaternion((float3x3)TEN_LOAD_RAW(BoneTransform, bones, boneOffset + boneIndices[i]).Transform);
            q *= sign(dot(blendedQuaternion, q) + 1e-5f);
            blendedQuaternion += q * boneWeights[i];
        }

        float3x3 rotation = QuaternionToRotationMatrix(normalize(blendedQuaternion));
        return float4x4(
            float4(rotation[0], 0.0f),
            float4(rotation[1], 0.0f),
            float4(rotation[2], 0.0f),
            float4(TEN_LOAD_RAW(BoneTransform, bones, boneOffset + boneIndices.x).Transform[3].xyz, 1.0f));
    }

    float totalWeight = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
    if (totalWeight < 0.0001f)
        return TEN_LOAD_RAW(BoneTransform, bones, boneOffset + boneIndices.x).Transform;

    float4x4 result = (float4x4)0;

    [unroll]
    for (int i = 0; i < BONE_WEIGHTS_MAX; i++)
        result += TEN_LOAD_RAW(BoneTransform, bones, boneOffset + boneIndices[i]).Transform * (boneWeights[i] / totalWeight);

    result[0].w = 0.0f;
    result[1].w = 0.0f;
    result[2].w = 0.0f;
    result[3].w = 1.0f;
    return result;
}

#endif // TEN_SKINNING
