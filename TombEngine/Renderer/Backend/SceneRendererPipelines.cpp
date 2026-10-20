#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/Structures/RendererObject.h"
#include "Renderer/Structures/RendererRoom.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

// Pipeline precompilation at level load. Scene pipelines are created lazily by GetScenePipeline; creating the ones a
// level can bind during LoadLevel avoids stalls at the first draw of a blend mode or pixel shader variant.

namespace TEN::Renderer::Backend
{
    // Blend modes precompiled for each shader of BLEND_MODE_SHADERS in PrecompileLevelPipelines.
    constexpr BlendMode PRECOMPILED_BLEND_MODES[] =
    {
        BlendMode::Opaque, BlendMode::Additive, BlendMode::NoDepthTest, BlendMode::Subtractive, BlendMode::Exclude, BlendMode::Screen,
        BlendMode::Lighten, BlendMode::AlphaBlend, BlendMode::FastAlphaBlend, BlendMode::PremultipliedAlphaBlend
    };

    // Pixel shader variants of the passes that draw a bucket. The prepass variant without normal maps is used while
    // ambient occlusion is off, which the settings can change at any time.
    void SceneRenderer::PrecompileBucketPipelines(const RendererBucket& bucket, const std::vector<TexturePage>& pages, SceneShader shader, SceneShader prepassShader)
    {
        int missingMaps = GetMissingMaps(bucket, pages);
        switch (bucket.BlendMode)
        {
        case BlendMode::Opaque:
        case BlendMode::AlphaTest:
            for (int isAlphaTested = 0; isAlphaTested < 2; isAlphaTested++)
            {
                // Bit 0 of the missing maps is the normal map (NO_NORMAL_MAP).
                GetScenePipeline(prepassShader, BlendMode::Opaque, isAlphaTested != 0, missingMaps);
                GetScenePipeline(prepassShader, BlendMode::Opaque, isAlphaTested != 0, missingMaps | 1);
                GetScenePipeline(shader, BlendMode::Opaque, isAlphaTested != 0, missingMaps);
            }

            if (bucket.BlendMode == BlendMode::AlphaTest)
                GetScenePipeline(shader, BlendMode::AlphaBlend, true, missingMaps, true);

            break;

        case BlendMode::Additive:
        case BlendMode::Distortion:
            GetScenePipeline(shader, bucket.BlendMode, false, missingMaps);
            break;

        default:
            break;
        }
    }

    // Base pipelines (all maps present) of every scene pass of a shader: prepass, opaque, alpha edges, additive, distortion.
    void SceneRenderer::PrecompilePassPipelines(SceneShader shader, SceneShader prepassShader)
    {
        for (int isAlphaTested = 0; isAlphaTested < 2; isAlphaTested++)
        {
            GetScenePipeline(prepassShader, BlendMode::Opaque, isAlphaTested != 0);
            GetScenePipeline(shader, BlendMode::Opaque, isAlphaTested != 0);
        }

        GetScenePipeline(shader, BlendMode::AlphaBlend, true, 0, true);
        GetScenePipeline(shader, BlendMode::Additive, false);
        GetScenePipeline(shader, BlendMode::Distortion, false);
    }

    // Bucket variants of every mesh of the objects, for the normal and the mirrored object shaders.
    void SceneRenderer::PrecompileObjectPipelines(const std::vector<std::optional<RendererObject>>& objects, const std::vector<TexturePage>& pages)
    {
        for (const auto& object : objects)
        {
            if (!object.has_value())
                continue;

            for (const auto* mesh : object->ObjectMeshes)
            {
                for (const auto& bucket : mesh->Buckets)
                {
                    PrecompileBucketPipelines(bucket, pages, SceneShader::Objects, SceneShader::ObjectsPrepass);
                    PrecompileBucketPipelines(bucket, pages, SceneShader::ObjectsMirrored, SceneShader::ObjectsPrepass);
                }
            }
        }
    }

    // Pipelines are created on first use, which stalls the frame of the first draw of an effect or texture group, so the
    // ones a level can bind are created at load: the variants of its buckets and every blend mode of the shaders that
    // draw with the full pixel shader, such as sorted geometry, sprites, debris, the sky and the inventory.
    void SceneRenderer::PrecompileLevelPipelines(const LevelGeometry& geometry)
    {
        constexpr SceneShader BLEND_MODE_SHADERS[] =
        {
            SceneShader::Rooms, SceneShader::Objects, SceneShader::ObjectsMirrored, SceneShader::ObjectsDebris, SceneShader::Sprites,
            SceneShader::SkySprites, SceneShader::Sky, SceneShader::SkyParaboloid, SceneShader::Inventory
        };
        constexpr SceneShader DISTORTION_SHADERS[] = { SceneShader::Rooms, SceneShader::Objects, SceneShader::ObjectsDebris, SceneShader::Sprites };

        auto startTime = std::chrono::steady_clock::now();
        int previousCount = GetScenePipelineCount();

        // Pass draws bind the variant with every map first.
        PrecompilePassPipelines(SceneShader::Rooms, SceneShader::RoomsPrepass);
        PrecompilePassPipelines(SceneShader::Objects, SceneShader::ObjectsPrepass);
        PrecompilePassPipelines(SceneShader::ObjectsMirrored, SceneShader::ObjectsPrepass);

        for (const auto& room : *geometry.Rooms)
        {
            for (const auto& bucket : room.Buckets)
                PrecompileBucketPipelines(bucket, _roomTextures, SceneShader::Rooms, SceneShader::RoomsPrepass);
        }

        PrecompileObjectPipelines(*geometry.StaticObjects, _staticTextures);
        PrecompileObjectPipelines(*geometry.MoveableObjects, _moveableTextures);

        for (auto shader : BLEND_MODE_SHADERS)
        {
            for (auto blendMode : PRECOMPILED_BLEND_MODES)
                GetScenePipeline(shader, blendMode);
        }

        for (auto shader : DISTORTION_SHADERS)
            GetScenePipeline(shader, BlendMode::Distortion);

        GetScenePipeline(SceneShader::ObjectsShadow, BlendMode::Opaque);
        GetScenePipeline(SceneShader::SkyHorizonMask, BlendMode::Opaque);

        float elapsed = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
        TENLog(fmt::format("Scene renderer: {} pipelines created at load in {:.1f} ms.", GetScenePipelineCount() - previousCount, elapsed), LogLevel::Info);
    }

    // Scene pipelines created so far, for the load log and the renderer info.
    int SceneRenderer::GetScenePipelineCount() const
    {
        int count = 0;
        for (const auto& shaderPipelines : _scenePipelines)
        {
            for (const auto& modePipelines : shaderPipelines)
            {
                for (const auto& pipeline : modePipelines)
                    count += (pipeline != nullptr) ? 1 : 0;
            }
        }

        return count;
    }
}
