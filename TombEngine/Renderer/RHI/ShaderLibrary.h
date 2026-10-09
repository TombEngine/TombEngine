#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI
{
    // Loads a shader compiled offline by Shaders/RHI/CompileShaders.ps1. The bytecode format is chosen
    // from the device backend, the resource counts come from the .meta file written by the compiler.
    std::unique_ptr<IShader> LoadShader(IDevice& device, const std::string& binaryDirectory, const std::string& name, const std::string& entryPoint,
                                        const std::string& permutation = {});
}
