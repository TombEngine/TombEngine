#ifndef SHADOWSSHADER
#define SHADOWSSHADER

#include "Blending.glsl"
#include "Math.glsl"
#include "ShaderLight.glsl"

#define SHADOW_INTENSITY (0.6)
#define SHADOW_BLUR      (2.0)

#ifndef SPHERE_STRUCT_DEFINED
#define SPHERE_STRUCT_DEFINED
struct Sphere
{
    vec3 position;
    float radius;
};
#endif

#ifndef SHADOWLIGHT_CB_MERGED
layout(std140, binding = 3) uniform ShadowLightBuffer
{
    ShaderLight Light;
    mat4 LightViewProjections[6];
    int CastShadows;
    int NumSpheres;
    int ShadowMapSize;
    int ShadowPadding;
    Sphere Spheres[16];
};
#endif

layout(binding = 3) uniform sampler2DArrayShadow ShadowMap;

vec2 TexOffset(int u, int v)
{
    return vec2(float(u) * 1.0 / float(ShadowMapSize), float(v) * 1.0 / float(ShadowMapSize));
}

// https://gist.github.com/JuanDiegoMontoya/d8788148dcb9780848ce8bf50f89b7bb
int GetCubeFaceIndex(vec3 dir)
{
    float x = abs(dir.x);
    float y = abs(dir.y);
    float z = abs(dir.z);
    if (x > y && x > z)
        return 0 + (dir.x > 0.0 ? 0 : 1);
    else if (y > z)
        return 2 + (dir.y > 0.0 ? 0 : 1);
    return 4 + (dir.z > 0.0 ? 0 : 1);
}

vec2 GetCubeUVFromDir(int faceIndex, vec3 dir)
{
    vec2 uv;
    switch (faceIndex)
    {
    case 0:
        uv = vec2(-dir.z, dir.y);
        break; // +X
    case 1:
        uv = vec2(dir.z, dir.y);
        break; // -X
    case 2:
        uv = vec2(dir.x, dir.z);
        break; // +Y
    case 3:
        uv = vec2(dir.x, -dir.z);
        break; // -Y
    case 4:
        uv = vec2(dir.x, dir.y);
        break; // +Z
    default:
        uv = vec2(-dir.x, dir.y);
        break; // -Z
    }
    return uv * 0.5 + 0.5;
}

vec3 DoBlobShadows(vec3 worldPos, vec3 lighting)
{
    float shadowFactor = 1.0;

    for (int i = 0; i < NumSpheres; i++)
    {
        Sphere s = Spheres[i];
        float dist = distance(worldPos, s.position);
        float insideSphere = clamp(1.0 - step(s.radius, dist), 0.0, 1.0);
        float radiusFactor = dist / s.radius;
        float factor = (1.0 - clamp(radiusFactor, 0.0, 1.0)) * insideSphere;
        shadowFactor -= factor * shadowFactor;
    }

    shadowFactor = clamp(shadowFactor, 0.0, 1.0);
    return lighting * clamp(1.0 - (1.0 - shadowFactor) * (SHADOW_INTENSITY * 0.5), 0.0, 1.0);
}

vec3 DoShadow(vec3 worldPos, vec3 normal, vec3 lighting, float bias)
{
    if (CastShadows == 0)
        return lighting;

    if (BlendMode != uint(BLENDMODE_OPAQUE) && BlendMode != uint(BLENDMODE_ALPHATEST) && BlendMode != uint(BLENDMODE_ALPHABLEND))
        return lighting;

    float shadowFactor = 1.0;

    vec3 dir = normalize(Light.Position - worldPos);
    float ndot = dot(normal, dir);
    float facingFactor = clamp((ndot - bias) / (1.0 - bias + EPSILON), 0.0, 1.0);

    for (int i = 0; i < 6; i++)
    {
        vec4 lightClipSpace = LightViewProjections[i] * vec4(worldPos, 1.0);
        lightClipSpace.xyz /= lightClipSpace.w;

        float insideLightBounds =
            step(-1.0, lightClipSpace.x) * step(lightClipSpace.x, 1.0) *
            step(-1.0, lightClipSpace.y) * step(lightClipSpace.y, 1.0) *
            step( 0.0, lightClipSpace.z) * step(lightClipSpace.z, 1.0);

        if (insideLightBounds > 0.0)
        {
            lightClipSpace.x = lightClipSpace.x / 2.0 + 0.5;
            lightClipSpace.y = lightClipSpace.y / 2.0 + 0.5;

            float sum = 0.0;
            float samples = 0.0;

            // Perform basic PCF filtering
            for (float y = -SHADOW_BLUR; y <= SHADOW_BLUR; y += 1.0)
            {
                for (float x = -SHADOW_BLUR; x <= SHADOW_BLUR; x += 1.0)
                {
                    sum += texture(ShadowMap, vec4(lightClipSpace.xy + TexOffset(int(x), int(y)), float(i), lightClipSpace.z));
                    samples += 1.0;
                }
            }

            shadowFactor = mix(shadowFactor, sum / samples, facingFactor * insideLightBounds);
        }
    }

    float isPoint = step(0.5, float(Light.Type == uint(LT_POINT))); // 1.0 if LT_POINT, 0.0 otherwise
    float isSpot  = step(0.5, float(Light.Type == uint(LT_SPOT)));  // 1.0 if LT_SPOT,  0.0 otherwise
    float isOther = 1.0 - (isPoint + isSpot); // 1.0 if neither LT_POINT nor LT_SPOT

    float pointFactor = Luma(DoPointLight(worldPos, normal, Light));
    float spotFactor  = Luma(DoSpotLight(worldPos, normal, Light));

    vec3 pointShadow = lighting * clamp(1.0 - (1.0 - shadowFactor) * SHADOW_INTENSITY * pointFactor, 0.0, 1.0);
    vec3 spotShadow  = lighting * clamp(1.0 - (1.0 - shadowFactor) * SHADOW_INTENSITY * spotFactor, 0.0, 1.0);

    return pointShadow * isPoint + spotShadow * isSpot + lighting * isOther;
}

#endif // SHADOWSSHADER
