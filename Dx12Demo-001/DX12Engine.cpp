#include "DX12Engine.h"

#pragma region 私有函数

LRESULT CALLBACK DX12Engine::CallBackFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;

	case WM_CHAR:	
	{
		switch (wParam)		// wParam 是按键对应的字符 ASCII 码
		{
		case 'w':
		case 'W':	// 向前移动
			m_FirstCamera.Walk(0.1);
			break;

		case 's':
		case 'S':	// 向后移动
			m_FirstCamera.Walk(-0.1);
			break;

		case 'a':
		case 'A':	// 向左移动
			m_FirstCamera.Strafe(-0.1);
			break;

		case 'd':
		case 'D':	// 向右移动
			m_FirstCamera.Strafe(0.1);
			break;
		}
	}
	break;

	case WM_MOUSEMOVE:	// 获取鼠标移动消息
	{
		switch (wParam)	// wParam 是鼠标按键的状态
		{
		case MK_LBUTTON:	// 当用户长按鼠标左键的同时移动鼠标，摄像机旋转
			m_FirstCamera.CameraRotate();
			break;

			// 按键没按，鼠标只是移动也要更新，否则就会发生摄像机视角瞬移
		default: m_FirstCamera.UpdateLastCursorPos();
		}
	}
	break;

	default: return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

#pragma region 模型纹理资源加载 

void DX12Engine::CreateSRVHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC SRVHeapDesc = {};
	SRVHeapDesc.NumDescriptors = m_ModelManager.Texture_SRV_Map.size();;
	SRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	SRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// 创建 SRV 描述符堆
	m_D3D12Device->CreateDescriptorHeap(&SRVHeapDesc, IID_PPV_ARGS(&m_SRVHeap));
}

//开始记录命令
void DX12Engine::StartCommandRecord()
{
	m_CommandAllocator->Reset();
	m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);
}

