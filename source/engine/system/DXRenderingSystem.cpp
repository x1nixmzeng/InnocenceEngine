#include "DXRenderingSystem.h"

#include "../component/DXGeometryRenderPassComponent.h"
#include "../component/DXLightRenderPassComponent.h"
#include "../component/DXFinalRenderPassComponent.h"

#include "../component/WindowSystemComponent.h"
#include "../component/DXWindowSystemComponent.h"
#include "../component/RenderingSystemComponent.h"
#include "../component/DXRenderingSystemComponent.h"
#include "../component/FileSystemComponent.h"

#include "DXRenderingSystemUtilities.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "../component/GameSystemComponent.h"

#include "ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE DXRenderingSystemNS
{
	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;

	bool setup();
	bool terminate();

	bool initializeDefaultAssets();
	bool initializeGeometryPass();
	bool initializeLightPass();
	bool initializeFinalBlendPass();

	TextureDataDesc deferredPassTextureDesc = TextureDataDesc();
	D3D11_RENDER_TARGET_VIEW_DESC deferredPassRTVDesc = D3D11_RENDER_TARGET_VIEW_DESC();

	ShaderFilePaths m_GeometryPassShaderFilePaths = { "DX11//geometryPassCookTorranceVertex.sf" , "", "DX11//geometryPassCookTorrancePixel.sf" };
	ShaderFilePaths m_LightPassShaderFilePaths = { "DX11//lightPassCookTorranceVertex.sf" , "", "DX11//lightPassCookTorrancePixel.sf" };
	ShaderFilePaths m_FinalPassShaderFilePaths = { "DX11//finalBlendPassVertex.sf" , "", "DX11//finalBlendPassPixel.sf" };

	void prepareRenderingData();

	void updateGeometryPass();
	void updateLightPass();
	void updateFinalBlendPass();

	static WindowSystemComponent* g_WindowSystemComponent;
	static DXWindowSystemComponent* g_DXWindowSystemComponent;
	static DXRenderingSystemComponent* g_DXRenderingSystemComponent;

	mat4 m_CamProj;
	mat4 m_CamRot;
	mat4 m_CamTrans;
	mat4 m_CamRTP;

	struct GPassMeshCBufferData
	{
		mat4 m;
		mat4 vp;
		mat4 m_normalMat;
	};

	struct GPassTextureCBufferData
	{
		vec4 albedo;
		vec4 MRA;
		int useNormalTexture = true;
		int useAlbedoTexture = true;
		int useMetallicTexture = true;
		int useRoughnessTexture = true;
		int useAOTexture = true;
		int padding1 = true;
		int padding2 = true;
		int padding3 = true;
	};

	struct GPassRenderingDataPack
	{
		size_t indiceSize;
		GPassMeshCBufferData meshCBuffer;
		GPassTextureCBufferData textureCBuffer;
		DXMeshDataComponent* DXMDC;
		MeshPrimitiveTopology meshPrimitiveTopology;
		DXTextureDataComponent* normalDXTDC;
		DXTextureDataComponent* albedoDXTDC;
		DXTextureDataComponent* metallicDXTDC;
		DXTextureDataComponent* roughnessDXTDC;
		DXTextureDataComponent* AODXTDC;
	};

	std::queue<GPassRenderingDataPack> m_GPassRenderingDataQueue;

	struct LPassCBufferData
	{
		vec4 viewPos;
		vec4 lightDir;
		vec4 color;
	};

	LPassCBufferData m_LPassCBufferData;

	bool createPhysicalDevices();
	bool createSwapChain();
	bool createBackBuffer();
	bool createRasterizer();
}

bool DXRenderingSystemNS::createPhysicalDevices()
{
	HRESULT result;
	IDXGIFactory* m_factory;

	DXGI_ADAPTER_DESC adapterDesc;
	IDXGIAdapter* m_adapter;
	IDXGIOutput* m_adapterOutput;

	unsigned int numModes;
	unsigned long long stringLength;
	DXGI_MODE_DESC* displayModeList;

	// Create a DirectX graphics interface factory.
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&m_factory);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create DXGI factory!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Use the factory to create an adapter for the primary graphics interface (video card).
	result = m_factory->EnumAdapters(0, &m_adapter);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create video card adapter!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Enumerate the primary adapter output (monitor).
	result = m_adapter->EnumOutputs(0, &m_adapterOutput);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create monitor adapter!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	result = m_adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't get DXGI_FORMAT_R8G8B8A8_UNORM fitted monitor!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	displayModeList = new DXGI_MODE_DESC[numModes];

	// Now fill the display mode list structures.
	result = m_adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't fill the display mode list structures!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	for (unsigned int i = 0; i < numModes; i++)
	{
		if (displayModeList[i].Width == (unsigned int)g_WindowSystemComponent->m_windowResolution.x
			&&
			displayModeList[i].Height == (unsigned int)g_WindowSystemComponent->m_windowResolution.y
			)
		{
			g_DXRenderingSystemComponent->m_refreshRate.x = displayModeList[i].RefreshRate.Numerator;
			g_DXRenderingSystemComponent->m_refreshRate.y = displayModeList[i].RefreshRate.Denominator;
		}
	}

	// Get the adapter (video card) description.
	result = m_adapter->GetDesc(&adapterDesc);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't get the video card adapter description!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Store the dedicated video card memory in megabytes.
	g_DXRenderingSystemComponent->m_videoCardMemory = (int)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	if (wcstombs_s(&stringLength, g_DXRenderingSystemComponent->m_videoCardDescription, 128, adapterDesc.Description, 128) != 0)
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't convert the name of the video card to a character array!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Release the display mode list.
	delete[] displayModeList;
	displayModeList = 0;

	// Release the adapter output.
	m_adapterOutput->Release();
	m_adapterOutput = 0;

	// Release the adapter.
	m_adapter->Release();
	m_adapter = 0;

	// Release the factory.
	m_factory->Release();
	m_factory = 0;

	return true;
}

