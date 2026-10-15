#ifndef TEN_ATMOSPHERIC_SKY_UNIFORMS
#define TEN_ATMOSPHERIC_SKY_UNIFORMS

// CBAtmosphericSky (Shaders/CBAtmosphericSky.hlsli) as pixel uniforms in slot 2; the frontend fills the block.

#include "./ShaderInterop.hlsli"

#define TEN_ATMOSPHERIC_SKY_BLOCK TEN_PS_UNIFORMS(AtmosphericSkyUniforms, 2)
#include "CBAtmosphericSky.hlsli"

#endif // TEN_ATMOSPHERIC_SKY_UNIFORMS
