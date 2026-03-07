#pragma once

struct ItemInfo;

namespace TEN::Animation
{
	enum class AnimCommandType
	{
		None,
		MoveOrigin,
		JumpVelocity,
		AttackReady,
		Deactivate,
		SoundEffect,
		FlipEffect,
		DisableInterpolation
	};

	// TODO: Not needed. SoundCondition enum class exists.
	enum class SoundEffectEnvCondition
	{
		Always,
		Land,
		ShallowWater,
		Quicksand,
		Underwater
	};

	class AnimCommand abstract
	{
	public:
		virtual void Execute(ItemInfo& item, bool isFrameBased) const = 0;
	};

	class MoveOriginCommand : public AnimCommand
	{
	private:
		const Vector3 _relOffset = Vector3::Zero;
		
	public:
		MoveOriginCommand(const Vector3& relOffset) : _relOffset(relOffset) {};
		void Execute(ItemInfo& item, bool isFrameBased) const override;
	};
	
	class JumpVelocityCommand : public AnimCommand
	{
	private:
		const Vector3 _jumpVelocity = Vector3::Zero;

	public:
		JumpVelocityCommand(const Vector3& jumpVel) : _jumpVelocity(jumpVel) {};
		void Execute(ItemInfo& item, bool isFrameBased) const override;
	};

	class AttackReadyCommand : public AnimCommand
	{
	public:
		AttackReadyCommand() {};
		void Execute(ItemInfo& item, bool isFrameBased) const override;
	};
	
	class DeactivateCommand : public AnimCommand
	{
	public:
		DeactivateCommand() {};
		void Execute(ItemInfo& item, bool isFrameBased) const override;
	};

	class SoundEffectCommand : public AnimCommand
	{
	private:
		const int					  _soundID		= 0;
		const int					  _frameNumber	= 0;
		const SoundEffectEnvCondition _envCondition = SoundEffectEnvCondition::Always;

	public:
		SoundEffectCommand(int soundID, int frameNumber, SoundEffectEnvCondition envCond) : _soundID(soundID), _frameNumber(frameNumber), _envCondition(envCond) {};
		void Execute(ItemInfo& item, bool isFrameBased) const override;
	};

	class FlipEffectCommand : public AnimCommand
	{
	private:
		const int _flipEffectID = 0;
		const int _frameNumber	= 0;

	public:
		FlipEffectCommand(int flipEffectID, int frameNumber) : _flipEffectID(flipEffectID), _frameNumber(frameNumber) {};
		void Execute(ItemInfo& item, bool isFrameBased) const override;
	};

	class DisableInterpolationCommand : public AnimCommand
	{
	private:
		const int _frameNumber = 0;

	public:
		DisableInterpolationCommand(int frameNumber) : _frameNumber(frameNumber) {};
		void Execute(ItemInfo& item, bool isFrameBased) const override;
	};
}
