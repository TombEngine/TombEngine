#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/effects.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Game/spotcam.h"
#include "Math/Math.h"
#include "Objects/Effects/LensFlare.h"
#include "Renderer/RenderView.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Environment;
using namespace TEN::Entities::Effects;
using namespace TEN::Math;
using namespace TEN::Utils;

namespace TEN::Renderer
{
	using TEN::Memory::LinearArrayBuffer;

	void Renderer::CollectRooms(RenderView& renderView, bool onlyRooms)
	{
		constexpr auto VIEW_PORT = Vector4(-1.0f, -1.0f, 1.0f, 1.0f);

		_visitedRoomsStack.clear();

		for (int i = 0; i < g_Level.Rooms.size(); i++)
		{
			auto& room = _rooms[i];

			room.ItemsToDraw.clear();
			room.EffectsToDraw.clear();
			room.StaticsToDraw.clear();
			room.LightsToDraw.clear();
			room.Visited = false;
			room.ViewPort = VIEW_PORT;

			for (auto& door : room.Doors)
			{
				door.Visited = false;
				door.InvisibleFromCamera = false;
				door.DotProduct = FLT_MAX;
			}
		}

		GetVisibleRooms(NO_VALUE, renderView.Camera.RoomNumber, VIEW_PORT, false, 0, onlyRooms, renderView);

		_invalidateCache = false;

		// Prepare real DX scissor test rectangle.
		for (auto* roomPtr : renderView.RoomsToDraw)
		{
			roomPtr->ClipBounds.Left = (roomPtr->ViewPort.x + 1.0f) * _screenWidth * 0.5f;
			roomPtr->ClipBounds.Bottom = (1.0f - roomPtr->ViewPort.y) * _screenHeight * 0.5f;
			roomPtr->ClipBounds.Right = (roomPtr->ViewPort.z + 1.0f) * _screenWidth * 0.5f;
			roomPtr->ClipBounds.Top = (1.0f - roomPtr->ViewPort.w) * _screenHeight * 0.5f;
		}

		// Collect fog bulbs.
		std::vector<RendererFogBulb> tempFogBulbs;
		tempFogBulbs.reserve(MAX_FOG_BULBS_DRAW);

		for (auto& room : _rooms)
		{
			if (!g_Level.Rooms[room.RoomNumber].Active())
				continue;

			for (const auto& light : room.Lights)
			{
				if (light.Type != LightType::FogBulb)
					continue;

				// Test bigger radius to avoid bad clipping.
				if (renderView.Camera.Frustum.SphereInFrustum(light.Position, light.Out * 1.2f))
				{
					RendererFogBulb bulb;

					bulb.Position = light.Position;
					bulb.Density = light.Intensity;
					bulb.Color = light.Color;
					bulb.Radius = light.Out;
					bulb.FogBulbToCameraVector = bulb.Position - renderView.Camera.WorldPosition;
					bulb.Distance = bulb.FogBulbToCameraVector.Length();

					tempFogBulbs.push_back(bulb);
				}
			}
		}

		std::sort(
			tempFogBulbs.begin(), tempFogBulbs.end(),
			[](const RendererFogBulb& bulb0, const RendererFogBulb& bulb1)
			{
				return bulb0.Distance < bulb1.Distance;
			});

		for (int i = 0; i < std::min(MAX_FOG_BULBS_DRAW, (int)tempFogBulbs.size()); i++)
			renderView.FogBulbsToDraw.push_back(tempFogBulbs[i]);

		// Collect lens flares.
		auto tempLensFlares = std::vector<RendererLensFlare>{};
		tempLensFlares.reserve(MAX_LENS_FLARES_DRAW);

		for (const auto& lensFlare : LensFlares)
		{
			auto lensFlareToCamera = lensFlare.Position - renderView.Camera.WorldPosition;
			
			float dist = 0.0f;
			if (!lensFlare.IsGlobal)
				dist = lensFlareToCamera.Length();
			lensFlareToCamera.Normalize();
			
			auto cameraDir = renderView.Camera.WorldDirection;
			cameraDir.Normalize();

			if (lensFlareToCamera.Dot(cameraDir) >= 0.0f)
			{
				auto lensFlareToDraw = RendererLensFlare{};
				lensFlareToDraw.Position = lensFlare.Position;
				lensFlareToDraw.Distance = dist;
				lensFlareToDraw.Color = lensFlare.Color;
				lensFlareToDraw.SpriteID = lensFlare.SpriteID;
				lensFlareToDraw.Direction = lensFlareToCamera;
				lensFlareToDraw.IsGlobal = lensFlare.IsGlobal;

				tempLensFlares.push_back(lensFlareToDraw);
			}
		}

		std::sort(
			tempLensFlares.begin(), tempLensFlares.end(),
			[](const RendererLensFlare& lensFlare0, const RendererLensFlare& lensFlare1)
			{
				if (lensFlare0.IsGlobal && !lensFlare1.IsGlobal)
					return true;

				if (!lensFlare0.IsGlobal && lensFlare1.IsGlobal)
					return false;

				return (lensFlare0.Distance < lensFlare1.Distance);
			});

		for (int i = 0; i < std::min(MAX_LENS_FLARES_DRAW, (int)tempLensFlares.size()); i++)
			renderView.LensFlaresToDraw.push_back(tempLensFlares[i]);
	}

