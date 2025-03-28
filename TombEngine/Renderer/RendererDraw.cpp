#include "framework.h"

#include <algorithm>
#include <chrono>
#include <execution>
#include <filesystem>

#include "ConstantBuffers/CameraMatrixBuffer.h"
#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/effects/Hair.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Objects/Effects/tr4_locusts.h"
#include "Objects/Generic/Object/rope.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Renderer/RenderView.h"
#include "Renderer/Renderer.h"
#include "Renderer/Structures/RendererSortableObject.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/winmain.h"

using namespace TEN::Effects::Hair;
using namespace TEN::Hud;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Entities::Generic;
using namespace TEN::Renderer::Structures;

extern GUNSHELL_STRUCT Gunshells[MAX_GUNSHELL];

namespace TEN::Renderer
{
	void Renderer::RenderBlobShadows(RenderView& renderView)
	{
		auto nearestSpheres = std::vector<Sphere>{};
		nearestSpheres.reserve(g_Configuration.ShadowBlobsMax);

		// Collect player spheres.
		static const std::array<LARA_MESHES, 4> sphereMeshes = { LM_HIPS, LM_TORSO, LM_LFOOT, LM_RFOOT };
		static const std::array<float, 4> sphereScaleFactors = { 6.0f, 3.2f, 2.8f, 2.8f };

		for (auto& room : renderView.RoomsToDraw)
		{
			for (auto& i : room->ItemsToDraw)
			{
				auto& nativeItem = g_Level.Items[i->ItemNumber];

				// Skip everything that isn't alive or a vehicle.
				if (Objects[nativeItem.ObjectNumber].shadowType == ShadowMode::None)
					continue;

				if (nativeItem.IsLara())
				{
					for (auto i = 0; i < sphereMeshes.size(); i++)
					{
						if (!nativeItem.MeshBits.Test(sphereMeshes[i]))
							continue;

						auto& mesh = g_Level.Meshes[nativeItem.Model.MeshIndex[sphereMeshes[i]]];

						// Push foot spheres a little lower.
						auto offset = Vector3i(mesh.sphere.Center.x, mesh.sphere.Center.y, mesh.sphere.Center.z);
						if (sphereMeshes[i] == LM_LFOOT || sphereMeshes[i] == LM_RFOOT)
							offset.y += 8;

						auto& newSphere = nearestSpheres.emplace_back();
						newSphere.position = GetJointPosition(LaraItem, sphereMeshes[i], offset).ToVector3();;
						newSphere.radius = mesh.sphere.Radius * sphereScaleFactors[i];
					}
				}
				else
				{
					auto bounds = GameBoundingBox(&nativeItem);
					auto extents = bounds.GetExtents();
					auto center = Geometry::TranslatePoint(nativeItem.Pose.Position.ToVector3(), nativeItem.Pose.Orientation, bounds.GetCenter());
					center.y = nativeItem.Pose.Position.y;

					auto& newSphere = nearestSpheres.emplace_back();
					newSphere.position = center;
					newSphere.radius = std::max(abs(extents.x), abs(extents.z)) * 1.5f;
				}
			}
		}

		if (nearestSpheres.size() > g_Configuration.ShadowBlobsMax)
		{
			std::sort(nearestSpheres.begin(), nearestSpheres.end(), [](const Sphere& a, const Sphere& b)
				{
					auto& laraPos = LaraItem->Pose.Position;
					return Vector3::Distance(laraPos.ToVector3(), a.position) < Vector3::Distance(laraPos.ToVector3(), b.position);
				});

			std::copy(nearestSpheres.begin(), nearestSpheres.begin() + g_Configuration.ShadowBlobsMax, _stShadowMap.Spheres);
			_stShadowMap.NumSpheres = g_Configuration.ShadowBlobsMax;
		}
		else
		{
			std::copy(nearestSpheres.begin(), nearestSpheres.end(), _stShadowMap.Spheres);
			_stShadowMap.NumSpheres = (int)nearestSpheres.size();
		}
	}

	void Renderer::ClearShadowMap()
	{
		for (int step = 0; step < _shadowMap.RenderTargetView.size(); step++)
		{
			_context->ClearRenderTargetView(_shadowMap.RenderTargetView[step].Get(), Colors::White);
			_context->ClearDepthStencilView(_shadowMap.DepthStencilView[step].Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
				1.0f, 0);
		}
	}

