#ifndef SHADER_LIGHT
#define SHADER_LIGHT

#include "./CBCamera.hlsli"
#include "./Math.hlsli"

float3 DoSpecularPoint(float3 pos, float3 n, ShaderLight light, float strength)
{
    if (strength <= 0.0)
		return float3(0, 0, 0);
	else
	{
		float3 lightPos = light.Position.xyz;
		float radius = light.Out;

		float dist = distance(lightPos, pos);
		if (dist > radius)
			return float3(0, 0, 0);
		else
		{
			float3 lightDir = normalize(lightPos - pos);
			float3 reflectDir = reflect(lightDir, n);

			float3 color = light.Color.xyz;
			float intensity = saturate(light.Intensity);
			float spec = pow(saturate(dot(CamDirectionWS.xyz, reflectDir)), strength * SPEC_FACTOR);
			float attenuation = (radius - dist) / radius;

			return attenuation * spec * color * intensity;
		}
	}
}

float3 DoSpecularSun(float3 n, ShaderLight light, float strength)
{
    if (strength <= 0.0)
		return float3(0, 0, 0);
	else
	{
		float3 lightDir = -normalize(light.Direction);
		float3 reflectDir = reflect(lightDir, n);

		float3 color = light.Color.xyz;
		float intensity = saturate(light.Intensity);
		float spec = pow(saturate(dot(CamDirectionWS.xyz, reflectDir)), strength * SPEC_FACTOR);

		return spec * color * intensity;
	}
}

float3 DoSpecularSpot(float3 pos, float3 n, ShaderLight light, float strength)
{
	if (strength <= 0.0)
		return float3(0, 0, 0);
	else
	{
		float3 lightPos = light.Position.xyz;
		float3 direction = light.Direction.xyz;
		float innerRange = light.In;
		float outerRange = light.Out;
		float coneIn = light.InRange;
		float coneOut = light.OutRange;

		float3 lightVec = pos - lightPos;
		float distance = length(lightVec);
		lightVec = normalize(lightVec);

		if (distance > outerRange)
			return float3(0, 0, 0);
		else
		{
			float cosine = dot(lightVec, direction);

			float minCosineIn = cos(coneIn * (PI / 180.0f));
			float attenuationIn = max((cosine - minCosineIn), 0.0f) / (1.0f - minCosineIn);

			float minCosineOut = cos(coneOut * (PI / 180.0f));
			float attenuationOut = max((cosine - minCosineOut), 0.0f) / (1.0f - minCosineOut);

			float attenuation = saturate(attenuationIn * 2.0f + attenuationOut);

			if (attenuation > 0.0f)
			{
				float3 lightDir = -lightVec;
				float3 reflectDir = reflect(lightDir, n);

				float3 color = light.Color.xyz;
				float intensity = saturate(light.Intensity);
				float spec = pow(saturate(dot(CamDirectionWS.xyz, reflectDir)), strength * SPEC_FACTOR);
				float falloff = saturate((outerRange - distance) / (outerRange - innerRange + 1.0f));

				return attenuation * spec * color * intensity * falloff;
			}
			else
				return float3(0, 0, 0);
		}
	}
}

float3 DoPointLight(float3 pos, float3 normal, ShaderLight light)
{
    float3 lightVec = light.Position.xyz - pos;
    float  distance = length(lightVec);
    float3 lightDir = normalize(lightVec);
    
    float attenuation = saturate((light.Out - distance) / (light.Out - light.In));
    float d = saturate(dot(normal, lightDir));
    
    return saturate(light.Color.xyz * light.Intensity * attenuation * d);
}

float3 DoShadowLight(float3 pos, float3 normal, ShaderLight light)
{
    float3 lightVec = light.Position.xyz - pos;
    float distance = length(lightVec);
    float3 lightDir = normalize(lightVec);
    
    float attenuation = saturate((light.Out - distance) / (light.Out - light.In));
    float d = saturate(dot(normal, lightDir));

    float absolute = light.Color.xyz * light.Intensity * attenuation;
    float directional = absolute * d;

    return saturate((absolute * 0.33f) + (directional * 0.66f)) * 2.0f;
}

float3 DoSpotLight(float3 pos, float3 normal, ShaderLight light)
{
    float3 lightVec = pos - light.Position.xyz;
    float  distance = length(lightVec);
    float3 lightDir = normalize(lightVec);

    float cosine = dot(lightDir, light.Direction.xyz);
    float angleAttenuation = saturate((cosine - light.OutRange) / (light.InRange - light.OutRange));
    float distanceAttenuation = saturate((light.Out - distance) / (light.Out - light.In));

    float d = saturate(dot(normal, -lightDir));
    return saturate(light.Color.xyz * light.Intensity * angleAttenuation * distanceAttenuation * d);
}

