#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/ParticleGroup.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
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
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"

using namespace TEN::Effects::ParticleGroups;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Renderer::RHI;
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

    // Additive, alpha tested buckets of a gun flash mesh, drawn after the sorted faces as in the legacy GunFlashes pass.
    void NextRenderer::AddGunFlash(GAME_OBJECT_ID flashObjectID, const Matrix& world, const Vector4& color, const Vector4& ambientLight)
    {
        const auto& flashObject = (*_legacyGeometry.MoveableObjects)[flashObjectID];
        if (!Objects[flashObjectID].loaded || !flashObject.has_value() || flashObject->ObjectMeshes.empty())
            return;

        int instance = AddInstance(world, color, ambientLight, LightMode::Static, 0, 0, 0, SKINNING_NONE);
        for (const auto& bucket : flashObject->ObjectMeshes[0]->Buckets)
        {
            if (bucket.BlendMode != BlendMode::Opaque && !bucket.Polygons.empty() && bucket.NumIndices > 0)
                _gunFlashDraws.push_back(ObjectDraw{ MeshSource::Moveables, &bucket, instance, 1 });
        }
    }

    // Player and creature muzzle flashes, with the transforms of the legacy DrawGunFlashes and DrawBaddyGunflashes.
    void NextRenderer::CollectGunFlashes(const RenderView& view)
    {
        _gunFlashDraws.clear();

        const auto& rooms = *_legacyGeometry.Rooms;

        if (LaraItem != nullptr && _legacyGeometry.Items != nullptr && Lara.Control.Look.OpticRange <= 0)
        {
            const auto& player = (*_legacyGeometry.Items)[LaraItem->Index];
            const auto& playerRoom = rooms[LaraItem->RoomNumber];

            auto addArmFlash = [&](const ArmInfo& arm, bool isLeft)
            {
                if (!arm.GunFlash || arm.GunFlashType == LaraWeaponType::None || arm.GunFlashType == LaraWeaponType::Flare)
                    return;

                const auto& settings = g_GameFlow->GetSettings()->Weapons[(int)arm.GunFlashType - 1];
                if (!settings.MuzzleFlash)
                    return;

                auto flashObjectID = (arm.GunFlashType == LaraWeaponType::HK && Objects[ID_GUN_FLASH2].loaded) ? ID_GUN_FLASH2 : ID_GUN_FLASH;
                if (!Objects[flashObjectID].loaded)
                    return;

                auto rootOffset = Objects[flashObjectID].Animations.front().Frames.front().RootPosition;
                auto offset = settings.MuzzleOffset + Vector3(rootOffset.x, rootOffset.z, rootOffset.y); // Bone orientation swaps axes.
                offset.x = isLeft ? -offset.x : offset.x;

                float zRotation = (flashObjectID == ID_GUN_FLASH2) ? 0.0f : TO_RAD((short)std::hash<int>()(GlobalCounter));
                auto xRotation = Matrix::CreateRotationX(TO_RAD(arm.GunFlashType == LaraWeaponType::Pistol ? ANGLE(268) : ANGLE(280)));
                auto world = player.InterpolatedAnimationTransforms[isLeft ? LM_LHAND : LM_RHAND] * player.InterpolatedWorld;
                world = Matrix::CreateRotationZ(zRotation) * xRotation * Matrix::CreateTranslation(offset) * world;

                AddGunFlash(flashObjectID, world, settings.ColorizeMuzzleFlash ? settings.FlashColor : NEUTRAL_COLOR, playerRoom.AmbientLight);
            };

            addArmFlash(Lara.LeftArm, true);
            addArmFlash(Lara.RightArm, false);
        }

        for (const auto* room : view.RoomsToDraw)
        {
            for (const auto* item : room->ItemsToDraw)
            {
                auto& nativeItem = g_Level.Items[item->ItemNumber];
                if (!nativeItem.IsCreature())
                    continue;

                const auto& creature = *GetCreatureInfo(&nativeItem);
                for (const auto& flash : creature.MuzzleFlash)
                {
                    if (flash.Delay == 0 || flash.Bite.BoneID == NO_VALUE)
                        continue;

                    auto flashObjectID = (flash.SwitchToMuzzle2 && (*_legacyGeometry.MoveableObjects)[ID_GUN_FLASH2].has_value()) ? ID_GUN_FLASH2 : ID_GUN_FLASH;
                    float zRotation = flash.SwitchToMuzzle2 ? 0.0f : TO_RAD((short)std::hash<int>()(GlobalCounter));

                    auto world = Matrix::CreateTranslation(flash.Bite.Position) * item->InterpolatedAnimationTransforms[flash.Bite.BoneID] * item->InterpolatedWorld;
                    if (flash.ApplyXRotation)
                        world = Matrix::CreateRotationX(TO_RAD(ANGLE(270.0f))) * world;

                    if (flash.ApplyZRotation)
                        world = Matrix::CreateRotationZ(zRotation) * world;

                    AddGunFlash(flashObjectID, world, CREATURE_GUNFLASH_COLOR, rooms[nativeItem.RoomNumber].AmbientLight);
                }
            }
        }
    }

    void NextRenderer::DrawGunFlashes(ICommandBuffer& commandBuffer, IRenderPass& renderPass)
    {
        if (_gunFlashDraws.empty())
            return;

        commandBuffer.PushDebugGroup("Gun flashes");
        renderPass.BindPipeline(GetScenePipeline(SceneShader::Objects, BlendMode::Additive));

        IBuffer* storageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 4);
        BindScenePixelStorage(renderPass);

        BufferBinding vertexBindings[2] = { { _moveableGeometry.Vertices.get(), 0 }, { _instanceIndexBuffer.get(), 0 } };
        renderPass.BindVertexBuffers(0, vertexBindings, 2);
        renderPass.BindIndexBuffer(BufferBinding{ _moveableGeometry.Indices.get(), 0 }, IndexSize::Bits32);

        auto state = BucketBindState{};
        auto drawInfo = BucketDrawInfo{};
        drawInfo.IsForcedAlphaTest = true;
        for (const auto& draw : _gunFlashDraws)
        {
            BindBucket(commandBuffer, renderPass, *draw.Bucket, _moveableTextures, BlendMode::Additive, 0, drawInfo, state);

            _frameStats.DrawCalls++;
            renderPass.DrawIndexed(draw.Bucket->NumIndices, 1, draw.Bucket->StartIndex, 0, draw.FirstInstance);
        }

        commandBuffer.PopDebugGroup();
    }
}