bool DX12Engine::LoadTextureFromFile(std::wstring TextureFilename)
{
	// 如果还没创建 WIC 工厂
	if (m_WICFactory == nullptr) CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_WICFactory));

	//读取图片创建位图解码器
	HRESULT hr = m_WICFactory->CreateDecoderFromFilename(TextureFilename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &m_WICBitmapDecoder);
	std::wostringstream output_str;
	switch (hr)
	{
	case S_OK: break;	// 解码成功，直接 break 进入下一步即可

	case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):	// 文件找不到
		output_str << L"找不到文件 " << TextureFilename << L" ！请检查文件路径是否有误！";
		MessageBox(NULL, output_str.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
		return false;

	case HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT):	// 文件句柄正在被另一个应用进程占用
		output_str << L"文件 " << TextureFilename << L" 已经被另一个应用进程打开并占用了！请先关闭那个应用进程！";
		MessageBox(NULL, output_str.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
		return false;

	case WINCODEC_ERR_COMPONENTNOTFOUND:			// 找不到可解码的组件，说明这不是有效的图像文件
		output_str << L"文件 " << TextureFilename << L" 不是有效的图像文件，无法解码！请检查文件是否为图像文件！";
		MessageBox(NULL, output_str.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
		return false;

	default:			// 发生其他未知错误
		output_str << L"文件 " << TextureFilename << L" 解码失败！发生了其他错误，错误码：" << hr << L" ，请查阅微软官方文档。";
		MessageBox(NULL, output_str.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
		return false;
	}

	//获取图片数据的第一帧
	m_WICBitmapDecoder->GetFrame(0, &m_WICBitmapDecodeFrame);

	//将源图格式转换成DX12支持的标准位图格式，在转换为DXGI格式
	WICPixelFormatGUID SourceFormat = {};
	GUID TargetFormat = {};
	m_WICBitmapDecodeFrame->GetPixelFormat(&SourceFormat);						// 获取源图格式
	if (DX12TextureHelper::GetTargetPixelFormat(&SourceFormat, &TargetFormat))	// 获取目标格式
	{
		TextureFormat = DX12TextureHelper::GetDXGIFormatFromPixelFormat(&TargetFormat);	// 获取 DX12 支持的格式
	}
	else	// 如果没有可支持的目标格式
	{
		::MessageBox(NULL, L"此纹理不受支持!", L"提示", MB_OK);
		return false;
	}

	//创建位图转换器将图片转为目标格式
	m_WICFactory->CreateFormatConverter(&m_WICFormatConverter);		// 创建图片转换器
	m_WICFormatConverter->Initialize(m_WICBitmapDecodeFrame.Get(), TargetFormat, WICBitmapDitherTypeNone,
		nullptr, 0.0f, WICBitmapPaletteTypeCustom);

	// 将位图数据继承到 WIC 位图资源，使用这个位图资源获取图片信息
	m_WICFormatConverter.As(&m_WICBitmapSource);
	m_WICBitmapSource->GetSize(&TextureWidth, &TextureHeight);		// 获取纹理宽高

	//获取BitsPerPixel
	ComPtr<IWICComponentInfo> _temp_WICComponentInfo = {};
	ComPtr<IWICPixelFormatInfo> _temp_WICPixelInfo = {};
	m_WICFactory->CreateComponentInfo(TargetFormat, &_temp_WICComponentInfo);
	_temp_WICComponentInfo.As(&_temp_WICPixelInfo);
	_temp_WICPixelInfo->GetBitsPerPixel(&BitsPerPixel);				// 获取 BitsPerPixel 图像深度

	return true;
}

void DX12Engine::CreateUploadAndDefaultResource(ModelManager::TEXTURE_MAP_INFO& Info)
{
	BytePerRowSize = TextureWidth * BitsPerPixel / 8;
	TextureSize = BytePerRowSize * TextureHeight;
	UploadResourceRowSize = Ceil(BytePerRowSize, 256) * 256;

	//上传资源大小，最后一行不用对齐
	UploadResourceSize = UploadResourceRowSize * (TextureHeight - 1) + BytePerRowSize;

	// 创建上传堆资源
	D3D12_RESOURCE_DESC UploadResourceDesc = {};
	UploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	UploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	UploadResourceDesc.Width = UploadResourceSize;
	UploadResourceDesc.Height = 1;
	UploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	UploadResourceDesc.DepthOrArraySize = 1;
	UploadResourceDesc.MipLevels = 1;
	UploadResourceDesc.SampleDesc.Count = 1;
	D3D12_HEAP_PROPERTIES UploadHeapDesc = { D3D12_HEAP_TYPE_UPLOAD };
	m_D3D12Device->CreateCommittedResource(&UploadHeapDesc, D3D12_HEAP_FLAG_NONE, &UploadResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Info.UploadHeapTextureResource));

	// 创建默认堆资源
	D3D12_RESOURCE_DESC DefaultResourceDesc = {};
	DefaultResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DefaultResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DefaultResourceDesc.Width = TextureWidth;
	DefaultResourceDesc.Height = TextureHeight;
	DefaultResourceDesc.Format = TextureFormat;
	DefaultResourceDesc.DepthOrArraySize = 1;
	DefaultResourceDesc.MipLevels = 1;
	DefaultResourceDesc.SampleDesc.Count = 1;
	D3D12_HEAP_PROPERTIES DefaultHeapDesc = { D3D12_HEAP_TYPE_DEFAULT };
	m_D3D12Device->CreateCommittedResource(&DefaultHeapDesc, D3D12_HEAP_FLAG_NONE, &DefaultResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&Info.DefaultHeapTextureResource));
}

