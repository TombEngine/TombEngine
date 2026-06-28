#include "framework.h"
#include "Flatbuffers.h"

namespace Common = TEN::Serialization::Common;

namespace TEN::Serialization
{
	Common::EulerAngles FromEulerAngles(const EulerAngles& eulers)
	{
		return Common::EulerAngles(eulers.x, eulers.y, eulers.z);
	}

	Common::Vector2 FromVector2(const DirectX::SimpleMath::Vector2& vec)
	{
		return Common::Vector2(vec.x, vec.y);
	}

	Common::Vector2 FromVector2i(const TEN::Math::Vector2i& vec)
	{
		return Common::Vector2((float)vec.x, (float)vec.y);
	}

	Common::Vector3 FromVector3(const DirectX::SimpleMath::Vector3& vec)
	{
		return Common::Vector3(vec.x, vec.y, vec.z);
	}

	Common::Vector3 FromVector3i(const Vector3i& vec)
	{
		return Common::Vector3((float)vec.x, (float)vec.y, (float)vec.z);
	}

	Common::Vector4 FromVector4(const DirectX::SimpleMath::Vector4& vec)
	{
		return Common::Vector4(vec.x, vec.y, vec.z, vec.w);
	}

	Common::GameVector FromGameVector(const GameVector& vec)
	{
		return Common::GameVector(vec.x, vec.y, vec.z, (int)vec.RoomNumber);
	}

	Common::Pose FromPose(const Pose& pose)
	{
		return Common::Pose(FromVector3i(pose.Position), FromEulerAngles(pose.Orientation), FromVector3(pose.Scale));
	}

	EulerAngles ToEulerAngles(const Common::EulerAngles* eulers)
	{
		return ::EulerAngles((short)round(eulers->x()), (short)round(eulers->y()), (short)round(eulers->z()));
	}

	DirectX::SimpleMath::Vector2 ToVector2(const Common::Vector2* vec)
	{
		return DirectX::SimpleMath::Vector2(vec->x(), vec->y());
	}

	TEN::Math::Vector2i ToVector2i(const Common::Vector2* vec)
	{
		return TEN::Math::Vector2i((int)round(vec->x()), (int)round(vec->y()));
	}

	Vector3i ToVector3i(const Common::Vector3* vec)
	{
		return ::Vector3i((int)round(vec->x()), (int)round(vec->y()), (int)round(vec->z()));
	}

	DirectX::SimpleMath::Vector3 ToVector3(const Common::Vector3* vec)
	{
		return DirectX::SimpleMath::Vector3(vec->x(), vec->y(), vec->z());
	}

	DirectX::SimpleMath::Vector4 ToVector4(const Common::Vector3* vec)
	{
		return DirectX::SimpleMath::Vector4(vec->x(), vec->y(), vec->z(), 1.0f);
	}

	DirectX::SimpleMath::Vector4 ToVector4(const Common::Vector4* vec)
	{
		return DirectX::SimpleMath::Vector4(vec->x(), vec->y(), vec->z(), vec->w());
	}

	GameVector ToGameVector(const Common::GameVector* vec)
	{
		return ::GameVector(vec->x(), vec->y(), vec->z(), (short)vec->room_number());
	}

	Pose ToPose(const Common::Pose& pose)
	{
		return ::Pose(ToVector3i(&pose.position()), ToEulerAngles(&pose.orientation()), ToVector3(&pose.scale()));
	}
}