bool DXRenderingSystemNS::createSwapChain()
{
	HRESULT result;
	D3D_FEATURE_LEVEL featureLevel;

	// Initialize the swap chain description.
	ZeroMemory(&g_DXRenderingSystemComponent->m_swapChainDesc, sizeof(g_DXRenderingSystemComponent->m_swapChainDesc));

	// Set to a single back buffer.
	g_DXRenderingSystemComponent->m_swapChainDesc.BufferCount = 1;

	// Set the width and height of the back buffer.
	g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.Width = (UINT)g_WindowSystemComponent->m_windowResolution.x;
	g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.Height = (UINT)g_WindowSystemComponent->m_windowResolution.y;

	// Set regular 32-bit surface for the back buffer.
	g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Set the refresh rate of the back buffer.
	if (g_DXRenderingSystemComponent->m_vsync_enabled)
	{
		g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.RefreshRate.Numerator = g_DXRenderingSystemComponent->m_refreshRate.x;
		g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.RefreshRate.Denominator = g_DXRenderingSystemComponent->m_refreshRate.y;
	}
	else
	{
		g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	// Set the usage of the back buffer.
	g_DXRenderingSystemComponent->m_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Set the handle for the window to render to.
	g_DXRenderingSystemComponent->m_swapChainDesc.OutputWindow = g_DXWindowSystemComponent->m_hwnd;

	// Turn multisampling off.
	g_DXRenderingSystemComponent->m_swapChainDesc.SampleDesc.Count = 1;
	g_DXRenderingSystemComponent->m_swapChainDesc.SampleDesc.Quality = 0;

	// Set to full screen or windowed mode.
	if (g_WindowSystemComponent->m_fullScreen)
	{
		g_DXRenderingSystemComponent->m_swapChainDesc.Windowed = false;
	}
	else
	{
		g_DXRenderingSystemComponent->m_swapChainDesc.Windowed = true;
	}

	// Set the scan line ordering and scaling to unspecified.
	g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	g_DXRenderingSystemComponent->m_swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	g_DXRenderingSystemComponent->m_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	g_DXRenderingSystemComponent->m_swapChainDesc.Flags = 0;

	// Set the feature level to DirectX 11.
	featureLevel = D3D_FEATURE_LEVEL_11_0;

	// Create the swap chain, Direct3D device, and Direct3D device context.
	result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
		D3D11_SDK_VERSION, &g_DXRenderingSystemComponent->m_swapChainDesc, &g_DXRenderingSystemComponent->m_swapChain, &g_DXRenderingSystemComponent->m_device, NULL, &g_DXRenderingSystemComponent->m_deviceContext);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create the swap chain/D3D device/D3D device context!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	return true;
}