void DX12Engine::CopyTextureDataToDefaultResource(ModelManager::TEXTURE_MAP_INFO& Info)
{
	//在CPU上创建纹理资源
	BYTE* TextureData = (BYTE*)malloc(TextureSize);
	m_WICBitmapSource->CopyPixels(nullptr, BytePerRowSize, TextureSize, TextureData);

	//将上传堆映射到CPU地址，之后用于将数据拷贝到上传堆
	BYTE* TransferPointer = nullptr;
	Info.UploadHeapTextureResource->Map(0, nullptr, reinterpret_cast<void**>(&TransferPointer));

	// 逐行复制数据！注意两个指针偏移的长度不同！
	for (UINT i = 0; i < TextureHeight; i++)
	{
		memcpy(TransferPointer, TextureData, BytePerRowSize);
		TextureData += BytePerRowSize;
		TransferPointer += UploadResourceRowSize;
	}
	//拷贝结束，释放CPU上的数据
	Info.UploadHeapTextureResource->Unmap(0, nullptr);
	TextureData -= TextureSize;
	free(TextureData);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedFootprint = {};
	D3D12_RESOURCE_DESC DefaultResourceDesc = Info.DefaultHeapTextureResource->GetDesc();

	// 获取纹理复制脚本，用于下文的纹理复制
	m_D3D12Device->GetCopyableFootprints(&DefaultResourceDesc, 0, 1, 0, &PlacedFootprint, nullptr, nullptr, nullptr);

	//复制目标
	D3D12_TEXTURE_COPY_LOCATION DstLocation = {};
	DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	DstLocation.SubresourceIndex = 0;
	DstLocation.pResource = Info.DefaultHeapTextureResource.Get();

	//复制源
	D3D12_TEXTURE_COPY_LOCATION SrcLocation = {};
	SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	SrcLocation.PlacedFootprint = PlacedFootprint;
	SrcLocation.pResource = Info.UploadHeapTextureResource.Get();

	//记录复制命令
	m_CommandList->CopyTextureRegion(&DstLocation, 0, 0, 0, &SrcLocation, nullptr);

}

void DX12Engine::CreateSRV(ModelManager::TEXTURE_MAP_INFO& Info,
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle)
{
	//创建着色器资源描述符
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDescriptorDesc = {};
	SRVDescriptorDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDescriptorDesc.Format = TextureFormat;
	SRVDescriptorDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDescriptorDesc.Texture2D.MipLevels = 1;

	m_D3D12Device->CreateShaderResourceView(Info.DefaultHeapTextureResource.Get(), &SRVDescriptorDesc, CPUHandle);
	Info.CPUHandle = CPUHandle;
	Info.GPUHandle = GPUHandle;
}

//开始执行
void DX12Engine::StartCommandExecute()
{
	m_CommandList->Close();

	ID3D12CommandList* _temp_cmdlists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(1, _temp_cmdlists);

	FenceValue++;
	m_CommandQueue->Signal(m_Fence.Get(), FenceValue);
	m_Fence->SetEventOnCompletion(FenceValue, RenderEvent);
}

#pragma endregion

#pragma endregion

