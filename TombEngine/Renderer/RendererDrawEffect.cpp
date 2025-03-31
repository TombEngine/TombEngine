#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/Blood.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/debris.h"
#include "Game/effects/Drip.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/explosion.h"
#include "Game/effects/Footprint.h"
#include "Game/effects/Ripple.h"
#include "Game/effects/simple_particle.h"
#include "Game/effects/smoke.h"
#include "Game/effects/spark.h"
#include "Game/effects/Splash.h"
#include "Game/effects/Streamer.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/TR5/Trap/LaserBarrier.h"
#include "Objects/TR5/Trap/LaserBeam.h"
#include "Objects/Utils/object_helper.h"
#include "Renderer/Structures/RendererSprite2D.h"
#include "Renderer/Structures/RendererSprite.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"
#include "Structures/RendererSpriteBucket.h"
#include "Objects/Effects/Fireflies.h"

using namespace TEN::Effects::Blood;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Drip;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Footprint;
using namespace TEN::Effects::Ripple;
using namespace TEN::Effects::Splash;
using namespace TEN::Effects::Streamer;
using namespace TEN::Entities::Creatures::TR5;
using namespace TEN::Entities::Traps;
using namespace TEN::Math;
using namespace TEN::Effects::Fireflies;

extern BLOOD_STRUCT Blood[MAX_SPARKS_BLOOD];
extern FIRE_SPARKS FireSparks[MAX_SPARKS_FIRE];
extern SMOKE_SPARKS SmokeSparks[MAX_SPARKS_SMOKE];
extern SHOCKWAVE_STRUCT ShockWaves[MAX_SHOCKWAVE];
extern std::array<DebrisFragment, MAX_DEBRIS> DebrisFragments;

namespace TEN::Renderer 
{
	using namespace TEN::Renderer::Structures;

	constexpr auto ELECTRICITY_RANGE_MAX = BLOCK(24);
		
	void Renderer::PrepareLaserBarriers(RenderView& view)
	{
		for (const auto& [itemNumber, barrier] : LaserBarriers)
		{
			for (const auto& beam : barrier.Beams)
			{
				AddColoredQuad(
					beam.VertexPoints[0], beam.VertexPoints[1],
					beam.VertexPoints[2], beam.VertexPoints[3],
					barrier.Color, barrier.Color,
					barrier.Color, barrier.Color,
					BlendMode::Additive, view, SpriteRenderType::LaserBarrier);
			}
		}
	}

	void Renderer::PrepareSingleLaserBeam(RenderView& view)
	{
		for (const auto& [itemNumber, beam] : LaserBeams)
		{
			if (!beam.IsActive)
				continue;

			for (int i = 0; i < LaserBeamEffect::SUBDIVISION_COUNT; i++)
			{
				bool isLastSubdivision = (i == (LaserBeamEffect::SUBDIVISION_COUNT - 1));

				auto color = Color::Lerp(beam.OldColor, beam.Color, GetInterpolationFactor());

				AddColoredQuad(
					Vector3::Lerp(beam.OldVertices[i], beam.Vertices[i], GetInterpolationFactor()),
					Vector3::Lerp(
						beam.OldVertices[isLastSubdivision ? 0 : (i + 1)], 
						beam.Vertices[isLastSubdivision ? 0 : (i + 1)],
						GetInterpolationFactor()),
					Vector3::Lerp(
						beam.OldVertices[LaserBeamEffect::SUBDIVISION_COUNT + (isLastSubdivision ? 0 : (i + 1))],
						beam.Vertices[LaserBeamEffect::SUBDIVISION_COUNT + (isLastSubdivision ? 0 : (i + 1))],
						GetInterpolationFactor()),
					Vector3::Lerp(
						beam.OldVertices[LaserBeamEffect::SUBDIVISION_COUNT + i],
						beam.Vertices[LaserBeamEffect::SUBDIVISION_COUNT + i],
						GetInterpolationFactor()),
					color, color, color, color,
					BlendMode::Additive, view, SpriteRenderType::LaserBeam);
			}
		}
	}

	void Renderer::PrepareStreamers(RenderView& view)
	{
		for (const auto& [itemNumber, group] : StreamerEffect.GetGroups())
		{
			for (const auto& [tag, pool] : group.GetPools())
			{
				for (const auto& streamer : pool)
				{
					for (int i = 0; i < streamer.GetSegments().size(); i++)
					{
						const auto& segment = streamer.GetSegments()[i];
						const auto& prevSegment = streamer.GetSegments()[std::max(i - 1, 0)];

						if (segment.Life <= 0.0f)
							continue;

						auto vertex0 = Vector3::Lerp(segment.PrevVertices[0], segment.Vertices[0], GetInterpolationFactor());
						auto vertex1 = Vector3::Lerp(segment.PrevVertices[1], segment.Vertices[1], GetInterpolationFactor());
						auto prevVertex0 = Vector3::Lerp(prevSegment.PrevVertices[0], prevSegment.Vertices[0], GetInterpolationFactor());
						auto prevVertex1 = Vector3::Lerp(prevSegment.PrevVertices[1], prevSegment.Vertices[1], GetInterpolationFactor());

						auto color = Vector4::Lerp(segment.PrevColor, segment.Color, GetInterpolationFactor());
						auto prevColor = Vector4::Lerp(prevSegment.PrevColor, prevSegment.Color, GetInterpolationFactor());

						switch (streamer.GetFeatherMode())
						{
							default:
							case StreamerFeatherMode::None:
								AddColoredQuad(
									vertex0, vertex1,
									prevVertex1,
									prevVertex0,
									color,
									color,
									prevColor,
									prevColor,
									streamer.GetBlendMode(), view);
								break;

							case StreamerFeatherMode::Center:
							{
								auto center = (vertex0 + vertex1) / 2;
								auto prevCenter = (prevVertex0 + prevVertex1) / 2;

								AddColoredQuad(
									center, vertex1, prevVertex1, prevCenter,
									color, Vector4::Zero, Vector4::Zero, prevColor,
									streamer.GetBlendMode(), view);
								AddColoredQuad(
									vertex0, center, prevCenter, prevVertex0,
									Vector4::Zero, color, prevColor, Vector4::Zero,
									streamer.GetBlendMode(), view);
							}
								break;

							case StreamerFeatherMode::Left:
								AddColoredQuad(
									vertex0, vertex1, prevVertex1, prevVertex0,
									color, Vector4::Zero, Vector4::Zero, prevColor,
									streamer.GetBlendMode(), view);
								break;

							case StreamerFeatherMode::Right:
								AddColoredQuad(
									vertex0, vertex1, prevVertex1, prevVertex0,
									Vector4::Zero, color, prevColor, Vector4::Zero,
									streamer.GetBlendMode(), view);
								break;
						}
					}
				}
			}
		}
	}

