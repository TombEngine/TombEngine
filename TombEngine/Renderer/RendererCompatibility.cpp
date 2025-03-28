#include "framework.h"
#include "Renderer/Renderer.h"

#include <execution>
#include <stack>
#include <tuple>

#include "Game/control/control.h"
#include "Game/effects/Hair.h"
#include "Game/Lara/lara_struct.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/objects.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/level.h"

using namespace TEN::Effects::Hair;
using namespace TEN::Renderer::Graphics;

namespace TEN::Renderer
{
	template class VertexBuffer<Vertex>;

	bool Renderer::PrepareDataForTheRenderer()
	{
		TENLog("Preparing renderer...", LogLevel::Info);

		_lastBlendMode = BlendMode::Unknown;
		_lastCullMode = CullMode::Unknown;
		_lastDepthState = DepthState::Unknown;

		_moveableObjects.resize(ID_NUMBER_OBJECTS);
		_spriteSequences.resize(ID_NUMBER_OBJECTS);
		_rooms.resize(g_Level.Rooms.size());

		_meshes.clear();

		_dynamicLightList = 0;
		for (auto& dynamicLightList : _dynamicLights)
			dynamicLightList.clear();

		int allocatedItemSize = (int)g_Level.Items.size() + MAX_SPAWNED_ITEM_COUNT;

		auto item = RendererItem();
		_items = std::vector<RendererItem>(allocatedItemSize, item);

		auto effect = RendererEffect();
		_effects = std::vector<RendererEffect>(allocatedItemSize, effect);

		TENLog("Allocated renderer object memory.", LogLevel::Info);

		_animatedTextures.resize(g_Level.AnimatedTextures.size());
		for (int i = 0; i < g_Level.AnimatedTextures.size(); i++)
		{
			TEXTURE* texture = &g_Level.AnimatedTextures[i];
			Texture2D normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultNormalTexture();
			}
			else
			{
				normal = Texture2D(_device.Get(), texture->normalMapData.data(), (int)texture->normalMapData.size());
			}

			TexturePair tex = std::make_tuple(Texture2D(_device.Get(), texture->colorMapData.data(), (int)texture->colorMapData.size()), normal);
			_animatedTextures[i] = tex;
		}

		if (_animatedTextures.size() > 0)
			TENLog("Generated " + std::to_string(_animatedTextures.size()) + " animated textures.", LogLevel::Info);

		std::transform(g_Level.AnimatedTexturesSequences.begin(), g_Level.AnimatedTexturesSequences.end(), std::back_inserter(_animatedTextureSets), [](ANIMATED_TEXTURES_SEQUENCE& sequence) {
			RendererAnimatedTextureSet set{};
			set.NumTextures = sequence.numFrames;
			std::transform(sequence.frames.begin(), sequence.frames.end(), std::back_inserter(set.Textures), [](ANIMATED_TEXTURES_FRAME& frm) {
				RendererAnimatedTexture tex{};
				tex.UV[0].x = frm.x1;
				tex.UV[0].y = frm.y1;
				tex.UV[1].x = frm.x2;
				tex.UV[1].y = frm.y2;
				tex.UV[2].x = frm.x3;
				tex.UV[2].y = frm.y3;
				tex.UV[3].x = frm.x4;
				tex.UV[3].y = frm.y4;
				return tex;
			});
			set.Fps = sequence.Fps;
			return set;
		});

		if (_animatedTextureSets.size() > 0)
			TENLog("Generated " + std::to_string(_animatedTextureSets.size()) + " animated texture sets.", LogLevel::Info);

		_roomTextures.resize(g_Level.RoomTextures.size());
		for (int i = 0; i < g_Level.RoomTextures.size(); i++)
		{
			TEXTURE *texture = &g_Level.RoomTextures[i];
			Texture2D normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultNormalTexture();
			}
			else
			{
				normal = Texture2D(_device.Get(), texture->normalMapData.data(), (int)texture->normalMapData.size());
			}

			TexturePair tex = std::make_tuple(Texture2D(_device.Get(), texture->colorMapData.data(), (int)texture->colorMapData.size()), normal);
			_roomTextures[i] = tex;

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump\\room_%d.png", i);