void DX12Engine::InitWindow(HINSTANCE hins) {
	WNDCLASS wc = {};					
	wc.hInstance = hins;				

	CallBackWrapper::Broker_Func = std::bind(&DX12Engine::CallBackFunc, this,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

	wc.lpfnWndProc = CallBackWrapper::CallBackFunc;
	wc.lpszClassName = L"DX12 Game";	

	//注册窗口类
	RegisterClass(&wc);					

	//创建窗口
	m_hwnd = CreateWindow(wc.lpszClassName, L"DX12Demo", WS_SYSMENU | WS_OVERLAPPED,
		10, 10, WindowWidth, WindowHeight,
		NULL, NULL, hins, NULL);

	//展示窗口
	ShowWindow(m_hwnd, SW_SHOW);
}

bool DX12Engine::InitDX12()
{
	::CoInitialize(nullptr);	

#if defined(_DEBUG)		
	//------------------- 创建调试层 -------------------
	D3D12GetDebugInterface(IID_PPV_ARGS(&m_D3D12DebugDevice));
	m_D3D12DebugDevice->EnableDebugLayer();
	m_DXGICreateFactoryFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif

	//------------------- 创建 DXGI 工厂 -------------------
	CreateDXGIFactory2(m_DXGICreateFactoryFlag, IID_PPV_ARGS(&m_DXGIFactory));

	const D3D_FEATURE_LEVEL dx12SupportLevel[] =
	{
		D3D_FEATURE_LEVEL_12_2,		// 12.2
		D3D_FEATURE_LEVEL_12_1,		// 12.1
		D3D_FEATURE_LEVEL_12_0,		// 12.0
		D3D_FEATURE_LEVEL_11_1,		// 11.1
		D3D_FEATURE_LEVEL_11_0		// 11.0
	};

	//-------------------枚举适配器，并使用支持dx12的适配器创建device-------------------
	bool succeeded = false;
	for (UINT i = 0; m_DXGIFactory->EnumAdapters1(i, &m_DXGIAdapter) != ERROR_NOT_FOUND; i++)
	{
		for (const auto& level : dx12SupportLevel)
		{
			if (SUCCEEDED(D3D12CreateDevice(m_DXGIAdapter.Get(), level, IID_PPV_ARGS(&m_D3D12Device))))
			{
				DXGI_ADAPTER_DESC1 adap = {};
				m_DXGIAdapter->GetDesc1(&adap);
				OutputDebugStringW(L"当前使用的显卡：");
				OutputDebugStringW(adap.Description);
				OutputDebugStringW(L"\n");
				succeeded = true;
				break;
			}
		}
		if (succeeded)
			break;
	}

	if (m_D3D12Device == nullptr)
	{
		MessageBox(NULL, L"找不到任何能支持 DX12 的显卡，请升级电脑上的硬件！", L"错误", MB_OK | MB_ICONERROR);
		return false;
	}

	// ------------------- 创建命令队列 -------------------
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	m_D3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue));

	// ------------------- 创建命令分配器 -------------------
	m_D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator));

	// ------------------- 创建命令列表，注意命令类型要一致 -------------------
	m_D3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(),
		nullptr, IID_PPV_ARGS(&m_CommandList));
	m_CommandList->Close();

	// ------------------- 创建 DXGI 交换链，用于将窗口缓冲区和渲染目标绑定 -------------------
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.BufferCount = 3;								
	swapchainDesc.Width = WindowWidth;							
	swapchainDesc.Height = WindowHeight;						
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.SampleDesc.Count = 1;							
	ComPtr<IDXGISwapChain1> _temp_swapchain;
	m_DXGIFactory->CreateSwapChainForHwnd(m_CommandQueue.Get(), m_hwnd,
		&swapchainDesc, nullptr, nullptr, &_temp_swapchain);
	// 通过 As 方法，将低版本接口的信息传递给高版本接口
	_temp_swapchain.As(&m_DXGISwapChain);

	// ------------------- 创建 RTV 描述符堆 -------------------
	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
	RTVHeapDesc.NumDescriptors = 3;
	RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	m_D3D12Device->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&m_RTVHeap));

	RTVHandle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
	RTVDescriptorSize = m_D3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//-------------------给窗口缓冲创建描述符-------------------
	for (UINT i = 0; i < 3; i++)
	{
		m_DXGISwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTarget[i]));
		m_D3D12Device->CreateRenderTargetView(m_RenderTarget[i].Get(), nullptr, RTVHandle);
		RTVHandle.ptr += RTVDescriptorSize;
	}

	D3D12_RESOURCE_DESC DSVResourceDesc = {};							
	DSVResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;		
	DSVResourceDesc.Format = DSVFormat;									
	DSVResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;				
	DSVResourceDesc.Width = WindowWidth;								
	DSVResourceDesc.Height = WindowHeight;								
	DSVResourceDesc.MipLevels = 1;										
	DSVResourceDesc.DepthOrArraySize = 1;								
	DSVResourceDesc.SampleDesc.Count = 1;								
	DSVResourceDesc.SampleDesc.Quality = 0;								
	DSVResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE DepthStencilBufferClearValue = {};				
	DepthStencilBufferClearValue.DepthStencil.Depth = 1.0f;				
	DepthStencilBufferClearValue.DepthStencil.Stencil = 0;				
	DepthStencilBufferClearValue.Format = DSVFormat;					

	//------------------- 创建深度模板缓冲-------------------
	D3D12_HEAP_PROPERTIES DefaultProperties = { D3D12_HEAP_TYPE_DEFAULT };
	m_D3D12Device->CreateCommittedResource(&DefaultProperties, D3D12_HEAP_FLAG_NONE, &DSVResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &DepthStencilBufferClearValue, IID_PPV_ARGS(&m_DepthStencilBuffer));

	D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc = {};		
	DSVHeapDesc.NumDescriptors = 1;						
	DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;	
	//------------------- 创建DSVHeap-------------------
	m_D3D12Device->CreateDescriptorHeap(&DSVHeapDesc, IID_PPV_ARGS(&m_DSVHeap));
	DSVHandle = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_DEPTH_STENCIL_VIEW_DESC DSVViewDesc = {};
	DSVViewDesc.Format = DSVFormat;								
	DSVViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	
	DSVViewDesc.Flags = D3D12_DSV_FLAG_NONE;					
	//------------------- 创建DSV-------------------
	m_D3D12Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), &DSVViewDesc, DSVHandle);

	//------------------- 创建 CPU 上的等待事件 -------------------
	RenderEvent = CreateEvent(nullptr, false, true, nullptr);

	// -------------------创建围栏，设定初始值为 0 -------------------
	m_D3D12Device->CreateFence(FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));

	// -------------------设置资源屏障 -------------------
	// beg_barrier 起始屏障：Present 呈现状态 -> Render Target 渲染目标状态
	beg_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;					// 指定类型为转换屏障		
	beg_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	beg_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	// end_barrier 终止屏障：Render Target 渲染目标状态 -> Present 呈现状态
	end_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	end_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	end_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	return true;
}

