#include "Camera.h"


Camera::Camera()
{
	ViewMatrix = XMMatrixLookAtLH(EyePosition, FocusPosition, UpDirection);
	ProjectionMatrix = XMMatrixPerspectiveFovLH(FovAngleY, AspectRatio, NearZ, FarZ);
}

void Camera::Walk(float Stride)
{
	EyePosition += Stride * ViewDirection;
	FocusPosition += Stride * ViewDirection;
}

void Camera::Strafe(float Stride)
{
	EyePosition += Stride * RightDirection;
	FocusPosition += Stride * RightDirection;
}

//鼠标Y方向上移动带来的旋转
void Camera::RotateByY(float angleY)
{
	XMMATRIX R = XMMatrixRotationAxis(RightDirection, angleY);

	UpDirection = XMVector3TransformNormal(UpDirection, R);
	ViewDirection = XMVector3TransformNormal(ViewDirection, R);

	// 利用 ViewDirection 观察向量、FocalLength 焦距，更新焦点位置
	FocusPosition = EyePosition + ViewDirection * FocalLength;
}

//鼠标X方向上移动带来的旋转
void Camera::RotateByX(float angleX)
{
	XMMATRIX R = XMMatrixRotationY(angleX);

	UpDirection = XMVector3TransformNormal(UpDirection, R);
	ViewDirection = XMVector3TransformNormal(ViewDirection, R);
	RightDirection = XMVector3TransformNormal(RightDirection, R);

	// 利用 ViewDirection 观察向量、FocalLength 焦距，更新焦点位置
	FocusPosition = EyePosition + ViewDirection * FocalLength;
}

// 更新上一次的鼠标位置
void Camera::UpdateLastCursorPos()
{
	GetCursorPos(&LastCursorPoint);
}

// 当鼠标左键长按并移动时，旋转摄像机视角
void Camera::CameraRotate()
{
	POINT CurrentCursorPoint = {};
	GetCursorPos(&CurrentCursorPoint);	// 获取当前鼠标位置

	// 根据鼠标在屏幕坐标系的 x,y 轴的偏移量，计算摄像机旋转角
	float AngleX = XMConvertToRadians(0.25 * static_cast<float>(CurrentCursorPoint.x - LastCursorPoint.x));
	float AngleY = XMConvertToRadians(0.25 * static_cast<float>(CurrentCursorPoint.y - LastCursorPoint.y));

	// 旋转摄像机
	RotateByY(AngleY);
	RotateByX(AngleX);

	UpdateLastCursorPos();		// 旋转完毕，更新上一次的鼠标位置
}

void Camera::UpdateVPMatrix()
{
	ViewMatrix = XMMatrixLookAtLH(EyePosition, FocusPosition, UpDirection);
	VPMatrix = ViewMatrix * ProjectionMatrix;
}

XMMATRIX& Camera::GetVPMatrix()
{
	UpdateVPMatrix();
	return VPMatrix;
}