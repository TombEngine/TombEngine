#pragma once

#include "Game/Lara/Context/Climb.h"
#include "Game/Lara/Context/GroundMovement.h"
#include "Game/Lara/Context/HangDescent.h"
#include "Game/Lara/Context/Jump.h"
#include "Game/Lara/Context/MonkeySwing.h"
#include "Game/Lara/Context/Vault.h"

// Move PlayerContext struct back to lara_struct.h?

#include "Game/Lara/Context/Structs.h"
#include "Math/Math.h"

//----debug
#include "Game/collision/Attractor.h"
using namespace TEN::Collision::Attractor;
//----

struct CollisionInfo;
struct ItemInfo;
struct LaraInfo;

using namespace TEN::Math;

namespace TEN::Player
{
	struct PlayerAttractorData
	{
		AttractorObject* Attractor		 = nullptr;
		float			 PathDistance	 = 0.0f;
		Vector3			 RelPosOffset	 = Vector3::Zero;
		EulerAngles		 RelOrientOffset = EulerAngles::Identity;

		Vector3		RelDeltaPos	   = Vector3::Zero;
		EulerAngles RelDeltaOrient = EulerAngles::Identity;

		~PlayerAttractorData();

		void Attach(ItemInfo& playerItem, AttractorObject& attrac, float chainDist,
					const Vector3& relPosOffset, const EulerAngles& relOrientOffset,
					const Vector3& relDeltaPos, const EulerAngles& relDeltaOrient);
		void Detach(ItemInfo& playerItem);

		void Update(ItemInfo& playerItem, AttractorObject& attrac, float chainDist,
					const Vector3& relPosOffset, const EulerAngles& relOrientOffset);
	};

	struct DebugAttractorData
	{
		std::optional<AttractorObject> Attrac0 = std::nullopt;
		std::optional<AttractorObject> Attrac1 = std::nullopt;
		std::optional<AttractorObject> Attrac2 = std::nullopt;

		std::vector<AttractorObject> Attracs = {};
	};

	// TODO: Savegame for attractors.
	struct PlayerContext
	{
	private:
		// TODO: Make LaraInfo aware of its parent ItemInfo.
		// Parent pointers
		/*const ItemInfo*		 Item = nullptr;
		const LaraInfo*		 Player = nullptr;
		const CollisionInfo* Collision = nullptr;*/

	public:
		PlayerAttractorData Attractor	 = {};
		DebugAttractorData	DebugAttracs = {};

		int			ProjectedFloorHeight = 0; // Used for primitive offset blend. TODO: Real offset blend feature + object parenting. -- Sezz 2023.09.27
		float		CalcJumpVelocity	 = 0;
		//Pose		NextCornerPos		 = Pose::Zero; // TODO: Remove from savegame before deleting this line.
		EulerAngles OrientOffset		 = EulerAngles::Identity;

		int		 WaterSurfaceDist	= 0;
		short	 WaterCurrentActive = 0; // Sink number? Often used as bool.
		Vector3i WaterCurrentPull	= Vector3i::Zero;

		int InteractedItem = 0; // InteractedItemNumber.
		int Vehicle		   = 0; // VehicleItemNumber.

		PlayerContext() = default;
		PlayerContext(const ItemInfo& item, const CollisionInfo& coll);

		// TODO: Move all functions below into this class. Resulting syntax will be a neat player.Context.CanDoXYZ().
	};

	// Slide contexts

	bool CanSlide(const ItemInfo& item, const CollisionInfo& coll);
	bool CanSteerOnSlide(const ItemInfo& item, const CollisionInfo& coll);

	// Object interaction contexts

	bool CanDismountTightrope(const ItemInfo& item, const CollisionInfo& coll);
}