// 读取并创建纹理资源
void DX12Engine::CreateModelTextureResource()
{
	//统一的SRV堆
	CreateSRVHeap();	

	D3D12_CPU_DESCRIPTOR_HANDLE CurrentCPUHandle = m_SRVHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE CurrentGPUHandle = m_SRVHeap->GetGPUDescriptorHandleForHeapStart();

	UINT SRVDescriptorSize = m_D3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	StartCommandRecord();	// 启动命令列表，开始录制命令

	// 对纹理映射表进行遍历
	for (auto& CurrentElem : m_ModelManager.Texture_SRV_Map)
	{
		//加载各自纹理，创建各自资源，在堆上指定位置创建SRV
		LoadTextureFromFile(CurrentElem.second.TextureFilePath);
		CreateUploadAndDefaultResource(CurrentElem.second);
		CopyTextureDataToDefaultResource(CurrentElem.second);
		CreateSRV(CurrentElem.second, CurrentCPUHandle, CurrentGPUHandle);

		CurrentCPUHandle.ptr += SRVDescriptorSize;
		CurrentGPUHandle.ptr += SRVDescriptorSize;
	}

	StartCommandExecute();	// 关闭命令列表，交给命令队列执行
}

// 创建模型顶点与索引资源
void DX12Engine::CreateModelVertexAndIndexResource()
{
	m_ModelManager.CreateBlock();
	m_ModelManager.CreateModelResource(m_D3D12Device);
}