bool DXRenderingSystemNS::createBackBuffer()
{
	HRESULT result;

	// Get the pointer to the back buffer.
	result = g_DXRenderingSystemComponent->m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&g_DXRenderingSystemComponent->m_renderTargetTexture);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't get back buffer pointer!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Create the render target view with the back buffer pointer.
	result = g_DXRenderingSystemComponent->m_device->CreateRenderTargetView(g_DXRenderingSystemComponent->m_renderTargetTexture, NULL, &g_DXRenderingSystemComponent->m_renderTargetView);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create render target view!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Release pointer to the back buffer as we no longer need it.
	g_DXRenderingSystemComponent->m_renderTargetTexture->Release();
	g_DXRenderingSystemComponent->m_renderTargetTexture = 0;

	// Initialize the description of the depth buffer.
	ZeroMemory(&g_DXRenderingSystemComponent->m_depthTextureDesc, sizeof(g_DXRenderingSystemComponent->m_depthTextureDesc));

	// Set up the description of the depth buffer.
	g_DXRenderingSystemComponent->m_depthTextureDesc.Width = (UINT)g_WindowSystemComponent->m_windowResolution.x;
	g_DXRenderingSystemComponent->m_depthTextureDesc.Height = (UINT)g_WindowSystemComponent->m_windowResolution.y;
	g_DXRenderingSystemComponent->m_depthTextureDesc.MipLevels = 1;
	g_DXRenderingSystemComponent->m_depthTextureDesc.ArraySize = 1;
	g_DXRenderingSystemComponent->m_depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	g_DXRenderingSystemComponent->m_depthTextureDesc.SampleDesc.Count = 1;
	g_DXRenderingSystemComponent->m_depthTextureDesc.SampleDesc.Quality = 0;
	g_DXRenderingSystemComponent->m_depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	g_DXRenderingSystemComponent->m_depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	g_DXRenderingSystemComponent->m_depthTextureDesc.CPUAccessFlags = 0;
	g_DXRenderingSystemComponent->m_depthTextureDesc.MiscFlags = 0;

	// Create the texture for the depth buffer using the filled out description.
	result = g_DXRenderingSystemComponent->m_device->CreateTexture2D(&g_DXRenderingSystemComponent->m_depthTextureDesc, NULL, &g_DXRenderingSystemComponent->m_depthStencilTexture);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create the texture for the depth buffer!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Initialize the description of the stencil state.
	ZeroMemory(&g_DXRenderingSystemComponent->m_depthStencilDesc, sizeof(g_DXRenderingSystemComponent->m_depthStencilDesc));

	// Set up the description of the stencil state.
	g_DXRenderingSystemComponent->m_depthStencilDesc.DepthEnable = true;
	g_DXRenderingSystemComponent->m_depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	g_DXRenderingSystemComponent->m_depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	g_DXRenderingSystemComponent->m_depthStencilDesc.StencilEnable = true;
	g_DXRenderingSystemComponent->m_depthStencilDesc.StencilReadMask = 0xFF;
	g_DXRenderingSystemComponent->m_depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	g_DXRenderingSystemComponent->m_depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	g_DXRenderingSystemComponent->m_depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	g_DXRenderingSystemComponent->m_depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	g_DXRenderingSystemComponent->m_depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	g_DXRenderingSystemComponent->m_depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	g_DXRenderingSystemComponent->m_depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	g_DXRenderingSystemComponent->m_depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	g_DXRenderingSystemComponent->m_depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create the depth stencil state.
	result = g_DXRenderingSystemComponent->m_device->CreateDepthStencilState(
		&g_DXRenderingSystemComponent->m_depthStencilDesc,
		&g_DXRenderingSystemComponent->m_depthStencilState);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create the depth stencil state!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Set the depth stencil state.
	g_DXRenderingSystemComponent->m_deviceContext->OMSetDepthStencilState(
		g_DXRenderingSystemComponent->m_depthStencilState, 1);

	// Initialize the depth stencil view.
	ZeroMemory(&g_DXRenderingSystemComponent->m_depthStencilViewDesc, sizeof(
		g_DXRenderingSystemComponent->m_depthStencilViewDesc));

	// Set up the depth stencil view description.
	g_DXRenderingSystemComponent->m_depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	g_DXRenderingSystemComponent->m_depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	g_DXRenderingSystemComponent->m_depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	result = g_DXRenderingSystemComponent->m_device->CreateDepthStencilView(
		g_DXRenderingSystemComponent->m_depthStencilTexture,
		&g_DXRenderingSystemComponent->m_depthStencilViewDesc,
		&g_DXRenderingSystemComponent->m_depthStencilView);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create the depth stencil view!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Bind the render target view and depth stencil buffer to the output render pipeline.
	g_DXRenderingSystemComponent->m_deviceContext->OMSetRenderTargets(
		1,
		&g_DXRenderingSystemComponent->m_renderTargetView,
		g_DXRenderingSystemComponent->m_depthStencilView);

	return true;
}

