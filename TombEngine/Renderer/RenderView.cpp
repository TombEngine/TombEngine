#include "framework.h"
#include "Renderer/RenderView.h"

namespace TEN::Renderer
{
	RenderView::RenderView(const CameraInfo& camera, float roll, float fov, float nearPlane, float farPlane, float width, float height) :
		Camera(camera, roll, fov, nearPlane, farPlane, width, height) 
	{
		Viewport = {};
		Viewport.X = 0;
		Viewport.Y = 0;
		Viewport.Width = width;
		Viewport.Height = height;
		Viewport.MinDepth = 0;
		Viewport.MaxDepth = 1;
	}

	RenderView::RenderView(const Vector3& pos, const Vector3& dir, const Vector3& up, float width, float height, int roomNumber, float nearPlane, float farPlane, float fov) :
		Camera(pos, dir, up, roomNumber, width, height, fov, nearPlane, farPlane) 
	{
		Viewport = {};
		Viewport.X = 0;
		Viewport.Y = 0;
		Viewport.Width = width;
		Viewport.Height = height;
		Viewport.MinDepth = 0;
		Viewport.MaxDepth = 1;
	}
	 
	void RenderView::FillConstantBuffer(CCameraMatrixBuffer& bufferToFill)
	{
		bufferToFill.Projection = Camera.Projection;
		bufferToFill.View = Camera.View;
		bufferToFill.ViewProjection = Camera.ViewProjection;
		bufferToFill.InverseView = Camera.View.Invert();
		bufferToFill.InverseProjection = Camera.Projection.Invert();
		bufferToFill.CamDirectionWS = Vector4(Camera.WorldDirection);
		bufferToFill.CamPositionWS = Vector4(Camera.WorldPosition);
		bufferToFill.ViewSize = Camera.ViewSize;
		bufferToFill.InvViewSize = Camera.InvViewSize;
		bufferToFill.RoomNumber = Camera.RoomNumber;
		bufferToFill.NearPlane = Camera.NearPlane;
		bufferToFill.FarPlane = Camera.FarPlane;
		bufferToFill.AspectRatio = Camera.ViewSize.x / Camera.ViewSize.y;
		bufferToFill.TanHalfFOV = tanf(DEG_TO_RAD(Camera.FOV / 2.0f));
	}

	void RenderView::Clear() 
	{
		RoomsToDraw.clear();
		LightsToDraw.clear();
		SpritesToDraw.clear();
		DisplaySpritesToDraw.clear();
		SortedStaticsToDraw.clear();
		FogBulbsToDraw.clear();
		LensFlaresToDraw.clear();
		TransparentObjectsToDraw.clear();
		Mirrors.clear();
	}

	RenderViewCamera::RenderViewCamera(const CameraInfo& camera, float roll, float fov, float nearPlane, float farPlane, float width, float height)
	{
		RoomNumber = camera.RoomNumber;
		WorldPosition = Vector3(camera.Position.x, camera.Position.y, camera.Position.z);

		auto target = Vector3(camera.LookAt.x, camera.LookAt.y, camera.LookAt.z);
		
		// Safety clamps to avoid NaNs in view direction calculation.
		auto rawDirection = target - WorldPosition;

		if (rawDirection == Vector3::Zero)
			target.y -= 10;

		if (std::abs(rawDirection.x) < EPSILON && std::abs(rawDirection.z) < EPSILON)
			target.x -= 1;

		WorldDirection = target - WorldPosition;
		WorldDirection.Normalize();
		
		Vector3 up = -Vector3::UnitY;
		Matrix upRotation = Matrix::CreateFromAxisAngle(WorldDirection, roll);
		up = Vector3::Transform(up, upRotation);
		up.Normalize();

		RoomNumber = camera.RoomNumber;
		WorldPosition = camera.Position + camera.Offset;
		WorldDirection = target - WorldPosition;
		WorldDirection.Normalize();
		View = Matrix::CreateLookAt(WorldPosition, target, up);
		Projection = Matrix::CreatePerspectiveFieldOfView(fov, width / height, nearPlane, farPlane);
		ViewProjection = View * Projection;
		ViewSize = Vector2(width, height);
		InvViewSize = Vector2::One / Vector2(width, height);
		Frustum.Update(View, Projection);
		NearPlane = nearPlane;
		FarPlane = farPlane;
		FOV = fov;
	}

	RenderViewCamera::RenderViewCamera(const Vector3& pos, const Vector3& dir, const Vector3& up, int roomNumber, float width, float height, float fov, float nearPlane, float farPlane) 
	{
		constexpr auto LOOK_AT_DIST = BLOCK(10);

		float screenAspect = width / height;

		RoomNumber = roomNumber;
		WorldPosition = pos;
		WorldDirection = dir;
		View = Matrix::CreateLookAt(pos, pos + (dir * LOOK_AT_DIST), up);
		Projection = Matrix::CreatePerspectiveFieldOfView(fov, screenAspect, nearPlane, farPlane);
		ViewProjection = View * Projection;
		ViewSize = Vector2(width, height);
		InvViewSize = Vector2::One / Vector2(width, height);
		Frustum.Update(View, Projection);
		NearPlane = nearPlane;
		FarPlane = farPlane;
		FOV = fov;
	}
}
