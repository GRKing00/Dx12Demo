#pragma once

#include"pch.h"

class Camera
{
private:

	XMVECTOR EyePosition = XMVectorSet(4, 3, 4, 1);			
	XMVECTOR FocusPosition = XMVectorSet(0, 1, 1, 1);		
	XMVECTOR UpDirection = XMVectorSet(0, 1, 0, 0);			

	XMVECTOR ViewDirection = XMVector3Normalize(FocusPosition - EyePosition);
	float FocalLength = XMVectorGetX(XMVector3Length(FocusPosition - EyePosition));
	XMVECTOR RightDirection = XMVector3Normalize(XMVector3Cross(UpDirection, ViewDirection));

	POINT LastCursorPoint = {};								

	float FovAngleY = XM_PIDIV4;							
	float AspectRatio = 4.0 / 3.0;							
	float NearZ = 0.1;										
	float FarZ = 1000;										

	XMMATRIX ViewMatrix;									
	XMMATRIX ProjectionMatrix;								

	XMMATRIX VPMatrix;		

public:
	Camera();
	void Walk(float Stride);
	void Strafe(float Stride);
	void RotateByY(float angleY);
	void RotateByX(float angleX);
	void UpdateLastCursorPos();
	void CameraRotate();
	void UpdateVPMatrix();
	XMMATRIX& GetVPMatrix();
};