bool DXRenderingSystemNS::createRasterizer()
{
	HRESULT result;

	// Setup the raster description which will determine how and what polygons will be drawn.
	g_DXRenderingSystemComponent->m_rasterDescForward.AntialiasedLineEnable = false;
	g_DXRenderingSystemComponent->m_rasterDescForward.CullMode = D3D11_CULL_NONE;
	g_DXRenderingSystemComponent->m_rasterDescForward.DepthBias = 0;
	g_DXRenderingSystemComponent->m_rasterDescForward.DepthBiasClamp = 0.0f;
	g_DXRenderingSystemComponent->m_rasterDescForward.DepthClipEnable = true;
	g_DXRenderingSystemComponent->m_rasterDescForward.FillMode = D3D11_FILL_SOLID;
	g_DXRenderingSystemComponent->m_rasterDescForward.FrontCounterClockwise = true;
	g_DXRenderingSystemComponent->m_rasterDescForward.MultisampleEnable = false;
	g_DXRenderingSystemComponent->m_rasterDescForward.ScissorEnable = false;
	g_DXRenderingSystemComponent->m_rasterDescForward.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state for forward pass
	result = g_DXRenderingSystemComponent->m_device->CreateRasterizerState(
		&g_DXRenderingSystemComponent->m_rasterDescForward,
		&g_DXRenderingSystemComponent->m_rasterStateForward);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create the rasterizer state for forward pass!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	g_DXRenderingSystemComponent->m_rasterDescDeferred.AntialiasedLineEnable = false;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.CullMode = D3D11_CULL_NONE;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.DepthBias = 0;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.DepthBiasClamp = 0.0f;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.DepthClipEnable = true;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.FillMode = D3D11_FILL_SOLID;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.FrontCounterClockwise = false;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.MultisampleEnable = false;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.ScissorEnable = false;
	g_DXRenderingSystemComponent->m_rasterDescDeferred.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state for deferred pass
	result = g_DXRenderingSystemComponent->m_device->CreateRasterizerState(
		&g_DXRenderingSystemComponent->m_rasterDescDeferred,
		&g_DXRenderingSystemComponent->m_rasterStateDeferred);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "DXRenderingSystem: can't create the rasterizer state for deferred pass!");
		m_objectStatus = ObjectStatus::STANDBY;
		return false;
	}

	// Setup the viewport for rendering.
	g_DXRenderingSystemComponent->m_viewport.Width =
		(float)g_WindowSystemComponent->m_windowResolution.x;
	g_DXRenderingSystemComponent->m_viewport.Height =
		(float)g_WindowSystemComponent->m_windowResolution.y;
	g_DXRenderingSystemComponent->m_viewport.MinDepth = 0.0f;
	g_DXRenderingSystemComponent->m_viewport.MaxDepth = 1.0f;
	g_DXRenderingSystemComponent->m_viewport.TopLeftX = 0.0f;
	g_DXRenderingSystemComponent->m_viewport.TopLeftY = 0.0f;

	return true;
}

