#include "Model.h"

Model::Model()
	: ModelMatrix(XMMatrixIdentity()),
	  VertexBufferView{},
	  IndexBufferView{}
{
}

void Model::AppendTextureKey(std::string&& TextureName)
{
	Texture_GPUHandle_Map[TextureName] = {};
}

const std::unordered_map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE>& Model::RequestForTextureMap()
{
	return Texture_GPUHandle_Map;
}

void Model::SetTextureGPUHandle(std::string TextureName, D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle)
{
	Texture_GPUHandle_Map[TextureName] = GPUHandle;
}

XMMATRIX Model::GetModelMatrix()
{
	return ModelMatrix;
}

void Model::SetModelMatrix(XMMATRIX Matrix)
{
	ModelMatrix = Matrix;
}



VERTEX SoildBlock::VertexArray[24] =
{
	// 正面
	{{0,1,0,1},{0,0}},
	{{1,1,0,1},{1,0}},
	{{1,0,0,1},{1,1}},
	{{0,0,0,1},{0,1}},

	// 背面
	{{1,1,1,1},{0,0}},
	{{0,1,1,1},{1,0}},
	{{0,0,1,1},{1,1}},
	{{1,0,1,1},{0,1}},

	// 左面
	{{0,1,1,1},{0,0}},
	{{0,1,0,1},{1,0}},
	{{0,0,0,1},{1,1}},
	{{0,0,1,1},{0,1}},

	// 右面
	{{1,1,0,1},{0,0}},
	{{1,1,1,1},{1,0}},
	{{1,0,1,1},{1,1}},
	{{1,0,0,1},{0,1}},

	// 上面
	{{0,1,1,1},{0,0}},
	{{1,1,1,1},{1,0}},
	{{1,1,0,1},{1,1}},
	{{0,1,0,1},{0,1}},

	// 下面
	{{0,0,0,1},{0,0}},
	{{1,0,0,1},{1,0}},
	{{1,0,1,1},{1,1}},
	{{0,0,1,1},{0,1}}
};

UINT SoildBlock::IndexArray[36] =
{
	// 正面
	0,1,2,0,2,3,
	// 背面
	4,5,6,4,6,7,
	// 左面
	8,9,10,8,10,11,
	// 右面
	12,13,14,12,14,15,
	// 上面
	16,17,18,16,18,19,
	// 下面
	20,21,22,20,22,23
};

