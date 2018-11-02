#include "DXRenderingSystem.h"

#include "../../component/DXFinalRenderPassSingletonComponent.h"

#include <sstream>
#include "../../component/WindowSystemSingletonComponent.h"
#include "../../component/DXWindowSystemSingletonComponent.h"
#include "../../component/RenderingSystemSingletonComponent.h"
#include "../../component/DXRenderingSystemSingletonComponent.h"
#include "../../component/AssetSystemSingletonComponent.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "../LowLevelSystem/LogSystem.h"
#include "../LowLevelSystem/MemorySystem.h"
#include "../HighLevelSystem/AssetSystem.h"
#include "../HighLevelSystem/GameSystem.h"
#include "../../component/GameSystemSingletonComponent.h"

InnoHighLevelSystem_EXPORT bool DXRenderingSystem::setup()
{
	HRESULT result;
	IDXGIFactory* factory;
	IDXGIAdapter* adapter;
	IDXGIOutput* adapterOutput;
	unsigned int numModes, i, numerator, denominator;
	unsigned long long stringLength;
	DXGI_MODE_DESC* displayModeList;
	DXGI_ADAPTER_DESC adapterDesc;
	int error;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	D3D_FEATURE_LEVEL featureLevel;
	ID3D11Texture2D* backBufferPtr;
	D3D11_TEXTURE2D_DESC depthBufferDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	D3D11_RASTERIZER_DESC rasterDesc;
	D3D11_VIEWPORT viewport;

	// Create a DirectX graphics interface factory.
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create DXGI factory!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Use the factory to create an adapter for the primary graphics interface (video card).
	result = factory->EnumAdapters(0, &adapter);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create video card adapter!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Enumerate the primary adapter output (monitor).
	result = adapter->EnumOutputs(0, &adapterOutput);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create monitor adapter!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't get DXGI_FORMAT_R8G8B8A8_UNORM fitted monitor!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	displayModeList = new DXGI_MODE_DESC[numModes];

	// Now fill the display mode list structures.
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't fill the display mode list structures!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	for (i = 0; i<numModes; i++)
	{
		if (displayModeList[i].Width == (unsigned int)WindowSystemSingletonComponent::getInstance().m_windowResolution.x)
		{
			if (displayModeList[i].Height == (unsigned int)WindowSystemSingletonComponent::getInstance().m_windowResolution.y)
			{
				numerator = displayModeList[i].RefreshRate.Numerator;
				denominator = displayModeList[i].RefreshRate.Denominator;
			}
		}
	}

	// Get the adapter (video card) description.
	result = adapter->GetDesc(&adapterDesc);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't get the video card adapter description!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Store the dedicated video card memory in megabytes.
	DXRenderingSystemSingletonComponent::getInstance().m_videoCardMemory = (int)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	error = wcstombs_s(&stringLength, DXRenderingSystemSingletonComponent::getInstance().m_videoCardDescription, 128, adapterDesc.Description, 128);
	if (error != 0)
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't convert the name of the video card to a character array!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Release the display mode list.
	delete[] displayModeList;
	displayModeList = 0;

	// Release the adapter output.
	adapterOutput->Release();
	adapterOutput = 0;

	// Release the adapter.
	adapter->Release();
	adapter = 0;

	// Release the factory.
	factory->Release();
	factory = 0;

	// Initialize the swap chain description.
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	// Set to a single back buffer.
	swapChainDesc.BufferCount = 1;

	// Set the width and height of the back buffer.
	swapChainDesc.BufferDesc.Width = (UINT)WindowSystemSingletonComponent::getInstance().m_windowResolution.x;
	swapChainDesc.BufferDesc.Height = (UINT)WindowSystemSingletonComponent::getInstance().m_windowResolution.y;

	// Set regular 32-bit surface for the back buffer.
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Set the refresh rate of the back buffer.
	if (DXRenderingSystemSingletonComponent::getInstance().m_vsync_enabled)
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
	}
	else
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	// Set the usage of the back buffer.
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Set the handle for the window to render to.
	swapChainDesc.OutputWindow = DXWindowSystemSingletonComponent::getInstance().m_hwnd;

	// Turn multisampling off.
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;

	// Set to full screen or windowed mode.
	if (WindowSystemSingletonComponent::getInstance().m_fullScreen)
	{
		swapChainDesc.Windowed = false;
	}
	else
	{
		swapChainDesc.Windowed = true;
	}

	// Set the scan line ordering and scaling to unspecified.
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	swapChainDesc.Flags = 0;

	// Set the feature level to DirectX 11.
	featureLevel = D3D_FEATURE_LEVEL_11_0;

	// Create the swap chain, Direct3D device, and Direct3D device context.
	result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
		D3D11_SDK_VERSION, &swapChainDesc, &DXRenderingSystemSingletonComponent::getInstance().m_swapChain, &DXRenderingSystemSingletonComponent::getInstance().m_device, NULL, &DXRenderingSystemSingletonComponent::getInstance().m_deviceContext);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create the swap chain/D3D device/D3D device context!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Get the pointer to the back buffer.
	result = DXRenderingSystemSingletonComponent::getInstance().m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferPtr);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't get back buffer pointer!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Create the render target view with the back buffer pointer.
	result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateRenderTargetView(backBufferPtr, NULL, &DXRenderingSystemSingletonComponent::getInstance().m_renderTargetView);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create render target view!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Release pointer to the back buffer as we no longer need it.
	backBufferPtr->Release();
	backBufferPtr = 0;

	// Initialize the description of the depth buffer.
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

	// Set up the description of the depth buffer.
	depthBufferDesc.Width = (UINT)WindowSystemSingletonComponent::getInstance().m_windowResolution.x;
	depthBufferDesc.Height = (UINT)WindowSystemSingletonComponent::getInstance().m_windowResolution.y;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;

	// Create the texture for the depth buffer using the filled out description.
	result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateTexture2D(&depthBufferDesc, NULL, &DXRenderingSystemSingletonComponent::getInstance().m_depthStencilBuffer);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create the texture for the depth buffer!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Initialize the description of the stencil state.
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	// Set up the description of the stencil state.
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create the depth stencil state.
	result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateDepthStencilState(&depthStencilDesc, &DXRenderingSystemSingletonComponent::getInstance().m_depthStencilState);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create the depth stencil state!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Set the depth stencil state.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->OMSetDepthStencilState(DXRenderingSystemSingletonComponent::getInstance().m_depthStencilState, 1);

	// Initialize the depth stencil view.
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	// Set up the depth stencil view description.
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateDepthStencilView(DXRenderingSystemSingletonComponent::getInstance().m_depthStencilBuffer, &depthStencilViewDesc, &DXRenderingSystemSingletonComponent::getInstance().m_depthStencilView);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create the depth stencil view!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Bind the render target view and depth stencil buffer to the output render pipeline.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->OMSetRenderTargets(1, &DXRenderingSystemSingletonComponent::getInstance().m_renderTargetView, DXRenderingSystemSingletonComponent::getInstance().m_depthStencilView);

	// Setup the raster description which will determine how and what polygons will be drawn.
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state from the description we just filled out.
	result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateRasterizerState(&rasterDesc, &DXRenderingSystemSingletonComponent::getInstance().m_rasterState);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create the rasterizer state!");
		m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Now set the rasterizer state.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->RSSetState(DXRenderingSystemSingletonComponent::getInstance().m_rasterState);

	// Setup the viewport for rendering.
	viewport.Width = (float)WindowSystemSingletonComponent::getInstance().m_windowResolution.x;
	viewport.Height = (float)WindowSystemSingletonComponent::getInstance().m_windowResolution.y;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	// Create the viewport.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->RSSetViewports(1, &viewport);

	m_objectStatus = objectStatus::ALIVE;
	return true;
}

