#include "framework.h"
#include "Game/effects/Streamer.h"

#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Math/Math.h"
#include "Renderer/RendererEnums.h"
#include "Specific/clock.h"

using namespace TEN::Math;

namespace TEN::Effects::Streamer
{
	StreamerEffectController StreamerEffect = {};

	void Streamer::StreamerSegment::InitializeVertices(const Vector3& pos, float width)
	{
		Vertices = { pos, pos };
		TransformVertices(0.0f, width / 2);
	}

	void Streamer::StreamerSegment::Update()
	{
		StoreInterpolationData();

		// Update color.
		float alpha = (float)Life / (float)LifeMax;
		Color.x = EaseInOutSine(ColorEnd.x, ColorStart.x, alpha);
		Color.y = EaseInOutSine(ColorEnd.y, ColorStart.y, alpha);
		Color.z = EaseInOutSine(ColorEnd.z, ColorStart.z, alpha);
		Color.w = EaseInOutSine(ColorEnd.w, ColorStart.w, alpha);

		// TODO: Not working. Make it work. -- Sezz 2025.03.02
		// Update orientation.
		Orientation.SetAngle(Orientation.GetAngle() + Rotation);
		
		// Update vertices.
		TransformVertices(Velocity, ExpRate);

		// Update life.
		Life--;
	}

	void Streamer::StreamerSegment::TransformVertices(float vel, float expRate)
	{
		// Apply expansion/contraction.
		if (expRate != 0.0f)
		{
			float distSqr = Vector3::DistanceSquared(Vertices[0], Vertices[1]);
			if (expRate < 0.0f && distSqr <= SQUARE(abs(expRate)))
			{
				auto center = (Vertices[0] + Vertices[1]) / 2;
				Vertices[0] =
				Vertices[1] = center;
			}
			else
			{

				auto dir = Orientation.ToDirection();
				Vertices[0] = Geometry::TranslatePoint(Vertices[0], -dir, expRate);
				Vertices[1] = Geometry::TranslatePoint(Vertices[1], dir, expRate);
			}
		}

		// Apply directional velocity.
		if (vel != 0.0f)
		{
			auto dir = Orientation.GetAxis();
			Vertices[0] = Geometry::TranslatePoint(Vertices[0], dir, vel);
			Vertices[1] = Geometry::TranslatePoint(Vertices[1], dir, vel);
		}
	}

	Streamer::Streamer(StreamerFeatherMode featherMode, BlendMode blendMode)
	{
		_segmentSpawnTimeOffset = GlobalCounter % SEGMENT_SPAWN_INTERVAL_TIME;
		_featherMode = featherMode;
		_blendMode = blendMode;
	}

	const std::vector<Streamer::StreamerSegment>& Streamer::GetSegments() const
	{
		return _segments;
	}

	StreamerFeatherMode Streamer::GetFeatherMode() const
	{
		return _featherMode;
	}

	BlendMode Streamer::GetBlendMode() const
	{
		return _blendMode;
	}

	bool Streamer::IsBroken() const
	{
		return _isBroken;
	}

	void Streamer::Extend(const Vector3& pos, const Vector3& dir, short orient, const Color& colorStart, const Color& colorEnd,
						  float width, float life, float vel, float expRate, short rot, unsigned int segmentCount)
	{
		constexpr auto FADE_IN_COEFF = 3.0f;

		auto& segment = GetSegment();

		// Avoid creating "clipped" streamers by clamping max life according to max segment count.
		int lifeMax = (int)std::min(round(life * FPS), (float)SEGMENT_COUNT_MAX);

		float alpha = (float(segmentCount * SEGMENT_SPAWN_INTERVAL_TIME) / (float)lifeMax) * FADE_IN_COEFF;
		float opacityMax = EaseInOutSine(colorEnd.w, colorStart.w, alpha);

		segment.Orientation = AxisAngle(dir, orient);
		segment.Color =
		segment.ColorStart = Vector4(colorStart.x, colorStart.y, colorStart.z, opacityMax);
		segment.ColorEnd = colorEnd;
		segment.Life =
		segment.LifeMax = lifeMax;
		segment.Velocity = vel;
		segment.ExpRate = expRate;
		segment.Rotation = rot;
		segment.InitializeVertices(pos, width);
	}

	void Streamer::Update()
	{
		if (_segments.empty())
			return;

		// Set flag to track if streamer was broken.
		const auto& newestSegment = _segments.back();
		if (newestSegment.Life != newestSegment.LifeMax)
			_isBroken = true;

		// Update segments.
		for (auto& segment : _segments)
			segment.Update();

		ClearInactiveEffects(_segments);
	}

