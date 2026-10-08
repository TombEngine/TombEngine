#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/ParticleGroup.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR4/Entity/Locust.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Renderer/Next/GpuTypes.h"
#include "Renderer/RenderView.h"
#include "Renderer/Structures/RendererEffect.h"
#include "Renderer/Structures/RendererItem.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/Structures/RendererObject.h"
#include "Renderer/Structures/RendererRoom.h"

using namespace TEN::Effects::ParticleGroups;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Next
{
    // Light slice of a room's light list, built once per frame and shared by every instance in the room.
    void NextRenderer::GetRoomLightSlice(int roomNumber, float interpolationFactor, int& lightOffset, int& lightCount)
    {
        if (roomNumber < 0 || roomNumber >= (int)_legacyGeometry.Rooms->size())
        {
            lightOffset = lightCount = 0;
            return;
        }

        if ((int)_roomLightSlices.size() != (int)_legacyGeometry.Rooms->size())
            _roomLightSlices.assign(_legacyGeometry.Rooms->size(), std::pair<int, int>(NO_VALUE, 0));

        auto& slice = _roomLightSlices[roomNumber];
        if (slice.first == NO_VALUE)
        {
            slice.second = AddLights((*_legacyGeometry.Rooms)[roomNumber].LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, slice.first);
            _roomLightSliceList.push_back(roomNumber);
        }

        lightOffset = slice.first;
        lightCount = slice.second;
    }

    void NextRenderer::QueueMeshInstance(const RendererMesh& mesh, const Matrix& world, const Vector4& color, const Vector4& ambientLight,
                                         LightMode lightMode, int lightOffset, int lightCount)
    {
        auto queued = QueuedMeshInstance{};
        queued.Mesh = &mesh;
        queued.Instance.World = world;
        queued.Instance.Color = color;
        queued.Instance.AmbientLight = ambientLight;
        queued.Instance.LightMode = (unsigned int)lightMode;
        queued.Instance.LightOffset = (unsigned int)lightOffset;
        queued.Instance.LightCount = (unsigned int)lightCount;
        _queuedMeshInstances.push_back(queued);
    }

    // Queued instances are grouped by mesh, so every mesh bucket becomes a single instanced draw.
    void NextRenderer::FlushMeshInstances()
    {
        std::sort(_queuedMeshInstances.begin(), _queuedMeshInstances.end(),
            [](const QueuedMeshInstance& first, const QueuedMeshInstance& second) { return first.Mesh < second.Mesh; });

        int index = 0;
        int count = (int)_queuedMeshInstances.size();
        while (index < count)
        {
            const auto* mesh = _queuedMeshInstances[index].Mesh;
            int firstInstance = (int)(_instanceData.size() / sizeof(GpuObjectInstance));

            int runEnd = index;
            while (runEnd < count && _queuedMeshInstances[runEnd].Mesh == mesh)
            {
                const auto* bytes = (const unsigned char*)&_queuedMeshInstances[runEnd].Instance;
                _instanceData.insert(_instanceData.end(), bytes, bytes + sizeof(GpuObjectInstance));
                runEnd++;
            }

            AddMeshDraws(*mesh, firstInstance, runEnd - index);
            index = runEnd;
        }

        _queuedMeshInstances.clear();
    }

    // Mesh effects, gun shells, creature swarms and particle group meshes, with the same transforms and
    // lighting as the legacy Draw* functions. Their sorted buckets are already in the legacy sorted list.
    void NextRenderer::CollectEffectMeshes(const RenderView& view, float interpolationFactor)
    {
        for (int roomNumber : _roomLightSliceList)
            _roomLightSlices[roomNumber].first = NO_VALUE;

        _roomLightSliceList.clear();

        const auto& rooms = *_legacyGeometry.Rooms;
        const auto& meshes = *_legacyGeometry.Meshes;
        int lightOffset = 0;
        int lightCount = 0;

        // Projectile and debris-like effects owned by rooms.
        for (const auto* room : view.RoomsToDraw)
        {
            for (const auto* effect : room->EffectsToDraw)
            {
                const auto& object = Objects[effect->ObjectID];
                if (object.Hidden || !object.loaded || effect->Mesh == nullptr)
                    continue;

                lightCount = AddLights(effect->LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, lightOffset);
                QueueMeshInstance(*effect->Mesh, effect->InterpolatedWorld, effect->Color, effect->AmbientLight, LightMode::Dynamic, lightOffset, lightCount);
            }
        }

        // Gun shells use the player's room ambient and light list.
        if (LaraItem != nullptr && _legacyGeometry.Items != nullptr)
        {
            const auto& player = (*_legacyGeometry.Items)[LaraItem->Index];
            const auto& playerRoom = rooms[LaraItem->RoomNumber];
            bool hasPlayerLights = false;

            for (const auto& shell : Gunshells)
            {
                if (shell.counter <= 0 || !(*_legacyGeometry.MoveableObjects)[shell.objectNumber].has_value())
                    continue;

                if (!hasPlayerLights)
                {
                    lightCount = AddLights(player.LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, lightOffset);
                    hasPlayerLights = true;
                }

                auto world = shell.pos.Orientation.ToRotationMatrix() * Matrix::CreateTranslation(shell.pos.Position.ToVector3());
                auto prevWorld = shell.PrevPose.Orientation.ToRotationMatrix() * Matrix::CreateTranslation(shell.PrevPose.Position.ToVector3());
                const auto& mesh = *(*_legacyGeometry.MoveableObjects)[shell.objectNumber]->ObjectMeshes[0];
                QueueMeshInstance(mesh, Matrix::Lerp(prevWorld, world, interpolationFactor), playerRoom.AmbientLight, playerRoom.AmbientLight,
                                  LightMode::Dynamic, lightOffset, lightCount);
            }
        }

        // Swarm creature of a room, drawn with neutral color and the room's ambient and lights.
        auto queueCreature = [&](const RendererMesh& mesh, const Matrix& world, int roomNumber, LightMode lightMode)
        {
            GetRoomLightSlice(roomNumber, interpolationFactor, lightOffset, lightCount);
            QueueMeshInstance(mesh, world, NEUTRAL_COLOR, rooms[roomNumber].AmbientLight, lightMode, lightOffset, lightCount);
        };

        if (Objects[ID_RATS_EMITTER].loaded)
        {
            auto lightMode = meshes[Objects[ID_RATS_EMITTER].meshIndex]->LightMode;
            for (int i = 0; i < NUM_RATS; i++)
            {
                if (Rats[i].On)
                {
                    const auto& mesh = *meshes[Objects[ID_RATS_EMITTER].meshIndex + (GlobalCounter + i) % Objects[ID_RATS_EMITTER].nmeshes];
                    queueCreature(mesh, Rats[i].Transform, Rats[i].RoomNumber, lightMode);
                }
            }
        }

        if (Objects[ID_BATS_EMITTER].loaded)
        {
            const auto& mesh = *meshes[Objects[ID_BATS_EMITTER].meshIndex + (GlobalCounter & 3)];
            for (const auto& bat : Bats)
            {
                if (bat.On)
                    queueCreature(mesh, Matrix::Lerp(bat.PrevTransform, bat.Transform, interpolationFactor), bat.RoomNumber, mesh.LightMode);
            }
        }

        if (Objects[ID_LITTLE_BEETLE].loaded)
        {
            const auto& mesh = *meshes[Objects[ID_LITTLE_BEETLE].meshIndex + ((Wibble >> 2) % 2)];
            for (const auto& beetle : TEN::Entities::TR4::BeetleSwarm)
            {
                if (beetle.On)
                    queueCreature(mesh, Matrix::Lerp(beetle.PrevTransform, beetle.Transform, interpolationFactor), beetle.RoomNumber, mesh.LightMode);
            }
        }

        if (Objects[ID_LOCUSTS].loaded)
        {
            auto lightMode = meshes[Objects[ID_LOCUSTS].meshIndex]->LightMode;
            for (const auto& locust : TEN::Entities::TR4::Locusts)
            {
                if (locust.On)
                {
                    const auto& mesh = *meshes[Objects[ID_LOCUSTS].meshIndex + (-locust.Counter & 3)];
                    queueCreature(mesh, Matrix::Lerp(locust.PrevTransform, locust.Transform, interpolationFactor), locust.RoomNumber, lightMode);
                }
            }
        }

        if (Objects[ID_FISH_EMITTER].loaded)
        {
            auto lightMode = meshes[Objects[ID_FISH_EMITTER].meshIndex]->LightMode;
            for (const auto& fish : FishSwarm)
            {
                if (fish.Life > 0.0f)
                {
                    const auto& mesh = *meshes[Objects[ID_FISH_EMITTER].meshIndex + fish.MeshIndex];
                    queueCreature(mesh, Matrix::Lerp(fish.PrevTransform, fish.Transform, interpolationFactor), fish.RoomNumber, lightMode);
                }
            }
        }

        // Particle group meshes keep their own color.
        for (const auto& group : ParticleGroupList)
        {
            if (!group.Active || !group.IsMeshGroup())
                continue;

            for (const auto& particle : group.Particles)
            {
                if (!particle.Active || !Objects[particle.ObjectID].loaded || Objects[particle.ObjectID].nmeshes <= 0 ||
                    Vector3::Distance(particle.Position, view.Camera.WorldPosition) > DEFAULT_RENDER_DISTANCE)
                {
                    continue;
                }

                int meshIndex = std::clamp(particle.SubIndex, 0, Objects[particle.ObjectID].nmeshes - 1);
                const auto& mesh = *meshes[Objects[particle.ObjectID].meshIndex + meshIndex];
                auto color = Vector4(particle.ParticleColor.R(), particle.ParticleColor.G(), particle.ParticleColor.B(), particle.ParticleColor.A());

                GetRoomLightSlice(particle.RoomNumber, interpolationFactor, lightOffset, lightCount);
                QueueMeshInstance(mesh, Matrix::Lerp(particle.PrevTransform, particle.Transform, interpolationFactor), color,
                                  rooms[particle.RoomNumber].AmbientLight, mesh.LightMode, lightOffset, lightCount);
            }
        }

        FlushMeshInstances();
    }
}
