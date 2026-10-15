#pragma once

#include "Math/Math.h"

using namespace TEN::Math;

struct ItemInfo;

namespace TEN::Collision::Interaction
{
	using InteractionRoutine = std::function<void(ItemInfo& interactor, ItemInfo& interactable)>;
	using OrientConstraintPair = std::pair<EulerAngles, EulerAngles>;

	enum class InteractionBasisType
	{
		Latch,
		Walk
	};

	struct InteractionBasis
	{
	public:
		Vector3i				   PosOffset = Vector3i::Zero;
		std::optional<EulerAngles> OrientOffset = std::nullopt;
		BoundingOrientedBox		   Box = BoundingOrientedBox();
		OrientConstraintPair	   OrientConstraint = OrientConstraintPair(EulerAngles::Identity, EulerAngles::Identity);

		InteractionBasis() {};
		InteractionBasis(const Vector3i& posOffset, const std::optional<EulerAngles>& orientOffset, const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint);
		InteractionBasis(const Vector3i& posOffset, const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint);
		InteractionBasis(const std::optional<EulerAngles>& orientOffset, const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint);
		InteractionBasis(const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint);
	};

	bool TestInteraction(const ItemInfo& interactor, const ItemInfo& interactable, const InteractionBasis& basis, std::optional<BoundingOrientedBox> expansionBox = std::nullopt);
	void SetInteraction(ItemInfo& interactor, ItemInfo& interactable, const InteractionBasis& basis, const InteractionRoutine& routine, InteractionBasisType type, const Vector3i& extraPosOffset = Vector3i::Zero, const EulerAngles& extraOrientOffset = EulerAngles::Identity);

	void DrawDebug(const ItemInfo& interactable, const InteractionBasis& basis);
}