bool DXRenderingSystemNS::setup()
{
	g_WindowSystemComponent = &WindowSystemComponent::get();
	g_DXWindowSystemComponent = &DXWindowSystemComponent::get();
	g_DXRenderingSystemComponent = &DXRenderingSystemComponent::get();

	bool result = true;
	result = result && createPhysicalDevices();
	result = result && createSwapChain();
	result = result && createBackBuffer();
	result = result && createRasterizer();

	// Setup the description of the deferred pass.
	deferredPassTextureDesc.textureUsageType = TextureUsageType::RENDER_TARGET;
	deferredPassTextureDesc.textureColorComponentsFormat = TextureColorComponentsFormat::RGBA16F;
	deferredPassTextureDesc.texturePixelDataFormat = TexturePixelDataFormat::RGBA;
	deferredPassTextureDesc.textureMinFilterMethod = TextureFilterMethod::NEAREST;
	deferredPassTextureDesc.textureMagFilterMethod = TextureFilterMethod::NEAREST;
	deferredPassTextureDesc.textureWrapMethod = TextureWrapMethod::CLAMP_TO_EDGE;
	deferredPassTextureDesc.textureWidth = g_WindowSystemComponent->m_windowResolution.x;
	deferredPassTextureDesc.textureHeight = g_WindowSystemComponent->m_windowResolution.y;
	deferredPassTextureDesc.texturePixelDataType = TexturePixelDataType::FLOAT;

	deferredPassRTVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	deferredPassRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	deferredPassRTVDesc.Texture2D.MipSlice = 0;

	m_objectStatus = ObjectStatus::ALIVE;
	return result;
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::setup()
{
	return DXRenderingSystemNS::setup();
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::initialize()
{
	DXRenderingSystemNS::initializeDefaultAssets();
	DXRenderingSystemNS::initializeGeometryPass();
	DXRenderingSystemNS::initializeLightPass();
	DXRenderingSystemNS::initializeFinalBlendPass();

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DXRenderingSystem has been initialized.");
	return true;
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::update()
{
	if (FileSystemComponent::get().m_uninitializedMeshComponents.size() > 0)
	{
		MeshDataComponent* l_meshDataComponent;
		if (FileSystemComponent::get().m_uninitializedMeshComponents.tryPop(l_meshDataComponent))
		{
			auto l_initializedDXMDC = DXRenderingSystemNS::generateDXMeshDataComponent(l_meshDataComponent);
		}
	}
	if (FileSystemComponent::get().m_uninitializedTextureComponents.size() > 0)
	{
		TextureDataComponent* l_textureDataComponent;
		if (FileSystemComponent::get().m_uninitializedTextureComponents.tryPop(l_textureDataComponent))
		{
			DXRenderingSystemNS::generateDXTextureDataComponent(l_textureDataComponent);
		}
	}
	// Clear the buffers to begin the scene.
	DXRenderingSystemNS::prepareRenderingData();

	DXRenderingSystemNS::updateGeometryPass();

	DXRenderingSystemNS::updateLightPass();

	DXRenderingSystemNS::updateFinalBlendPass();

	return true;
}

bool DXRenderingSystemNS::terminate()
{
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	if (g_DXRenderingSystemComponent->m_swapChain)
	{
		g_DXRenderingSystemComponent->m_swapChain->SetFullscreenState(false, NULL);
	}

	if (g_DXRenderingSystemComponent->m_rasterStateDeferred)
	{
		g_DXRenderingSystemComponent->m_rasterStateDeferred->Release();
		g_DXRenderingSystemComponent->m_rasterStateDeferred = 0;
	}

	if (g_DXRenderingSystemComponent->m_depthStencilView)
	{
		g_DXRenderingSystemComponent->m_depthStencilView->Release();
		g_DXRenderingSystemComponent->m_depthStencilView = 0;
	}

	if (g_DXRenderingSystemComponent->m_depthStencilState)
	{
		g_DXRenderingSystemComponent->m_depthStencilState->Release();
		g_DXRenderingSystemComponent->m_depthStencilState = 0;
	}

	if (g_DXRenderingSystemComponent->m_depthStencilTexture)
	{
		g_DXRenderingSystemComponent->m_depthStencilTexture->Release();
		g_DXRenderingSystemComponent->m_depthStencilTexture = 0;
	}

	if (g_DXRenderingSystemComponent->m_renderTargetView)
	{
		g_DXRenderingSystemComponent->m_renderTargetView->Release();
		g_DXRenderingSystemComponent->m_renderTargetView = 0;
	}

	if (g_DXRenderingSystemComponent->m_deviceContext)
	{
		g_DXRenderingSystemComponent->m_deviceContext->Release();
		g_DXRenderingSystemComponent->m_deviceContext = 0;
	}

	if (g_DXRenderingSystemComponent->m_device)
	{
		g_DXRenderingSystemComponent->m_device->Release();
		g_DXRenderingSystemComponent->m_device = 0;
	}

	if (g_DXRenderingSystemComponent->m_swapChain)
	{
		g_DXRenderingSystemComponent->m_swapChain->Release();
		g_DXRenderingSystemComponent->m_swapChain = 0;
	}

	m_objectStatus = ObjectStatus::SHUTDOWN;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "DXRenderingSystem has been terminated.");
	return true;
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::terminate()
{
	return DXRenderingSystemNS::terminate();
}

ObjectStatus DXRenderingSystem::getStatus()
{
	return DXRenderingSystemNS::m_objectStatus;
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::resize()
{
	return true;
}

bool  DXRenderingSystemNS::initializeDefaultAssets()
{
	auto l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(MeshShapeType::LINE);
	g_DXRenderingSystemComponent->m_UnitLineDXMDC = generateDXMeshDataComponent(l_MDC);

	l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(MeshShapeType::QUAD);
	g_DXRenderingSystemComponent->m_UnitQuadDXMDC = generateDXMeshDataComponent(l_MDC);

	l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(MeshShapeType::CUBE);
	g_DXRenderingSystemComponent->m_UnitCubeDXMDC = generateDXMeshDataComponent(l_MDC);

	l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(MeshShapeType::SPHERE);
	g_DXRenderingSystemComponent->m_UnitSphereDXMDC = generateDXMeshDataComponent(l_MDC);

	g_DXRenderingSystemComponent->m_basicNormalDXTDC = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(TextureUsageType::NORMAL));
	g_DXRenderingSystemComponent->m_basicAlbedoDXTDC = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(TextureUsageType::ALBEDO));
	g_DXRenderingSystemComponent->m_basicMetallicDXTDC = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(TextureUsageType::METALLIC));
	g_DXRenderingSystemComponent->m_basicRoughnessDXTDC = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(TextureUsageType::ROUGHNESS));
	g_DXRenderingSystemComponent->m_basicAODXTDC = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(TextureUsageType::AMBIENT_OCCLUSION));

	g_DXRenderingSystemComponent->m_iconTemplate_OBJ = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(FileExplorerIconType::OBJ));
	g_DXRenderingSystemComponent->m_iconTemplate_PNG = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(FileExplorerIconType::PNG));
	g_DXRenderingSystemComponent->m_iconTemplate_SHADER = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(FileExplorerIconType::SHADER));
	g_DXRenderingSystemComponent->m_iconTemplate_UNKNOWN = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(FileExplorerIconType::UNKNOWN));

	g_DXRenderingSystemComponent->m_iconTemplate_DirectionalLight = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(WorldEditorIconType::DIRECTIONAL_LIGHT));
	g_DXRenderingSystemComponent->m_iconTemplate_PointLight = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(WorldEditorIconType::POINT_LIGHT));
	g_DXRenderingSystemComponent->m_iconTemplate_SphereLight = generateDXTextureDataComponent(g_pCoreSystem->getAssetSystem()->getTextureDataComponent(WorldEditorIconType::SPHERE_LIGHT));

	return true;
}

