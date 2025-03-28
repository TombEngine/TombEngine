#pragma once

#include "Math/Math.h"

struct ItemInfo;

using namespace TEN::Math;

namespace TEN::Entities::Generic
{
	class BridgeObject
	{
	private:
		// Fields

		bool _isEnabled = false;

		CollisionMesh	_collisionMesh = CollisionMesh();
		// TODO: Uncomment when attractors are complete.
		//AttractorObject _attractor	   = AttractorObject();

		Pose				_prevTransform	= Pose::Zero;
		int					_prevRoomNumber = 0;
		BoundingBox			_prevAabb		= BoundingBox();
		BoundingOrientedBox _prevObb		= BoundingOrientedBox();

	public:
		// Routines

		std::function<std::optional<int>(const ItemInfo& item, const Vector3i& pos)> GetFloorHeight	  = nullptr;
		std::function<std::optional<int>(const ItemInfo& item, const Vector3i& pos)> GetCeilingHeight = nullptr;
		std::function<int(const ItemInfo& item)>									 GetFloorBorder	  = nullptr;
		std::function<int(const ItemInfo& item)>									 GetCeilingBorder = nullptr;

		// Getters

		const CollisionMesh&   GetCollisionMesh() const;
		// TODO: Uncomment when attractors are complete.
		//const AttractorObject& GetAttractor() const;

		// Inquirers

		bool IsEnabled() const;

		// Utilities

		void Initialize(const ItemInfo& item);
		void Update(const ItemInfo& item);
		void Enable(const ItemInfo& item);
		void Disable(const ItemInfo& item);

	private:
		// Helpers

		void InitializeCollisionMesh(const ItemInfo& item);
		void InitializeAttractor(const ItemInfo& item);

		void UpdateCollisionMesh(const ItemInfo& item);
		void UpdateAttractor(const ItemInfo& item);
		void UpdateSectorAssignments(const ItemInfo& item);

		void AssignSectors(const ItemInfo& item);
		void DeassignSectors(const ItemInfo& item) const;
	};

	const BridgeObject& GetBridgeObject(const ItemInfo& item);
	BridgeObject&		GetBridgeObject(ItemInfo& item);
}