	bool Renderer::CheckPortal(short parentRoomNumber, RendererDoor* door, Vector4 viewPort, Vector4* clipPort, RenderView& renderView)
	{
		_numCheckPortalCalls++;

		RendererRoom* room = &_rooms[parentRoomNumber];

		int  zClip = 0;
		Vector4 p[4];

		*clipPort = Vector4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (int i = 0; i < 4; i++)
		{
			if (!door->Visited)
			{
				p[i] = Vector4::Transform(door->AbsoluteVertices[i], renderView.Camera.ViewProjection);
				if (p[i].w > 0.0f)
				{
					p[i].x *= (1.0f / p[i].w);
					p[i].y *= (1.0f / p[i].w);

				}
				door->TransformedVertices[i] = p[i];
			}
			else
			{
				p[i] = door->TransformedVertices[i];
			}

			if (p[i].w > 0.0f)
			{
				clipPort->x = std::min(clipPort->x, p[i].x);
				clipPort->y = std::min(clipPort->y, p[i].y);
				clipPort->z = std::max(clipPort->z, p[i].x);
				clipPort->w = std::max(clipPort->w, p[i].y);
			}
			else
			{
				zClip++;
			}
		}

		door->Visited = true;

		if (zClip == 4)
			return false;

		if (zClip > 0)
		{
			for (int i = 0; i < 4; i++) 
			{
				auto a = p[i];
				auto b = p[(i + 1) % 4];

				if ((a.w > 0.0f) ^ (b.w > 0.0f))
				{

					if (a.x < 0.0f && b.x < 0.0f)
					{
						clipPort->x = -1.0f;
					}
					else
					{
						if (a.x > 0.0f && b.x > 0.0f)
						{
							clipPort->z = 1.0f;
						}
						else 
						{
							clipPort->x = -1.0f;
							clipPort->z = 1.0f;
						}
					}

					if (a.y < 0.0f && b.y < 0.0f)
					{
						clipPort->y = -1.0f;
					}
					else
					{
						if (a.y > 0.0f && b.y > 0.0f)
						{
							clipPort->w = 1.0f;
						}
						else 
						{
							clipPort->y = -1.0f;
							clipPort->w = 1.0f;
						}
					}
				}
			}
		}

		if (clipPort->x > viewPort.z ||
			clipPort->y > viewPort.w ||
			clipPort->z < viewPort.x || 
			clipPort->w < viewPort.y)
		{
			return false;
		}

		clipPort->x = std::max(clipPort->x, viewPort.x);
		clipPort->y = std::max(clipPort->y, viewPort.y);
		clipPort->z = std::min(clipPort->z, viewPort.z);
		clipPort->w = std::min(clipPort->w, viewPort.w);

		return true;
	}