bool  DXRenderingSystemNS::initializeGeometryPass()
{
	DXGeometryRenderPassComponent::get().m_DXRPC = addDXRenderPassComponent(8, deferredPassRTVDesc, deferredPassTextureDesc);
	DXGeometryRenderPassComponent::get().m_DXSPC = g_pCoreSystem->getMemorySystem()->spawn<DXShaderProgramComponent>();

	DXGeometryRenderPassComponent::get().m_DXSPC->m_vertexShaderCBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_vertexShaderCBufferDesc.ByteWidth = sizeof(GPassMeshCBufferData);
	DXGeometryRenderPassComponent::get().m_DXSPC->m_vertexShaderCBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_vertexShaderCBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_vertexShaderCBufferDesc.MiscFlags = 0;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_vertexShaderCBufferDesc.StructureByteStride = 0;

	DXGeometryRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.ByteWidth = sizeof(GPassTextureCBufferData);
	DXGeometryRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.MiscFlags = 0;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.StructureByteStride = 0;

	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.MipLODBias = 0.0f;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.MaxAnisotropy = 1;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[0] = 0;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[1] = 0;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[2] = 0;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[3] = 0;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.MinLOD = 0;
	DXGeometryRenderPassComponent::get().m_DXSPC->m_samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	return initializeDXShaderProgramComponent(DXGeometryRenderPassComponent::get().m_DXSPC, m_GeometryPassShaderFilePaths);
}

bool  DXRenderingSystemNS::initializeLightPass()
{
	DXLightRenderPassComponent::get().m_DXRPC = addDXRenderPassComponent(1, deferredPassRTVDesc, deferredPassTextureDesc);
	DXLightRenderPassComponent::get().m_DXSPC = g_pCoreSystem->getMemorySystem()->spawn<DXShaderProgramComponent>();

	DXLightRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	DXLightRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.ByteWidth = sizeof(LPassCBufferData);
	DXLightRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	DXLightRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	DXLightRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.MiscFlags = 0;
	DXLightRenderPassComponent::get().m_DXSPC->m_pixelShaderCBufferDesc.StructureByteStride = 0;

	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.MipLODBias = 0.0f;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.MaxAnisotropy = 1;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[0] = 0;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[1] = 0;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[2] = 0;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[3] = 0;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.MinLOD = 0;
	DXLightRenderPassComponent::get().m_DXSPC->m_samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	return initializeDXShaderProgramComponent(DXLightRenderPassComponent::get().m_DXSPC, m_LightPassShaderFilePaths);
}

bool DXRenderingSystemNS::initializeFinalBlendPass()
{
	DXFinalRenderPassComponent::get().m_DXSPC = g_pCoreSystem->getMemorySystem()->spawn<DXShaderProgramComponent>();

	// Create a texture sampler state description.
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.MipLODBias = 0.0f;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.MaxAnisotropy = 1;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[0] = 0;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[1] = 0;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[2] = 0;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.BorderColor[3] = 0;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.MinLOD = 0;
	DXFinalRenderPassComponent::get().m_DXSPC->m_samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	return initializeDXShaderProgramComponent(DXFinalRenderPassComponent::get().m_DXSPC, m_FinalPassShaderFilePaths);;
}

