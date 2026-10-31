#pragma once

#include "Objects/objectslist.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Effects::ParticleGroups
{
	using namespace DirectX::SimpleMath;

	// Constants.
	constexpr auto MAX_PARTICLE_GROUPS	  = 64;
	constexpr auto MAX_GROUP_PARTICLES	  = 256;
	constexpr auto DEFAULT_RENDER_DISTANCE = BLOCK(32);

	enum class ParticleGroupState
	{
		Stopped,
		Running,
		Paused
	};

	struct GroupParticle
	{
		// Identity
		int	 ID	    = 0;
		bool Active = false;

		// Physics
		Vector3 Position     = Vector3::Zero;
		Vector3 Velocity     = Vector3::Zero;
		Vector3 Acceleration = Vector3::Zero;

		// Visuals
		Color          ParticleColor = NEUTRAL_COLOR;
		float          Size           = 1.0f;
		float          Rotation       = 0.0f;
		int            SubIndex  = 0;
		GAME_OBJECT_ID ObjectID  = GAME_OBJECT_ID::ID_DEFAULT_SPRITES;

		// Mesh orientation (used when group renders meshes).
		Vector3 Orientation   = Vector3::Zero;
		Matrix  Transform     = Matrix::Identity;
		Matrix  PrevTransform = Matrix::Identity;

		// Contact.
		float ContactRadius        = 128.0f;
		bool  TouchingPlayer = false;

		// Lifetime
		float Age           = 0.0f;
		float Lifetime      = 1.0f;
		float AgeNormalized = 0.0f;

		// Room
		short RoomNumber = 0;

		// Interpolation
		Vector3 PrevPosition = Vector3::Zero;
		float   PrevSize     = 0.0f;
		float   PrevRotation = 0.0f;
		bool    Teleport     = false;

		void StoreInterpolationData()
		{
			PrevPosition  = Position;
			PrevSize      = Size;
			PrevRotation  = Rotation;
			PrevTransform = Transform;
		}
	};

	class ParticleGroup
	{
	public:
		// Identity
		int  ID         = 0;
		int  Generation = 0;
		bool Active     = false;

		// Object (can be sprite sequence or mesh object).
		GAME_OBJECT_ID ObjectID     = GAME_OBJECT_ID::ID_DEFAULT_SPRITES;
		int            MaxParticles = MAX_GROUP_PARTICLES;

		// Emission state
		ParticleGroupState State         = ParticleGroupState::Stopped;
		float              EmissionRate  = 10.0f;
		float              EmissionAccum = 0.0f;

		// Position (emission point)
		Vector3 EmitterPosition = Vector3::Zero;

		// Initial particle templates
		Vector3 InitVelocity       = Vector3::Zero;
		Vector3 InitVelocityRandom = Vector3::Zero;
		Vector3 InitAcceleration   = Vector3::Zero;
		float   LifetimeMin        = 1.0f;
		float   LifetimeMax        = 1.0f;
		float   InitSizeMin        = 16.0f;
		float   InitSizeMax        = 16.0f;
		Color   InitColorMin       = NEUTRAL_COLOR;
		Color   InitColorMax       = NEUTRAL_COLOR;
		float   InitRotation       = 0.0f;
		float   InitRotationVel    = 0.0f;
		int     InitSubIndex    = 0;

		// Mesh-specific initial templates.
		Vector3 InitOrientation = Vector3::Zero;

		// Initial contact.
		float InitContactRadius = 128.0f;

		// Rendering. Blend mode applies to sprite groups only; mesh groups use per-material blend modes.
		BlendMode RenderBlendMode = BlendMode::AlphaBlend;

		// Room
		short RoomNumber = 0;

		// Particles
		std::vector<GroupParticle> Particles = {};

		// Interpolation
		Vector3 PrevEmitterPosition = Vector3::Zero;

		// Construction
		ParticleGroup() = default;

		// Emission control
		void Start();
		void Stop();
		void Pause();
		void Resume();
		void EmitBurst(int count);

		// Update
		void Update(float dt);

		// Queries
		int  GetActiveCount() const;
		bool IsMeshGroup() const;

		void StoreInterpolationData();

	private:
		int _nextParticleID = 0;
		int _activeCount    = 0;

		void EmitParticle();
	};

	// Stable Lua handle for a particle group. Validates group identity via generation counter
	// before each access to prevent stale references after slot reuse.
	struct ParticleGroupHandle
	{
		int Index      = -1;
		int Generation = 0;

		bool IsValid() const;
		ParticleGroup*       Get();
		const ParticleGroup* Get() const;
	};

	// Global particle group storage.
	extern std::array<ParticleGroup, MAX_PARTICLE_GROUPS> ParticleGroupList;

	// Management functions.
	int  CreateParticleGroup(GAME_OBJECT_ID objectID, int maxParticles);
	void StoreParticleGroupsInterpolationData();
	void UpdateParticleGroups();
	void ClearParticleGroups();
}
