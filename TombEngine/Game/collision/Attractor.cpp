#include "framework.h"
#include "Game/collision/Attractor.h"

#include "Game/camera.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Math/Math.h"
#include "Specific/level.h"
#include "Specific/trutils.h"
#include "Renderer/Renderer.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Math;
using namespace TEN::Utils;
using           TEN::Renderer::g_Renderer;

namespace TEN::Collision::Attractor
{
	AttractorObject::AttractorObject(AttractorType type, const Vector3& pos, int roomNumber, const Quaternion& orient, const std::vector<Vector3>& points)
	{
		TENAssert(!points.empty(), "Attempted to initialize invalid attractor with no points.");

		_type = type;
		_position = pos;
		_roomNumber = roomNumber;
		_orientation = orient;
		_points = points;

		if (_points.size() == 1)
		{
			_segmentLengths.push_back(0.0f);
			_length = 0.0f;
		}
		else
		{
			for (int i = 0; i < GetSegmentCount(); i++)
			{
				const auto& origin = _points[i];
				const auto& target = _points[i + 1];

				float segmentLength = Vector3::Distance(origin, target);

				_segmentLengths.push_back(segmentLength);
				_length += segmentLength;
			}
		}
		
		_aabb = Geometry::GetAabb(points);
	}

	AttractorObject::~AttractorObject()
	{
		DetachAllItems();
	}

	AttractorType AttractorObject::GetType() const
	{
		return _type;
	}

	int AttractorObject::GetRoomNumber() const
	{
		return _roomNumber;
	}

	float AttractorObject::GetLength() const
	{
		return _length;
	}

	// NOTE: AABB is in local space.
	const BoundingBox& AttractorObject::GetAabb() const
	{
		return _aabb;
	}

	unsigned int AttractorObject::GetSegmentCount() const
	{
		return std::max<unsigned int>((int)_points.size() - 1, 1);
	}

	unsigned int AttractorObject::GetSegmentIDAtPathDistance(float pathDist) const
	{
		// Check if single segment exists.
		if (GetSegmentCount() == 1)
			return 0;

		// Normalize distance along path.
		pathDist = NormalizePathDistance(pathDist);

		// Check if path distance is on attractor edge.
		if (pathDist <= 0.0f)
		{
			return 0;
		}
		else if (pathDist >= _length)
		{
			return (GetSegmentCount() - 1);
		}

		// Find segment at distance along path.
		float pathDistTraveled = 0.0f;
		for (int i = 0; i < GetSegmentCount(); i++)
		{
			// Accumulate distance traveled along path.
			pathDistTraveled += _segmentLengths[i];

			// Check for found segment.
			if (pathDistTraveled >= pathDist)
				return i;
		}

		// Use end segment ID as fallback.
		return (GetSegmentCount() - 1);
	}

	Vector3 AttractorObject::GetIntersectionAtPathDistance(float pathDist) const
	{
		auto transformMatrix = GetTransformMatrix();

		// Check if single point exists.
		if (_points.size() == 1)
			return Vector3::Transform(_points.front(), transformMatrix);

		// Normalize distance along path.
		pathDist = NormalizePathDistance(pathDist);

		// Check if line distance is outside attractor.
		if (pathDist <= 0.0f)
		{
			return Vector3::Transform(_points.front(), transformMatrix);
		}
		else if (pathDist >= _length)
		{
			return Vector3::Transform(_points.back(), transformMatrix);
		}

		// Find intersection at distance along path.
		float pathDistTraveled = 0.0f;
		for (int i = 0; i < GetSegmentCount(); i++)
		{
			float segmentLength = _segmentLengths[i];
			float remainingPathDist = pathDist - pathDistTraveled;

			// Found segment of distance along path; return intersection.
			if (remainingPathDist <= segmentLength)
			{
				// Get segment points.
				const auto& origin = _points[i];
				const auto& target = _points[i + 1];

				float alpha = remainingPathDist / segmentLength;
				return Vector3::Transform(Vector3::Lerp(origin, target, alpha), transformMatrix);
			}

			// Accumulate distance traveled along path.
			pathDistTraveled += segmentLength;
		}

		// Return end point.
		return Vector3::Transform(_points.back(), transformMatrix);
	}