	void Renderer::GetVisibleRooms(short from, short to, Vector4 viewPort, bool water, int count, bool onlyRooms, RenderView& renderView)
	{
		// FIXME: This is an urgent hack to fix stack overflow crashes.
		// See https://github.com/MontyTRC89/TombEngine/issues/947 for details.
		// NOTE by MontyTRC: I'd keep this as a failsafe solution for 0.00000001% of cases we could have problems

		int stackSize = (int)_visitedRoomsStack.size();
		int stackMinIndex = std::max(0, int(stackSize - 5));

		for (int i = stackSize - 1; i >= stackMinIndex; i--)
		{
			if (_visitedRoomsStack[i] == to)
			{
				TENLog("Circle detected in room " + std::to_string(to), LogLevel::Warning, LogConfig::Debug);
				return;
			}
		}
		
		static constexpr int MAX_SEARCH_DEPTH = 64;
		if (_rooms[to].Visited && count > MAX_SEARCH_DEPTH)
		{
			TENLog(
				"Maximum room collection depth of " + std::to_string(MAX_SEARCH_DEPTH) + " was reached with room " + std::to_string(to),
				LogLevel::Warning, LogConfig::Debug);
			return;
		}

		_visitedRoomsStack.push_back(to);

		_numGetVisibleRoomsCalls++;

		auto* room = &_rooms[to];

		if (!room->Visited)
		{
			room->Visited = true;

			renderView.RoomsToDraw.push_back(room);

			CollectLightsForRoom(to, renderView);

			if (!onlyRooms)
			{
				CollectItems(to, renderView);
				CollectStatics(to, renderView);
				CollectEffects(to);
			}
		}

		room->ViewPort.x = std::min(room->ViewPort.x, viewPort.x);
		room->ViewPort.y = std::min(room->ViewPort.y, viewPort.y);
		room->ViewPort.z = std::max(room->ViewPort.z, viewPort.z);
		room->ViewPort.w = std::max(room->ViewPort.w, viewPort.w);

		Vector4 clipPort;
		for (int i = 0; i < room->Doors.size(); i++)
		{
			auto* door = &room->Doors[i];

			if (door->InvisibleFromCamera)
				continue;

			if (!door->Visited)
			{
				door->CameraToDoor = Vector3(
					Camera.pos.x - (door->AbsoluteVertices[0].x),
					Camera.pos.y - (door->AbsoluteVertices[0].y),
					Camera.pos.z - (door->AbsoluteVertices[0].z));
				door->CameraToDoor.Normalize();
			}

			// IMPORTANT: dot = 0 would generate ambiguity becase door could be traversed in both directions, potentially 
			// generating endless loops. We need to exclude this.

			if (door->DotProduct == FLT_MAX)
			{
				door->DotProduct = 
					door->Normal.x * door->CameraToDoor.x +
					door->Normal.y * door->CameraToDoor.y +
					door->Normal.z * door->CameraToDoor.z;
				_numDotProducts++;
			}

			if (door->DotProduct < 0)
			{
				door->InvisibleFromCamera = true;
				continue;
			}

			if (from != door->RoomNumber && CheckPortal(to, door, viewPort, &clipPort, renderView))
				GetVisibleRooms(to, door->RoomNumber, clipPort, water, count + 1, onlyRooms, renderView);
		}

		_visitedRoomsStack.pop_back();
	}

	void Renderer::CollectMirrors(RenderView& renderView)
	{
		// Collect mirrors first because they are needed while collecting moveables.
		for (const auto& mirror : g_Level.Mirrors)
		{
			// TODO: Avoid LaraItem global.
			if (mirror.RoomNumber != Camera.pos.RoomNumber && mirror.RoomNumber != LaraItem->RoomNumber)
				continue;

			if (!mirror.Enabled)
				continue;

			auto& rendererMirror = renderView.Mirrors.emplace_back();
			rendererMirror.RoomNumber = mirror.RoomNumber;
			rendererMirror.ReflectionMatrix = mirror.ReflectionMatrix;
			rendererMirror.ReflectPlayer = mirror.ReflectPlayer;
			rendererMirror.ReflectMoveables = mirror.ReflectMoveables;
			rendererMirror.ReflectStatics = mirror.ReflectStatics;
			rendererMirror.ReflectSprites = mirror.ReflectSprites;
			rendererMirror.ReflectLights = mirror.ReflectLights;
		}
	}