void DX12Engine::CreateCBVResource()
{
	UINT CBufferWidth = Ceil(sizeof(CBuffer), 256) * 256;

	D3D12_RESOURCE_DESC CBVResourceDesc = {};						
	CBVResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;	
	CBVResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;		
	CBVResourceDesc.Width = CBufferWidth;							
	CBVResourceDesc.Height = 1;										
	CBVResourceDesc.Format = DXGI_FORMAT_UNKNOWN;					
	CBVResourceDesc.DepthOrArraySize = 1;							
	CBVResourceDesc.MipLevels = 1;									
	CBVResourceDesc.SampleDesc.Count = 1;							

	// 创建常量缓冲资源
	D3D12_HEAP_PROPERTIES UploadHeapDesc = { D3D12_HEAP_TYPE_UPLOAD };
	m_D3D12Device->CreateCommittedResource(&UploadHeapDesc, D3D12_HEAP_FLAG_NONE, &CBVResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_CBVResource));

	//映射到CPU内存地址上
	m_CBVResource->Map(0, nullptr, reinterpret_cast<void**>(&VPBuffer));
}

void DX12Engine::CreateRootSignature()
{
	//-------------------根签名将资源描述符绑定到相应的 GPU 寄存器槽上，供着色器高速访问使用-------------------
	ComPtr<ID3DBlob> SignatureBlob;		
	ComPtr<ID3DBlob> ErrorBlob;		

	//根参数
	D3D12_ROOT_PARAMETER RootParameters[2] = {};

	//根描述符
	D3D12_ROOT_DESCRIPTOR CBVRootDescriptorDesc = {};
	CBVRootDescriptorDesc.ShaderRegister = 0;
	CBVRootDescriptorDesc.RegisterSpace = 0;

	RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;	// 常量缓冲对整个渲染管线都可见
	RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	RootParameters[0].Descriptor = CBVRootDescriptorDesc;

	//描述符Range
	D3D12_DESCRIPTOR_RANGE SRVDescriptorRangeDesc = {};						
	SRVDescriptorRangeDesc.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;		
	SRVDescriptorRangeDesc.NumDescriptors = 1;								
	SRVDescriptorRangeDesc.BaseShaderRegister = 0;							
	SRVDescriptorRangeDesc.RegisterSpace = 0;							
	SRVDescriptorRangeDesc.OffsetInDescriptorsFromTableStart = 0;			

	//根描述符表
	D3D12_ROOT_DESCRIPTOR_TABLE RootDescriptorTableDesc = {};				
	RootDescriptorTableDesc.pDescriptorRanges = &SRVDescriptorRangeDesc;	
	RootDescriptorTableDesc.NumDescriptorRanges = 1;		

	RootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	RootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	RootParameters[1].DescriptorTable = RootDescriptorTableDesc;

	

	//静态采样器
	D3D12_STATIC_SAMPLER_DESC StaticSamplerDesc = {};						
	StaticSamplerDesc.ShaderRegister = 0;									
	StaticSamplerDesc.RegisterSpace = 0;									
	StaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;		
	StaticSamplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;	
	StaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;			
	StaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;			
	StaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;			
	StaticSamplerDesc.MinLOD = 0;											
	StaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;							
	StaticSamplerDesc.MipLODBias = 0;										
	StaticSamplerDesc.MaxAnisotropy = 1;								
	StaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;			


	D3D12_ROOT_SIGNATURE_DESC rootsignatureDesc = {};	
	rootsignatureDesc.NumParameters = 2;				
	rootsignatureDesc.pParameters = RootParameters;
	rootsignatureDesc.NumStaticSamplers = 1;			
	rootsignatureDesc.pStaticSamplers = &StaticSamplerDesc;
	rootsignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//将根签名编译成二进制字节码
	D3D12SerializeRootSignature(&rootsignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &SignatureBlob, &ErrorBlob);
	if (ErrorBlob)		
	{
		OutputDebugStringA((const char*)ErrorBlob->GetBufferPointer());
		OutputDebugStringA("\n");
	}

	//创建根签名对象
	m_D3D12Device->CreateRootSignature(0, SignatureBlob->GetBufferPointer(), SignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature));
}

