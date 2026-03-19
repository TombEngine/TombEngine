#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Native::SDLGPU
{
	// Resource counts corrected after SPIRV remapping.
	struct SPIRVRemapResult
	{
		bool     success = false;
		uint32_t numSamplerSlots = 0;   // Combined image+sampler slots (set 0/2).
		uint32_t numUniformBuffers = 0; // UBO slots (set 1/3).
		uint32_t numStorageTextures = 0;
		uint32_t numStorageBuffers = 0;

		// Mapping from HLSL register number (old binding) → new contiguous SPIRV binding.
		// Used to translate engine TextureRegister slots to SPIRV binding indices.
		std::map<uint32_t, uint32_t> samplerOldToNewBinding;
		std::map<uint32_t, uint32_t> uboOldToNewBinding;

		// Per new-binding flag: true if the image at that binding is arrayed (Texture2DArray).
		std::set<uint32_t> arrayedSamplerBindings;
	};

	// --- SPIRV descriptor set and binding remapping for SDL_GPU ---
	//
	// DXC compiles HLSL register(tN/sN/bN) to SPIRV set=0 with auto-assigned bindings.
	// SDL_GPU's Vulkan backend expects resources split across 4 descriptor sets with
	// contiguous bindings starting from 0:
	//   VS: set 0 = samplers/textures/storage (bindings 0..N-1), set 1 = UBOs (bindings 0..M-1)
	//   FS: set 2 = samplers/textures/storage (bindings 0..N-1), set 3 = UBOs (bindings 0..M-1)
	//
	// Also fixes resource counts: SDL_ShaderCross reflection miscounts separate HLSL
	// images as storage textures. This function returns the correct counts.

	inline SPIRVRemapResult RemapSPIRVDescriptorSets(void* spirvData, size_t spirvSize,
	                                                  SDL_ShaderCross_ShaderStage stage)
	{
		SPIRVRemapResult result;
		if (!spirvData || spirvSize < 20)
			return result;

		uint32_t* words = static_cast<uint32_t*>(spirvData);
		size_t wordCount = spirvSize / sizeof(uint32_t);

		if (words[0] != 0x07230203)
			return result;

		constexpr uint32_t OpDecorate = 71;
		constexpr uint32_t OpVariable = 59;
		constexpr uint32_t DecorationDescriptorSet = 34;
		constexpr uint32_t DecorationBinding = 33;
		constexpr uint32_t StorageClassUniformConstant = 0;
		constexpr uint32_t StorageClassUniform = 2;
		constexpr uint32_t StorageClassStorageBuffer = 12;

		uint32_t samplerSet, uboSet;
		if (stage == SDL_SHADERCROSS_SHADERSTAGE_VERTEX)
		{
			samplerSet = 0;
			uboSet = 1;
		}
		else
		{
			samplerSet = 2;
			uboSet = 3;
		}

		// Pass 1: collect variable storage classes.
		std::unordered_map<uint32_t, uint32_t> varStorageClass;
		size_t i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;
			if (op == OpVariable && wc >= 4)
				varStorageClass[words[i + 2]] = words[i + 3];
			i += wc;
		}

		// Pass 2: determine target set for each variable and collect old bindings per new set.
		// We need to know: for each variable ID, what new set it belongs to and what its old binding is.
		struct VarInfo { uint32_t id; uint32_t oldBinding; uint32_t newSet; };

		std::unordered_map<uint32_t, uint32_t> varNewSet;    // id → new set
		std::unordered_map<uint32_t, uint32_t> varOldBinding; // id → old binding

		// First, determine new set for each variable with a DescriptorSet decoration.
		i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;
			if (op == OpDecorate && wc >= 4)
			{
				uint32_t target = words[i + 1];
				uint32_t decoration = words[i + 2];
				if (decoration == DecorationDescriptorSet)
				{
					auto it = varStorageClass.find(target);
					if (it != varStorageClass.end())
					{
						uint32_t sc = it->second;
						if (sc == StorageClassUniformConstant)
							varNewSet[target] = samplerSet;
						else if (sc == StorageClassUniform || sc == StorageClassStorageBuffer)
							varNewSet[target] = uboSet;
					}
				}
				else if (decoration == DecorationBinding)
				{
					varOldBinding[words[i + 1]] = words[i + 3];
				}
			}
			i += wc;
		}

		// Determine which resource variables are actually USED in the shader code
		// BEFORE building binding maps. DXC with -fspv-preserve-bindings keeps
		// unused variables (e.g. textures declared at file scope but only used by
		// the PS, not the VS). We must assign used variables contiguous bindings
		// first, then push unused variables beyond the used range.
		std::set<uint32_t> usedVarIds;
		{
			constexpr uint32_t OpLoad = 61;
			constexpr uint32_t OpAccessChain = 65;
			constexpr uint32_t OpInBoundsAccessChain = 66;

			std::set<uint32_t> resourceVarIds;
			for (auto& [id, _] : varNewSet)
				resourceVarIds.insert(id);

			i = 5;
			while (i < wordCount)
			{
				uint32_t wc = words[i] >> 16;
				uint32_t op = words[i] & 0xFFFF;
				if (wc == 0) break;

				if (op == OpLoad && wc >= 4)
				{
					if (resourceVarIds.count(words[i + 3]))
						usedVarIds.insert(words[i + 3]);
				}
				else if ((op == OpAccessChain || op == OpInBoundsAccessChain) && wc >= 4)
				{
					if (resourceVarIds.count(words[i + 3]))
						usedVarIds.insert(words[i + 3]);
				}

				i += wc;
			}
		}

		// Build new binding map per set: USED variables get contiguous bindings 0..N-1,
		// then UNUSED variables get bindings N, N+1, ... (beyond the used range so they
		// don't occupy descriptor slots SDL_GPU allocates). Variables that share an old
		// binding keep the same new binding (e.g. separate image + sampler pair).
		std::unordered_map<uint32_t, uint32_t> varNewBinding;
		std::map<uint32_t, uint32_t> samplerOldToNew; // Per-set old→new for samplers (used only).
		std::map<uint32_t, uint32_t> uboOldToNew;     // Per-set old→new for UBOs (used only).
		{
			std::map<uint32_t, std::vector<uint32_t>> setVariables;
			for (auto& [id, newSet] : varNewSet)
				setVariables[newSet].push_back(id);

			for (auto& [set, vars] : setVariables)
			{
				std::sort(vars.begin(), vars.end(), [&](uint32_t a, uint32_t b)
				{
					return varOldBinding[a] < varOldBinding[b];
				});

				// Phase 1: assign contiguous bindings 0..N-1 to USED variables.
				std::map<uint32_t, uint32_t> oldToNew;
				uint32_t nextBinding = 0;
				for (uint32_t id : vars)
				{
					if (!usedVarIds.count(id))
						continue;
					uint32_t oldB = varOldBinding[id];
					auto it = oldToNew.find(oldB);
					if (it == oldToNew.end())
					{
						oldToNew[oldB] = nextBinding;
						varNewBinding[id] = nextBinding++;
					}
					else
					{
						varNewBinding[id] = it->second;
					}
				}

				// Phase 2: assign bindings N, N+1, ... to UNUSED variables.
				// If an unused variable shares a binding with a used one (e.g.
				// separate sampler paired with a used image), give it the same binding.
				uint32_t unusedBinding = nextBinding;
				std::map<uint32_t, uint32_t> unusedOldToNew;
				for (uint32_t id : vars)
				{
					if (usedVarIds.count(id))
						continue;
					uint32_t oldB = varOldBinding[id];
					// Check if a used variable already claimed this binding.
					auto usedIt = oldToNew.find(oldB);
					if (usedIt != oldToNew.end())
					{
						varNewBinding[id] = usedIt->second;
						continue;
					}
					auto it = unusedOldToNew.find(oldB);
					if (it == unusedOldToNew.end())
					{
						unusedOldToNew[oldB] = unusedBinding;
						varNewBinding[id] = unusedBinding++;
					}
					else
					{
						varNewBinding[id] = it->second;
					}
				}

				if (set == samplerSet)
					samplerOldToNew = oldToNew;
				else if (set == uboSet)
					uboOldToNew = oldToNew;
			}
		}

		// Pass 2.5: detect arrayed images (Texture2DArray) for dummy texture selection.
		// OpVariable → OpTypePointer (resultType) → OpTypeImage/OpTypeSampledImage.
		// OpTypeImage has: OpTypeImage %resultType %sampledType Dim ... Arrayed ...
		//   word layout: [wc|op] [resultType] [sampledType] [Dim] [Depth] [Arrayed] ...
		std::set<uint32_t> arrayedBindings;
		{
			constexpr uint32_t OpTypeImage = 25;
			constexpr uint32_t OpTypePointer = 32;

			// Build type chain: typeID → {op, words...}
			std::unordered_map<uint32_t, uint32_t> pointerBaseType; // typePtr → baseType
			std::unordered_map<uint32_t, bool> imageTypeArrayed;     // typeID → arrayed

			i = 5;
			while (i < wordCount)
			{
				uint32_t wc = words[i] >> 16;
				uint32_t op = words[i] & 0xFFFF;
				if (wc == 0) break;

				if (op == OpTypePointer && wc >= 4)
					pointerBaseType[words[i + 1]] = words[i + 3]; // resultID → pointee type

				if (op == OpTypeImage && wc >= 9)
					imageTypeArrayed[words[i + 1]] = (words[i + 6] != 0); // Arrayed flag

				i += wc;
			}

			// For each sampler-set variable, check if its type chain leads to an arrayed image.
			for (auto& [id, newSet] : varNewSet)
			{
				if (newSet != samplerSet) continue;
				auto scIt = varStorageClass.find(id);
				if (scIt == varStorageClass.end() || scIt->second != StorageClassUniformConstant)
					continue;

				// OpVariable has resultType at words[i+1]. We need to find it.
				// Re-scan for this variable's result type.
				size_t j = 5;
				while (j < wordCount)
				{
					uint32_t wc2 = words[j] >> 16;
					uint32_t op2 = words[j] & 0xFFFF;
					if (wc2 == 0) break;
					if (op2 == OpVariable && wc2 >= 4 && words[j + 2] == id)
					{
						uint32_t ptrType = words[j + 1]; // OpTypePointer
						auto ptIt = pointerBaseType.find(ptrType);
						if (ptIt != pointerBaseType.end())
						{
							auto aiIt = imageTypeArrayed.find(ptIt->second);
							if (aiIt != imageTypeArrayed.end() && aiIt->second)
								arrayedBindings.insert(varNewBinding[id]);
						}
						break;
					}
					j += wc2;
				}
			}
		}

		// Pass 3: patch the SPIRV — update both Binding and DescriptorSet decorations.
		// DXC puts everything on set=0. SDL_GPU's Vulkan backend expects:
		//   VS: set 0 = samplers/textures, set 1 = UBOs
		//   FS: set 2 = samplers/textures, set 3 = UBOs
		// We patch DescriptorSet based on varNewSet and Binding based on varNewBinding.
		i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;
			if (op == OpDecorate && wc >= 4)
			{
				uint32_t target = words[i + 1];
				uint32_t decoration = words[i + 2];
				if (decoration == DecorationBinding)
				{
					auto it = varNewBinding.find(target);
					if (it != varNewBinding.end())
						words[i + 3] = it->second;
				}
				else if (decoration == DecorationDescriptorSet)
				{
					auto it = varNewSet.find(target);
					if (it != varNewSet.end())
						words[i + 3] = it->second;
				}
			}
			i += wc;
		}

		// Resource counts come directly from the used-only old→new maps.
		result.numSamplerSlots = (uint32_t)samplerOldToNew.size();
		result.numUniformBuffers = (uint32_t)uboOldToNew.size();
		result.numStorageTextures = 0;
		result.numStorageBuffers = 0;

		// Populate the old→new binding maps (used bindings only).
		for (auto& [oldB, newB] : samplerOldToNew)
			result.samplerOldToNewBinding[oldB] = newB;
		for (auto& [oldB, newB] : uboOldToNew)
			result.uboOldToNewBinding[oldB] = newB;
		result.arrayedSamplerBindings = arrayedBindings;

		result.success = true;
		return result;
	}

	// Patch SPIRV descriptor sets in-place for SDL_GPU's Vulkan backend.
	// DXC puts all resources in set 0. SDL_GPU expects:
	//   VS: set 0 = samplers, set 1 = UBOs/storage
	//   FS: set 2 = samplers, set 3 = UBOs/storage
	// Bindings within each set are left unchanged (already contiguous from HLSL_SDLGPU).
	inline bool PatchSPIRVDescriptorSets(void* spirvData, size_t spirvSize, bool isFragment)
	{
		if (!spirvData || spirvSize < 20) return false;

		uint32_t* words = static_cast<uint32_t*>(spirvData);
		size_t wordCount = spirvSize / sizeof(uint32_t);
		if (words[0] != 0x07230203) return false;

		constexpr uint32_t OpDecorate = 71;
		constexpr uint32_t OpVariable = 59;
		constexpr uint32_t DecorationDescriptorSet = 34;
		constexpr uint32_t StorageClassUniformConstant = 0; // images, samplers
		constexpr uint32_t StorageClassUniform = 2;         // UBOs
		constexpr uint32_t StorageClassStorageBuffer = 12;

		// First pass: collect storage class for each OpVariable result ID.
		std::unordered_map<uint32_t, uint32_t> varStorageClass;
		size_t i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;
			if (op == OpVariable && wc >= 4)
				varStorageClass[words[i + 2]] = words[i + 3];
			i += wc;
		}

		// Second pass: patch DescriptorSet decorations.
		uint32_t samplerSet = isFragment ? 2 : 0;
		uint32_t bufferSet  = isFragment ? 3 : 1;

		i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;

			if (op == OpDecorate && wc >= 4 && words[i + 2] == DecorationDescriptorSet)
			{
				uint32_t targetId = words[i + 1];
				auto scIt = varStorageClass.find(targetId);
				if (scIt != varStorageClass.end())
				{
					uint32_t sc = scIt->second;
					if (sc == StorageClassUniformConstant)
						words[i + 3] = samplerSet;
					else if (sc == StorageClassUniform || sc == StorageClassStorageBuffer)
						words[i + 3] = bufferSet;
				}
			}
			i += wc;
		}

		return true;
	}

	// Diagnostic: dump all SPIRV descriptor set/binding decorations.
	inline void DumpSPIRVDescriptors(const void* spirvData, size_t spirvSize, const std::string& label)
	{
		if (!spirvData || spirvSize < 20) return;
		const uint32_t* words = static_cast<const uint32_t*>(spirvData);
		size_t wordCount = spirvSize / sizeof(uint32_t);
		if (words[0] != 0x07230203) return;

		constexpr uint32_t OpDecorate = 71;
		constexpr uint32_t OpVariable = 59;
		constexpr uint32_t OpName = 5;
		constexpr uint32_t DecorationDescriptorSet = 34;
		constexpr uint32_t DecorationBinding = 33;

		std::unordered_map<uint32_t, uint32_t> varSet, varBinding, varStorageClass;
		std::unordered_map<uint32_t, std::string> varName;

		size_t i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;

			if (op == OpVariable && wc >= 4)
				varStorageClass[words[i + 2]] = words[i + 3];
			else if (op == OpDecorate && wc >= 4)
			{
				if (words[i + 2] == DecorationDescriptorSet)
					varSet[words[i + 1]] = words[i + 3];
				else if (words[i + 2] == DecorationBinding)
					varBinding[words[i + 1]] = words[i + 3];
			}
			else if (op == OpName && wc >= 3)
			{
				const char* nameStr = reinterpret_cast<const char*>(&words[i + 2]);
				size_t maxLen = (wc - 2) * 4;
				varName[words[i + 1]] = std::string(nameStr, strnlen(nameStr, maxLen));
			}

			i += wc;
		}

		std::string log = "SPIRV descriptors [" + label + "]:";
		for (auto& [id, sc] : varStorageClass)
		{
			auto sIt = varSet.find(id);
			auto bIt = varBinding.find(id);
			if (sIt == varSet.end() && bIt == varBinding.end()) continue;

			std::string name = varName.count(id) ? varName[id] : ("id" + std::to_string(id));
			std::string scName = (sc == 0) ? "UniformConst" : (sc == 2) ? "Uniform" : (sc == 12) ? "StorageBuf" : ("sc" + std::to_string(sc));

			log += " [" + name + " " + scName;
			if (sIt != varSet.end()) log += " set=" + std::to_string(sIt->second);
			if (bIt != varBinding.end()) log += " bind=" + std::to_string(bIt->second);
			log += "]";
		}
		TENLog(log, LogLevel::Info);
	}

	// Count SPIRV resources without patching. Returns resource counts as
	// max_binding + 1 per category, covering ALL declared variables (used or
	// not). This is critical because PatchSPIRVDescriptorSets moves ALL
	// variables to the correct descriptor sets, so the descriptor set layout
	// must have enough entries to cover every declared binding.
	struct SPIRVResourceCounts
	{
		uint32_t numSamplers = 0;
		uint32_t numUniformBuffers = 0;
		uint32_t numStorageTextures = 0;
		uint32_t numStorageBuffers = 0;
	};

	inline SPIRVResourceCounts CountSPIRVResources(const void* spirvData, size_t spirvSize)
	{
		SPIRVResourceCounts counts;
		if (!spirvData || spirvSize < 20) return counts;

		const uint32_t* words = static_cast<const uint32_t*>(spirvData);
		size_t wordCount = spirvSize / sizeof(uint32_t);
		if (words[0] != 0x07230203) return counts;

		constexpr uint32_t OpDecorate = 71;
		constexpr uint32_t OpVariable = 59;
		constexpr uint32_t DecorationBinding = 33;
		constexpr uint32_t DecorationDescriptorSet = 34;
		constexpr uint32_t StorageClassUniformConstant = 0;
		constexpr uint32_t StorageClassUniform = 2;
		constexpr uint32_t StorageClassStorageBuffer = 12;

		// Collect storage classes for all variables.
		std::unordered_map<uint32_t, uint32_t> varStorageClass;
		size_t i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;
			if (op == OpVariable && wc >= 4)
				varStorageClass[words[i + 2]] = words[i + 3];
			i += wc;
		}

		// Find ALL resource variables (those with a DescriptorSet decoration).
		std::set<uint32_t> resourceVarIds;
		i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;
			if (op == OpDecorate && wc >= 4 && words[i + 2] == DecorationDescriptorSet)
				resourceVarIds.insert(words[i + 1]);
			i += wc;
		}

		// Collect bindings for all resource vars.
		std::unordered_map<uint32_t, uint32_t> varBinding;
		i = 5;
		while (i < wordCount)
		{
			uint32_t wc = words[i] >> 16;
			uint32_t op = words[i] & 0xFFFF;
			if (wc == 0) break;
			if (op == OpDecorate && wc >= 4 && words[i + 2] == DecorationBinding)
				varBinding[words[i + 1]] = words[i + 3];
			i += wc;
		}

		// Track unique bindings per storage class category.
		std::set<uint32_t> samplerBindings;
		std::set<uint32_t> uboBindings;
		std::set<uint32_t> storageBufferBindings;

		for (uint32_t id : resourceVarIds)
		{
			auto scIt = varStorageClass.find(id);
			auto bIt = varBinding.find(id);
			if (scIt == varStorageClass.end() || bIt == varBinding.end()) continue;

			if (scIt->second == StorageClassUniformConstant)
				samplerBindings.insert(bIt->second);
			else if (scIt->second == StorageClassUniform)
				uboBindings.insert(bIt->second);
			else if (scIt->second == StorageClassStorageBuffer)
				storageBufferBindings.insert(bIt->second);
		}

		// max_binding + 1 gives the number of descriptor set entries needed.
		counts.numSamplers = samplerBindings.empty() ? 0 : (*samplerBindings.rbegin() + 1);
		counts.numUniformBuffers = uboBindings.empty() ? 0 : (*uboBindings.rbegin() + 1);
		counts.numStorageBuffers = storageBufferBindings.empty() ? 0 : (*storageBufferBindings.rbegin() + 1);
		counts.numStorageTextures = 0;
		return counts;
	}

	// --- Surface format conversion ---

	inline SDL_GPUTextureFormat GetSDLGPUTextureFormat(SurfaceFormat format)
	{
		switch (format)
		{
		case SurfaceFormat::SF_RGBA8_Unorm:      return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		case SurfaceFormat::SF_RGBA8_Unorm_Srgb: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
		case SurfaceFormat::SF_RG8_Unorm:        return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
		case SurfaceFormat::SF_R8_Unorm:         return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
		case SurfaceFormat::SF_R32_Float:        return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
		case SurfaceFormat::SF_RGBA32_Float:     return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
		case SurfaceFormat::SF_BGRA8_Unorm:      return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
		default:                                 return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		}
	}

	// --- Depth format conversion ---

	inline SDL_GPUTextureFormat GetSDLGPUDepthFormat(DepthFormat format)
	{
		switch (format)
		{
		case DepthFormat::Depth24Stencil8: return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
		case DepthFormat::Depth32:         return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
		default:                           return SDL_GPU_TEXTUREFORMAT_INVALID;
		}
	}

	// --- Vertex input format conversion ---

	inline SDL_GPUVertexElementFormat GetSDLGPUVertexFormat(VertexInputFormat format)
	{
		switch (format)
		{
		case VertexInputFormat::VI_RGBA8_Unorm: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
		case VertexInputFormat::VI_RGBA8_Snorm: return SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM;
		case VertexInputFormat::VI_RGBA8_Uint:  return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
		case VertexInputFormat::VI_RG8_Unorm:   return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM;
		case VertexInputFormat::VI_R32_Float:   return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
		case VertexInputFormat::VI_R32_Uint:    return SDL_GPU_VERTEXELEMENTFORMAT_UINT;
		case VertexInputFormat::VI_RG32_Float:  return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
		case VertexInputFormat::VI_RGB32_Float: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
		case VertexInputFormat::VI_RGBA32_Float:return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
		default:                                return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
		}
	}

	inline int GetVertexFormatByteSize(VertexInputFormat format)
	{
		switch (format)
		{
		case VertexInputFormat::VI_RGBA8_Unorm:
		case VertexInputFormat::VI_RGBA8_Snorm:
		case VertexInputFormat::VI_RGBA8_Uint:  return 4;
		case VertexInputFormat::VI_RG8_Unorm:   return 2;
		case VertexInputFormat::VI_R32_Float:
		case VertexInputFormat::VI_R32_Uint:    return 4;
		case VertexInputFormat::VI_RG32_Float:  return 8;
		case VertexInputFormat::VI_RGB32_Float: return 12;
		case VertexInputFormat::VI_RGBA32_Float:return 16;
		default:                                return 16;
		}
	}

	// --- Primitive type conversion ---

	inline SDL_GPUPrimitiveType GetSDLGPUPrimitiveType(PrimitiveType type)
	{
		switch (type)
		{
		case PrimitiveType::TriangleList:  return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		case PrimitiveType::TriangleStrip: return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
		case PrimitiveType::LineList:      return SDL_GPU_PRIMITIVETYPE_LINELIST;
		default:                           return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		}
	}

	// --- Blend state helpers ---

	inline SDL_GPUColorTargetBlendState GetSDLGPUBlendState(BlendMode mode)
	{
		SDL_GPUColorTargetBlendState state = {};
		state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
		                         SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

		switch (mode)
		{
		case BlendMode::Opaque:
		case BlendMode::AlphaTest:
			state.enable_blend = false;
			break;

		case BlendMode::Additive:
			state.enable_blend = true;
			state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.color_blend_op = SDL_GPU_BLENDOP_ADD;
			state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
			break;

		case BlendMode::Subtractive:
			state.enable_blend = true;
			state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
			state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.alpha_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
			break;

		case BlendMode::Screen:
			state.enable_blend = true;
			state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
			state.color_blend_op = SDL_GPU_BLENDOP_ADD;
			state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
			state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
			break;

		case BlendMode::Lighten:
			state.enable_blend = true;
			state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.color_blend_op = SDL_GPU_BLENDOP_MAX;
			state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.alpha_blend_op = SDL_GPU_BLENDOP_MAX;
			break;

		case BlendMode::Exclude:
			state.enable_blend = true;
			state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
			state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
			state.color_blend_op = SDL_GPU_BLENDOP_ADD;
			state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
			state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
			state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
			break;

		case BlendMode::AlphaBlend:
		case BlendMode::FastAlphaBlend:
		case BlendMode::NoDepthTest:
			state.enable_blend = true;
			state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
			state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
			state.color_blend_op = SDL_GPU_BLENDOP_ADD;
			state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
			state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
			state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
			break;

		default:
			state.enable_blend = false;
			break;
		}

		return state;
	}

	// --- Compute texture byte size ---

	inline size_t ComputeTextureSize(int width, int height, SDL_GPUTextureFormat format)
	{
		int bpp = 4; // Default bytes per pixel.
		switch (format)
		{
		case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
			bpp = 1; break;
		case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM:
			bpp = 2; break;
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
		case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
		case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
		case SDL_GPU_TEXTUREFORMAT_R32_FLOAT:
		case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
		case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
			bpp = 4; break;
		case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
			bpp = 16; break;
		default:
			bpp = 4; break;
		}
		return static_cast<size_t>(width) * height * bpp;
	}
}

#endif
