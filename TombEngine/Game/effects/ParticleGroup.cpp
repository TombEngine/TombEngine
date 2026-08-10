#include "framework.h"
#include "Game/effects/ParticleGroup.h"

#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/clock.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Math;

namespace TEN::Effects::ParticleGroups
{
	std::array<ParticleGroup, MAX_PARTICLE_GROUPS> ParticleGroupList = {};

	int CreateParticleGroup(GAME_OBJECT_ID objectID, int maxParticles)
	{
		maxParticles = std::clamp(maxParticles, 1, MAX_GROUP_PARTICLES);

		// Find inactive slot.
		for (int i = 0; i < MAX_PARTICLE_GROUPS; i++)
		{
			if (!ParticleGroupList[i].Active)
			{
				int nextGen = ParticleGroupList[i].Generation + 1;
				auto& group = ParticleGroupList[i];
				group = ParticleGroup();
				group.Generation = nextGen;
				group.ID = i;
				group.Active = true;
				group.ObjectID = objectID;
				group.MaxParticles = maxParticles;
				group.Particles.reserve(maxParticles);
				return i;
			}
		}

		TENLog("ParticleGroup limit reached.", LogLevel::Warning);
		return NO_VALUE;
	}

	bool ParticleGroupHandle::IsValid() const
	{
		if (Index < 0 || Index >= MAX_PARTICLE_GROUPS)
			return false;

		const auto& group = ParticleGroupList[Index];
		return group.Active && group.Generation == Generation;
	}

	ParticleGroup* ParticleGroupHandle::Get()
	{
		if (!IsValid())
			return nullptr;

		return &ParticleGroupList[Index];
	}

	const ParticleGroup* ParticleGroupHandle::Get() const
	{
		if (!IsValid())
			return nullptr;

		return &ParticleGroupList[Index];
	}

	bool ParticleGroup::IsMeshGroup() const
	{
		return Objects[ObjectID].loaded && Objects[ObjectID].nmeshes > 0 &&
			   !TEN::Utils::Contains(SpriteSequencesIds, (int)ObjectID);
	}

	void ParticleGroup::Start()
	{
		State = ParticleGroupState::Running;
	}

	void ParticleGroup::Stop()
	{
		State = ParticleGroupState::Stopped;
	}

	void ParticleGroup::Pause()
	{
		if (State == ParticleGroupState::Running)
			State = ParticleGroupState::Paused;
	}

	void ParticleGroup::Resume()
	{
		if (State == ParticleGroupState::Paused)
			State = ParticleGroupState::Running;
	}

	void ParticleGroup::EmitBurst(int count)
	{
		for (int i = 0; i < count; i++)
			EmitParticle();
	}

	void ParticleGroup::EmitParticle()
	{
		// Check particle limit.
		if (_activeCount >= MaxParticles)
			return;

		// Find inactive particle or add new one.
		GroupParticle* particle = nullptr;

		for (auto& p : Particles)
		{
			if (!p.Active)
			{
				particle = &p;
				break;
			}
		}

		if (particle == nullptr)
		{
			if ((int)Particles.size() >= MaxParticles)
				return;

			Particles.emplace_back();
			particle = &Particles.back();
		}

		// Initialize particle.
		*particle = GroupParticle();
		particle->ID = _nextParticleID++;
		particle->Active = true;
		_activeCount++;
		particle->Position = EmitterPosition;
		particle->PrevPosition = EmitterPosition;

		// Velocity with randomization.
		particle->Velocity = InitVelocity;
		if (InitVelocityRandom != Vector3::Zero)
		{
			particle->Velocity.x += Random::GenerateFloat(-InitVelocityRandom.x, InitVelocityRandom.x);
			particle->Velocity.y += Random::GenerateFloat(-InitVelocityRandom.y, InitVelocityRandom.y);
			particle->Velocity.z += Random::GenerateFloat(-InitVelocityRandom.z, InitVelocityRandom.z);
		}

		particle->Acceleration = InitAcceleration;

		// Lifetime.
		particle->Lifetime = Random::GenerateFloat(LifetimeMin, LifetimeMax);
		if (particle->Lifetime <= 0.0f)
			particle->Lifetime = 0.01f;

		// Size.
		particle->Size = Random::GenerateFloat(InitSizeMin, InitSizeMax);
		particle->PrevSize = particle->Size;

		// Color with randomization.
		float r = Random::GenerateFloat(InitColorMin.R(), InitColorMax.R());
		float g = Random::GenerateFloat(InitColorMin.G(), InitColorMax.G());
		float b = Random::GenerateFloat(InitColorMin.B(), InitColorMax.B());
		float a = Random::GenerateFloat(InitColorMin.A(), InitColorMax.A());
		particle->ParticleColor = Color(r, g, b, a);

		// Rotation.
		particle->Rotation = InitRotation;
		particle->PrevRotation = InitRotation;

		// Sprite / Mesh index.
		particle->SubIndex = InitSubIndex;
		particle->ObjectID = ObjectID;

		// Mesh orientation.
		particle->Orientation = InitOrientation;

		// Contact.
		particle->ContactRadius = InitContactRadius;

		// Room.
		particle->RoomNumber = RoomNumber;

		// Build initial transform for mesh particles.
		if (IsMeshGroup())
		{
			auto rotMatrix   = Matrix::CreateFromYawPitchRoll(
				particle->Orientation.y, particle->Orientation.x, particle->Orientation.z);
			auto scaleMatrix = Matrix::CreateScale(particle->Size);
			particle->Transform = scaleMatrix * rotMatrix * Matrix::CreateTranslation(particle->Position);
			particle->PrevTransform = particle->Transform;
		}
	}

