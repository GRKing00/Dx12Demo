#pragma once

#include "pch.h"

struct VERTEX
{
	XMFLOAT4 position;
	XMFLOAT2 texcoordUV;
};

inline UINT Ceil(UINT A, UINT B)
{
	return (A + B - 1) / B;
}