#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/GpuTypes.h"
#include "Renderer/RenderView.h"
#include "Renderer/Structures/RendererMirror.h"

using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Next
{
    constexpr auto LIGHT_COUNT_MASK = 0xFFFF;

    bool NextRenderer::IsMirrored(const InstanceOrigin& origin, const RendererMirror& mirror)
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
    int NextRenderer::AddMirroredInstance(int instanceIndex, const RendererMirror& mirror)
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

    // Legacy mirrors draw the player, moveables and statics of the mirror room a second time, reflected by the
    // mirror plane with inverted culling. Reflected copies of their instances form a separate draw list; buckets
    // sharing an instance range share its reflected range.
    void NextRenderer::CollectMirrors(const RenderView& view)
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
