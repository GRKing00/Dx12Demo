#pragma once
#include<Windows.h>		
#include<d3d12.h>		
#include<dxgi1_6.h>	
#include<DirectXColors.h>
#include<DirectXMath.h>
#include<d3dcompiler.h>	

#include<wrl.h>			//使用ComPtr

#pragma comment(lib,"d3d12.lib")	
#pragma comment(lib,"dxgi.lib")		
#pragma comment(lib,"dxguid.lib")	
#pragma comment(lib,"d3dcompiler.lib")

using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

// DX12 引擎
class DX12Engine
{
private:

	int WindowWidth = 640;		// 窗口宽度
	int WindowHeight = 480;		// 窗口高度
	HWND m_hwnd;				// 窗口句柄

	ComPtr<ID3D12Debug> m_D3D12DebugDevice;				
	UINT m_DXGICreateFactoryFlag = NULL;				

	ComPtr<IDXGIFactory5> m_DXGIFactory;				
	ComPtr<IDXGIAdapter1> m_DXGIAdapter;				
	ComPtr<ID3D12Device4> m_D3D12Device;				

	ComPtr<ID3D12CommandQueue> m_CommandQueue;			
	ComPtr<ID3D12CommandAllocator> m_CommandAllocator;	
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;	

	ComPtr<IDXGISwapChain3> m_DXGISwapChain;	
	ComPtr<ID3D12Resource> m_RenderTarget[3];
	ComPtr<ID3D12DescriptorHeap> m_RTVHeap;				
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle;				
	UINT RTVDescriptorSize = 0;							
	UINT FrameIndex = 0;	

	ComPtr<ID3D12Fence> m_Fence;						
	UINT64 FenceValue = 0;								
	HANDLE RenderEvent = NULL;							
	D3D12_RESOURCE_BARRIER beg_barrier = {};		
	D3D12_RESOURCE_BARRIER end_barrier = {};			

	ComPtr<ID3D12RootSignature> m_RootSignature;		
	ComPtr<ID3D12PipelineState> m_PipelineStateObject;

	ComPtr<ID3D12Resource> m_VertexResource;			
	struct VERTEX										
	{
		XMFLOAT4 position;								
		XMFLOAT4 color;									
	};
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;			

	D3D12_VIEWPORT viewPort = D3D12_VIEWPORT{ 0, 0, float(WindowWidth), float(WindowHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT ScissorRect = D3D12_RECT{ 0, 0, WindowWidth, WindowHeight };

public:
	static LRESULT CALLBACK CallBackFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default: return DefWindowProc(hwnd, msg, wParam, lParam);
		}

		return 0;
	}

	void InitWindow(HINSTANCE hins);
	bool InitDX12();

	void CreateRootSignature();
	void CreatePSO();
	void CreateVertexResource();

	void Render();
	void RenderLoop();

	static void Run(HINSTANCE hins)
	{
		DX12Engine engine;
		engine.InitWindow(hins);
		if (!engine.InitDX12())
			return ;

		engine.CreateRootSignature();
		engine.CreatePSO();
		engine.CreateVertexResource();

		engine.RenderLoop();
	}
};