void DXRenderingSystemNS::prepareRenderingData()
{
	// camera and light
	auto l_mainCamera = GameSystemComponent::get().m_CameraComponents[0];
	auto l_mainCameraTransformComponent = g_pCoreSystem->getGameSystem()->get<TransformComponent>(l_mainCamera->m_parentEntity);
	auto l_directionalLight = GameSystemComponent::get().m_DirectionalLightComponents[0];
	auto l_directionalLightTransformComponent = g_pCoreSystem->getGameSystem()->get<TransformComponent>(l_directionalLight->m_parentEntity);

	auto l_p = l_mainCamera->m_projectionMatrix;
	auto l_r =
		InnoMath::getInvertRotationMatrix(
			l_mainCameraTransformComponent->m_globalTransformVector.m_rot
		);
	auto l_t =
		InnoMath::getInvertTranslationMatrix(
			l_mainCameraTransformComponent->m_globalTransformVector.m_pos
		);

	m_CamProj = l_p;
	m_CamRot = l_r;
	m_CamTrans = l_t;
	m_CamRTP = l_p * l_r * l_t;

	m_LPassCBufferData.viewPos = l_mainCameraTransformComponent->m_globalTransformVector.m_pos;
	m_LPassCBufferData.lightDir = InnoMath::getDirection(direction::BACKWARD, l_directionalLightTransformComponent->m_globalTransformVector.m_rot);

	m_LPassCBufferData.color = l_directionalLight->m_color;

	for (auto& l_renderDataPack : RenderingSystemComponent::get().m_renderDataPack)
	{
		auto l_DXMDC = getDXMeshDataComponent(l_renderDataPack.MDC->m_parentEntity);
		if (l_DXMDC && l_DXMDC->m_objectStatus == ObjectStatus::ALIVE)
		{
			GPassRenderingDataPack l_renderingDataPack;

			l_renderingDataPack.indiceSize = l_renderDataPack.MDC->m_indicesSize;
			l_renderingDataPack.meshPrimitiveTopology = l_renderDataPack.MDC->m_meshPrimitiveTopology;
			l_renderingDataPack.meshCBuffer.m = l_renderDataPack.m;
			l_renderingDataPack.meshCBuffer.vp = m_CamRTP;
			l_renderingDataPack.meshCBuffer.m_normalMat = l_renderDataPack.normalMat;
			l_renderingDataPack.DXMDC = l_DXMDC;

			auto l_material = l_renderDataPack.material;
			// any normal?
			auto l_TDC = l_material->m_texturePack.m_normalTDC.second;
			if (l_TDC && l_TDC->m_objectStatus == ObjectStatus::ALIVE)
			{
				l_renderingDataPack.normalDXTDC = getDXTextureDataComponent(l_TDC->m_parentEntity);
			}
			else
			{
				l_renderingDataPack.textureCBuffer.useNormalTexture = false;
			}
			// any albedo?
			l_TDC = l_material->m_texturePack.m_albedoTDC.second;
			if (l_TDC && l_TDC->m_objectStatus == ObjectStatus::ALIVE)
			{
				l_renderingDataPack.albedoDXTDC = getDXTextureDataComponent(l_TDC->m_parentEntity);
			}
			else
			{
				l_renderingDataPack.textureCBuffer.useAlbedoTexture = false;
			}
			// any metallic?
			l_TDC = l_material->m_texturePack.m_metallicTDC.second;
			if (l_TDC && l_TDC->m_objectStatus == ObjectStatus::ALIVE)
			{
				l_renderingDataPack.metallicDXTDC = getDXTextureDataComponent(l_TDC->m_parentEntity);
			}
			else
			{
				l_renderingDataPack.textureCBuffer.useMetallicTexture = false;
			}
			// any roughness?
			l_TDC = l_material->m_texturePack.m_roughnessTDC.second;
			if (l_TDC && l_TDC->m_objectStatus == ObjectStatus::ALIVE)
			{
				l_renderingDataPack.roughnessDXTDC = getDXTextureDataComponent(l_TDC->m_parentEntity);
			}
			else
			{
				l_renderingDataPack.textureCBuffer.useRoughnessTexture = false;
			}
			// any ao?
			l_TDC = l_material->m_texturePack.m_roughnessTDC.second;
			if (l_TDC && l_TDC->m_objectStatus == ObjectStatus::ALIVE)
			{
				l_renderingDataPack.AODXTDC = getDXTextureDataComponent(l_TDC->m_parentEntity);
			}
			else
			{
				l_renderingDataPack.textureCBuffer.useAOTexture = false;
			}

			l_renderingDataPack.textureCBuffer.albedo = vec4(
				l_material->m_meshCustomMaterial.albedo_r,
				l_material->m_meshCustomMaterial.albedo_g,
				l_material->m_meshCustomMaterial.albedo_b,
				1.0f
			);
			l_renderingDataPack.textureCBuffer.MRA = vec4(
				l_material->m_meshCustomMaterial.metallic,
				l_material->m_meshCustomMaterial.roughness,
				l_material->m_meshCustomMaterial.ao,
				1.0f
			);

			m_GPassRenderingDataQueue.push(l_renderingDataPack);
		}
	}
}