	void Renderer::CollectItems(short roomNumber, RenderView& renderView)
	{
		if (_rooms.size() <= roomNumber)
			return;

		auto& rendererRoom = _rooms[roomNumber];
		const auto& room = g_Level.Rooms[rendererRoom.RoomNumber];

		bool isRoomReflected = IsRoomReflected(renderView, roomNumber);

		short itemNumber = NO_VALUE;
		for (itemNumber = room.itemNumber; itemNumber != NO_VALUE; itemNumber = g_Level.Items[itemNumber].NextItem)
		{
			const auto& item = g_Level.Items[itemNumber];

			if (item.ObjectNumber == ID_LARA && itemNumber == g_Level.Items[itemNumber].NextItem)
				break;

			if (item.Status == ITEM_INVISIBLE)
				continue;

			if (item.Model.Color.w < EPSILON)
				continue;

			if (item.ObjectNumber == ID_LARA && (SpotcamOverlay || SpotcamDontDrawLara))
				continue;

			if (item.ObjectNumber == ID_LARA && CurrentLevel == 0 && !g_GameFlow->IsLaraInTitleEnabled())
				continue;

			if (!_moveableObjects[item.ObjectNumber].has_value())
				continue;

			auto& obj = _moveableObjects[item.ObjectNumber].value();

			if (obj.DoNotDraw)
				continue;

			// Clip object by frustum only if it doesn't cast shadows and is not in mirror room,
			// otherwise disappearing shadows or reflections may be seen if object gets out of frustum.
			if (!isRoomReflected && obj.ShadowType == ShadowMode::None)
			{
				// Get all spheres and check if frustum intersects any of them.
				auto spheres = GetSpheres(itemNumber);

				bool inFrustum = false;
				for (int i = 0; !inFrustum, i < spheres.size(); i++)
				{
					// Blow up sphere radius by half for cases of too small calculated spheres.
					if (renderView.Camera.Frustum.SphereInFrustum(spheres[i].Center, spheres[i].Radius * 1.5f))
						inFrustum = true;
				}

				if (!inFrustum)
					continue;
			}

			auto& newItem = _items[itemNumber];

			newItem.ItemNumber = itemNumber;
			newItem.ObjectID = item.ObjectNumber;
			newItem.Color = item.Model.Color;
			newItem.Position = item.Pose.Position.ToVector3();
			newItem.Translation = Matrix::CreateTranslation(newItem.Position);
			newItem.Rotation = item.Pose.Orientation.ToRotationMatrix();
			newItem.Scale = Matrix::CreateScale(item.Pose.Scale);
			newItem.World = newItem.Scale * newItem.Rotation * newItem.Translation;

			// Disable interpolation either when renderer slot or item slot has flag. 
			// Renderer slot has no interpolation flag set in case it is fetched for first time (e.g. item first time in frustum).
			newItem.DisableInterpolation = item.DisableInterpolation || newItem.DisableInterpolation;

			// Disable interpolation when object has traveled significant distance.
			// Needed because when object goes out of frustum, previous position doesn't update.
			bool posChanged = Vector3::Distance(newItem.PrevPosition, newItem.Position) > BLOCK(1);

			if (newItem.DisableInterpolation || posChanged)
			{
				// NOTE: Interpolation always returns same result.
				newItem.PrevPosition = newItem.Position;
				newItem.PrevTranslation = newItem.Translation;
				newItem.PrevRotation = newItem.Rotation;
				newItem.PrevScale = newItem.Scale;
				newItem.PrevWorld = newItem.World;

				// Otherwise all frames until next ControlPhase will not be interpolated.
				newItem.DisableInterpolation = false;
				
				for (int j = 0; j < MAX_BONES; j++)
					newItem.PrevAnimTransforms[j] = newItem.AnimTransforms[j];
			}

			// Force interpolation only for player in player freeze mode.
			bool forceValue = g_GameFlow->CurrentFreezeMode == FreezeMode::Player && item.ObjectNumber == ID_LARA;
			float interpFactor = GetInterpolationFactor(forceValue);

			newItem.InterpolatedPosition = Vector3::Lerp(newItem.PrevPosition, newItem.Position, interpFactor);
			newItem.InterpolatedTranslation = Matrix::Lerp(newItem.PrevTranslation, newItem.Translation, interpFactor);
			newItem.InterpolatedRotation = Matrix::Lerp(newItem.InterpolatedRotation, newItem.Rotation, interpFactor);
			newItem.InterpolatedScale = Matrix::Lerp(newItem.InterpolatedScale, newItem.Scale, interpFactor);
			newItem.InterpolatedWorld = Matrix::Lerp(newItem.PrevWorld, newItem.World, interpFactor);
			
			for (int j = 0; j < MAX_BONES; j++)
				newItem.InterpolatedAnimTransforms[j] = Matrix::Lerp(newItem.PrevAnimTransforms[j], newItem.AnimTransforms[j], GetInterpolationFactor(forceValue));

			CalculateLightFades(&newItem);
			CollectLightsForItem(&newItem);

			rendererRoom.ItemsToDraw.push_back(&newItem);
		}
	}

