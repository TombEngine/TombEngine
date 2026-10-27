#ifndef SAMPLERSSHADER
#define SAMPLERSSHADER

SamplerState PointWrapSampler : register(s1);
SamplerState LinearWrapSampler : register(s2);
SamplerState LinearClampSampler : register(s3);
SamplerState AnisotropicWrapSampler : register(s4);
SamplerState AnisotropicClampSampler : register(s5);
SamplerComparisonState ShadowMapSampler : register(s6);

#endif // SAMPLERSSHADER