#pragma once
#include <vector>
#include <SimpleMath.h>
#include "Math/Objects/GameBoundingBox.h"
#include "Math/Objects/Pose.h"
#include "Renderer/Structures/RendererLight.h"

namespace TEN::Renderer::Structures
{
	using namespace DirectX;
	using namespace DirectX::SimpleMath;

	struct RendererStatic
	{
		int ObjectNumber;
		int RoomNumber;
		int IndexInRoom;
		Pose PrevPose;
		Pose Pose;
		Matrix World;
		Vector4 Color;
		Vector4 AmbientLight;
		std::vector<RendererLight*> LightsToDraw;
		std::vector<RendererLightNode> CachedRoomLights;
		bool CacheLights;
		BoundingSphere OriginalSphere;
		BoundingSphere Sphere;

		void Update(float interpolationFactor)
		{
			auto pos = Vector3::Lerp(PrevPose.Position.ToVector3(), Pose.Position.ToVector3(), interpolationFactor);
			auto scale = Vector3::Lerp(PrevPose.Scale, Pose.Scale, interpolationFactor);
			auto orient = Matrix::Lerp(PrevPose.Orientation.ToRotationMatrix(), Pose.Orientation.ToRotationMatrix(), interpolationFactor);

			World = (orient * Matrix::CreateScale(scale) * Matrix::CreateTranslation(pos));
			Sphere = BoundingSphere(Vector3::Transform(OriginalSphere.Center, World), OriginalSphere.Radius * std::max(std::max(Pose.Scale.x, Pose.Scale.y), Pose.Scale.z));
			CacheLights = true;
		}
	};
}