void DoPointAndSpotLight(float3 pos, float3 normal, ShaderLight light, out float3 pointOutput, out float3 spotOutput)
{
    float3 lightVec = light.Position.xyz - pos;
    float  distance = length(lightVec);
    float3 lightDir = normalize(lightVec);
	
    float cosine = dot(-lightDir, light.Direction.xyz);
    float distanceAttenuation = saturate((light.Out - distance) / (light.Out - light.In));
    float angleAttenuation = saturate((cosine - light.OutRange) / (light.InRange - light.OutRange));

    float d = saturate(dot(normal, lightDir));
    pointOutput = saturate(light.Color.xyz * light.Intensity * distanceAttenuation * d);
    spotOutput  = saturate(light.Color.xyz * light.Intensity * angleAttenuation * distanceAttenuation * d);
}

float3 DoDirectionalLight(float3 pos, float3 normal, ShaderLight light)
{
    float d = saturate(dot(-light.Direction.xyz, normal));
    return light.Color.xyz * light.Intensity * d;
}

float DoFogBulb(float3 pos, ShaderFogBulb bulb)
{
	// We find the intersection points p0 and p1 between the sphere of the fog bulb and the ray from camera to vertex.
	// The magnitude of (p2 - p1) is used as the fog factor.
	// We need to consider different cases for getting the correct points.
	// We use the geometric solution as in legacy engines. An analytic solution also exists.

	float3 p0;
	float3 p1;

	p0 = p1 = float3(0, 0, 0);

	float3 bulbToVertex = pos - bulb.Position;
	float bulbToVertexSquaredDistance = pow(pos.x - bulb.Position.x, 2) + pow(pos.y - bulb.Position.y, 2) + pow(pos.z - bulb.Position.z, 2);
	float3 cameraToVertexDirection = normalize(pos - CamPositionWS);
	float cameraToVertexSquaredDistance = pow(pos.x - CamPositionWS.x, 2) + pow(pos.y - CamPositionWS.y, 2) + pow(pos.z - CamPositionWS.z, 2);
		
	if (bulb.SquaredCameraToFogBulbDistance < bulb.SquaredRadius)
	{
		// Camera is INSIDE the bulb

		if (bulbToVertexSquaredDistance < bulb.SquaredRadius)
		{
			// Vertex is INSIDE the bulb

			p0 = CamPositionWS;
			p1 = pos;
		}
		else
		{
			// Vertex is OUTSIDE the bulb

			float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);
			float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
			float Thc = sqrt(bulb.SquaredRadius - d2);
			float t1 = Tca + Thc;

			p0 = CamPositionWS;
			p1 = CamPositionWS + cameraToVertexDirection * t1;
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

			p0 = CamPositionWS + cameraToVertexDirection * t0;
			p1 = pos;
		}
		else
		{
			// Vertex is OUTSIDE the bulb

			float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);

			if (Tca > 0 && cameraToVertexSquaredDistance > Tca * Tca)
			{
				float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
				if (d2 < bulb.SquaredRadius)
				{
					float Thc = sqrt(bulb.SquaredRadius - d2);

					float t0 = Tca - Thc;
					float t1 = Tca + Thc;

					p0 = CamPositionWS + cameraToVertexDirection * t0;
					p1 = CamPositionWS + cameraToVertexDirection * t1;
				}
				else
				{
					return 0;
				}
			}
			else
			{
				return 0;
			}
		}
	}

	float fog = length(p1 - p0) * bulb.Density / 255.0f;

	return fog;
}

