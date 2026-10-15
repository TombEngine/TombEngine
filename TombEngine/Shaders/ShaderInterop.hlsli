#ifndef TEN_SHADER_INTEROP
#define TEN_SHADER_INTEROP

// Binding macros shared by FXC (DXBC for DX11) and DXC (SPIR-V for SDL GPU).
// Shader Model 5.0 features only. Slot rules follow SDL GPU:
// - Textures: slot N gets texture tN and sampler sN (combined image sampler on SPIR-V).
// - Storage textures and storage buffers continue the t range after the sampled textures,
//   so their binding is (number of sampled textures + index).
// - Uniform buffers: slot N is register bN, at most 4 per stage.
// - Vertex inputs: TEN_LOCATION(N) with semantic TEXCOORD<N>.

#ifdef __spirv__

#define TEN_LOCATION(n) [[vk::location(n)]]

#define TEN_VS_UNIFORMS(name, slot) [[vk::binding(slot, 1)]] cbuffer name
#define TEN_PS_UNIFORMS(name, slot) [[vk::binding(slot, 3)]] cbuffer name
#define TEN_CS_UNIFORMS(name, slot) [[vk::binding(slot, 2)]] cbuffer name

#define TEN_VS_TEXTURE(type, name, slot) \
    [[vk::combinedImageSampler]] [[vk::binding(slot, 0)]] type name; \
    [[vk::combinedImageSampler]] [[vk::binding(slot, 0)]] SamplerState name##Sampler

#define TEN_PS_TEXTURE(type, name, slot) \
    [[vk::combinedImageSampler]] [[vk::binding(slot, 2)]] type name; \
    [[vk::combinedImageSampler]] [[vk::binding(slot, 2)]] SamplerState name##Sampler

#define TEN_PS_COMPARISON_TEXTURE(type, name, slot) \
    [[vk::combinedImageSampler]] [[vk::binding(slot, 2)]] type name; \
    [[vk::combinedImageSampler]] [[vk::binding(slot, 2)]] SamplerComparisonState name##Sampler

#define TEN_CS_TEXTURE(type, name, slot) \
    [[vk::combinedImageSampler]] [[vk::binding(slot, 0)]] type name; \
    [[vk::combinedImageSampler]] [[vk::binding(slot, 0)]] SamplerState name##Sampler

#define TEN_VS_STORAGE_BUFFER(type, name, index) [[vk::binding(index, 0)]] StructuredBuffer<type> name
#define TEN_PS_STORAGE_BUFFER(type, name, index) [[vk::binding(index, 2)]] StructuredBuffer<type> name
#define TEN_CS_STORAGE_BUFFER(type, name, index) [[vk::binding(index, 0)]] StructuredBuffer<type> name

#define TEN_CS_RW_TEXTURE(type, name, index)        [[vk::binding(index, 1)]] type name
#define TEN_CS_RW_STORAGE_BUFFER(type, name, index) [[vk::binding(index, 1)]] RWStructuredBuffer<type> name

#else

#define TEN_LOCATION(n)

#define TEN_VS_UNIFORMS(name, slot) cbuffer name : register(b##slot)
#define TEN_PS_UNIFORMS(name, slot) cbuffer name : register(b##slot)
#define TEN_CS_UNIFORMS(name, slot) cbuffer name : register(b##slot)

#define TEN_VS_TEXTURE(type, name, slot) \
    type name : register(t##slot); \
    SamplerState name##Sampler : register(s##slot)

#define TEN_PS_TEXTURE(type, name, slot) \
    type name : register(t##slot); \
    SamplerState name##Sampler : register(s##slot)

#define TEN_PS_COMPARISON_TEXTURE(type, name, slot) \
    type name : register(t##slot); \
    SamplerComparisonState name##Sampler : register(s##slot)

#define TEN_CS_TEXTURE(type, name, slot) \
    type name : register(t##slot); \
    SamplerState name##Sampler : register(s##slot)

#define TEN_VS_STORAGE_BUFFER(type, name, index) StructuredBuffer<type> name : register(t##index)
#define TEN_PS_STORAGE_BUFFER(type, name, index) StructuredBuffer<type> name : register(t##index)
#define TEN_CS_STORAGE_BUFFER(type, name, index) StructuredBuffer<type> name : register(t##index)

#define TEN_CS_RW_TEXTURE(type, name, index)        type name : register(u##index)
#define TEN_CS_RW_STORAGE_BUFFER(type, name, index) RWStructuredBuffer<type> name : register(u##index)

#endif

#endif // TEN_SHADER_INTEROP