InnoHighLevelSystem_EXPORT bool DXRenderingSystem::initialize()
{
	initializeFinalBlendPass();

	InnoLogSystem::printLog("DXRenderingSystem has been initialized.");
	return true;
}

InnoHighLevelSystem_EXPORT bool DXRenderingSystem::update()
{
	if (AssetSystemSingletonComponent::getInstance().m_uninitializedMeshComponents.size() > 0)
	{
		MeshDataComponent* l_meshDataComponent;
		if (AssetSystemSingletonComponent::getInstance().m_uninitializedMeshComponents.tryPop(l_meshDataComponent))
		{
			initializeMeshDataComponent(l_meshDataComponent);
		}
	}
	//if (AssetSystemSingletonComponent::getInstance().m_uninitializedTextureComponents.size() > 0)
	//{
	//	TextureDataComponent* l_textureDataComponent;
	//	if (AssetSystemSingletonComponent::getInstance().m_uninitializedTextureComponents.tryPop(l_textureDataComponent))
	//	{
	//		initializeTextureDataComponent(l_textureDataComponent);
	//	}
	//}
	// Clear the buffers to begin the scene.
	beginScene(0.0f, 0.0f, 0.0f, 0.0f);

	updateFinalBlendPass();

	// Present the rendered scene to the screen.
	endScene();
	return true;
}

