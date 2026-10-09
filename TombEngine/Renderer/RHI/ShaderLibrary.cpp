#include "framework.h"
#include "Renderer/RHI/ShaderLibrary.h"

namespace TEN::Renderer::RHI
{
    static bool ReadBinaryFile(const std::filesystem::path& path, std::vector<unsigned char>& data)
    {
        auto file = std::ifstream(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            return false;

        auto size = file.tellg();
        data.resize((int)size);
        file.seekg(0);
        file.read((char*)data.data(), size);
        return file.good();
    }

    static std::unordered_map<std::string, std::string> ReadMetadata(const std::filesystem::path& path)
    {
        auto result = std::unordered_map<std::string, std::string>{};

        auto file = std::ifstream(path);
        auto token = std::string();
        while (file >> token)
        {
            auto separator = token.find('=');
            if (separator != std::string::npos)
                result[token.substr(0, separator)] = token.substr(separator + 1);
        }

        return result;
    }

    static int GetMetadataInt(const std::unordered_map<std::string, std::string>& metadata, const char* key)
    {
        auto it = metadata.find(key);
        return (it != metadata.end()) ? std::stoi(it->second) : 0;
    }

    static ShaderStage ParseStage(const std::string& stage)
    {
        if (stage == "Pixel")
            return ShaderStage::Pixel;

        if (stage == "Compute")
            return ShaderStage::Compute;

        return ShaderStage::Vertex;
    }

    std::unique_ptr<IShader> LoadShader(IDevice& device, const std::string& binaryDirectory, const std::string& name, const std::string& entryPoint,
                                        const std::string& permutation)
    {
        auto baseName = name + "." + entryPoint + (permutation.empty() ? "" : "." + permutation);
        auto directory = std::filesystem::path(binaryDirectory);

        auto formatDirectory = std::string();
        auto extension = std::string();
        switch (device.GetShaderFormat())
        {
        case ShaderFormat::Dxbc:
            formatDirectory = "dxbc";
            extension = ".dxbc";
            break;

        case ShaderFormat::Spirv:
            formatDirectory = "spirv";
            extension = ".spv";
            break;

        case ShaderFormat::Dxil:
            formatDirectory = "dxil";
            extension = ".dxil";
            break;

        default:
            return nullptr;
        }

        auto code = std::vector<unsigned char>();
        auto codePath = directory / formatDirectory / (baseName + extension);
        if (!ReadBinaryFile(codePath, code))
        {
            TENLog("Shader binary not found: " + codePath.string(), LogLevel::Error);
            return nullptr;
        }

        auto metadataPath = directory / (baseName + ".meta");
        auto metadata = ReadMetadata(metadataPath);
        if (metadata.empty())
        {
            TENLog("Shader metadata not found: " + metadataPath.string(), LogLevel::Error);
            return nullptr;
        }

        auto desc = ShaderDesc{};
        desc.Stage = ParseStage(metadata["stage"]);
        desc.Code = code.data();
        desc.CodeSize = (int)code.size();
        desc.EntryPoint = entryPoint;
        desc.NumSamplers = GetMetadataInt(metadata, "samplers");
        desc.NumStorageTextures = GetMetadataInt(metadata, "storage_textures");
        desc.NumStorageBuffers = GetMetadataInt(metadata, "storage_buffers");
        desc.NumUniformBuffers = GetMetadataInt(metadata, "uniform_buffers");
        desc.NumRWStorageTextures = GetMetadataInt(metadata, "rw_storage_textures");
        desc.NumRWStorageBuffers = GetMetadataInt(metadata, "rw_storage_buffers");
        desc.Name = baseName;

        auto threads = metadata["threads"];
        if (!threads.empty())
            sscanf(threads.c_str(), "%d,%d,%d", &desc.ThreadCountX, &desc.ThreadCountY, &desc.ThreadCountZ);

        return device.CreateShader(desc);
    }
}