void SoildBlock::CreateResourceAndDescriptor(ComPtr<ID3D12Device4>& pD3D12Device)
{
	//分配24个模型矩阵
	XMFLOAT4X4 _temp_ModelMatrix = {};
	XMStoreFloat4x4(&_temp_ModelMatrix, ModelMatrix);
	std::vector<XMFLOAT4X4> _temp_ModelMatrixGroup;
	_temp_ModelMatrixGroup.assign(24, _temp_ModelMatrix);


	//创建顶点缓冲资源
	D3D12_RESOURCE_DESC UploadResourceDesc = {};
	UploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;		
	UploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;			
	UploadResourceDesc.Height = 1;										
	UploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;					
	UploadResourceDesc.MipLevels = 1;									
	UploadResourceDesc.DepthOrArraySize = 1;							
	UploadResourceDesc.SampleDesc.Count = 1;							
	UploadResourceDesc.SampleDesc.Quality = 0;						
	D3D12_HEAP_PROPERTIES HeapProperties = { D3D12_HEAP_TYPE_UPLOAD };

	//创建顶点资源
	UploadResourceDesc.Width = 24 * sizeof(VERTEX);		
	pD3D12Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&UploadResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_VertexResource));

	//创建模型矩阵资源
	UploadResourceDesc.Width = 24 * sizeof(XMFLOAT4X4);
	pD3D12Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&UploadResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_ModelMatrixResource));

	// 创建 IndexResource
	UploadResourceDesc.Width = 36 * sizeof(XMFLOAT4X4);
	pD3D12Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&UploadResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_IndexResource));


	// 将数据进行转移
	BYTE* TransmitPointer = nullptr;	

	//顶点资源拷贝
	m_VertexResource->Map(0, nullptr, reinterpret_cast<void**>(&TransmitPointer));
	memcpy(TransmitPointer, VertexArray, 24 * sizeof(VERTEX));
	m_VertexResource->Unmap(0, nullptr);

	//模型矩阵资源拷贝
	m_ModelMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&TransmitPointer));
	memcpy(TransmitPointer, &_temp_ModelMatrixGroup[0], 24 * sizeof(XMFLOAT4X4));
	m_ModelMatrixResource->Unmap(0, nullptr);

	//索引资源拷贝
	m_IndexResource->Map(0, nullptr, reinterpret_cast<void**>(&TransmitPointer));
	memcpy(TransmitPointer, IndexArray, 36 * sizeof(UINT));
	m_IndexResource->Unmap(0, nullptr);


	// 填写 VBV 和 IBV 描述符了

	// VBV[0]: 描述顶点资源
	VertexBufferView[0].BufferLocation = m_VertexResource->GetGPUVirtualAddress();		
	VertexBufferView[0].SizeInBytes = 24 * sizeof(VERTEX);							
	VertexBufferView[0].StrideInBytes = sizeof(VERTEX);								

	// VBV[1]: 描述模型矩阵资源
	VertexBufferView[1].BufferLocation = m_ModelMatrixResource->GetGPUVirtualAddress();	
	VertexBufferView[1].SizeInBytes = 24 * sizeof(XMFLOAT4X4);							
	VertexBufferView[1].StrideInBytes = sizeof(XMFLOAT4X4);								

	// IBV: 描述顶点的索引
	IndexBufferView.BufferLocation = m_IndexResource->GetGPUVirtualAddress();			
	IndexBufferView.SizeInBytes = 36 * sizeof(UINT);									
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;										
}

VERTEX SoildStair::VertexArray[40] =
{
	// 台阶底面
	{{0,0,0,1},{0,0}},
	{{1,0,0,1},{1,0}},
	{{1,0,1,1},{1,1}},
	{{0,0,1,1},{0,1}},

	// 台阶背面
	{{1,1,1,1},{0,0}},
	{{0,1,1,1},{1,0}},
	{{0,0,1,1},{1,1}},
	{{1,0,1,1},{0,1}},

	// 台阶正面
	{{0,0.5,0,1},{0,0.5}},
	{{1,0.5,0,1},{1,0.5}},
	{{1,0,0,1},{1,1}},
	{{0,0,0,1},{0,1}},

	{{0,1,0.5,1},{0,0}},
	{{1,1,0.5,1},{1,0}},
	{{1,0.5,0.5,1},{1,0.5}},
	{{0,0.5,0.5,1},{0,0.5}},

	// 台阶顶面
	{{0,0.5,0.5,1},{0,0.5}},
	{{1,0.5,0.5,1},{1,0.5}},
	{{1,0.5,0,1},{1,1}},
	{{0,0.5,0,1},{0,1}},

	{{0,1,1,1},{0,0}},
	{{1,1,1,1},{1,0}},
	{{1,1,0.5,1},{1,0.5}},
	{{0,1,0.5,1},{0,0.5}},

	// 台阶左面
	{{0,1,1,1},{0,0}},
	{{0,1,0.5,1},{0.5,0}},
	{{0,0,0.5,1},{0.5,1}},
	{{0,0,1,1},{0,1}},

	{{0,0.5,0.5,1},{0.5,0.5}},
	{{0,0.5,0,1},{1,0.5}},
	{{0,0,0,1},{1,1}},
	{{0,0,0.5,1},{0.5,1}},

	// 台阶右面
	{{1,1,0.5,1},{0.5,0}},
	{{1,1,1,1},{1,0}},
	{{1,0,1,1},{1,1}},
	{{1,0,0.5,1},{0.5,1}},

	{{1,0.5,0,1},{0,0.5}},
	{{1,0.5,0.5,1},{0.5,0.5}},
	{{1,0,0.5,1},{0.5,1}},
	{{1,0,0,1},{0,1}}
};

