#ifndef SHADER_LIGHT
#define SHADER_LIGHT

#include "CBCamera.glsl"
#include "Math.glsl"

float RoughnessToExpMul(float roughness)
{
    float r = clamp(roughness, 0.0, 1.0);
    r = max(r, 0.04);
    float gloss = 1.0 - r;
    return mix(0.04, 4.0, gloss * gloss);
}

vec3 DoSpecularPoint(vec3 pos, vec3 n, ShaderLight light, float strength, float specularIntensity, float roughness)
{
    float m = clamp(sign(strength), 0.0, 1.0);

    vec3 lightPos = light.Position.xyz;
    float radius = light.Out;

    float dist = distance(lightPos, pos);
    float attenuation = clamp((radius - dist) / max(radius, EPSILON), 0.0, 1.0);

    vec3 lightDir = normalize(lightPos - pos);
    vec3 reflectDir = reflect(lightDir, n);

    vec3 color = light.Color.xyz;
    float intenSpec = mix(clamp(specularIntensity, 0.0, 1.0), 1.0, m) * light.Intensity;

    float expBase = RoughnessToExpMul(roughness);
    float expMul = mix(expBase, 1.0, m);

    float strengthUsed = mix(1.0, strength, m);
    float expVal = max((strengthUsed * SPEC_FACTOR) * expMul, 1.0);

    float vr = clamp(dot(CamDirectionWS.xyz, reflectDir), 0.0, 1.0);
    float spec = pow(vr, max(expVal, 1.0));

    return attenuation * spec * color * intenSpec;
}

vec3 DoSpecularSun(vec3 n, ShaderLight light, float strength, float specularIntensity, float roughness)
{
    float m = clamp(sign(strength), 0.0, 1.0);

    vec3 lightDir = -normalize(light.Direction);
    vec3 reflectDir = reflect(lightDir, n);

    vec3 color = light.Color.xyz;
    float intenSpec = mix(clamp(specularIntensity, 0.0, 1.0), 1.0, m) * light.Intensity;

    float expBase = RoughnessToExpMul(roughness);
    float expMul = mix(expBase, 1.0, m);

    float strengthUsed = mix(1.0, strength, m);
    float expVal = max((strengthUsed * SPEC_FACTOR) * expMul, 1.0);

    float vr = clamp(dot(CamDirectionWS.xyz, reflectDir), 0.0, 1.0);
    float spec = pow(vr, max(expVal, 1.0));

    return spec * color * intenSpec;
}

vec3 DoSpecularSpot(vec3 pos, vec3 n, ShaderLight light, float strength, float specularIntensity, float roughness)
{
    float m = clamp(sign(strength), 0.0, 1.0);

    vec3 lightPos = light.Position.xyz;
    float radius = light.Out;

    float dist = distance(lightPos, pos);
    float attenuation = clamp((radius - dist) / max(radius, EPSILON), 0.0, 1.0);

    vec3 lightDir = normalize(lightPos - pos);
    vec3 reflectDir = reflect(lightDir, n);

    vec3 color = light.Color.xyz;
    float intenSpec = mix(clamp(specularIntensity, 0.0, 1.0), 1.0, m) * light.Intensity;

    float expBase = RoughnessToExpMul(roughness);
    float expMul = mix(expBase, 1.0, m);

    float strengthUsed = mix(1.0, strength, m);
    float expVal = max((strengthUsed * SPEC_FACTOR) * expMul, 1.0);

    float vr = clamp(dot(CamDirectionWS.xyz, reflectDir), 0.0, 1.0);
    float spec = pow(vr, max(expVal, 1.0));

    float cosine = dot(-lightDir, light.Direction.xyz);
    float angleAttenuation = clamp((cosine - light.OutRange) / (light.InRange - light.OutRange), 0.0, 1.0);
    float distanceAttenuation = clamp((light.Out - dist) / (light.Out - light.In), 0.0, 1.0);

    return angleAttenuation * distanceAttenuation * spec * color * intenSpec;
}