	AttractorCollisionData AttractorObject::GetCollision(float pathDist, short headingAngle,
														 const Vector3& axis)
	{
		constexpr auto SPHERE_RADIUS = 1.0f;

		auto intersect = GetIntersectionAtPathDistance(pathDist);
		unsigned int segmentID = GetSegmentIDAtPathDistance(pathDist);

		auto attracColl = GetCollision(intersect, SPHERE_RADIUS, headingAngle, segmentID, axis);
		TENAssert(attracColl.has_value(), "AttractorObject::GetCollision(): Path overload failed.");
		return *attracColl;
	}

	std::optional<AttractorCollisionData> AttractorObject::GetCollision(const Vector3& pos, float radius, short headingAngle, unsigned int segmentID,
																		const Vector3& axis)
	{
		constexpr auto HEADING_ANGLE_OFFSET = ANGLE(-90.0);

		// Handle out-of-bounds segment ID.
		if (segmentID >= GetSegmentCount())
		{
			TENLog(fmt::format("Attempted to get attractor collision for invalid segment ID {}.", segmentID), LogLevel::Warning);
			segmentID = 0;
		}

		auto transformMatrix = GetTransformMatrix();
		auto localSphere = BoundingSphere(Vector3::Transform(pos, transformMatrix.Invert()), radius);
		bool isPath = (_points.size() > 1);

		// Test sphere-segment intersection.
		float intersectDist = isPath ? Geometry::GetDistanceToLine(localSphere.Center, _points[segmentID], _points[segmentID + 1]) :
									   Vector3::Distance(localSphere.Center, _points[segmentID]);
		if (intersectDist > localSphere.Radius)
			return std::nullopt;

		// Calculate intersection.
		auto intersect = isPath ? Geometry::GetClosestPointOnLinePerp(localSphere.Center, _points[segmentID], _points[segmentID + 1], axis) :
								  _points[segmentID];

		// Define intersection room number.
		auto dir = intersect - _position;
		dir.Normalize();
		float dist = Vector3::Distance(_position, intersect);
		int roomNumber = GetPointCollision(_position, _roomNumber, dir, dist).GetRoomNumber();

		// Calculate path distance.
		float pathDist = 0.0f;
		if (isPath)
		{
			// Accumulate distance traveled along path toward intersection.
			for (unsigned int i = 0; i < segmentID; i++)
				pathDist += _segmentLengths[i];

			// Accumulate final distance traveled along path toward intersection.
			pathDist += Vector3::Distance(_points[segmentID], intersect);
		}

		// TODO: Hardcoded to Y+, which is the expected case. Could also consider gravity axis in the future.
		auto refOrient = EulerAngles(0, headingAngle - EulerAngles(_orientation).y, 0);
		auto segmentOrient = isPath ?
			Geometry::GetOrientToPoint(_points[segmentID], _points[segmentID + 1]) + EulerAngles(0, EulerAngles(_orientation).y, 0) :
			refOrient;

		// Create attractor collision.
		auto attracColl = AttractorCollisionData{};
		attracColl.Attractor = this;
		attracColl.Intersection = Vector3::Transform(intersect, transformMatrix);
		attracColl.RoomNumber = roomNumber;
		attracColl.Distance2D = Vector2::Distance(Vector2(localSphere.Center.x, localSphere.Center.z), Vector2(intersect.x, intersect.z));
		attracColl.Distance3D = Vector3::Distance(localSphere.Center, intersect);
		attracColl.PathDistance = pathDist;
		attracColl.SegmentID = segmentID;
		attracColl.HeadingAngle = segmentOrient.y + HEADING_ANGLE_OFFSET;
		attracColl.SlopeAngle = abs(segmentOrient.x);
		attracColl.IsInFront = Geometry::IsPointInFront(localSphere.Center, intersect, refOrient);
		attracColl.IsFacingFront = Geometry::IsPointOnLeft(localSphere.Center, _points[segmentID], refOrient);
		return attracColl;
	}

	void AttractorObject::SetPosition(const Vector3& pos)
	{
		_position = pos;
	}

	void AttractorObject::SetRoomNumber(int roomNumber)
	{
		_roomNumber = roomNumber;
	}

	void AttractorObject::SetOrientation(const Quaternion& orient)
	{
		_orientation = orient;
	}

	bool AttractorObject::IsEdge() const
	{
		return (_type == AttractorType::Edge);
	}

	bool AttractorObject::IsWallEdge() const
	{
		return (_type == AttractorType::WallEdge);
	}