UINT SoildStair::IndexArray[60] =
{
	// 台阶底面
	0,1,2,0,2,3,
	// 台阶背面
	4,5,6,4,6,7,
	// 台阶正面
	8,9,10,8,10,11,
	12,13,14,12,14,15,
	// 台阶顶面
	16,17,18,16,18,19,
	20,21,22,20,22,23,
	// 台阶左面
	24,25,26,24,26,27,
	28,29,30,28,30,31,
	// 台阶右面
	32,33,34,32,34,35,
	36,37,38,36,38,39
};

void SoildStair::CreateResourceAndDescriptor(ComPtr<ID3D12Device4>& pD3D12Device)
{
	//分配40个模型矩阵
	XMFLOAT4X4 _temp_ModelMatrix = {};
	XMStoreFloat4x4(&_temp_ModelMatrix, ModelMatrix);
	std::vector<XMFLOAT4X4> _temp_ModelMatrixGroup;
	_temp_ModelMatrixGroup.assign(40, _temp_ModelMatrix);


	D3D12_RESOURCE_DESC UploadResourceDesc = {};
	UploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;		
	UploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;			
	UploadResourceDesc.Height = 1;										
	UploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;					
	UploadResourceDesc.MipLevels = 1;									
	UploadResourceDesc.DepthOrArraySize = 1;							
	UploadResourceDesc.SampleDesc.Count = 1;							
	UploadResourceDesc.SampleDesc.Quality = 0;							
	D3D12_HEAP_PROPERTIES HeapProperties = { D3D12_HEAP_TYPE_UPLOAD };

	UploadResourceDesc.Width = 40 * sizeof(VERTEX);		
	pD3D12Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&UploadResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_VertexResource));

	UploadResourceDesc.Width = 40 * sizeof(XMFLOAT4X4);
	pD3D12Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&UploadResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_ModelMatrixResource));

	UploadResourceDesc.Width = 60 * sizeof(XMFLOAT4X4);
	pD3D12Device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE,
		&UploadResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_IndexResource));

	BYTE* TransmitPointer = nullptr;	

	m_VertexResource->Map(0, nullptr, reinterpret_cast<void**>(&TransmitPointer));
	memcpy(TransmitPointer, VertexArray, 40 * sizeof(VERTEX));
	m_VertexResource->Unmap(0, nullptr);

	m_ModelMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&TransmitPointer));
	memcpy(TransmitPointer, &_temp_ModelMatrixGroup[0], 40 * sizeof(XMFLOAT4X4));
	m_ModelMatrixResource->Unmap(0, nullptr);

	m_IndexResource->Map(0, nullptr, reinterpret_cast<void**>(&TransmitPointer));
	memcpy(TransmitPointer, IndexArray, 60 * sizeof(UINT));
	m_IndexResource->Unmap(0, nullptr);

	VertexBufferView[0].BufferLocation = m_VertexResource->GetGPUVirtualAddress();		
	VertexBufferView[0].SizeInBytes = 40 * sizeof(VERTEX);								
	VertexBufferView[0].StrideInBytes = sizeof(VERTEX);									

	VertexBufferView[1].BufferLocation = m_ModelMatrixResource->GetGPUVirtualAddress();	
	VertexBufferView[1].SizeInBytes = 40 * sizeof(XMFLOAT4X4);							
	VertexBufferView[1].StrideInBytes = sizeof(XMFLOAT4X4);								

	IndexBufferView.BufferLocation = m_IndexResource->GetGPUVirtualAddress();			
	IndexBufferView.SizeInBytes = 60 * sizeof(UINT);									
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;										
}





Dirt::Dirt()
{
	this->AppendTextureKey("dirt");
}

void Dirt::DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList)
{
	pCommandList->IASetIndexBuffer(&IndexBufferView);
	pCommandList->IASetVertexBuffers(0, 2, VertexBufferView);
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["dirt"]);
	pCommandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

Planks_Oak::Planks_Oak()
{
	this->AppendTextureKey("planks_oak");
}

void Planks_Oak::DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) 
{
	pCommandList->IASetIndexBuffer(&IndexBufferView);
	pCommandList->IASetVertexBuffers(0, 2, VertexBufferView);
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["planks_oak"]);
	pCommandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