void DX12Engine::CreatePSO()
{
	//使用PSO来配置渲染管线
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};

	// 配置输入布局，IA阶段如何读取顶点数据
	D3D12_INPUT_LAYOUT_DESC InputLayoutDesc = {};
	D3D12_INPUT_ELEMENT_DESC InputElementDesc[6] = {};

	//语义1
	InputElementDesc[0].SemanticName = "POSITION";
	InputElementDesc[0].SemanticIndex = 0;
	InputElementDesc[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	InputElementDesc[0].InputSlot = 0;
	InputElementDesc[0].AlignedByteOffset = 0;
	InputElementDesc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	InputElementDesc[0].InstanceDataStepRate = 0;

	//语义2
	InputElementDesc[1].SemanticName = "TEXCOORD";
	InputElementDesc[1].SemanticIndex = 0;
	InputElementDesc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	InputElementDesc[1].InputSlot = 0;
	InputElementDesc[1].AlignedByteOffset = 16;
	InputElementDesc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	InputElementDesc[1].InstanceDataStepRate = 0;

	//语义3
	InputElementDesc[2].SemanticName = "MATRIX";
	InputElementDesc[2].SemanticIndex = 0;
	InputElementDesc[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	InputElementDesc[2].InputSlot = 1;
	InputElementDesc[2].AlignedByteOffset = 0;
	InputElementDesc[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	InputElementDesc[2].InstanceDataStepRate = 0;

	InputElementDesc[3].SemanticName = "MATRIX";
	InputElementDesc[3].SemanticIndex = 1;
	InputElementDesc[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	InputElementDesc[3].InputSlot = 1;
	InputElementDesc[3].AlignedByteOffset = 16;
	InputElementDesc[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	InputElementDesc[3].InstanceDataStepRate = 0;

	InputElementDesc[4].SemanticName = "MATRIX";
	InputElementDesc[4].SemanticIndex = 2;
	InputElementDesc[4].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	InputElementDesc[4].InputSlot = 1;
	InputElementDesc[4].AlignedByteOffset = 32;
	InputElementDesc[4].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	InputElementDesc[4].InstanceDataStepRate = 0;

	InputElementDesc[5].SemanticName = "MATRIX";
	InputElementDesc[5].SemanticIndex = 3;
	InputElementDesc[5].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	InputElementDesc[5].InputSlot = 1;
	InputElementDesc[5].AlignedByteOffset = 48;
	InputElementDesc[5].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	InputElementDesc[5].InstanceDataStepRate = 0;

	InputLayoutDesc.NumElements = 6;
	InputLayoutDesc.pInputElementDescs = InputElementDesc;
	PSODesc.InputLayout = InputLayoutDesc;					// 设置渲染管线 IA 阶段的输入样式

	ComPtr<ID3DBlob> VertexShaderBlob;
	ComPtr<ID3DBlob> PixelShaderBlob;
	ComPtr<ID3DBlob> ErrorBlob;

	// 编译顶点着色器 Vertex Shader
	D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_1", NULL, NULL, &VertexShaderBlob, &ErrorBlob);
	if (ErrorBlob)
	{
		OutputDebugStringA((const char*)ErrorBlob->GetBufferPointer());
		OutputDebugStringA("\n");
	}

	// 编译像素着色器 Pixel Shader
	D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_1", NULL, NULL, &PixelShaderBlob, &ErrorBlob);
	if (ErrorBlob)
	{
		OutputDebugStringA((const char*)ErrorBlob->GetBufferPointer());
		OutputDebugStringA("\n");
	}

	PSODesc.VS.pShaderBytecode = VertexShaderBlob->GetBufferPointer();
	PSODesc.VS.BytecodeLength = VertexShaderBlob->GetBufferSize();
	PSODesc.PS.pShaderBytecode = PixelShaderBlob->GetBufferPointer();
	PSODesc.PS.BytecodeLength = PixelShaderBlob->GetBufferSize();

	// Rasterizer 光栅化
	PSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;	
	PSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;	

	//其他配置
	PSODesc.pRootSignature = m_RootSignature.Get();
	PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	PSODesc.NumRenderTargets = 1;
	PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	PSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	PSODesc.SampleDesc.Count = 1;
	PSODesc.SampleMask = UINT_MAX;

	// 设置深度测试状态
	PSODesc.DSVFormat = DSVFormat;											
	PSODesc.DepthStencilState.DepthEnable = true;							
	PSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;		
	PSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;	

	// 最终创建 PSO 对象
	m_D3D12Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&m_PipelineStateObject));
}

void DX12Engine::UpdateConstantBuffer()
{
	XMStoreFloat4x4(&VPBuffer->VPMatrix, m_FirstCamera.GetVPMatrix());
}

void DX12Engine::Render()
{
	UpdateConstantBuffer();

	m_CommandAllocator->Reset();
	m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);

	//资源屏障
	FrameIndex = m_DXGISwapChain->GetCurrentBackBufferIndex();
	beg_barrier.Transition.pResource = m_RenderTarget[FrameIndex].Get();
	m_CommandList->ResourceBarrier(1, &beg_barrier);

	m_CommandList->SetPipelineState(m_PipelineStateObject.Get());
	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
	m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_CommandList->RSSetViewports(1, &viewPort);
	m_CommandList->RSSetScissorRects(1, &ScissorRect);

	//设置描述符堆，和根参数
	ID3D12DescriptorHeap* _temp_DescriptorHeaps[] = { m_SRVHeap.Get() };
	m_CommandList->SetDescriptorHeaps(1, _temp_DescriptorHeaps);
	m_CommandList->SetGraphicsRootConstantBufferView(0, m_CBVResource->GetGPUVirtualAddress());


	// 用 RTV、DSV 句柄设置渲染目标
	RTVHandle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
	RTVHandle.ptr += FrameIndex * RTVDescriptorSize;//获取渲染目标的RTV
	m_CommandList->OMSetRenderTargets(1, &RTVHandle, false, &DSVHandle);

	// 清屏
	m_CommandList->ClearDepthStencilView(DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
	m_CommandList->ClearRenderTargetView(RTVHandle, DirectX::Colors::SkyBlue, 0, nullptr);

	m_ModelManager.RenderAllModel(m_CommandList);

	//资源屏障
	end_barrier.Transition.pResource = m_RenderTarget[FrameIndex].Get();
	m_CommandList->ResourceBarrier(1, &end_barrier);

	//执行命令
	m_CommandList->Close();
	ID3D12CommandList* _temp_cmdlists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(1, _temp_cmdlists);

	//呈现
	m_DXGISwapChain->Present(1, NULL);

	//围栏标记，以及到达标记时的响应事件
	FenceValue++;
	m_CommandQueue->Signal(m_Fence.Get(), FenceValue);
	m_Fence->SetEventOnCompletion(FenceValue, RenderEvent);
}

void DX12Engine::RenderLoop()
{
	bool isExit = false;	
	MSG msg = {};			

	while (isExit != true)
	{
		//获取消息
		DWORD ActiveEvent = ::MsgWaitForMultipleObjects(1, &RenderEvent, false, INFINITE, QS_ALLINPUT);

		switch (ActiveEvent - WAIT_OBJECT_0)
		{
		case 0:				
			Render();//执行渲染操作
			break;

		case 1:				
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message != WM_QUIT)
				{
					TranslateMessage(&msg);					
					DispatchMessage(&msg);					
				}
				else
				{
					isExit = true;						
				}
			}
			break;

		case WAIT_TIMEOUT:	// 渲染超时
			break;
		}
	}
}


void DX12Engine::Run(HINSTANCE hins)
{
	DX12Engine engine;
	engine.InitWindow(hins);
	if (!engine.InitDX12())
		return;

	engine.CreateModelTextureResource();
	engine.CreateModelVertexAndIndexResource();

	engine.CreateCBVResource();

	engine.CreateRootSignature();
	engine.CreatePSO();

	engine.RenderLoop();
}