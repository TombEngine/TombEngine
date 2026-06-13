#pragma once

#include <cstdint>

#include "framework.h"
#include "Math/Math.h"
#include "Specific/Serialization/flatbuffers/ten_common_generated.h"

namespace TEN::Serialization
{
	Common::EulerAngles FromEulerAngles(const EulerAngles& eulers);
	Common::Vector2 FromVector2(const DirectX::SimpleMath::Vector2& vec);
	Common::Vector2 FromVector2i(const TEN::Math::Vector2i& vec);
	Common::Vector3 FromVector3(const DirectX::SimpleMath::Vector3& vec);
	Common::Vector3 FromVector3i(const Vector3i& vec);
	Common::Vector4 FromVector4(const DirectX::SimpleMath::Vector4& vec);
	Common::GameVector FromGameVector(const GameVector& vec);
	Common::Pose FromPose(const Pose& pose);

	EulerAngles ToEulerAngles(const Common::EulerAngles* eulers);
	DirectX::SimpleMath::Vector2 ToVector2(const Common::Vector2* vec);
	TEN::Math::Vector2i ToVector2i(const Common::Vector2* vec);
	DirectX::SimpleMath::Vector3 ToVector3(const Common::Vector3* vec);
	Vector3i ToVector3i(const Common::Vector3* vec);
	DirectX::SimpleMath::Vector4 ToVector4(const Common::Vector3* vec);
	DirectX::SimpleMath::Vector4 ToVector4(const Common::Vector4* vec);
	GameVector ToGameVector(const Common::GameVector* vec);
	Pose ToPose(const Common::Pose& pose);
}