			std::ofstream outfile(filename, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (_roomTextures.size() > 0)
			TENLog("Generated " + std::to_string(_roomTextures.size()) + " room texture atlases.", LogLevel::Info);

		_moveablesTextures.resize(g_Level.MoveablesTextures.size());
		for (int i = 0; i < g_Level.MoveablesTextures.size(); i++)
		{
			TEXTURE *texture = &g_Level.MoveablesTextures[i];
			Texture2D normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultNormalTexture();
			}
			else
			{
				normal = Texture2D(_device.Get(), texture->normalMapData.data(), (int)texture->normalMapData.size());
			}

			TexturePair tex = std::make_tuple(Texture2D(_device.Get(), texture->colorMapData.data(), (int)texture->colorMapData.size()), normal);
			_moveablesTextures[i] = tex;

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump\\moveable_%d.png", i);

			std::ofstream outfile(filename, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (_moveablesTextures.size() > 0)
			TENLog("Generated " + std::to_string(_moveablesTextures.size()) + " moveable texture atlases.", LogLevel::Info);

		_staticTextures.resize(g_Level.StaticsTextures.size());
		for (int i = 0; i < g_Level.StaticsTextures.size(); i++)
		{
			TEXTURE *texture = &g_Level.StaticsTextures[i];
			Texture2D normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultNormalTexture();
			}
			else
			{
				normal = Texture2D(_device.Get(), texture->normalMapData.data(), (int)texture->normalMapData.size());
			}

			TexturePair tex = std::make_tuple(Texture2D(_device.Get(), texture->colorMapData.data(), (int)texture->colorMapData.size()), normal);
			_staticTextures[i] = tex;

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump\\static_%d.png", i);

			std::ofstream outfile(filename, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (_staticTextures.size() > 0)
			TENLog("Generated " + std::to_string(_staticTextures.size()) + " static mesh texture atlases.", LogLevel::Info);

		_spritesTextures.resize(g_Level.SpritesTextures.size());
		for (int i = 0; i < g_Level.SpritesTextures.size(); i++)
		{
			auto& texture = g_Level.SpritesTextures[i];
			_spritesTextures[i] = Texture2D(_device.Get(), texture.colorMapData.data(), (int)texture.colorMapData.size());
		}

		if (_spritesTextures.size() > 0)
			TENLog("Generated " + std::to_string((int)_spritesTextures.size()) + " sprite atlases.", LogLevel::Info);

		_skyTexture = Texture2D(_device.Get(), g_Level.SkyTexture.colorMapData.data(), (int)g_Level.SkyTexture.colorMapData.size());

		TENLog("Loaded sky texture.", LogLevel::Info);

		int totalVertices = 0;
		int totalIndices = 0;
		for (auto& room : g_Level.Rooms)
			for (auto& bucket : room.buckets)
			{ 
				totalVertices += bucket.numQuads * 4 + bucket.numTriangles * 3;
				totalIndices += bucket.numQuads * 6 + bucket.numTriangles * 3;
			}

		if (!totalVertices || !totalIndices)
			throw std::exception("Level has no textured room geometry.");

		_roomsVertices.resize(totalVertices);
		_roomsIndices.resize(totalIndices);

		TENLog("Loaded total " + std::to_string(totalVertices) + " room vertices.", LogLevel::Info);

		int lastVertex = 0;
		int lastIndex = 0;

		TENLog("Preparing room data...", LogLevel::Info);

		for (int i = 0; i < g_Level.Rooms.size(); i++)
		{
			auto& room = g_Level.Rooms[i];
			auto& rendererRoom = _rooms[i];

			rendererRoom.RoomNumber = i;
			rendererRoom.AmbientLight = Vector4(room.ambient.x, room.ambient.y, room.ambient.z, 1.0f);
			rendererRoom.ItemsToDraw.reserve(MAX_ITEMS_DRAW);
			rendererRoom.EffectsToDraw.reserve(MAX_ITEMS_DRAW);

			auto boxMin = Vector3(room.Position.x + BLOCK(1), room.TopHeight - CLICK(1), room.Position.z + BLOCK(1));
			auto boxMax = Vector3(room.Position.x + (room.XSize - 1) * BLOCK(1), room.BottomHeight + CLICK(1), room.Position.z + (room.ZSize - 1) * BLOCK(1));
			auto center = (boxMin + boxMax) / 2.0f;
			auto extents = boxMax - center;
			rendererRoom.BoundingBox = BoundingBox(center, extents);

			rendererRoom.Neighbors.clear();
			for (int j : room.NeighborRoomNumbers)
			{
				if (g_Level.Rooms[j].Active())
					rendererRoom.Neighbors.push_back(j);
			}

			if (!room.Portals.empty())
			{
				rendererRoom.Doors.resize((int)room.Portals.size());

				for (int j = 0; j < room.Portals.size(); j++)
				{
					const auto& portal = room.Portals[j];
					auto& rendererDoor = rendererRoom.Doors[j];

					rendererDoor.RoomNumber = portal.RoomNumber;
					rendererDoor.Normal = portal.Normal;

					for (int k = 0; k < 4; k++)
					{
						rendererDoor.AbsoluteVertices[k] = Vector4(
							room.Position.x + portal.Vertices[k].x,
							room.Position.y + portal.Vertices[k].y,
							room.Position.z + portal.Vertices[k].z,
							1.0f);
					}
				}
			}

			if (room.mesh.size() != 0)
			{
				rendererRoom.Statics.resize(room.mesh.size());

				for (int l = 0; l < (int)room.mesh.size(); l++)
				{
					RendererStatic* staticInfo = &rendererRoom.Statics[l];
					MESH_INFO* oldMesh = &room.mesh[l];

					oldMesh->Dirty = true;

					staticInfo->ObjectNumber = oldMesh->staticNumber;
					staticInfo->RoomNumber = oldMesh->roomNumber;
					staticInfo->Color = oldMesh->color;
					staticInfo->AmbientLight = rendererRoom.AmbientLight;
					staticInfo->Pose = oldMesh->pos;
					staticInfo->Scale = oldMesh->scale;
					staticInfo->OriginalSphere = Statics[staticInfo->ObjectNumber].visibilityBox.ToLocalBoundingSphere();
					staticInfo->IndexInRoom = l;

					staticInfo->Update();
				}
			}

			if (room.positions.empty())
				continue;
			
			for (auto& levelBucket : room.buckets)
			{
				RendererBucket bucket{};

				bucket.Animated = levelBucket.animated;
				bucket.BlendMode = static_cast<BlendMode>(levelBucket.blendMode);
				bucket.Texture = levelBucket.texture;
				bucket.StartVertex = lastVertex;
				bucket.StartIndex = lastIndex;
				bucket.NumVertices += levelBucket.numQuads * 4 + levelBucket.numTriangles * 3;
				bucket.NumIndices += levelBucket.numQuads * 6 + levelBucket.numTriangles * 3;
				bucket.Centre = Vector3::Zero;

				for (auto& poly : levelBucket.polygons)
				{
					RendererPolygon newPoly;

					newPoly.Shape = poly.shape;

					newPoly.Centre = (
						room.positions[poly.indices[0]] +
						room.positions[poly.indices[1]] +
						room.positions[poly.indices[2]]) / 3.0f;

					Vector3 p1 = room.positions[poly.indices[0]];
					Vector3 p2 = room.positions[poly.indices[1]];
					Vector3 p3 = room.positions[poly.indices[2]];

					Vector3 n = (p2 - p1).Cross(p3 - p1);
					n.Normalize();

					newPoly.Normal = n;
					
					int baseVertices = lastVertex;
					for (int k = 0; k < poly.indices.size(); k++)
					{
						Vertex* vertex = &_roomsVertices[lastVertex];
						int index = poly.indices[k];

						vertex->Position.x = room.Position.x + room.positions[index].x;
						vertex->Position.y = room.Position.y + room.positions[index].y;
						vertex->Position.z = room.Position.z + room.positions[index].z;

						bucket.Centre += vertex->Position;

						vertex->Normal = poly.normals[k];
						vertex->UV = poly.textureCoordinates[k];
						vertex->Color = Vector4(room.colors[index].x, room.colors[index].y, room.colors[index].z, 1.0f);
						vertex->Tangent = poly.tangents[k];
						vertex->Binormal = poly.binormals[k];
						vertex->AnimationFrameOffset = poly.animatedFrame;
						vertex->IndexInPoly = k;
						vertex->OriginalIndex = index;
						vertex->Effects = Vector4(room.effects[index].x, room.effects[index].y, room.effects[index].z, 0);

						const unsigned long long primes[]{ 73856093ULL, 19349663ULL, 83492791ULL };
						vertex->Hash = (unsigned int)std::hash<float>{}
						((vertex->Position.x)* primes[0]) ^
							((unsigned int)std::hash<float>{}(vertex->Position.y) * primes[1]) ^
							(unsigned int)std::hash<float>{}(vertex->Position.z) * primes[2];
						vertex->Bone = 0;

						lastVertex++;
					}

					if (poly.shape == 0)
					{
						newPoly.BaseIndex = lastIndex;

						_roomsIndices[lastIndex + 0] = baseVertices + 0;
						_roomsIndices[lastIndex + 1] = baseVertices + 1;
						_roomsIndices[lastIndex + 2] = baseVertices + 3;
						_roomsIndices[lastIndex + 3] = baseVertices + 2;
						_roomsIndices[lastIndex + 4] = baseVertices + 3;
						_roomsIndices[lastIndex + 5] = baseVertices + 1;

						lastIndex += 6;
					}
					else
					{
						newPoly.BaseIndex = lastIndex;
 
						_roomsIndices[lastIndex + 0] = baseVertices + 0;
						_roomsIndices[lastIndex + 1] = baseVertices + 1;
						_roomsIndices[lastIndex + 2] = baseVertices + 2;

						lastIndex += 3;
					}

					bucket.Polygons.push_back(newPoly);
				}

				bucket.Centre /= bucket.NumIndices;

				rendererRoom.Buckets.push_back(bucket);		
			}

			if (room.lights.size() != 0)
			{
				rendererRoom.Lights.resize(room.lights.size());

				for (int l = 0; l < room.lights.size(); l++)
				{
					RendererLight* light = &rendererRoom.Lights[l];
					RoomLightData* oldLight = &room.lights[l];

					if (oldLight->type == 0)
					{
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->Direction = Vector3(oldLight->dx, oldLight->dy, oldLight->dz);
						light->CastShadows = oldLight->castShadows;
						light->Type = LightType::Sun;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == 1)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->CastShadows = oldLight->castShadows;
						light->Type = LightType::Point;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == 3)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->CastShadows = false;
						light->Type = LightType::Shadow;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == 2)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->Direction = Vector3(oldLight->dx, oldLight->dy, oldLight->dz);
						light->In = oldLight->length;     
						light->Out = oldLight->cutoff;
						light->InRange = oldLight->in;
						light->OutRange = oldLight->out;
						light->CastShadows = oldLight->castShadows;
						light->Type = LightType::Spot;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == 4)
					{  
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b);
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->Type = LightType::FogBulb;
						light->Luma = Luma(light->Color);
					} 

					// Monty's temp variables for sorting
					light->LocalIntensity = 0;
					light->Distance = 0;
					light->RoomNumber = i;
					light->AffectNeighbourRooms = light->Type != LightType::Sun;

					oldLight++;
				}
			}
		}
		_roomsVertexBuffer = VertexBuffer<Vertex>(_device.Get(), (int)_roomsVertices.size(), &_roomsVertices[0]);
		_roomsIndexBuffer = IndexBuffer(_device.Get(), (int)_roomsIndices.size(), _roomsIndices.data());

		std::for_each(std::execution::par_unseq,
			_rooms.begin(),
			_rooms.end(),
			[](RendererRoom& room)
			{
				std::sort(
					room.Buckets.begin(),
					room.Buckets.end(),
					[](RendererBucket& a, RendererBucket& b)
					{
						if (a.BlendMode == b.BlendMode)
							return (a.Texture < b.Texture);
						else
							return (a.BlendMode < b.BlendMode);
					}
				);
			}
		);

		TENLog("Preparing object data...", LogLevel::Info);
			 
		bool isSkinPresent = false;

		totalVertices = 0;
		totalIndices = 0;
		for (int i = 0; i < MoveablesIds.size(); i++)
		{
			int objNum = MoveablesIds[i];
			ObjectInfo* obj = &Objects[objNum];

			for (int j = 0; j < obj->nmeshes; j++)
			{
				MESH* mesh = &g_Level.Meshes[obj->meshIndex + j];

				for (auto& bucket : mesh->buckets)
				{
					totalVertices += bucket.numQuads * 4 + bucket.numTriangles * 3;
					totalIndices += bucket.numQuads * 6 + bucket.numTriangles * 3;
				}
			}
		}
		_moveablesVertices.resize(totalVertices);
		_moveablesIndices.resize(totalIndices);

		lastVertex = 0;
		lastIndex = 0;
		for (int i = 0; i < MoveablesIds.size(); i++)
		{
			int objNum = MoveablesIds[i];
			ObjectInfo *obj = &Objects[objNum];

			if (obj->nmeshes > 0)
			{
				_moveableObjects[MoveablesIds[i]] = RendererObject();
				RendererObject &moveable = *_moveableObjects[MoveablesIds[i]];
				moveable.Id = MoveablesIds[i];
				moveable.DoNotDraw = (obj->drawRoutine == nullptr);
				moveable.ShadowType = obj->shadowType;
													   
				for (int j = 0; j < obj->nmeshes; j++)
				{              
					// HACK: mesh pointer 0 is the placeholder for Lara's body parts and is right hand with pistols
					// We need to override the bone index because the engine will take mesh 0 while drawing pistols anim,
					// and vertices have bone index 0 and not 10.
					RendererMesh *mesh = GetRendererMeshFromTrMesh(
						&moveable,
						&g_Level.Meshes[obj->meshIndex + j],
						j, MoveablesIds[i] == ID_LARA_SKIN_JOINTS,
						MoveablesIds[i] == ID_HAIR_PRIMARY || MoveablesIds[i] == ID_HAIR_SECONDARY, &lastVertex, &lastIndex);

					moveable.ObjectMeshes.push_back(mesh);
					_meshes.push_back(mesh);
				}

				if (objNum == ID_IMP_ROCK || objNum == ID_ENERGY_BUBBLES || objNum == ID_BUBBLES || objNum == ID_BODY_PART)
				{
					// HACK: these objects must have nmeshes = 0 because engine will use them in a different way while drawing Effects.
					// In Core's code this was done in SETUP.C but we must do it here because we need to create renderer's meshes.
					obj->nmeshes = 0;
				}
				else
				{
					for (int j = 0; j < obj->nmeshes; j++)
					{
						moveable.LinearizedBones.push_back(new RendererBone(j));
						moveable.AnimationTransforms.push_back(Matrix::Identity);
						moveable.BindPoseTransforms.push_back(Matrix::Identity);
					}

					if (obj->nmeshes > 1)
					{
						int *bone = &g_Level.Bones[obj->boneIndex];

						std::stack<RendererBone *> stack;

						RendererBone *currentBone = moveable.LinearizedBones[0];
						RendererBone *stackBone = moveable.LinearizedBones[0];

						for (int mi = 0; mi < obj->nmeshes - 1; mi++)
						{
							int j = mi + 1;

							int opcode = *(bone++);
							int linkX = *(bone++);
							int linkY = *(bone++);
							int linkZ = *(bone++);

							byte flags = opcode & 0x1C;

							moveable.LinearizedBones[j]->ExtraRotationFlags = flags;

							switch (opcode & 0x03)
							{
							case 0:
								moveable.LinearizedBones[j]->Parent = currentBone;
								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 1:
								if (stack.empty())
									continue;

								currentBone = stack.top();
								stack.pop();

								moveable.LinearizedBones[j]->Parent = currentBone;
								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 2:
								stack.push(currentBone);

								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								moveable.LinearizedBones[j]->Parent = currentBone;
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 3:
								if (stack.empty())
									continue;

								RendererBone *theBone = stack.top();
								stack.pop();

								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								moveable.LinearizedBones[j]->Parent = theBone;
								theBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								stack.push(theBone);
								break;
							}
						}
					}

					for (int n = 0; n < obj->nmeshes; n++)
					{
						moveable.LinearizedBones[n]->Transform = Matrix::CreateTranslation(
							moveable.LinearizedBones[n]->Translation.x,
							moveable.LinearizedBones[n]->Translation.y,
							moveable.LinearizedBones[n]->Translation.z);
					}

					moveable.Skeleton = moveable.LinearizedBones[0];
					BuildHierarchy(&moveable);

					// Fix player skin joints and hair units.
					if (MoveablesIds[i] == ID_LARA_SKIN_JOINTS)
					{
						isSkinPresent = true;
						int bonesToCheck[2] = { 0, 0 };

						const auto& objSkin = GetRendererObject(GAME_OBJECT_ID::ID_LARA_SKIN);

						for (int j = 1; j < obj->nmeshes; j++)
						{
							const auto* jointMesh = moveable.ObjectMeshes[j];
							const auto* jointBone = moveable.LinearizedBones[j];

							bonesToCheck[0] = jointBone->Parent->Index;
							bonesToCheck[1] = j;

							for (int b1 = 0; b1 < jointMesh->Buckets.size(); b1++)
							{
								const auto* jointBucket = &jointMesh->Buckets[b1];

								for (int v1 = 0; v1 < jointBucket->NumVertices; v1++)
								{
									auto* jointVertex = &_moveablesVertices[jointBucket->StartVertex + v1];

									bool isDone = false;

									for (int k = 0; k < 2; k++)
									{
										const auto* skinMesh = objSkin.ObjectMeshes[bonesToCheck[k]];
										const auto* skinBone = objSkin.LinearizedBones[bonesToCheck[k]];

										for (int b2 = 0; b2 < skinMesh->Buckets.size(); b2++)
										{
											const auto* skinBucket = &skinMesh->Buckets[b2];
											for (int v2 = 0; v2 < skinBucket->NumVertices; v2++)
											{
												auto* skinVertex = &_moveablesVertices[skinBucket->StartVertex + v2];

												// NOTE: Don't vectorize these coordinates, it breaks the connection in some cases. -- Lwmte, 21.12.24

												int x1 = _moveablesVertices[jointBucket->StartVertex + v1].Position.x + jointBone->GlobalTranslation.x;
												int y1 = _moveablesVertices[jointBucket->StartVertex + v1].Position.y + jointBone->GlobalTranslation.y;
												int z1 = _moveablesVertices[jointBucket->StartVertex + v1].Position.z + jointBone->GlobalTranslation.z;

												int x2 = _moveablesVertices[skinBucket->StartVertex + v2].Position.x + skinBone->GlobalTranslation.x;
												int y2 = _moveablesVertices[skinBucket->StartVertex + v2].Position.y + skinBone->GlobalTranslation.y;
												int z2 = _moveablesVertices[skinBucket->StartVertex + v2].Position.z + skinBone->GlobalTranslation.z;

												if (abs(x1 - x2) < 2 && abs(y1 - y2) < 2 && abs(z1 - z2) < 2)
												{
													jointVertex->Bone = bonesToCheck[k];
													jointVertex->Position = skinVertex->Position;
													jointVertex->Normal = skinVertex->Normal;

													isDone = true;
													break;
												}
											}

											if (isDone)
												break;
										}

										if (isDone)
											break;
									}
								}
							}
						}
					}
					else if ((MoveablesIds[i] == ID_HAIR_PRIMARY || MoveablesIds[i] == ID_HAIR_SECONDARY) && isSkinPresent)
					{
						bool isYoung = (g_GameFlow->GetLevel(CurrentLevel)->GetLaraType() == LaraType::Young);
						bool isSecond = isYoung && MoveablesIds[i] == ID_HAIR_SECONDARY;
						const auto& skinObj = GetRendererObject(GAME_OBJECT_ID::ID_LARA_SKIN);
						const auto& settings = g_GameFlow->GetSettings()->Hair;

						for (int j = 0; j < obj->nmeshes; j++)
						{
							const auto* currentMesh = moveable.ObjectMeshes[j];
							const auto* currentBone = moveable.LinearizedBones[j];

							for (const auto& currentBucket : currentMesh->Buckets)
							{
								for (int v1 = 0; v1 < currentBucket.NumVertices; v1++)
								{
									auto* currentVertex = &_moveablesVertices[currentBucket.StartVertex + v1];
									currentVertex->Bone = j + 1;

									// Link mesh 0 to root mesh.
									if (j == 0)
									{
										const auto& vertices0 = isYoung ? settings[(int)PlayerHairType::YoungLeft].Indices :
																		  settings[(int)PlayerHairType::Normal].Indices;

										const auto& vertices1 = isYoung ? settings[(int)PlayerHairType::YoungRight].Indices :
																		  settings[(int)PlayerHairType::Normal].Indices;

										int rootMesh = HairUnit::GetRootMeshID(isSecond ? 1 : 0);

										const auto* parentMesh = skinObj.ObjectMeshes[rootMesh];
										const auto* parentBone = skinObj.LinearizedBones[rootMesh];

										// Link listed vertices.
										if ((!isSecond && currentVertex->OriginalIndex >= vertices0.size()) || 
											 (isSecond && currentVertex->OriginalIndex >= vertices1.size()))
										{
											continue;
										}

										for (int b2 = 0; b2 < parentMesh->Buckets.size(); b2++)
										{
											const auto* parentBucket = &parentMesh->Buckets[b2];
											for (int v2 = 0; v2 < parentBucket->NumVertices; v2++)
											{
												const auto* parentVertex = &_moveablesVertices[parentBucket->StartVertex + v2];
												if ((parentVertex->OriginalIndex == vertices1[currentVertex->OriginalIndex] &&  isSecond) ||
													(parentVertex->OriginalIndex == vertices0[currentVertex->OriginalIndex] && !isSecond))
												{
													currentVertex->Bone = 0;
													currentVertex->Position = parentVertex->Position;
													currentVertex->Normal = parentVertex->Normal;
												}
											}
										}
									}
									// Link meshes > 0 to parent meshes.
									else
									{
										const auto* parentMesh = moveable.ObjectMeshes[j - 1];
										const auto* parentBone = moveable.LinearizedBones[j - 1];

										for (int b2 = 0; b2 < parentMesh->Buckets.size(); b2++)
										{
											const auto* parentBucket = &parentMesh->Buckets[b2];
											for (int v2 = 0; v2 < parentBucket->NumVertices; v2++)
											{
												auto* parentVertex = &_moveablesVertices[parentBucket->StartVertex + v2];

												int x1 = _moveablesVertices[currentBucket.StartVertex + v1].Position.x + currentBone->GlobalTranslation.x;
												int y1 = _moveablesVertices[currentBucket.StartVertex + v1].Position.y + currentBone->GlobalTranslation.y;
												int z1 = _moveablesVertices[currentBucket.StartVertex + v1].Position.z + currentBone->GlobalTranslation.z;

												int x2 = _moveablesVertices[parentBucket->StartVertex + v2].Position.x + parentBone->GlobalTranslation.x;
												int y2 = _moveablesVertices[parentBucket->StartVertex + v2].Position.y + parentBone->GlobalTranslation.y;
												int z2 = _moveablesVertices[parentBucket->StartVertex + v2].Position.z + parentBone->GlobalTranslation.z;

												// FIXME: If a tolerance is used, a strange bug occurs where certain vertices don't connect. -- Lwmte, 14.12.2024

												if (abs(x1 - x2) == 0 && abs(y1 - y2) == 0 && abs(z1 - z2) == 0)
												{
													currentVertex->Bone = j;
													currentVertex->Position = parentVertex->Position;
													currentVertex->Normal = parentVertex->Normal;
													break;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		_moveablesVertexBuffer = VertexBuffer<Vertex>(_device.Get(), (int)_moveablesVertices.size(), &_moveablesVertices[0]);
		_moveablesIndexBuffer = IndexBuffer(_device.Get(), (int)_moveablesIndices.size(), _moveablesIndices.data());

		TENLog("Preparing static mesh data...", LogLevel::Info);

		totalVertices = 0;
		totalIndices = 0;
		for (const auto& staticObj : Statics)
		{
			const auto& mesh = g_Level.Meshes[staticObj.meshNumber];
			for (const auto& bucket : mesh.buckets)
			{
				totalVertices += (bucket.numQuads * 4) + (bucket.numTriangles * 3);
				totalIndices += (bucket.numQuads * 6) + (bucket.numTriangles * 3);
			}
		}

		_staticsVertices.resize(totalVertices);
		_staticsIndices.resize(totalIndices);

		lastVertex = 0;
		lastIndex = 0;
		for (const auto& staticObj : Statics)
		{
			auto newStaticObj = RendererObject();
			newStaticObj.Type = 1;
			newStaticObj.Id = staticObj.ObjectNumber;

			auto& mesh = *GetRendererMeshFromTrMesh(&newStaticObj, &g_Level.Meshes[staticObj.meshNumber], 0, false, false, &lastVertex, &lastIndex);

			newStaticObj.ObjectMeshes.push_back(&mesh);
			_meshes.push_back(&mesh);

			_staticObjects.push_back(newStaticObj);
		}

		_staticsVertexBuffer = VertexBuffer<Vertex>(_device.Get(), (int)_staticsVertices.size(), _staticsVertices.data());
		_staticsIndexBuffer = IndexBuffer(_device.Get(), (int)_staticsIndices.size(), _staticsIndices.data());

		TENLog("Preparing sprite data...", LogLevel::Info);
		
		// Step 5: prepare sprites
		_sprites.resize(g_Level.Sprites.size());

		for (int i = 0; i < g_Level.Sprites.size(); i++)
		{
			SPRITE *oldSprite = &g_Level.Sprites[i];
			_sprites[i] = RendererSprite();
			RendererSprite &sprite = _sprites[i];

			sprite.UV[0] = Vector2(oldSprite->x1, oldSprite->y1);
			sprite.UV[1] = Vector2(oldSprite->x2, oldSprite->y2);
			sprite.UV[2] = Vector2(oldSprite->x3, oldSprite->y3);
			sprite.UV[3] = Vector2(oldSprite->x4, oldSprite->y4);
			sprite.Texture = &_spritesTextures[oldSprite->tile];
			sprite.Width = round((oldSprite->x2 - oldSprite->x1) * (float)sprite.Texture->Width + 1.0f);
			sprite.Height = round((oldSprite->y3 - oldSprite->y2) * (float)sprite.Texture->Height + 1.0f);
			sprite.X = oldSprite->x1 * sprite.Texture->Width;
			sprite.Y = oldSprite->y1 * sprite.Texture->Height;
		}

		for (int i = 0; i < SpriteSequencesIds.size(); i++)
		{
			ObjectInfo *obj = &Objects[SpriteSequencesIds[i]];

			if (obj->nmeshes < 0)
			{
				short numSprites = abs(obj->nmeshes);
				short baseSprite = obj->meshIndex;
				_spriteSequences[SpriteSequencesIds[i]] = RendererSpriteSequence();

				// TODO: Why a custom =& operator is needed? It creates everytime new N null sprites
				RendererSpriteSequence &sequence = _spriteSequences[SpriteSequencesIds[i]];

				sequence.NumSprites = numSprites;
				sequence.SpritesList.resize(numSprites);
				for (int j = baseSprite; j < baseSprite + numSprites; j++)
				{
					sequence.SpritesList[j - baseSprite] = &_sprites[j];
				}

				_spriteSequences[SpriteSequencesIds[i]] = sequence;

				if (SpriteSequencesIds[i] == ID_CAUSTIC_TEXTURES)
				{
					_causticTextures.clear();
					for (int j = 0; j < sequence.SpritesList.size(); j++)
					{
						_causticTextures.push_back(
							Texture2D(
								_device.Get(),
								_context.Get(),
								sequence.SpritesList[j]->Texture->Texture.Get(),
								sequence.SpritesList[j]->X,
								sequence.SpritesList[j]->Y,
								sequence.SpritesList[j]->Width,
								sequence.SpritesList[j]->Height
							)
						);
					}
				}
			}
		}

		return true;
	}

	RendererMesh* Renderer::GetRendererMeshFromTrMesh(RendererObject* obj, MESH* meshPtr, short boneIndex, int isJoints, int isHairs, int* lastVertex, int* lastIndex)
	{
		RendererMesh* mesh = new RendererMesh();

		mesh->Sphere = meshPtr->sphere;
		mesh->LightMode = meshPtr->lightMode;

		if (meshPtr->positions.empty())
			return mesh;

		mesh->Positions.resize(meshPtr->positions.size());
		for (int i = 0; i < meshPtr->positions.size(); i++)
			mesh->Positions[i] = meshPtr->positions[i];

		for (int n = 0; n < meshPtr->buckets.size(); n++)
		{
			BUCKET* levelBucket = &meshPtr->buckets[n];
			RendererBucket bucket{};
			bucket.Animated = levelBucket->animated;
			bucket.Texture = levelBucket->texture;
			bucket.BlendMode = static_cast<BlendMode>(levelBucket->blendMode);
			bucket.StartVertex = *lastVertex;
			bucket.StartIndex = *lastIndex;
			bucket.NumVertices = levelBucket->numQuads * 4 + levelBucket->numTriangles * 3;
			bucket.NumIndices = levelBucket->numQuads * 6 + levelBucket->numTriangles * 3;

			for (int p = 0; p < (int)levelBucket->polygons.size(); p++)
			{
				POLYGON* poly = &levelBucket->polygons[p];
				RendererPolygon newPoly;

				newPoly.Shape = poly->shape;
				newPoly.Centre = (
					meshPtr->positions[poly->indices[0]] +
					meshPtr->positions[poly->indices[1]] +
					meshPtr->positions[poly->indices[2]]) / 3.0f;

				int baseVertices = *lastVertex;

				for (int k = 0; k < (int)poly->indices.size(); k++)
				{
					Vertex vertex;
					int v = poly->indices[k];

					vertex.Position.x = meshPtr->positions[v].x;
					vertex.Position.y = meshPtr->positions[v].y;
					vertex.Position.z = meshPtr->positions[v].z;
					 
					vertex.Normal.x = poly->normals[k].x;
					vertex.Normal.y = poly->normals[k].y;
					vertex.Normal.z = poly->normals[k].z;

					vertex.Tangent.x = poly->tangents[k].x;
					vertex.Tangent.y = poly->tangents[k].y;
					vertex.Tangent.z = poly->tangents[k].z;

					vertex.Binormal.x = poly->binormals[k].x;
					vertex.Binormal.y = poly->binormals[k].y;
					vertex.Binormal.z = poly->binormals[k].z;

					vertex.UV.x = poly->textureCoordinates[k].x;
					vertex.UV.y = poly->textureCoordinates[k].y;

					vertex.Color.x = meshPtr->colors[v].x;
					vertex.Color.y = meshPtr->colors[v].y;
					vertex.Color.z = meshPtr->colors[v].z;
					vertex.Color.w = 1.0f;

					vertex.Bone = meshPtr->bones[v];
					vertex.OriginalIndex = v;

					vertex.Effects = Vector4(meshPtr->effects[v].x, meshPtr->effects[v].y, meshPtr->effects[v].z, poly->shineStrength);
					vertex.Hash = (unsigned int)std::hash<float>{}
						(vertex.Position.x) ^
							(unsigned int)std::hash<float>{}(vertex.Position.y) ^
							(unsigned int)std::hash<float>{}(vertex.Position.z);

					if (obj->Type == 0)
						_moveablesVertices[*lastVertex] = vertex;
					else
						_staticsVertices[*lastVertex] = vertex;

					*lastVertex = *lastVertex + 1;
				}

				if (poly->shape == 0)
				{
					newPoly.BaseIndex = *lastIndex;

					if (obj->Type == 0)
					{
						_moveablesIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						_moveablesIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						_moveablesIndices[newPoly.BaseIndex + 2] = baseVertices + 3;
						_moveablesIndices[newPoly.BaseIndex + 3] = baseVertices + 2;
						_moveablesIndices[newPoly.BaseIndex + 4] = baseVertices + 3;
						_moveablesIndices[newPoly.BaseIndex + 5] = baseVertices + 1;
					}
					else
					{
						_staticsIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						_staticsIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						_staticsIndices[newPoly.BaseIndex + 2] = baseVertices + 3;
						_staticsIndices[newPoly.BaseIndex + 3] = baseVertices + 2;
						_staticsIndices[newPoly.BaseIndex + 4] = baseVertices + 3;
						_staticsIndices[newPoly.BaseIndex + 5] = baseVertices + 1;
					}

					*lastIndex = *lastIndex + 6;
				}
				else
				{
					newPoly.BaseIndex = *lastIndex;

					if (obj->Type == 0)
					{
						_moveablesIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						_moveablesIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						_moveablesIndices[newPoly.BaseIndex + 2] = baseVertices + 2;
					}
					else
					{
						_staticsIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						_staticsIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						_staticsIndices[newPoly.BaseIndex + 2] = baseVertices + 2;
					}

					*lastIndex = *lastIndex + 3;
				}

				bucket.Polygons.push_back(newPoly);
			}

			mesh->Buckets.push_back(bucket);
		}

		return mesh;
	}
}