	Streamer::StreamerSegment& Streamer::GetSegment()
	{
		TENAssert(_segments.size() <= SEGMENT_COUNT_MAX, "Streamer segment count overflow.");

		// Return newest segment.
		if (!_segments.empty() && TestGlobalTimeInterval(SEGMENT_SPAWN_INTERVAL_TIME, _segmentSpawnTimeOffset))
			return _segments.back();

		// Clear oldest segment if vector is full.
		if (_segments.size() == SEGMENT_COUNT_MAX)
			_segments.erase(_segments.begin());

		// Add and return new segment.
		return _segments.emplace_back();
	}

	const std::unordered_map<int, std::vector<Streamer>>& StreamerGroup::GetPools() const
	{
		return _pools;
	}

	void StreamerGroup::AddStreamer(int tag, const Vector3& pos, const Vector3& dir, short orient, const Color& colorStart, const Color& colorEnd,
									float width, float life, float vel, float expRate, short rot,
									StreamerFeatherMode featherMode, BlendMode blendMode)
	{
		TENAssert(_pools.size() <= POOL_COUNT_MAX, "Streamer pool count overflow.");

		// Return early if pool map is full and element with tag key doesn't already exist.
		if (_pools.size() == POOL_COUNT_MAX && !_pools.count(tag))
			return;

		// Get and extend streamer iteration.
		auto& streamer = GetStreamerIteration(tag, featherMode, blendMode);
		streamer.Extend(pos, dir, orient, colorStart, colorEnd, width, life, vel, expRate, rot, (unsigned int)streamer.GetSegments().size());
	}

	void StreamerGroup::Update()
	{
		if (_pools.empty())
			return;

		for (auto& [tag, pool] : _pools)
		{
			for (auto& streamer : pool)
				streamer.Update();

			ClearInactiveStreamers(tag);
		}

		ClearInactivePools();
	}

	std::vector<Streamer>& StreamerGroup::GetPool(int tag)
	{
		// Get pool at tag key.
		_pools.insert({ tag, {} });
		auto& pool = _pools.at(tag);
		return pool;
	}

	Streamer& StreamerGroup::GetStreamerIteration(int tag, StreamerFeatherMode featherMode, BlendMode blendMode)
	{
		auto& pool = GetPool(tag);
		TENAssert(pool.size() <= STREAMER_COUNT_MAX, "Streamer pool size overflow.");

		// Return most recent streamer iteration if it exists and is unbroken.
		if (!pool.empty())
		{
			auto& streamer = pool.back();
			if (!streamer.IsBroken())
				return streamer;
		}

		// Clear oldest streamer if pool is full.
		if (pool.size() == STREAMER_COUNT_MAX)
			pool.erase(pool.begin());

		// Add and return new streamer iteration.
		return pool.emplace_back(Streamer(featherMode, blendMode));
	}

	void StreamerGroup::ClearInactivePools()
	{
		for (auto it = _pools.begin(); it != _pools.end();)
		{
			const auto& pool = it->second;
			if (pool.empty())
			{
				it = _pools.erase(it);
				continue;
			}
			
			++it;
		}
	}

	void StreamerGroup::ClearInactiveStreamers(int tag)
	{
		auto& pool = _pools.at(tag);

		pool.erase(
			std::remove_if(
				pool.begin(), pool.end(),
				[](const auto& streamer)
				{
					return streamer.GetSegments().empty();
				}),
			pool.end());
	}

	const std::unordered_map<int, StreamerGroup>& StreamerEffectController::GetGroups() const
	{
		return _groups;
	}

	void StreamerEffectController::Spawn(int itemNumber, int tag, const Vector3& pos, const Vector3& dir, short orient, const Color& colorStart, const Color& colorEnd,
										 float width, float life, float vel, float expRate, short rot,
										 StreamerFeatherMode featherMode, BlendMode blendMode)
	{
		TENAssert(_groups.size() <= GROUP_COUNT_MAX, "Streamer group count overflow.");

		// Return early if group map is full and element with itemNumber key doesn't already exist.
		if (_groups.size() == GROUP_COUNT_MAX && !_groups.count(itemNumber))
			return;

		// Add new or extend existing streamer.
		auto& group = GetGroup(itemNumber);
		group.AddStreamer(tag, pos, dir, orient, colorStart, colorEnd, width, life, vel, expRate, rot, featherMode, blendMode);
	}

	void StreamerEffectController::Update()
	{
		if (_groups.empty())
			return;

		for (auto& [itemNumber, group] : _groups)
			group.Update();

		ClearInactiveGroups();
	}

	void StreamerEffectController::Clear()
	{
		*this = {};
	}

	StreamerGroup& StreamerEffectController::GetGroup(int itemNumber)
	{
		// Get group at itemNumber key.
		_groups.insert({ itemNumber, {} });
		auto& group = _groups.at(itemNumber);
		return group;
	}

	void StreamerEffectController::ClearInactiveGroups()
	{
		for (auto it = _groups.begin(); it != _groups.end();)
		{
			const auto& group = it->second;
			if (group.GetPools().empty())
			{
				it = _groups.erase(it);
				continue;
			}
			
			++it;
		}
	}
}
