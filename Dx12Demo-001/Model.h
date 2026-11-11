#pragma once

#include "pch.h"
#include "Utils.h"

class Model
{
protected:	

	XMMATRIX ModelMatrix;			

	ComPtr<ID3D12Resource> m_VertexResource;			
	ComPtr<ID3D12Resource> m_ModelMatrixResource;		
	ComPtr<ID3D12Resource> m_IndexResource;				

	//VertexBufferView[1]对应每个顶点的模型矩阵资源的描述符，会一起传到IA阶段的顶点缓冲里面
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView[2];
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	// 纹理名 - GPU 句柄映射表，用于索引纹理，设置根参数
	std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> Texture_GPUHandle_Map;

	// 添加新纹理 (key) 到映射表，
	void AppendTextureKey(std::string&& TextureName);

public:

	Model();

	// 获取模型需要的纹理
	const std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE>& RequestForTextureMap();

	void SetTextureGPUHandle(std::string TextureName, D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle);

	XMMATRIX GetModelMatrix();

	void SetModelMatrix(XMMATRIX Matrix);

	// 创建资源与描述符，
	virtual void CreateResourceAndDescriptor(ComPtr<ID3D12Device4>& pD3D12Device) = 0;

	// 绘制模型，
	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) = 0;

};


// 全遮挡固体方块类 
class SoildBlock : public Model
{
protected:	


	static VERTEX VertexArray[24];

	static UINT IndexArray[36];


public:		

	// 创建资源与描述符
	virtual void CreateResourceAndDescriptor(ComPtr<ID3D12Device4>& pD3D12Device) override;

};


// 台阶方块类 (抽象类)
class SoildStair : public Model
{
protected:	

	static VERTEX VertexArray[40];

	static UINT IndexArray[60];


public:		

	virtual void CreateResourceAndDescriptor(ComPtr<ID3D12Device4>& pD3D12Device) override;
};


// 泥土 
class Dirt : public SoildBlock
{
public:

	Dirt();

	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};


// 橡木木板 
class Planks_Oak : public SoildBlock
{
public:

	Planks_Oak();

	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};

// 熔炉
class Furnace : public SoildBlock
{
public:

	Furnace();

	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};


// 工作台
class Crafting_Table : public SoildBlock
{
public:

	Crafting_Table();

	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};


// 橡树原木 
class Log_Oak : public SoildBlock
{
public:

	Log_Oak();

	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};


// 草方块
class Grass : public SoildBlock
{
public:

	Grass();

	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};


// 橡木完整台阶
class Planks_Oak_SoildStair : public SoildStair
{
public:

	Planks_Oak_SoildStair();

	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};

// 淡蓝色玻璃 
class Glass_Light_Blue : public SoildBlock
{
public:

	Glass_Light_Blue();

	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};

// 门 
class Door : public Model
{
protected:

	static VERTEX VertexArray[96];

	static UINT IndexArray[144];

	virtual const VERTEX* GetVertexData() const;

public:

	Door();

	// 创建资源与描述符，
	virtual void CreateResourceAndDescriptor(ComPtr<ID3D12Device4>& pD3D12Device) override;

	// 绘制模型，
	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};


// 门的镜像版本 (实例类)，继承自门
class Mirror_Door : public Door
{
protected:

	static VERTEX VertexArray[96];

	const VERTEX* GetVertexData() const override;

public:

	Mirror_Door();
};


// 床 
class Bed : public Model
{
protected:

	static VERTEX VertexArray[32];

	static UINT IndexArray[48];

public:

	Bed();

	// 创建资源与描述符
	virtual void CreateResourceAndDescriptor(ComPtr<ID3D12Device4>& pD3D12Device) override;

	// 绘制模型
	virtual void DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) override;
};

// 模型管理器
class ModelManager
{
public:

	// 纹理映射表元素结构体
	struct TEXTURE_MAP_INFO
	{
		std::wstring TextureFilePath;			// 文件路径

		ComPtr<ID3D12Resource> DefaultHeapTextureResource;
		ComPtr<ID3D12Resource> UploadHeapTextureResource;

		D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle;	
		D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;	
	};

	// 纹理映射表
	std::unordered_map<std::string, TEXTURE_MAP_INFO> Texture_SRV_Map;

	// 非透明模型组，
	std::vector<Model*> OpaqueGroup;
	// 透明模型组
	std::vector<Model*> TransparentGroup;
	// 半透明模型组
	std::vector<Model*> TranslucenceGroup;


public:


	ModelManager();

	// 创建方块
	void CreateBlock();

	// 创建模型资源
	void CreateModelResource(ComPtr<ID3D12Device4>& pD3D12Device);

	void RenderTranslucenceModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList);
	void RenderTransparentModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList);
	void RenderOpaqueModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList);
};