InnoHighLevelSystem_EXPORT bool DXRenderingSystem::terminate()
{
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	if (DXRenderingSystemSingletonComponent::getInstance().m_swapChain)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_swapChain->SetFullscreenState(false, NULL);
	}

	if (DXRenderingSystemSingletonComponent::getInstance().m_rasterState)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_rasterState->Release();
		DXRenderingSystemSingletonComponent::getInstance().m_rasterState = 0;
	}

	if (DXRenderingSystemSingletonComponent::getInstance().m_depthStencilView)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_depthStencilView->Release();
		DXRenderingSystemSingletonComponent::getInstance().m_depthStencilView = 0;
	}

	if (DXRenderingSystemSingletonComponent::getInstance().m_depthStencilState)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_depthStencilState->Release();
		DXRenderingSystemSingletonComponent::getInstance().m_depthStencilState = 0;
	}

	if (DXRenderingSystemSingletonComponent::getInstance().m_depthStencilBuffer)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_depthStencilBuffer->Release();
		DXRenderingSystemSingletonComponent::getInstance().m_depthStencilBuffer = 0;
	}

	if (DXRenderingSystemSingletonComponent::getInstance().m_renderTargetView)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_renderTargetView->Release();
		DXRenderingSystemSingletonComponent::getInstance().m_renderTargetView = 0;
	}

	if (DXRenderingSystemSingletonComponent::getInstance().m_deviceContext)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->Release();
		DXRenderingSystemSingletonComponent::getInstance().m_deviceContext = 0;
	}

	if (DXRenderingSystemSingletonComponent::getInstance().m_device)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_device->Release();
		DXRenderingSystemSingletonComponent::getInstance().m_device = 0;
	}

	if (DXRenderingSystemSingletonComponent::getInstance().m_swapChain)
	{
		DXRenderingSystemSingletonComponent::getInstance().m_swapChain->Release();
		DXRenderingSystemSingletonComponent::getInstance().m_swapChain = 0;
	}

	m_objectStatus = objectStatus::SHUTDOWN;
	InnoLogSystem::printLog("DXRenderingSystem has been terminated.");
	return true;
}

objectStatus DXRenderingSystem::getStatus()
{
	return m_objectStatus;
}

void DXRenderingSystem::initializeFinalBlendPass()
{
	initializeShader(shaderType::VERTEX, L"..//res//shaders//DX11//testVertex.sf");

	initializeShader(shaderType::FRAGMENT, L"..//res//shaders//DX11//testPixel.sf");

	// Setup the description of the dynamic matrix constant buffer
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.ByteWidth = sizeof(DirectX::XMMATRIX);
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.MiscFlags = 0;
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer
	auto result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateBuffer(&DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc, NULL, &DXFinalRenderPassSingletonComponent::getInstance().m_matrixBuffer);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create matrix buffer pointer!");
		m_objectStatus = objectStatus::STANDBY;
		return;
	}

	// Create a texture sampler state description.
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.MipLODBias = 0.0f;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.MaxAnisotropy = 1;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.BorderColor[0] = 0;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.BorderColor[1] = 0;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.BorderColor[2] = 0;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.BorderColor[3] = 0;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.MinLOD = 0;
	DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateSamplerState(&DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc, &DXFinalRenderPassSingletonComponent::getInstance().m_sampleState);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't create texture sampler state!");
		m_objectStatus = objectStatus::STANDBY;
		return;
	}
}