	void Renderer::RenderShadowMap(RendererItem* item, RenderView& renderView)
	{
		// Doesn't cast shadow.
		if (_moveableObjects[item->ObjectID].value().ShadowType == ShadowMode::None)
			return;

		// Only render for player if such setting is active.
		if (g_Configuration.ShadowType == ShadowMode::Player && _moveableObjects[item->ObjectID].value().ShadowType != ShadowMode::Player)
			return;

		// No shadow light found.
		if (_shadowLight == nullptr)
			return;

		// Shadow light found but type is incorrect.
		if (_shadowLight->Type != LightType::Point && _shadowLight->Type != LightType::Spot)
			return;

		// Reset GPU state.
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		auto shadowLightPos = (_shadowLight->Hash == 0) ?
			_shadowLight->Position :
			Vector3::Lerp(_shadowLight->PrevPosition, _shadowLight->Position, GetInterpolationFactor());

		for (int step = 0; step < 6; step++)
		{
			// Bind render target.
			_context->OMSetRenderTargets(1, _shadowMap.RenderTargetView[step].GetAddressOf(),
				_shadowMap.DepthStencilView[step].Get());

			_context->RSSetViewports(1, &_shadowMapViewport);
			ResetScissor();

			if (shadowLightPos == item->Position)
				return;

			unsigned int stride = sizeof(Vertex);
			unsigned int offset = 0;

			// Set shaders.
			_shaders.Bind(Shader::ShadowMap);

			_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());
			_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			// Set texture.
			BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[0]), SamplerStateRegister::AnisotropicClamp);
			BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[0]), SamplerStateRegister::AnisotropicClamp);

			// Set camera matrices.
			auto view = Matrix::CreateLookAt(shadowLightPos, shadowLightPos +
				RenderTargetCube::forwardVectors[step] * BLOCK(10),
				RenderTargetCube::upVectors[step]);

			auto projection = Matrix::CreatePerspectiveFieldOfView(90.0f * PI / 180.0f, 1.0f, 16.0f, _shadowLight->Out);

			auto shadowProjection = CCameraMatrixBuffer{};
			shadowProjection.ViewProjection = view * projection;
			_cbCameraMatrices.UpdateData(shadowProjection, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Camera, _cbCameraMatrices.get());

			_stShadowMap.LightViewProjections[step] = (view * projection);

			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			auto& obj = GetRendererObject((GAME_OBJECT_ID)item->ObjectID);

			_stItem.World = item->InterpolatedWorld;
			_stItem.Color = item->Color;
			_stItem.AmbientLight = item->AmbientLight;
			memcpy(_stItem.BonesMatrices, item->InterpolatedAnimTransforms, sizeof(Matrix) * MAX_BONES);
			for (int k = 0; k < MAX_BONES; k++)
				_stItem.BoneLightModes[k] = (int)LightMode::Static;

			_cbItem.UpdateData(_stItem, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Item, _cbItem.get());
			BindConstantBufferPS(ConstantBufferRegister::Item, _cbItem.get());

			for (int k = 0; k < obj.ObjectMeshes.size(); k++)
			{
				if (item->MeshIds.size() <= k)
				{
					TENLog("Mesh structure was not properly initialized for object " + GetObjectName((GAME_OBJECT_ID)item->ObjectID));
					break;
				}

				auto* mesh = GetMesh(item->MeshIds[k]);

				for (auto& bucket : mesh->Buckets)
				{
					if (bucket.NumVertices == 0)
						continue;

					if (bucket.BlendMode != BlendMode::Opaque && bucket.BlendMode != BlendMode::AlphaTest)
						continue;

					// Draw vertices.
					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

					_numShadowMapDrawCalls++;
				}
			}

			if (item->ObjectID == ID_LARA)
			{
				auto& room = _rooms[item->RoomNumber];

				DrawLaraHolsters(item, &room, renderView, RendererPass::ShadowMap);
				DrawLaraJoints(item, &room, renderView, RendererPass::ShadowMap);
				DrawLaraHair(item, &room, renderView, RendererPass::ShadowMap);
			}
		}
	}

	void Renderer::DrawGunShells(RenderView& view, RendererPass rendererPass)
	{
		auto& room = _rooms[LaraItem->RoomNumber];
		auto* item = &_items[LaraItem->Index];

		int gunShellCount = 0;
		int objectID = 0;

		for (int i = 0; i < MAX_GUNSHELL; i++)
		{
			auto* gunshell = &Gunshells[i];

			if (gunshell->counter <= 0)
				continue;

			if (IgnoreReflectionPassForRoom(gunshell->roomNumber))
				continue;

			objectID = gunshell->objectNumber;

			auto translation = Matrix::CreateTranslation(gunshell->pos.Position.ToVector3());
			auto rotMatrix = gunshell->pos.Orientation.ToRotationMatrix();
			auto worldMatrix = rotMatrix * translation;

			auto prevTranslation = Matrix::CreateTranslation(
				gunshell->PrevPose.Position.x,
				gunshell->PrevPose.Position.y,
				gunshell->PrevPose.Position.z);
			auto prevRotMatrix = gunshell->PrevPose.Orientation.ToRotationMatrix();
			auto prevWorldMatrix = prevRotMatrix * prevTranslation;

			worldMatrix = Matrix::Lerp(prevWorldMatrix, worldMatrix, GetInterpolationFactor());
			ReflectMatrixOptionally(worldMatrix);

			_stInstancedStaticMeshBuffer.StaticMeshes[gunShellCount].World = worldMatrix;
			_stInstancedStaticMeshBuffer.StaticMeshes[gunShellCount].Ambient = room.AmbientLight;
			_stInstancedStaticMeshBuffer.StaticMeshes[gunShellCount].Color = room.AmbientLight;
			_stInstancedStaticMeshBuffer.StaticMeshes[gunShellCount].LightMode = (int)LightMode::Dynamic;
			BindInstancedStaticLights(item->LightsToDraw, gunShellCount);

			gunShellCount++;
		}

		if (gunShellCount > 0)
		{
			auto& moveableObject = *_moveableObjects[objectID];

			_shaders.Bind(Shader::InstancedStatics);

			unsigned int stride = sizeof(Vertex);
			unsigned int offset = 0;
			_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			SetBlendMode(BlendMode::Opaque);
			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			_cbInstancedStaticMeshBuffer.UpdateData(_stInstancedStaticMeshBuffer, _context.Get());

			const auto& mesh = *moveableObject.ObjectMeshes[0];
			for (const auto& bucket : mesh.Buckets)
			{
				if (bucket.NumVertices == 0)
					continue;

				BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
				BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

				DrawIndexedInstancedTriangles(bucket.NumIndices, gunShellCount, bucket.StartIndex, 0);

				_numInstancedStaticsDrawCalls++;
			}
		}
	}

	void Renderer::PrepareRopes(RenderView& view)
	{
		for (const auto& rope : Ropes)
		{
			if (!rope.active)
				continue;

			auto translationMatrix = Matrix::CreateTranslation(rope.position.ToVector3());
			auto absolutePoints = std::array<Vector3, 24>{};

			for (int i = 0; i < rope.segment.size(); i++)
			{
				const auto* segment = &rope.PrevMeshSegments[i];

				auto relPos = Vector3(segment->x >> FP_SHIFT, segment->y >> FP_SHIFT, segment->z >> FP_SHIFT);
				auto prevOutput = Vector3::Transform(relPos, translationMatrix);

				segment = &rope.meshSegment[i];

				relPos = Vector3(segment->x >> FP_SHIFT, segment->y >> FP_SHIFT, segment->z >> FP_SHIFT);
				auto currentOutput = Vector3::Transform(relPos, translationMatrix);

				auto absolutePos = Vector3::Lerp(prevOutput, currentOutput, GetInterpolationFactor());
				absolutePoints[i] = absolutePos;
			}

			for (int i = 0; i < (rope.segment.size() - 1); i++)
			{
				const auto& pos0 = absolutePoints[i];
				const auto& pos1 = absolutePoints[i + 1];
				auto pos = (pos0 + pos1) / 2;

				auto dir = pos1 - pos0;
				dir.Normalize();

				AddSpriteBillboardConstrained(
					&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_EMPTY1],
					pos, _rooms[rope.room].AmbientLight,
					PI_DIV_2, 1.0f, Vector2(32.0f, Vector3::Distance(pos0, pos1)), BlendMode::AlphaBlend, dir, false, view);
			}
		}
	}

	void Renderer::DrawLines2D()
	{
		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::None);

		_shaders.Bind(Shader::Solid);
		auto worldMatrix = Matrix::CreateOrthographicOffCenter(0, _screenWidth, _screenHeight, 0, _viewport.MinDepth, _viewport.MaxDepth);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		_primitiveBatch->Begin();

		for (const auto& line : _lines2DToDraw)
		{
			auto vertex0 = Vertex{};
			vertex0.Position.x = line.Origin.x;
			vertex0.Position.y = line.Origin.y;
			vertex0.Position.z = 1.0f;
			vertex0.Color = line.Color;

			auto vertex1 = Vertex{};
			vertex1.Position.x = line.Target.x;
			vertex1.Position.y = line.Target.y;
			vertex1.Position.z = 1.0f;
			vertex1.Color = line.Color;

			vertex0.Position = Vector3::Transform(vertex0.Position, worldMatrix);
			vertex1.Position = Vector3::Transform(vertex1.Position, worldMatrix);
			vertex0.Position.z = 0.5f;
			vertex1.Position.z = 0.5f;

			_numLinesDrawCalls++;
			_numDrawCalls++;
		}

		_primitiveBatch->End();

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
	}

	void Renderer::DrawSpiders(RenderView& view, RendererPass rendererPass)
	{
		/*XMMATRIX world;
		UINT cPasses = 1;

		if (Objects[ID_SPIDERS_EMITTER].loaded)
		{
			ObjectInfo* obj = &Objects[ID_SPIDERS_EMITTER];
			RendererObject* moveableObj = m_moveableObjects[ID_SPIDERS_EMITTER].get();
			short* meshPtr = Meshes[Objects[ID_SPIDERS_EMITTER].meshIndex + ((Wibble >> 2) & 2)];
			RendererMesh* mesh = m_meshPointersToMesh[meshPtr];
			RendererBucket* bucket = mesh->GetBucket(bucketIndex);

			if (bucket->NumVertices == 0)
				return true;

			setGpuStateForBucket(bucketIndex);

			m_device->SetStreamSource(0, bucket->GetVertexBuffer(), 0, sizeof(Vertex));
			m_device->SetIndices(bucket->GetIndexBuffer());

			LPD3DXEFFECT effect;
			if (pass == RENDERER_PASS_SHADOW_MAP)
				effect = m_shaderDepth->GetEffect();
			else if (pass == RENDERER_PASS_RECONSTRUCT_DEPTH)
				effect = m_shaderReconstructZBuffer->GetEffect();
			else if (pass == RENDERER_PASS_GBUFFER)
				effect = m_shaderFillGBuffer->GetEffect();
			else
				effect = m_shaderTransparent->GetEffect();

			effect->SetBool(effect->GetParameterByName(nullptr, "UseSkinning"), false);
			effect->SetInt(effect->GetParameterByName(nullptr, "ModelType"), MODEL_TYPE_MOVEABLE);

			if (bucketIndex == RENDERER_BUCKET_SOLID || bucketIndex == RENDERER_BUCKET_SOLID_DS)
				effect->SetInt(effect->GetParameterByName(nullptr, "BlendMode"), BlendMode::Opaque);
			else
				effect->SetInt(effect->GetParameterByName(nullptr, "BlendMode"), BlendMode::AlphaTest);

			for (int i = 0; i < NUM_SPIDERS; i++)
			{
				SPIDER_STRUCT* spider = &Spiders[i];

				if (spider->on)
				{
					XMMATRIXTranslation(&m_tempTranslation, spider->pos.xPos, spider->pos.yPos, spider->pos.zPos);
					XMMATRIXRotationYawPitchRoll(&m_tempRotation, spider->pos.yRot, spider->pos.xRot, spider->pos.zRot);
					XMMATRIXMultiply(&m_tempWorld, &m_tempRotation, &m_tempTranslation);
					effect->SetMatrix(effect->GetParameterByName(nullptr, "World"), &m_tempWorld);

					effect->SetVector(effect->GetParameterByName(nullptr, "AmbientLight"), &rooms[spider->roomNumber]->AmbientLight);

					for (int iPass = 0; iPass < cPasses; iPass++)
					{
						effect->BeginPass(iPass);
						effect->CommitChanges();

						drawPrimitives(D3DPT_TRIANGLELIST, 0, 0, bucket->NumVertices, 0, bucket->Indices.size() / 3);

						effect->EndPass();
					}
				}
			}
		}*/
	}

	void Renderer::DrawRats(RenderView& view, RendererPass rendererPass)
	{
		if (!Objects[ID_RATS_EMITTER].loaded)
			return;

		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			for (int i = 0; i < NUM_RATS; i++)
			{
				auto* rat = &Rats[i];

				if (IgnoreReflectionPassForRoom(rat->RoomNumber))
					continue;

				if (rat->On)
				{
					int index = (GlobalCounter + i) % Objects[ID_RATS_EMITTER].nmeshes;
					auto* mesh = GetMesh(Objects[ID_RATS_EMITTER].meshIndex + index);

					for (int j = 0; j < mesh->Buckets.size(); j++)
					{
						auto& bucket = mesh->Buckets[j];

						if (IsSortedBlendMode(bucket.BlendMode))
						{
							for (int p = 0; p < bucket.Polygons.size(); p++)
							{
								auto centre = Vector3::Transform(
									bucket.Polygons[p].Centre, rat->Transform);
								int distance = (centre - view.Camera.WorldPosition).Length();

								RendererSortableObject object;
								object.ObjectType = RendererObjectType::MoveableAsStatic;
								object.Centre = centre;
								object.Distance = distance;
								object.BlendMode = bucket.BlendMode;
								object.Bucket = &bucket;
								object.LightMode = mesh->LightMode;
								object.Polygon = &bucket.Polygons[p];
								object.World = rat->Transform;
								object.Room = &_rooms[rat->RoomNumber];

								view.TransparentObjectsToDraw.push_back(object);
							}
						}
					}
				}
			}
		}
		else
		{
			bool activeRatsExist = false;
			for (int i = 0; i < NUM_RATS; i++)
			{
				auto* rat = &Rats[i];

				if (IgnoreReflectionPassForRoom(rat->RoomNumber))
					continue;

				if (rat->On)
				{
					activeRatsExist = true;
					break;
				}
			}

			if (activeRatsExist)
			{
				if (rendererPass == RendererPass::GBuffer)
				{
					_shaders.Bind(Shader::GBuffer);
					_shaders.Bind(Shader::GBufferStatics);
				}
				else
				{
					_shaders.Bind(Shader::Statics);
				}

				UINT stride = sizeof(Vertex);
				UINT offset = 0;

				_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
				_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

				RendererObject& moveableObj = *_moveableObjects[ID_RATS_EMITTER];

				_stStatic.LightMode = (int)moveableObj.ObjectMeshes[0]->LightMode;

				for (int i = 0; i < NUM_RATS; i++)
				{
					auto* rat = &Rats[i];

					if (rat->On)
					{
						int index = (GlobalCounter + i) % Objects[ID_RATS_EMITTER].nmeshes;
						const auto& mesh = *GetMesh(Objects[ID_RATS_EMITTER].meshIndex + index);

						auto world = rat->Transform;
						ReflectMatrixOptionally(world);

						_stStatic.World = world;
						_stStatic.Color = Vector4::One;
						_stStatic.AmbientLight = _rooms[rat->RoomNumber].AmbientLight;

						if (rendererPass != RendererPass::GBuffer)
							BindStaticLights(_rooms[rat->RoomNumber].LightsToDraw);

						_cbStatic.UpdateData(_stStatic, _context.Get());

						for (const auto& bucket : mesh.Buckets)
						{
							if (bucket.NumVertices == 0)
								continue;

							int passes = rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;

							for (int p = 0; p < passes; p++)
							{
								if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
									continue;

								BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
								BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

								DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

								_numMoveablesDrawCalls++;
							}
						}
					}
				}
			}
		}
	}

	void Renderer::DrawFishSwarm(RenderView& view, RendererPass rendererPass)
	{
		if (!Objects[ID_FISH_EMITTER].loaded)
			return;

		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			for (const auto& fish : FishSwarm)
			{
				if (fish.Life <= 0.0f) 
					continue;

				auto& mesh = *GetMesh(Objects[ID_FISH_EMITTER].meshIndex + fish.MeshIndex);
				for (auto& bucket : mesh.Buckets)
				{
					if (!IsSortedBlendMode(bucket.BlendMode))
						continue;
						
					for (auto& poly : bucket.Polygons)
					{
						auto worldMatrix = Matrix::Lerp(fish.PrevTransform, fish.Transform, GetInterpolationFactor());
						auto center = Vector3::Transform(poly.Centre, worldMatrix);
						float dist = Vector3::Distance(center, view.Camera.WorldPosition);

						auto object = RendererSortableObject{};
						object.ObjectType = RendererObjectType::MoveableAsStatic;
						object.Centre = center;
						object.Distance = dist;
						object.BlendMode = bucket.BlendMode;
						object.Bucket = &bucket;
						object.LightMode = mesh.LightMode;
						object.Polygon = &poly;
						object.World = worldMatrix;
						object.Room = &_rooms[fish.RoomNumber];

						view.TransparentObjectsToDraw.push_back(object);
					}
				}
			}
		}
		else
		{
			bool doesActiveFishExist = false;
			for (const auto& fish : FishSwarm)
			{
				if (fish.Life > 0.0f)
				{
					doesActiveFishExist = true;
					break;
				}
			}

			if (doesActiveFishExist)
			{
				if (rendererPass == RendererPass::GBuffer)
				{
					_shaders.Bind(Shader::GBuffer);
					_shaders.Bind(Shader::GBufferStatics);
				}
				else
				{
					_shaders.Bind(Shader::Statics);
				}

				unsigned int stride = sizeof(Vertex);
				unsigned int offset = 0;

				_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
				_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

				const auto& moveableObj = *_moveableObjects[ID_FISH_EMITTER];

				_stStatic.LightMode = (int)moveableObj.ObjectMeshes[0]->LightMode;

				for (const auto& fish : FishSwarm)
				{
					if (fish.Life <= 0.0f)
						continue;

					const auto& mesh = *GetMesh(Objects[ID_FISH_EMITTER].meshIndex + fish.MeshIndex);

					_stStatic.World = Matrix::Lerp(fish.PrevTransform, fish.Transform, GetInterpolationFactor());
					_stStatic.Color = Vector4::One;
					_stStatic.AmbientLight = _rooms[fish.RoomNumber].AmbientLight;

					if (rendererPass != RendererPass::GBuffer)
						BindStaticLights(_rooms[fish.RoomNumber].LightsToDraw);

					_cbStatic.UpdateData(_stStatic, _context.Get());

					for (const auto& bucket : mesh.Buckets)
					{
						if (bucket.NumVertices == 0)
							continue;

						int passCount = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;
						for (int p = 0; p < passCount; p++)
						{
							if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
								continue;

							BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
							BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

							DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

							_numMoveablesDrawCalls++;
						}
					}
				}
			}
		}
	}

	void Renderer::DrawBats(RenderView& view, RendererPass rendererPass)
	{
		if (!Objects[ID_BATS_EMITTER].loaded)
			return;

		auto& mesh = *GetMesh(Objects[ID_BATS_EMITTER].meshIndex + (GlobalCounter & 3));

		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			for (const auto& bat : Bats)
			{
				if (!bat.On)
					continue;

				if (IgnoreReflectionPassForRoom(bat.RoomNumber))
					continue;

				for (auto& bucket : mesh.Buckets)
				{
					if (!IsSortedBlendMode(bucket.BlendMode))
						continue;

					for (int p = 0; p < bucket.Polygons.size(); p++)
					{
						auto transformMatrix = Matrix::Lerp(bat.PrevTransform, bat.Transform, GetInterpolationFactor());
						auto centre = Vector3::Transform(bucket.Polygons[p].Centre, transformMatrix);
						float dist = (centre - view.Camera.WorldPosition).Length();

						auto object = RendererSortableObject{};
						object.ObjectType = RendererObjectType::MoveableAsStatic;
						object.Centre = centre;
						object.Distance = dist;
						object.BlendMode = bucket.BlendMode;
						object.Bucket = &bucket;
						object.LightMode = mesh.LightMode;
						object.Polygon = &bucket.Polygons[p];
						object.World = transformMatrix;
						object.Room = &_rooms[bat.RoomNumber];

						view.TransparentObjectsToDraw.push_back(object);
					}
				}
			}
		}
		else
		{
			int batCount = 0;
			for (int i = 0; i < NUM_BATS; i++)
			{
				const auto& bat = Bats[i];

				if (IgnoreReflectionPassForRoom(bat.RoomNumber))
					continue;

				if (bat.On)
				{
					auto& room = _rooms[bat.RoomNumber];

					auto transformMatrix = Matrix::Lerp(bat.PrevTransform, bat.Transform, GetInterpolationFactor());

					auto world = transformMatrix;
					ReflectMatrixOptionally(world);

					_stInstancedStaticMeshBuffer.StaticMeshes[batCount].World = world;
					_stInstancedStaticMeshBuffer.StaticMeshes[batCount].Ambient = room.AmbientLight;
					_stInstancedStaticMeshBuffer.StaticMeshes[batCount].Color = Vector4::One;
					_stInstancedStaticMeshBuffer.StaticMeshes[batCount].LightMode = (int)mesh.LightMode;

					if (rendererPass != RendererPass::GBuffer)
						BindInstancedStaticLights(room.LightsToDraw, batCount);

					batCount++;
				}

				if (batCount == INSTANCED_STATIC_MESH_BUCKET_SIZE ||
					(i == (NUM_BATS - 1) && batCount > 0))
				{
					if (rendererPass == RendererPass::GBuffer)
					{
						_shaders.Bind(Shader::GBuffer);
						_shaders.Bind(Shader::GBufferInstancedStatics);
					}
					else
					{
						_shaders.Bind(Shader::InstancedStatics);
					}

					unsigned int stride = sizeof(Vertex);
					unsigned int offset = 0;

					_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
					_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

					_cbInstancedStaticMeshBuffer.UpdateData(_stInstancedStaticMeshBuffer, _context.Get());

					for (auto& bucket : mesh.Buckets)
					{
						if (bucket.NumVertices == 0)
							continue;

						int passCount = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;
						for (int p = 0; p < passCount; p++)
						{
							if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
								continue;

							BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
							BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

							DrawIndexedInstancedTriangles(bucket.NumIndices, batCount, bucket.StartIndex, 0);

							_numMoveablesDrawCalls++;
						}
					}

					batCount = 0;
				}
			}
		}
	}

	void Renderer::DrawScarabs(RenderView& view, RendererPass rendererPass)
	{
		if (!Objects[ID_LITTLE_BEETLE].loaded)
			return;

		auto& mesh = *GetMesh(Objects[ID_LITTLE_BEETLE].meshIndex + ((Wibble >> 2) % 2));

		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			for (const auto& beetle : TEN::Entities::TR4::BeetleSwarm)
			{
				if (!beetle.On)
					continue;

				if (IgnoreReflectionPassForRoom(beetle.RoomNumber))
					continue;

				auto transformMatrix = Matrix::Lerp(beetle.PrevTransform, beetle.Transform, GetInterpolationFactor());

				for (auto& bucket : mesh.Buckets)
				{
					if (!IsSortedBlendMode(bucket.BlendMode))
						continue;

					for (int p = 0; p < bucket.Polygons.size(); p++)
					{
						auto centre = Vector3::Transform(bucket.Polygons[p].Centre, transformMatrix);
						float dist = (centre - view.Camera.WorldPosition).Length();

						auto object = RendererSortableObject{};
						object.ObjectType = RendererObjectType::MoveableAsStatic;
						object.Centre = centre;
						object.Distance = dist;
						object.BlendMode = bucket.BlendMode;
						object.Bucket = &bucket;
						object.LightMode = mesh.LightMode;
						object.Polygon = &bucket.Polygons[p];
						object.World = transformMatrix;
						object.Room = &_rooms[beetle.RoomNumber];

						view.TransparentObjectsToDraw.push_back(object);
					}
				}
			}
		}
		else
		{	
			int beetleCount = 0;
			for (int i = 0; i < TEN::Entities::TR4::NUM_BEETLES; i++)
			{
				const auto& beetle = TEN::Entities::TR4::BeetleSwarm[i];
				
				if (!beetle.On)
					continue;

				if (IgnoreReflectionPassForRoom(beetle.RoomNumber))
					continue;

				auto& room = _rooms[beetle.RoomNumber];

				auto transformMatrix = Matrix::Lerp(beetle.PrevTransform, beetle.Transform, GetInterpolationFactor());

				auto world = transformMatrix;
				ReflectMatrixOptionally(world);

				_stInstancedStaticMeshBuffer.StaticMeshes[beetleCount].World = world;
				_stInstancedStaticMeshBuffer.StaticMeshes[beetleCount].Ambient = room.AmbientLight;
				_stInstancedStaticMeshBuffer.StaticMeshes[beetleCount].Color = Vector4::One;
				_stInstancedStaticMeshBuffer.StaticMeshes[beetleCount].LightMode = (int)mesh.LightMode;

				if (rendererPass != RendererPass::GBuffer)
				{
					auto lights = std::vector<RendererLight*>{};
					for (int i = 0; i < std::min((int)room.LightsToDraw.size(), MAX_LIGHTS_PER_ITEM); i++)
						lights.push_back(room.LightsToDraw[i]);
						
					BindInstancedStaticLights(lights, beetleCount);
				}

				beetleCount++;

				if (beetleCount == INSTANCED_STATIC_MESH_BUCKET_SIZE || 
					(i == TEN::Entities::TR4::NUM_BEETLES - 1 && beetleCount > 0))
				{
					if (rendererPass == RendererPass::GBuffer)
					{
						_shaders.Bind(Shader::GBuffer);
						_shaders.Bind(Shader::GBufferInstancedStatics);
					}
					else
					{
						_shaders.Bind(Shader::InstancedStatics);
					}

					unsigned int stride = sizeof(Vertex);
					unsigned int offset = 0;

					_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
					_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

					_cbInstancedStaticMeshBuffer.UpdateData(_stInstancedStaticMeshBuffer, _context.Get());

					for (auto& bucket : mesh.Buckets)
					{
						if (bucket.NumVertices == 0)
							continue;

						int passCount = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;
						for (int p = 0; p < passCount; p++)
						{
							if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
								continue;

							BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
							BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

							DrawIndexedInstancedTriangles(bucket.NumIndices, beetleCount, bucket.StartIndex, 0);

							_numInstancedStaticsDrawCalls++;
						}
					}

					beetleCount = 0;
				}
			}
		}
	}

	void Renderer::DrawLocusts(RenderView& view, RendererPass rendererPass)
	{
		if (!Objects[ID_LOCUSTS].loaded)
			return;

		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			auto* obj = &Objects[ID_LOCUSTS];
			auto& moveableObj = *_moveableObjects[ID_LOCUSTS];

			for (const auto& locust : TEN::Entities::TR4::Locusts)
			{
				if (!locust.on)
					continue;

				if (IgnoreReflectionPassForRoom(locust.roomNumber))
					continue;

				auto& mesh = *GetMesh(Objects[ID_LOCUSTS].meshIndex + (-locust.counter & 3));

				for (auto& bucket : mesh.Buckets)
				{
					if (!IsSortedBlendMode(bucket.BlendMode))
						continue;

					for (int p = 0; p < bucket.Polygons.size(); p++)
					{
						auto transformMatrix = Matrix::Lerp(locust.PrevTransform, locust.Transform, GetInterpolationFactor());
						auto centre = Vector3::Transform(bucket.Polygons[p].Centre, transformMatrix);
						float dist = (centre - view.Camera.WorldPosition).Length();

						auto object = RendererSortableObject{};
						object.ObjectType = RendererObjectType::MoveableAsStatic;
						object.Centre = centre;
						object.Distance = dist;
						object.BlendMode = bucket.BlendMode;
						object.Bucket = &bucket;
						object.LightMode = mesh.LightMode;
						object.Polygon = &bucket.Polygons[p];
						object.World = transformMatrix;
						object.Room = &_rooms[locust.roomNumber];

						view.TransparentObjectsToDraw.push_back(object);
					}
				}
			}
		}
		else
		{
			int activeLocustsExist = false;
			for (const auto& locust : TEN::Entities::TR4::Locusts)
			{
				if (!locust.on)
					continue;

				if (IgnoreReflectionPassForRoom(locust.roomNumber))
					continue;

				activeLocustsExist = true;
				break;
			}

			if (activeLocustsExist)
			{
				if (rendererPass == RendererPass::GBuffer)
				{
					_shaders.Bind(Shader::GBuffer);
					_shaders.Bind(Shader::GBufferStatics);
				}
				else
				{
					_shaders.Bind(Shader::Statics);
				}

				unsigned int stride = sizeof(Vertex);
				unsigned int offset = 0;

				_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
				_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

				auto* obj = &Objects[ID_LOCUSTS];
				auto& moveableObj = *_moveableObjects[ID_LOCUSTS];

				_stStatic.LightMode = (int)moveableObj.ObjectMeshes[0]->LightMode;

				for (const auto& locust : TEN::Entities::TR4::Locusts)
				{
					if (!locust.on)
						continue;

					auto& mesh = *GetMesh(Objects[ID_LOCUSTS].meshIndex + (-locust.counter & 3));

					auto world = Matrix::Lerp(locust.PrevTransform, locust.Transform, GetInterpolationFactor());
					ReflectMatrixOptionally(world);

					_stStatic.World = world;
					_stStatic.Color = Vector4::One;
					_stStatic.AmbientLight = _rooms[locust.roomNumber].AmbientLight;
					_cbStatic.UpdateData(_stStatic, _context.Get());

					for (auto& bucket : mesh.Buckets)
					{
						if (bucket.NumVertices == 0)
							continue;

						int passCount = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;
						for (int p = 0; p < passCount; p++)
						{
							if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
								continue;

							BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
							BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

							DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

							_numMoveablesDrawCalls++;
						}
					}
				}
			}
		}
	}

	void Renderer::DrawLines3D(RenderView& view)
	{
		SetBlendMode(BlendMode::Additive);
		SetCullMode(CullMode::None);

		_shaders.Bind(Shader::Solid);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		_primitiveBatch->Begin();

		for (const auto& line : _lines3DToDraw)
		{
			auto vertex0 = Vertex{};
			vertex0.Position = line.Origin;
			vertex0.Color = line.Color;

			auto vertex1 = Vertex{};
			vertex1.Position = line.Target;
			vertex1.Color = line.Color;

			_primitiveBatch->DrawLine(vertex0, vertex1);

			_numLinesDrawCalls++;
			_numDrawCalls++;
		}

		_primitiveBatch->End();

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
	}

	void Renderer::DrawTriangles3D(RenderView& view)
	{
		SetBlendMode(BlendMode::Additive);
		SetCullMode(CullMode::None);

		_shaders.Bind(Shader::Solid);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_primitiveBatch->Begin();

		for (const auto& tri : _triangles3DToDraw)
		{
			auto rVertices = std::vector<Vertex>{};
			rVertices.reserve(tri.Vertices.size());

			for (const auto& vertex : tri.Vertices)
			{
				auto rVertex = Vertex{};
				rVertex.Position = vertex;
				rVertex.Color = tri.Color;

				rVertices.push_back(rVertex);
			}

			_primitiveBatch->DrawTriangle(rVertices[0], rVertices[1], rVertices[2]);

			_numLinesDrawCalls++;
			_numDrawCalls++;
		}

		_primitiveBatch->End();

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
	}

	void Renderer::AddDebugLine(const Vector3& origin, const Vector3& target, const Color& color, RendererDebugPage page)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto line = RendererLine3D{};
		line.Origin = origin;
		line.Target = target;
		line.Color = color;

		_lines3DToDraw.push_back(line);
	}

	void Renderer::AddDebugTriangle(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Color& color, RendererDebugPage page)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto tri = RendererTriangle3D{};
		tri.Vertices = { vertex0, vertex1, vertex2 };
		tri.Color = color;

		_triangles3DToDraw.push_back(tri);
	}

	void Renderer::AddDebugTarget(const Vector3& center, const Quaternion& orient, float radius, const Color& color, RendererDebugPage page)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto rotMatrix = Matrix::CreateFromQuaternion(orient);

		auto origin0 = center + Vector3::Transform(Vector3(radius, 0.0f, 0.0f), rotMatrix);
		auto target0 = center + Vector3::Transform(Vector3(-radius, 0.0f, 0.0f), rotMatrix);
		AddDebugLine(origin0, target0, color);

		auto origin1 = center + Vector3::Transform(Vector3(0.0f, radius, 0.0f), rotMatrix);
		auto target1 = center + Vector3::Transform(Vector3(0.0f, -radius, 0.0f), rotMatrix);
		AddDebugLine(origin1, target1, color);

		auto origin2 = center + Vector3::Transform(Vector3(0.0f, 0.0f, radius), rotMatrix);
		auto target2 = center + Vector3::Transform(Vector3(0.0f, 0.0f, -radius), rotMatrix);
		AddDebugLine(origin2, target2, color);
	}

	void Renderer::AddDebugBox(const std::array<Vector3, BOX_VERTEX_COUNT>& corners, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		// Construct box.
		if (isWireframe)
		{
			for (int i = 0; i < BOX_EDGE_COUNT; i++)
			{
				switch (i)
				{
				case 0:
					AddDebugLine(corners[0], corners[1], color, page);
					break;
				case 1:
					AddDebugLine(corners[1], corners[2], color, page);
					break;
				case 2:
					AddDebugLine(corners[2], corners[3], color, page);
					break;
				case 3:
					AddDebugLine(corners[3], corners[0], color, page);
					break;

				case 4:
					AddDebugLine(corners[4], corners[5], color, page);
					break;
				case 5:
					AddDebugLine(corners[5], corners[6], color, page);
					break;
				case 6:
					AddDebugLine(corners[6], corners[7], color, page);
					break;
				case 7:
					AddDebugLine(corners[7], corners[4], color, page);
					break;

				case 8:
					AddDebugLine(corners[0], corners[4], color, page);
					break;
				case 9:
					AddDebugLine(corners[1], corners[5], color, page);
					break;
				case 10:
					AddDebugLine(corners[2], corners[6], color, page);
					break;
				case 11:
					AddDebugLine(corners[3], corners[7], color, page);
					break;
				}
			}
		}
		else
		{
			for (int i = 0; i < BOX_FACE_COUNT; i++)
			{
				switch (i)
				{
				case 0:
					AddDebugTriangle(corners[0], corners[1], corners[2], color, page);
					AddDebugTriangle(corners[0], corners[2], corners[3], color, page);
					break;

				case 1:
					AddDebugTriangle(corners[4], corners[5], corners[6], color, page);
					AddDebugTriangle(corners[4], corners[6], corners[7], color, page);
					break;

				case 2:
					AddDebugTriangle(corners[0], corners[1], corners[4], color, page);
					AddDebugTriangle(corners[1], corners[4], corners[5], color, page);
					break;

				case 3:
					AddDebugTriangle(corners[1], corners[2], corners[5], color, page);
					AddDebugTriangle(corners[2], corners[5], corners[6], color, page);
					break;

				case 4:
					AddDebugTriangle(corners[2], corners[3], corners[6], color, page);
					AddDebugTriangle(corners[3], corners[6], corners[7], color, page);
					break;

				case 5:
					AddDebugTriangle(corners[0], corners[3], corners[4], color, page);
					AddDebugTriangle(corners[3], corners[4], corners[7], color, page);
					break;
				}
			}
		}
	}

	void Renderer::AddDebugBox(const Vector3& min, const Vector3& max, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto box = BoundingBox((max + min) / 2, (max - min) / 2);
		AddDebugBox(box, color, page, isWireframe);
	}

	void Renderer::AddDebugBox(const BoundingOrientedBox& box, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto corners = std::array<Vector3, BOX_VERTEX_COUNT>{};
		box.GetCorners(corners.data());

		AddDebugBox(corners, color, page, isWireframe);
	}

	void Renderer::AddDebugBox(const BoundingBox& box, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto corners = std::array<Vector3, BOX_VERTEX_COUNT>{};
		box.GetCorners(corners.data());

		AddDebugBox(corners, color, page, isWireframe);
	}

	void Renderer::AddDebugCone(const Vector3& center, const Quaternion& orient, float radius, float length, const Vector4& color, RendererDebugPage page, bool isWireframe)
	{
		constexpr auto SUBDIVISION_COUNT = 32;

		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto rotMatrix = Matrix::CreateFromQuaternion(orient);

		auto baseVertices = std::vector<Vector3>{};
		baseVertices.reserve(SUBDIVISION_COUNT);
		float angle = 0.0f;

		// Calculate base circle vertices.
		for (int i = 0; i <= SUBDIVISION_COUNT; i++)
		{
			float sinAngle = sin(angle);
			float cosAngle = cos(angle);

			auto vertex = center + Vector3::Transform(Vector3(radius * sinAngle, radius * cosAngle, 0.0f), rotMatrix);
			baseVertices.push_back(vertex);

			angle += PI_MUL_2 / SUBDIVISION_COUNT;
		}

		auto dir = Geometry::ConvertQuatToDirection(orient);
		auto topCenter = Geometry::TranslatePoint(center, dir, length);

		// Construct cone.
		for (int i = 0; i < baseVertices.size(); i++)
		{
			const auto& vertex0 = baseVertices[i];
			const auto& vertex1 = baseVertices[(i == (baseVertices.size() - 1)) ? 0 : (i + 1)];

			if (isWireframe)
			{
				AddDebugLine(vertex0, vertex1, color);

				if ((i % (SUBDIVISION_COUNT / 8)) == 0)
					AddDebugLine(vertex0, topCenter, color);
			}
			else
			{
				AddDebugTriangle(vertex0, vertex1, center, color);
				AddDebugTriangle(vertex0, vertex1, topCenter, color);
			}
		}
	}

	void Renderer::AddDebugCylinder(const Vector3& center, const Quaternion& orient, float radius, float length, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		constexpr auto SUBDIVISION_COUNT = 32;

		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto rotMatrix = Matrix::CreateFromQuaternion(orient);

		auto baseVertices = std::vector<Vector3>{};
		baseVertices.reserve(SUBDIVISION_COUNT);
		float angle = 0.0f;

		// Calculate base circle vertices.
		for (int i = 0; i <= SUBDIVISION_COUNT; i++)
		{
			float sinAngle = sin(angle);
			float cosAngle = cos(angle);

			auto vertex = center + Vector3::Transform(Vector3(radius * sinAngle, radius * cosAngle, 0.0f), rotMatrix);
			baseVertices.push_back(vertex);

			angle += PI_MUL_2 / SUBDIVISION_COUNT;
		}

		auto dir = Geometry::ConvertQuatToDirection(orient);

		// Calculate top circle vertices.
		auto topVertices = baseVertices;
		for (auto& vertex : topVertices)
			vertex = Geometry::TranslatePoint(vertex, dir, length);

		// Construct cylinder.
		auto topCenter = Geometry::TranslatePoint(center, dir, length);
		for (int i = 0; i < baseVertices.size(); i++)
		{
			const auto& baseVertex0 = baseVertices[i];
			const auto& baseVertex1 = baseVertices[(i == (baseVertices.size() - 1)) ? 0 : (i + 1)];

			const auto& topVertex0 = topVertices[i];
			const auto& topVertex1 = topVertices[(i == (topVertices.size() - 1)) ? 0 : (i + 1)];

			if (isWireframe)
			{
				AddDebugLine(baseVertex0, baseVertex1, color);
				AddDebugLine(topVertex0, topVertex1, color);

				if ((i % (SUBDIVISION_COUNT / 8)) == 0)
					AddDebugLine(baseVertex0, topVertex0, color);
			}
			else
			{
				AddDebugTriangle(baseVertex0, baseVertex1, center, color);
				AddDebugTriangle(topVertex0, topVertex1, topCenter, color);
				AddDebugTriangle(baseVertex0, baseVertex1, topVertex1, color);
				AddDebugTriangle(baseVertex0, topVertex0, topVertex1, color);
			}
		}
	}

	void Renderer::AddDebugSphere(const Vector3& center, float radius, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		constexpr auto AXIS_COUNT		 = 3;
		constexpr auto SUBDIVISION_COUNT = 16;
		constexpr auto STEP_ANGLE		 = PI / (SUBDIVISION_COUNT / 4);

		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		// Construct sphere.
		if (isWireframe)
		{
			auto prevVertices = std::array<Vector3, AXIS_COUNT>{};
			for (int i = 0; i < (SUBDIVISION_COUNT / 4); i++)
			{
				float x = sin(STEP_ANGLE * i) * radius;
				float z = cos(STEP_ANGLE * i) * radius;

				float angle = 0.0f;

				for (int j = 0; j < SUBDIVISION_COUNT; j++)
				{
					auto vertices = std::array<Vector3, AXIS_COUNT>
					{
						center + Vector3(sin(angle) * abs(x), z, cos(angle) * abs(x)),
							center + Vector3(cos(angle) * abs(x), sin(angle) * abs(x), z),
							center + Vector3(z, sin(angle) * abs(x), cos(angle) * abs(x))
					};

					if (j > 0)
					{
						for (int k = 0; k < vertices.size(); k++)
							AddDebugLine(prevVertices[k], vertices[k], color);
					}

					prevVertices = vertices;
					angle += PI_MUL_2 / (SUBDIVISION_COUNT - 1);
				}
			}
		}
		else
		{
			auto vertices = std::vector<Vector3>{};
			for (int i = 0; i <= SUBDIVISION_COUNT; i++)
			{
				float pitch = ((float)i / SUBDIVISION_COUNT) * PI;
				float y = cos(pitch) * radius;
				float radiusAtY = sin(pitch) * radius;

				for (int j = 0; j < SUBDIVISION_COUNT; j++)
				{
					float theta = ((float)j / SUBDIVISION_COUNT) * PI_MUL_2;
					float x = cos(theta) * radiusAtY;
					float z = sin(theta) * radiusAtY;

					auto vertex = center + Vector3(x, y, z);
					vertices.push_back(vertex);
				}
			}

			for (int i = 0; i < SUBDIVISION_COUNT; i++)
			{
				for (int j = 0; j < SUBDIVISION_COUNT; j++)
				{
					int index0 = i * SUBDIVISION_COUNT + j;
					int index1 = ((i + 1) * SUBDIVISION_COUNT) + j;
					int index2 = ((i + 1) * SUBDIVISION_COUNT) + (j + 1) % SUBDIVISION_COUNT;
					int index3 = (i * SUBDIVISION_COUNT) + ((j + 1) % SUBDIVISION_COUNT);

					AddDebugTriangle(vertices[index0], vertices[index1], vertices[index2], color);
					AddDebugTriangle(vertices[index0], vertices[index2], vertices[index3], color);
				}
			}
		}
	}

	void Renderer::AddDebugSphere(const BoundingSphere& sphere, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		AddDebugSphere(sphere.Center, sphere.Radius, color, page, isWireframe);
	}

	void Renderer::AddDynamicSpotLight(const Vector3& pos, const Vector3& dir, float radius, float falloff, float distance, const Color& color, bool castShadows, int hash)
	{
		if (_isLocked || g_GameFlow->LastFreezeMode != FreezeMode::None)
			return;

		RendererLight dynamicLight = {};

		dynamicLight.Color = Vector3(color.x, color.y, color.z);
		if (falloff < 8)
			dynamicLight.Color *= (falloff / 8.0f);

		// Calculate outer cone angle (degrees) based on radius at the cone's end distance.
		float outerConeAngle = atan(radius / distance) * (180.0f / PI);
		float innerConeAngle = atan((radius - falloff) / distance) * (180.0f / PI);
		float innerDistance = std::max(0.0f, distance - distance * (falloff / radius));

		// Normalize direction for safety.
		auto normalizedDirection = dir;
		normalizedDirection.Normalize();

		dynamicLight.RoomNumber = NO_VALUE;
		dynamicLight.Intensity = 1.0f;
		dynamicLight.Position = pos;
		dynamicLight.Direction = normalizedDirection;
		dynamicLight.In = innerDistance;
		dynamicLight.Out = distance;
		dynamicLight.InRange = innerConeAngle > 0.0f ? innerConeAngle : 0.5f;
		dynamicLight.OutRange = outerConeAngle;
		dynamicLight.Type = LightType::Spot;
		dynamicLight.CastShadows = castShadows;
		dynamicLight.BoundingSphere = BoundingSphere(pos, distance);
		dynamicLight.Luma = Luma(dynamicLight.Color);
		dynamicLight.Hash = hash;

		PrepareDynamicLight(dynamicLight);
	}

	void Renderer::AddDynamicPointLight(const Vector3& pos, float radius, const Color& color, bool castShadows, int hash)
	{
		if (_isLocked || g_GameFlow->LastFreezeMode != FreezeMode::None)
			return;

		auto dynamicLight = RendererLight{};

		dynamicLight.Color = Vector3(color.x, color.y, color.z);
		if (radius < BLOCK(2))
			dynamicLight.Color *= (radius / BLOCK(2));

		dynamicLight.RoomNumber = NO_VALUE;
		dynamicLight.Intensity = 1.0f;
		dynamicLight.Position = pos;
		dynamicLight.In = 1.0f;
		dynamicLight.Out = radius;
		dynamicLight.Type = LightType::Point;
		dynamicLight.CastShadows = castShadows;
		dynamicLight.BoundingSphere = BoundingSphere(pos, radius);
		dynamicLight.Luma = Luma(dynamicLight.Color);
		dynamicLight.Hash = hash;

		PrepareDynamicLight(dynamicLight);
	}

	void Renderer::PrepareDynamicLight(RendererLight& light)
	{
		// If hash is provided, search for same light in previous buffer.
		if (light.Hash != 0)
		{
			// Determine previous buffer index.
			const auto& prevList = _dynamicLights[1 - _dynamicLightList];

			// Find light in previous buffer with same hash.
			auto it = std::find_if(
				prevList.begin(), prevList.end(),
				[&light](const auto& prevLight)
				{
					return (prevLight.Hash == light.Hash);
				});

			if (it != prevList.end())
			{
				// If matching light is found, copy it.
				const auto& prevLight = *it;
				light.PrevPosition = prevLight.Position;
				light.PrevDirection = prevLight.Direction;
			}
		}

		// Queue dynamic light.
		_dynamicLights[_dynamicLightList].push_back(light);

		// Check if light is spawned in mirrored room and create reflection.
		for (const auto& mirror : g_Level.Mirrors)
		{
			if (!mirror.ReflectLights)
				continue;

			// TODO: Avoid LaraItem global.
			if ((Camera.pos.RoomNumber == mirror.RoomNumber || LaraItem->RoomNumber == mirror.RoomNumber) && 
				IsPointInRoom(light.Position, mirror.RoomNumber))
			{
				auto reflectedLight = light;
				reflectedLight.Position = Vector3::Transform(light.Position, mirror.ReflectionMatrix);
				reflectedLight.Direction = Vector3::Transform(light.Direction, mirror.ReflectionMatrix);
				reflectedLight.Hash = 0;

				_dynamicLights[_dynamicLightList].push_back(reflectedLight);
			}
		}
	}

	void Renderer::PrepareScene()
	{
		if (g_GameFlow->CurrentFreezeMode == FreezeMode::None)
		{
			_dynamicLightList ^= 1;
			_dynamicLights[_dynamicLightList].clear();
		}

		_lines2DToDraw.clear();
		_lines3DToDraw.clear();
		_triangles3DToDraw.clear();
		_stringsToDraw.clear();

		_currentCausticsFrame++;
		_currentCausticsFrame %= 32;

		constexpr auto BLINK_VALUE_MAX = 1.0f;
		constexpr auto BLINK_VALUE_MIN = 0.1f;
		constexpr auto BLINK_TIME_STEP = 0.2f;

		// Calculate blink increment based on sine wave.
		float blink = ((sin(_blinkTime) + BLINK_VALUE_MAX) * 0.5f) + BLINK_VALUE_MIN;
		_blinkColorValue = Vector4(blink, blink, blink, 1.0f);

		// Update blink time.
		_blinkTime += BLINK_TIME_STEP;
		if (_blinkTime > PI_MUL_2)
			_blinkTime -= PI_MUL_2;

		_oldGameCamera = _currentGameCamera;
		Camera.DisableInterpolation = false;

		_isLocked = false;
	}

	void Renderer::ClearScene()
	{
		_gameCamera.Clear();

		ResetItems();
		ClearShadowMap();
	}

	void Renderer::RenderScene(RenderTarget2D* renderTarget, RenderView& view, SceneRenderMode renderMode)
	{
		using ns = std::chrono::nanoseconds;
		using get_time = std::chrono::steady_clock;

		ResetDebugVariables();

		auto& level = *g_GameFlow->GetLevel(CurrentLevel);

		// Prepare scene to draw.
		auto time1 = std::chrono::high_resolution_clock::now();
		CollectMirrors(view);
		CollectRooms(view, false);
		auto time = std::chrono::high_resolution_clock::now();
		_timeRoomsCollector = (std::chrono::duration_cast<ns>(time - time1)).count() / 1000000;
		time1 = time;

		UpdateLaraAnimations(false);
		UpdateItemAnimations(view);

		_stBlending.AlphaTest = -1;
		_stBlending.AlphaThreshold = -1;

		CollectLightsForCamera();
		RenderItemShadows(view);

		// Prepare all sprites for later.
		PrepareFires(view);
		PrepareSmokes(view);
		PrepareSmokeParticles(view);
		PrepareSimpleParticles(view);
		PrepareSparkParticles(view);
		PrepareExplosionParticles(view);
		PrepareFootprints(view);
		PrepareBlood(view);
		PrepareWeatherParticles(view);
		PrepareParticles(view);
		PrepareBubbles(view);
		PrepareDrips(view);
		PrepareRipples(view);
		PrepareUnderwaterBloodParticles(view);
		PrepareSplashes(view);
		PrepareShockwaves(view);
		PrepareElectricity(view);
		PrepareHelicalLasers(view);
		PrepareRopes(view);
		PrepareStreamers(view);
		PrepareLaserBarriers(view);
		PrepareSingleLaserBeam(view);
		PrepareFireflies(view);

		// Sprites grouped in buckets for instancing. Non-commutative sprites are collected at a later stage.
		SortAndPrepareSprites(view);

		auto time2 = std::chrono::high_resolution_clock::now();
		_timeUpdate = (std::chrono::duration_cast<ns>(time2 - time1)).count() / 1000000;
		time1 = time2;

		// Reset GPU state.
		SetBlendMode(BlendMode::Opaque, true);
		SetDepthState(DepthState::Write, true);
		SetCullMode(CullMode::CounterClockwise, true);

		// Bind constant buffers.
		BindConstantBufferVS(ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		BindConstantBufferVS(ConstantBufferRegister::Item, _cbItem.get());
		BindConstantBufferVS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());
		BindConstantBufferVS(ConstantBufferRegister::ShadowLight, _cbShadowMap.get());
		BindConstantBufferVS(ConstantBufferRegister::Room, _cbRoom.get());
		BindConstantBufferVS(ConstantBufferRegister::AnimatedTextures, _cbAnimated.get());
		BindConstantBufferVS(ConstantBufferRegister::Static, _cbStatic.get());
		BindConstantBufferVS(ConstantBufferRegister::Sprite, _cbSprite.get());
		BindConstantBufferVS(ConstantBufferRegister::Blending, _cbBlending.get());
		BindConstantBufferVS(ConstantBufferRegister::InstancedSprites, _cbInstancedSpriteBuffer.get());
		BindConstantBufferVS(ConstantBufferRegister::PostProcess, _cbPostProcessBuffer.get());

		BindConstantBufferPS(ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		BindConstantBufferPS(ConstantBufferRegister::Item, _cbItem.get());
		BindConstantBufferPS(ConstantBufferRegister::InstancedStatics, _cbInstancedStaticMeshBuffer.get());
		BindConstantBufferPS(ConstantBufferRegister::ShadowLight, _cbShadowMap.get());
		BindConstantBufferPS(ConstantBufferRegister::Room, _cbRoom.get());
		BindConstantBufferPS(ConstantBufferRegister::AnimatedTextures, _cbAnimated.get());
		BindConstantBufferPS(ConstantBufferRegister::Static, _cbStatic.get());
		BindConstantBufferPS(ConstantBufferRegister::Sprite, _cbSprite.get());
		BindConstantBufferPS(ConstantBufferRegister::Blending, _cbBlending.get());
		BindConstantBufferPS(ConstantBufferRegister::InstancedSprites, _cbInstancedSpriteBuffer.get());
		BindConstantBufferPS(ConstantBufferRegister::PostProcess, _cbPostProcessBuffer.get());

		// Set up vertex parameters.
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// TODO: Needs improvements before enabling it.
		// RenderSimpleSceneToParaboloid(&_roomAmbientMapFront, LaraItem->Pose.Position.ToVector3(), 1);
		// RenderSimpleSceneToParaboloid(&_roomAmbientMapBack, LaraItem->Pose.Position.ToVector3(), -1);

		// Bind and clear render target.
		_context->ClearRenderTargetView(_renderTarget.RenderTargetView.Get(), _debugPage == RendererDebugPage::WireframeMode ? Colors::DimGray : Colors::Black);
		_context->ClearDepthStencilView(_renderTarget.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// Reset viewport and scissor.
		_context->RSSetViewports(1, &view.Viewport);
		ResetScissor();

		// Camera constant buffer contains matrices, camera position, fog values, and other things shared for all shaders.
		CCameraMatrixBuffer cameraConstantBuffer;
		view.FillConstantBuffer(cameraConstantBuffer);
		cameraConstantBuffer.Frame = GlobalCounter;
		cameraConstantBuffer.RefreshRate = _refreshRate;
		cameraConstantBuffer.CameraUnderwater = g_Level.Rooms[cameraConstantBuffer.RoomNumber].flags & ENV_FLAG_WATER;
		cameraConstantBuffer.DualParaboloidView = Matrix::CreateLookAt(LaraItem->Pose.Position.ToVector3(), LaraItem->Pose.Position.ToVector3() + Vector3(0, 0, 1024), -Vector3::UnitY);

		if (level.GetFogMaxDistance() > 0)
		{
			auto fogColor = level.GetFogColor();
			cameraConstantBuffer.FogColor = Vector4(fogColor.GetR() / 255.0f, fogColor.GetG() / 255.0f, fogColor.GetB() / 255.0f, 1.0f);
			cameraConstantBuffer.FogMinDistance = level.GetFogMinDistance();
			cameraConstantBuffer.FogMaxDistance = level.GetFogMaxDistance();
		}
		else
		{
			cameraConstantBuffer.FogMaxDistance = 0.0f;
			cameraConstantBuffer.FogColor = Vector4::Zero;
		}

		cameraConstantBuffer.AmbientOcclusion = g_Configuration.EnableAmbientOcclusion ? 1 : 0;
		cameraConstantBuffer.AmbientOcclusionExponent = 2;

		// Set fog bulbs.
		cameraConstantBuffer.NumFogBulbs = (int)view.FogBulbsToDraw.size();
		for (int i = 0; i < view.FogBulbsToDraw.size(); i++)
		{
			cameraConstantBuffer.FogBulbs[i].Position = view.FogBulbsToDraw[i].Position;
			cameraConstantBuffer.FogBulbs[i].Density = view.FogBulbsToDraw[i].Density * (CLICK(1) / view.FogBulbsToDraw[i].Radius);
			cameraConstantBuffer.FogBulbs[i].SquaredRadius = SQUARE(view.FogBulbsToDraw[i].Radius);
			cameraConstantBuffer.FogBulbs[i].Color = view.FogBulbsToDraw[i].Color;
			cameraConstantBuffer.FogBulbs[i].SquaredCameraToFogBulbDistance = SQUARE(view.FogBulbsToDraw[i].Distance);
			cameraConstantBuffer.FogBulbs[i].FogBulbToCameraVector = view.FogBulbsToDraw[i].FogBulbToCameraVector;
		}

		_cbCameraMatrices.UpdateData(cameraConstantBuffer, _context.Get());

		ID3D11RenderTargetView* pRenderViewPtrs[2];

		// Bind main render target.
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(), _renderTarget.DepthStencilView.Get());

		// Draw horizon and sky.
		DrawHorizonAndSky(view, _renderTarget.DepthStencilView.Get());

		// Build G-Buffer (normals + depth).
		_context->ClearRenderTargetView(_normalsRenderTarget.RenderTargetView.Get(), Colors::Black);
		_context->ClearRenderTargetView(_depthRenderTarget.RenderTargetView.Get(), Colors::White);

		pRenderViewPtrs[0] = _normalsRenderTarget.RenderTargetView.Get();
		pRenderViewPtrs[1] = _depthRenderTarget.RenderTargetView.Get();
		_context->OMSetRenderTargets(2, &pRenderViewPtrs[0], _renderTarget.DepthStencilView.Get());

		// Render G-Buffer pass.
		DoRenderPass(RendererPass::GBuffer, view, true);

		// Calculate ambient occlusion.
		if (g_Configuration.EnableAmbientOcclusion)
			CalculateSSAO(view);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_context->RSSetViewports(1, &view.Viewport);
		ResetScissor();

		// Bind main render target again. Main depth buffer is already filled and avoids overdraw in following steps.
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(), _renderTarget.DepthStencilView.Get());

		DoRenderPass(RendererPass::Opaque, view, true);
		DoRenderPass(RendererPass::Additive, view, true);
		DoRenderPass(RendererPass::CollectTransparentFaces, view, false);
		SortTransparentFaces(view);

		DoRenderPass(RendererPass::Transparent, view, true);
		DoRenderPass(RendererPass::GunFlashes, view, true); // HACK: Gunflashes are drawn after everything because they are near camera.
		
		// Draw 3D debug lines and triangles.
		DrawLines3D(view);
		DrawTriangles3D(view);

		// Draw HUD.
		ClearDrawPhaseDisplaySprites();

		_context->ClearDepthStencilView(_renderTarget.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// HUD must be drawn before post-processing to be antialiased.
		if (renderMode == SceneRenderMode::Full && g_GameFlow->LastGameStatus == GameStatus::Normal)
			g_Hud.Draw(*LaraItem);
		
		if (renderMode != SceneRenderMode::NoPostprocess)
		{
			DrawPostprocess(renderTarget, view, renderMode);
			DrawOverlays(view);
			DrawLines2D();
		}

		if (renderMode == SceneRenderMode::Full && g_GameFlow->LastGameStatus == GameStatus::Normal)
		{
			// Draw display sprites sorted by priority.
			CollectDisplaySprites(view);
			DrawDisplaySprites(view);

			DrawDebugInfo(view);
			DrawAllStrings();
		}

		time2 = std::chrono::high_resolution_clock::now();
		_timeFrame = (std::chrono::duration_cast<ns>(time2 - time1)).count() / 1000000;
		time1 = time2;

		ClearScene();
		CalculateFrameRate();
	}

	void Renderer::RenderSimpleSceneToParaboloid(RenderTarget2D* renderTarget, Vector3 position, int emisphere)
	{
		// TODO: Update the horizon draw code here once paraboloids are required. TrainWreck Feb 2, 2025.
		
		// Reset GPU state
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		_shaders.Bind(Shader::RoomAmbient);

		// Bind and clear render target
		_context->ClearRenderTargetView(renderTarget->RenderTargetView.Get(), Colors::Black);
		_context->ClearDepthStencilView(renderTarget->DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		_context->OMSetRenderTargets(1, renderTarget->RenderTargetView.GetAddressOf(), renderTarget->DepthStencilView.Get());

		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = ROOM_AMBIENT_MAP_SIZE;
		viewport.Height = ROOM_AMBIENT_MAP_SIZE;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;

		_context->RSSetViewports(1, &viewport);

		D3D11_RECT rects[1];
		rects[0].left = 0;
		rects[0].right = ROOM_AMBIENT_MAP_SIZE;
		rects[0].top = 0;
		rects[0].bottom = ROOM_AMBIENT_MAP_SIZE;

		_context->RSSetScissorRects(1, rects);

		// Opaque geometry
		SetBlendMode(BlendMode::Opaque);

		if (emisphere == -1)
		{
			SetCullMode(CullMode::CounterClockwise);
		}
		else
		{
			SetCullMode(CullMode::Clockwise);
		}

		auto view = RenderView(&Camera, 0, PI / 2.0f, 32, DEFAULT_FAR_VIEW, ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE);

		CCameraMatrixBuffer cameraConstantBuffer;
		cameraConstantBuffer.DualParaboloidView = Matrix::CreateLookAt(position, position + Vector3(0, 0, 1024), -Vector3::UnitY);
		cameraConstantBuffer.Emisphere = emisphere;
		view.FillConstantBuffer(cameraConstantBuffer);
		_cbCameraMatrices.UpdateData(cameraConstantBuffer, _context.Get());

		// Draw horizon and sky.
		auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);

		if (levelPtr->GetHorizonEnabled(0) || levelPtr->GetHorizonEnabled(1))
		{
			_shaders.Bind(Shader::RoomAmbientSky);

			if (Lara.Control.Look.OpticRange != 0)
				AlterFOV(ANGLE(DEFAULT_FOV) - Lara.Control.Look.OpticRange, false);

			unsigned int stride = sizeof(Vertex);
			unsigned int offset = 0;

			// Draw sky.
			auto rotation = Matrix::CreateRotationX(PI);

			BindTexture(TextureRegister::ColorMap, &_skyTexture, SamplerStateRegister::AnisotropicClamp);

			_context->IASetVertexBuffers(0, 1, _skyVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetIndexBuffer(_skyIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			SetBlendMode(BlendMode::Additive);

			for (int s = 0; s < 2; s++)
			{
				for (int i = 0; i < 2; i++)
				{
					auto weather = TEN::Effects::Environment::Weather;

					auto translation = Matrix::CreateTranslation(Camera.pos.x + weather.SkyPosition(s) - i * SKY_SIZE,
						Camera.pos.y - 1536.0f, Camera.pos.z);
					auto world = rotation * translation;

					_stStatic.World = (rotation * translation);
					_stStatic.Color = weather.SkyColor(s);
					_stStatic.ApplyFogBulbs = s == 0 ? 1 : 0;
					_cbStatic.UpdateData(_stStatic, _context.Get());

					DrawIndexedTriangles(SKY_INDICES_COUNT, 0, 0);
				}
			}

			_context->ClearDepthStencilView(renderTarget->DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

			// Draw horizon.
			if (_moveableObjects[ID_HORIZON].has_value()) // FIXME: Replace with same function as in the main pipeline!
			{
				_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
				_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

				const auto& moveableObj = *_moveableObjects[ID_HORIZON]; // FIXME: Replace with same function as in the main pipeline!

				_stStatic.World = Matrix::CreateTranslation(LaraItem->Pose.Position.ToVector3());
				_stStatic.Color = Vector4::One;
				_stStatic.ApplyFogBulbs = 1;
				_cbStatic.UpdateData(_stStatic, _context.Get());

				for (const auto* mesh : moveableObj.ObjectMeshes)
				{
					for (const auto& bucket : mesh->Buckets)
					{
						if (bucket.NumVertices == 0)
							continue;

						BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

						// Always render horizon as alpha-blended surface.
						SetBlendMode(bucket.BlendMode == BlendMode::AlphaTest ? BlendMode::AlphaBlend : bucket.BlendMode);
						SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

						// Draw vertices.
						DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

						_numMoveablesDrawCalls++;
					}
				}
			}

			// Clear just the Z-buffer to start drawing on top of horizon.
			_context->ClearDepthStencilView(renderTarget->DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		}

		_shaders.Bind(Shader::RoomAmbient);

		// Draw rooms
		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		// Bind vertex and index buffer.
		_context->IASetVertexBuffers(0, 1, _roomsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetIndexBuffer(_roomsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		for (int i = 0; i < _rooms.size(); i++)
		{
			int index = i;
			auto* room = &_rooms[index];
			auto* nativeRoom = &g_Level.Rooms[room->RoomNumber];

			// Avoid drawing of too far rooms... Environment map is tiny, blurred, so very far rooms would not contribute to the
			// final pixel colors
			if (Vector3::Distance(room->BoundingBox.Center, LaraItem->Pose.Position.ToVector3()) >= BLOCK(40))
			{
				//continue;
			}
			  
			cameraConstantBuffer.CameraUnderwater = g_Level.Rooms[_rooms[i].RoomNumber].flags & ENV_FLAG_WATER;
			_cbCameraMatrices.UpdateData(cameraConstantBuffer, _context.Get());

			_stRoom.Caustics = 0;
			_stRoom.AmbientColor = room->AmbientLight;
			_stRoom.NumRoomLights = 0;
			_stRoom.Water = (nativeRoom->flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
			_cbRoom.UpdateData(_stRoom, _context.Get());

			for (auto& bucket : room->Buckets)
			{
				if (bucket.NumVertices == 0)
					continue;

				SetBlendMode(bucket.BlendMode);
				SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

				if (bucket.Animated)
				{
					BindTexture(TextureRegister::ColorMap, &std::get<0>(_animatedTextures[bucket.Texture]),
						SamplerStateRegister::AnisotropicClamp);
				}
				else
				{
					BindTexture(TextureRegister::ColorMap, &std::get<0>(_roomTextures[bucket.Texture]),
						SamplerStateRegister::AnisotropicClamp);
				}

				DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

				_numRoomsDrawCalls++;
			}
		}
		  
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);
		SetBlendMode(BlendMode::Opaque, true);

		// TODO: to finish
		/*
		// Smooth the ambient map with guassian 5x5 filter
		_context->ClearRenderTargetView(_tempRoomAmbientRenderTarget1.RenderTargetView.Get(), Colors::Black);
		_context->ClearDepthStencilView(_tempRoomAmbientRenderTarget1.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		_context->OMSetRenderTargets(1, _tempRoomAmbientRenderTarget1.RenderTargetView.GetAddressOf(), _tempRoomAmbientRenderTarget1.DepthStencilView.Get());

		_postProcess->SetSourceTexture(renderTarget->ShaderResourceView.Get());
		_postProcess->SetEffect(BasicPostProcess::GaussianBlur_5x5);
		_postProcess->SetGaussianParameter(1);
		_postProcess->Process(_context.Get());

		_context->ClearRenderTargetView(_tempRoomAmbientRenderTarget2.RenderTargetView.Get(), Colors::Black);
		_context->ClearDepthStencilView(_tempRoomAmbientRenderTarget2.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		_context->OMSetRenderTargets(1, _tempRoomAmbientRenderTarget2.RenderTargetView.GetAddressOf(), _tempRoomAmbientRenderTarget2.DepthStencilView.Get());

		_postProcess->SetSourceTexture(_tempRoomAmbientRenderTarget1.ShaderResourceView.Get());
		_postProcess->SetEffect(BasicPostProcess::GaussianBlur_5x5);
		_postProcess->SetGaussianParameter(1);
		_postProcess->Process(_context.Get()); 

		_context->ClearRenderTargetView(_tempRoomAmbientRenderTarget3.RenderTargetView.Get(), Colors::Black);
		_context->ClearDepthStencilView(_tempRoomAmbientRenderTarget3.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		_context->OMSetRenderTargets(1, _tempRoomAmbientRenderTarget3.RenderTargetView.GetAddressOf(), _tempRoomAmbientRenderTarget3.DepthStencilView.Get());

		_postProcess->SetSourceTexture(_tempRoomAmbientRenderTarget2.ShaderResourceView.Get());
		_postProcess->SetEffect(BasicPostProcess::GaussianBlur_5x5);
		_postProcess->SetGaussianParameter(1);
		_postProcess->Process(_context.Get());

		_context->ClearRenderTargetView(_tempRoomAmbientRenderTarget4.RenderTargetView.Get(), Colors::Black);
		_context->ClearDepthStencilView(_tempRoomAmbientRenderTarget4.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		_context->OMSetRenderTargets(1, _tempRoomAmbientRenderTarget4.RenderTargetView.GetAddressOf(), _tempRoomAmbientRenderTarget4.DepthStencilView.Get());

		_postProcess->SetSourceTexture(_tempRoomAmbientRenderTarget3.ShaderResourceView.Get());
		_postProcess->SetEffect(BasicPostProcess::GaussianBlur_5x5);
		_postProcess->SetGaussianParameter(1);
		_postProcess->Process(_context.Get());

		_context->ClearRenderTargetView(renderTarget->RenderTargetView.Get(), Colors::Black);
		_context->ClearDepthStencilView(renderTarget->DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		_context->OMSetRenderTargets(1, renderTarget->RenderTargetView.GetAddressOf(), renderTarget->DepthStencilView.Get());

		// Copy back the filtered map to the render target
		_postProcess->SetSourceTexture(_tempRoomAmbientRenderTarget4.ShaderResourceView.Get());
		_postProcess->SetEffect(BasicPostProcess::Copy);
		_postProcess->Process(_context.Get());

		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);
		SetBlendMode(BlendMode::Opaque, true);*/
	}

	void Renderer::DumpGameScene(SceneRenderMode renderMode)
	{
		RenderScene(&_dumpScreenRenderTarget, _gameCamera, renderMode);
	}

	void Renderer::DoRenderPass(RendererPass pass, RenderView& view, bool drawMirrors)
	{
		// Reset GPU state.
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::Write);

		// Draw room geometry first if applicable for a given pass.
		if (pass != RendererPass::Transparent && pass != RendererPass::GunFlashes)
			DrawRooms(view, pass);

		// Draw all objects.
		DrawObjects(pass, view, true, true, true, true);

		// If mirrors are in view, render mirrored objects for every mirror.
		if (drawMirrors && !view.Mirrors.empty())
		{
			SetCullMode(CullMode::Clockwise);
			for (auto& mirror : view.Mirrors)
			{
				_currentMirror = &mirror;
				DrawObjects(pass, view, mirror.ReflectPlayer, mirror.ReflectMoveables, mirror.ReflectStatics, mirror.ReflectSprites);
				_currentMirror = nullptr;
			}

			SetCullMode(CullMode::CounterClockwise);
		}
	}

	void Renderer::DrawObjects(RendererPass pass, RenderView& view, bool player, bool moveables, bool statics, bool sprites)
	{
		switch (pass)
		{
		case RendererPass::Transparent:
			DrawSortedFaces(view);
			break;

		case RendererPass::GunFlashes:
			DrawGunFlashes(view);
			DrawBaddyGunflashes(view);
			break;

		default:
			if (moveables)
			{
				DrawItems(view, pass);
				DrawEffects(view, pass);
				DrawGunShells(view, pass);
				DrawSpiders(view, pass);
				DrawScarabs(view, pass);
				DrawBats(view, pass);
				DrawRats(view, pass);
				DrawLocusts(view, pass);
				DrawFishSwarm(view, pass);
			}
			else if (player)
			{
				DrawItems(view, pass, true);
				DrawGunShells(view, pass);
			}

			if (statics)
			{
				DrawStatics(view, pass);
				DrawDebris(view, pass); // Debris mostly originate from shatter statics.
			}

			// Sorted sprites already collected at beginning of frame.
			if (sprites && pass != RendererPass::CollectTransparentFaces)
				DrawSprites(view, pass);

			break;
		}
	}

	void Renderer::DrawItems(RenderView& view, RendererPass rendererPass, bool onlyPlayer)
	{
		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		// Set shaders.
		if (rendererPass == RendererPass::GBuffer)
		{
			_shaders.Bind(Shader::GBuffer);
			_shaders.Bind(Shader::GBufferItems);
		}
		else
		{
			_shaders.Bind(Shader::Items);
		}

		BindRenderTargetAsTexture(TextureRegister::SSAO, &_SSAOBlurredRenderTarget, SamplerStateRegister::PointWrap);

		for (auto room : view.RoomsToDraw)
		{
			if (IgnoreReflectionPassForRoom(room->RoomNumber))
				continue;

			for (auto itemToDraw : room->ItemsToDraw)
			{
				if (_currentMirror != nullptr && (g_Level.Items[itemToDraw->ItemNumber].Flags & IFLAG_CLEAR_BODY))
					continue;

				if (onlyPlayer && itemToDraw->ObjectID != ID_LARA)
					continue;

				switch (itemToDraw->ObjectID)
				{ 
				case ID_LARA:
					DrawLara(view, rendererPass);
					continue;

				case ID_WATERFALL1:
				case ID_WATERFALL2:
				case ID_WATERFALL3:
				case ID_WATERFALL4:
				case ID_WATERFALL5:
				case ID_WATERFALL6:
				case ID_WATERFALLSS1:
				case ID_WATERFALLSS2:
					DrawWaterfalls(itemToDraw, view, 10, rendererPass);
					continue;

				default:
					DrawAnimatingItem(itemToDraw, view, rendererPass);
					continue;
				}
			}
		}
	}

	void Renderer::RenderItemShadows(RenderView& renderView)
	{
		RenderBlobShadows(renderView);

		if (g_Configuration.ShadowType != ShadowMode::None)
		{
			for (auto room : renderView.RoomsToDraw)
				for (auto itemToDraw : room->ItemsToDraw)
					RenderShadowMap(itemToDraw, renderView);
		}
	}

	void Renderer::DrawWaterfalls(RendererItem* item, RenderView& view, int fps, RendererPass rendererPass)
	{
		// Extremely hacky function to get first rendered face of a waterfall object mesh, calculate
		// its texture height and scroll all the textures according to that height.

		RendererObject& moveableObj = *_moveableObjects[item->ObjectID];

		// No mesh or bucket, abort
		if (!moveableObj.ObjectMeshes.size() || !moveableObj.ObjectMeshes[0]->Buckets.size())
			return;

		// Get first three vertices of a waterfall object, meaning the very first triangle
		const auto& v1 = _moveablesVertices[moveableObj.ObjectMeshes[0]->Buckets[0].StartVertex + 0];
		const auto& v2 = _moveablesVertices[moveableObj.ObjectMeshes[0]->Buckets[0].StartVertex + 1];
		const auto& v3 = _moveablesVertices[moveableObj.ObjectMeshes[0]->Buckets[0].StartVertex + 2];

		// Calculate height of the texture by getting min/max UV.y coords of all three vertices
		auto minY = std::min(std::min(v1.UV.y, v2.UV.y), v3.UV.y);
		auto maxY = std::max(std::max(v1.UV.y, v2.UV.y), v3.UV.y);
		auto minX = std::min(std::min(v1.UV.x, v2.UV.x), v3.UV.x);
		auto maxX = std::max(std::max(v1.UV.x, v2.UV.x), v3.UV.x);

		// Setup animated buffer
		_stAnimated.Fps = fps;
		_stAnimated.NumFrames = 1;
		_stAnimated.Type = 1; // UVRotate

		// We need only top/bottom Y coordinate for UVRotate, but we pass whole
		// rectangle anyway, in case later we may want to implement different UVRotate modes.
		_stAnimated.Textures[0].TopLeft     = Vector2(minX, minY);
		_stAnimated.Textures[0].TopRight    = Vector2(maxX, minY);
		_stAnimated.Textures[0].BottomLeft  = Vector2(minX, maxY);
		_stAnimated.Textures[0].BottomRight = Vector2(maxX, maxY);
		
		_cbAnimated.UpdateData(_stAnimated, _context.Get());

		DrawAnimatingItem(item, view, rendererPass);

		// Reset animated buffer after rendering just in case
		_stAnimated.Fps = _stAnimated.NumFrames = _stAnimated.Type = 0;
		_cbAnimated.UpdateData(_stAnimated, _context.Get());
	}

	void Renderer::DrawAnimatingItem(RendererItem* item, RenderView& view, RendererPass rendererPass)
	{
		ItemInfo* nativeItem = &g_Level.Items[item->ItemNumber];
		RendererRoom* room = &_rooms[item->RoomNumber];
		RendererObject& moveableObj = *_moveableObjects[item->ObjectID];

		// Bind item main properties
		_stItem.World = item->InterpolatedWorld;
		ReflectMatrixOptionally(_stItem.World);

		_stItem.Color = item->Color;
		_stItem.AmbientLight = item->AmbientLight;
		memcpy(_stItem.BonesMatrices, item->InterpolatedAnimTransforms, sizeof(Matrix) * MAX_BONES);

		for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
			_stItem.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

		bool acceptsShadows = moveableObj.ShadowType == ShadowMode::None;
		BindMoveableLights(item->LightsToDraw, item->RoomNumber, item->PrevRoomNumber, item->LightFade, acceptsShadows);
		_cbItem.UpdateData(_stItem, _context.Get());

		for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
		{
			if (!nativeItem->MeshBits.Test(k))
				continue;

			DrawMoveableMesh(item, GetMesh(item->MeshIds[k]), room, k, view, rendererPass);
		}
	}

	void Renderer::DrawStatics(RenderView& view, RendererPass rendererPass)
	{
		if (_staticTextures.size() == 0 || view.SortedStaticsToDraw.size() == 0)
			return;
		 
		if (rendererPass != RendererPass::CollectTransparentFaces)
		{
#ifdef DISABLE_INSTANCING
			if (rendererPass == RendererPass::GBuffer)
			{
				_shaders.Bind(Shader::GBuffer);
				_shaders.Bind(Shader::GBufferStatics);
			}
			else
			{
				_shaders.Bind(Shader::Statics);
			}

			// Bind vertex and index buffer
			UINT stride = sizeof(Vertex);
			UINT offset = 0;
			_context->IASetVertexBuffers(0, 1, _staticsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetIndexBuffer(_staticsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			BindRenderTargetAsTexture(TextureRegister::SSAO, &_SSAOBlurredRenderTarget, SamplerStateRegister::PointWrap);

			for (auto it = view.SortedStaticsToDraw.begin(); it != view.SortedStaticsToDraw.end(); it++)
			{
				std::vector<RendererStatic*> statics = it->second;

				RendererStatic* refStatic = statics[0];
				RendererObject& refStaticObj = GetStaticRendererObject(refStatic->ObjectNumber);
				if (refStaticObj.ObjectMeshes.size() == 0)
					continue;

				RendererMesh* refMesh = refStaticObj.ObjectMeshes[0];

				int staticsCount = (int)statics.size();

				for (int s = 0; s < staticsCount; s++)
				{
					RendererStatic* current = statics[s];
					RendererRoom* room = &_rooms[current->RoomNumber];

					if (IgnoreReflectionPassForRoom(current->RoomNumber))
						continue;

					_stStatic.World = current->World;
					_stStatic.Color = current->Color;
					_stStatic.AmbientLight = room->AmbientLight;
					_stStatic.LightMode = (int)refMesh->LightMode;

					if (rendererPass != RendererPass::GBuffer)
					{
						BindStaticLights(current->LightsToDraw);
					}

					_cbStatic.UpdateData(_stStatic, _context.Get());

					for (auto& bucket : refMesh->Buckets)
					{
						if (bucket.NumVertices == 0)
						{
							continue;
						}

						int passes = rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;

						for (int p = 0; p < passes; p++)
						{
							if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
							{
								continue;
							}

							BindTexture(TextureRegister::ColorMap,
								&std::get<0>(_staticTextures[bucket.Texture]),
								SamplerStateRegister::AnisotropicClamp);
							BindTexture(TextureRegister::NormalMap,
								&std::get<1>(_staticTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

							DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

							_numInstancedStaticsDrawCalls++;
						}
					}
				}
			}
#else
			if (rendererPass == RendererPass::GBuffer)
			{
				_shaders.Bind(Shader::GBuffer);
				_shaders.Bind(Shader::GBufferInstancedStatics);
			}
			else
			{
				_shaders.Bind(Shader::InstancedStatics);
			}

			// Bind vertex and index buffer
			UINT stride = sizeof(Vertex);
			UINT offset = 0;
			_context->IASetVertexBuffers(0, 1, _staticsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetIndexBuffer(_staticsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			
			BindRenderTargetAsTexture(TextureRegister::SSAO, &_SSAOBlurredRenderTarget, SamplerStateRegister::PointWrap);

			for (auto it = view.SortedStaticsToDraw.begin(); it != view.SortedStaticsToDraw.end(); it++)
			{
				auto statics = it->second;

				auto* refStatic = statics[0];
				auto& refStaticObj = GetStaticRendererObject(refStatic->ObjectNumber);
				if (refStaticObj.ObjectMeshes.size() == 0)
					continue;

				auto* refMesh = refStaticObj.ObjectMeshes[0];

				int staticsCount = (int)statics.size();
				int bucketSize = INSTANCED_STATIC_MESH_BUCKET_SIZE;
				int baseStaticIndex = 0;

				while (baseStaticIndex < staticsCount)
				{
					int instancesCount = 0;
					int max = std::min(baseStaticIndex + bucketSize, staticsCount);

					for (int s = baseStaticIndex; s < max; s++)
					{
						auto* current = statics[s];
						auto* room = &_rooms[current->RoomNumber];

						if (IgnoreReflectionPassForRoom(current->RoomNumber))
							continue;

						if (current->Color.w < ALPHA_BLEND_THRESHOLD)
							continue;

						auto world = current->World;
						ReflectMatrixOptionally(world);

						_stInstancedStaticMeshBuffer.StaticMeshes[instancesCount].World = world;
						_stInstancedStaticMeshBuffer.StaticMeshes[instancesCount].Color = current->Color;
						_stInstancedStaticMeshBuffer.StaticMeshes[instancesCount].Ambient = room->AmbientLight;
						_stInstancedStaticMeshBuffer.StaticMeshes[instancesCount].LightMode = (int)refMesh->LightMode;

						if (rendererPass != RendererPass::GBuffer)
							BindInstancedStaticLights(current->LightsToDraw, instancesCount);

						instancesCount++;
					}

					baseStaticIndex += bucketSize;

					if (instancesCount > 0)
					{
						_cbInstancedStaticMeshBuffer.UpdateData(_stInstancedStaticMeshBuffer, _context.Get());

						for (const auto& bucket : refMesh->Buckets)
						{
							if (bucket.NumVertices == 0)
								continue;

							int passes = rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;
							for (int p = 0; p < passes; p++)
							{
								if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
									continue;

								if (_staticTextures.size() <= bucket.Texture)
								{
									TENLog("Attempted to set incorrect static mesh texture atlas", LogLevel::Warning);
									continue;
								}

								BindTexture(TextureRegister::ColorMap,
									&std::get<0>(_staticTextures[bucket.Texture]),
									SamplerStateRegister::AnisotropicClamp);
								BindTexture(TextureRegister::NormalMap,
									&std::get<1>(_staticTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

								DrawIndexedInstancedTriangles(bucket.NumIndices, instancesCount, bucket.StartIndex, 0);

								_numInstancedStaticsDrawCalls++;
							}
						}
					}
				}
			}
#endif
		}
		else
		{
			// Collect sorted blend modes faces ordered by room if doing transparent pass.
			for (auto it = view.SortedStaticsToDraw.begin(); it != view.SortedStaticsToDraw.end(); it++)
			{
				auto statics = it->second;

				auto* refStatic = statics[0];
				auto& refStaticObj = GetStaticRendererObject(refStatic->ObjectNumber);
				if (refStaticObj.ObjectMeshes.size() == 0)
					continue;

				auto* refMesh = refStaticObj.ObjectMeshes[0];

				for (int i = 0; i < statics.size(); i++)
				{
					for (int j = 0; j < refMesh->Buckets.size(); j++)
					{
						auto& bucket = refMesh->Buckets[j];

						if (bucket.NumVertices == 0)
							continue;

						auto blendMode = GetBlendModeFromAlpha(bucket.BlendMode, statics[i]->Color.w);

						if (IsSortedBlendMode(blendMode) || statics[i]->Color.w < ALPHA_BLEND_THRESHOLD)
						{
							for (int p = 0; p < bucket.Polygons.size(); p++)
							{
								auto object = RendererSortableObject{};

								object.ObjectType = RendererObjectType::Static;
								object.Bucket = &bucket;
								object.Static = statics[i];
								object.Centre = Vector3::Transform(bucket.Polygons[p].Centre, statics[i]->World);
								object.Distance = Vector3::Distance(object.Centre, view.Camera.WorldPosition);
								object.BlendMode = blendMode;
								object.LightMode = refMesh->LightMode;
								object.Polygon = &bucket.Polygons[p];
								object.Room = &_rooms[object.Static->RoomNumber];

								view.TransparentObjectsToDraw.push_back(object);
							}
						}
					}
				}
			}
		}
	}

	void Renderer::DrawRooms(RenderView& view, RendererPass rendererPass)
	{
		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			for (int i = (int)view.RoomsToDraw.size() - 1; i >= 0; i--)
			{
				int index = i;
				RendererRoom* room = view.RoomsToDraw[index];

				for (int animated = 0; animated < 2; animated++)
				{
					for (int j = 0; j < room->Buckets.size(); j++)
					{
						auto& bucket = room->Buckets[j];

						if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
						{
							continue;
						}

						if (rendererPass == RendererPass::CollectTransparentFaces)
						{
							if (IsSortedBlendMode(bucket.BlendMode))
							{
								for (int p = 0; p < bucket.Polygons.size(); p++)
								{
									RendererSortableObject object;

									object.ObjectType = RendererObjectType::Room;
									object.Centre = bucket.Centre;
									object.Distance = Vector3::Distance(view.Camera.WorldPosition, bucket.Polygons[p].Centre);
									object.BlendMode = bucket.BlendMode;
									object.Bucket = &bucket;
									object.Room = room;
									object.Polygon = &bucket.Polygons[p];

									view.TransparentObjectsToDraw.push_back(object);
								}
							}
						}
					}
				}
			}
		}
		else
		{
			if (rendererPass == RendererPass::GBuffer)
			{
				_shaders.Bind(Shader::GBuffer);
			}
			else
			{
				_shaders.Bind(Shader::Rooms);
			}

			UINT stride = sizeof(Vertex);
			UINT offset = 0;

			// Bind vertex and index buffer.
			_context->IASetVertexBuffers(0, 1, _roomsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetIndexBuffer(_roomsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			   
			if (rendererPass != RendererPass::GBuffer)
			{
				// Bind caustics texture.
				if (_causticTextures.size() > 0)
				{     
					int nmeshes = -Objects[ID_CAUSTIC_TEXTURES].nmeshes;
					int meshIndex = Objects[ID_CAUSTIC_TEXTURES].meshIndex;
					int causticsFrame = GlobalCounter % _causticTextures.size();
					BindTexture(TextureRegister::CausticsMap, &_causticTextures[causticsFrame], SamplerStateRegister::AnisotropicClamp);
				} 

				// Set shadow map data and bind shadow map texture.
				if (_shadowLight != nullptr)
				{
					BindLight(*_shadowLight, &_stShadowMap.Light, 0);
					_stShadowMap.ShadowMapSize = g_Configuration.ShadowMapSize;
					_stShadowMap.CastShadows = true;

					BindTexture(TextureRegister::ShadowMap, &_shadowMap, SamplerStateRegister::ShadowMap);
				}
				else
				{
					_stShadowMap.CastShadows = false;
				}

				_cbShadowMap.UpdateData(_stShadowMap, _context.Get());
			}

			if (g_Configuration.EnableAmbientOcclusion && rendererPass != RendererPass::GBuffer)
			{
				BindRenderTargetAsTexture(TextureRegister::SSAO, &_SSAOBlurredRenderTarget, SamplerStateRegister::PointWrap);
			}

			for (int i = (int)view.RoomsToDraw.size() - 1; i >= 0; i--)
			{
				const auto& room = *view.RoomsToDraw[i];
				const auto& nativeRoom = g_Level.Rooms[room.RoomNumber];				

				if (rendererPass != RendererPass::GBuffer)
				{
					_stRoom.Caustics = int(g_Configuration.EnableCaustics && (nativeRoom.flags & ENV_FLAG_WATER));
					_stRoom.AmbientColor = room.AmbientLight;
					BindRoomLights(view.LightsToDraw);
				}

				_stRoom.Water = (nativeRoom.flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
				_cbRoom.UpdateData(_stRoom, _context.Get());

				SetScissor(room.ClipBounds);

				for (int animated = 0; animated < 2; animated++)
				{
					if (rendererPass != RendererPass::GBuffer)
					{
						if (animated == 0) _shaders.Bind(Shader::Rooms); else _shaders.Bind(Shader::RoomsAnimated);
					}
					else
					{
						if (animated == 0) _shaders.Bind(Shader::GBufferRooms); else _shaders.Bind(Shader::GBufferRoomsAnimated);
					}

					for (const auto& bucket : room.Buckets)
					{
						if ((animated == 1) ^ bucket.Animated)
							continue;

						if (bucket.NumVertices == 0)
							continue;

						int passes = rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;

						for (int p = 0; p < passes; p++)
						{
							if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
								continue;

							// Draw geometry.
							if (animated)
							{
								BindTexture(TextureRegister::ColorMap,
									&std::get<0>(_animatedTextures[bucket.Texture]),
									SamplerStateRegister::AnisotropicClamp);
								BindTexture(TextureRegister::NormalMap,
									&std::get<1>(_animatedTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

								const auto& set = _animatedTextureSets[bucket.Texture];
								_stAnimated.NumFrames = set.NumTextures;
								_stAnimated.Type = 0;
								_stAnimated.Fps = set.Fps;

								for (unsigned char j = 0; j < set.NumTextures; j++)
								{
									if (j >= _stAnimated.Textures.size())
									{
										TENLog("Animated frame " + std::to_string(j) + " out of bounds. Too many frames in sequence.");
										break;
									}

									_stAnimated.Textures[j].TopLeft = set.Textures[j].UV[0];
									_stAnimated.Textures[j].TopRight = set.Textures[j].UV[1];
									_stAnimated.Textures[j].BottomRight = set.Textures[j].UV[2];
									_stAnimated.Textures[j].BottomLeft = set.Textures[j].UV[3];
								}
								_cbAnimated.UpdateData(_stAnimated, _context.Get());
							}
							else
							{
								BindTexture(
									TextureRegister::ColorMap, &std::get<0>(_roomTextures[bucket.Texture]),
									SamplerStateRegister::AnisotropicClamp);
								BindTexture(
									TextureRegister::NormalMap, &std::get<1>(_roomTextures[bucket.Texture]),
									SamplerStateRegister::AnisotropicClamp);
							}

							DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

							_numRoomsDrawCalls++;
						}
					}
				}
			}

			ResetScissor();
		}
	}
	
	void Renderer::DrawHorizonAndSky(RenderView& renderView, ID3D11DepthStencilView* depthTarget)
	{
		constexpr auto STAR_SIZE = 2;
		constexpr auto SUN_SIZE	 = 64;

		auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);

		bool anyOutsideRooms = false;
		for (int k = 0; k < renderView.RoomsToDraw.size(); k++)
		{
			const auto& nativeRoom = g_Level.Rooms[renderView.RoomsToDraw[k]->RoomNumber];
			if (nativeRoom.flags & ENV_FLAG_SKYBOX)
			{
				anyOutsideRooms = true;
				break;
			}
		}

		if ((!levelPtr->GetHorizonEnabled(0) && !levelPtr->GetHorizonEnabled(1)) || !anyOutsideRooms)
			return;

		if (Lara.Control.Look.OpticRange != 0)
			AlterFOV(ANGLE(DEFAULT_FOV) - Lara.Control.Look.OpticRange, false);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		// Draw sky.
		auto rotation = Matrix::CreateRotationX(PI);

		_shaders.Bind(Shader::Sky);
		BindTexture(TextureRegister::ColorMap, &_skyTexture, SamplerStateRegister::AnisotropicClamp);

		_context->IASetVertexBuffers(0, 1, _skyVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetIndexBuffer(_skyIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		SetBlendMode(BlendMode::Additive);

		for (int layer = 0; layer < 2; layer++)
		{
			if (Vector3(Weather.SkyColor(layer)) == Vector3::Zero)
				continue;

			for (int i = 0; i < 2; i++)
			{

				auto translation = Matrix::CreateTranslation(
					renderView.Camera.WorldPosition.x + Weather.SkyPosition(layer) - i * SKY_SIZE,
					renderView.Camera.WorldPosition.y - 1536.0f, 
					renderView.Camera.WorldPosition.z);
				auto world = rotation * translation;

				_stStatic.World = (rotation * translation);
				_stStatic.Color = Weather.SkyColor(layer);
				_stStatic.ApplyFogBulbs = layer == 0 ? 1 : 0;
				_cbStatic.UpdateData(_stStatic, _context.Get());

				DrawIndexedTriangles(SKY_INDICES_COUNT, 0, 0);

				_numMoveablesDrawCalls++;
			}
		}

		_context->ClearDepthStencilView(depthTarget, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

		if (Weather.GetStars().size() > 0)
		{
			SetDepthState(DepthState::Read);
			SetBlendMode(BlendMode::Additive);
			SetCullMode(CullMode::None);

			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			_shaders.Bind(Shader::InstancedSprites);

			// Set up vertex buffer and parameters.
			UINT stride = sizeof(Vertex);
			UINT offset = 0;
			_context->IASetVertexBuffers(0, 1, _quadVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

			BindTexture(TextureRegister::ColorMap, _sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_LENS_FLARE_3].Texture, SamplerStateRegister::LinearClamp);

			int drawnStars = 0;
			int starCount = (int)Weather.GetStars().size();

			while (drawnStars < starCount)
			{
				int starsToDraw =
					(starCount - drawnStars) > INSTANCED_SPRITES_BUCKET_SIZE ? 
					INSTANCED_SPRITES_BUCKET_SIZE : 
					(starCount - drawnStars);
				int i = 0;

				for (int i = 0; i < starsToDraw; i++)
				{
					auto& star = Weather.GetStars()[drawnStars + i];

					RendererSpriteToDraw rDrawSprite;
					rDrawSprite.Sprite = &_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_LENS_FLARE_3];

					rDrawSprite.Type = SpriteType::Billboard;
					rDrawSprite.pos = renderView.Camera.WorldPosition + star.Direction * BLOCK(1);
					rDrawSprite.Rotation = 0;
					rDrawSprite.Scale = 1;
					rDrawSprite.Width = STAR_SIZE * star.Scale;
					rDrawSprite.Height = STAR_SIZE * star.Scale;

					_stInstancedSpriteBuffer.Sprites[i].World = GetWorldMatrixForSprite(rDrawSprite, renderView);
					_stInstancedSpriteBuffer.Sprites[i].Color = Vector4(
						star.Color.x,
						star.Color.y,
						star.Color.z,
						star.Blinking * star.Extinction);
					_stInstancedSpriteBuffer.Sprites[i].IsBillboard = 1;
					_stInstancedSpriteBuffer.Sprites[i].IsSoftParticle = 0;

					// NOTE: Strange packing due to particular HLSL 16 byte alignment requirements.
					_stInstancedSpriteBuffer.Sprites[i].UV[0].x = rDrawSprite.Sprite->UV[0].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].y = rDrawSprite.Sprite->UV[1].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].z = rDrawSprite.Sprite->UV[2].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].w = rDrawSprite.Sprite->UV[3].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].x = rDrawSprite.Sprite->UV[0].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].y = rDrawSprite.Sprite->UV[1].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].z = rDrawSprite.Sprite->UV[2].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].w = rDrawSprite.Sprite->UV[3].y;
				}

				_cbInstancedSpriteBuffer.UpdateData(_stInstancedSpriteBuffer, _context.Get());

				// Draw sprites with instancing.
				DrawInstancedTriangles(4, starsToDraw, 0);

				drawnStars += starsToDraw;
			}

			// Draw meteors
			if (Weather.GetMeteors().size() > 0)
			{
				RendererSpriteToDraw rDrawSprite;
				rDrawSprite.Sprite = &_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_LENS_FLARE_3];
				BindTexture(TextureRegister::ColorMap, rDrawSprite.Sprite->Texture, SamplerStateRegister::LinearClamp);

				int drawnMeteors = 0;
				int meteorsCount = (int)Weather.GetMeteors().size();

				while (drawnMeteors < meteorsCount)
				{
					int meteorsToDraw =
						(meteorsCount - drawnMeteors) > INSTANCED_SPRITES_BUCKET_SIZE ?
						INSTANCED_SPRITES_BUCKET_SIZE :
						(meteorsCount - drawnMeteors);
					int i = 0;

					for (int i = 0; i < meteorsToDraw; i++)
					{
						auto meteor = Weather.GetMeteors()[drawnMeteors + i];

						if (meteor.Active == false)
							continue;

						rDrawSprite.Type = SpriteType::CustomBillboard;
						rDrawSprite.pos =
							renderView.Camera.WorldPosition +
							Vector3::Lerp(meteor.PrevPosition, meteor.Position, GetInterpolationFactor());
						rDrawSprite.Rotation = 0;
						rDrawSprite.Scale = 1;
						rDrawSprite.Width = 2;
						rDrawSprite.Height = 192;
						rDrawSprite.ConstrainAxis = meteor.Direction;

						_stInstancedSpriteBuffer.Sprites[i].World = GetWorldMatrixForSprite(rDrawSprite, renderView);
						_stInstancedSpriteBuffer.Sprites[i].Color = Vector4(
							meteor.Color.x,
							meteor.Color.y,
							meteor.Color.z,
							Lerp(meteor.PrevFade, meteor.Fade, GetInterpolationFactor()));
						_stInstancedSpriteBuffer.Sprites[i].IsBillboard = 1;
						_stInstancedSpriteBuffer.Sprites[i].IsSoftParticle = 0;

						// NOTE: Strange packing due to particular HLSL 16 byte alignment requirements.
						_stInstancedSpriteBuffer.Sprites[i].UV[0].x = rDrawSprite.Sprite->UV[0].x;
						_stInstancedSpriteBuffer.Sprites[i].UV[0].y = rDrawSprite.Sprite->UV[1].x;
						_stInstancedSpriteBuffer.Sprites[i].UV[0].z = rDrawSprite.Sprite->UV[2].x;
						_stInstancedSpriteBuffer.Sprites[i].UV[0].w = rDrawSprite.Sprite->UV[3].x;
						_stInstancedSpriteBuffer.Sprites[i].UV[1].x = rDrawSprite.Sprite->UV[0].y;
						_stInstancedSpriteBuffer.Sprites[i].UV[1].y = rDrawSprite.Sprite->UV[1].y;
						_stInstancedSpriteBuffer.Sprites[i].UV[1].z = rDrawSprite.Sprite->UV[2].y;
						_stInstancedSpriteBuffer.Sprites[i].UV[1].w = rDrawSprite.Sprite->UV[3].y;
					}

					_cbInstancedSpriteBuffer.UpdateData(_stInstancedSpriteBuffer, _context.Get());

					// Draw sprites with instancing.
					DrawInstancedTriangles(4, meteorsToDraw, 0);

					drawnMeteors += meteorsToDraw;
				}
			}

			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		// Draw horizon.
		for (int layer = 0; layer < 2; layer++)
		{
			if (!levelPtr->GetHorizonEnabled(layer) || levelPtr->GetHorizonTransparency(layer) <= EPSILON)
				continue;

			SetDepthState(DepthState::None);
			SetBlendMode(BlendMode::Opaque);
			SetCullMode(CullMode::CounterClockwise);

			_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
			_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			_shaders.Bind(Shader::Sky);

			auto pos = Vector3::Lerp(levelPtr->GetHorizonPrevPosition(layer), levelPtr->GetHorizonPosition(layer), GetInterpolationFactor());
			auto orient = EulerAngles::Lerp(levelPtr->GetHorizonPrevOrientation(layer), levelPtr->GetHorizonOrientation(layer), GetInterpolationFactor());
			auto rotMatrix = orient.ToRotationMatrix();
			auto translationMatrix = Matrix::CreateTranslation(pos);
			auto cameraMatrix = Matrix::CreateTranslation(renderView.Camera.WorldPosition);

			float alpha = levelPtr->GetHorizonTransparency(layer);

			_stStatic.World = rotMatrix * translationMatrix * cameraMatrix;
			_stStatic.Color = Color(1.0f, 1.0f, 1.0f, alpha);
			_stStatic.ApplyFogBulbs = 1;
			_cbStatic.UpdateData(_stStatic, _context.Get());

			const auto& moveableObj = *_moveableObjects[levelPtr->GetHorizonObjectID(layer)];
			for (const auto* mesh : moveableObj.ObjectMeshes)
			{
				for (const auto& bucket : mesh->Buckets)
				{
					if (bucket.NumVertices == 0)
						continue;

					BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

					// Always render horizon as alpha-blended surface.
					SetBlendMode(GetBlendModeFromAlpha((bucket.BlendMode == BlendMode::AlphaTest) ? BlendMode::AlphaBlend : bucket.BlendMode, alpha));
					SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

					// Draw vertices.
					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

					_numMoveablesDrawCalls++;
				}
			}
		}

		// Eventually draw the sun sprite.
		if (!renderView.LensFlaresToDraw.empty() && renderView.LensFlaresToDraw[0].IsGlobal)
		{
			SetDepthState(DepthState::Read);
			SetBlendMode(BlendMode::Additive);
			SetCullMode(CullMode::None);

			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			_shaders.Bind(Shader::InstancedSprites);

			// Set up vertex buffer and parameters.
			unsigned int stride = sizeof(Vertex);
			unsigned int offset = 0;
			_context->IASetVertexBuffers(0, 1, _quadVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

			auto rDrawSprite = RendererSpriteToDraw{};
			rDrawSprite.Sprite = &_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + renderView.LensFlaresToDraw[0].SpriteID];

			rDrawSprite.Type = SpriteType::Billboard;
			rDrawSprite.pos = renderView.Camera.WorldPosition + renderView.LensFlaresToDraw[0].Direction * BLOCK(1);
			rDrawSprite.Rotation = 0.0f;
			rDrawSprite.Scale = 1.0f;
			rDrawSprite.Width = SUN_SIZE;
			rDrawSprite.Height = SUN_SIZE;
			rDrawSprite.color = renderView.LensFlaresToDraw[0].Color;

			_stInstancedSpriteBuffer.Sprites[0].World = GetWorldMatrixForSprite(rDrawSprite, renderView);
			_stInstancedSpriteBuffer.Sprites[0].Color = renderView.LensFlaresToDraw[0].Color;
			_stInstancedSpriteBuffer.Sprites[0].IsBillboard = 1;
			_stInstancedSpriteBuffer.Sprites[0].IsSoftParticle = 0;

			// NOTE: Strange packing due to particular HLSL 16 byte alignment requirements.
			_stInstancedSpriteBuffer.Sprites[0].UV[0].x = rDrawSprite.Sprite->UV[0].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].y = rDrawSprite.Sprite->UV[1].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].z = rDrawSprite.Sprite->UV[2].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].w = rDrawSprite.Sprite->UV[3].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].x = rDrawSprite.Sprite->UV[0].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].y = rDrawSprite.Sprite->UV[1].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].z = rDrawSprite.Sprite->UV[2].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].w = rDrawSprite.Sprite->UV[3].y;

			BindTexture(TextureRegister::ColorMap, rDrawSprite.Sprite->Texture, SamplerStateRegister::LinearClamp);

			_cbInstancedSpriteBuffer.UpdateData(_stInstancedSpriteBuffer, _context.Get());

			// Draw sprites with instancing.
			DrawInstancedTriangles(4, 1, 0);

			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		// Clear just the Z-buffer to start drawing on top of horizon.
		_context->ClearDepthStencilView(depthTarget, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void Renderer::Render(float interpFactor)
	{
		InterpolateCamera(interpFactor);
		RenderScene(&_backBuffer, _gameCamera);

		_context->ClearState();
		_swapChain->Present(1, 0);
	}

	void Renderer::DrawMoveableMesh(RendererItem* itemToDraw, RendererMesh* mesh, RendererRoom* room, int boneIndex, RenderView& view, RendererPass rendererPass)
	{
		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			for (auto& bucket : mesh->Buckets)
			{
				if (bucket.NumVertices == 0)
					continue;

				auto blendMode = GetBlendModeFromAlpha(bucket.BlendMode, itemToDraw->Color.w);

				if (IsSortedBlendMode(blendMode))
				{
					for (int p = 0; p < bucket.Polygons.size(); p++)
					{
						auto center = Vector3::Transform(bucket.Polygons[p].Centre, itemToDraw->InterpolatedAnimTransforms[boneIndex] * itemToDraw->InterpolatedWorld);
						int dist = Vector3::Distance(center, Camera.pos.ToVector3());

						auto object = RendererSortableObject{};
						object.ObjectType = RendererObjectType::Moveable;
						object.Centre = center;
						object.Distance = dist;
						object.BlendMode = blendMode;
						object.Bucket = &bucket;
						object.Item = itemToDraw;
						object.LightMode = mesh->LightMode;
						object.Polygon = &bucket.Polygons[p];

						view.TransparentObjectsToDraw.push_back(object);
					}
				}
			}
		}
		else
		{
			for (auto& bucket : mesh->Buckets)
			{
				if (bucket.NumVertices == 0)
					continue;

				auto blendMode = GetBlendModeFromAlpha(bucket.BlendMode, itemToDraw->Color.w);
				
				if (rendererPass == RendererPass::ShadowMap)
				{
					SetBlendMode(BlendMode::Opaque);
					SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

					DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

					_numShadowMapDrawCalls++;
				}
				else
				{
					int passes = rendererPass == RendererPass::Opaque && blendMode == BlendMode::AlphaTest ? 2 : 1;

					for (int p = 0; p < passes; p++)
					{
						if (!SetupBlendModeAndAlphaTest(blendMode, rendererPass, p))
							continue;

						BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
						BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

						DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

						_numMoveablesDrawCalls++;
					}
				}
			}
		}
	}

	bool Renderer::SetupBlendModeAndAlphaTest(BlendMode blendMode, RendererPass rendererPass, int drawPass)
	{
		switch (rendererPass)
		{
		case RendererPass::GBuffer:
			if (blendMode != BlendMode::Opaque &&
				blendMode != BlendMode::AlphaTest &&
				blendMode != BlendMode::FastAlphaBlend)
			{
				return false;
			}

			if (blendMode == BlendMode::Opaque)
			{
				SetBlendMode(BlendMode::Opaque);
				SetAlphaTest(AlphaTestMode::None, 1.0f);
			}
			else
			{
				SetBlendMode(BlendMode::AlphaTest);
				SetAlphaTest(AlphaTestMode::GreatherThan, FAST_ALPHA_BLEND_THRESHOLD);
			}
			break;

		case RendererPass::Opaque:
			if (blendMode != BlendMode::Opaque &&
				blendMode != BlendMode::AlphaTest)
			{
				return false;
			}

			if (blendMode == BlendMode::Opaque)
			{
				SetBlendMode(BlendMode::Opaque);
				SetAlphaTest(AlphaTestMode::None, 1.0f);
			}
			else
			{
				if (drawPass == 0)
				{
					SetBlendMode(BlendMode::Opaque);
					SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);
				}
				else
				{
					SetBlendMode(BlendMode::AlphaBlend);
					SetAlphaTest(AlphaTestMode::LessThan, ALPHA_TEST_THRESHOLD);
				};
			}
			break;

		case RendererPass::Additive:
			if (blendMode != BlendMode::Additive)
			{
				return false;
			}

			SetBlendMode(BlendMode::Additive);
			SetAlphaTest(AlphaTestMode::None, 1.0f);
			break;

		default:
			return false;

		}

		return true;
	}

	void Renderer::SortTransparentFaces(RenderView& view)
	{
		std::sort(
			view.TransparentObjectsToDraw.begin(),
			view.TransparentObjectsToDraw.end(),
			[](RendererSortableObject& a, RendererSortableObject& b)
			{
				return (a.Distance > b.Distance);
			}
		);
	}

	void Renderer::DrawSortedFaces(RenderView& view)
	{
		for (int i = 0; i < view.TransparentObjectsToDraw.size(); i++)
		{
			auto* object = &view.TransparentObjectsToDraw[i];
			auto lastObjectType = (i > 0 ? view.TransparentObjectsToDraw[i - 1].ObjectType : RendererObjectType::Unknown);

			_sortedPolygonsVertices.clear();
			_sortedPolygonsIndices.clear();

			if (_currentMirror != nullptr && object->ObjectType == RendererObjectType::Room)
				continue;

			if (object->ObjectType == RendererObjectType::Room)
			{
				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].ObjectType == object->ObjectType &&
					view.TransparentObjectsToDraw[i].Room->RoomNumber == object->Room->RoomNumber &&
					view.TransparentObjectsToDraw[i].Bucket->Texture == object->Bucket->Texture &&
					view.TransparentObjectsToDraw[i].BlendMode == object->BlendMode &&
					_sortedPolygonsIndices.size() + (view.TransparentObjectsToDraw[i].Polygon->Shape == 0 ? 6 : 3) < MAX_TRANSPARENT_VERTICES)
				{
					auto* currentObject = &view.TransparentObjectsToDraw[i];
					_sortedPolygonsIndices.bulk_push_back(
						_roomsIndices.data(),
						currentObject->Polygon->BaseIndex,
						currentObject->Polygon->Shape == 0 ? 6 : 3);
					i++;
				}

				DrawRoomSorted(object, lastObjectType, view);

				if (i == view.TransparentObjectsToDraw.size())
					return;

				i--;
			}
			else if (object->ObjectType == RendererObjectType::Moveable)
			{
				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].ObjectType == object->ObjectType &&
					view.TransparentObjectsToDraw[i].Item->ItemNumber == object->Item->ItemNumber &&
					view.TransparentObjectsToDraw[i].Bucket->Texture == object->Bucket->Texture &&
					view.TransparentObjectsToDraw[i].BlendMode == object->BlendMode &&
					_sortedPolygonsIndices.size() + (view.TransparentObjectsToDraw[i].Polygon->Shape == 0 ? 6 : 3) < MAX_TRANSPARENT_VERTICES)
				{
					auto* currentObject = &view.TransparentObjectsToDraw[i];
					_sortedPolygonsIndices.bulk_push_back(
						_moveablesIndices.data(),
						currentObject->Polygon->BaseIndex,
						currentObject->Polygon->Shape == 0 ? 6 : 3);
					i++;
				}

				DrawItemSorted(object, lastObjectType, view);

				if (i == view.TransparentObjectsToDraw.size())
					return;

				i--;
			}
			else if (object->ObjectType == RendererObjectType::Static)
			{
				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].ObjectType == object->ObjectType &&
					view.TransparentObjectsToDraw[i].Static->RoomNumber == object->Static->RoomNumber &&
					view.TransparentObjectsToDraw[i].Static->IndexInRoom == object->Static->IndexInRoom &&
					view.TransparentObjectsToDraw[i].Bucket->Texture == object->Bucket->Texture &&
					view.TransparentObjectsToDraw[i].BlendMode == object->BlendMode &&
					_sortedPolygonsIndices.size() + (view.TransparentObjectsToDraw[i].Polygon->Shape == 0 ? 6 : 3) < MAX_TRANSPARENT_VERTICES)
				{
					auto* currentObject = &view.TransparentObjectsToDraw[i];
					_sortedPolygonsIndices.bulk_push_back(
						_staticsIndices.data(),
						currentObject->Polygon->BaseIndex,
						currentObject->Polygon->Shape == 0 ? 6 : 3);
					i++;
				}

				DrawStaticSorted(object, lastObjectType, view);

				if (i == view.TransparentObjectsToDraw.size())
					return;

				i--;
			}
			else if (object->ObjectType == RendererObjectType::MoveableAsStatic)
			{
				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].ObjectType == object->ObjectType &&
					view.TransparentObjectsToDraw[i].Room->RoomNumber == object->Room->RoomNumber &&
					view.TransparentObjectsToDraw[i].Bucket->Texture == object->Bucket->Texture &&
					view.TransparentObjectsToDraw[i].BlendMode == object->BlendMode &&
					_sortedPolygonsIndices.size() + (view.TransparentObjectsToDraw[i].Polygon->Shape == 0 ? 6 : 3) < MAX_TRANSPARENT_VERTICES)
				{
					auto* currentObject = &view.TransparentObjectsToDraw[i];
					_sortedPolygonsIndices.bulk_push_back(
						_staticsIndices.data(),
						currentObject->Polygon->BaseIndex,
						currentObject->Polygon->Shape == 0 ? 6 : 3);
					i++;
				}

				DrawMoveableAsStaticSorted(object, lastObjectType, view);

				if (i == view.TransparentObjectsToDraw.size())
					return;

				i--;
			}
			else if (object->ObjectType == RendererObjectType::Sprite)
			{			
				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].ObjectType == object->ObjectType &&
					view.TransparentObjectsToDraw[i].Sprite->Type == object->Sprite->Type &&
					view.TransparentObjectsToDraw[i].Sprite->SoftParticle == object->Sprite->SoftParticle &&
					view.TransparentObjectsToDraw[i].Sprite->Sprite->Texture == object->Sprite->Sprite->Texture &&
					view.TransparentObjectsToDraw[i].Sprite->BlendMode == object->Sprite->BlendMode &&
					_sortedPolygonsIndices.size() + 6 < MAX_TRANSPARENT_VERTICES)
				{
					RendererSortableObject* currentObject = &view.TransparentObjectsToDraw[i];
					RendererSpriteToDraw* spr = currentObject->Sprite;

					Vector3 p0t;
					Vector3 p1t;
					Vector3 p2t;
					Vector3 p3t;

					Vector2 uv0;
					Vector2 uv1;
					Vector2 uv2;
					Vector2 uv3;

					if (spr->Type == SpriteType::ThreeD)
					{
						p0t = spr->vtx1;
						p1t = spr->vtx2;
						p2t = spr->vtx3;
						p3t = spr->vtx4;


					}
					else
					{
						p0t = Vector3(-0.5, 0.5, 0);
						p1t = Vector3(0.5, 0.5, 0);
						p2t = Vector3(0.5, -0.5, 0);
						p3t = Vector3(-0.5, -0.5, 0);
					}

					uv0 = spr->Sprite->UV[0];
					uv1 = spr->Sprite->UV[1];
					uv2 = spr->Sprite->UV[2];
					uv3 = spr->Sprite->UV[3];

					auto world = GetWorldMatrixForSprite(*currentObject->Sprite, view);
					
					Vertex v0;
					v0.Position = Vector3::Transform(p0t, world);
					v0.UV = uv0;
					v0.Color = spr->c1;

					Vertex v1;
					v1.Position = Vector3::Transform(p1t, world);
					v1.UV = uv1;
					v1.Color = spr->c2;

					Vertex v2;
					v2.Position = Vector3::Transform(p2t, world);
					v2.UV = uv2;
					v2.Color = spr->c3;

					Vertex v3;
					v3.Position = Vector3::Transform(p3t, world);
					v3.UV = uv3;
					v3.Color = spr->c4;

					_sortedPolygonsVertices.push_back(v0);
					_sortedPolygonsVertices.push_back(v1);
					_sortedPolygonsVertices.push_back(v3);
					_sortedPolygonsVertices.push_back(v2);
					_sortedPolygonsVertices.push_back(v3);
					_sortedPolygonsVertices.push_back(v1);

					i++;
				}

				DrawSpriteSorted(object, lastObjectType, view);

				if (i == view.TransparentObjectsToDraw.size())
				{
					return;
				}

				i--;
			}
		}
	}

	void Renderer::DrawRoomSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view)
	{
		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::CounterClockwise);

		RoomData* nativeRoom = &g_Level.Rooms[objectInfo->Room->RoomNumber];

		_shaders.Bind(Shader::Rooms);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		_context->IASetVertexBuffers(0, 1, _roomsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_stRoom.Caustics = (int)(g_Configuration.EnableCaustics && (nativeRoom->flags & ENV_FLAG_WATER));
		_stRoom.AmbientColor = objectInfo->Room->AmbientLight;
		BindRoomLights(view.LightsToDraw);
		_stRoom.Water = (nativeRoom->flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
		_cbRoom.UpdateData(_stRoom, _context.Get());

		SetScissor(objectInfo->Room->ClipBounds);

		if (objectInfo->Bucket->Animated != 0)
			_shaders.Bind(Shader::RoomsAnimated);

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		// Draw geometry
		if (objectInfo->Bucket->Animated)
		{
			BindTexture(TextureRegister::ColorMap,
				&std::get<0>(_animatedTextures[objectInfo->Bucket->Texture]),
				SamplerStateRegister::AnisotropicClamp);
			BindTexture(TextureRegister::NormalMap,
				&std::get<1>(_animatedTextures[objectInfo->Bucket->Texture]), SamplerStateRegister::AnisotropicClamp);

			RendererAnimatedTextureSet& set = _animatedTextureSets[objectInfo->Bucket->Texture];
			_stAnimated.NumFrames = set.NumTextures;
			_stAnimated.Type = 0;
			_stAnimated.Fps = set.Fps;

			for (unsigned char j = 0; j < set.NumTextures; j++)
			{
				if (j >= _stAnimated.Textures.size())
				{
					TENLog("Animated frame " + std::to_string(j) + " is out of bounds, too many frames in sequence.");
					break;
				}

				_stAnimated.Textures[j].TopLeft = set.Textures[j].UV[0];
				_stAnimated.Textures[j].TopRight = set.Textures[j].UV[1];
				_stAnimated.Textures[j].BottomRight = set.Textures[j].UV[2];
				_stAnimated.Textures[j].BottomLeft = set.Textures[j].UV[3];
			}
			_cbAnimated.UpdateData(_stAnimated, _context.Get());
		}
		else
		{
			BindTexture(TextureRegister::ColorMap, &std::get<0>(_roomTextures[objectInfo->Bucket->Texture]),
				SamplerStateRegister::AnisotropicClamp);
			BindTexture(TextureRegister::NormalMap,
				&std::get<1>(_roomTextures[objectInfo->Bucket->Texture]), SamplerStateRegister::AnisotropicClamp);
		}

		_sortedPolygonsIndexBuffer.Update(_context.Get(), _sortedPolygonsIndices, 0, (int)_sortedPolygonsIndices.size());
		_context->IASetIndexBuffer(_sortedPolygonsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		DrawIndexedTriangles((int)_sortedPolygonsIndices.size(), 0, 0);

		_numSortedRoomsDrawCalls++;
		_numSortedTriangles += (int)_sortedPolygonsIndices.size() / 3;

		ResetScissor();
	}

	void Renderer::DrawItemSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view)
	{
		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;
		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::CounterClockwise);
		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		_shaders.Bind(Shader::Items);

		// Bind main item properties.
		Matrix world = objectInfo->Item->InterpolatedWorld;
		_stItem.World = world;
		_stItem.Color = objectInfo->Item->Color;
		_stItem.AmbientLight = objectInfo->Item->AmbientLight;
		memcpy(_stItem.BonesMatrices, objectInfo->Item->InterpolatedAnimTransforms, sizeof(Matrix) * MAX_BONES);

		const auto& moveableObj = *_moveableObjects[objectInfo->Item->ObjectID];
		for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
			_stItem.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

		bool acceptsShadows = moveableObj.ShadowType == ShadowMode::None;
		BindMoveableLights(objectInfo->Item->LightsToDraw, objectInfo->Item->RoomNumber, objectInfo->Item->PrevRoomNumber, objectInfo->Item->LightFade, acceptsShadows);
		_cbItem.UpdateData(_stItem, _context.Get());

		BindTexture(
			TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[objectInfo->Bucket->Texture]),
			SamplerStateRegister::AnisotropicClamp);
		BindTexture(
			TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[objectInfo->Bucket->Texture]),
			SamplerStateRegister::AnisotropicClamp);

		_sortedPolygonsIndexBuffer.Update(_context.Get(), _sortedPolygonsIndices, 0, (int)_sortedPolygonsIndices.size());
		_context->IASetIndexBuffer(_sortedPolygonsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		DrawIndexedTriangles((int)_sortedPolygonsIndices.size(), 0, 0);

		_numSortedMoveablesDrawCalls++;
		_numSortedTriangles += (int)_sortedPolygonsIndices.size() / 3;
	}

	void Renderer::DrawStaticSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view)
	{
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		_context->IASetVertexBuffers(0, 1, _staticsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_shaders.Bind(Shader::Statics);

		auto world = objectInfo->Static->World;
		_stStatic.World = world;

		_stStatic.Color = objectInfo->Static->Color;
		_stStatic.AmbientLight = objectInfo->Room->AmbientLight;
		_stStatic.LightMode = (int)GetStaticRendererObject(objectInfo->Static->ObjectNumber).ObjectMeshes[0]->LightMode;
		BindStaticLights(objectInfo->Static->LightsToDraw);
		_cbStatic.UpdateData(_stStatic, _context.Get());

		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::CounterClockwise);
		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindTexture(TextureRegister::ColorMap, &std::get<0>(_staticTextures[objectInfo->Bucket->Texture]),
			SamplerStateRegister::AnisotropicClamp);
		BindTexture(TextureRegister::NormalMap, &std::get<1>(_staticTextures[objectInfo->Bucket->Texture]),
			SamplerStateRegister::AnisotropicClamp);

		_sortedPolygonsIndexBuffer.Update(_context.Get(), _sortedPolygonsIndices, 0, (int)_sortedPolygonsIndices.size());
		_context->IASetIndexBuffer(_sortedPolygonsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		DrawIndexedTriangles((int)_sortedPolygonsIndices.size(), 0, 0);

		_numSortedStaticsDrawCalls++;
		_numSortedTriangles += (int)_sortedPolygonsIndices.size() / 3;
	}

	void Renderer::DrawMoveableAsStaticSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view)
	{
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_shaders.Bind(Shader::Statics);

		auto world = objectInfo->World;
		_stStatic.World = world;

		_stStatic.Color = Vector4::One;
		_stStatic.AmbientLight = objectInfo->Room->AmbientLight;
		_stStatic.LightMode = (int)objectInfo->LightMode;
		BindStaticLights(objectInfo->Room->LightsToDraw);
		_cbStatic.UpdateData(_stStatic, _context.Get());

		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::CounterClockwise);
		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

		BindTexture(TextureRegister::ColorMap, &std::get<0>(_staticTextures[objectInfo->Bucket->Texture]),
			SamplerStateRegister::AnisotropicClamp);
		BindTexture(TextureRegister::NormalMap, &std::get<1>(_staticTextures[objectInfo->Bucket->Texture]),
			SamplerStateRegister::AnisotropicClamp);

		_sortedPolygonsIndexBuffer.Update(_context.Get(), _sortedPolygonsIndices, 0, (int)_sortedPolygonsIndices.size());
		_context->IASetIndexBuffer(_sortedPolygonsIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		DrawIndexedTriangles((int)_sortedPolygonsIndices.size(), 0, 0);

		_numSortedStaticsDrawCalls++;
		_numSortedTriangles += (int)_sortedPolygonsIndices.size() / 3;
	}

	void Renderer::CalculateSSAO(RenderView& view)
	{
		_doingFullscreenPass = true;

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::Write);

		// Common vertex shader to all full screen effects.
		_shaders.Bind(Shader::PostProcess);

		// SSAO pixel shader.
		_shaders.Bind(Shader::Ssao);

		_context->ClearRenderTargetView(_SSAORenderTarget.RenderTargetView.Get(), Colors::White);
		_context->OMSetRenderTargets(1, _SSAORenderTarget.RenderTargetView.GetAddressOf(), nullptr);

		// Must set correctly viewport because SSAO is done at 1/4 screen resolution.
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = _screenWidth;
		viewport.Height = _screenHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		_context->RSSetViewports(1, &viewport);
	
		D3D11_RECT rects[1];
		rects[0].left = 0;
		rects[0].right = viewport.Width;
		rects[0].top = 0;
		rects[0].bottom = viewport.Height;

		_context->RSSetScissorRects(1, rects);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), &_depthRenderTarget, SamplerStateRegister::PointWrap);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), &_normalsRenderTarget, SamplerStateRegister::PointWrap);
		BindTexture(static_cast<TextureRegister>(2), &_SSAONoiseTexture, SamplerStateRegister::PointWrap);

		_stPostProcessBuffer.ViewportWidth = viewport.Width;
		_stPostProcessBuffer.ViewportHeight = viewport.Height;
		memcpy(_stPostProcessBuffer.SSAOKernel, _SSAOKernel.data(), 16 * _SSAOKernel.size());
		_cbPostProcessBuffer.UpdateData(_stPostProcessBuffer, _context.Get());

		DrawTriangles(3, 0);

		// Blur step.
		_shaders.Bind(Shader::SsaoBlur);

		_context->ClearRenderTargetView(_SSAOBlurredRenderTarget.RenderTargetView.Get(), Colors::Black);
		_context->OMSetRenderTargets(1, _SSAOBlurredRenderTarget.RenderTargetView.GetAddressOf(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::SSAO, &_SSAORenderTarget, SamplerStateRegister::PointWrap);
 
		DrawTriangles(3, 0);

		_doingFullscreenPass = false;
	}

	void Renderer::InterpolateCamera(float interpFactor)
	{
		_interpolationFactor = interpFactor;

		// Interpolate camera.
		if (!Camera.DisableInterpolation)
		{
			_gameCamera.Camera.WorldPosition = Vector3::Lerp(_oldGameCamera.Camera.WorldPosition, _currentGameCamera.Camera.WorldPosition, interpFactor);
			_gameCamera.Camera.WorldDirection = Vector3::Lerp(_oldGameCamera.Camera.WorldDirection, _currentGameCamera.Camera.WorldDirection, interpFactor);
			_gameCamera.Camera.View = Matrix::Lerp(_oldGameCamera.Camera.View, _currentGameCamera.Camera.View, interpFactor);
			_gameCamera.Camera.Projection = Matrix::Lerp(_oldGameCamera.Camera.Projection, _currentGameCamera.Camera.Projection, interpFactor);
			_gameCamera.Camera.ViewProjection = _gameCamera.Camera.View * _gameCamera.Camera.Projection;
			_gameCamera.Camera.FOV = Lerp(_oldGameCamera.Camera.FOV, _currentGameCamera.Camera.FOV, interpFactor);
			_gameCamera.Camera.Frustum.Update(_gameCamera.Camera.View, _gameCamera.Camera.Projection);
		}
		else
		{
			_gameCamera.Camera.WorldPosition = _currentGameCamera.Camera.WorldPosition;
			_gameCamera.Camera.WorldDirection = _currentGameCamera.Camera.WorldDirection;
			_gameCamera.Camera.View = _currentGameCamera.Camera.View;
			_gameCamera.Camera.Projection = _currentGameCamera.Camera.Projection;
			_gameCamera.Camera.ViewProjection = _gameCamera.Camera.View * _gameCamera.Camera.Projection;
			_gameCamera.Camera.FOV = _currentGameCamera.Camera.FOV;
			_gameCamera.Camera.Frustum = _currentGameCamera.Camera.Frustum;
		}

		_gameCamera.Camera.ViewSize = _currentGameCamera.Camera.ViewSize;
		_gameCamera.Camera.InvViewSize = _currentGameCamera.Camera.InvViewSize;
		_gameCamera.Camera.NearPlane = _currentGameCamera.Camera.NearPlane;
		_gameCamera.Camera.FarPlane = _currentGameCamera.Camera.FarPlane;
	}
}