	void ParticleGroup::Update(float dt)
	{
		if (State == ParticleGroupState::Paused)
			return;

		// Emit new particles if running.
		if (State == ParticleGroupState::Running && EmissionRate > 0.0f)
		{
			EmissionAccum += EmissionRate * dt;

			while (EmissionAccum >= 1.0f)
			{
				EmitParticle();
				EmissionAccum -= 1.0f;
			}
		}

		bool isMesh = IsMeshGroup();

		// Update existing particles.
		for (auto& p : Particles)
		{
			if (!p.Active)
				continue;

			// Update lifetime.
			p.Age += dt;
			p.AgeNormalized = p.Age / p.Lifetime;

			if (p.Age >= p.Lifetime)
			{
				p.Active = false;
				_activeCount--;
				continue;
			}

			// Update physics.
			p.Velocity += p.Acceleration * dt;
			p.Position += p.Velocity * dt;

			// Update rotation.
			p.Rotation += InitRotationVel * dt;

			// Rebuild transform for mesh particles.
			if (isMesh)
			{
				auto rotMatrix   = Matrix::CreateFromYawPitchRoll(
					p.Orientation.y, p.Orientation.x, p.Orientation.z);
				auto scaleMatrix = Matrix::CreateScale(p.Size);
				p.Transform = scaleMatrix * rotMatrix * Matrix::CreateTranslation(p.Position);
			}

			// Snap interpolation data after transform rebuild if teleport was requested.
			if (p.Teleport)
			{
				p.StoreInterpolationData();
				p.Teleport = false;
			}

			// Update contact status with Lara's deadly bounds.
			p.TouchingPlayer = false;
			if (LaraItem.Get() != nullptr)
			{
				float cr = p.ContactRadius;
				if (p.Position.x + cr > DeadlyBounds.X1 && p.Position.x - cr < DeadlyBounds.X2 &&
					p.Position.y + cr > DeadlyBounds.Y1 && p.Position.y - cr < DeadlyBounds.Y2 &&
					p.Position.z + cr > DeadlyBounds.Z1 && p.Position.z - cr < DeadlyBounds.Z2)
				{
					p.TouchingPlayer= true;
				}
			}
		}
	}

	int ParticleGroup::GetActiveCount() const
	{
		return _activeCount;
	}

	void ParticleGroup::StoreInterpolationData()
	{
		PrevEmitterPosition = EmitterPosition;

		for (auto& p : Particles)
		{
			if (p.Active)
				p.StoreInterpolationData();
		}
	}

	void StoreParticleGroupsInterpolationData()
	{
		for (auto& group : ParticleGroupList)
		{
			if (group.Active)
				group.StoreInterpolationData();
		}
	}

	void UpdateParticleGroups()
	{
		GetLaraDeadlyBounds();

		for (auto& group : ParticleGroupList)
		{
			if (!group.Active)
				continue;

			group.Update(DELTA_TIME);

			// Deactivate group if stopped and no active particles remain.
			if (group.State == ParticleGroupState::Stopped && group.GetActiveCount() == 0)
				group.Active = false;
		}
	}

	void ClearParticleGroups()
	{
		for (auto& group : ParticleGroupList)
		{
			int prevGeneration = group.Generation;
			group = ParticleGroup();
			group.Generation = prevGeneration + 1;
		}
	}
}