void DXRenderingSystem::initializeShader(shaderType shaderType, const std::wstring & shaderFilePath)
{
	auto l_shaderFilePath = shaderFilePath.c_str();
	auto l_shaderName = std::string(shaderFilePath.begin(), shaderFilePath.end());
	std::reverse(l_shaderName.begin(), l_shaderName.end());
	l_shaderName = l_shaderName.substr(l_shaderName.find(".") + 1, l_shaderName.find("//") - l_shaderName.find(".") - 1);
	std::reverse(l_shaderName.begin(), l_shaderName.end());

	HRESULT result;
	ID3D10Blob* errorMessage;
	ID3D10Blob* shaderBuffer;

	// Initialize the pointers this function will use to null.
	errorMessage = 0;
	shaderBuffer = 0;

	switch (shaderType)
	{
	case shaderType::VERTEX:
		// Compile the shader code.
		result = D3DCompileFromFile(l_shaderFilePath, NULL, NULL, l_shaderName.c_str(), "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
			&shaderBuffer, &errorMessage);
		if (FAILED(result))
		{
			// If the shader failed to compile it should have writen something to the error message.
			if (errorMessage)
			{
				OutputShaderErrorMessage(errorMessage, DXWindowSystemSingletonComponent::getInstance().m_hwnd, l_shaderName.c_str());
			}
			// If there was nothing in the error message then it simply could not find the shader file itself.
			else
			{
				MessageBox(DXWindowSystemSingletonComponent::getInstance().m_hwnd, l_shaderName.c_str(), "Missing Shader File", MB_OK);
				InnoLogSystem::printLog("Error: Shader creation failed: cannot find shader!");
			}

			return;
		}
		InnoLogSystem::printLog("DXRenderingSystem: innoShader: " + l_shaderName + " Shader has been compiled.");

		// Create the shader from the buffer.
		result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateVertexShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), NULL, &DXFinalRenderPassSingletonComponent::getInstance().m_vertexShader);
		if (FAILED(result))
		{
			InnoLogSystem::printLog("Error: DXRenderingSystem: can't create vertex shader!");
			m_objectStatus = objectStatus::STANDBY;
			return;
		}
		InnoLogSystem::printLog("DXRenderingSystem: innoShader: " + l_shaderName + " Shader has been created.");

		D3D11_INPUT_ELEMENT_DESC polygonLayout[3];
		unsigned int numElements;

		// Create the vertex input layout description.
		polygonLayout[0].SemanticName = "POSITION";
		polygonLayout[0].SemanticIndex = 0;
		polygonLayout[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		polygonLayout[0].InputSlot = 0;
		polygonLayout[0].AlignedByteOffset = 0;
		polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		polygonLayout[0].InstanceDataStepRate = 0;

		polygonLayout[1].SemanticName = "TEXCOORD";
		polygonLayout[1].SemanticIndex = 0;
		polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
		polygonLayout[1].InputSlot = 0;
		polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		polygonLayout[1].InstanceDataStepRate = 0;

		polygonLayout[2].SemanticName = "NORMAL";
		polygonLayout[2].SemanticIndex = 0;
		polygonLayout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		polygonLayout[2].InputSlot = 0;
		polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		polygonLayout[2].InstanceDataStepRate = 0;

		// Get a count of the elements in the layout.
		numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

		// Create the vertex input layout.
		result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateInputLayout(polygonLayout, numElements, shaderBuffer->GetBufferPointer(),
			shaderBuffer->GetBufferSize(), &DXFinalRenderPassSingletonComponent::getInstance().m_layout);
		if (FAILED(result))
		{
			InnoLogSystem::printLog("Error: DXRenderingSystem: can't create shader layout!");
			m_objectStatus = objectStatus::STANDBY;
			return;
		}
		break;

	case shaderType::GEOMETRY:
		break;

	case shaderType::FRAGMENT:
		// Compile the shader code.
		result = D3DCompileFromFile(l_shaderFilePath, NULL, NULL, l_shaderName.c_str(), "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
			&shaderBuffer, &errorMessage);
		if (FAILED(result))
		{
			// If the shader failed to compile it should have writen something to the error message.
			if (errorMessage)
			{
				OutputShaderErrorMessage(errorMessage, DXWindowSystemSingletonComponent::getInstance().m_hwnd, l_shaderName.c_str());
			}
			// If there was nothing in the error message then it simply could not find the shader file itself.
			else
			{
				MessageBox(DXWindowSystemSingletonComponent::getInstance().m_hwnd, l_shaderName.c_str(), "Missing Shader File", MB_OK);
				InnoLogSystem::printLog("Error: Shader creation failed: cannot find shader!");
			}

			return;
		}
		InnoLogSystem::printLog("DXRenderingSystem: innoShader: " + l_shaderName + " Shader has been compiled.");

		// Create the shader from the buffer.
		result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreatePixelShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), NULL, &DXFinalRenderPassSingletonComponent::getInstance().m_pixelShader);
		if (FAILED(result))
		{
			InnoLogSystem::printLog("Error: DXRenderingSystem: can't create pixel shader!");
			m_objectStatus = objectStatus::STANDBY;
			return;
		}
		InnoLogSystem::printLog("DXRenderingSystem: innoShader: " + l_shaderName + " Shader has been created.");
		break;

	default:
		break;
	}

	shaderBuffer->Release();
	shaderBuffer = 0;
}

