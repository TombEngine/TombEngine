#ifndef TEN_SHADER_INTEROP
#define TEN_SHADER_INTEROP

// Binding macros shared by FXC (DXBC for DX11) and DXC (SPIR-V and DXIL for SDL GPU).
// Shader Model 5.0 features only. Slot rules follow SDL GPU:
// - Textures: slot N gets texture tN and sampler sN (combined image sampler on SPIR-V).
// - Storage textures and storage buffers continue the t range after the sampled textures,
//   so their binding is (number of sampled textures + index).
// - Uniform buffers: slot N is register bN, at most 4 per stage.
// - Vertex inputs: TEN_LOCATION(N) with semantic TEXCOORD<N>.
// - Raw buffers: storage buffers read with TEN_LOAD_RAW. SDL GPU creates raw views on Direct3D 12, where structured
//   loads with divergent indices or small elements read wrong data on AMD, so DXIL reads byte address buffers.
//   SPIR-V keeps structured buffers: its templated byte address loads change the matrix layout.

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
#define TEN_VS_RAW_BUFFER(type, name, index)     [[vk::binding(index, 0)]] StructuredBuffer<type> name
#define TEN_PS_RAW_BUFFER(type, name, index)     [[vk::binding(index, 2)]] StructuredBuffer<type> name
#define TEN_RAW_BUFFER_TYPE(type)                StructuredBuffer<type>
#define TEN_LOAD_RAW(type, name, index)          name[index]

#define TEN_CS_RW_TEXTURE(type, name, index)        [[vk::binding(index, 1)]] type name
#define TEN_CS_RW_STORAGE_BUFFER(type, name, index) [[vk::binding(index, 1)]] RWStructuredBuffer<type> name

#elif defined(TEN_DXIL)

// SDL GPU on Direct3D 12 uses the register spaces of the SPIR-V descriptor sets.
#define TEN_LOCATION(n)

#define TEN_VS_UNIFORMS(name, slot) cbuffer name : register(b##slot, space1)
#define TEN_PS_UNIFORMS(name, slot) cbuffer name : register(b##slot, space3)
#define TEN_CS_UNIFORMS(name, slot) cbuffer name : register(b##slot, space2)

#define TEN_VS_TEXTURE(type, name, slot) \
    type name : register(t##slot, space0); \
    SamplerState name##Sampler : register(s##slot, space0)

#define TEN_PS_TEXTURE(type, name, slot) \
    type name : register(t##slot, space2); \
    SamplerState name##Sampler : register(s##slot, space2)

#define TEN_PS_COMPARISON_TEXTURE(type, name, slot) \
    type name : register(t##slot, space2); \
    SamplerComparisonState name##Sampler : register(s##slot, space2)

#define TEN_CS_TEXTURE(type, name, slot) \
    type name : register(t##slot, space0); \
    SamplerState name##Sampler : register(s##slot, space0)

#define TEN_VS_STORAGE_BUFFER(type, name, index) StructuredBuffer<type> name : register(t##index, space0)
#define TEN_PS_STORAGE_BUFFER(type, name, index) StructuredBuffer<type> name : register(t##index, space2)
#define TEN_CS_STORAGE_BUFFER(type, name, index) StructuredBuffer<type> name : register(t##index, space0)
#define TEN_VS_RAW_BUFFER(type, name, index)     ByteAddressBuffer name : register(t##index, space0)
#define TEN_PS_RAW_BUFFER(type, name, index)     ByteAddressBuffer name : register(t##index, space2)
#define TEN_RAW_BUFFER_TYPE(type)                ByteAddressBuffer
#define TEN_LOAD_RAW(type, name, index)          name.Load<type>((index) * sizeof(type))

#define TEN_CS_RW_TEXTURE(type, name, index)        type name : register(u##index, space1)
#define TEN_CS_RW_STORAGE_BUFFER(type, name, index) RWStructuredBuffer<type> name : register(u##index, space1)

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
#define TEN_VS_RAW_BUFFER(type, name, index)     StructuredBuffer<type> name : register(t##index)
#define TEN_PS_RAW_BUFFER(type, name, index)     StructuredBuffer<type> name : register(t##index)
#define TEN_RAW_BUFFER_TYPE(type)                StructuredBuffer<type>
#define TEN_LOAD_RAW(type, name, index)          name[index]

#define TEN_CS_RW_TEXTURE(type, name, index)        type name : register(u##index)
#define TEN_CS_RW_STORAGE_BUFFER(type, name, index) RWStructuredBuffer<type> name : register(u##index)

#endif

#endif // TEN_SHADER_INTEROP