	bool AttractorObject::IsLoop() const
	{
		constexpr auto DIST_THRESHOLD = 1.0f;

		// Loop not possible if ingle segment exists;.
		if (GetSegmentCount() == 1)
			return false;

		// Test if start and end points occupy same approximate position.
		float distSqr = Vector3::DistanceSquared(_points.front(), _points.back());
		return (distSqr <= DIST_THRESHOLD);
	}

	bool AttractorObject::Intersects(const BoundingSphere& sphere) const
	{
		auto localCenter = Vector3::Transform(sphere.Center, GetTransformMatrix().Invert());
		auto localSphere = BoundingSphere(localCenter, sphere.Radius);
		return localSphere.Intersects(_aabb);
	}

	void AttractorObject::AttachItem(ItemInfo& item)
	{
		_itemNumbers.insert(item.Index);
	}

	void AttractorObject::DetachItem(ItemInfo& item)
	{
		_itemNumbers.erase(item.Index);
	}

	void AttractorObject::DetachAllItems()
	{
		// TODO: Crashes trying to loop?

		for (int itemNumber : _itemNumbers)
		{
			auto& item = g_Level.Items[itemNumber];
			if (item.IsLara())
			{
				auto& player = GetLaraInfo(item);

				if (player.Context.Attractor.Attractor != nullptr)
					player.Context.Attractor.Detach(item);
			}
		}

		_itemNumbers.clear();
	}

	void AttractorObject::DrawDebug(unsigned int segmentID) const
	{
		constexpr auto LABEL_OFFSET				= Vector3(0.0f, -CLICK(0.5f), 0.0f);
		constexpr auto INDICATOR_LINE_LENGTH	= BLOCK(1.0f / 20.0f);
		constexpr auto SPHERE_RADIUS			= BLOCK(1.0f / 52.0f);
		constexpr auto COLOR_YELLOW_OPAQUE		= Color(1.0f, 1.0f, 0.4f);
		constexpr auto COLOR_YELLOW_TRANSLUCENT = Color(1.0f, 1.0f, 0.4f, 0.3f);
		constexpr auto COLOR_GREEN				= Color(0.4f, 1.0f, 0.4f);
		constexpr auto BOX_COLOR				= Color(1.0f, 1.0f, 1.0f, 0.1f);

		auto getLabelScale = [](const Vector3& camPos, const Vector3& labelPos)
		{
			constexpr auto RANGE		   = BLOCK(5);
			constexpr auto LABEL_SCALE_MAX = 0.8f;
			constexpr auto LABEL_SCALE_MIN = 0.4f;

			float camDist = Vector3::Distance(camPos, labelPos);
			float alpha = camDist / RANGE;
			return Lerp(LABEL_SCALE_MAX, LABEL_SCALE_MIN, alpha);
		};

		auto transformMatrix = GetTransformMatrix();

		// Define label string.
		auto labelString = std::string();
		switch (_type)
		{
		case AttractorType::Edge:
			labelString = "Edge";
			break;

		case AttractorType::WallEdge:
			labelString = "Wall edge";
			break;

		default:
			labelString = "Unknown attractor";
			break;
		}

		// Draw debug elements.
		if (_points.size() == 1)
		{
			// Get segment position.
			auto pos = Vector3::Transform(_points.front(), transformMatrix);

			// Draw sphere.
			DrawDebugSphere(pos, SPHERE_RADIUS, COLOR_YELLOW_TRANSLUCENT, RendererDebugPage::AttractorStats, false);

			// Define label parameters.
			auto labelPos2D = Get2DPosition(pos);
			float labelScale = getLabelScale(Camera.pos.ToVector3(), pos);

			// Draw label.
			if (labelPos2D.has_value())
				DrawDebugString(labelString, *labelPos2D, COLOR_YELLOW_OPAQUE, labelScale, RendererDebugPage::AttractorStats);
		}
		else
		{
			// Get segment points.
			const auto& origin = Vector3::Transform(_points[segmentID], transformMatrix);
			const auto& target = Vector3::Transform(_points[segmentID + 1], transformMatrix);

			DrawDebugLine(origin, LaraItem->Pose.Position.ToVector3(), Color());

			// Draw main line.
			DrawDebugLine(origin, target, COLOR_YELLOW_OPAQUE, RendererDebugPage::AttractorStats);

			auto orient = EulerAngles(0, Geometry::GetOrientToPoint(origin, target).y + ANGLE(90.0f), 0);
			auto dir = orient.ToDirection();

			// Draw segment heading indicator lines.
			DrawDebugLine(origin, Geometry::TranslatePoint(origin, dir, INDICATOR_LINE_LENGTH), COLOR_GREEN, RendererDebugPage::AttractorStats);
			DrawDebugLine(target, Geometry::TranslatePoint(target, dir, INDICATOR_LINE_LENGTH), COLOR_GREEN, RendererDebugPage::AttractorStats);

			// Define label parameters.
			auto labelPos = ((origin + target) / 2) + LABEL_OFFSET;
			auto labelPos2D = Get2DPosition(labelPos);
			float labelScale = getLabelScale(Camera.pos.ToVector3(), labelPos);

			// Draw label.
			if (labelPos2D.has_value())
				DrawDebugString(labelString, *labelPos2D, COLOR_YELLOW_OPAQUE, labelScale, RendererDebugPage::AttractorStats);

			// Draw start indicator line.
			if (segmentID == 0)
				DrawDebugLine(origin, Geometry::TranslatePoint(origin, -Vector3::UnitY, INDICATOR_LINE_LENGTH), COLOR_GREEN, RendererDebugPage::AttractorStats);
			
			// Draw end indicator line.
			if (segmentID == (GetSegmentCount() - 1))
				DrawDebugLine(target, Geometry::TranslatePoint(target, -Vector3::UnitY, INDICATOR_LINE_LENGTH), COLOR_GREEN, RendererDebugPage::AttractorStats);

			// Draw local AABB.
			/*auto obb = BoundingOrientedBox(_position + _aabb.Center, _aabb.Extents, _orientation);
			DrawDebugBox(obb, BOX_COLOR, RendererDebugPage::AttractorStats);*/

			// Draw local segment AABBs.
			/*for (const auto& aabb : _segmentAabbs)
			{
				auto obb = BoundingOrientedBox(_position + _aabb.Center, _aabb.Extents, _orientation);
				DrawDebugBox(obb, BOX_COLOR, RendererDebugPage::AttractorStats);
			}*/
		}
	}

