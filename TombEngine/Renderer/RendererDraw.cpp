#include "framework.h"

#include <algorithm>
#include <chrono>
#include <execution>
#include <filesystem>

#include "ConstantBuffers/CameraMatrixBuffer.h"
#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/effects/hair.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/rope.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR4/Entity/Locust.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Renderer/RenderView.h"
#include "Renderer/Renderer.h"
#include "Renderer/Structures/RendererSortableObject.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Animation;
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
		for (int step = 0; step < _shadowMap->GetRenderTarget()->GetArraySize(); step++)
		{
			_graphicsDevice->ClearRenderTarget2D(_shadowMap->GetRenderTarget(), step, Colors::White);
			_graphicsDevice->ClearDepthStencil(_shadowMap->GetDepthTarget(), step, DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
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

		static constexpr Vector3 forwardVectors[6] =
		{
			
			Vector3(-1,  0,  0), // +X (right)
			Vector3( 1 , 0,  0), // -X (left)
			Vector3( 0, -1,  0), // -Y (up)
			Vector3( 0,  1,  0), // +Y (down)
			Vector3( 0,  0,  1), // +Z (forward)
			Vector3( 0,  0, -1), // -Z (backward)
		};

		static constexpr Vector3 upVectors[6] =
		{
			Vector3(0, -1,  0),
			Vector3(0, -1,  0),
			Vector3(0,  0, -1),
			Vector3(0,  0,  1),
			Vector3(0, -1,  0),
			Vector3(0, -1,  0),
		};

		for (int step = 0; step < 6; step++)
		{
			// Bind render target.
			_graphicsDevice->BindRenderTarget(
				IRenderTargetBinding(_shadowMap->GetRenderTarget(), step),
				IDepthTargetBinding(_shadowMap->GetDepthTarget(), step));
			_graphicsDevice->SetViewport(_shadowMapViewport);
			_graphicsDevice->SetScissor(_shadowMapViewport);

			if (shadowLightPos == item->Position)
				return;

			// Set shaders.
			_shaders.Bind(Shader::ShadowMap);

			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
			_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

			// Set texture.
			BindTexture(TextureRegister::ColorMap, std::get<0>(_moveablesTextures[0]).get(), SamplerStateRegister::AnisotropicClamp);
			BindTexture(TextureRegister::NormalMap, std::get<1>(_moveablesTextures[0]).get(), SamplerStateRegister::AnisotropicClamp);

			// Set camera matrices.
			auto view = Matrix::CreateLookAt(shadowLightPos, shadowLightPos +
				forwardVectors[step] * BLOCK(10),
				upVectors[step]);

			auto projection = Matrix::CreatePerspectiveFieldOfView(90.0f * PI / 180.0f, 1.0f, 16.0f, _shadowLight->Out);

			auto shadowProjection = CCameraMatrixBuffer{};
			shadowProjection.ViewProjection = view * projection;
			UpdateConstantBuffer(&shadowProjection, _cbCameraMatrices.get());
			BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Camera, _cbCameraMatrices.get());
			BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Objects, _cbObjects.get());

			_stShadowMap.LightViewProjections[step] = (view * projection);

			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			auto& obj = GetRendererObject((GAME_OBJECT_ID)item->ObjectID);
			auto skinMode = GetSkinningMode(obj, item->SkinIndex);


			_stObjects.Objects[0].World = item->InterpolatedWorld;
			_stObjects.Objects[0].Color = item->Color;
			_stObjects.Objects[0].AmbientLight = item->AmbientLight;
			_stObjects.Skinned = (int)skinMode;

			for (int k = 0; k < BONE_COUNT_MAX; k++)
				_stObjects.BoneLightModes[k] = (int)LightMode::Static;

			if (skinMode == SkinningMode::Full)
			{
				for (int m = 0; m < obj.AnimationTransforms.size(); m++)
					_stObjects.Bones[m] = obj.BindPoseTransforms[m] * item->InterpolatedAnimationTransforms[m];
				UpdateConstantBuffer(&_stObjects, _cbObjects.get());

				auto* mesh = GetMesh(item->SkinIndex);

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

			memcpy(_stObjects.Bones, item->InterpolatedAnimationTransforms, sizeof(Matrix) * obj.AnimationTransforms.size());
			UpdateConstantBuffer(&_stObjects, _cbObjects.get());

			for (int k = 0; k < item->MeshIndex.size(); k++)
			{
				auto* mesh = GetMesh(item->MeshIndex[k]);

				if (skinMode == SkinningMode::Full && g_Level.Meshes[item->MeshIndex[k]].hidden)
					continue;

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

				if (skinMode == SkinningMode::Classic)
					DrawLaraJoints(item, &room, renderView, RendererPass::ShadowMap);

				DrawLaraHolsters(item, &room, renderView, RendererPass::ShadowMap);
				DrawLaraHair(item, &room, renderView, RendererPass::ShadowMap);
			}
		}
	}

	void Renderer::DrawGunShells(RenderView& view, RendererPass rendererPass)
	{
		_stObjects.Skinned = (int)SkinningMode::Static;

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

			if (!_moveableObjects[objectID].has_value())
				continue;

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

			_stObjects.Objects[gunShellCount].World = worldMatrix;
			_stObjects.Objects[gunShellCount].AmbientLight = room.AmbientLight;
			_stObjects.Objects[gunShellCount].Color = room.AmbientLight;
			_stObjects.Objects[gunShellCount].LightMode = (int)LightMode::Dynamic;
			BindInstancedStaticLights(item->LightsToDraw, gunShellCount);

			gunShellCount++;
		}

		if (gunShellCount > 0)
		{
			auto& moveableObject = *_moveableObjects[objectID];

			_shaders.Bind(Shader::InstancedStatics);

			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

			SetBlendMode(BlendMode::Opaque);
			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			UpdateConstantBuffer(&_stObjects, _cbObjects.get());

			const auto& mesh = *moveableObject.ObjectMeshes[0];
			for (const auto& bucket : mesh.Buckets)
			{
				if (bucket.NumVertices == 0)
					continue;

				BindBucketTextures(bucket, TextureSource::Moveables, false);
				BindMaterial(bucket.MaterialIndex, false);

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
					pos, _rooms[rope.room].AmbientLight * g_Level.Items[rope.index].Model.Color,
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
		auto worldMatrix = Matrix::CreateOrthographicOffCenter(0, _graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight(), 0, _viewport.MinDepth, _viewport.MaxDepth);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::LineList);

		_primitiveBatch->Begin();

		for (const auto& line : _lines2DToDraw)
		{
			auto vertex0 = Vertex{};
			vertex0.Position.x = line.Origin.x;
			vertex0.Position.y = line.Origin.y;
			vertex0.Position.z = 1.0f;
			vertex0.Color = VectorColorToRGBA(line.Color);

			auto vertex1 = Vertex{};
			vertex1.Position.x = line.Target.x;
			vertex1.Position.y = line.Target.y;
			vertex1.Position.z = 1.0f;
			vertex1.Color = VectorColorToRGBA(line.Color);

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
		_stObjects.Skinned = (int)SkinningMode::Static;
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

		_stObjects.Skinned = (int)SkinningMode::Static;

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
					_shaders.Bind(Shader::GBufferInstancedStatics);
				}
				else
				{
					_shaders.Bind(Shader::InstancedStatics);
				}

				_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
				_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

				RendererObject& moveableObj = *_moveableObjects[ID_RATS_EMITTER];

				for (int i = 0; i < NUM_RATS; i++)
				{
					auto* rat = &Rats[i];

					if (rat->On)
					{
						int index = (GlobalCounter + i) % Objects[ID_RATS_EMITTER].nmeshes;
						const auto& mesh = *GetMesh(Objects[ID_RATS_EMITTER].meshIndex + index);

						auto world = rat->Transform;
						ReflectMatrixOptionally(world);

						_stObjects.Objects[0].World = world;
						_stObjects.Objects[0].Color = NEUTRAL_COLOR;
						_stObjects.Objects[0].AmbientLight = _rooms[rat->RoomNumber].AmbientLight;
						_stObjects.Objects[0].LightMode = (int)moveableObj.ObjectMeshes[0]->LightMode;

						if (rendererPass != RendererPass::GBuffer)
							BindInstancedStaticLights(_rooms[rat->RoomNumber].LightsToDraw, 0);

						UpdateConstantBuffer(&_stObjects, _cbObjects.get());

						for (int animated = 0; animated < 2; animated++)
						{
							for (const auto& bucket : mesh.Buckets)
							{
								if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
									continue;

								BindBucketTextures(bucket, TextureSource::Moveables, animated);
								BindMaterial(bucket.MaterialIndex, false);

								int passes = rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;

								for (int p = 0; p < passes; p++)
								{
									if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
										continue;

									DrawIndexedInstancedTriangles(bucket.NumIndices, 1, bucket.StartIndex, 0);

									_numMoveablesDrawCalls++;
								}
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

		_stObjects.Skinned = (int)SkinningMode::Static;

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
					_shaders.Bind(Shader::GBufferInstancedStatics);
				}
				else
				{
					_shaders.Bind(Shader::InstancedStatics);
				}

				_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
				_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

				const auto& moveableObj = *_moveableObjects[ID_FISH_EMITTER];

				for (const auto& fish : FishSwarm)
				{
					if (fish.Life <= 0.0f)
						continue;

					const auto& mesh = *GetMesh(Objects[ID_FISH_EMITTER].meshIndex + fish.MeshIndex);

					_stObjects.Objects[0].World = Matrix::Lerp(fish.PrevTransform, fish.Transform, GetInterpolationFactor());
					_stObjects.Objects[0].Color = NEUTRAL_COLOR;
					_stObjects.Objects[0].AmbientLight = _rooms[fish.RoomNumber].AmbientLight;
					_stObjects.Objects[0].LightMode = (int)moveableObj.ObjectMeshes[0]->LightMode;

					if (rendererPass != RendererPass::GBuffer)
						BindInstancedStaticLights(_rooms[fish.RoomNumber].LightsToDraw, 0);

					UpdateConstantBuffer(&_stObjects, _cbObjects.get());

					for (int animated = 0; animated < 2; animated++)
					{
						for (const auto& bucket : mesh.Buckets)
						{
							if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
								continue;

							BindBucketTextures(bucket, TextureSource::Moveables, animated);
							BindMaterial(bucket.MaterialIndex, false);

							int passCount = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;
							for (int p = 0; p < passCount; p++)
							{
								if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
									continue;
	
								DrawIndexedInstancedTriangles(bucket.NumIndices, 1, bucket.StartIndex, 0);

								_numMoveablesDrawCalls++;
							}
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

		_stObjects.Skinned = (int)SkinningMode::Static;

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

					_stObjects.Objects[batCount].World = world;
					_stObjects.Objects[batCount].AmbientLight = room.AmbientLight;
					_stObjects.Objects[batCount].Color = NEUTRAL_COLOR;
					_stObjects.Objects[batCount].LightMode = (int)mesh.LightMode;

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

					_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
					_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

					UpdateConstantBuffer(&_stObjects, _cbObjects.get());

					for (int animated = 0; animated < 2; animated++)
					{
						for (auto& bucket : mesh.Buckets)
						{
							if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
								continue;

							BindBucketTextures(bucket, TextureSource::Moveables, animated);
							BindMaterial(bucket.MaterialIndex, false);

							int passCount = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;
							for (int p = 0; p < passCount; p++)
							{
								if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
									continue;

								DrawIndexedInstancedTriangles(bucket.NumIndices, batCount, bucket.StartIndex, 0);

								_numMoveablesDrawCalls++;
							}
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

		_stObjects.Skinned = (int)SkinningMode::Static;

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

				if (IgnoreReflectionPassForRoom(beetle.RoomNumber))
					continue;
				
				if (beetle.On)
				{
					auto& room = _rooms[beetle.RoomNumber];

					auto transformMatrix = Matrix::Lerp(beetle.PrevTransform, beetle.Transform, GetInterpolationFactor());

					auto world = transformMatrix;
					ReflectMatrixOptionally(world);

					_stObjects.Objects[beetleCount].World = world;
					_stObjects.Objects[beetleCount].AmbientLight = room.AmbientLight;
					_stObjects.Objects[beetleCount].Color = NEUTRAL_COLOR;
					_stObjects.Objects[beetleCount].LightMode = (int)mesh.LightMode;

					if (rendererPass != RendererPass::GBuffer)
					{
						auto lights = std::vector<RendererLight*>{};
						for (int i = 0; i < std::min((int)room.LightsToDraw.size(), MAX_LIGHTS_PER_ITEM); i++)
							lights.push_back(room.LightsToDraw[i]);

						BindInstancedStaticLights(lights, beetleCount);
					}

					beetleCount++;
				}

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

					_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
					_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

					UpdateConstantBuffer(&_stObjects, _cbObjects.get());

					for (int animated = 0; animated < 2; animated++)
					{
						for (auto& bucket : mesh.Buckets)
						{
							if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
								continue;

							BindBucketTextures(bucket, TextureSource::Moveables, animated);
							BindMaterial(bucket.MaterialIndex, false);

							int passCount = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;
							for (int p = 0; p < passCount; p++)
							{
								if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
									continue;

								DrawIndexedInstancedTriangles(bucket.NumIndices, beetleCount, bucket.StartIndex, 0);

								_numMoveablesDrawCalls++;
							}
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

		_stObjects.Skinned = (int)SkinningMode::Static;

		if (rendererPass == RendererPass::CollectTransparentFaces)
		{
			auto* obj = &Objects[ID_LOCUSTS];
			auto& moveableObj = *_moveableObjects[ID_LOCUSTS];

			for (const auto& locust : TEN::Entities::TR4::Locusts)
			{
				if (!locust.On)
					continue;

				if (IgnoreReflectionPassForRoom(locust.RoomNumber))
					continue;

				auto& mesh = *GetMesh(Objects[ID_LOCUSTS].meshIndex + (-locust.Counter & 3));

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
						object.Room = &_rooms[locust.RoomNumber];

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
				if (!locust.On)
					continue;

				if (IgnoreReflectionPassForRoom(locust.RoomNumber))
					continue;

				activeLocustsExist = true;
				break;
			}

			if (activeLocustsExist)
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

				_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
				_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

				auto* obj = &Objects[ID_LOCUSTS];
				auto& moveableObj = *_moveableObjects[ID_LOCUSTS];

				for (const auto& locust : TEN::Entities::TR4::Locusts)
				{
					if (!locust.On)
						continue;

					auto& mesh = *GetMesh(Objects[ID_LOCUSTS].meshIndex + (-locust.Counter & 3));

					auto world = Matrix::Lerp(locust.PrevTransform, locust.Transform, GetInterpolationFactor());
					ReflectMatrixOptionally(world);

					_stObjects.Objects[0].World = world;
					_stObjects.Objects[0].Color = NEUTRAL_COLOR;
					_stObjects.Objects[0].AmbientLight = _rooms[locust.RoomNumber].AmbientLight;
					_stObjects.Objects[0].LightMode = (int)moveableObj.ObjectMeshes[0]->LightMode;

					if (rendererPass != RendererPass::GBuffer)
						BindInstancedStaticLights(_rooms[locust.RoomNumber].LightsToDraw, 0);

					UpdateConstantBuffer(&_stObjects, _cbObjects.get());

					for (int animated = 0; animated < 2; animated++)
					{
						for (auto& bucket : mesh.Buckets)
						{
							if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
								continue;

							BindBucketTextures(bucket, TextureSource::Moveables, animated);
							BindMaterial(bucket.MaterialIndex, false);

							int passCount = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;
							for (int p = 0; p < passCount; p++)
							{
								if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
									continue;

								DrawIndexedInstancedTriangles(bucket.NumIndices, 1, bucket.StartIndex, 0);

								_numMoveablesDrawCalls++;
							}
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

		_graphicsDevice->SetPrimitiveType(PrimitiveType::LineList);
		_primitiveBatch->Begin();

		for (const auto& line : _lines3DToDraw)
		{
			auto vertex0 = Vertex{};
			vertex0.Position = line.Origin;
			vertex0.Color = VectorColorToRGBA(line.Color);

			auto vertex1 = Vertex{};
			vertex1.Position = line.Target;
			vertex1.Color = VectorColorToRGBA(line.Color);

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

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

		_primitiveBatch->Begin();

		for (const auto& tri : _triangles3DToDraw)
		{
			auto rVertices = std::vector<Vertex>{};
			rVertices.reserve(tri.Vertices.size());

			for (const auto& vertex : tri.Vertices)
			{
				auto rVertex = Vertex{};
				rVertex.Position = vertex;
				rVertex.Color = VectorColorToRGBA(tri.Color);

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
		if (_isLocked || g_GameFlow->LastFreezeMode == FreezeMode::Full)
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
		if (_isLocked || g_GameFlow->LastFreezeMode == FreezeMode::Full)
			return;

		if (radius <= EPSILON)
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

	void Renderer::AddDynamicFogBulb(const Vector3& pos, float radius, float density, const Color& color, int hash)
	{
		if (_isLocked || g_GameFlow->LastFreezeMode == FreezeMode::Full)
			return;

		auto dynamicLight = RendererLight{};

		dynamicLight.Color = Vector3(color.x, color.y, color.z);

		dynamicLight.RoomNumber = NO_VALUE;
		dynamicLight.Intensity = density;
		dynamicLight.Position = pos;
		dynamicLight.In = 1.0f;
		dynamicLight.Out = radius;
		dynamicLight.Type = LightType::FogBulb;
		dynamicLight.CastShadows = false;
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
		if (g_GameFlow->CurrentFreezeMode != FreezeMode::Full &&
			g_Gui.GetInventoryMode() == InventoryMode::None)
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

		DrawDebugInfo(_gameCamera);
	}

	void Renderer::ClearScene()
	{
		_gameCamera.Clear();

		ResetItems();
		ClearShadowMap();
	}

	void Renderer::RenderScene(IRenderSurface2D* renderTarget, RenderView& view, SceneRenderMode renderMode)
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

		_stPerDraw.AlphaTest = NO_VALUE;
		_stPerDraw.AlphaThreshold = NO_VALUE;

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

		// Bind constant buffers.
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::PerDraw, _cbPerDraw.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Objects, _cbObjects.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::ShadowLight, _cbShadowMap.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Room, _cbRoom.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Sprites, _cbInstancedSpriteBuffer.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::PostProcess, _cbPostProcessBuffer.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Sky, _cbSky.get());

		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Camera, _cbCameraMatrices.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::PerDraw, _cbPerDraw.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Objects, _cbObjects.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::ShadowLight, _cbShadowMap.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Room, _cbRoom.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Sprites, _cbInstancedSpriteBuffer.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::PostProcess, _cbPostProcessBuffer.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Sky, _cbSky.get());

		_graphicsDevice->BindStructuredBuffer(ShaderStage::VertexShader, TextureRegister::AnimatedFrames, _animatedFramesBuffer.get());
		_graphicsDevice->BindStructuredBuffer(ShaderStage::PixelShader, TextureRegister::AnimatedFrames, _animatedFramesBuffer.get());

		// Reset GPU state.
		SetBlendMode(BlendMode::Opaque, true);
		SetDepthState(DepthState::Write, true);
		SetCullMode(CullMode::CounterClockwise, true);

		BindMaterial(0, true);

		_stPerDraw.Animated = 0;
		UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());

		// Set up vertex parameters.
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);

		// Draw skybox to paraboloid
		DrawHorizonAndSkyForReflections(view);

		_stPerDraw.Animated = 0;
		UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());

		// Bind and clear render target.
		_graphicsDevice->BindRenderTarget(_renderTarget->GetRenderTarget(), _renderTarget->GetDepthTarget());
		_graphicsDevice->ClearRenderTarget2D(_renderTarget->GetRenderTarget(), _debugPage == RendererDebugPage::WireframeMode ? Colors::DimGray : Colors::Black);
		_graphicsDevice->ClearDepthStencil(_renderTarget->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

		// Clear distortion pass data.
		_graphicsDevice->ClearRenderTarget2D(_distortionRenderTarget->GetRenderTarget(), Colors::Transparent);
		_hasDistortionMask = false;

		// Reset viewport and scissor.
		_graphicsDevice->SetViewport(view.Viewport);
		_graphicsDevice->SetScissor(view.Viewport);

		// Camera constant buffer contains matrices, camera position, fog values, and other things shared for all shaders.
		auto cameraConstantBuffer = CCameraMatrixBuffer{};
		view.FillConstantBuffer(cameraConstantBuffer);
		cameraConstantBuffer.Frame = GlobalCounter;
		cameraConstantBuffer.InterpolatedFrame = (float)GlobalCounter + GetInterpolationFactor();
		cameraConstantBuffer.RefreshRate = _graphicsDevice->GetRefreshRate();
		cameraConstantBuffer.CameraUnderwater = g_Level.Rooms[cameraConstantBuffer.RoomNumber].flags & ENV_FLAG_WATER;
		cameraConstantBuffer.DualParaboloidView = Matrix::CreateLookAt(_gameCamera.Camera.WorldPosition, _gameCamera.Camera.WorldPosition + Vector3(0, -1024, 0), Vector3::UnitX);
		cameraConstantBuffer.Gamma = g_Configuration.Gamma;

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

		cameraConstantBuffer.AmbientOcclusion = (g_GameFlow->GetSettings()->Graphics.AmbientOcclusion && g_Configuration.EnableAmbientOcclusion) ? 1 : 0;
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

		cameraConstantBuffer.Hemisphere = 0;

		UpdateConstantBuffer(&cameraConstantBuffer, _cbCameraMatrices.get());

		// Draw horizon and sky.
		DrawHorizonAndSky(_renderTarget->GetDepthTarget(), view);

		// Build G-Buffer (normals + depth).
		_graphicsDevice->ClearRenderTarget2D(_normalsAndMaterialIndexRenderTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->ClearRenderTarget2D(_depthRenderTarget->GetRenderTarget(), Colors::White);
		_graphicsDevice->ClearRenderTarget2D(_emissiveAndRoughnessRenderTarget->GetRenderTarget(), Colors::Transparent);
		
		std::vector<IRenderTarget2D*> gbuffer;
		gbuffer.push_back(_normalsAndMaterialIndexRenderTarget->GetRenderTarget());
		gbuffer.push_back(_depthRenderTarget->GetRenderTarget());
		gbuffer.push_back(_emissiveAndRoughnessRenderTarget->GetRenderTarget());

		_graphicsDevice->BindRenderTargets(gbuffer, _renderTarget->GetDepthTarget());

		// Render G-Buffer pass.
		DoRenderPass(RendererPass::GBuffer, view, true);

		// Calculate ambient occlusion.
		if (g_GameFlow->GetSettings()->Graphics.AmbientOcclusion && g_Configuration.EnableAmbientOcclusion)
			CalculateSSAO(view);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

		_graphicsDevice->SetViewport(view.Viewport);
		_graphicsDevice->SetScissor(view.Viewport);

		// Bind main render target again. Main depth buffer is already filled and avoids overdraw in following steps.
		_graphicsDevice->BindRenderTarget(_renderTarget->GetRenderTarget(), _renderTarget->GetDepthTarget());

		DoRenderPass(RendererPass::Opaque, view, true);
		DoRenderPass(RendererPass::Additive, view, true);
		DoRenderPass(RendererPass::Distortion, view, true);
		DoRenderPass(RendererPass::CollectTransparentFaces, view, false);
		SortTransparentFaces(view);

		DoRenderPass(RendererPass::Transparent, view, true);
		DoRenderPass(RendererPass::GunFlashes, view, true); // HACK: Gunflashes are drawn after everything because they are near camera.

		// Draw 3D debug lines and triangles.
		DrawLines3D(view);
		DrawTriangles3D(view);

		// Copy current scene to the reflections render target for the next frame.
		// RT -> LRRT
		CopyRenderTargetAndDownscale(_renderTarget.get(), _legacyReflectionsRenderTarget.get(), POSTPROCESS_DOWNSCALE_FACTOR, view);
		_graphicsDevice->BindRenderTarget(_renderTarget->GetRenderTarget(), _renderTarget->GetDepthTarget());

		_doingFullscreenPass = true;

		// Calculate full-screen effects.
		ApplyDistortion(_renderTarget.get(), view);
		ApplyDOF(_renderTarget.get(), view);
		ApplyGlow(_renderTarget.get(), view);

		// Draw HUD-space object renders after DOF so they are not blurred by scene depth.
		if (renderMode == SceneRenderMode::Full && g_GameFlow->LastGameStatus == GameStatus::Normal)
		{
			_doingFullscreenPass = false;
			_graphicsDevice->BindRenderTarget(_renderTarget->GetRenderTarget(), _renderTarget->GetDepthTarget());
			_graphicsDevice->ClearDepthStencil(_renderTarget->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
			g_Hud.Draw3D();
			g_DrawItems.Draw();
			_doingFullscreenPass = true;
		}

		// Apply the antialiasing, now 3D geometry and 3D HUD are antialiased
		// FXAA: RT -> PPRT0, PPRT0 -> RT
		// SMAA: RT -> ..., ... -> RT
		ApplyAntialiasing(_renderTarget.get(), view);

		// Now we can apply the color grade, lens flare, cinematic bars and post process effects
		// RT -> PPRT0, [PPRT0 -> PPRT1], PPRT1 -> PPRT0, PPRT0 -> RT
		DrawPostprocess(renderTarget, view, renderMode);

		// Draw text and 2D HUD
		ClearDrawPhaseDisplaySprites();
		if (renderMode == SceneRenderMode::Full && g_GameFlow->LastGameStatus == GameStatus::Normal)
			g_Hud.Draw2D(*LaraItem);

		_doingFullscreenPass = false;

		DrawOverlays(view);
		DrawLines2D();

		if (renderMode == SceneRenderMode::Full && g_GameFlow->LastGameStatus == GameStatus::Normal)
		{
			CollectDisplaySprites(view);
			DrawDisplaySprites(view, false);

			DrawDebugRenderTargets(view);

			// HACK: Strings in the title level are drawn in a separate menu pass, so we bypass it here.
			if (CurrentLevel != 0)
				DrawAllStrings();

			DrawDisplaySprites(view, true);
		}

		time2 = std::chrono::high_resolution_clock::now();
		_timeFrame = (std::chrono::duration_cast<ns>(time2 - time1)).count() / 1000000;
		time1 = time2;

		ClearScene();
		CalculateFrameRate();
	}

	void Renderer::RenderSimpleSceneToParaboloid(IRenderTarget2D* renderTarget, Vector3 position, int hemisphere)
	{
		// TODO: Update the horizon draw code here once paraboloids are required. TrainWreck Feb 2, 2025.
		
#ifdef PARABOLOID
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

		if (hemisphere == -1)
		{
			SetCullMode(CullMode::CounterClockwise);
		}
		else
		{
			SetCullMode(CullMode::Clockwise);
		}

		auto view = RenderView(&Camera, 0, PI / 2.0f, 32, DEFAULT_FAR_VIEW, ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE);

		auto cameraConstantBuffer = CCameraMatrixBuffer{};
		cameraConstantBuffer.DualParaboloidView = Matrix::CreateLookAt(position, position + Vector3(0, 0, 1024), -Vector3::UnitY);
		cameraConstantBuffer.Hemisphere = hemisphere;
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

					_stObjects.Objects[0].World = (rotation * translation);
					_stObjects.Objects[0].Color = weather.SkyColor(s);
					_stObjects.Objects[0].AmbientLight = NEUTRAL_COLOR;
					_stObjects.Objects[0].LightMode = 0;
					_stObjects.Objects[0].NumLights = 0;
					_stObjects.Objects[0].ApplyFogBulbs = s == 0 ? 1 : 0;

					UpdateConstantBuffer(&_stObjects, _cbObjects);

					DrawIndexedInstancedTriangles(SKY_INDICES_COUNT, 1, 0, 0);
				}
			}

			_context->ClearDepthStencilView(renderTarget->DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

			// Draw horizon.
			if (_moveableObjects[ID_HORIZON].has_value()) // FIXME: Replace with same function as in the main pipeline!
			{
				_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
				_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

				const auto& moveableObj = *_moveableObjects[ID_HORIZON]; // FIXME: Replace with same function as in the main pipeline!

				_stObjects.Objects[0].World = Matrix::CreateTranslation(LaraItem->Pose.Position.ToVector3());
				_stObjects.Objects[0].Color = NEUTRAL_COLOR;
				_stObjects.Objects[0].AmbientLight = NEUTRAL_COLOR;
				_stObjects.Objects[0].LightMode = 0;
				_stObjects.Objects[0].NumLights = 0;
				_stObjects.Objects[0].ApplyFogBulbs = 1;

				UpdateConstantBuffer(&_stObjects, _cbObjects);

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
						DrawIndexedInstancedTriangles(bucket.NumIndices, 1, bucket.StartIndex, 0);

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
		_graphicsDevice->BindVertexBuffer(_roomsVertexBuffer.get());
		_graphicsDevice->BindIndexBuffer(_roomsIndexBuffer.get());

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
			UpdateConstantBuffer(&_stRoom, _cbRoom);

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
#endif
	}

	void Renderer::RenderFullScreenTexture(ITextureBase* texture, float aspect)
	{
		if (texture == nullptr)
			return;

		// Set basic render states.
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		// Clear screen
		_graphicsDevice->ClearRenderTarget2D(_backBuffer->GetRenderTarget(), Colors::Black);
		_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

		// Bind back buffer.
		_graphicsDevice->BindRenderTarget(_backBuffer->GetRenderTarget(), _backBuffer->GetDepthTarget());
		
		_graphicsDevice->SetViewport(_viewport);
		_graphicsDevice->SetScissor(_viewport);

		// Draw full screen background.
		DrawFullScreenQuad(texture, Vector3::One, true, aspect);

		ClearScene();

		_graphicsDevice->ClearState();
		_graphicsDevice->Present();
	}

	void Renderer::DumpGameScene(SceneRenderMode renderMode, float blur)
	{
		if (blur > EPSILON)
			SetDOF({ DOFMode::Full, 0, 0, blur }, false);

		RenderScene(_dumpScreenRenderTarget.get(), _gameCamera, renderMode);

		if (blur > EPSILON)
			RestoreDOF();
	}

	void Renderer::DoRenderPass(RendererPass pass, RenderView& view, bool drawMirrors)
	{
		// Reset GPU state.
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		if (pass == RendererPass::Distortion)
		{
			_graphicsDevice->BindRenderTarget(_distortionRenderTarget->GetRenderTarget(), nullptr);
			_graphicsDevice->SetViewport(_distortionViewport);
			_graphicsDevice->SetScissor(_distortionViewport);
		}

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

		if (pass == RendererPass::Distortion)
		{
			_graphicsDevice->BindRenderTarget(_renderTarget->GetRenderTarget(), _renderTarget->GetDepthTarget());
			_graphicsDevice->SetViewport(view.Viewport);
			_graphicsDevice->SetScissor(view.Viewport);
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
		if (rendererPass != RendererPass::CollectTransparentFaces)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

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

			if (g_GameFlow->GetSettings()->Graphics.AmbientOcclusion && g_Configuration.EnableAmbientOcclusion && rendererPass != RendererPass::GBuffer)
			{
				BindRenderTargetAsTexture(TextureRegister::SSAO, _SSAOBlurredRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
			}
		}

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

	void Renderer::DrawWaterfalls(RendererItem* item, RenderView& view, float speed, RendererPass rendererPass)
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
		  
		// Setup animated metadata in PerDraw and frames in the structured buffer.
		_stPerDraw.AnimFps = speed;
		_stPerDraw.NumAnimFrames = 1;
		_stPerDraw.AnimType = 1; // UVRotate
		_stPerDraw.Animated = 1;
		_stPerDraw.IsWaterfall = 1;
		UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());

		// We need only top/bottom Y coordinate for UVRotate, but we pass whole
		// rectangle anyway, in case later we may want to implement different UVRotate modes.
		_animatedFrames[0].TopLeft     = Vector2(minX, minY);
		_animatedFrames[0].TopRight    = Vector2(maxX, minY);
		_animatedFrames[0].BottomLeft  = Vector2(minX, maxY);
		_animatedFrames[0].BottomRight = Vector2(maxX, maxY);
		_graphicsDevice->UpdateStructuredBuffer(_animatedFramesBuffer.get(), _animatedFrames.data(), 1);

		DrawAnimatingItem(item, view, rendererPass);

		// Reset animated metadata after rendering just in case.
		_stPerDraw.AnimFps = _stPerDraw.NumAnimFrames = _stPerDraw.AnimType = 0;
		UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());
	}

	void Renderer::DrawAnimatingItem(RendererItem* item, RenderView& view, RendererPass rendererPass)
	{
		ItemInfo* nativeItem = &g_Level.Items[item->ItemNumber];
		RendererRoom* room = &_rooms[item->RoomNumber];
		RendererObject& moveableObj = *_moveableObjects[item->ObjectID];

		auto skinMode = GetSkinningMode(moveableObj, item->SkinIndex);

		if (rendererPass != RendererPass::CollectTransparentFaces)
		{
			// Bind item main properties
			_stObjects.Objects[0].World = item->InterpolatedWorld;
			ReflectMatrixOptionally(_stObjects.Objects[0].World);

			_stObjects.Objects[0].Color = item->Color;
			_stObjects.Objects[0].AmbientLight = item->AmbientLight;
			_stObjects.Skinned = (int)skinMode;

			for (int k = 0; k < item->MeshIndex.size(); k++)
				_stObjects.BoneLightModes[k] = (int)GetMesh(item->MeshIndex[k])->LightMode;

			bool acceptsShadows = moveableObj.ShadowType == ShadowMode::None;
			BindMoveableLights(item->LightsToDraw, item->RoomNumber, item->PrevRoomNumber, item->LightFade, acceptsShadows);

			if (skinMode == SkinningMode::Full)
			{
				for (int m = 0; m < moveableObj.AnimationTransforms.size(); m++)
					_stObjects.Bones[m] = moveableObj.BindPoseTransforms[m] * item->InterpolatedAnimationTransforms[m];
				UpdateConstantBuffer(&_stObjects, _cbObjects.get());

				DrawMesh(item, GetMesh(item->SkinIndex), RendererObjectType::Moveable, 0, true, view, rendererPass);
			}

			memcpy(_stObjects.Bones, item->InterpolatedAnimationTransforms, moveableObj.AnimationTransforms.size() * sizeof(Matrix));
			UpdateConstantBuffer(&_stObjects, _cbObjects.get());
		}

		for (int k = 0; k < item->MeshIndex.size(); k++)
		{
			if (!nativeItem->MeshBits.Test(k))
				continue;

			if (skinMode == SkinningMode::Full && g_Level.Meshes[nativeItem->Model.MeshIndex[k]].hidden)
				continue;

			DrawMesh(item, GetMesh(item->MeshIndex[k]), RendererObjectType::Moveable, k, false, view, rendererPass);
		}
	}

	void Renderer::DrawStatics(RenderView& view, RendererPass rendererPass)
	{
		_stObjects.Skinned = (int)SkinningMode::Static;

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
			unsigned int stride = sizeof(Vertex);
			unsigned int offset = 0;
			_graphicsDevice->BindVertexBuffer(_staticsVertexBuffer.get());
			_graphicsDevice->BindIndexBuffer(_staticsIndexBuffer.get());;

			if (g_GameFlow->GetSettings()->Graphics.AmbientOcclusion && g_Configuration.EnableAmbientOcclusion && rendererPass != RendererPass::GBuffer)
			{
				BindRenderTargetAsTexture(TextureRegister::SSAO, &_SSAOBlurredRenderTarget, SamplerStateRegister::PointWrap);
			}
			
			BindRenderTargetAsTexture(TextureRegister::LegacyEnvironmentReflections, &_skyboxRenderTarget, SamplerStateRegister::AnisotropicClamp);

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
			_graphicsDevice->BindVertexBuffer(_staticsVertexBuffer.get());
			_graphicsDevice->BindIndexBuffer(_staticsIndexBuffer.get());
			
			if (g_GameFlow->GetSettings()->Graphics.AmbientOcclusion && g_Configuration.EnableAmbientOcclusion && rendererPass != RendererPass::GBuffer)
			{
				BindRenderTargetAsTexture(TextureRegister::SSAO, _SSAOBlurredRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
			}

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

						_stObjects.Objects[instancesCount].World = world;
						_stObjects.Objects[instancesCount].Color = current->Color;
						_stObjects.Objects[instancesCount].AmbientLight = room->AmbientLight;
						_stObjects.Objects[instancesCount].LightMode = (int)refMesh->LightMode;

						if (rendererPass != RendererPass::GBuffer)
							BindInstancedStaticLights(current->LightsToDraw, instancesCount);

						instancesCount++;
					}

					baseStaticIndex += bucketSize;

					if (instancesCount > 0)
					{
						UpdateConstantBuffer(&_stObjects, _cbObjects.get());

						bool bindTextureAndMaterialsRequired = true;

						for (int animated = 0; animated < 2; animated++)
						{
							for (auto& bucket : refMesh->Buckets)
							{
								if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
								{
									continue;
								}
								 
								int passes = rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;
								for (int p = 0; p < passes; p++)
								{
									if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
										continue;

									if (bindTextureAndMaterialsRequired)
									{
										BindBucketTextures(bucket, TextureSource::Statics, animated);
										BindMaterial(bucket.MaterialIndex, false);

										bindTextureAndMaterialsRequired = false;
									}
																		
									DrawIndexedInstancedTriangles(bucket.NumIndices, instancesCount, bucket.StartIndex, 0);

									_numInstancedStaticsDrawCalls++;
								}

								bindTextureAndMaterialsRequired = true; 
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

				for (int j = 0; j < room->Buckets.size(); j++)
				{
					auto& bucket = room->Buckets[j];

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
		else
		{
			if (rendererPass == RendererPass::GBuffer)
			{
				_shaders.Bind(Shader::GBuffer);
				_shaders.Bind(Shader::GBufferRooms);
			}
			else
			{
				_shaders.Bind(Shader::Rooms);
			}

			// Bind vertex and index buffer.
			_graphicsDevice->BindVertexBuffer(_roomsVertexBuffer.get());
			_graphicsDevice->BindIndexBuffer(_roomsIndexBuffer.get());
			   
			if (rendererPass != RendererPass::GBuffer)
			{
				// Bind caustics texture.
				if (TEN::Utils::Contains(SpriteSequencesIds, (int)ID_CAUSTIC_TEXTURES))
				{     
					int nmeshes = -Objects[ID_CAUSTIC_TEXTURES].nmeshes;
					int meshIndex = Objects[ID_CAUSTIC_TEXTURES].meshIndex;
					int causticsFrame = GlobalCounter % nmeshes;
					auto causticsSprite = _spriteSequences[ID_CAUSTIC_TEXTURES].SpritesList[causticsFrame];

					BindTexture(TextureRegister::CausticsMap, causticsSprite->Texture, SamplerStateRegister::AnisotropicClamp);
				
					_stRoom.CausticsSize = Vector2(
						(float)causticsSprite->Width / (float)causticsSprite->Texture->GetWidth(),
						(float)causticsSprite->Height / (float)causticsSprite->Texture->GetHeight());
					_stRoom.CausticsStartUV = causticsSprite->UV[0];
				} 

				// Set shadow map data and bind shadow map texture.
				if (_shadowLight != nullptr)
				{
					BindLight(*_shadowLight, &_stShadowMap.Light, 0);
					_stShadowMap.ShadowMapSize = g_Configuration.ShadowMapSize;
					_stShadowMap.CastShadows = true;

					BindTexture(TextureRegister::ShadowMap, _shadowMap->GetRenderTarget(), SamplerStateRegister::ShadowMap);
				}
				else
				{
					_stShadowMap.CastShadows = false;
				}
				
				UpdateConstantBuffer(&_stShadowMap, _cbShadowMap.get());
			}

			if (g_GameFlow->GetSettings()->Graphics.AmbientOcclusion && g_Configuration.EnableAmbientOcclusion && rendererPass != RendererPass::GBuffer)
			{
				BindRenderTargetAsTexture(TextureRegister::SSAO, _SSAOBlurredRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
			}

			for (int i = (int)view.RoomsToDraw.size() - 1; i >= 0; i--)
			{
				const auto& room = *view.RoomsToDraw[i];
				const auto& nativeRoom = g_Level.Rooms[room.RoomNumber];

				bool bindRoomDataRequired = true;
				bool bindTexturesAndMaterialsRequired = true;
				
				for (int animated = 0; animated < 2; animated++)
				{
					for (const auto& bucket : room.Buckets)
					{
						if (((animated == 1) ^ bucket.Animated) || bucket.NumVertices == 0)
							continue;

						int passes = rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest ? 2 : 1;

						for (int p = 0; p < passes; p++)
						{
							if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
								continue;

							if (bindRoomDataRequired)
							{
								if (rendererPass != RendererPass::GBuffer)
								{
									_stRoom.Caustics = int(g_Configuration.EnableCaustics && (nativeRoom.flags & ENV_FLAG_WATER) && !(nativeRoom.flags & ENV_FLAG_NOCAUSTICS));
									_stRoom.AmbientColor = Vector3(room.AmbientLight.x, room.AmbientLight.y, room.AmbientLight.z);
									BindRoomLights(view.LightsToDraw);
									BindRoomDecals(room.Decals);
								}

								_stRoom.Water = (nativeRoom.flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
								UpdateConstantBuffer(&_stRoom, _cbRoom.get());

								SetScissor(room.ClipBounds);

								bindRoomDataRequired = false;
							}

							if (bindTexturesAndMaterialsRequired)
							{
								BindMaterial(bucket.MaterialIndex, false);
								BindBucketTextures(bucket, TextureSource::Rooms, animated);

								bindTexturesAndMaterialsRequired = false;
							}

							DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

							_numRoomsDrawCalls++;
						}

						bindTexturesAndMaterialsRequired = true;
					}
				}
			}

			ResetScissor();
		}
	}
	
	void Renderer::DrawHorizonAndSkyForReflections(RenderView& renderView)
	{
		_graphicsDevice->ClearRenderTarget2D(_skyboxRenderTarget->GetRenderTarget(), 0, Colors::Black);
		_graphicsDevice->ClearDepthStencil(_skyboxRenderTarget->GetDepthTarget(), 0, DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
		_graphicsDevice->BindRenderTarget(
			IRenderTargetBinding(_skyboxRenderTarget->GetRenderTarget(), 0),
			IDepthTargetBinding(_skyboxRenderTarget->GetDepthTarget(), 0)
		);

		RendererViewport viewport;
		viewport.X = 0;
		viewport.Y = 0;
		viewport.Width = ROOM_AMBIENT_MAP_SIZE;
		viewport.Height = ROOM_AMBIENT_MAP_SIZE;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;

		_graphicsDevice->SetViewport(viewport);
		_graphicsDevice->SetScissor(viewport);

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		auto view = RenderView(_gameCamera.Camera.WorldPosition, Vector3::UnitX, Vector3::UnitY, ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE, 0, 32, DEFAULT_FAR_VIEW, PI / 2.0f);

		auto cameraConstantBuffer = CCameraMatrixBuffer{};
		cameraConstantBuffer.DualParaboloidView = Matrix::CreateLookAt(_gameCamera.Camera.WorldPosition, _gameCamera.Camera.WorldPosition + Vector3(0, -1024, 0), Vector3::UnitX);
		cameraConstantBuffer.Hemisphere = -1;
		cameraConstantBuffer.Frame = GlobalCounter;
		cameraConstantBuffer.InterpolatedFrame = (float)GlobalCounter + GetInterpolationFactor();
		cameraConstantBuffer.RefreshRate = _graphicsDevice->GetRefreshRate();
		view.FillConstantBuffer(cameraConstantBuffer);
		UpdateConstantBuffer(&cameraConstantBuffer, _cbCameraMatrices.get());

		DrawHorizonAndSky(_skyboxRenderTarget->GetDepthTarget(), view, 0, true);

		_graphicsDevice->ClearRenderTarget2D(_skyboxRenderTarget->GetRenderTarget(), 1, Colors::Black);
		_graphicsDevice->ClearDepthStencil(_skyboxRenderTarget->GetDepthTarget(), 1, DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
		_graphicsDevice->BindRenderTarget(
			IRenderTargetBinding(_skyboxRenderTarget->GetRenderTarget(), 1),
			IDepthTargetBinding(_skyboxRenderTarget->GetDepthTarget(), 1)
		);
		SetCullMode(CullMode::Clockwise);

		cameraConstantBuffer.Hemisphere = 1;
		UpdateConstantBuffer(&cameraConstantBuffer, _cbCameraMatrices.get());

		DrawHorizonAndSky(_skyboxRenderTarget->GetDepthTarget(), view, 1, true);

		SetCullMode(CullMode::CounterClockwise);
	}

	void Renderer::DrawHorizonAndSky(IDepthTarget* depthTarget, RenderView& renderView, int arrayIndex, bool reflectionPass)
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

		if ((!levelPtr->GetHorizonEnabled(0) && !levelPtr->GetHorizonEnabled(1)) || (!anyOutsideRooms && !reflectionPass))
			return;

		if (Lara.Control.Look.OpticRange != 0)
			AlterFOV(ANGLE(DEFAULT_FOV) - Lara.Control.Look.OpticRange, false);

		// Draw sky.
		auto rotation = Matrix::CreateRotationX(PI);

		_shaders.Bind(reflectionPass ? Shader::RoomAmbientSky : Shader::Sky);
		BindTexture(TextureRegister::ColorMap, _skyTexture.get(), SamplerStateRegister::AnisotropicClamp);

		_graphicsDevice->BindVertexBuffer(_skyVertexBuffer.get());
		_graphicsDevice->BindIndexBuffer(_skyIndexBuffer.get());

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

				_stSky.World = (rotation * translation);
				_stSky.Color = Weather.SkyColor(layer);
				_stSky.ApplyFogBulbs = layer == 0 ? 1 : 0;
				_stSky.Ambient = Vector4::One;
				UpdateConstantBuffer(&_stSky, _cbSky.get());

				DrawIndexedTriangles(SKY_INDICES_COUNT, 0, 0);

				_numMoveablesDrawCalls++;
			}
		}

		_graphicsDevice->ClearDepthStencil(depthTarget, arrayIndex, DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);

		if (Weather.GetStars().size() > 0 && !reflectionPass)
		{
			SetDepthState(DepthState::Read);
			SetBlendMode(BlendMode::Additive);
			SetCullMode(CullMode::None);

			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleStrip);

			_shaders.Bind(Shader::InstancedSprites);

			_graphicsDevice->BindVertexBuffer(_quadVertexBuffer.get());

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

					// NOTE: Strange packing due to particular HLSL 16-byte alignment requirements.
					_stInstancedSpriteBuffer.Sprites[i].UV[0].x = rDrawSprite.Sprite->UV[0].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].y = rDrawSprite.Sprite->UV[1].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].z = rDrawSprite.Sprite->UV[2].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].w = rDrawSprite.Sprite->UV[3].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].x = rDrawSprite.Sprite->UV[0].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].y = rDrawSprite.Sprite->UV[1].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].z = rDrawSprite.Sprite->UV[2].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].w = rDrawSprite.Sprite->UV[3].y;
				}

				UpdateConstantBuffer(&_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer.get());

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

						// NOTE: Strange packing due to particular HLSL 16-byte alignment requirements.
						_stInstancedSpriteBuffer.Sprites[i].UV[0].x = rDrawSprite.Sprite->UV[0].x;
						_stInstancedSpriteBuffer.Sprites[i].UV[0].y = rDrawSprite.Sprite->UV[1].x;
						_stInstancedSpriteBuffer.Sprites[i].UV[0].z = rDrawSprite.Sprite->UV[2].x;
						_stInstancedSpriteBuffer.Sprites[i].UV[0].w = rDrawSprite.Sprite->UV[3].x;
						_stInstancedSpriteBuffer.Sprites[i].UV[1].x = rDrawSprite.Sprite->UV[0].y;
						_stInstancedSpriteBuffer.Sprites[i].UV[1].y = rDrawSprite.Sprite->UV[1].y;
						_stInstancedSpriteBuffer.Sprites[i].UV[1].z = rDrawSprite.Sprite->UV[2].y;
						_stInstancedSpriteBuffer.Sprites[i].UV[1].w = rDrawSprite.Sprite->UV[3].y;
					}

					UpdateConstantBuffer(&_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer.get());

					// Draw sprites with instancing.
					DrawInstancedTriangles(4, meteorsToDraw, 0);

					drawnMeteors += meteorsToDraw;
				}
			}

			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);

			SetCullMode(CullMode::CounterClockwise);
		}

		// Draw horizon.
		for (int layer = 0; layer < 2; layer++)
		{
			if (!levelPtr->GetHorizonEnabled(layer) || levelPtr->GetHorizonTransparency(layer) <= EPSILON)
				continue;

			if (!_moveableObjects[levelPtr->GetHorizonObjectID(layer)].has_value())
				continue;
			
			_shaders.Bind(reflectionPass ? Shader::RoomAmbientSky : Shader::Sky);

			SetDepthState(DepthState::None);
			SetBlendMode(BlendMode::Opaque);

			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

			auto pos = Vector3::Lerp(levelPtr->GetHorizonPrevPosition(layer), levelPtr->GetHorizonPosition(layer), GetInterpolationFactor());
			auto orient = EulerAngles::Lerp(levelPtr->GetHorizonPrevOrientation(layer), levelPtr->GetHorizonOrientation(layer), GetInterpolationFactor());
			auto rotMatrix = orient.ToRotationMatrix();
			auto translationMatrix = Matrix::CreateTranslation(pos);
			auto cameraMatrix = Matrix::CreateTranslation(renderView.Camera.WorldPosition);

			float alpha = levelPtr->GetHorizonTransparency(layer);

			_stSky.World = rotMatrix * translationMatrix * cameraMatrix;
			_stSky.Color = Color(1.0f, 1.0f, 1.0f, alpha);
			_stSky.ApplyFogBulbs = 1;
			UpdateConstantBuffer(&_stSky, _cbSky.get());

			const auto& moveableObj = *_moveableObjects[levelPtr->GetHorizonObjectID(layer)];
			for (auto* mesh : moveableObj.ObjectMeshes)
			{ 
				for (int animated = 0; animated < 2; animated++)
				{
					for (auto& bucket : mesh->Buckets)
					{
						if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
						{
							continue;
						}
					    
						BindBucketTextures(bucket, TextureSource::Moveables, animated);

						// Always render horizon as alpha-blended surface.
						SetBlendMode(GetBlendModeFromAlpha((bucket.BlendMode == BlendMode::AlphaTest) ? BlendMode::AlphaBlend : bucket.BlendMode, alpha));
						SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

						// Draw vertices.
						DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

						_numMoveablesDrawCalls++;
					}
				}
			}
		}

		// Eventually draw the sun sprite.
		if (!renderView.LensFlaresToDraw.empty() && renderView.LensFlaresToDraw[0].IsGlobal && !reflectionPass)
		{
			SetDepthState(DepthState::Read);
			SetBlendMode(BlendMode::Additive);
			SetCullMode(CullMode::None);

			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleStrip);

			_shaders.Bind(Shader::InstancedSprites);

			// Set up vertex buffer and parameters.
			_graphicsDevice->BindVertexBuffer(_quadVertexBuffer.get());

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

			// NOTE: Strange packing due to particular HLSL 16-byte alignment requirements.
			_stInstancedSpriteBuffer.Sprites[0].UV[0].x = rDrawSprite.Sprite->UV[0].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].y = rDrawSprite.Sprite->UV[1].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].z = rDrawSprite.Sprite->UV[2].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].w = rDrawSprite.Sprite->UV[3].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].x = rDrawSprite.Sprite->UV[0].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].y = rDrawSprite.Sprite->UV[1].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].z = rDrawSprite.Sprite->UV[2].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].w = rDrawSprite.Sprite->UV[3].y;

			BindTexture(TextureRegister::ColorMap, rDrawSprite.Sprite->Texture, SamplerStateRegister::LinearClamp);

			UpdateConstantBuffer(&_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer.get());

			// Draw sprites with instancing.
			DrawInstancedTriangles(4, 1, 0);

			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		}

		// Clear just the Z-buffer to start drawing on top of horizon.
		_graphicsDevice->ClearDepthStencil(depthTarget, arrayIndex, DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
	}

	void Renderer::Render(float interpFactor)
	{
		InterpolateCamera(interpFactor);
		RenderScene(_backBuffer.get(), _gameCamera);

		_graphicsDevice->ClearState();
		_graphicsDevice->Present(); // 1 0
	}

	void Renderer::DrawMesh(RendererItem* itemToDraw, RendererMesh* mesh, RendererObjectType type, int boneIndex, bool skinned, RenderView& view, RendererPass rendererPass)
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
						auto center = Vector3::Transform(bucket.Polygons[p].Centre, itemToDraw->InterpolatedAnimationTransforms[boneIndex] * itemToDraw->InterpolatedWorld);
						int dist = Vector3::Distance(center, Camera.pos.ToVector3());

						auto object = RendererSortableObject{};
						object.ObjectType = type;
						object.Centre = center;
						object.Distance = dist;
						object.Skinned = skinned;
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
			bool bindTextureAndMaterialsRequired = true;

			for (int animated = 0; animated < 2; animated++)
			{
				for (auto& bucket : mesh->Buckets)
				{
					if ((animated == 1) ^ bucket.Animated || bucket.NumVertices == 0)
					{
						continue;
					}

					// HACK: waterfalls have a hardcoded step
					// We bind non animated textures in this case because the engine calculates at runtime the scroll effect
					// TODO: in the future uniform this thing
					if (IsWaterfall(itemToDraw->ObjectID) && animated == 1)
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

							if (bindTextureAndMaterialsRequired)
							{
								// HACK: for waterfalls
								if (IsWaterfall(itemToDraw->ObjectID))
									BindAtlasTextures(bucket, TextureSource::Moveables);
								else
									BindBucketTextures(bucket, TextureSource::Moveables, animated);

								BindMaterial(bucket.MaterialIndex, false);

								bindTextureAndMaterialsRequired = false;
							}
												
							DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);

							_numMoveablesDrawCalls++;
						}

						bindTextureAndMaterialsRequired = true;
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

			SetBlendMode(blendMode);
			SetAlphaTest(AlphaTestMode::None, 1.0f);
			break;

		case RendererPass::Distortion:
			if (blendMode != BlendMode::Distortion)
			{
				return false;
			}

			_hasDistortionMask = true;
			SetBlendMode(blendMode);
			SetAlphaTest(AlphaTestMode::None, 1.0f);
			break;

		default:
			return false;

		}

		return true;
	}

	void Renderer::SortTransparentFaces(RenderView& view)
	{
		// Group sortable polygons by drawable unit instead of sorting them individually
		// (issue #1793): rooms form one group per bucket, other types one group per
		// (instance, bucket) - per (texture, depth band) for sprites. Room groups draw
		// first, following the portal traversal order of RoomsToDraw (approximate
		// back-to-front); object groups follow, sorted by their mean polygon distance.
		// Polygons are depth-sorted only inside their own group, so each group draws as
		// a single batch instead of one batch per depth interleave.

		if (view.TransparentObjectsToDraw.empty())
			return;

		// Depth step below which two object groups count as equally distant and fall back
		// to the blend mode priority for a stable order.
		static constexpr float GROUP_DEPTH_STEP = 128.0f;

		// Same-texture sprites group together only within this depth band, so a local
		// cloud batches as one unit without dragging distant sprites out of depth order.
		constexpr float SPRITE_GROUP_DEPTH_BAND = BLOCK(4);

		struct GroupKey
		{
			const void* Primary	  = nullptr;
			const void* Secondary = nullptr;
			uint64_t	Extra	  = 0;

			bool operator ==(const GroupKey& key) const
			{
				return (Primary == key.Primary && Secondary == key.Secondary && Extra == key.Extra);
			}
		};

		struct GroupKeyHash
		{
			size_t operator ()(const GroupKey& key) const
			{
				uint64_t hash = (uint64_t)(uintptr_t)key.Primary;
				hash ^= (uint64_t)(uintptr_t)key.Secondary * 0x9E3779B97F4A7C15;
				hash ^= key.Extra * 0xC2B2AE3D27D4EB4F;
				hash ^= hash >> 29;
				return (size_t)hash;
			}
		};

		struct SortedGroup
		{
			bool  IsRoom		= false;
			int	  RoomSlot		= 0;
			float DistanceSum	= 0.0f;
			int	  PolygonCount	= 0;
			int	  BlendPriority = 0;
		};

		// Fixed priority between blend modes of groups at the same depth (issue #1793),
		// so their relative order is stable and predictable instead of arbitrary.
		auto getBlendPriority = [](BlendMode blendMode)
		{
			switch (blendMode)
			{
			case BlendMode::Exclude:	 return 0;
			case BlendMode::Subtractive: return 1;
			case BlendMode::Lighten:	 return 2;
			case BlendMode::Screen:		 return 3;
			case BlendMode::AlphaBlend:	 return 4;
			default:					 return 5;
			}
		};

		// Position of every room in the back-to-front draw order (portal traversal reversed).
		auto roomSlots = std::unordered_map<const RendererRoom*, int>{};
		roomSlots.reserve(view.RoomsToDraw.size());
		for (int i = 0; i < view.RoomsToDraw.size(); i++)
			roomSlots[view.RoomsToDraw[i]] = (int)view.RoomsToDraw.size() - 1 - i;

		auto groupIndices = std::unordered_map<GroupKey, int, GroupKeyHash>{};
		auto groups = std::vector<SortedGroup>{};

		for (auto& object : view.TransparentObjectsToDraw)
		{
			auto key = GroupKey{};
			auto blendMode = object.BlendMode;

			switch (object.ObjectType)
			{
			case RendererObjectType::Room:
				key.Primary = object.Bucket;
				break;

			case RendererObjectType::Moveable:
			case RendererObjectType::HairPrimary:
			case RendererObjectType::HairSecondary:
				key.Primary = object.Item;
				key.Secondary = object.Bucket;
				key.Extra = (uint64_t)object.ObjectType;
				break;

			case RendererObjectType::Static:
				key.Primary = object.Static;
				key.Secondary = object.Bucket;
				break;

			case RendererObjectType::Effect:
				key.Primary = object.Effect;
				key.Secondary = object.Bucket;
				break;

			case RendererObjectType::MoveableAsStatic:
				// No stable instance pointer exists for swarm objects: the world translation
				// tells instances of the same mesh apart (e.g. two bats sharing buckets).
				key.Primary = object.Bucket;
				key.Extra =
					((uint64_t)(std::lround(object.World._41) & 0x1FFFFF)) |
					((uint64_t)(std::lround(object.World._42) & 0x1FFFFF) << 21) |
					((uint64_t)(std::lround(object.World._43) & 0x1FFFFF) << 42);
				break;

			case RendererObjectType::Sprite:
				blendMode = object.Sprite->BlendMode;
				key.Primary = object.Sprite->Sprite;
				key.Extra =
					(uint64_t)(uint32_t)(int)(object.Distance / SPRITE_GROUP_DEPTH_BAND) |
					((uint64_t)blendMode << 32) |
					((uint64_t)object.Sprite->Type << 40) |
					((uint64_t)object.Sprite->renderType << 48) |
					((uint64_t)(object.Sprite->SoftParticle ? 1 : 0) << 56);
				break;

			default:
				key.Primary = object.Polygon;
				key.Extra = (uint64_t)object.ObjectType;
				break;
			}

			auto [it, isNewGroup] = groupIndices.try_emplace(key, (int)groups.size());
			if (isNewGroup)
			{
				auto& group = groups.emplace_back();
				group.IsRoom = (object.ObjectType == RendererObjectType::Room);
				group.BlendPriority = getBlendPriority(blendMode);

				if (group.IsRoom)
				{
					auto slot = roomSlots.find(object.Room);
					group.RoomSlot = (slot != roomSlots.end()) ? slot->second : (int)view.RoomsToDraw.size();
				}
			}

			auto& group = groups[it->second];
			group.DistanceSum += object.Distance;
			group.PolygonCount++;

			// Temporarily the group index; remapped to the final rank below.
			object.GroupRank = it->second;
		}

		auto order = std::vector<int>(groups.size());
		for (int i = 0; i < order.size(); i++)
			order[i] = i;

		std::sort(
			order.begin(), order.end(),
			[&groups](int index0, int index1)
			{
				const auto& group0 = groups[index0];
				const auto& group1 = groups[index1];

				// Rooms draw first, back to front by portal traversal order.
				if (group0.IsRoom != group1.IsRoom)
					return group0.IsRoom;

				float distance0 = group0.DistanceSum / group0.PolygonCount;
				float distance1 = group1.DistanceSum / group1.PolygonCount;

				if (group0.IsRoom)
				{
					if (group0.RoomSlot != group1.RoomSlot)
						return (group0.RoomSlot < group1.RoomSlot);

					return (distance0 > distance1);
				}

				// Object groups draw back to front by mean polygon distance.
				int depth0 = (int)(distance0 / GROUP_DEPTH_STEP);
				int depth1 = (int)(distance1 / GROUP_DEPTH_STEP);
				if (depth0 != depth1)
					return (depth0 > depth1);

				if (group0.BlendPriority != group1.BlendPriority)
					return (group0.BlendPriority < group1.BlendPriority);

				return (index0 < index1);
			});

		auto groupRanks = std::vector<int>(groups.size());
		for (int i = 0; i < order.size(); i++)
			groupRanks[order[i]] = i;

		for (auto& object : view.TransparentObjectsToDraw)
			object.GroupRank = groupRanks[object.GroupRank];

		// Make groups contiguous in rank order, keeping polygons depth-sorted inside
		// their own group.
		std::sort(
			view.TransparentObjectsToDraw.begin(),
			view.TransparentObjectsToDraw.end(),
			[](const RendererSortableObject& object0, const RendererSortableObject& object1)
			{
				if (object0.GroupRank != object1.GroupRank)
					return (object0.GroupRank < object1.GroupRank);

				return (object0.Distance > object1.Distance);
			});
	}

	void Renderer::DrawSortedFaces(RenderView& view)
	{
		// Invalidate the room CB cache used by DrawRoomSorted.
		// Invalidate the CB caches used by the sorted draw functions.
		_lastSortedRoomNumber = NO_VALUE;
		_lastSortedObjectType = RendererObjectType::Unknown;
		_lastSortedObject = nullptr;

		_sortedPolygonsVertices.clear();
		_sortedPolygonsIndices.clear();
		_sortedPolygonsBatches.clear();

		// Type of the last batch actually drawn, used by the draw functions to skip redundant
		// shader and vertex buffer binds when consecutive batches share the object type.
		auto lastObjectType = RendererObjectType::Unknown;

		// Upload the accumulated geometry with a single map per buffer, then draw the recorded
		// batches. Uploading once per flush instead of once per batch keeps the number of
		// WRITE_DISCARD maps (and driver buffer renames) independent from the batch count.
		auto flushBatches = [&]()
		{
			if (_sortedPolygonsBatches.empty())
				return;

			if (!_sortedPolygonsIndices.empty())
			{
				_graphicsDevice->UpdateIndexBuffer(_sortedPolygonsIndexBuffer.get(), (int)_sortedPolygonsIndices.size(), 0, _sortedPolygonsIndices.data());
				_graphicsDevice->BindIndexBuffer(_sortedPolygonsIndexBuffer.get());
			}

			if (!_sortedPolygonsVertices.empty())
				_graphicsDevice->UpdateVertexBuffer(_sortedPolygonsVertexBuffer.get(), 0, (int)_sortedPolygonsVertices.size(), _sortedPolygonsVertices.data());

			for (const auto& batch : _sortedPolygonsBatches)
			{
				switch (batch.Object->ObjectType)
				{
				case RendererObjectType::Room:
					DrawRoomSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::Moveable:
					DrawItemSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::HairPrimary:
				case RendererObjectType::HairSecondary:
					DrawHairSorted(batch.Object, lastObjectType, view, batch.Object->ObjectType == RendererObjectType::HairPrimary ? 0 : 1, batch.Base, batch.Count);
					break;

				case RendererObjectType::Static:
					DrawStaticSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::MoveableAsStatic:
					DrawMoveableAsStaticSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::Effect:
					DrawEffectSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::Sprite:
					DrawSpriteSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				default:
					continue;
				}

				lastObjectType = batch.Object->ObjectType;
			}

			_sortedPolygonsBatches.clear();
			_sortedPolygonsIndices.clear();
			_sortedPolygonsVertices.clear();
		};

		for (int i = 0; i < view.TransparentObjectsToDraw.size(); i++)
		{
			auto* object = &view.TransparentObjectsToDraw[i];

			if (_currentMirror != nullptr && object->ObjectType == RendererObjectType::Room)
				continue;

			int startEntry = i;
			int base = (object->ObjectType == RendererObjectType::Sprite ?
				(int)_sortedPolygonsVertices.size() : (int)_sortedPolygonsIndices.size());
			int count = 0;

			if (object->ObjectType != RendererObjectType::Sprite)
			{
				// All entries of a group share the object type, so the index source only
				// depends on the group head. Moveables, hair, swarm objects and effects
				// all reference moveable geometry.
				int* sourceIndices = nullptr;
				switch (object->ObjectType)
				{
				case RendererObjectType::Room:
					sourceIndices = _roomsIndices.data();
					break;

				case RendererObjectType::Static:
					sourceIndices = _staticsIndices.data();
					break;

				case RendererObjectType::Moveable:
				case RendererObjectType::HairPrimary:
				case RendererObjectType::HairSecondary:
				case RendererObjectType::MoveableAsStatic:
				case RendererObjectType::Effect:
					sourceIndices = _moveablesIndices.data();
					break;

				default:
					continue;
				}

				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].GroupRank == object->GroupRank &&
					_sortedPolygonsIndices.size() + (view.TransparentObjectsToDraw[i].Polygon->Shape == 0 ? 6 : 3) < MAX_TRANSPARENT_VERTICES)
				{
					auto* currentObject = &view.TransparentObjectsToDraw[i];
					_sortedPolygonsIndices.bulk_push_back(
						sourceIndices,
						currentObject->Polygon->BaseIndex,
						currentObject->Polygon->Shape == 0 ? 6 : 3);
					i++;
				}

				count = (int)_sortedPolygonsIndices.size() - base;
			}
			else
			{
				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].GroupRank == object->GroupRank &&
					_sortedPolygonsVertices.size() + 6 < MAX_TRANSPARENT_VERTICES)
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
					v0.Color = VectorColorToRGBA(spr->c1);
					v0.Effects = 0 << INDEX_IN_POLY_VERTEX_SHIFT;

					Vertex v1;
					v1.Position = Vector3::Transform(p1t, world);
					v1.UV = uv1;
					v1.Color = VectorColorToRGBA(spr->c2);
					v1.Effects = 1 << INDEX_IN_POLY_VERTEX_SHIFT;

					Vertex v2;
					v2.Position = Vector3::Transform(p2t, world);
					v2.UV = uv2;
					v2.Color = VectorColorToRGBA(spr->c3);
					v2.Effects = 2 << INDEX_IN_POLY_VERTEX_SHIFT;

					Vertex v3;
					v3.Position = Vector3::Transform(p3t, world);
					v3.UV = uv3;
					v3.Color = VectorColorToRGBA(spr->c4);
					v3.Effects = 3 << INDEX_IN_POLY_VERTEX_SHIFT;

					_sortedPolygonsVertices.push_back(v0);
					_sortedPolygonsVertices.push_back(v1);
					_sortedPolygonsVertices.push_back(v3);
					_sortedPolygonsVertices.push_back(v2);
					_sortedPolygonsVertices.push_back(v3);
					_sortedPolygonsVertices.push_back(v1);

					i++;
				}

				count = (int)_sortedPolygonsVertices.size() - base;
			}

			if (count == 0)
			{
				// The shared buffer is full: draw what has been accumulated so far and retry
				// this entry against the emptied buffers.
				flushBatches();
				i = startEntry - 1;
				continue;
			}

			_sortedPolygonsBatches.push_back({ object, base, count });

			i--;
		}

		flushBatches();

	}

	void Renderer::DrawRoomSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_roomsVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::Rooms);
		}

		// Rebuild and upload the room CB only when the room changes. Consecutive sorted batches
		// from the same room would produce identical CB contents (lights come from the view, not
		// the room), and nothing else touches the room CB during the sorted faces pass.
		if (objectInfo->Room->RoomNumber != _lastSortedRoomNumber)
		{
			RoomData* nativeRoom = &g_Level.Rooms[objectInfo->Room->RoomNumber];

			_stRoom.Caustics =  int(g_Configuration.EnableCaustics && (nativeRoom->flags & ENV_FLAG_WATER) && !(nativeRoom->flags & ENV_FLAG_NOCAUSTICS));
			_stRoom.AmbientColor = Vector3(objectInfo->Room->AmbientLight.x, objectInfo->Room->AmbientLight.y, objectInfo->Room->AmbientLight.z);
			BindRoomLights(view.LightsToDraw);
			_stRoom.NumRoomDecals = 0; // Don't draw decals on sorted faces to avoid slowdowns.
			_stRoom.Water = (nativeRoom->flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
			UpdateConstantBuffer(&_stRoom, _cbRoom.get());

			_lastSortedRoomNumber = objectInfo->Room->RoomNumber;
		}

		SetScissor(objectInfo->Room->ClipBounds);

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Rooms, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedTriangles(count, baseIndex, 0);

		_numSortedRoomsDrawCalls++;
		_numSortedTriangles += count / 3;

		ResetScissor();
	}

	void Renderer::DrawItemSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::Items);
		}

		// Rebuild and upload the objects CB only when the item changes: grouping splits the
		// same item into one batch per bucket which would re-upload identical data.
		if (_lastSortedObjectType != objectInfo->ObjectType || _lastSortedObject != objectInfo->Item)
		{
			// Bind main item properties.
			Matrix world = objectInfo->Item->InterpolatedWorld;
			_stObjects.Objects[0].World = world;
			_stObjects.Objects[0].Color = objectInfo->Item->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Item->AmbientLight;
			_stObjects.Skinned = (int)(objectInfo->Skinned ? SkinningMode::Full : SkinningMode::None);

			const auto& moveableObj = *_moveableObjects[objectInfo->Item->ObjectID];

			if (objectInfo->Skinned)
			{
				for (int m = 0; m < moveableObj.BindPoseTransforms.size(); m++)
					_stObjects.Bones[m] = moveableObj.BindPoseTransforms[m] * objectInfo->Item->InterpolatedAnimationTransforms[m];
			}
			else
			{
				memcpy(_stObjects.Bones, objectInfo->Item->InterpolatedAnimationTransforms, sizeof(Matrix) * BONE_COUNT_MAX);
			}

			for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
				_stObjects.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

			bool acceptsShadows = moveableObj.ShadowType == ShadowMode::None;
			BindMoveableLights(objectInfo->Item->LightsToDraw, objectInfo->Item->RoomNumber, objectInfo->Item->PrevRoomNumber, objectInfo->Item->LightFade, acceptsShadows);

			// Only Objects[0] is used, so upload just the CB prefix.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Item;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Moveables, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedTriangles(count, baseIndex, 0);

		_numSortedMoveablesDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawStaticSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_staticsVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::InstancedStatics);
		}

		// Rebuild and upload the objects CB only when the static changes: grouping splits the
		// same static into one batch per bucket which would re-upload identical data.
		if (_lastSortedObjectType != objectInfo->ObjectType || _lastSortedObject != objectInfo->Static)
		{
			_stObjects.Skinned = (int)SkinningMode::Static;

			auto world = objectInfo->Static->World;
			_stObjects.Objects[0].World = world;

			_stObjects.Objects[0].Color = objectInfo->Static->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Room->AmbientLight;
			_stObjects.Objects[0].LightMode = (int)GetStaticRendererObject(objectInfo->Static->ObjectNumber).ObjectMeshes[0]->LightMode;
			BindInstancedStaticLights(objectInfo->Static->LightsToDraw, 0);

			// Only Objects[0] is used, so upload just the CB prefix.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Static;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Statics, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedInstancedTriangles(count, 1, baseIndex, 0);

		_numSortedStaticsDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawMoveableAsStaticSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		_stObjects.Skinned = (int)SkinningMode::Static;

		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::InstancedStatics);
		}

		auto world = objectInfo->World;
		_stObjects.Objects[0].World = world;

		_stObjects.Objects[0].Color = NEUTRAL_COLOR;
		_stObjects.Objects[0].AmbientLight = objectInfo->Room->AmbientLight;
		_stObjects.Objects[0].LightMode = (int)objectInfo->LightMode;
		BindInstancedStaticLights(objectInfo->Room->LightsToDraw, 0);

		// Only Objects[0] is used, so upload just the CB prefix. No per-object key exists for
		// this type, so the upload always happens and the objects CB cache is invalidated.
		UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));
		_lastSortedObjectType = objectInfo->ObjectType;
		_lastSortedObject = nullptr;

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Statics, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedInstancedTriangles(count, 1, baseIndex, 0);

		_numSortedStaticsDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawEffectSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::InstancedStatics);
		}

		// Rebuild and upload the objects CB only when the effect changes: grouping splits the
		// same effect into one batch per bucket which would re-upload identical data.
		if (_lastSortedObjectType != objectInfo->ObjectType || _lastSortedObject != objectInfo->Effect)
		{
			_stObjects.Skinned = (int)SkinningMode::Static;

			auto world = objectInfo->Effect->InterpolatedWorld;
			_stObjects.Objects[0].World = world;

			_stObjects.Objects[0].Color = objectInfo->Effect->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Effect->AmbientLight;
			_stObjects.Objects[0].LightMode = (int)LightMode::Dynamic;
			BindInstancedStaticLights(objectInfo->Effect->LightsToDraw, 0);

			// Only Objects[0] is used, so upload just the CB prefix.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Effect;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Moveables, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedInstancedTriangles(count, 1, baseIndex, 0);

		_numEffectsDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawHairSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int index, int baseIndex, int count)
	{
		if (index >= HairEffect.Units.size())
		{
			TENLog("Attempt to draw nonexistent hair unit", LogLevel::Warning);
			return;
		}

		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::Items);
		}

		// Rebuild and upload the objects CB only when the hair unit changes: grouping splits
		// the same unit into one batch per bucket which would re-upload identical data.
		// The object type distinguishes the primary and secondary units of the same item.
		if (_lastSortedObjectType != objectInfo->ObjectType || _lastSortedObject != objectInfo->Item)
		{
			// Bind main item properties.
			Matrix world = objectInfo->Item->InterpolatedWorld;
			_stObjects.Objects[0].World = world;
			_stObjects.Objects[0].Color = objectInfo->Item->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Item->AmbientLight;
			_stObjects.Skinned = (int)(objectInfo->Skinned ? SkinningMode::Full : SkinningMode::None);

			const auto& moveableObj = *_moveableObjects[(int)GAME_OBJECT_ID::ID_HAIR_PRIMARY + index];

			_stObjects.Objects[0].World = Matrix::Identity;
			_stObjects.Bones[0] = objectInfo->Item->InterpolatedAnimationTransforms[HairUnit::GetRootMeshID(index)] * objectInfo->Item->InterpolatedWorld;
			ReflectMatrixOptionally(_stObjects.Bones[0]);

			for (int i = 0; i < HairEffect.Units[index].Segments.size(); i++)
			{
				const auto& segment = HairEffect.Units[index].Segments[i];
				auto worldMatrix = segment.GlobalTransform;

				ReflectMatrixOptionally(worldMatrix);

				_stObjects.Bones[i + 1] = worldMatrix;
				_stObjects.BoneLightModes[i] = (int)LightMode::Dynamic;
			}

			for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
				_stObjects.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

			bool acceptsShadows = moveableObj.ShadowType == ShadowMode::None;
			BindMoveableLights(objectInfo->Item->LightsToDraw, objectInfo->Item->RoomNumber, objectInfo->Item->PrevRoomNumber, objectInfo->Item->LightFade, acceptsShadows);

			// Only Objects[0] is used, so upload just the CB prefix.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Item;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Moveables, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedTriangles(count, baseIndex, 0);

		_numSortedMoveablesDrawCalls++;
		_numSortedTriangles += count / 3;
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

		_graphicsDevice->ClearRenderTarget2D(_SSAORenderTarget->GetRenderTarget(), Colors::White);
		_graphicsDevice->BindRenderTarget(_SSAORenderTarget->GetRenderTarget(), nullptr);

		// Must set correctly viewport because SSAO is done at 1/4 screen resolution.
		RendererViewport viewport = { 0, 0, _graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight(), 0.0f, 1.0f };
		_graphicsDevice->SetViewport(viewport);
		_graphicsDevice->SetScissor(viewport);
	
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());

		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), _depthRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), _normalsAndMaterialIndexRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		BindTexture(static_cast<TextureRegister>(2), _SSAONoiseTexture.get(), SamplerStateRegister::PointWrap);

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		_stPostProcessBuffer.TexelSize = Vector2(1.0f / _graphicsDevice->GetScreenWidth(), 1.0f /  _graphicsDevice->GetScreenHeight());
		memcpy(_stPostProcessBuffer.SSAOKernel, _SSAOKernel.data(), 16 * _SSAOKernel.size());
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		DrawTriangles(3, 0);

		// Blur step.
		_shaders.Bind(Shader::SsaoBlur);

		_graphicsDevice->ClearRenderTarget2D(_SSAOBlurredRenderTarget->GetRenderTarget(), Colors::Black);
		_graphicsDevice->BindRenderTarget(_SSAOBlurredRenderTarget->GetRenderTarget(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::SSAO, _SSAORenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
 
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

	void Renderer::SetupAnimatedTextures(const RendererBucket& bucket)
	{
		const auto& set = _animatedTextureSets[bucket.Texture];

		_stPerDraw.Animated = 1;
		_stPerDraw.IsWaterfall = 0;
		_stPerDraw.AnimType = (int)set.Type;

		int frameCount = 0;

		// Stream video texture, if video playback is active, otherwise show original texture.
		if (set.Type == AnimatedTextureType::Video && _videoSprite.Texture && _videoSprite.Texture->IsValid())
		{
			_stPerDraw.AnimFps = 1;
			_stPerDraw.NumAnimFrames = 1;

			// Use normalized UVs, because we are showing the whole video texture on a single face.
			_animatedFrames[0].TopLeft = set.Textures[0].NormalizedUV[0];
			_animatedFrames[0].TopRight = set.Textures[0].NormalizedUV[1];
			_animatedFrames[0].BottomRight = set.Textures[0].NormalizedUV[2];
			_animatedFrames[0].BottomLeft = set.Textures[0].NormalizedUV[3];
			frameCount = 1;
		}
		else if (set.Type == AnimatedTextureType::UVRotate)
		{
			_stPerDraw.AnimFps = set.Fps;
			_stPerDraw.UVRotateDirection = set.UVRotateDirection;
			_stPerDraw.UVRotateSpeed = set.UVRotateSpeed;
			_stPerDraw.NumAnimFrames = 1;

			_animatedFrames[0].TopLeft = set.Textures[0].UV[0];
			_animatedFrames[0].TopRight = set.Textures[0].UV[1];
			_animatedFrames[0].BottomRight = set.Textures[0].UV[2];
			_animatedFrames[0].BottomLeft = set.Textures[0].UV[3];
			frameCount = 1;
		}
		else
		{
			_stPerDraw.AnimFps = set.Fps;
			_stPerDraw.NumAnimFrames = set.NumTextures;

			for (unsigned char j = 0; j < set.NumTextures; j++)
			{
				if (j >= _animatedFrames.size())
				{
					TENLog("Animated frame " + std::to_string(j) + " out of bounds. Too many frames in sequence.");
					break;
				}

				_animatedFrames[j].TopLeft = set.Textures[j].UV[0];
				_animatedFrames[j].TopRight = set.Textures[j].UV[1];
				_animatedFrames[j].BottomRight = set.Textures[j].UV[2];
				_animatedFrames[j].BottomLeft = set.Textures[j].UV[3];
			}
			frameCount = std::min<int>(set.NumTextures, (int)_animatedFrames.size());
		}

		UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());
		_graphicsDevice->UpdateStructuredBuffer(_animatedFramesBuffer.get(), _animatedFrames.data(), frameCount);
	}

	void Renderer::BindBucketTextures(const RendererBucket& bucket, TextureSource textureSource, bool animated)
	{
		if (animated)
		{
			SetupAnimatedTextures(bucket);
		}
		else
		{
			TexturesAreNotAnimated();
		}

		if (animated)
		{
			// If video texture is used, directly bind it as a color map, and optionally as an emissive map, if it was
			// set for the source placeholder texture.

			const auto& set = _animatedTextureSets[bucket.Texture];
				
			if (set.Type == AnimatedTextureType::Video && _videoSprite.Texture && _videoSprite.Texture->IsValid())
			{
				BindTexture(TextureRegister::ColorMap, _videoSprite.Texture, SamplerStateRegister::AnisotropicClamp);

				if (g_Level.Materials[bucket.MaterialIndex].HasEmissiveMap)
					BindTexture(TextureRegister::EmissiveMap, _videoSprite.Texture, SamplerStateRegister::AnisotropicClamp);
			}
			else
				BindAtlasTextures(bucket, TextureSource::Animated);
		}
		else
			BindAtlasTextures(bucket, textureSource);
	}

	void Renderer::BindAtlasTextures(const RendererBucket& bucket, TextureSource textureSource)
	{
		auto* atlasList = &_roomTextures;

		switch (textureSource)
		{
			case TextureSource::Rooms:     atlasList = &_roomTextures;      break;
			case TextureSource::Moveables: atlasList = &_moveablesTextures; break;
			case TextureSource::Statics:   atlasList = &_staticTextures;    break;
			case TextureSource::Animated:  atlasList = &_animatedTextures;  break;
			default: return;
		}

		auto& atlas = (*atlasList)[bucket.Texture];

		BindTexture(TextureRegister::ColorMap, std::get<0>(atlas).get(), SamplerStateRegister::AnisotropicClamp);
		BindTexture(TextureRegister::NormalMap, std::get<1>(atlas).get(), SamplerStateRegister::AnisotropicClamp);
		BindTexture(TextureRegister::ORSHMap, std::get<2>(atlas).get(), SamplerStateRegister::AnisotropicClamp);
		BindTexture(TextureRegister::EmissiveMap, std::get<3>(atlas).get(), SamplerStateRegister::AnisotropicClamp);
	}
}