	void Renderer::CollectStatics(short roomNumber, RenderView& renderView)
	{
		if (_rooms.size() <= roomNumber)
			return;

		auto& room = _rooms[roomNumber];
		auto* r = &g_Level.Rooms[room.RoomNumber];

		if (r->mesh.empty())
			return;

		bool isRoomReflected = IsRoomReflected(renderView, roomNumber);

		for (int i = 0; i < room.Statics.size(); i++)
		{
			auto* mesh = &room.Statics[i];
			auto* nativeMesh = &r->mesh[i];

			if (nativeMesh->Dirty || _invalidateCache)
			{
				mesh->ObjectNumber = nativeMesh->staticNumber;
				mesh->Color = nativeMesh->color;
				mesh->OriginalSphere = Statics[mesh->ObjectNumber].visibilityBox.ToLocalBoundingSphere();
				mesh->Pose = nativeMesh->pos;
				mesh->Scale = nativeMesh->scale;
				mesh->Update();

				nativeMesh->Dirty = false;
			}

			if (!(nativeMesh->flags & StaticMeshFlags::SM_VISIBLE))
				continue;

			if (nativeMesh->color.w < EPSILON)
				continue;

			if (!_staticObjects[Statics.GetIndex(mesh->ObjectNumber)].has_value())
				continue;

			auto& obj = GetStaticRendererObject(mesh->ObjectNumber);

			if (obj.ObjectMeshes.empty())
				continue;

			if (!isRoomReflected && !renderView.Camera.Frustum.SphereInFrustum(mesh->Sphere.Center, mesh->Sphere.Radius))
				continue;

			// Collect the lights
			std::vector<RendererLight*> lights;
			std::vector<RendererLightNode> cachedRoomLights;
			if (obj.ObjectMeshes.front()->LightMode != LightMode::Static)
			{
				if (mesh->CacheLights || _invalidateCache)
				{
					// Collect all lights and return also cached light for the next frames
					CollectLights(mesh->Pose.Position.ToVector3(),1024, room.RoomNumber, NO_VALUE, false, false, &cachedRoomLights, &lights);
					mesh->CacheLights = false;
					mesh->CachedRoomLights = cachedRoomLights;
				}
				else
				{
					// Collect only dynamic lights and use cached lights from rooms
					CollectLights(mesh->Pose.Position.ToVector3(), 1024, room.RoomNumber, NO_VALUE, false, true, &mesh->CachedRoomLights, &lights);
				}
			}
			mesh->LightsToDraw = lights;

			// At this point, we are sure that we must draw the static mesh
			room.StaticsToDraw.push_back(mesh);

			if (renderView.SortedStaticsToDraw.find(mesh->ObjectNumber) == renderView.SortedStaticsToDraw.end())
			{
				std::vector<RendererStatic*> vec;
				renderView.SortedStaticsToDraw.insert(std::pair<int, std::vector<RendererStatic*>>(mesh->ObjectNumber, std::vector<RendererStatic*>()));
			}
			renderView.SortedStaticsToDraw[mesh->ObjectNumber].push_back(mesh);
		}
	}