Furnace::Furnace()
{
	this->AppendTextureKey("furnace_front_off");
	this->AppendTextureKey("furnace_side");
	this->AppendTextureKey("furnace_top");
}

void Furnace::DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList) 
{
	pCommandList->IASetIndexBuffer(&IndexBufferView);
	pCommandList->IASetVertexBuffers(0, 2, VertexBufferView);

	// 渲染上下面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["furnace_top"]);
	pCommandList->DrawIndexedInstanced(12, 1, 24, 0, 0);

	// 渲染左右背面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["furnace_side"]);
	pCommandList->DrawIndexedInstanced(18, 1, 6, 0, 0);

	// 渲染正面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["furnace_front_off"]);
	pCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

Crafting_Table::Crafting_Table()
{
	this->AppendTextureKey("crafting_table_front");
	this->AppendTextureKey("crafting_table_side");
	this->AppendTextureKey("crafting_table_top");
}

void Crafting_Table::DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList)
{
	pCommandList->IASetIndexBuffer(&IndexBufferView);
	pCommandList->IASetVertexBuffers(0, 2, VertexBufferView);

	// 渲染上下面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["crafting_table_top"]);
	pCommandList->DrawIndexedInstanced(12, 1, 24, 0, 0);

	// 渲染左右背面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["crafting_table_side"]);
	pCommandList->DrawIndexedInstanced(18, 1, 6, 0, 0);

	// 渲染正面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["crafting_table_front"]);
	pCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

Log_Oak::Log_Oak()
{
	this->AppendTextureKey("log_oak");
	this->AppendTextureKey("log_oak_top");
}

void Log_Oak::DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList)
{
	pCommandList->IASetIndexBuffer(&IndexBufferView);
	pCommandList->IASetVertexBuffers(0, 2, VertexBufferView);

	// 渲染上下面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["log_oak_top"]);
	pCommandList->DrawIndexedInstanced(12, 1, 24, 0, 0);

	// 渲染左右正背面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["log_oak"]);
	pCommandList->DrawIndexedInstanced(24, 1, 0, 0, 0);
}

Grass::Grass()
{
	this->AppendTextureKey("grass_side");
	this->AppendTextureKey("grass_top");
	this->AppendTextureKey("dirt");
}

void Grass::DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList)
{
	pCommandList->IASetIndexBuffer(&IndexBufferView);
	pCommandList->IASetVertexBuffers(0, 2, VertexBufferView);

	// 渲染上面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["grass_top"]);
	pCommandList->DrawIndexedInstanced(6, 1, 24, 0, 0);

	// 渲染下面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["dirt"]);
	pCommandList->DrawIndexedInstanced(6, 1, 30, 0, 0);

	// 渲染左右正背面
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["grass_side"]);
	pCommandList->DrawIndexedInstanced(24, 1, 0, 0, 0);
}

Planks_Oak_SoildStair::Planks_Oak_SoildStair()
{
	this->AppendTextureKey("planks_oak");
}

void Planks_Oak_SoildStair::DrawModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList)
{
	pCommandList->IASetIndexBuffer(&IndexBufferView);
	pCommandList->IASetVertexBuffers(0, 2, VertexBufferView);
	pCommandList->SetGraphicsRootDescriptorTable(1, Texture_GPUHandle_Map["planks_oak"]);
	pCommandList->DrawIndexedInstanced(60, 1, 0, 0, 0);
}





ModelManager::ModelManager()
{
	Texture_SRV_Map["dirt"].TextureFilePath = L"resource/dirt.png";
	Texture_SRV_Map["grass_top"].TextureFilePath = L"resource/grass_top.png";
	Texture_SRV_Map["grass_side"].TextureFilePath = L"resource/grass_side.png";
	Texture_SRV_Map["log_oak"].TextureFilePath = L"resource/log_oak.png";
	Texture_SRV_Map["log_oak_top"].TextureFilePath = L"resource/log_oak_top.png";
	Texture_SRV_Map["furnace_front_off"].TextureFilePath = L"resource/furnace_front_off.png";
	Texture_SRV_Map["furnace_side"].TextureFilePath = L"resource/furnace_side.png";
	Texture_SRV_Map["furnace_top"].TextureFilePath = L"resource/furnace_top.png";
	Texture_SRV_Map["crafting_table_front"].TextureFilePath = L"resource/crafting_table_front.png";
	Texture_SRV_Map["crafting_table_side"].TextureFilePath = L"resource/crafting_table_side.png";
	Texture_SRV_Map["crafting_table_top"].TextureFilePath = L"resource/crafting_table_top.png";
	Texture_SRV_Map["planks_oak"].TextureFilePath = L"resource/planks_oak.png";
}