vec3 DoPointLight(vec3 pos, vec3 normal, ShaderLight light)
{
    vec3 lightVec = light.Position.xyz - pos;
    float dist = length(lightVec);
    vec3 lightDir = normalize(lightVec);

    float attenuation = clamp((light.Out - dist) / (light.Out - light.In), 0.0, 1.0);
    float d = clamp(dot(normal, lightDir), 0.0, 1.0);

    return clamp(light.Color.xyz * light.Intensity * attenuation * d, 0.0, 1.0);
}

vec3 DoShadowLight(vec3 pos, vec3 normal, ShaderLight light)
{
    vec3 lightVec = light.Position.xyz - pos;
    float dist = length(lightVec);
    vec3 lightDir = normalize(lightVec);

    float attenuation = clamp((light.Out - dist) / (light.Out - light.In), 0.0, 1.0);
    float d = clamp(dot(normal, lightDir), 0.0, 1.0);

    float absolute = light.Color.x * light.Intensity * attenuation;
    float directional = absolute * d;

    return clamp(vec3(absolute * 0.33 + directional * 0.66), 0.0, 1.0) * 2.0;
}

vec3 DoSpotLight(vec3 pos, vec3 normal, ShaderLight light)
{
    vec3 lightVec = pos - light.Position.xyz;
    float dist = length(lightVec);
    vec3 lightDir = normalize(lightVec);

    float cosine = dot(lightDir, light.Direction.xyz);
    float angleAttenuation = clamp((cosine - light.OutRange) / (light.InRange - light.OutRange), 0.0, 1.0);
    float distanceAttenuation = clamp((light.Out - dist) / (light.Out - light.In), 0.0, 1.0);

    float d = clamp(dot(normal, -lightDir), 0.0, 1.0);
    return clamp(light.Color.xyz * light.Intensity * angleAttenuation * distanceAttenuation * d, 0.0, 1.0);
}

void DoPointAndSpotLight(vec3 pos, vec3 normal, ShaderLight light, float specularIntensity, float roughness, out vec3 pointOutput, out vec3 spotOutput)
{
    vec3 lightVec = light.Position.xyz - pos;
    float dist = length(lightVec);
    vec3 lightDir = normalize(lightVec);

    float cosine = dot(-lightDir, light.Direction.xyz);
    float distanceAttenuation = clamp((light.Out - dist) / (light.Out - light.In), 0.0, 1.0);
    float angleAttenuation = clamp((cosine - light.OutRange) / (light.InRange - light.OutRange), 0.0, 1.0);

    float d = clamp(dot(normal, lightDir), 0.0, 1.0);
    pointOutput = clamp(light.Color.xyz * light.Intensity * distanceAttenuation * d, 0.0, 1.0);
    spotOutput  = clamp(light.Color.xyz * light.Intensity * angleAttenuation * distanceAttenuation * d, 0.0, 1.0);

    pointOutput += DoSpecularSpot(pos, normal, light, 0.0, specularIntensity, roughness);
    spotOutput += DoSpecularSpot(pos, normal, light, 0.0, specularIntensity, roughness);
}

vec3 DoDirectionalLight(vec3 pos, vec3 normal, ShaderLight light)
{
    float d = clamp(dot(-light.Direction.xyz, normal), 0.0, 1.0);
    return light.Color.xyz * light.Intensity * d;
}