	void Renderer::CollectLights(const Vector3& pos, float radius, int roomNumber, int prevRoomNumber, bool prioritizeShadowLight, bool useCachedRoomLights, std::vector<RendererLightNode>* roomsLights, std::vector<RendererLight*>* outputLights)
	{
		if (_rooms.size() <= roomNumber)
			return;

		// Now collect lights from dynamic list and from rooms
		std::vector<RendererLightNode> tempLights;
		tempLights.reserve(MAX_LIGHTS_DRAW);

		auto& room = _rooms[roomNumber];

		RendererLight* brightestLight = nullptr;
		float brightest = 0.0f;

		// Dynamic lights have the priority
		for (auto& light : _dynamicLights[_dynamicLightList])
		{
			float distSqr = Vector3::DistanceSquared(pos, light.Position);

			// Collect only lights nearer than 20 sectors
			if (distSqr >= SQUARE(BLOCK(20)))
				continue;

			// Check the out radius
			if (distSqr > SQUARE(light.Out + radius))
				continue;

			float distance = sqrt(distSqr);
			float attenuation = 1.0f - distance / light.Out;
			float intensity = attenuation * light.Intensity * light.Luma;

			// If collecting shadows, try collecting shadow-casting light.
			if (prioritizeShadowLight && light.CastShadows && intensity >= brightest)
			{
				brightest = intensity;
				brightestLight = &light;
			}

			RendererLightNode node = { &light, intensity, distance, 1 };
			tempLights.push_back(node);
		}

		if (!useCachedRoomLights)
		{
			// Check current room and neighbor rooms.
			for (int roomToCheck : room.Neighbors)
			{
				auto& currentRoom = _rooms[roomToCheck];
				int lightCount = (int)currentRoom.Lights.size();

				for (int j = 0; j < lightCount; j++)
				{
					auto& light = currentRoom.Lights[j];

					float intensity = 0;
					float dist = 0;

					// Check only lights different from sun.
					if (light.Type == LightType::Sun)
					{
						// Suns from non-adjacent rooms not added.
						if (roomToCheck != roomNumber && (prevRoomNumber != roomToCheck || prevRoomNumber == NO_VALUE))
							continue;

						// Sun is added without distance checks.
						intensity = light.Intensity * Luma(light.Color);
					}
					else if (light.Type == LightType::Point || light.Type == LightType::Shadow)
					{
						float distSqr = Vector3::DistanceSquared(pos, light.Position);

						// Collect only lights nearer than 20 blocks.
						if (distSqr >= SQUARE(BLOCK(20)))
							continue;

						// Check out radius.
						if (distSqr > SQUARE(light.Out + radius))
							continue;

						dist = sqrt(distSqr);
						float attenuation = 1.0f - dist / light.Out;
						intensity = attenuation * light.Intensity * Luma(light.Color);

						// If collecting shadows, try collecting shadow-casting light.
						if (prioritizeShadowLight && light.CastShadows && light.Type == LightType::Point && intensity >= brightest)
						{
							brightest = intensity;
							brightestLight = &light;
						}
					}
					else if (light.Type == LightType::Spot)
					{
						float distSqr = Vector3::DistanceSquared(pos, light.Position);

						// Collect only lights nearer than 20 blocks.
						if (distSqr >= SQUARE(BLOCK(20)))
							continue;

						// Check range.
						if (distSqr > SQUARE(light.Out + radius))
							continue;

						dist = sqrt(distSqr);
						float attenuation = 1.0f - dist / light.Out;
						intensity = attenuation * light.Intensity * light.Luma;

						// If collecting shadows, try collecting shadow-casting light.
						if (prioritizeShadowLight && light.CastShadows && intensity >= brightest)
						{
							brightest = intensity;
							brightestLight = &light;
						}
					}
					else
					{
						// Invalid light type.
						continue;
					}

					RendererLightNode node = { &light, intensity, dist, 0 };

					if (roomsLights != nullptr)
						roomsLights->push_back(node);

					tempLights.push_back(node);
				}
			}
		}
		else
		{
			for (auto& node : *roomsLights)
				tempLights.push_back(node);
		}

		// Sort lights.
		if (tempLights.size() > MAX_LIGHTS_PER_ITEM)
		{
			std::sort(tempLights.begin(), tempLights.end(), [](const RendererLightNode& a, const RendererLightNode& b)
			{
				return (a.Dynamic == b.Dynamic) ? (a.LocalIntensity > b.LocalIntensity) : (a.Dynamic > b.Dynamic);
			});
		}

		// Put actual lights in provided vector.
		outputLights->clear();

		// Add brightest ligh, if collecting shadow light is specified, even if it's far in range.
		if (prioritizeShadowLight && brightestLight)
			outputLights->push_back(brightestLight);

		// Add max 8 lights per item, including shadow light for player eventually.
		for (auto& l : tempLights)
		{
			if (prioritizeShadowLight && brightestLight == l.Light)
				continue;

			outputLights->push_back(l.Light);

			if (outputLights->size() == MAX_LIGHTS_PER_ITEM)
				break;
		}
	}