	void AttractorObject::DrawDebug() const
	{
		for (int i = 0; i < GetSegmentCount(); i++)
			DrawDebug(i);
	}

	float AttractorObject::NormalizePathDistance(float pathDist) const
	{
		// Check if distance along path is within length bounds.
		if (pathDist >= 0.0f && pathDist <= _length)
			return pathDist;

		// Wrap or clamp distance along path.
		return (IsLoop() ? fmod(pathDist + _length, _length) : std::clamp(pathDist, 0.0f, _length));
	}

	Matrix AttractorObject::GetTransformMatrix() const
	{
		auto translationMatrix = Matrix::CreateTranslation(_position);
		auto rotMatrix = Matrix::CreateFromQuaternion(_orientation);
		return (rotMatrix * translationMatrix);
	}

	static std::vector<AttractorObject*> GetBoundedAttractors(const BoundingSphere& sphere, int roomNumber)
	{
		// Debug.
		auto getDebugAttractors = []()
		{
			auto& player = GetLaraInfo(*LaraItem);

			auto attracs = std::vector<AttractorObject*>{};
			attracs.push_back(&*player.Context.DebugAttracs.Attrac0);
			attracs.push_back(&*player.Context.DebugAttracs.Attrac1);
			attracs.push_back(&*player.Context.DebugAttracs.Attrac2);

			for (auto& attrac : player.Context.DebugAttracs.Attracs)
				attracs.push_back(&attrac);

			return attracs;
		};

		DrawDebugSphere(sphere.Center, sphere.Radius, Vector4::One, RendererDebugPage::AttractorStats);

		auto boundedAttracs = std::vector<AttractorObject*>{};

		// TEMP
		// Collect debug attractors.
		auto debugAttracs = getDebugAttractors();
		for (auto* attrac : debugAttracs)
		{
			if (attrac->Intersects(sphere))
				boundedAttracs.push_back(attrac);
		}

		// 1) Collect bounded room attractors.
		auto& room = g_Level.Rooms[roomNumber];
		for (int neighborRoomNumber : room.NeighborRoomNumbers)
		{
			// Check if room is active.
			auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			if (!neighborRoom.Active())
				continue;

			auto attracIds = neighborRoom.AttractorTree.GetBoundedObjectIds(sphere);
			for (int attracID : attracIds)
			{
				auto& attrac = neighborRoom.Attractors[attracID];
				boundedAttracs.push_back(&attrac);
			}
		}

		auto visitedBridgeItemNumbers = std::set<int>{};

		// 2) Collect bounded bridge attractors.
		unsigned int sectorSearchDepth = (int)ceil(sphere.Radius / BLOCK(1));
		auto sectors = GetNeighborSectors((Vector3)sphere.Center, roomNumber, sectorSearchDepth);
		for (const auto* sector : sectors)
		{
			// TODO: Check, not working. Would optimise this check a little.
			// Test if sphere intersects sector.
			//if (!sphere.Intersects(sector->Box))
			//	continue;

			PrintDebugMessage("%d", (int)sector->BridgeItemNumbers.size());

			// Run through bridges in sector.
			for (int bridgeItemNumber : sector->BridgeItemNumbers)
			{
				// Test if bridge was already visited.
				if (Contains(visitedBridgeItemNumbers, bridgeItemNumber))
					continue;
				visitedBridgeItemNumbers.insert(bridgeItemNumber);

				auto& bridgeItem = g_Level.Items[bridgeItemNumber];
				auto& bridge = GetBridgeObject(bridgeItem);

				// Test if sphere intersects bridge attractor OBB.
				if (!bridge.GetAttractor().Intersects(sphere))
					continue;

				boundedAttracs.push_back(&bridge.GetAttractor());
			}
		}

		// 3) Return bounded attractors.
		return boundedAttracs;
	}