float DoFogBulb(vec3 pos, ShaderFogBulb bulb)
{
    // We find the intersection points p0 and p1 between the sphere of the fog bulb and the ray from camera to vertex.
    // The magnitude of (p2 - p1) is used as the fog factor.
    // We need to consider different cases for getting the correct points.
    // We use the geometric solution as in legacy engines. An analytic solution also exists.

    vec3 p0;
    vec3 p1;

    p0 = p1 = vec3(0, 0, 0);

    vec3 bulbToVertex = pos - bulb.Position;
    float bulbToVertexSquaredDistance = pow(pos.x - bulb.Position.x, 2.0) + pow(pos.y - bulb.Position.y, 2.0) + pow(pos.z - bulb.Position.z, 2.0);
    vec3 cameraToVertexDirection = normalize(pos - CamPositionWS.xyz);
    float cameraToVertexSquaredDistance = pow(pos.x - CamPositionWS.x, 2.0) + pow(pos.y - CamPositionWS.y, 2.0) + pow(pos.z - CamPositionWS.z, 2.0);

    if (bulb.SquaredCameraToFogBulbDistance < bulb.SquaredRadius)
    {
        // Camera is INSIDE the bulb

        if (bulbToVertexSquaredDistance < bulb.SquaredRadius)
        {
            // Vertex is INSIDE the bulb

            p0 = CamPositionWS.xyz;
            p1 = pos;
        }
        else
        {
            // Vertex is OUTSIDE the bulb

            float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);
            float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
            float Thc = sqrt(bulb.SquaredRadius - d2);
            float t1 = Tca + Thc;

            p0 = CamPositionWS.xyz;
            p1 = CamPositionWS.xyz + cameraToVertexDirection * t1;
        }
    }
    else
    {
        // Camera is OUTSIDE the bulb

        if (bulbToVertexSquaredDistance < bulb.SquaredRadius)
        {
            // Vertex is INSIDE the bulb

            float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);
            float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
            float Thc = sqrt(bulb.SquaredRadius - d2);
            float t0 = Tca - Thc;

            p0 = CamPositionWS.xyz + cameraToVertexDirection * t0;
            p1 = pos;
        }
        else
        {
            // Vertex is OUTSIDE the bulb

            float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);

            if (Tca > 0.0 && cameraToVertexSquaredDistance > Tca * Tca)
            {
                float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
                if (d2 < bulb.SquaredRadius)
                {
                    float Thc = sqrt(bulb.SquaredRadius - d2);

                    float t0 = Tca - Thc;
                    float t1 = Tca + Thc;

                    p0 = CamPositionWS.xyz + cameraToVertexDirection * t0;
                    p1 = CamPositionWS.xyz + cameraToVertexDirection * t1;
                }
                else
                {
                    return 0.0;
                }
            }
            else
            {
                return 0.0;
            }
        }
    }

    float fog = length(p1 - p0) * bulb.Density / 255.0;

    return fog;
}

float DoFogBulbForSky(vec3 pos, ShaderFogBulb bulb)
{
    // We find the intersection points p0 and p1 between the sphere of the fog bulb and the ray from camera to vertex.
    // The magnitude of (p2 - p1) is used as the fog factor.
    // We need to consider different cases for getting the correct points.
    // We use the geometric solution as in legacy engines. An analytic solution also exists.

    vec3 p0;
    vec3 p1;

    p0 = p1 = vec3(0, 0, 0);

    vec3 cameraToVertexDirection = normalize(pos - CamPositionWS.xyz);

    if (bulb.SquaredCameraToFogBulbDistance < bulb.SquaredRadius)
    {
        // Camera is INSIDE the bulb

        // Vertex is ALWAYS OUTSIDE the bulb

        float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);
        float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
        float Thc = sqrt(bulb.SquaredRadius - d2);
        float t1 = Tca + Thc;

        p0 = CamPositionWS.xyz;
        p1 = CamPositionWS.xyz + cameraToVertexDirection * t1;
    }
    else
    {
        // Camera is OUTSIDE the bulb

        // Vertex is ALWAYS OUTSIDE the bulb

        float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);

        if (Tca > 0.0)
        {
            float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
            if (d2 < bulb.SquaredRadius)
            {
                float Thc = sqrt(bulb.SquaredRadius - d2);

                float t0 = Tca - Thc;
                float t1 = Tca + Thc;

                p0 = CamPositionWS.xyz + cameraToVertexDirection * t0;
                p1 = CamPositionWS.xyz + cameraToVertexDirection * t1;
            }
            else
            {
                return 0.0;
            }
        }
        else
        {
            return 0.0;
        }
    }

    float fog = length(p1 - p0) * bulb.Density / 255.0;

    return fog;
}

