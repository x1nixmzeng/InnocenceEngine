#pragma once
#include "../common/InnoType.h"
#include "../system/DXHeaders.h"

class DXShaderProgramComponent
{
public:
	DXShaderProgramComponent() {};
	~DXShaderProgramComponent() {};

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	ID3D11VertexShader* m_vertexShader = 0;
	ID3D11PixelShader* m_pixelShader = 0;
	ID3D11InputLayout* m_layout = 0;

	D3D11_BUFFER_DESC m_constantBufferDesc = D3D11_BUFFER_DESC();
	ID3D11Buffer* m_constantBuffer = 0;

	D3D11_SAMPLER_DESC m_samplerDesc = D3D11_SAMPLER_DESC();
	ID3D11SamplerState* m_samplerState = 0;
};