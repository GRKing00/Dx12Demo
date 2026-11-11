#pragma once

#include<Windows.h>		
#include<d3d12.h>		
#include<dxgi1_6.h>	
#include<DirectXColors.h>
#include<DirectXMath.h>
#include<d3dcompiler.h>	
#include<wincodec.h>	//使用WIC

#include<wrl.h>			//使用ComPtr
#include<string>	
#include<sstream>
#include<functional>

#pragma comment(lib,"d3d12.lib")	
#pragma comment(lib,"dxgi.lib")		
#pragma comment(lib,"dxguid.lib")	
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")

using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;