	void Renderer::CollectLightsForCamera()
	{
		std::vector<RendererLight*> lightsToDraw;
		CollectLights(Vector3(Camera.pos.x, Camera.pos.y, Camera.pos.z), CAMERA_LIGHT_COLLECTION_RADIUS, Camera.pos.RoomNumber, NO_VALUE, true, false, nullptr, &lightsToDraw);

		if (g_Configuration.ShadowType != ShadowMode::None && !lightsToDraw.empty() && lightsToDraw.front()->CastShadows)
		{
			_shadowLight = lightsToDraw.front();
		}
		else
		{
			_shadowLight = nullptr;
		}
	}	
	
	void Renderer::CollectLightsForEffect(short roomNumber, RendererEffect* effect)
	{
		CollectLights(effect->Position, ITEM_LIGHT_COLLECTION_RADIUS, roomNumber, NO_VALUE, false, false, nullptr, &effect->LightsToDraw);
	}

	void Renderer::CollectLightsForItem(RendererItem* item)
	{
		CollectLights(item->Position, ITEM_LIGHT_COLLECTION_RADIUS, item->RoomNumber, item->PrevRoomNumber, false, false, nullptr, &item->LightsToDraw);
	}

	void Renderer::CalculateLightFades(RendererItem *item)
	{
		ItemInfo* nativeItem = &g_Level.Items[item->ItemNumber];

		// Interpolate ambient light between rooms
		if (item->PrevRoomNumber == NO_VALUE)
		{
			item->PrevRoomNumber = nativeItem->RoomNumber;
			item->RoomNumber = nativeItem->RoomNumber;
			item->LightFade = 1.0f;
		}
		else if (nativeItem->RoomNumber != item->RoomNumber)
		{
			item->PrevRoomNumber = item->RoomNumber;
			item->RoomNumber = nativeItem->RoomNumber;
			item->LightFade = 0.0f;
		}
		else if (item->LightFade < 1.0f)
		{
			item->LightFade += AMBIENT_LIGHT_INTERPOLATION_STEP;
			item->LightFade = std::clamp(item->LightFade, 0.0f, 1.0f);
		}

		if (item->PrevRoomNumber == NO_VALUE || item->LightFade == 1.0f)
			item->AmbientLight = _rooms[nativeItem->RoomNumber].AmbientLight;
		else
		{
			auto prev = _rooms[item->PrevRoomNumber].AmbientLight;
			auto next = _rooms[item->RoomNumber].AmbientLight;

			item->AmbientLight.x = Lerp(prev.x, next.x, item->LightFade);
			item->AmbientLight.y = Lerp(prev.y, next.y, item->LightFade);
			item->AmbientLight.z = Lerp(prev.z, next.z, item->LightFade);
		}

		// Multiply calculated ambient light by object tint
		item->AmbientLight *= nativeItem->Model.Color;
	}

