#include "DX12Engine.h"

void DX12Engine::InitWindow(HINSTANCE hins) {
	WNDCLASS wc = {};					
	wc.hInstance = hins;				
	wc.lpfnWndProc = CallBackFunc;		
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


void DX12Engine::CreateRootSignature()
{
	//-------------------根签名将资源描述符绑定到相应的 GPU 寄存器槽上，供着色器高速访问使用-------------------
	ComPtr<ID3DBlob> SignatureBlob;		
	ComPtr<ID3DBlob> ErrorBlob;			

	D3D12_ROOT_SIGNATURE_DESC rootsignatureDesc = {};	
	rootsignatureDesc.NumParameters = 0;				
	rootsignatureDesc.pParameters = nullptr;			
	rootsignatureDesc.NumStaticSamplers = 0;			
	rootsignatureDesc.pStaticSamplers = nullptr;		
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
	D3D12_INPUT_ELEMENT_DESC InputElementDesc[2] = {};

	//语义1
	InputElementDesc[0].SemanticName = "POSITION";
	InputElementDesc[0].SemanticIndex = 0;
	InputElementDesc[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	InputElementDesc[0].InputSlot = 0;
	InputElementDesc[0].AlignedByteOffset = 0;
	InputElementDesc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	InputElementDesc[0].InstanceDataStepRate = 0;

	//语义2
	InputElementDesc[1].SemanticName = "COLOR";
	InputElementDesc[1].SemanticIndex = 0;
	InputElementDesc[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	InputElementDesc[1].InputSlot = 0;
	InputElementDesc[1].AlignedByteOffset = 16;
	InputElementDesc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	InputElementDesc[1].InstanceDataStepRate = 0;

	InputLayoutDesc.NumElements = 2;
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

	// 最终创建 PSO 对象
	m_D3D12Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&m_PipelineStateObject));
}

void DX12Engine::CreateVertexResource()
{
	//CPU上顶点资源
	VERTEX vertexs[6] =
	{
		{{-0.75f, 0.75f, 0.0f, 1.0f}, XMFLOAT4(Colors::Blue)},
		{{0.75f, 0.75f, 0.0f, 1.0f}, XMFLOAT4(Colors::Yellow)},
		{{0.75f, -0.75f, 0.0f, 1.0f}, XMFLOAT4(Colors::Red)},
		{{-0.75f, 0.75f, 0.0f, 1.0f}, XMFLOAT4(Colors::Blue)},
		{{0.75f, -0.75f, 0.0f, 1.0f}, XMFLOAT4(Colors::Red)},
		{{-0.75f, -0.75f, 0.0f, 1.0f}, XMFLOAT4(Colors::Green)}
	};

	//GPU上顶点缓冲
	D3D12_RESOURCE_DESC VertexDesc = {};						
	VertexDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;		
	VertexDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;			
	VertexDesc.Width = sizeof(vertexs);							
	VertexDesc.Height = 1;										
	VertexDesc.Format = DXGI_FORMAT_UNKNOWN;					
	VertexDesc.DepthOrArraySize = 1;							
	VertexDesc.MipLevels = 1;									
	VertexDesc.SampleDesc.Count = 1;				
	//在上传堆上创建顶点缓冲
	D3D12_HEAP_PROPERTIES UploadHeapDesc = { D3D12_HEAP_TYPE_UPLOAD };
	m_D3D12Device->CreateCommittedResource(&UploadHeapDesc, D3D12_HEAP_FLAG_NONE,
		&VertexDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_VertexResource));

	//顶点缓冲映射到一个地址，然后拷贝CPU上的顶点数据到缓冲里面
	BYTE* TransferPointer = nullptr;
	m_VertexResource->Map(0, nullptr, reinterpret_cast<void**>(&TransferPointer));
	memcpy(TransferPointer, vertexs, sizeof(vertexs));
	m_VertexResource->Unmap(0, nullptr);

	//创建顶点缓冲描述符
	VertexBufferView.BufferLocation = m_VertexResource->GetGPUVirtualAddress();		
	VertexBufferView.SizeInBytes = sizeof(vertexs);									
	VertexBufferView.StrideInBytes = sizeof(VERTEX);								

}



void DX12Engine::Render()
{
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

	// 用 RTV 句柄设置渲染目标
	RTVHandle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();
	RTVHandle.ptr += FrameIndex * RTVDescriptorSize;//获取渲染目标的RTV
	m_CommandList->OMSetRenderTargets(1, &RTVHandle, false, nullptr);

	// 清屏
	m_CommandList->ClearRenderTargetView(RTVHandle, DirectX::Colors::SkyBlue, 0, nullptr);

	m_CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	m_CommandList->DrawInstanced(6, 1, 0, 0);

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