	std::vector<AttractorCollisionData> GetAttractorCollisions(const Vector3& pos, float radius, int roomNumber, short headingAngle,
															   const Vector3& axis)
	{
		constexpr auto COLL_COUNT_MAX = 64;

		// 1) Get bounded attractors.
		auto attracs = GetBoundedAttractors(BoundingSphere(pos, radius), roomNumber);

		// 2) Collect attractor collisions.
		auto attracColls = std::vector<AttractorCollisionData>{};
		for (auto* attrac : attracs)
		{
			// Collide every segment.
			for (int i = 0; i < attrac->GetSegmentCount(); i++)
			{
				auto attracColl = attrac->GetCollision(pos, radius, headingAngle, i);
				if (attracColl.has_value())
					attracColls.push_back(std::move(*attracColl));
			}
		}

		// 3) Sort collisions by 2D then 3D distance.
		std::sort(
			attracColls.begin(), attracColls.end(),
			[](const auto& attracColl0, const auto& attracColl1)
			{
				if (attracColl0.Distance2D == attracColl1.Distance2D)
					return (attracColl0.Distance3D < attracColl1.Distance3D);
				
				return (attracColl0.Distance2D < attracColl1.Distance2D);
			});

		// 4) Trim collection.
		if (attracColls.size() > COLL_COUNT_MAX)
			attracColls.resize(COLL_COUNT_MAX);

		// 5) Return attractor collisions in capped vector sorted by 2D then 3D distance.
		return attracColls;
	}

	std::vector<AttractorCollisionData> GetAttractorCollisions(const Vector3& pos, float radius, int roomNumber, short headingAngle,
															   float forward, float down, float right,
															   const Vector3& axis)
	{
		auto relOffset = Vector3(right, down, forward);
		auto rotMatrix = AxisAngle(axis, headingAngle).ToRotationMatrix();
		auto probePos = pos + Vector3::Transform(relOffset, rotMatrix);
		int probeRoomNumber = GetPointCollision(pos, roomNumber, headingAngle, forward, down, right).GetRoomNumber();

		return GetAttractorCollisions(probePos, radius, probeRoomNumber, headingAngle);
	}
	
	void DrawNearbyAttractors(const Vector3& pos, int roomNumber, short headingAngle)
	{
		constexpr auto RADIUS = BLOCK(5);

		auto uniqueAttracs = std::set<AttractorObject*>{};

		auto attracColls = GetAttractorCollisions(pos, RADIUS, roomNumber, headingAngle, 0.0f, 0.0f, 0.0f);
		for (const auto& attracColl : attracColls)
		{
			uniqueAttracs.insert(attracColl.Attractor);
			attracColl.Attractor->DrawDebug(attracColl.SegmentID);
		}

		if (g_Renderer.GetDebugPage() == RendererDebugPage::AttractorStats)
		{
			PrintDebugMessage("Nearby attractors: %d", (int)uniqueAttracs.size());
			PrintDebugMessage("Nearby attractor segments: %d", (int)attracColls.size());
		}
	}
}