void DXRenderingSystem::OutputShaderErrorMessage(ID3D10Blob * errorMessage, HWND hwnd, const std::string & shaderFilename)
{
	char* compileErrors;
	unsigned long long bufferSize, i;
	std::stringstream errorSStream;

	// Get a pointer to the error message text buffer.
	compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	bufferSize = errorMessage->GetBufferSize();

	// Write out the error message.
	for (i = 0; i<bufferSize; i++)
	{
		errorSStream << compileErrors[i];
	}

	// Release the error message.
	errorMessage->Release();
	errorMessage = 0;

	MessageBox(DXWindowSystemSingletonComponent::getInstance().m_hwnd, errorSStream.str().c_str(), shaderFilename.c_str(), MB_OK);
	InnoLogSystem::printLog("DXRenderingSystem: innoShader: " + shaderFilename + " compile error: " + errorSStream.str() + "\n -- --------------------------------------------------- -- ");
}

DXMeshDataComponent* DXRenderingSystem::initializeMeshDataComponent(MeshDataComponent * rhs)
{
	if (rhs->m_objectStatus == objectStatus::ALIVE)
	{
		return getDXMeshDataComponent(rhs->m_parentEntity);
	}
	else
	{
		auto l_ptr = addDXMeshDataComponent(rhs->m_parentEntity);

		// Set up the description of the static vertex buffer.
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(Vertex) * (UINT)rhs->m_vertices.size();
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;
		vertexBufferDesc.StructureByteStride = 0;

		// Give the subresource structure a pointer to the vertex data.
		// @TODO: InnoMath's vec4 is 32bit while XMFLOAT4 is 16bit
		D3D11_SUBRESOURCE_DATA vertexData;
		ZeroMemory(&vertexData, sizeof(vertexData));
		vertexData.pSysMem = &rhs->m_vertices[0];
		vertexData.SysMemPitch = 0;
		vertexData.SysMemSlicePitch = 0;

		// Now create the vertex buffer.
		HRESULT result;
		result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateBuffer(&vertexBufferDesc, &vertexData, &l_ptr->m_vertexBuffer);
		if (FAILED(result))
		{
			InnoLogSystem::printLog("Error: DXRenderingSystem: can't create vbo!");
			return nullptr;
		}

		// Set up the description of the static index buffer.
		D3D11_BUFFER_DESC indexBufferDesc;
		ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.ByteWidth = sizeof(unsigned long) * (UINT)rhs->m_indices.size();
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;
		indexBufferDesc.StructureByteStride = 0;

		// Give the subresource structure a pointer to the index data.
		D3D11_SUBRESOURCE_DATA indexData;
		ZeroMemory(&indexData, sizeof(indexData));
		indexData.pSysMem = &rhs->m_indices[0];
		indexData.SysMemPitch = 0;
		indexData.SysMemSlicePitch = 0;

		// Create the index buffer.
		result = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateBuffer(&indexBufferDesc, &indexData, &l_ptr->m_indexBuffer);
		if (FAILED(result))
		{
			InnoLogSystem::printLog("Error: DXRenderingSystem: can't create ibo!");
			return nullptr;
		}
		l_ptr->m_objectStatus = objectStatus::ALIVE;
		rhs->m_objectStatus = objectStatus::ALIVE;
		return l_ptr;
	}
}

