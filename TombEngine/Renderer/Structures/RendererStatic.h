#pragma once

#include "Math/Objects/GameBoundingBox.h"
#include "Math/Objects/Pose.h"
#include "Renderer/Structures/RendererLight.h"

namespace TEN::Renderer::Structures
{
	// Static mesh instance of a room, with interpolated world matrix, culling sphere and cached lights.
	struct RendererStatic
	{
		// Static slot ID and the room of the instance.
		int ObjectNumber;
		int RoomNumber;

		// PrevPose and Pose are the poses of the previous and current game tick; Update blends them into World.
		Pose 	PrevPose;
		Pose	Pose;
		Matrix	World;
		Vector4 Color;
		Vector4 AmbientLight;

		std::vector<RendererLight*>	   LightsToDraw;
		std::vector<RendererLightNode> CachedRoomLights;
		// CachedRoomLights holds the room lights found for the static; CacheLights asks for a new gathering and is set by Update.
		bool CacheLights;

		// OriginalSphere is in object space; Sphere is its world space version used for culling.
		BoundingSphere OriginalSphere;
		BoundingSphere Sphere;

		// Rebuilds the interpolated world matrix and sphere, and requests a light refresh.
		void Update(float interpolationFactor)
		{
			if (Vector3i::Distance(PrevPose.Position, Pose.Position) > BIG_DISTANCE_THRESHOLD)
				PrevPose = Pose;

			auto pos = Vector3::Lerp(PrevPose.Position.ToVector3(), Pose.Position.ToVector3(), interpolationFactor);
			auto scale = Vector3::Lerp(PrevPose.Scale, Pose.Scale, interpolationFactor);
			
			auto translationMatrix = Matrix::CreateTranslation(pos);
			auto scaleMatrix = Matrix::CreateScale(scale);
			auto rotMatrix = Matrix::Lerp(PrevPose.Orientation.ToRotationMatrix(), Pose.Orientation.ToRotationMatrix(), interpolationFactor);
			
			auto worldMatrix = rotMatrix * scaleMatrix * translationMatrix;

			auto sphereCenter = Vector3::Transform(OriginalSphere.Center, worldMatrix);
			float sphereScale = std::max({ Pose.Scale.x, Pose.Scale.y, Pose.Scale.z });
			float sphereRadius = OriginalSphere.Radius * sphereScale;

			World = worldMatrix;
			Sphere = BoundingSphere(sphereCenter, sphereRadius);
			
			CacheLights = true;
		}
	};
}