	void Renderer::CollectLightsForRoom(short roomNumber, RenderView &renderView)
	{
		if (_rooms.size() <= roomNumber)
			return;

		RendererRoom& room = _rooms[roomNumber];
		RoomData* r = &g_Level.Rooms[roomNumber];
		
		// Collect dynamic lights for rooms
		for (int i = 0; i < _dynamicLights[_dynamicLightList].size(); i++)
		{
			RendererLight* light = &_dynamicLights[_dynamicLightList][i];

			// If no radius, ignore
			if (light->Out == 0.0f)
			{
				continue;
			}

			// Light buffer is full
			if (renderView.LightsToDraw.size() >= NUM_LIGHTS_PER_BUFFER)
			{
				break;
			}

			// Light already on a list
			if (std::find(renderView.LightsToDraw.begin(), renderView.LightsToDraw.end(), light) != renderView.LightsToDraw.end())
			{
				continue;
			}

			// Light is not within room bounds
			if (!room.BoundingBox.Intersects(light->BoundingSphere))
			{
				continue;
			}

			renderView.LightsToDraw.push_back(light);
			room.LightsToDraw.push_back(light);
		}
	}

	void Renderer::CollectEffects(short roomNumber)
	{
		if (_rooms.size() <= roomNumber)
			return;

		RendererRoom& room = _rooms[roomNumber];
		RoomData* r = &g_Level.Rooms[room.RoomNumber];

		short fxNum = NO_VALUE;
		for (fxNum = r->fxNumber; fxNum != NO_VALUE; fxNum = EffectList[fxNum].nextFx)
		{
			FX_INFO *fx = &EffectList[fxNum];
			if (fx->objectNumber < 0 || fx->color.w <= 0)
				continue;

			ObjectInfo *obj = &Objects[fx->objectNumber];

			RendererEffect *newEffect = &_effects[fxNum];

			newEffect->Translation = Matrix::CreateTranslation(fx->pos.Position.x, fx->pos.Position.y, fx->pos.Position.z);
			newEffect->Rotation = fx->pos.Orientation.ToRotationMatrix();
			newEffect->Scale = Matrix::CreateScale(1.0f);
			newEffect->World = newEffect->Rotation * newEffect->Translation;
			newEffect->ObjectID = fx->objectNumber;
			newEffect->RoomNumber = fx->roomNumber;
			newEffect->Position = fx->pos.Position.ToVector3();
			newEffect->AmbientLight = room.AmbientLight;
			newEffect->Color = fx->color;
			newEffect->Mesh = GetMesh(obj->nmeshes ? obj->meshIndex : fx->frameNumber);

			if (fx->DisableInterpolation)
			{
				// In this way the interpolation will return always the same result
				newEffect->PrevPosition = newEffect->Position;
				newEffect->PrevTranslation = newEffect->Translation;
				newEffect->PrevRotation = newEffect->Rotation;
				newEffect->PrevWorld = newEffect->World;
				newEffect->PrevScale = newEffect->Scale;
			}

			newEffect->InterpolatedPosition = Vector3::Lerp(newEffect->PrevPosition, newEffect->Position, GetInterpolationFactor());
			newEffect->InterpolatedTranslation = Matrix::Lerp(newEffect->PrevTranslation, newEffect->Translation, GetInterpolationFactor());
			newEffect->InterpolatedRotation = Matrix::Lerp(newEffect->InterpolatedRotation, newEffect->Rotation, GetInterpolationFactor());
			newEffect->InterpolatedWorld = Matrix::Lerp(newEffect->PrevWorld, newEffect->World, GetInterpolationFactor());
			newEffect->InterpolatedScale = Matrix::Lerp(newEffect->PrevScale, newEffect->Scale, GetInterpolationFactor());

			CollectLightsForEffect(fx->roomNumber, newEffect);

			room.EffectsToDraw.push_back(newEffect);
		}
	}

	void Renderer::ResetItems()
	{
		for (auto& item : _items)
			item.DoneAnimations = false;
	}

	void Renderer::SaveOldState()
	{
		for (auto& item : _items)
		{
			item.PrevPosition = item.Position;
			item.PrevWorld = item.World;
			item.PrevTranslation = item.Translation;
			item.PrevRotation = item.Rotation;
			item.PrevScale = item.Scale;

			for (int j = 0; j < MAX_BONES; j++)
				item.PrevAnimTransforms[j] = item.AnimTransforms[j];
		}

		for (auto& effect : _effects)
		{
			effect.PrevPosition = effect.Position;
			effect.PrevWorld = effect.World;
			effect.PrevTranslation = effect.Translation;
			effect.PrevRotation = effect.Rotation;
			effect.PrevScale = effect.Scale;
		}
	}
}