DXTextureDataComponent* DXRenderingSystem::initializeTextureDataComponent(TextureDataComponent * rhs)
{
	if (rhs->m_objectStatus == objectStatus::ALIVE)
	{
		return getDXTextureDataComponent(rhs->m_parentEntity);
	}
	else
	{
		auto l_ptr = addDXTextureDataComponent(rhs->m_parentEntity);

		// set texture formats
		DXGI_FORMAT l_internalFormat = DXGI_FORMAT_UNKNOWN;

		// @TODO: Unified internal format
		// Setup the description of the texture.
		if (rhs->m_textureType == textureType::ALBEDO)
		{
			if (rhs->m_texturePixelDataFormat == texturePixelDataFormat::RGB)
			{
				l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			}
			else if (rhs->m_texturePixelDataFormat == texturePixelDataFormat::RGBA)
			{
				l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			}
		}
		else
		{
			// Different than OpenGL, DX's format didn't allow a RGB structure for 8-bits and 16-bits per channel
			switch (rhs->m_textureColorComponentsFormat)
			{
			case textureColorComponentsFormat::RED: l_internalFormat = DXGI_FORMAT_R8_UINT; break;
			case textureColorComponentsFormat::RG: l_internalFormat = DXGI_FORMAT_R8G8_UINT; break;
			case textureColorComponentsFormat::RGB: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UINT; break;
			case textureColorComponentsFormat::RGBA: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UINT; break;

			case textureColorComponentsFormat::R8: l_internalFormat = DXGI_FORMAT_R8_UINT; break;
			case textureColorComponentsFormat::RG8: l_internalFormat = DXGI_FORMAT_R8G8_UINT; break;
			case textureColorComponentsFormat::RGB8: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UINT; break;
			case textureColorComponentsFormat::RGBA8: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UINT; break;

			case textureColorComponentsFormat::R16: l_internalFormat = DXGI_FORMAT_R16_UINT; break;
			case textureColorComponentsFormat::RG16: l_internalFormat = DXGI_FORMAT_R16G16_UINT; break;
			case textureColorComponentsFormat::RGB16: l_internalFormat = DXGI_FORMAT_R16G16B16A16_UINT; break;
			case textureColorComponentsFormat::RGBA16: l_internalFormat = DXGI_FORMAT_R16G16B16A16_UINT; break;

			case textureColorComponentsFormat::R16F: l_internalFormat = DXGI_FORMAT_R16_FLOAT; break;
			case textureColorComponentsFormat::RG16F: l_internalFormat = DXGI_FORMAT_R16G16_FLOAT; break;
			case textureColorComponentsFormat::RGB16F: l_internalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
			case textureColorComponentsFormat::RGBA16F: l_internalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; break;

			case textureColorComponentsFormat::R32F: l_internalFormat = DXGI_FORMAT_R32_FLOAT; break;
			case textureColorComponentsFormat::RG32F: l_internalFormat = DXGI_FORMAT_R32G32_FLOAT; break;
			case textureColorComponentsFormat::RGB32F: l_internalFormat = DXGI_FORMAT_R32G32B32_FLOAT; break;
			case textureColorComponentsFormat::RGBA32F: l_internalFormat = DXGI_FORMAT_R32G32B32A32_FLOAT; break;

			case textureColorComponentsFormat::SRGB: break;
			case textureColorComponentsFormat::SRGBA: break;
			case textureColorComponentsFormat::SRGB8: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
			case textureColorComponentsFormat::SRGBA8: l_internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;

			case textureColorComponentsFormat::DEPTH_COMPONENT: break;
			}
		}

		D3D11_TEXTURE2D_DESC textureDesc;
		ZeroMemory(&textureDesc, sizeof(textureDesc));
		textureDesc.Height = rhs->m_textureHeight;
		textureDesc.Width = rhs->m_textureWidth;
		textureDesc.MipLevels = 0;
		textureDesc.ArraySize = 1;
		textureDesc.Format = l_internalFormat;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		textureDesc.CPUAccessFlags = 0;
		textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = -1;

		// Create the empty texture.
		ID3D11Texture2D* l_texture;
		ID3D11ShaderResourceView* l_textureView;

		HRESULT hResult;
		hResult = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateTexture2D(&textureDesc, NULL, &l_texture);
		if (FAILED(hResult))
		{
			InnoLogSystem::printLog("Error: DXRenderingSystem: can't create texture!");
			return nullptr;
		}

		unsigned int rowPitch;
		rowPitch = (rhs->m_textureWidth * 4) * sizeof(unsigned char);
		DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->UpdateSubresource(l_texture, 0, NULL, rhs->m_textureData[0], rowPitch, 0);

		// Setup the shader resource view description.
		// Create the shader resource view for the texture.
		hResult = DXRenderingSystemSingletonComponent::getInstance().m_device->CreateShaderResourceView(l_texture, &srvDesc, &l_textureView);
		if (FAILED(hResult))
		{
			InnoLogSystem::printLog("Error: DXRenderingSystem: can't create shader resource view for texture!");
			return nullptr;
		}

		// Generate mipmaps for this texture.
		DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->GenerateMips(l_ptr->m_textureView);

		l_ptr->m_objectStatus = objectStatus::ALIVE;
		rhs->m_objectStatus = objectStatus::ALIVE;

		return l_ptr;
	}
}

