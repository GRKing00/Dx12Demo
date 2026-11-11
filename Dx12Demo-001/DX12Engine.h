#pragma once
#include "pch.h"
#include "Utils.h"
#include "CallBackWrapper.h"
#include "Camera.h"
#include "DX12TextureHelper.h"
#include "Model.h"


// DX12 引擎
class DX12Engine
{
private:

	int WindowWidth = 640;		// 窗口宽度
	int WindowHeight = 480;		// 窗口高度
	HWND m_hwnd;				// 窗口句柄

	ComPtr<ID3D12Debug> m_D3D12DebugDevice;				
	UINT m_DXGICreateFactoryFlag = NULL;		
	UINT m_D3DShaderCompileFlag = NULL;

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

	ComPtr<ID3D12DescriptorHeap> m_DSVHeap;					
	D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle;					
	ComPtr<ID3D12Resource> m_DepthStencilBuffer;		
	DXGI_FORMAT DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ModelManager m_ModelManager;

	ComPtr<IWICImagingFactory> m_WICFactory;				
	ComPtr<IWICBitmapDecoder> m_WICBitmapDecoder;			
	ComPtr<IWICBitmapFrameDecode> m_WICBitmapDecodeFrame;	
	ComPtr<IWICFormatConverter> m_WICFormatConverter;		
	ComPtr<IWICBitmapSource> m_WICBitmapSource;				
	UINT TextureWidth = 0;									
	UINT TextureHeight = 0;									
	UINT BitsPerPixel = 0;									
	UINT BytePerRowSize = 0;								
	DXGI_FORMAT TextureFormat = DXGI_FORMAT_UNKNOWN;		

	ComPtr<ID3D12DescriptorHeap> m_SRVHeap;					
	
	UINT TextureSize = 0;									
	UINT UploadResourceRowSize = 0;							
	UINT UploadResourceSize = 0;		

	ComPtr<ID3D12Resource> m_CBVResource;		
	struct CBuffer								
	{
		XMFLOAT4X4 VPMatrix;	
	};
	CBuffer* VPBuffer = nullptr;

	Camera m_FirstCamera;

	ComPtr<ID3D12RootSignature> m_RootSignature;

	D3D12_INPUT_LAYOUT_DESC InputLayoutDesc = {};
	D3D12_INPUT_ELEMENT_DESC InputElementDesc[6] = {};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC OpaquePSODesc = {};		
	ComPtr<ID3D12PipelineState> m_OpaquePSO;					
	D3D12_GRAPHICS_PIPELINE_STATE_DESC TransparentPSODesc = {};	
	ComPtr<ID3D12PipelineState> m_TransparentPSO;				
	D3D12_GRAPHICS_PIPELINE_STATE_DESC TranslucencePSODesc = {};
	ComPtr<ID3D12PipelineState> m_TranslucencePSO;				

	D3D12_VIEWPORT viewPort = D3D12_VIEWPORT{ 0, 0, float(WindowWidth), float(WindowHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT ScissorRect = D3D12_RECT{ 0, 0, WindowWidth, WindowHeight };

private:
	LRESULT CALLBACK CallBackFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void CreateSRVHeap();
	void StartCommandRecord();
	bool LoadTextureFromFile(std::wstring TextureFilename);
	void CreateUploadAndDefaultResource(ModelManager::TEXTURE_MAP_INFO& Info);
	void CopyTextureDataToDefaultResource(ModelManager::TEXTURE_MAP_INFO& Info);
	void CreateSRV(ModelManager::TEXTURE_MAP_INFO& Info,
		D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle);
	void StartCommandExecute();

public:

	void InitWindow(HINSTANCE hins);
	bool InitDX12();


	void CreateModelTextureResource();
	void CreateModelVertexAndIndexResource();

	void CreateCBVResource();

	void CreateRootSignature();
	void CreateInputLayout();
	void CreateOpaquePSO();
	void CreateTransparentPSO();
	void CreateTranslucencePSO();


	void UpdateConstantBuffer();
	void Render();
	void RenderLoop();

	static void Run(HINSTANCE hins);
};