void DXRenderingSystemNS::updateGeometryPass()
{
	// Set Rasterizer State
	g_DXRenderingSystemComponent->m_deviceContext->RSSetState(
		g_DXRenderingSystemComponent->m_rasterStateForward);

	activateDXShaderProgramComponent(DXGeometryRenderPassComponent::get().m_DXSPC);

	// Set the render buffers to be the render target.
	// Bind the render target view array and depth stencil buffer to the output render pipeline.
	g_DXRenderingSystemComponent->m_deviceContext->OMSetRenderTargets(
		(unsigned int)DXGeometryRenderPassComponent::get().m_DXRPC->m_renderTargetViews.size(),
		&DXGeometryRenderPassComponent::get().m_DXRPC->m_renderTargetViews[0],
		DXGeometryRenderPassComponent::get().m_DXRPC->m_depthStencilView);

	// Set the viewport.
	g_DXRenderingSystemComponent->m_deviceContext->RSSetViewports(
		1,
		&DXGeometryRenderPassComponent::get().m_DXRPC->m_viewport);

	// Clear the render buffers.
	for (auto i : DXGeometryRenderPassComponent::get().m_DXRPC->m_renderTargetViews)
	{
		cleanRTV(vec4(0.0f, 0.0f, 0.0f, 0.0f), i);
	}
	cleanDSV(DXGeometryRenderPassComponent::get().m_DXRPC->m_depthStencilView);

	// draw
	while (m_GPassRenderingDataQueue.size() > 0)
	{
		auto l_renderPack = m_GPassRenderingDataQueue.front();

		// Set the type of primitive that should be rendered from this vertex buffer.
		D3D_PRIMITIVE_TOPOLOGY l_primitiveTopology;

		if (l_renderPack.meshPrimitiveTopology == MeshPrimitiveTopology::TRIANGLE)
		{
			l_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
		else
		{
			l_primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		}

		g_DXRenderingSystemComponent->m_deviceContext->IASetPrimitiveTopology(l_primitiveTopology);

		updateShaderParameter<GPassMeshCBufferData>(ShaderType::VERTEX, DXGeometryRenderPassComponent::get().m_DXSPC->m_vertexShaderCBuffer, &l_renderPack.meshCBuffer);
		updateShaderParameter<GPassTextureCBufferData>(ShaderType::FRAGMENT, DXGeometryRenderPassComponent::get().m_DXSPC->m_pixelShaderCBuffer, &l_renderPack.textureCBuffer);

		// bind to textures
		// any normal?
		if (l_renderPack.textureCBuffer.useNormalTexture)
		{
			g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(0, 1, &l_renderPack.normalDXTDC->m_SRV);
		}
		// any albedo?
		if (l_renderPack.textureCBuffer.useAlbedoTexture)
		{
			g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(1, 1, &l_renderPack.albedoDXTDC->m_SRV);
		}
		// any metallic?
		if (l_renderPack.textureCBuffer.useMetallicTexture)
		{
			g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(2, 1, &l_renderPack.metallicDXTDC->m_SRV);
		}
		// any roughness?
		if (l_renderPack.textureCBuffer.useRoughnessTexture)
		{
			g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(3, 1, &l_renderPack.roughnessDXTDC->m_SRV);
		}
		// any ao?
		if (l_renderPack.textureCBuffer.useAOTexture)
		{
			g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(4, 1, &l_renderPack.AODXTDC->m_SRV);
		}

		drawMesh(l_renderPack.indiceSize, l_renderPack.DXMDC);

		m_GPassRenderingDataQueue.pop();
	}
}

void DXRenderingSystemNS::updateLightPass()
{
	// Set Rasterizer State
	g_DXRenderingSystemComponent->m_deviceContext->RSSetState(
		g_DXRenderingSystemComponent->m_rasterStateDeferred);

	activateDXShaderProgramComponent(DXLightRenderPassComponent::get().m_DXSPC);

	g_DXRenderingSystemComponent->m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set the render buffers to be the render target.
	// Bind the render target view array and depth stencil buffer to the output render pipeline.
	g_DXRenderingSystemComponent->m_deviceContext->OMSetRenderTargets(
		(unsigned int)DXLightRenderPassComponent::get().m_DXRPC->m_renderTargetViews.size(),
		&DXLightRenderPassComponent::get().m_DXRPC->m_renderTargetViews[0],
		DXLightRenderPassComponent::get().m_DXRPC->m_depthStencilView);

	// Set the viewport.
	g_DXRenderingSystemComponent->m_deviceContext->RSSetViewports(
		1,
		&DXLightRenderPassComponent::get().m_DXRPC->m_viewport);

	// Clear the render buffers.
	for (auto i : DXLightRenderPassComponent::get().m_DXRPC->m_renderTargetViews)
	{
		cleanRTV(vec4(0.0f, 0.0f, 0.0f, 0.0f), i);
	}
	cleanDSV(DXLightRenderPassComponent::get().m_DXRPC->m_depthStencilView);

	auto l_LPassCBufferData = m_LPassCBufferData;

	updateShaderParameter<LPassCBufferData>(ShaderType::FRAGMENT, DXLightRenderPassComponent::get().m_DXSPC->m_pixelShaderCBuffer, &l_LPassCBufferData);

	// bind to previous pass render target textures
	g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(0, 1, &DXGeometryRenderPassComponent::get().m_DXRPC->m_DXTDCs[0]->m_SRV);
	g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(1, 1, &DXGeometryRenderPassComponent::get().m_DXRPC->m_DXTDCs[1]->m_SRV);
	g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(2, 1, &DXGeometryRenderPassComponent::get().m_DXRPC->m_DXTDCs[2]->m_SRV);

	// draw
	drawMesh(6, g_DXRenderingSystemComponent->m_UnitQuadDXMDC);
}

void DXRenderingSystemNS::updateFinalBlendPass()
{
	// Set Rasterizer State
	g_DXRenderingSystemComponent->m_deviceContext->RSSetState(
		g_DXRenderingSystemComponent->m_rasterStateDeferred);

	activateDXShaderProgramComponent(DXFinalRenderPassComponent::get().m_DXSPC);

	g_DXRenderingSystemComponent->m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set the render buffers to be the render target.
	// Bind the render target view array and depth stencil buffer to the output render pipeline.
	g_DXRenderingSystemComponent->m_deviceContext->OMSetRenderTargets(
		1,
		&g_DXRenderingSystemComponent->m_renderTargetView,
		g_DXRenderingSystemComponent->m_depthStencilView);

	// Set the viewport.
	g_DXRenderingSystemComponent->m_deviceContext->RSSetViewports(
		1,
		&g_DXRenderingSystemComponent->m_viewport);

	// Clear the render buffers.
	cleanRTV(vec4(0.0f, 0.0f, 0.0f, 0.0f), g_DXRenderingSystemComponent->m_renderTargetView);
	cleanDSV(g_DXRenderingSystemComponent->m_depthStencilView);

	// bind to previous pass render target textures
	g_DXRenderingSystemComponent->m_deviceContext->PSSetShaderResources(0, 1, &DXLightRenderPassComponent::get().m_DXRPC->m_DXTDCs[0]->m_SRV);

	// draw
	drawMesh(6, g_DXRenderingSystemComponent->m_UnitQuadDXMDC);
}