float DoDistanceFogForVertex(vec3 pos)
{
    float fog = 0.0;

    if (FogMaxDistance > 0.0)
    {
        float d = length(CamPositionWS.xyz - pos);
        fog = clamp((d - FogMinDistance * 1024.0) / (FogMaxDistance * 1024.0 - FogMinDistance * 1024.0), 0.0, 1.0);
    }

    return fog;
}

vec4 DoFogBulbsForVertex(vec3 pos)
{
    vec4 fog = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < NumFogBulbs; i++)
    {
        float fogFactor = DoFogBulb(pos, FogBulbs[i]);
        vec3 fogColor = FogBulbs[i].Color.xyz * fogFactor;

        fog.xyz += fogColor;
        fog.w += fogFactor;

        fog.w = clamp(fog.w, 0.0, 1.0);
    }

    return fog;
}

vec4 DoFogBulbsForSky(vec3 pos)
{
    vec4 fog = vec4(0.0, 0.0, 0.0, 0.0);

    for (int i = 0; i < NumFogBulbs; i++)
    {
        float fogFactor = DoFogBulbForSky(pos, FogBulbs[i]);
        vec3 fogColor = FogBulbs[i].Color.xyz * fogFactor;

        fog.xyz += fogColor;
        fog.w += fogFactor;

        fog.w = clamp(fog.w, 0.0, 1.0);
    }

    return fog;
}

vec3 CombineLights(vec3 ambient, vec3 vertex, vec3 tex, vec3 pos, vec3 normal, float sheen,
    const ShaderLight lights[MAX_LIGHTS_PER_ITEM], int numLights, float fogBulbsDensity, vec3 emissive, float specular, float roughness)
{
    vec3 diffuse = vec3(0);
    vec3 shadow  = vec3(0);
    vec3 spec    = vec3(0);

    int lightTypeMask = (numLights & ~LT_MASK);
    numLights = numLights & LT_MASK;

    for (int i = 0; i < numLights; i++)
    {
        if ((lightTypeMask & LT_MASK_SUN) != 0)
        {
            float isSun = step(0.5, float(lights[i].Type == uint(LT_SUN)));
            diffuse += isSun * DoDirectionalLight(pos, normal, lights[i]);
            spec += isSun * DoSpecularSun(normal, lights[i], sheen, specular, roughness);
        }

        if ((lightTypeMask & LT_MASK_POINT) != 0)
        {
            float isPoint = step(0.5, float(lights[i].Type == uint(LT_POINT)));
            diffuse += isPoint * DoPointLight(pos, normal, lights[i]);
            spec += isPoint * DoSpecularPoint(pos, normal, lights[i], sheen, specular, roughness);
        }

        if ((lightTypeMask & LT_MASK_SPOT) != 0)
        {
            float isSpot = step(0.5, float(lights[i].Type == uint(LT_SPOT)));
            diffuse += isSpot * DoSpotLight(pos, normal, lights[i]);
            spec += isSpot * DoSpecularSpot(pos, normal, lights[i], sheen, specular, roughness);
        }

        if ((lightTypeMask & LT_MASK_SHADOW) != 0)
        {
            float isShadow = step(0.5, float(lights[i].Type == uint(LT_SHADOW)));
            shadow += isShadow * DoShadowLight(pos, normal, lights[i]);
        }
    }

    shadow = clamp(shadow, 0.0, 1.0);
    diffuse *= tex;

    vec3 ambTex = (ambient - shadow) * tex;
    vec3 combined = ambTex + diffuse + spec + emissive;

    combined -= vec3(fogBulbsDensity, fogBulbsDensity, fogBulbsDensity);

    return clamp(combined * vertex, 0.0, 1.0);
}

vec3 StaticLight(vec3 vertex, vec3 tex, float fogBulbsDensity, vec3 emissive)
{
    vec3 result = tex * vertex + emissive;

    result -= vec3(fogBulbsDensity, fogBulbsDensity, fogBulbsDensity);

    return clamp(result, 0.0, 1.0);
}

#endif