float DoFogBulbForSky(float3 pos, ShaderFogBulb bulb)
{
	// We find the intersection points p0 and p1 between the sphere of the fog bulb and the ray from camera to vertex.
	// The magnitude of (p2 - p1) is used as the fog factor.
	// We need to consider different cases for getting the correct points.
	// We use the geometric solution as in legacy engines. An analytic solution also exists.

	float3 p0;
	float3 p1;

	p0 = p1 = float3(0, 0, 0);

	float3 cameraToVertexDirection = normalize(pos - CamPositionWS);
	
	if (bulb.SquaredCameraToFogBulbDistance < bulb.SquaredRadius)
	{
		// Camera is INSIDE the bulb

		// Vertex is ALWAYS OUTSIDE the bulb

		float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);
		float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
		float Thc = sqrt(bulb.SquaredRadius - d2);
		float t1 = Tca + Thc;

		p0 = CamPositionWS;
		p1 = CamPositionWS + cameraToVertexDirection * t1;
	}
	else
	{
		// Camera is OUTSIDE the bulb

		// Vertex is ALWAYS OUTSIDE the bulb

		float Tca = dot(bulb.FogBulbToCameraVector, cameraToVertexDirection);

		if (Tca > 0)
		{
			float d2 = bulb.SquaredCameraToFogBulbDistance - Tca * Tca;
			if (d2 < bulb.SquaredRadius)
			{
				float Thc = sqrt(bulb.SquaredRadius - d2);

				float t0 = Tca - Thc;
				float t1 = Tca + Thc;

				p0 = CamPositionWS + cameraToVertexDirection * t0;
				p1 = CamPositionWS + cameraToVertexDirection * t1;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}

	float fog = length(p1 - p0) * bulb.Density / 255.0f;

	return fog;
}

float DoDistanceFogForVertex(float3 pos)
{
	float fog = 0.0f;

	if (FogMaxDistance > 0.0f)
	{
		float d = length(CamPositionWS.xyz - pos);
		fog = clamp((d - FogMinDistance * 1024) / (FogMaxDistance * 1024 - FogMinDistance * 1024), 0, 1);
	}

	return fog;
}

float4 DoFogBulbsForVertex(float3 pos)
{
	float4 fog = float4(0.0f, 0.0f, 0.0f, 0.0f);

	for (int i = 0; i < NumFogBulbs; i++)
	{
		float fogFactor = DoFogBulb(pos, FogBulbs[i]);
		float3 fogColor = FogBulbs[i].Color.xyz * fogFactor;

		fog.xyz += fogColor;
		fog.w += fogFactor;

		fog.w = saturate(fog.w);
	}

	return fog;
}

float4 DoFogBulbsForSky(float3 pos)
{
	float4 fog = float4(0.0f, 0.0f, 0.0f, 0.0f);

	for (int i = 0; i < NumFogBulbs; i++)
	{
		float fogFactor = DoFogBulbForSky(pos, FogBulbs[i]);
		float3 fogColor = FogBulbs[i].Color.xyz * fogFactor;

		fog.xyz += fogColor;
		fog.w += fogFactor;

		fog.w = saturate(fog.w);
	}

	return fog;
}

float3 CombineLights(float3 ambient, float3 vertex, float3 tex, float3 pos, float3 normal, float sheen, 
	const ShaderLight lights[MAX_LIGHTS_PER_ITEM], int numLights, float fogBulbsDensity)
{
	float3 diffuse = 0;
	float3 shadow  = 0;
	float3 spec    = 0;

	int lightTypeMask = (numLights & ~LT_MASK);
	numLights = numLights & LT_MASK;
	
	for (int i = 0; i < numLights; i++)
	{
		if (lightTypeMask & LT_MASK_SUN)
		{
			float isSun = step(0.5f, float(lights[i].Type == LT_SUN));
			diffuse += isSun * DoDirectionalLight(pos, normal, lights[i]);
			spec    += isSun * DoSpecularSun(normal, lights[i], sheen);
		}

		if (lightTypeMask & LT_MASK_POINT)
		{
			float isPoint = step(0.5f, float(lights[i].Type == LT_POINT));
			diffuse += isPoint * DoPointLight(pos, normal, lights[i]);
			spec    += isPoint * DoSpecularPoint(pos, normal, lights[i], sheen);
		}

		if (lightTypeMask & LT_MASK_SPOT)
		{
			float isSpot = step(0.5f, float(lights[i].Type == LT_SPOT));
			diffuse += isSpot * DoSpotLight(pos, normal, lights[i]);
			spec    += isSpot * DoSpecularSpot(pos, normal, lights[i], sheen);
		}
		
		if (lightTypeMask & LT_MASK_SHADOW)
		{
			float isShadow = step(0.5f, float(lights[i].Type == LT_SHADOW));
			shadow += isShadow * DoShadowLight(pos, normal, lights[i]);
		}
	}

	shadow = saturate(shadow);
	diffuse *= tex;

	float3 ambTex = saturate(ambient - shadow) * tex;
	float3 combined = ambTex + diffuse + spec;

	combined -= float3(fogBulbsDensity, fogBulbsDensity, fogBulbsDensity);

	return saturate(combined * vertex);
}

float3 StaticLight(float3 vertex, float3 tex, float fogBulbsDensity)
{
	float3 result = tex * vertex;

	result -= float3(fogBulbsDensity, fogBulbsDensity, fogBulbsDensity);

	return saturate(result);
}

#endif