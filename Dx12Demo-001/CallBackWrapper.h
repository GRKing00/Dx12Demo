#pragma once

#include "pch.h"

// 用于绑定回调函数的中间层
class CallBackWrapper
{
public:

	inline static std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> Broker_Func;

	static LRESULT CALLBACK CallBackFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return Broker_Func(hwnd, msg, wParam, lParam);
	}

};