DXMeshDataComponent* DXRenderingSystem::addDXMeshDataComponent(EntityID rhs)
{
	DXMeshDataComponent* newMesh = InnoMemorySystem::spawn<DXMeshDataComponent>();
	newMesh->m_parentEntity = rhs;
	auto l_meshMap = &DXRenderingSystemSingletonComponent::getInstance().m_meshMap;
	l_meshMap->emplace(std::pair<EntityID, DXMeshDataComponent*>(rhs, newMesh));
	return newMesh;
}

DXTextureDataComponent* DXRenderingSystem::addDXTextureDataComponent(EntityID rhs)
{
	DXTextureDataComponent* newTexture = InnoMemorySystem::spawn<DXTextureDataComponent>();
	newTexture->m_parentEntity = rhs;
	auto l_textureMap = &DXRenderingSystemSingletonComponent::getInstance().m_textureMap;
	l_textureMap->emplace(std::pair<EntityID, DXTextureDataComponent*>(rhs, newTexture));
	return newTexture;
}

DXMeshDataComponent * DXRenderingSystem::getDXMeshDataComponent(EntityID rhs)
{
	auto result = DXRenderingSystemSingletonComponent::getInstance().m_meshMap.find(rhs);
	if (result != DXRenderingSystemSingletonComponent::getInstance().m_meshMap.end())
	{
		return result->second;
	}
	else
	{
		return nullptr;
	}
}

DXTextureDataComponent * DXRenderingSystem::getDXTextureDataComponent(EntityID rhs)
{
	auto result = DXRenderingSystemSingletonComponent::getInstance().m_textureMap.find(rhs);
	if (result != DXRenderingSystemSingletonComponent::getInstance().m_textureMap.end())
	{
		return result->second;
	}
	else
	{
		return nullptr;
	}
}

void DXRenderingSystem::updateFinalBlendPass()
{
	// Set the vertex and pixel shaders that will be used to render this triangle.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->VSSetShader(DXFinalRenderPassSingletonComponent::getInstance().m_vertexShader, NULL, 0);
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->PSSetShader(DXFinalRenderPassSingletonComponent::getInstance().m_pixelShader, NULL, 0);

	// Set the sampler state in the pixel shader.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->PSSetSamplers(0, 1, &DXFinalRenderPassSingletonComponent::getInstance().m_sampleState);
	
	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set the vertex input layout.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->IASetInputLayout(DXFinalRenderPassSingletonComponent::getInstance().m_layout);

	auto l_mainCamera = GameSystemSingletonComponent::getInstance().m_cameraComponents[0];
	mat4 p = l_mainCamera->m_projectionMatrix;
	mat4 r = InnoMath::getInvertRotationMatrix(InnoGameSystem::getTransformComponent(l_mainCamera->m_parentEntity)->m_globalTransformVector.m_rot);
	mat4 t = InnoMath::getInvertTranslationMatrix(InnoGameSystem::getTransformComponent(l_mainCamera->m_parentEntity)->m_globalTransformVector.m_pos);
	mat4 v = p * r * t;

	for (auto& l_visibleComponent : RenderingSystemSingletonComponent::getInstance().m_inFrustumVisibleComponents)
	{
		if (l_visibleComponent->m_visiblilityType == visiblilityType::STATIC_MESH)
		{
			// draw each graphic data of visibleComponent
			for (auto& l_graphicData : l_visibleComponent->m_modelMap)
			{
					// Set the shader parameters that it will use for rendering.

					mat4 m = InnoGameSystem::getTransformComponent(l_visibleComponent->m_parentEntity)->m_globalTransformMatrix.m_transformationMat;
					auto mvp = v * m;

					updateShaderParameter(shaderType::VERTEX, DXFinalRenderPassSingletonComponent::getInstance().m_matrixBuffer, &mvp);

					// draw meshes
					drawMesh(l_graphicData.first);
			}
		}
	}
}