void ModelManager::CreateBlock()
{
	// 两层泥土地基，y 是高度
	for (int x = 0; x < 10; x++)
	{
		for (int z = -4; z < 10; z++)
		{
			for (int y = -2; y < 0; y++)
			{
				Model* dirt = new Dirt();							
				dirt->SetModelMatrix(XMMatrixTranslation(x, y, z));	
				ModelGroup.push_back(dirt);							
			}
		}
	}

	// 一层草方块地基
	for (int x = 0; x < 10; x++)
	{
		for (int z = -4; z < 10; z++)
		{
			Model* grass = new Grass();
			grass->SetModelMatrix(XMMatrixTranslation(x, 0, z));
			ModelGroup.push_back(grass);
		}
	}

	// 4x4 木板房基

	for (int x = 3; x < 7; x++)
	{
		for (int z = 3; z < 7; z++)
		{
			Model* plank = new Planks_Oak();
			plank->SetModelMatrix(XMMatrixTranslation(x, 2, z));
			ModelGroup.push_back(plank);
		}
	}


	// 8 柱原木 

	for (int y = 1; y < 7; y++)
	{
		Model* log_oak = new Log_Oak();
		log_oak->SetModelMatrix(XMMatrixTranslation(3, y, 2));
		ModelGroup.push_back(log_oak);
	}

	for (int y = 1; y < 7; y++)
	{
		Model* log_oak = new Log_Oak();
		log_oak->SetModelMatrix(XMMatrixTranslation(2, y, 3));
		ModelGroup.push_back(log_oak);
	}

	for (int y = 1; y < 7; y++)
	{
		Model* log_oak = new Log_Oak();
		log_oak->SetModelMatrix(XMMatrixTranslation(6, y, 2));
		ModelGroup.push_back(log_oak);
	}

	for (int y = 1; y < 7; y++)
	{
		Model* log_oak = new Log_Oak();
		log_oak->SetModelMatrix(XMMatrixTranslation(7, y, 3));
		ModelGroup.push_back(log_oak);
	}

	for (int y = 1; y < 7; y++)
	{
		Model* log_oak = new Log_Oak();
		log_oak->SetModelMatrix(XMMatrixTranslation(7, y, 6));
		ModelGroup.push_back(log_oak);
	}

	for (int y = 1; y < 7; y++)
	{
		Model* log_oak = new Log_Oak();
		log_oak->SetModelMatrix(XMMatrixTranslation(6, y, 7));
		ModelGroup.push_back(log_oak);
	}

	for (int y = 1; y < 7; y++)
	{
		Model* log_oak = new Log_Oak();
		log_oak->SetModelMatrix(XMMatrixTranslation(2, y, 6));
		ModelGroup.push_back(log_oak);
	}

	for (int y = 1; y < 7; y++)
	{
		Model* log_oak = new Log_Oak();
		log_oak->SetModelMatrix(XMMatrixTranslation(3, y, 7));
		ModelGroup.push_back(log_oak);
	}


	// 其他木板与门前台阶
	{
		Model* plank = new Planks_Oak();
		plank->SetModelMatrix(XMMatrixTranslation(4, 2, 2));
		ModelGroup.push_back(plank);

		plank = new Planks_Oak();
		plank->SetModelMatrix(XMMatrixTranslation(5, 2, 2));
		ModelGroup.push_back(plank);

		for (int y = 5; y < 7; y++)
		{
			for (int x = 4; x < 6; x++)
			{
				plank = new Planks_Oak();
				plank->SetModelMatrix(XMMatrixTranslation(x, y, 2));
				ModelGroup.push_back(plank);
			}
		}

		for (int y = 2; y < 4; y++)
		{
			for (int z = 4; z < 6; z++)
			{
				plank = new Planks_Oak();
				plank->SetModelMatrix(XMMatrixTranslation(2, y, z));
				ModelGroup.push_back(plank);
			}
		}

		for (int y = 2; y < 4; y++)
		{
			for (int x = 4; x < 6; x++)
			{
				plank = new Planks_Oak();
				plank->SetModelMatrix(XMMatrixTranslation(x, y, 7));
				ModelGroup.push_back(plank);
			}
		}

		for (int y = 2; y < 4; y++)
		{
			for (int z = 4; z < 6; z++)
			{
				plank = new Planks_Oak();
				plank->SetModelMatrix(XMMatrixTranslation(7, y, z));
				ModelGroup.push_back(plank);
			}
		}

		plank = new Planks_Oak();
		plank->SetModelMatrix(XMMatrixTranslation(2, 6, 4));
		ModelGroup.push_back(plank);

		plank = new Planks_Oak();
		plank->SetModelMatrix(XMMatrixTranslation(2, 6, 5));
		ModelGroup.push_back(plank);

		plank = new Planks_Oak();
		plank->SetModelMatrix(XMMatrixTranslation(4, 6, 7));
		ModelGroup.push_back(plank);

		plank = new Planks_Oak();
		plank->SetModelMatrix(XMMatrixTranslation(5, 6, 7));
		ModelGroup.push_back(plank);

		plank = new Planks_Oak();
		plank->SetModelMatrix(XMMatrixTranslation(7, 6, 4));
		ModelGroup.push_back(plank);

		plank = new Planks_Oak();
		plank->SetModelMatrix(XMMatrixTranslation(7, 6, 5));
		ModelGroup.push_back(plank);

		Model* stair = new Planks_Oak_SoildStair();
		stair->SetModelMatrix(XMMatrixTranslation(4, 2, 1));
		ModelGroup.push_back(stair);

		stair = new Planks_Oak_SoildStair();
		stair->SetModelMatrix(XMMatrixTranslation(5, 2, 1));
		ModelGroup.push_back(stair);

		stair = new Planks_Oak_SoildStair();
		stair->SetModelMatrix(XMMatrixTranslation(4, 1, 0));
		ModelGroup.push_back(stair);

		stair = new Planks_Oak_SoildStair();
		stair->SetModelMatrix(XMMatrixTranslation(5, 1, 0));
		ModelGroup.push_back(stair);
	}

	// 4x4 木板房顶

	for (int x = 3; x < 7; x++)
	{
		for (int z = 3; z < 7; z++)
		{
			Model* plank = new Planks_Oak();
			plank->SetModelMatrix(XMMatrixTranslation(x, 6, z));
			ModelGroup.push_back(plank);
		}
	}

	// 屋顶

	{
		// 第一层

		for (int x = 3; x < 7; x++)
		{
			Model* stair = new Planks_Oak_SoildStair();
			stair->SetModelMatrix(XMMatrixTranslation(x, 6, 1));
			ModelGroup.push_back(stair);
		}

		for (int x = 3; x < 7; x++)
		{
			XMMATRIX transform = XMMatrixTranslation(-0.5, -0.5, -0.5);
			transform *= XMMatrixRotationY(XM_PI);						// 平移中心后，再旋转，否则会出错 (旋转角度是弧度)
			transform *= XMMatrixTranslation(0.5, 0.5, 0.5);			// 旋转完再还原
			transform *= XMMatrixTranslation(x, 6, 8);					// 再平移到对应的坐标
			Model* stair = new Planks_Oak_SoildStair();
			stair->SetModelMatrix(transform);
			ModelGroup.push_back(stair);
		}

		for (int z = 3; z < 7; z++)
		{
			XMMATRIX transform = XMMatrixTranslation(-0.5, -0.5, -0.5);
			transform *= XMMatrixRotationY(XM_PIDIV2);					// 旋转 90°
			transform *= XMMatrixTranslation(0.5, 0.5, 0.5);
			transform *= XMMatrixTranslation(1, 6, z);
			Model* stair = new Planks_Oak_SoildStair();
			stair->SetModelMatrix(transform);
			ModelGroup.push_back(stair);
		}

		for (int z = 3; z < 7; z++)
		{
			XMMATRIX transform = XMMatrixTranslation(-0.5, -0.5, -0.5);
			transform *= XMMatrixRotationY(XM_PI + XM_PIDIV2);			// 旋转 270°
			transform *= XMMatrixTranslation(0.5, 0.5, 0.5);
			transform *= XMMatrixTranslation(8, 6, z);
			Model* stair = new Planks_Oak_SoildStair();
			stair->SetModelMatrix(transform);
			ModelGroup.push_back(stair);
		}

		// 第二层

		for (int x = 3; x < 7; x++)
		{
			Model* stair = new Planks_Oak_SoildStair();
			stair->SetModelMatrix(XMMatrixTranslation(x, 7, 2));
			ModelGroup.push_back(stair);
		}

		for (int x = 3; x < 7; x++)
		{
			XMMATRIX transform = XMMatrixTranslation(-0.5, -0.5, -0.5);
			transform *= XMMatrixRotationY(XM_PI);
			transform *= XMMatrixTranslation(0.5, 0.5, 0.5);
			transform *= XMMatrixTranslation(x, 7, 7);
			Model* stair = new Planks_Oak_SoildStair();
			stair->SetModelMatrix(transform);
			ModelGroup.push_back(stair);
		}

		for (int z = 3; z < 7; z++)
		{
			XMMATRIX transform = XMMatrixTranslation(-0.5, -0.5, -0.5);
			transform *= XMMatrixRotationY(XM_PIDIV2);
			transform *= XMMatrixTranslation(0.5, 0.5, 0.5);
			transform *= XMMatrixTranslation(2, 7, z);
			Model* stair = new Planks_Oak_SoildStair();
			stair->SetModelMatrix(transform);
			ModelGroup.push_back(stair);
		}

		for (int z = 3; z < 7; z++)
		{
			XMMATRIX transform = XMMatrixTranslation(-0.5, -0.5, -0.5);
			transform *= XMMatrixRotationY(XM_PI + XM_PIDIV2);
			transform *= XMMatrixTranslation(0.5, 0.5, 0.5);
			transform *= XMMatrixTranslation(7, 7, z);
			Model* stair = new Planks_Oak_SoildStair();
			stair->SetModelMatrix(transform);
			ModelGroup.push_back(stair);
		}

		// 补上屋顶空位

		for (int x = 3; x < 7; x++)
		{
			for (int z = 3; z < 7; z++)
			{
				Model* plank = new Planks_Oak();
				plank->SetModelMatrix(XMMatrixTranslation(x, 7, z));
				ModelGroup.push_back(plank);
			}
		}
	}

	// 工作台和熔炉
	{
		Model* craft_table = new Crafting_Table();
		craft_table->SetModelMatrix(XMMatrixTranslation(3, 3, 6));
		ModelGroup.push_back(craft_table);

		Model* furnace = new Furnace();
		furnace->SetModelMatrix(XMMatrixTranslation(4, 3, 6));
		ModelGroup.push_back(furnace);

		furnace = new Furnace();
		furnace->SetModelMatrix(XMMatrixTranslation(5, 3, 6));
		ModelGroup.push_back(furnace);
	}

}

void ModelManager::CreateModelResource(ComPtr<ID3D12Device4>& pD3D12Device)
{
	for (auto& model : ModelGroup)
	{
		// 创建模型资源
		model->CreateResourceAndDescriptor(pD3D12Device);
		//映射模型要使用的纹理
		for (const auto& texture : model->RequestForTextureMap())
		{
			model->SetTextureGPUHandle(texture.first, Texture_SRV_Map[texture.first].GPUHandle);
		}
	}
}

void ModelManager::RenderAllModel(ComPtr<ID3D12GraphicsCommandList>& pCommandList)
{
	// 遍历模型组进行绘制
	for (const auto& model : ModelGroup)
	{
		model->DrawModel(pCommandList);
	}
}