	void Renderer::PrepareHelicalLasers(RenderView& view)
	{
		if (HelicalLasers.empty())
			return;

		if (!CheckIfSlotExists(ID_DEFAULT_SPRITES, "Helical lasers rendering"))
			return;

		for (const auto& laser : HelicalLasers)
		{
			if (laser.Life <= 0.0f)
				continue;

			auto color = Vector4::Lerp(laser.PrevColor, laser.Color, GetInterpolationFactor());
			color.w = Lerp(laser.PrevOpacity, laser.Opacity, GetInterpolationFactor());

			auto laserTarget = Vector3::Lerp(laser.PrevTarget, laser.Target, GetInterpolationFactor());

			ElectricityKnots[0] = laserTarget;
			ElectricityKnots[1] = Vector3::Lerp(laser.PrevOrigin, laser.Origin, GetInterpolationFactor());
			
			for (int j = 0; j < 2; j++)
				ElectricityKnots[j] -= laserTarget;

			CalculateHelixSpline(laser, ElectricityKnots, ElectricityBuffer);

			if (abs(ElectricityKnots[0].x) <= ELECTRICITY_RANGE_MAX &&
				abs(ElectricityKnots[0].y) <= ELECTRICITY_RANGE_MAX &&
				abs(ElectricityKnots[0].z) <= ELECTRICITY_RANGE_MAX)
			{
				int bufferIndex = 0;

				auto& interpPosArray = ElectricityBuffer;
				for (int s = 0; s < laser.NumSegments ; s++)
				{
					auto origin = laserTarget + interpPosArray[bufferIndex];
					bufferIndex++;
					auto target = laserTarget + interpPosArray[bufferIndex];

					auto center = (origin + target) / 2;
					auto dir = target - origin;
					dir.Normalize();

					AddSpriteBillboardConstrained(
						&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_LIGHTHING],
						center, color, PI_DIV_2, 1.0f, Vector2(5 * 8.0f, Vector3::Distance(origin, target)),
						BlendMode::Additive, dir, true, view);							
				}
			}
		}
	}

	void Renderer::PrepareElectricity(RenderView& view)
	{
		if (ElectricityArcs.empty())
			return;

		if (!CheckIfSlotExists(ID_DEFAULT_SPRITES, "Electricity rendering"))
			return;

		for (const auto& arc : ElectricityArcs)
		{
			if (arc.life <= 0)
				continue;

			ElectricityKnots[0] = Vector3::Lerp(arc.PrevPos1, arc.pos1, GetInterpolationFactor());
			ElectricityKnots[1] = Vector3::Lerp(arc.PrevPos1, arc.pos1, GetInterpolationFactor());
			ElectricityKnots[2] = Vector3::Lerp(arc.PrevPos2, arc.pos2, GetInterpolationFactor());
			ElectricityKnots[3] = Vector3::Lerp(arc.PrevPos3, arc.pos3, GetInterpolationFactor());
			ElectricityKnots[4] = Vector3::Lerp(arc.PrevPos4, arc.pos4, GetInterpolationFactor());
			ElectricityKnots[5] = Vector3::Lerp(arc.PrevPos4, arc.pos4, GetInterpolationFactor());

			for (int j = 0; j < ElectricityKnots.size(); j++)
				ElectricityKnots[j] -= LaraItem->Pose.Position.ToVector3();

			CalculateElectricitySpline(arc, ElectricityKnots, ElectricityBuffer);

			if (abs(ElectricityKnots[0].x) <= ELECTRICITY_RANGE_MAX &&
				abs(ElectricityKnots[0].y) <= ELECTRICITY_RANGE_MAX &&
				abs(ElectricityKnots[0].z) <= ELECTRICITY_RANGE_MAX)
			{
				int bufferIndex = 0;

				auto& interpPosArray = ElectricityBuffer;

				for (int s = 0; s < ((arc.segments * 3) - 1); s++)
				{
					auto origin = (LaraItem->Pose.Position + interpPosArray[bufferIndex]).ToVector3();
					bufferIndex++;
					auto target = (LaraItem->Pose.Position + interpPosArray[bufferIndex]).ToVector3();

					auto center = (origin + target) / 2;
					auto dir = target - origin;
					dir.Normalize();

					byte r, g, b;
					if (arc.life >= 16)
					{
						r = arc.r;
						g = arc.g;
						b = arc.b;
					}
					else
					{
						r = (arc.life * arc.r) / 16;
						g = (arc.life * arc.g) / 16;
						b = (arc.life * arc.b) / 16;
					}


					byte oldR, oldG, oldB;
					if (arc.PrevLife >= 16)
					{
						oldR = arc.PrevR;
						oldG = arc.PrevG;
						oldB = arc.PrevB;
					}
					else
					{
						oldR = (arc.PrevLife * arc.PrevR) / 16;
						oldG = (arc.PrevLife * arc.PrevG) / 16;
						oldB = (arc.PrevLife * arc.PrevB) / 16;
					}

					r = (byte)Lerp(oldR, r, GetInterpolationFactor());
					g = (byte)Lerp(oldG, g, GetInterpolationFactor());
					b = (byte)Lerp(oldB, b, GetInterpolationFactor());

					AddSpriteBillboardConstrained(
						&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_LIGHTHING],
						center,
						Vector4(1.0f, 1.0f, 1.0f, 0.5f),
						PI_DIV_2, 1.0f, Vector2(4, Vector3::Distance(origin, target)), BlendMode::Additive, dir, true, view);


					AddSpriteBillboardConstrained(
						&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_LIGHTHING],
						center,
						Vector4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f),
						PI_DIV_2, 1.0f, Vector2(arc.width * 8, Vector3::Distance(origin, target)), BlendMode::Additive, dir, true, view);					
				}
			}
		}
	}

	void Renderer::PrepareFireflies(RenderView& view)
	{
		if (!Objects[ID_FIREFLY_EMITTER].loaded)
			return;

		for (auto& firefly : FireflySwarm)
		{
			if (!firefly.on)
				continue;


			if (!CheckIfSlotExists(ID_SPARK_SPRITE, "Particle rendering"))
				continue;

			auto axis = Vector3(0,0,0);
			axis.Normalize();


			firefly.scalar = 3;
			firefly.size = 3;

			auto pos = Vector3::Lerp(
				Vector3(firefly.PrevX, firefly.PrevY, firefly.PrevZ),
				Vector3(firefly.Position.x, firefly.Position.y, firefly.Position.z),
				GetInterpolationFactor());

			pos = Vector3(firefly.Position.x, firefly.Position.y, firefly.Position.z);

			// Disallow sprites out of bounds.
			int spriteIndex = Objects[firefly.SpriteSeqID].meshIndex + firefly.SpriteID;
			spriteIndex = std::clamp(spriteIndex, 0, (int)_sprites.size());

			AddSpriteBillboard(
				&_sprites[spriteIndex],
				pos,
				Color(firefly.r / (float)UCHAR_MAX, firefly.g / (float)UCHAR_MAX, firefly.b / (float)UCHAR_MAX, 1.0f),
				TO_RAD(firefly.rotAng << 4), firefly.scalar,
				Vector2(firefly.size, firefly.size),
				firefly.blendMode, true, view);

		}
	}

	void Renderer::PrepareSmokes(RenderView& view) 
	{
		for (const auto& smoke : SmokeSparks) 
		{
			if (!smoke.on)
				continue;

			AddSpriteBillboard(
				&_sprites[smoke.def],
				Vector3::Lerp(
					Vector3(smoke.PrevPosition.x, smoke.PrevPosition.y, smoke.PrevPosition.z),
					Vector3(smoke.position.x, smoke.position.y, smoke.position.z),
					GetInterpolationFactor()),
				Vector4::Lerp(
					Vector4(smoke.PrevShade / 255.0f, smoke.PrevShade / 255.0f, smoke.PrevShade / 255.0f, 1.0f),
					Vector4(smoke.shade / 255.0f, smoke.shade / 255.0f, smoke.shade / 255.0f, 1.0f),
					GetInterpolationFactor()),
				TO_RAD(Lerp(smoke.PrevRotAng << 4, smoke.rotAng << 4, GetInterpolationFactor())),
				Lerp(smoke.PrevScalar, smoke.scalar, GetInterpolationFactor()),
				{
					Lerp(smoke.PrevSize, smoke.size, GetInterpolationFactor()) * 4.0f,
					Lerp(smoke.PrevSize, smoke.size, GetInterpolationFactor()) * 4.0f
				},
				BlendMode::Additive, true, view);
		}
	}

	void Renderer::PrepareFires(RenderView& view) 
	{
		for (const auto& fire : Fires)
		{
			auto oldFade = fire.PrevFade == 1 ? 1.0f : (float)(255 - fire.PrevFade) / 255.0f;
			auto fade = fire.fade == 1 ? 1.0f : (float)(255 - fire.fade) / 255.0f;
			fade = Lerp(oldFade, fade, GetInterpolationFactor());

			for (int i = 0; i < MAX_SPARKS_FIRE; i++) 
			{
				auto* spark = &FireSparks[i];
				if (spark->on)
				{
					AddSpriteBillboard(
						&_sprites[spark->def],
						Vector3::Lerp(
							Vector3(
								fire.PrevPosition.x + spark->PrevPosition.x * fire.PrevSize / 2,
								fire.PrevPosition.y + spark->PrevPosition.y * fire.PrevSize / 2,
								fire.PrevPosition.z + spark->PrevPosition.z * fire.PrevSize / 2),
							Vector3(
								fire.position.x + spark->position.x * fire.size / 2,
								fire.position.y + spark->position.y * fire.size / 2,
								fire.position.z + spark->position.z * fire.size / 2),
							GetInterpolationFactor()),
						Vector4::Lerp(
							Vector4(
								spark->PrevColor.x / 255.0f * fade,
								spark->PrevColor.y / 255.0f * fade,
								spark->PrevColor.z / 255.0f * fade,
								1.0f),
							Vector4(
								spark->color.x / 255.0f * fade,
								spark->color.y / 255.0f * fade,
								spark->color.z / 255.0f * fade,
								1.0f),
							GetInterpolationFactor()),
						TO_RAD(Lerp(spark->PrevRotAng << 4, spark->rotAng << 4, GetInterpolationFactor())),
						Lerp(spark->PrevScalar, spark->scalar, GetInterpolationFactor()),
						Vector2::Lerp(
							Vector2(fire.PrevSize * spark->PrevSize, fire.PrevSize * spark->PrevSize),
							Vector2(fire.size * spark->size, fire.size * spark->size),
							GetInterpolationFactor()),
						BlendMode::Additive, true, view);
				}
			}
		}
	}

	void Renderer::PrepareParticles(RenderView& view)
	{
		for (int i = 0; i < ParticleNodeOffsetIDs::NodeMax; i++)
			NodeOffsets[i].gotIt = false;

		for (auto& particle : Particles)
		{
			if (!particle.on)
				continue;

			if (particle.flags & SP_DEF)
			{
				auto pos = Vector3::Lerp(
					Vector3(particle.PrevX, particle.PrevY, particle.PrevZ),
					Vector3(particle.x, particle.y, particle.z),
					GetInterpolationFactor());

				if (particle.flags & SP_FX)
				{
					const auto& fx = EffectList[particle.fxObj];

					auto& newEffect = _effects[particle.fxObj];

					newEffect.Translation = Matrix::CreateTranslation(fx.pos.Position.ToVector3());
					newEffect.Rotation = fx.pos.Orientation.ToRotationMatrix();
					newEffect.Scale = Matrix::CreateScale(1.0f);
					newEffect.World = newEffect.Rotation * newEffect.Translation;
					newEffect.ObjectID = fx.objectNumber;
					newEffect.RoomNumber = fx.roomNumber;
					newEffect.Position = fx.pos.Position.ToVector3();
					
					newEffect.InterpolatedPosition = Vector3::Lerp(newEffect.PrevPosition, newEffect.Position, GetInterpolationFactor());
					newEffect.InterpolatedTranslation = Matrix::Lerp(newEffect.PrevTranslation, newEffect.Translation, GetInterpolationFactor());
					newEffect.InterpolatedRotation = Matrix::Lerp(newEffect.InterpolatedRotation, newEffect.Rotation, GetInterpolationFactor());
					newEffect.InterpolatedWorld = Matrix::Lerp(newEffect.PrevWorld, newEffect.World, GetInterpolationFactor());
					newEffect.InterpolatedScale = Matrix::Lerp(newEffect.PrevScale, newEffect.Scale, GetInterpolationFactor());

					pos += newEffect.InterpolatedPosition;

					if ((particle.sLife - particle.life) > Random::GenerateInt(8, 12))
					{
						// Particle becomes autonome.
						particle.flags &= ~SP_FX;

						particle.x = particle.PrevX = pos.x;
						particle.y = particle.PrevY = pos.y;
						particle.z = particle.PrevZ = pos.z;
					}
				}
				else if (!(particle.flags & SP_ITEM))
				{
					// NOTE: pos already set previously.
					//pos.x = particle.x;
					//pos.y = particle.y;
					//pos.z = particle.z;
				}
				else
				{
					auto* item = &g_Level.Items[particle.fxObj];

					auto nodePos = Vector3i::Zero;
					if (particle.flags & SP_NODEATTACH)
					{
						if (NodeOffsets[particle.nodeNumber].gotIt)
						{
							nodePos = NodeVectors[particle.nodeNumber];
						}
						else
						{
							nodePos.x = NodeOffsets[particle.nodeNumber].x;
							nodePos.y = NodeOffsets[particle.nodeNumber].y;
							nodePos.z = NodeOffsets[particle.nodeNumber].z;

							int meshIndex = NodeOffsets[particle.nodeNumber].meshNum;
							if (meshIndex >= 0)
							{
								nodePos = GetJointPosition(item, meshIndex, nodePos);
							}
							else
							{
								nodePos = GetJointPosition(LaraItem, -meshIndex, nodePos);
							}

							NodeOffsets[particle.nodeNumber].gotIt = true;
							NodeVectors[particle.nodeNumber] = nodePos;
						}

						pos += nodePos.ToVector3();

						if ((particle.sLife - particle.life) > Random::GenerateInt(4, 8))
						{
							// Particle becomes autonome.
							particle.flags &= ~SP_ITEM;

							particle.x = particle.PrevX = pos.x;
							particle.y = particle.PrevY = pos.y;
							particle.z = particle.PrevZ = pos.z;
						}
					}
					else
					{
						pos += _items[particle.fxObj].InterpolatedPosition; 
					}
				}

				// Disallow sprites out of bounds.
				int spriteIndex = Objects[particle.SpriteSeqID].meshIndex + particle.SpriteID;
				spriteIndex = std::clamp(spriteIndex, 0, (int)_sprites.size());

				if (particle.flags & SP_CONSTRAINED)
				{

					AddSpriteBillboardRotated(
						&_sprites[spriteIndex],
						pos,
						Color(particle.r / (float)UCHAR_MAX, particle.g / (float)UCHAR_MAX, particle.b / (float)UCHAR_MAX, 1.0f),
						TO_RAD(particle.rotAng << 4),
						particle.scalar,
						Vector2(particle.size, particle.size), particle.blendMode, particle.constraint, true, view);
				}
				else
				{
					AddSpriteBillboard(
						&_sprites[spriteIndex],
						pos,
						Color(particle.r / (float)UCHAR_MAX, particle.g / (float)UCHAR_MAX, particle.b / (float)UCHAR_MAX, 1.0f),
						TO_RAD(particle.rotAng << 4), particle.scalar,
						Vector2(particle.size, particle.size),
						particle.blendMode, true, view);
				}
				
			}
			else
			{
				if (!CheckIfSlotExists(ID_SPARK_SPRITE, "Particle rendering"))
					continue;

				auto pos = Vector3::Lerp(
					Vector3(particle.PrevX, particle.PrevY, particle.PrevZ),
					Vector3(particle.x, particle.y, particle.z),
					GetInterpolationFactor());

				auto axis = Vector3(particle.xVel, particle.yVel, particle.zVel);
				axis.Normalize();

				AddSpriteBillboardConstrained(
					&_sprites[Objects[ID_SPARK_SPRITE].meshIndex],
					pos,
					Vector4(particle.r / (float)UCHAR_MAX, particle.g / (float)UCHAR_MAX, particle.b / (float)UCHAR_MAX, 1.0f),
					TO_RAD(particle.rotAng << 4),
					particle.scalar,
					Vector2(4, particle.size), particle.blendMode, axis, true, view);
			}
		}
	}

	void Renderer::PrepareSplashes(RenderView& view) 
	{
		constexpr auto POINT_COUNT = 9;
		constexpr auto ALPHA	   = 360 / POINT_COUNT;

		for (const auto& splash : SplashEffects) 
		{
			if (!splash.isActive)
				continue;

			if (!CheckIfSlotExists(ID_DEFAULT_SPRITES, "Splashes rendering"))
				return;

			byte color = (splash.life >= 32 ? 128 : (byte)((splash.life / 32.0f) * 128));

			if (!splash.isRipple) 
			{
				if (splash.HeightSpeed < 0 && splash.height < 1024) 
				{
					float multiplier = splash.height / 1024.0f;
					color = (float)color * multiplier;
				}
			}

			byte prevColor = (splash.PrevLife >= 32 ? 128 : (byte)((splash.PrevLife / 32.0f) * 128));

			if (!splash.isRipple)
			{
				if (splash.PrevHeightSpeed < 0 && splash.PrevHeight < 1024)
				{
					float multiplier = splash.PrevHeight / 1024.0f;
					prevColor = (float)prevColor * multiplier;
				}
			}

			color = (byte)Lerp(prevColor, color, GetInterpolationFactor());

			float xInner;
			float zInner;
			float xOuter;
			float zOuter;
			float x2Inner;
			float z2Inner;
			float x2Outer;
			float z2Outer;
			float yInner = splash.Position.y;
			float yOuter = splash.Position.y - splash.height;

			float innerRadius = Lerp(splash.PrevInnerRadius, splash.InnerRadius, GetInterpolationFactor());
			float outerRadius = Lerp(splash.PrevOuterRadius, splash.OuterRadius, GetInterpolationFactor());

			for (int i = 0; i < POINT_COUNT; i++) 
			{
				xInner = innerRadius * sin(ALPHA * i * PI / 180);
				zInner = innerRadius * cos(ALPHA * i * PI / 180);
				xOuter = outerRadius * sin(ALPHA * i * PI / 180);
				zOuter = outerRadius * cos(ALPHA * i * PI / 180);
				xInner += splash.Position.x;
				zInner += splash.Position.z;
				xOuter += splash.Position.x;
				zOuter += splash.Position.z;
				int j = (i + 1) % POINT_COUNT;
				x2Inner = innerRadius * sin(ALPHA * j * PI / 180);
				x2Inner += splash.Position.x;
				z2Inner = innerRadius * cos(ALPHA * j * PI / 180);
				z2Inner += splash.Position.z;
				x2Outer = outerRadius * sin(ALPHA * j * PI / 180);
				x2Outer += splash.Position.x;
				z2Outer = outerRadius * cos(ALPHA * j * PI / 180);
				z2Outer += splash.Position.z;

				AddQuad(
					&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + splash.SpriteSeqStart + (int)splash.AnimPhase],
					Vector3(xOuter, yOuter, zOuter),
					Vector3(x2Outer, yOuter, z2Outer),
					Vector3(x2Inner, yInner, z2Inner),
					Vector3(xInner, yInner, zInner),
					Vector4(color / 255.0f, color / 255.0f, color / 255.0f, 1.0f),
					0, 1, Vector2::Zero,
					BlendMode::Additive, false, view);
			}
		}
	}

	void Renderer::PrepareBubbles(RenderView& view) 
	{
		if (Bubbles.empty())
			return;

		if (!CheckIfSlotExists(ID_DEFAULT_SPRITES, "Bubbles rendering"))
			return;

		for (const auto& bubble : Bubbles)
		{
			if (bubble.Life <= 0.0f)
				continue;

			AddSpriteBillboard(
				&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + bubble.SpriteIndex],
				Vector3::Lerp(bubble.PrevPosition, bubble.Position, GetInterpolationFactor()),
				Vector4::Lerp(bubble.PrevColor, bubble.Color, GetInterpolationFactor()),
				0.0f,
				1.0f,
				Vector2::Lerp(bubble.PrevSize, bubble.Size, GetInterpolationFactor()) / 2,
				BlendMode::Additive, true, view);
		}
	}

	void Renderer::PrepareDrips(RenderView& view)
	{
		if (Drips.empty())
			return;

		if (!CheckIfSlotExists(ID_DRIP_SPRITE, "Drips rendering"))
			return;

		for (const auto& drip : Drips)
		{
			if (drip.Life <= 0.0f)
				continue;

			auto axis = drip.Velocity;
			drip.Velocity.Normalize(axis);

			auto prevAxis = drip.PrevVelocity;
			drip.PrevVelocity.Normalize(prevAxis);

			AddSpriteBillboardConstrained(
				&_sprites[Objects[ID_DRIP_SPRITE].meshIndex],
				Vector3::Lerp(drip.PrevPosition, drip.Position, GetInterpolationFactor()),
				Vector4::Lerp(drip.PrevColor, drip.Color, GetInterpolationFactor()),
				0.0f, 1.0f, Vector2::Lerp(drip.PrevSize, drip.Size, GetInterpolationFactor()),
				BlendMode::Additive, -Vector3::Lerp(prevAxis, axis, GetInterpolationFactor()), false, view);
		}
	}

	void Renderer::PrepareRipples(RenderView& view) 
	{
		if (Ripples.empty())
			return;

		for (const auto& ripple : Ripples)
		{
			if (ripple.Life <= 0.0f)
				continue;

			float opacity = ripple.Color.w * ((ripple.Flags & (int)RippleFlags::LowOpacity) ? 0.5f : 1.0f);
			auto color = ripple.Color;
			color.w = opacity;

			float oldOpacity = ripple.PrevColor.w * ((ripple.Flags & (int)RippleFlags::LowOpacity) ? 0.5f : 1.0f);
			auto oldColor = ripple.PrevColor;
			oldColor.w = oldOpacity;

			AddSpriteBillboardConstrainedLookAt(
				&_sprites[ripple.SpriteIndex],
				Vector3::Lerp(ripple.PrevPosition, ripple.Position, GetInterpolationFactor()),
				Vector4::Lerp(oldColor, color, GetInterpolationFactor()),
				0.0f, 1.0f, Vector2(Lerp(ripple.PrevSize, ripple.Size, GetInterpolationFactor()) * 2),
				BlendMode::Additive, ripple.Normal, true, view);
		}
	}

	void Renderer::PrepareUnderwaterBloodParticles(RenderView& view)
	{
		if (UnderwaterBloodParticles.empty())
			return;

		for (const auto& uwBlood : UnderwaterBloodParticles)
		{
			if (uwBlood.Life <= 0.0f)
				continue;

			auto color = Vector4::Zero;
			if (uwBlood.Init)
			{
				color = Vector4(uwBlood.Init / 2, 0, uwBlood.Init / 16, UCHAR_MAX);
			}
			else
			{
				color = Vector4(uwBlood.Life / 2, 0, uwBlood.Life / 16, UCHAR_MAX);
			}

			color.x = (int)std::clamp((int)color.x, 0, UCHAR_MAX);
			color.y = (int)std::clamp((int)color.y, 0, UCHAR_MAX);
			color.z = (int)std::clamp((int)color.z, 0, UCHAR_MAX);
			color /= UCHAR_MAX;

			auto oldColor = Vector4::Zero;
			if (uwBlood.Init)
				oldColor = Vector4(uwBlood.Init / 2, 0, uwBlood.Init / 16, UCHAR_MAX);
			else
				oldColor = Vector4(uwBlood.PrevLife / 2, 0, uwBlood.PrevLife / 16, UCHAR_MAX);

			oldColor.x = (int)std::clamp((int)oldColor.x, 0, UCHAR_MAX);
			oldColor.y = (int)std::clamp((int)oldColor.y, 0, UCHAR_MAX);
			oldColor.z = (int)std::clamp((int)oldColor.z, 0, UCHAR_MAX);
			oldColor /= UCHAR_MAX;

			AddSpriteBillboard(
				&_sprites[uwBlood.SpriteIndex],
				Vector3::Lerp(uwBlood.PrevPosition, uwBlood.Position, GetInterpolationFactor()),
				Vector4::Lerp(oldColor, color, GetInterpolationFactor()),
				0.0f, 1.0f,
				Vector2(
					Lerp(uwBlood.PrevSize, uwBlood.Size, GetInterpolationFactor()),
					Lerp(uwBlood.PrevSize, uwBlood.Size, GetInterpolationFactor())) * 2,
				BlendMode::Additive, true, view);
		}
	}

	void Renderer::PrepareShockwaves(RenderView& view)
	{
		unsigned char r = 0;
		unsigned char g = 0;
		unsigned char b = 0;
		float c = 0;
		float s = 0;
		float angle = 0;

		for (int i = 0; i < MAX_SHOCKWAVE; i++)
		{
			auto* shockwave = &ShockWaves[i];

			if (!shockwave->life)
				continue;

			if (!CheckIfSlotExists(ID_DEFAULT_SPRITES, "Shockwaves rendering"))
				return;

			byte color = shockwave->life * 8;

			shockwave->yRot += shockwave->yRot / FPS;

			auto rotMatrix =
				Matrix::CreateRotationY(shockwave->yRot / 4) *
				Matrix::CreateRotationZ(shockwave->zRot) *
				Matrix::CreateRotationX(shockwave->xRot);

			auto pos = Vector3(shockwave->x, shockwave->y, shockwave->z);

			float innerRadius = Lerp(shockwave->PrevInnerRad, shockwave->innerRad, GetInterpolationFactor());
			float outerRadius = Lerp(shockwave->PrevOuterRad, shockwave->outerRad, GetInterpolationFactor());

			// Inner circle
			if (shockwave->style == (int)ShockwaveStyle::Normal)
			{
				angle = PI / 32.0f;
				c = cos(angle);
				s = sin(angle);
				angle -= PI / 8.0f;
			}
			else
			{
				angle = PI / 16.0f;
				c = cos(angle);
				s = sin(angle);
				angle -= PI / 4.0f;
			}

			float x1 = (innerRadius * c);
			float z1 = (innerRadius * s);
			float x4 = (innerRadius * c);
			float z4 = (innerRadius * s);

			auto p1 = Vector3(x1, 0, z1);
			auto p4 = Vector3(x4, 0, z4);

			p1 = Vector3::Transform(p1, rotMatrix);
			p4 = Vector3::Transform(p4, rotMatrix);

			if (shockwave->fadeIn == true)
			{
				if (shockwave->sr < shockwave->r)
				{
					shockwave->sr += shockwave->r / 18;
					r = shockwave->sr * shockwave->life / 255.0f;
				}
				else
				{
					r = shockwave->r * shockwave->life / 255.0f;
				}

				if (shockwave->sg < shockwave->g)
				{
					shockwave->sg += shockwave->g / 18;
					g = shockwave->sg * shockwave->life / 255.0f;
				}
				else
				{
					g = shockwave->g * shockwave->life / 255.0f;
				}

				if (shockwave->sb < shockwave->b)
				{
					shockwave->sb += shockwave->b / 18;
					b = shockwave->sb * shockwave->life / 255.0f;
				}
				else
				{
					b = shockwave->b * shockwave->life / 255.0f;
				}

				if (r == shockwave->r && g == shockwave->g && b == shockwave->b)
					shockwave->fadeIn = false;

			}
			else
			{
				r = shockwave->r * shockwave->life / 255.0f;
				g = shockwave->g * shockwave->life / 255.0f;
				b = shockwave->b * shockwave->life / 255.0f;
			}

			for (int j = 0; j <= 16; j++)
			{
				c = cos(angle);
				s = sin(angle);

				float x2 = (innerRadius * c);
				float z2 = (innerRadius * s);

				float x3 = (outerRadius * c);
				float z3 = (outerRadius * s);

				auto p2 = Vector3(x2, 0, z2);
				auto p3 = Vector3(x3, 0, z3);

				p2 = Vector3::Transform(p2, rotMatrix);
				p3 = Vector3::Transform(p3, rotMatrix);

				if (shockwave->style == (int)ShockwaveStyle::Invisible)
					return;

				if (shockwave->style == (int)ShockwaveStyle::Normal)
				{
					angle -= PI / 8.0f;

					AddQuad(&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_SPLASH],
						pos + p1,
						pos + p2,
						pos + p3,
						pos + p4,
						Vector4(
							r / 16.0f,
							g / 16.0f,
							b / 16.0f,
							1.0f),
						0, 1, { 0,0 }, BlendMode::Additive, false, view);
				}
				else if (shockwave->style == (int)ShockwaveStyle::Sophia)
				{
					angle -= PI / 4.0f;

					AddQuad(&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_SPLASH3],
						pos + p1,
						pos + p2,
						pos + p3,
						pos + p4,
						Vector4(
							r / 16.0f,
							g / 16.0f,
							b / 16.0f,
							1.0f),
						0, 1, Vector2::Zero, BlendMode::Additive, true, view);

				}
				else if (shockwave->style == (int)ShockwaveStyle::Knockback)
				{
					angle -= PI / 4.0f;

					AddQuad(&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_SPLASH3],
						pos + p4,
						pos + p3,
						pos + p2,
						pos + p1,
						Vector4(
							r / 16.0f,
							g / 16.0f,
							b / 16.0f,
							1.0f),
						0, 1, Vector2::Zero, BlendMode::Additive, true, view);
				}

				p1 = p2;
				p4 = p3;
			}
		}
	}

	void Renderer::PrepareBlood(RenderView& view) 
	{
		for (int i = 0; i < 32; i++) 
		{
			auto* blood = &Blood[i];

			if (blood->on)
			{
				if (!CheckIfSlotExists(ID_DEFAULT_SPRITES, "Blood rendering"))
					return;

				AddSpriteBillboard(
					&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_BLOOD],
					Vector3::Lerp(
						Vector3(blood->PrevPosition.x, blood->PrevPosition.y, blood->PrevPosition.z),
						Vector3(blood->x, blood->y, blood->z),
						GetInterpolationFactor()),
					Vector4::Lerp(
						Vector4(blood->PrevShade / 255.0f, blood->PrevShade * 0, blood->PrevShade * 0, 1.0f),
						Vector4(blood->shade / 255.0f, blood->shade * 0, blood->shade * 0, 1.0f),
						GetInterpolationFactor()),
					TO_RAD(Lerp(blood->PrevRotAng << 4, blood->rotAng << 4, GetInterpolationFactor())),
					1.0f,
					Vector2(
						Lerp(blood->PrevSize, blood->size, GetInterpolationFactor()) * 8.0f,
						Lerp(blood->PrevSize, blood->size, GetInterpolationFactor()) * 8.0f),
					BlendMode::Additive, true, view);
			}
		}
	}

	void Renderer::PrepareWeatherParticles(RenderView& view) 
	{
		constexpr auto RAIN_WIDTH = 4.0f;

		for (const auto& part : Weather.GetParticles())
		{
			if (!part.Enabled)
				continue;

			switch (part.Type)
			{
			case WeatherType::None:

				if (!CheckIfSlotExists(ID_DEFAULT_SPRITES, "Underwater dust rendering"))
					return;

				AddSpriteBillboard(
					&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_UNDERWATERDUST],
					Vector3::Lerp(part.PrevPosition, part.Position, GetInterpolationFactor()),
					Color(1.0f, 1.0f, 1.0f, part.Transparency()),
					0.0f, 1.0f, Vector2(Lerp(part.PrevSize, part.Size, GetInterpolationFactor())),
					BlendMode::Additive, true, view);

				break;

			case WeatherType::Snow:

				if (!CheckIfSlotExists(ID_DEFAULT_SPRITES, "Snow rendering"))
					return;

				AddSpriteBillboard(
					&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_UNDERWATERDUST],
					Vector3::Lerp(part.PrevPosition, part.Position, GetInterpolationFactor()),
					Color(1.0f, 1.0f, 1.0f, part.Transparency()),
					0.0f, 1.0f, Vector2(Lerp(part.PrevSize, part.Size, GetInterpolationFactor())),
					BlendMode::Additive, true, view);

				break;

			case WeatherType::Rain:

				if (!CheckIfSlotExists(ID_DRIP_SPRITE, "Rain rendering"))
					return;

				Vector3 v;
				part.Velocity.Normalize(v);

				AddSpriteBillboardConstrained(
					&_sprites[Objects[ID_DRIP_SPRITE].meshIndex], 
					Vector3::Lerp(part.PrevPosition, part.Position, GetInterpolationFactor()),
					Color(0.8f, 1.0f, 1.0f, part.Transparency()),
					0.0f, 1.0f,
					Vector2(RAIN_WIDTH, Lerp(part.PrevSize, part.Size, GetInterpolationFactor())),
					BlendMode::Additive, -v, true, view);

				break;
			}
		}
	}

	bool Renderer::DrawGunFlashes(RenderView& view)
	{
		if (!Lara.RightArm.GunFlash && !Lara.LeftArm.GunFlash)
			return false;

		if (Lara.Control.Look.OpticRange > 0 && _currentMirror == nullptr)
			return false;

		const auto& settings = g_GameFlow->GetSettings()->Weapons[(int)Lara.Control.Weapon.GunType - 1];
		if (!settings.MuzzleFlash)
			return false;

		_shaders.Bind(Shader::Statics);

		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		const auto& room = _rooms[LaraItem->RoomNumber];
		auto* itemPtr = &_items[LaraItem->Index];

		// Divide gunflash tint by 2 because tinting uses multiplication and additive color which doesn't look good with overbright color values.
		_stStatic.Color = settings.ColorizeMuzzleFlash ? ((Vector4)settings.FlashColor / 2) : Vector4::One;
		_stStatic.AmbientLight = room.AmbientLight;
		_stStatic.LightMode = (int)LightMode::Static;
		BindStaticLights(itemPtr->LightsToDraw);

		short length = 0;
		short zOffset = 0;
		short rotationX = 0;

		SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);
		SetBlendMode(BlendMode::Additive);

		if (Lara.Control.Weapon.GunType != LaraWeaponType::Flare &&
			Lara.Control.Weapon.GunType != LaraWeaponType::Crossbow)
		{
			switch (Lara.Control.Weapon.GunType)
			{
			case LaraWeaponType::Revolver:
				length = 192;
				zOffset = 68;
				rotationX = -14560;
				break;

			case LaraWeaponType::Uzi:
				length = 190;
				zOffset = 50;
				rotationX = -14560;
				break;

			case LaraWeaponType::HK:
			case LaraWeaponType::Shotgun:
				length = 300;
				zOffset = 92;
				rotationX = -14560;
				break;

			default:
			case LaraWeaponType::Pistol:
				length = 180;
				zOffset = 40;
				rotationX = -16830;
				break;
			}

			// Use MP5 flash if available.
			auto gunflash = GAME_OBJECT_ID::ID_GUN_FLASH;
			if (Lara.Control.Weapon.GunType == LaraWeaponType::HK && Objects[GAME_OBJECT_ID::ID_GUN_FLASH2].loaded)
			{
				gunflash = GAME_OBJECT_ID::ID_GUN_FLASH2;
				length += 20;
				zOffset += 10;
			}

			const auto& flashMoveable = *_moveableObjects[gunflash];
			const auto& flashMesh = *flashMoveable.ObjectMeshes[0];

			for (const auto& flashBucket : flashMesh.Buckets) 
			{
				if (flashBucket.BlendMode == BlendMode::Opaque)
					continue;

				if (flashBucket.Polygons.size() == 0)
					continue;

				BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[flashBucket.Texture]), SamplerStateRegister::AnisotropicClamp);

				auto tMatrix = Matrix::CreateTranslation(0, length, zOffset);
				auto rotMatrix = Matrix::CreateRotationX(TO_RAD(rotationX));

				auto worldMatrix = Matrix::Identity;
				if (Lara.LeftArm.GunFlash)
				{
					worldMatrix = itemPtr->AnimTransforms[LM_LHAND] * itemPtr->World;
					worldMatrix = tMatrix * worldMatrix;
					worldMatrix = rotMatrix * worldMatrix;
					ReflectMatrixOptionally(worldMatrix);

					_stStatic.World = worldMatrix;
					_cbStatic.UpdateData(_stStatic, _context.Get());

					DrawIndexedTriangles(flashBucket.NumIndices, flashBucket.StartIndex, 0);

					_numMoveablesDrawCalls++;
				}

				if (Lara.RightArm.GunFlash)
				{
					worldMatrix = itemPtr->AnimTransforms[LM_RHAND] * itemPtr->World;
					worldMatrix = tMatrix * worldMatrix;
					worldMatrix = rotMatrix * worldMatrix;
					ReflectMatrixOptionally(worldMatrix);

					_stStatic.World = worldMatrix;
					_cbStatic.UpdateData(_stStatic, _context.Get());

					DrawIndexedTriangles(flashBucket.NumIndices, flashBucket.StartIndex, 0);

					_numMoveablesDrawCalls++;
				}
			}
		}

		SetBlendMode(BlendMode::Opaque);
		return true;
	}

	void Renderer::DrawBaddyGunflashes(RenderView& view)
	{
		_shaders.Bind(Shader::Statics);

		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		for (auto* rRoomPtr : view.RoomsToDraw)
		{
			if (IgnoreReflectionPassForRoom(rRoomPtr->RoomNumber))
				continue;

			for (auto* rItemPtr : rRoomPtr->ItemsToDraw)
			{
				auto& nativeItem = g_Level.Items[rItemPtr->ItemNumber];

				if (!nativeItem.IsCreature())
					continue;

				auto& creature = *GetCreatureInfo(&nativeItem);
				const auto& rRoom = _rooms[nativeItem.RoomNumber];

				_stStatic.Color = Vector4::One;
				_stStatic.AmbientLight = rRoom.AmbientLight;
				_stStatic.LightMode = (int)LightMode::Static;

				BindStaticLights(rItemPtr->LightsToDraw); // FIXME: Is it really needed for gunflashes? -- Lwmte, 15.07.22
				SetBlendMode(BlendMode::Additive);
				SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

				if (creature.MuzzleFlash[0].Delay != 0 && creature.MuzzleFlash[0].Bite.BoneID != -1)
				{
					auto flashObjectID = creature.MuzzleFlash[0].SwitchToMuzzle2 ?
						_moveableObjects[ID_GUN_FLASH2].has_value() ? ID_GUN_FLASH2 : ID_GUN_FLASH :
						ID_GUN_FLASH;

					const auto& flashMoveable = *_moveableObjects[flashObjectID]->ObjectMeshes.at(0);
					
					for (const auto& flashBucket : flashMoveable.Buckets)
					{
						if (flashBucket.BlendMode == BlendMode::Opaque)
							continue;

						if (flashBucket.Polygons.size() == 0)
							continue;

						BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[flashBucket.Texture]), SamplerStateRegister::AnisotropicClamp);

						auto tMatrix = Matrix::CreateTranslation(creature.MuzzleFlash[0].Bite.Position);
						auto rotMatrixX = Matrix::CreateRotationX(TO_RAD(ANGLE(270.0f)));
						auto rotMatrixZ = Matrix::CreateRotationZ(TO_RAD(2 * GetRandomControl()));

						auto worldMatrix = rItemPtr->AnimTransforms[creature.MuzzleFlash[0].Bite.BoneID] * rItemPtr->World;
						worldMatrix = tMatrix * worldMatrix;

						if (creature.MuzzleFlash[0].ApplyXRotation)
							worldMatrix = rotMatrixX * worldMatrix;

						if (creature.MuzzleFlash[0].ApplyZRotation)
							worldMatrix = rotMatrixZ * worldMatrix;

						ReflectMatrixOptionally(worldMatrix);

						_stStatic.World = worldMatrix;
						_cbStatic.UpdateData(_stStatic, _context.Get());

						DrawIndexedTriangles(flashBucket.NumIndices, flashBucket.StartIndex, 0);

						_numMoveablesDrawCalls++;
					}
				}

				if (creature.MuzzleFlash[1].Delay != 0 && creature.MuzzleFlash[1].Bite.BoneID != -1)
				{
					auto flashObjectID = creature.MuzzleFlash[1].SwitchToMuzzle2 ?
						_moveableObjects[ID_GUN_FLASH2].has_value() ? ID_GUN_FLASH2 : ID_GUN_FLASH :
						ID_GUN_FLASH;

					const auto& flashMoveable = *_moveableObjects[flashObjectID]->ObjectMeshes.at(0);
					
					for (auto& flashBucket : flashMoveable.Buckets)
					{
						if (flashBucket.BlendMode == BlendMode::Opaque)
							continue;

						if (flashBucket.Polygons.size() == 0)
							continue;

						BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[flashBucket.Texture]), SamplerStateRegister::AnisotropicClamp);

						auto tMatrix = Matrix::CreateTranslation(creature.MuzzleFlash[1].Bite.Position);
						auto rotMatrixX = Matrix::CreateRotationX(TO_RAD(ANGLE(270.0f)));
						auto rotMatrixZ = Matrix::CreateRotationZ(TO_RAD(2 * GetRandomControl()));

						auto worldMatrix = rItemPtr->AnimTransforms[creature.MuzzleFlash[1].Bite.BoneID] * rItemPtr->World;
						worldMatrix = tMatrix * worldMatrix;

						if (creature.MuzzleFlash[1].ApplyXRotation)
							worldMatrix = rotMatrixX * worldMatrix;

						if (creature.MuzzleFlash[1].ApplyZRotation)
							worldMatrix = rotMatrixZ * worldMatrix;

						ReflectMatrixOptionally(worldMatrix);

						_stStatic.World = worldMatrix;
						_cbStatic.UpdateData(_stStatic, _context.Get());

						DrawIndexedTriangles(flashBucket.NumIndices, flashBucket.StartIndex, 0);

						_numMoveablesDrawCalls++;
					}
				}
			}
		}

		SetBlendMode(BlendMode::Opaque);
	}

	Texture2D Renderer::CreateDefaultNormalTexture() 
	{
		auto data = std::vector<byte>{ 128, 128, 255, 1 };
		return Texture2D(_device.Get(), 1, 1, data.data());
	}

	void Renderer::PrepareFootprints(RenderView& view) 
	{
		for (const auto& footprint : Footprints)
		{
			AddQuad(
				&_sprites[footprint.SpriteIndex],
				footprint.VertexPoints[0], footprint.VertexPoints[1], footprint.VertexPoints[2], footprint.VertexPoints[3],
				Vector4(footprint.Opacity), 0.0f, 1.0f, Vector2::One, BlendMode::Subtractive, false, view);
		}
	}

	Matrix Renderer::GetWorldMatrixForSprite(const RendererSpriteToDraw& sprite, RenderView& view)
	{
		auto spriteMatrix = Matrix::Identity;
		auto scaleMatrix = Matrix::CreateScale(sprite.Width * sprite.Scale, sprite.Height * sprite.Scale, sprite.Scale);

		auto spritePos = sprite.pos;

		if (sprite.Type == SpriteType::ThreeD)
		{
			ReflectMatrixOptionally(spriteMatrix);
		}
		else
		{
			ReflectVectorOptionally(spritePos);
		}

		switch (sprite.Type)
		{
		case SpriteType::Billboard:
		{
			auto cameraUp = Vector3(view.Camera.View._12, view.Camera.View._22, view.Camera.View._32);
			spriteMatrix = scaleMatrix * Matrix::CreateRotationZ(sprite.Rotation) * Matrix::CreateBillboard(spritePos, Camera.pos.ToVector3(), cameraUp);
		}
		break;

		case SpriteType::CustomBillboard:
		{
			auto rotMatrix = Matrix::CreateRotationY(sprite.Rotation);
			auto quadForward = Vector3(0.0f, 0.0f, 1.0f);
			spriteMatrix = scaleMatrix * Matrix::CreateConstrainedBillboard(
				spritePos,
				Camera.pos.ToVector3(),
				sprite.ConstrainAxis,
				nullptr,
				&quadForward);
		}
		break;

		case SpriteType::LookAtBillboard:
		{
			auto translationMatrix = Matrix::CreateTranslation(spritePos);
			auto rotMatrix = Matrix::CreateRotationZ(sprite.Rotation) * Matrix::CreateLookAt(Vector3::Zero, sprite.LookAtAxis, Vector3::UnitZ);
			spriteMatrix = scaleMatrix * rotMatrix * translationMatrix;
		}
		break;

		case SpriteType::RotatedBillboard:
		{
			auto translationMatrix = Matrix::CreateTranslation(spritePos);

			auto rotX = Matrix::CreateRotationX(sprite.LookAtAxis.x);
			auto rotY = Matrix::CreateRotationY(sprite.LookAtAxis.y);
			auto rotZ = Matrix::CreateRotationZ(sprite.LookAtAxis.z);

			auto rotMatrix = rotX * rotY * rotZ;

			spriteMatrix = scaleMatrix * rotMatrix * translationMatrix;
		}
		break;

		case SpriteType::ThreeD:
		default:
			break;
		}

		return spriteMatrix;
	}

	void Renderer::DrawEffect(RenderView& view, RendererEffect* effect, RendererPass rendererPass)
	{
		const auto& room = _rooms[effect->RoomNumber];

		auto world = effect->InterpolatedWorld;
		ReflectMatrixOptionally(world);

		_stStatic.World = world;
		_stStatic.Color = effect->Color;
		_stStatic.AmbientLight = effect->AmbientLight;
		_stStatic.LightMode = (int)LightMode::Dynamic;
		BindStaticLights(effect->LightsToDraw);
		_cbStatic.UpdateData(_stStatic, _context.Get());

		auto& mesh = *effect->Mesh;
		for (auto& bucket : mesh.Buckets) 
		{
			if (bucket.NumVertices == 0)
				continue;

			int passes = (rendererPass == RendererPass::Opaque && bucket.BlendMode == BlendMode::AlphaTest) ? 2 : 1;

			for (int p = 0; p < passes; p++)
			{
				if (!SetupBlendModeAndAlphaTest(bucket.BlendMode, rendererPass, p))
					continue;

				BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
				BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);

				DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0); 
				
				_numEffectsDrawCalls++;
			}
		}
	}

	void Renderer::DrawEffects(RenderView& view, RendererPass rendererPass)
	{
		_shaders.Bind(Shader::Statics);

		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		for (auto* roomPtr : view.RoomsToDraw)
		{
			if (IgnoreReflectionPassForRoom(roomPtr->RoomNumber))
				continue;

			for (auto* effectPtr : roomPtr->EffectsToDraw)
			{
				const auto& room = _rooms[effectPtr->RoomNumber];
				const auto& object = Objects[effectPtr->ObjectID];

				if (object.drawRoutine && object.loaded)
					DrawEffect(view, effectPtr, rendererPass);
			}
		}
	}

	void Renderer::DrawDebris(RenderView& view, RendererPass rendererPass)
	{
		bool activeDebrisExist = false;
		for (auto& deb : DebrisFragments)
		{
			if (deb.active)
			{
				if (IgnoreReflectionPassForRoom(deb.roomNumber))
					continue;

				activeDebrisExist = true;
				break;
			}
		}

		if (activeDebrisExist)
		{
			_shaders.Bind(Shader::Statics);

			SetCullMode(CullMode::None);

			_primitiveBatch->Begin();

			for (auto& deb : DebrisFragments)
			{
				if (deb.active)
				{
					if (IgnoreReflectionPassForRoom(deb.roomNumber))
						continue;

					if (!SetupBlendModeAndAlphaTest(deb.mesh.blendMode, rendererPass, 0))
						continue;

					if (deb.isStatic)
					{
						BindTexture(TextureRegister::ColorMap, &std::get<0>(_staticTextures[deb.mesh.tex]), SamplerStateRegister::LinearClamp);
					}
					else
					{
						BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[deb.mesh.tex]), SamplerStateRegister::LinearClamp);
					}

					_stStatic.World = Matrix::Identity;
					_stStatic.Color = deb.color;
					_stStatic.AmbientLight = _rooms[deb.roomNumber].AmbientLight;
					_stStatic.LightMode = (int)deb.lightMode;

					_cbStatic.UpdateData(_stStatic, _context.Get());

					auto matrix = Matrix::Lerp(deb.PrevTransform, deb.Transform, GetInterpolationFactor());
					ReflectMatrixOptionally(matrix);

					Vertex vtx0;
					vtx0.Position = Vector3::Transform(deb.mesh.Positions[0], matrix);
					vtx0.UV = deb.mesh.TextureCoordinates[0];
					vtx0.Normal = deb.mesh.Normals[0];
					vtx0.Color = deb.mesh.Colors[0];

					Vertex vtx1;
					vtx1.Position = Vector3::Transform(deb.mesh.Positions[1], matrix);
					vtx1.UV = deb.mesh.TextureCoordinates[1];
					vtx1.Normal = deb.mesh.Normals[1];
					vtx1.Color = deb.mesh.Colors[1];

					Vertex vtx2;
					vtx2.Position = Vector3::Transform(deb.mesh.Positions[2], matrix);
					vtx2.UV = deb.mesh.TextureCoordinates[2];
					vtx2.Normal = deb.mesh.Normals[2];
					vtx2.Color = deb.mesh.Colors[2];

					_primitiveBatch->DrawTriangle(vtx0, vtx1, vtx2);

					_numDebrisDrawCalls++;
					_numDrawCalls++;
					_numTriangles++;
				}
			}

			_primitiveBatch->End();

			// TODO: temporary fix, we need to remove every use of SpriteBatch and PrimitiveBatch because
			// they mess up render states cache.

			SetBlendMode(BlendMode::Opaque, true);
			SetDepthState(DepthState::Write, true);
		}
	}

	void Renderer::PrepareSmokeParticles(RenderView& view)
	{
		using TEN::Effects::Smoke::SmokeParticles;
		using TEN::Effects::Smoke::SmokeParticle;

		for (const auto& smoke : SmokeParticles) 
		{
			if (!smoke.active)
				continue;

			if (!CheckIfSlotExists(ID_SMOKE_SPRITES, "Smoke rendering"))
				return;

			AddSpriteBillboard(
				&_sprites[Objects[ID_SMOKE_SPRITES].meshIndex + smoke.sprite],
				Vector3::Lerp(smoke.PrevPosition, smoke.position, GetInterpolationFactor()),
				Vector4::Lerp(smoke.PrevColor, smoke.color, GetInterpolationFactor()),
				Lerp(smoke.PrevRotation, smoke.rotation, GetInterpolationFactor()),
				1.0f,
				Vector2(
					Lerp(smoke.PrevSize, smoke.size, GetInterpolationFactor()),
					Lerp(smoke.PrevSize, smoke.size, GetInterpolationFactor())),
				BlendMode::AlphaBlend, true, view);
		}
	}

	void Renderer::PrepareSparkParticles(RenderView& view)
	{
		using TEN::Effects::Spark::SparkParticle;
		using TEN::Effects::Spark::SparkParticles;

		extern std::array<SparkParticle, 128> SparkParticles;

		for (int i = 0; i < SparkParticles.size(); i++) 
		{
			auto& s = SparkParticles[i];
			if (!s.active) continue;

			if (!CheckIfSlotExists(ID_SPARK_SPRITE, "Spark particle rendering"))
				return;

			Vector3 prevVelocity;
			Vector3 velocity;
			s.PrevVelocity.Normalize(prevVelocity);
			s.velocity.Normalize(velocity);

			velocity = Vector3::Lerp(prevVelocity, velocity, GetInterpolationFactor());
			velocity.Normalize();

			float normalizedLife = s.age / s.life;
			auto height = Lerp(1.0f, 0.0f, normalizedLife);
			auto color = Vector4::Lerp(s.sourceColor, s.destinationColor, normalizedLife);

			AddSpriteBillboardConstrained(
				&_sprites[Objects[ID_SPARK_SPRITE].meshIndex],
				Vector3::Lerp(s.PrevPosition, s.pos, GetInterpolationFactor()),
				color, 
				0, 1,
				Vector2(
					s.width, 
					s.height * height),
				BlendMode::Additive, -velocity, false, view);
		}
	}

	void Renderer::PrepareExplosionParticles(RenderView& view)
	{
		using TEN::Effects::Explosion::explosionParticles;
		using TEN::Effects::Explosion::ExplosionParticle;

		for (int i = 0; i < explosionParticles.size(); i++) 
		{
			auto& exp = explosionParticles[i];
			if (!exp.active) continue;

			if (!CheckIfSlotExists(ID_EXPLOSION_SPRITES, "Explosion particles rendering"))
				return;

			AddSpriteBillboard(
				&_sprites[Objects[ID_EXPLOSION_SPRITES].meshIndex + exp.sprite], 
				Vector3::Lerp(exp.oldPos, exp.pos, GetInterpolationFactor()),
				Vector4::Lerp(exp.oldTint, exp.tint, GetInterpolationFactor()),
				Lerp(exp.oldRotation, exp.rotation, GetInterpolationFactor()),
				1.0f,
				Vector2(
					Lerp(exp.oldSize, exp.size, GetInterpolationFactor()),
					Lerp(exp.oldSize, exp.size, GetInterpolationFactor())),
				BlendMode::Additive, true, view);
		}
	}

	void Renderer::PrepareSimpleParticles(RenderView& view)
	{
		using namespace TEN::Effects;

		for (const auto& part : simpleParticles)
		{
			if (!part.active)
				continue;

			if (!CheckIfSlotExists(part.sequence, "Particle rendering"))
				continue;

			AddSpriteBillboard(
				&_sprites[Objects[part.sequence].meshIndex + part.sprite],
				Vector3::Lerp(part.PrevWorldPosition, part.worldPosition, GetInterpolationFactor()),
				Color(1.0f, 1.0f, 1.0f), 0, 1.0f,
				Vector2(
					Lerp(part.PrevSize, part.size, GetInterpolationFactor()),
					Lerp(part.PrevSize, part.size, GetInterpolationFactor()) / 2),
				BlendMode::AlphaBlend, true, view);
		}
	}
}
