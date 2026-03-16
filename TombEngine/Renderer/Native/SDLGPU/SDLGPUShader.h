#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include "Renderer/Graphics/IShader.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	class SDLGPUShader final : public IShader
	{
	public:
		// Merged UBOs: SPIRV slot → list of engine CB registers to concatenate.
		struct MergedSlot { uint32_t spirvSlot; std::vector<uint32_t> engineRegs; };

	private:
		SDL_GPUShader* _vertexShader   = nullptr;
		SDL_GPUShader* _fragmentShader = nullptr;
		SDL_GPUDevice* _device         = nullptr;

		// Reflection metadata for pipeline creation.
		unsigned int _numVertexSamplers   = 0;
		unsigned int _numVertexUBOs       = 0;
		unsigned int _numVertexBuffers    = 0;
		unsigned int _numFragmentSamplers = 0;
		unsigned int _numFragmentUBOs     = 0;
		unsigned int _numFragmentBuffers  = 0;

		// HLSL register → SPIRV binding maps (for translating engine slots to GPU slots).
		std::map<uint32_t, uint32_t> _fsSamplerOldToNew;
		std::set<uint32_t> _fsArrayedBindings; // SPIRV bindings that expect Texture2DArray.
		std::set<uint32_t> _vsArrayedBindings; // VS SPIRV bindings that expect Texture2DArray.

		// UBO binding maps per stage.
		std::map<uint32_t, uint32_t> _vsUBOOldToNew;
		std::map<uint32_t, uint32_t> _fsUBOOldToNew;

		std::vector<MergedSlot> _vsMergedUBOs;
		std::vector<MergedSlot> _fsMergedUBOs;

	public:
		SDLGPUShader() = default;
		~SDLGPUShader();

		SDL_GPUShader* GetVertexShader() const { return _vertexShader; }
		SDL_GPUShader* GetFragmentShader() const { return _fragmentShader; }

		void SetVertexShader(SDL_GPUShader* shader) { _vertexShader = shader; }
		void SetFragmentShader(SDL_GPUShader* shader) { _fragmentShader = shader; }
		void SetDevice(SDL_GPUDevice* device) { _device = device; }

		unsigned int GetNumVertexSamplers() const { return _numVertexSamplers; }
		unsigned int GetNumVertexUBOs() const { return _numVertexUBOs; }
		unsigned int GetNumVertexBuffers() const { return _numVertexBuffers; }
		unsigned int GetNumFragmentSamplers() const { return _numFragmentSamplers; }
		unsigned int GetNumFragmentUBOs() const { return _numFragmentUBOs; }
		unsigned int GetNumFragmentBuffers() const { return _numFragmentBuffers; }

		// Map engine texture register → SPIRV binding. Returns -1 if not found.
		int MapFragmentSamplerSlot(int engineSlot) const
		{
			auto it = _fsSamplerOldToNew.find((uint32_t)engineSlot);
			return (it != _fsSamplerOldToNew.end()) ? (int)it->second : -1;
		}

		bool IsArrayedSamplerBinding(int spirvBinding) const
		{
			return _fsArrayedBindings.count((uint32_t)spirvBinding) > 0;
		}

		void SetVertexResourceCounts(unsigned int samplers, unsigned int ubos, unsigned int buffers)
		{
			_numVertexSamplers = samplers;
			_numVertexUBOs = ubos;
			_numVertexBuffers = buffers;
		}

		void SetFragmentResourceCounts(unsigned int samplers, unsigned int ubos, unsigned int buffers)
		{
			_numFragmentSamplers = samplers;
			_numFragmentUBOs = ubos;
			_numFragmentBuffers = buffers;
		}

		void SetFragmentSamplerMapping(std::map<uint32_t, uint32_t> oldToNew, std::set<uint32_t> arrayed)
		{
			_fsSamplerOldToNew = std::move(oldToNew);
			_fsArrayedBindings = std::move(arrayed);
		}

		void SetVertexUBOMapping(std::map<uint32_t, uint32_t> oldToNew)
		{
			_vsUBOOldToNew = std::move(oldToNew);
		}

		void SetVertexArrayedBindings(std::set<uint32_t> arrayed)
		{
			_vsArrayedBindings = std::move(arrayed);
		}

		bool IsVSArrayedSamplerBinding(int spirvBinding) const
		{
			return _vsArrayedBindings.count((uint32_t)spirvBinding) > 0;
		}

		void SetFragmentUBOMapping(std::map<uint32_t, uint32_t> oldToNew)
		{
			_fsUBOOldToNew = std::move(oldToNew);
		}

		// Map engine CB register → SPIRV UBO slot. Returns -1 if not found.
		int MapVertexUBOSlot(int engineSlot) const
		{
			auto it = _vsUBOOldToNew.find((uint32_t)engineSlot);
			return (it != _vsUBOOldToNew.end()) ? (int)it->second : -1;
		}

		int MapFragmentUBOSlot(int engineSlot) const
		{
			auto it = _fsUBOOldToNew.find((uint32_t)engineSlot);
			return (it != _fsUBOOldToNew.end()) ? (int)it->second : -1;
		}

		void SetVertexMergedUBOs(std::vector<MergedSlot> merged) { _vsMergedUBOs = std::move(merged); }
		void SetFragmentMergedUBOs(std::vector<MergedSlot> merged) { _fsMergedUBOs = std::move(merged); }
		const std::vector<MergedSlot>& GetVertexMergedUBOs() const { return _vsMergedUBOs; }
		const std::vector<MergedSlot>& GetFragmentMergedUBOs() const { return _fsMergedUBOs; }
	};
}

#endif
