#include "framework.h"
#include "Renderer/RenderView.h"

namespace TEN::Renderer
{
	RenderView::RenderView(CAMERA_INFO* cam, float roll, float fov, float nearPlane, float farPlane, int w, int h) : Camera(cam, roll, fov, nearPlane, farPlane, w, h) 
	{
		Viewport = {};
		Viewport.TopLeftX = 0;
		Viewport.TopLeftY = 0;
		Viewport.Width = w;
		Viewport.Height = h;
		Viewport.MinDepth = 0;
		Viewport.MaxDepth = 1;
	}

	RenderView::RenderView(const Vector3& pos, const Vector3& dir, const Vector3& up, int w, int h, int room, float nearPlane, float farPlane, float fov) : Camera(pos, dir, up, room, w, h, fov, nearPlane, farPlane) 
	{

		Viewport = {};
		Viewport.TopLeftX = 0;
		Viewport.TopLeftY = 0;
		Viewport.Width = w;
		Viewport.Height = h;
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

	RenderViewCamera::RenderViewCamera(CAMERA_INFO* cam, float roll, float fov, float n, float f, int w, int h)
	{
		RoomNumber = cam->pos.RoomNumber;
		WorldPosition = Vector3(cam->pos.x, cam->pos.y, cam->pos.z);

		Vector3 target = Vector3(cam->target.x, cam->target.y, cam->target.z);
		if ((target - WorldPosition) == Vector3::Zero)
			target.y -= 10;

		WorldDirection = target - WorldPosition;
		WorldDirection.Normalize();
		
		Vector3 up = -Vector3::UnitY;
		Matrix upRotation = Matrix::CreateFromYawPitchRoll(0.0f, 0.0f, roll);
		up = Vector3::Transform(up, upRotation);
		up.Normalize();

		View = Matrix::CreateLookAt(WorldPosition, target, up);
		Projection = Matrix::CreatePerspectiveFieldOfView(fov, w / (float)h, n, f);
		ViewProjection = View * Projection;
		ViewSize = { (float)w, (float)h };
		InvViewSize = { 1.0f / w, 1.0f / h };
		Frustum.Update(View, Projection);
		NearPlane = n;
		FarPlane = f;
		FOV = fov;
	}

	RenderViewCamera::RenderViewCamera(const Vector3& pos, const Vector3& dir, const Vector3& up, int room, int width, int height, float fov, float n, float f) 
	{
		RoomNumber = room;
		WorldPosition = pos;
		WorldDirection = dir;
		View = Matrix::CreateLookAt(pos, pos + dir * 10240, up);
		float aspect = (float)width / (float)height;
		Projection = Matrix::CreatePerspectiveFieldOfView(fov, aspect, n, f);
		ViewProjection = View * Projection;
		ViewSize = { (float)width, (float)height };
		InvViewSize = { 1.0f / width, 1.0f / height };
		Frustum.Update(View, Projection);
		NearPlane = n;
		FarPlane = f;
		FOV = fov;
	}
}
