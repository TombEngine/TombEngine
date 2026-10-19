#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Backend/GpuTypes.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/Structures/RendererMirror.h"

using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Backend
{
    bool SceneRenderer::IsMirrored(const InstanceOrigin& origin, const RendererMirror& mirror)
    {
        if (origin.RoomNumber != mirror.RoomNumber)
            return false;

        switch (origin.Category)
        {
        case InstanceCategory::Player:
            return mirror.ReflectPlayer;

        case InstanceCategory::Moveable:
            return mirror.ReflectMoveables;

        case InstanceCategory::Static:
            return mirror.ReflectStatics;

        default:
            return false;
        }
    }

    // Copy of an instance reflected by the mirror; its lights are reflected too when the mirror asks for it.
    int SceneRenderer::AddMirroredInstance(int instanceIndex, const RendererMirror& mirror)
    {
        auto instance = ((const GpuObjectInstance*)_instanceData.data())[instanceIndex];
        instance.World = instance.World * mirror.ReflectionMatrix;

        int lightCount = (int)(instance.LightCount & LIGHT_COUNT_MASK);
        if (mirror.ReflectLights && lightCount > 0)
        {
            int lightOffset = (int)(_lightData.size() / sizeof(GpuLight));
            for (int i = 0; i < lightCount; i++)
            {
                auto light = ((const GpuLight*)_lightData.data())[instance.LightOffset + i];
                light.Position = Vector3::Transform(light.Position, mirror.ReflectionMatrix);
                light.Direction = Vector3::TransformNormal(light.Direction, mirror.ReflectionMatrix);

                const auto* bytes = (const unsigned char*)&light;
                _lightData.insert(_lightData.end(), bytes, bytes + sizeof(light));
            }

            instance.LightOffset = (unsigned int)lightOffset;
        }

        int index = (int)(_instanceData.size() / sizeof(GpuObjectInstance));
        const auto* bytes = (const unsigned char*)&instance;
        _instanceData.insert(_instanceData.end(), bytes, bytes + sizeof(instance));
        _instanceOrigins.push_back(InstanceOrigin{});
        return index;
    }

    // Reflected copies of a run of sprites, or NO_VALUE when the frame sprite capacity is exhausted.
    int SceneRenderer::AddMirroredSprites(int firstSprite, int count, const RendererMirror& mirror, const RenderView& view)
    {
        int first = (int)(_spriteData.size() / sizeof(GpuSprite));
        if (first + count > INSTANCE_INDEX_COUNT)
            return NO_VALUE;

        for (int i = 0; i < count; i++)
            AddSprite(*_spriteSources[firstSprite + i], view, &mirror);

        return first;
    }

    // As in the frontend, unsorted sprites are reflected when the mirror asks for it, and the sorted faces
    // of every object and sprite in view are reflected unconditionally; sorted room faces are not.
    void SceneRenderer::CollectMirroredSorted(const RenderView& view)
    {
        _mirrorSpriteDraws.clear();
        _mirrorSortedDraws.clear();

        for (const auto& mirror : view.Mirrors)
        {
            if (mirror.ReflectSprites)
            {
                for (const auto& draw : _spriteDraws)
                {
                    int first = AddMirroredSprites(draw.FirstSprite, draw.SpriteCount, mirror, view);
                    if (first == NO_VALUE)
                        break;

                    auto mirrorDraw = draw;
                    mirrorDraw.FirstSprite = first;
                    _mirrorSpriteDraws.push_back(mirrorDraw);
                }
            }

            auto reflectedInstances = std::unordered_map<int, int>{};
            for (const auto& draw : _sortedDraws)
            {
                if (draw.IsRoom)
                    continue;

                auto mirrorDraw = draw;
                if (draw.IsSprite)
                {
                    mirrorDraw.Sprites.FirstSprite = AddMirroredSprites(draw.Sprites.FirstSprite, draw.Sprites.SpriteCount, mirror, view);
                    if (mirrorDraw.Sprites.FirstSprite == NO_VALUE)
                        continue;
                }
                else
                {
                    auto it = reflectedInstances.find(draw.Instance);
                    if (it == reflectedInstances.end())
                        it = reflectedInstances.emplace(draw.Instance, AddMirroredInstance(draw.Instance, mirror)).first;

                    mirrorDraw.Instance = it->second;
                }

                _mirrorSortedDraws.push_back(mirrorDraw);
            }
        }
    }

    // Mirrors draw the player, moveables and statics of the mirror room a second time, reflected by the
    // mirror plane with inverted culling. Reflected copies of their instances form a separate draw list; buckets
    // sharing an instance range share its reflected range.
    void SceneRenderer::CollectMirrors(const RenderView& view)
    {
        _mirrorDraws.clear();

        int drawCount = (int)_objectDraws.size();
        for (const auto& mirror : view.Mirrors)
        {
            auto reflectedRanges = std::unordered_map<int, std::pair<int, int>>{};
            for (int drawIndex = 0; drawIndex < drawCount; drawIndex++)
            {
                const auto draw = _objectDraws[drawIndex];

                auto it = reflectedRanges.find(draw.FirstInstance);
                if (it == reflectedRanges.end())
                {
                    int firstReflected = (int)(_instanceData.size() / sizeof(GpuObjectInstance));
                    int reflectedCount = 0;
                    for (int i = draw.FirstInstance; i < draw.FirstInstance + draw.InstanceCount; i++)
                    {
                        if (!IsMirrored(_instanceOrigins[i], mirror))
                            continue;

                        AddMirroredInstance(i, mirror);
                        reflectedCount++;
                    }

                    it = reflectedRanges.emplace(draw.FirstInstance, std::pair<int, int>(firstReflected, reflectedCount)).first;
                }

                if (it->second.second == 0)
                    continue;

                auto mirrorDraw = draw;
                mirrorDraw.FirstInstance = it->second.first;
                mirrorDraw.InstanceCount = it->second.second;
                _mirrorDraws.push_back(mirrorDraw);
            }
        }
    }
}