void DXRenderingSystem::drawMesh(EntityID rhs)
{
	auto l_MDC = InnoAssetSystem::getMeshDataComponent(rhs);
	if (l_MDC)
	{
		drawMesh(l_MDC);
	}
}

void DXRenderingSystem::drawMesh(MeshDataComponent * MDC)
{
	auto l_DXMDC = getDXMeshDataComponent(MDC->m_parentEntity);
	if (l_DXMDC)
	{
		if (MDC->m_objectStatus == objectStatus::ALIVE && l_DXMDC->m_objectStatus == objectStatus::ALIVE)
		{
			unsigned int stride;
			unsigned int offset;

			// Set vertex buffer stride and offset.
			stride = sizeof(Vertex);
			offset = 0;

			// Set the vertex buffer to active in the input assembler so it can be rendered.
			DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->IASetVertexBuffers(0, 1, &l_DXMDC->m_vertexBuffer, &stride, &offset);

			// Set the index buffer to active in the input assembler so it can be rendered.
			DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->IASetIndexBuffer(l_DXMDC->m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

			// Render the triangle.
			DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->DrawIndexed((UINT)MDC->m_indicesSize, 0, 0);
		}
	}
}

void DXRenderingSystem::updateShaderParameter(shaderType shaderType, ID3D11Buffer * matrixBuffer, mat4* parameterValue)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	DirectX::XMMATRIX* dataPtr;
	unsigned int bufferNumber;

	//parameterValue = DirectX::XMMatrixTranspose(parameterValue);

	// Lock the constant buffer so it can be written to.
	result = DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->Map(matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		InnoLogSystem::printLog("Error: DXRenderingSystem: can't lock the shader matrix buffer!");
		m_objectStatus = objectStatus::STANDBY;
		return;
	}

	dataPtr = (DirectX::XMMATRIX*)mappedResource.pData;

	*dataPtr = *(DirectX::XMMATRIX*)parameterValue;

	// Unlock the constant buffer.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->Unmap(matrixBuffer, 0);

	bufferNumber = 0;

	switch (shaderType)
	{
	case shaderType::VERTEX:
		DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->VSSetConstantBuffers(bufferNumber, 1, &matrixBuffer);
		break;
	case shaderType::GEOMETRY:
		DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->GSSetConstantBuffers(bufferNumber, 1, &matrixBuffer);
		break;
	case shaderType::FRAGMENT:
		DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->PSSetConstantBuffers(bufferNumber, 1, &matrixBuffer);
		break;
	default:
		break;
	}
}

void DXRenderingSystem::beginScene(float r, float g, float b, float a)
{
	float color[4];

	// Setup the color to clear the buffer to.
	color[0] = r;
	color[1] = g;
	color[2] = b;
	color[3] = a;

	// Clear the back buffer.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->ClearRenderTargetView(DXRenderingSystemSingletonComponent::getInstance().m_renderTargetView, color);

	// Clear the depth buffer.
	DXRenderingSystemSingletonComponent::getInstance().m_deviceContext->ClearDepthStencilView(DXRenderingSystemSingletonComponent::getInstance().m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void DXRenderingSystem::endScene()
{
	// Present the back buffer to the screen since rendering is complete.
	if (DXRenderingSystemSingletonComponent::getInstance().m_vsync_enabled)
	{
		// Lock to screen refresh rate.
		DXRenderingSystemSingletonComponent::getInstance().m_swapChain->Present(1, 0);
	}
	else
	{
		// Present as fast as possible.
		DXRenderingSystemSingletonComponent::getInstance().m_swapChain->Present(0, 0);
	}
}
