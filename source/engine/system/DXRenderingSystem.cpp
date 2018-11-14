#include "DXRenderingSystem.h"

#include <sstream>

#include "../component/DXGeometryRenderPassSingletonComponent.h"
#include "../component/DXFinalRenderPassSingletonComponent.h"

#include "../component/WindowSystemSingletonComponent.h"
#include "../component/DXWindowSystemSingletonComponent.h"
#include "../component/RenderingSystemSingletonComponent.h"
#include "../component/DXRenderingSystemSingletonComponent.h"
#include "../component/AssetSystemSingletonComponent.h"

#include "../component/MeshDataComponent.h"
#include "../component/TextureDataComponent.h"
#include "../component/DXMeshDataComponent.h"
#include "../component/DXTextureDataComponent.h"


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "../component/GameSystemSingletonComponent.h"

#include "ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE DXRenderingSystemNS
{
	objectStatus m_objectStatus = objectStatus::SHUTDOWN;

DXMeshDataComponent* initializeMeshDataComponent(MeshDataComponent* rhs);
DXTextureDataComponent* initializeTextureDataComponent(TextureDataComponent* rhs);

DXMeshDataComponent* addDXMeshDataComponent(EntityID rhs);
DXTextureDataComponent* addDXTextureDataComponent(EntityID rhs);
DXMeshDataComponent* getDXMeshDataComponent(EntityID rhs);
DXTextureDataComponent* getDXTextureDataComponent(EntityID rhs);

bool initializeGeometryPass();
bool initializeFinalBlendPass();

ID3D10Blob* loadShaderBuffer(shaderType shaderType, const std::wstring & shaderFilePath);
void OutputShaderErrorMessage(ID3D10Blob * errorMessage, HWND hwnd, const std::string & shaderFilename);

void prepareRenderingData();

void updateGeometryPass();
void updateFinalBlendPass();

void drawMesh(EntityID rhs);
void drawMesh(MeshDataComponent* MDC);
void drawMesh(size_t indicesSize, DXMeshDataComponent * DXMDC);

void updateShaderParameter(shaderType shaderType, ID3D11Buffer* matrixBuffer, mat4* parameterValue);
void cleanRTV(vec4 color, ID3D11RenderTargetView* RTV);
void cleanDSV(ID3D11DepthStencilView* DSV);
void swapBuffer();

static WindowSystemSingletonComponent* g_WindowSystemSingletonComponent;
static DXWindowSystemSingletonComponent* g_DXWindowSystemSingletonComponent;
static DXRenderingSystemSingletonComponent* g_DXRenderingSystemSingletonComponent;

mat4 m_CamProj;
mat4 m_CamRot;
mat4 m_CamTrans;
mat4 m_CamViewProj;

std::unordered_map<EntityID, DXMeshDataComponent*> m_initializedMeshComponents;
std::queue<std::tuple<size_t, mat4, DXMeshDataComponent*>> m_MeshComponentRenderingQueue;
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::setup()
{
	DXRenderingSystemNS::g_WindowSystemSingletonComponent = &WindowSystemSingletonComponent::getInstance();
	DXRenderingSystemNS::g_DXWindowSystemSingletonComponent = &DXWindowSystemSingletonComponent::getInstance();
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent = &DXRenderingSystemSingletonComponent::getInstance();

	HRESULT result;
	IDXGIFactory* m_factory;

	DXGI_ADAPTER_DESC adapterDesc;
	IDXGIAdapter* m_adapter;
	IDXGIOutput* m_adapterOutput;

	unsigned int numModes, i, numerator, denominator;
	unsigned long long stringLength;
	DXGI_MODE_DESC* displayModeList;

	int error;
	D3D_FEATURE_LEVEL featureLevel;


	// Create a DirectX graphics interface factory.
	result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&m_factory);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create DXGI factory!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Use the factory to create an adapter for the primary graphics interface (video card).
	result = m_factory->EnumAdapters(0, &m_adapter);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create video card adapter!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Enumerate the primary adapter output (monitor).
	result = m_adapter->EnumOutputs(0, &m_adapterOutput);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create monitor adapter!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	result = m_adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't get DXGI_FORMAT_R8G8B8A8_UNORM fitted monitor!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	displayModeList = new DXGI_MODE_DESC[numModes];

	// Now fill the display mode list structures.
	result = m_adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't fill the display mode list structures!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	for (i = 0; i < numModes; i++)
	{
		if (displayModeList[i].Width == (unsigned int)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.x
			&&
			displayModeList[i].Height == (unsigned int)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.y
			)
		{
			numerator = displayModeList[i].RefreshRate.Numerator;
			denominator = displayModeList[i].RefreshRate.Denominator;
		}
	}

	// Get the adapter (video card) description.
	result = m_adapter->GetDesc(&adapterDesc);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't get the video card adapter description!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Store the dedicated video card memory in megabytes.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_videoCardMemory = (int)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	// Convert the name of the video card to a character array and store it.
	error = wcstombs_s(&stringLength, DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_videoCardDescription, 128, adapterDesc.Description, 128);
	if (error != 0)
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't convert the name of the video card to a character array!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
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

	// Initialize the swap chain description.
	ZeroMemory(&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc, sizeof(DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc));

	// Set to a single back buffer.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferCount = 1;

	// Set the width and height of the back buffer.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.Width = (UINT)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.x;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.Height = (UINT)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.y;

	// Set regular 32-bit surface for the back buffer.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Set the refresh rate of the back buffer.
	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_vsync_enabled)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
	}
	else
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	// Set the usage of the back buffer.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Set the handle for the window to render to.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.OutputWindow = DXRenderingSystemNS::g_DXWindowSystemSingletonComponent->m_hwnd;

	// Turn multisampling off.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.SampleDesc.Count = 1;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.SampleDesc.Quality = 0;

	// Set to full screen or windowed mode.
	if (DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_fullScreen)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.Windowed = false;
	}
	else
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.Windowed = true;
	}

	// Set the scan line ordering and scaling to unspecified.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Discard the back buffer contents after presenting.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Don't set the advanced flags.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc.Flags = 0;

	// Set the feature level to DirectX 11.
	featureLevel = D3D_FEATURE_LEVEL_11_0;

	// Create the swap chain, Direct3D device, and Direct3D device context.
	result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
		D3D11_SDK_VERSION, &DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChainDesc, &DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain, &DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device, NULL, &DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create the swap chain/D3D device/D3D device context!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Get the pointer to the back buffer.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetTexture);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't get back buffer pointer!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Create the render target view with the back buffer pointer.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateRenderTargetView(DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetTexture, NULL, &DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetView);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create render target view!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Release pointer to the back buffer as we no longer need it.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetTexture->Release();
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetTexture = 0;

	// Initialize the description of the depth buffer.
	ZeroMemory(&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc, sizeof(DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc));

	// Set up the description of the depth buffer.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.Width = (UINT)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.x;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.Height = (UINT)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.y;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.MipLevels = 1;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.ArraySize = 1;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.SampleDesc.Count = 1;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.SampleDesc.Quality = 0;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.CPUAccessFlags = 0;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc.MiscFlags = 0;

	// Create the texture for the depth buffer using the filled out description.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateTexture2D(&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthTextureDesc, NULL, &DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilTexture);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create the texture for the depth buffer!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Initialize the description of the stencil state.
	ZeroMemory(&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc, sizeof(DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc));

	// Set up the description of the stencil state.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.DepthEnable = true;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.StencilEnable = true;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.StencilReadMask = 0xFF;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create the depth stencil state.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateDepthStencilState(
		&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilDesc, 
		&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilState);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create the depth stencil state!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Set the depth stencil state.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->OMSetDepthStencilState(
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilState, 1);

	// Initialize the depth stencil view.
	ZeroMemory(&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilViewDesc, sizeof(
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilViewDesc));

	// Set up the depth stencil view description.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateDepthStencilView(
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilTexture, 
		&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilViewDesc, 
		&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilView);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create the depth stencil view!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Bind the render target view and depth stencil buffer to the output render pipeline.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->OMSetRenderTargets(
		1, 
		&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetView, 
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilView);

	// Setup the raster description which will determine how and what polygons will be drawn.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.AntialiasedLineEnable = false;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.CullMode = D3D11_CULL_BACK;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.DepthBias = 0;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.DepthBiasClamp = 0.0f;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.DepthClipEnable = true;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.FillMode = D3D11_FILL_SOLID;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.FrontCounterClockwise = false;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.MultisampleEnable = false;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.ScissorEnable = false;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state from the description we just filled out.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateRasterizerState(
		&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterDesc, 
		&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterState);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create the rasterizer state!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Now set the rasterizer state.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->RSSetState(
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterState);

	// Setup the viewport for rendering.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_viewport.Width = 
		(float)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.x;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_viewport.Height = 
		(float)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.y;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_viewport.MinDepth = 0.0f;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_viewport.MaxDepth = 1.0f;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_viewport.TopLeftX = 0.0f;
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_viewport.TopLeftY = 0.0f;

	DXRenderingSystemNS::m_objectStatus = objectStatus::ALIVE;
	return true;
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::initialize()
{
	DXRenderingSystemNS::initializeGeometryPass();
	DXRenderingSystemNS::initializeFinalBlendPass();

	g_pCoreSystem->getLogSystem()->printLog("DXRenderingSystem has been initialized.");
	return true;
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::update()
{
	if (AssetSystemSingletonComponent::getInstance().m_uninitializedMeshComponents.size() > 0)
	{
		MeshDataComponent* l_meshDataComponent;
		if (AssetSystemSingletonComponent::getInstance().m_uninitializedMeshComponents.tryPop(l_meshDataComponent))
		{
			auto l_initializedDXMDC = DXRenderingSystemNS::initializeMeshDataComponent(l_meshDataComponent);
			DXRenderingSystemNS::m_initializedMeshComponents.emplace(l_initializedDXMDC->m_parentEntity, l_initializedDXMDC);
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
	DXRenderingSystemNS::prepareRenderingData();

	DXRenderingSystemNS::updateGeometryPass();

	DXRenderingSystemNS::updateFinalBlendPass();

	// Present the rendered scene to the screen.
	DXRenderingSystemNS::swapBuffer();

	return true;
}

INNO_SYSTEM_EXPORT bool DXRenderingSystem::terminate()
{
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain->SetFullscreenState(false, NULL);
	}

	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterState)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterState->Release();
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_rasterState = 0;
	}

	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilView)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilView->Release();
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilView = 0;
	}

	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilState)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilState->Release();
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilState = 0;
	}

	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilTexture)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilTexture->Release();
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilTexture = 0;
	}

	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetView)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetView->Release();
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetView = 0;
	}

	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->Release();
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext = 0;
	}

	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->Release();
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device = 0;
	}

	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain)
	{
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain->Release();
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain = 0;
	}

	DXRenderingSystemNS::m_objectStatus = objectStatus::SHUTDOWN;
	g_pCoreSystem->getLogSystem()->printLog("DXRenderingSystem has been terminated.");
	return true;
}

objectStatus DXRenderingSystem::getStatus()
{
	return DXRenderingSystemNS::m_objectStatus;
}

bool  DXRenderingSystemNS::initializeGeometryPass()
{
	HRESULT result;

	// Initialize the render target texture description.
	ZeroMemory(&DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc, 
		sizeof(DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc));

	// Setup the render target texture description.
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.Width = 
		(UINT)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.x;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.Height = 
		(UINT)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.y;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.MipLevels = 1;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.ArraySize = 1;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.SampleDesc.Count = 1;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.CPUAccessFlags = 0;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.MiscFlags = 0;

	auto l_renderTargetNumbers = 8;

	// Create the render target textures.
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextures.reserve(l_renderTargetNumbers);

	for (auto i = 0; i < l_renderTargetNumbers; i++)
	{
		DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextures.emplace_back();
		result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateTexture2D(
			&DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc, 
			NULL, 
			&DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextures[i]);
		if (FAILED(result))
		{
			g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create render target texture!");
			DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
			return false;
		}
	}

	// Setup the description of the render target view.
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViewDesc.Format = 
		DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.Format;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViewDesc.Texture2D.MipSlice = 0;

	// Create the render target views.
	DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViews.reserve(l_renderTargetNumbers);

	for (auto i = 0; i < l_renderTargetNumbers; i++)
	{
		DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViews.emplace_back();
		result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateRenderTargetView(
			DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextures[i], 
			&DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViewDesc,
			&DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViews[i]);
		if (FAILED(result))
		{
			g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create render target view!");
			DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
			return false;
		}
	}

	// Setup the description of the shader resource view.
	DXGeometryRenderPassSingletonComponent::getInstance().m_shaderResourceViewDesc.Format = 
		DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextureDesc.Format;
	DXGeometryRenderPassSingletonComponent::getInstance().m_shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	DXGeometryRenderPassSingletonComponent::getInstance().m_shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
	DXGeometryRenderPassSingletonComponent::getInstance().m_shaderResourceViewDesc.Texture2D.MipLevels = 1;

	// Create the shader resource views.
	DXGeometryRenderPassSingletonComponent::getInstance().m_shaderResourceViews.reserve(l_renderTargetNumbers);

	for (auto i = 0; i < l_renderTargetNumbers; i++)
	{
		DXGeometryRenderPassSingletonComponent::getInstance().m_shaderResourceViews.emplace_back();
		result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateShaderResourceView(
			DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetTextures[i], 
			&DXGeometryRenderPassSingletonComponent::getInstance().m_shaderResourceViewDesc,
			&DXGeometryRenderPassSingletonComponent::getInstance().m_shaderResourceViews[i]);
		if (FAILED(result))
		{
			g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create shader resource view!");
			DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
			return false;
		}
	}

	// Initialize the description of the depth buffer.
	ZeroMemory(&DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc, 
		sizeof(DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc));

	// Set up the description of the depth buffer.
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.Width = 
		(UINT)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.x;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.Height = 
		(UINT)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.y;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.MipLevels = 1;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.ArraySize = 1;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.SampleDesc.Count = 1;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.SampleDesc.Quality = 0;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.CPUAccessFlags = 0;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc.MiscFlags = 0;

	// Create the texture for the depth buffer using the filled out description.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateTexture2D(
		&DXGeometryRenderPassSingletonComponent::getInstance().m_depthBufferDesc, 
		NULL,
		&DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilBuffer);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create the texture for the depth buffer!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Initailze the depth stencil view description.
	ZeroMemory(&DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilViewDesc, 
		sizeof(DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilViewDesc));

	// Set up the depth stencil view description.
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateDepthStencilView(
		DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilBuffer, 
		&DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilViewDesc, 
		&DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilView);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create the depth stencil view!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Setup the viewport for rendering.
	DXGeometryRenderPassSingletonComponent::getInstance().m_viewport.Width = (float)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.x;
	DXGeometryRenderPassSingletonComponent::getInstance().m_viewport.Height = (float)DXRenderingSystemNS::g_WindowSystemSingletonComponent->m_windowResolution.y;
	DXGeometryRenderPassSingletonComponent::getInstance().m_viewport.MinDepth = 0.0f;
	DXGeometryRenderPassSingletonComponent::getInstance().m_viewport.MaxDepth = 1.0f;
	DXGeometryRenderPassSingletonComponent::getInstance().m_viewport.TopLeftX = 0.0f;
	DXGeometryRenderPassSingletonComponent::getInstance().m_viewport.TopLeftY = 0.0f;

	ID3D10Blob* l_errorMessage;
	ID3D10Blob* l_shaderBuffer;

	// Initialize the pointers this function will use to null.
	l_errorMessage = 0;
	l_shaderBuffer = 0;

	// Compile the shader code.
	l_shaderBuffer = loadShaderBuffer(shaderType::VERTEX, L"..//res//shaders//DX11//geometryPassCookTorranceVertex.sf");

	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateVertexShader(
		l_shaderBuffer->GetBufferPointer(), l_shaderBuffer->GetBufferSize(),
		NULL,
		&DXGeometryRenderPassSingletonComponent::getInstance().m_vertexShader);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create vertex shader!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC l_polygonLayout[3];
	unsigned int l_numElements;

	// Create the vertex input layout description.
	l_polygonLayout[0].SemanticName = "POSITION";
	l_polygonLayout[0].SemanticIndex = 0;
	l_polygonLayout[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	l_polygonLayout[0].InputSlot = 0;
	l_polygonLayout[0].AlignedByteOffset = 0;
	l_polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_polygonLayout[0].InstanceDataStepRate = 0;

	l_polygonLayout[1].SemanticName = "TEXCOORD";
	l_polygonLayout[1].SemanticIndex = 0;
	l_polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	l_polygonLayout[1].InputSlot = 0;
	l_polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	l_polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_polygonLayout[1].InstanceDataStepRate = 0;

	l_polygonLayout[2].SemanticName = "NORMAL";
	l_polygonLayout[2].SemanticIndex = 0;
	l_polygonLayout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	l_polygonLayout[2].InputSlot = 0;
	l_polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	l_polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_polygonLayout[2].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	l_numElements = sizeof(l_polygonLayout) / sizeof(l_polygonLayout[0]);

	// Create the vertex input layout.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateInputLayout(
		l_polygonLayout, l_numElements, l_shaderBuffer->GetBufferPointer(),
		l_shaderBuffer->GetBufferSize(), &DXGeometryRenderPassSingletonComponent::getInstance().m_layout);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create vertex shader layout!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Setup the description of the dynamic matrix constant buffer
	DXGeometryRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	DXGeometryRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.ByteWidth = sizeof(DirectX::XMMATRIX);
	DXGeometryRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	DXGeometryRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	DXGeometryRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.MiscFlags = 0;
	DXGeometryRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateBuffer(&DXGeometryRenderPassSingletonComponent::getInstance().m_matrixBufferDesc, NULL, &DXGeometryRenderPassSingletonComponent::getInstance().m_matrixBuffer);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create matrix buffer pointer!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	l_shaderBuffer->Release();
	l_shaderBuffer = 0;

	// Compile the shader code.
	l_shaderBuffer = loadShaderBuffer(shaderType::FRAGMENT, L"..//res//shaders//DX11//geometryPassCookTorrancePixel.sf");

	// Create the shader from the buffer.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreatePixelShader(
		l_shaderBuffer->GetBufferPointer(), 
		l_shaderBuffer->GetBufferSize(), 
		NULL, 
		&DXGeometryRenderPassSingletonComponent::getInstance().m_pixelShader);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: GeometryPass: can't create pixel shader!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	l_shaderBuffer->Release();
	l_shaderBuffer = 0;

	return true;
}

bool DXRenderingSystemNS::initializeFinalBlendPass()
{
	HRESULT result;
	ID3D10Blob* l_errorMessage;
	ID3D10Blob* l_shaderBuffer;

	// Initialize the pointers this function will use to null.
	l_errorMessage = 0;
	l_shaderBuffer = 0;

	// Compile the shader code.
	l_shaderBuffer = loadShaderBuffer(shaderType::VERTEX, L"..//res//shaders//DX11//testVertex.sf");

	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateVertexShader(
		l_shaderBuffer->GetBufferPointer(),
		l_shaderBuffer->GetBufferSize(), 
		NULL, 
		&DXFinalRenderPassSingletonComponent::getInstance().m_vertexShader);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: FinalBlendPass: can't create vertex shader!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC l_polygonLayout[3];
	unsigned int l_numElements;

	// Create the vertex input layout description.
	l_polygonLayout[0].SemanticName = "POSITION";
	l_polygonLayout[0].SemanticIndex = 0;
	l_polygonLayout[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	l_polygonLayout[0].InputSlot = 0;
	l_polygonLayout[0].AlignedByteOffset = 0;
	l_polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_polygonLayout[0].InstanceDataStepRate = 0;

	l_polygonLayout[1].SemanticName = "TEXCOORD";
	l_polygonLayout[1].SemanticIndex = 0;
	l_polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	l_polygonLayout[1].InputSlot = 0;
	l_polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	l_polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_polygonLayout[1].InstanceDataStepRate = 0;

	l_polygonLayout[2].SemanticName = "NORMAL";
	l_polygonLayout[2].SemanticIndex = 0;
	l_polygonLayout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	l_polygonLayout[2].InputSlot = 0;
	l_polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	l_polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	l_polygonLayout[2].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	l_numElements = sizeof(l_polygonLayout) / sizeof(l_polygonLayout[0]);

	// Create the vertex input layout.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateInputLayout(
		l_polygonLayout, l_numElements, l_shaderBuffer->GetBufferPointer(),
		l_shaderBuffer->GetBufferSize(), &DXFinalRenderPassSingletonComponent::getInstance().m_layout);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: FinalBlendPass: can't create vertex shader layout!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	// Setup the description of the dynamic matrix constant buffer
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.ByteWidth = sizeof(DirectX::XMMATRIX);
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.MiscFlags = 0;
	DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateBuffer(
		&DXFinalRenderPassSingletonComponent::getInstance().m_matrixBufferDesc, 
		NULL,
		&DXFinalRenderPassSingletonComponent::getInstance().m_matrixBuffer);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: FinalBlendPass: can't create matrix buffer pointer!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	l_shaderBuffer->Release();
	l_shaderBuffer = 0;

	// Compile the shader code.
	l_shaderBuffer = loadShaderBuffer(shaderType::FRAGMENT, L"..//res//shaders//DX11//testPixel.sf");

	// Create the shader from the buffer.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreatePixelShader(
		l_shaderBuffer->GetBufferPointer(), 
		l_shaderBuffer->GetBufferSize(), 
		NULL,
		&DXFinalRenderPassSingletonComponent::getInstance().m_pixelShader);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: FinalBlendPass: can't create pixel shader!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	l_shaderBuffer->Release();
	l_shaderBuffer = 0;

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
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateSamplerState(
		&DXFinalRenderPassSingletonComponent::getInstance().m_samplerDesc,
		&DXFinalRenderPassSingletonComponent::getInstance().m_sampleState);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: FinalBlendPass: can't create texture sampler state!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return false;
	}

	return true;
}

ID3D10Blob * DXRenderingSystemNS::loadShaderBuffer(shaderType shaderType, const std::wstring & shaderFilePath)
{
	auto l_shaderFilePath = shaderFilePath.c_str();
	auto l_shaderName = std::string(shaderFilePath.begin(), shaderFilePath.end());
	std::reverse(l_shaderName.begin(), l_shaderName.end());
	l_shaderName = l_shaderName.substr(l_shaderName.find(".") + 1, l_shaderName.find("//") - l_shaderName.find(".") - 1);
	std::reverse(l_shaderName.begin(), l_shaderName.end());

	HRESULT result;
	ID3D10Blob* l_errorMessage;
	ID3D10Blob* l_shaderBuffer;

	std::string l_shaderTypeName;

	switch (shaderType)
	{
	case shaderType::VERTEX:
		l_shaderTypeName = "vs_5_0";
		break;
	case shaderType::GEOMETRY:
		l_shaderTypeName = "gs_5_0";
		break;
	case shaderType::FRAGMENT:
		l_shaderTypeName = "ps_5_0";
		break;
	default:
		break;
	}

	result = D3DCompileFromFile(l_shaderFilePath, NULL, NULL, l_shaderName.c_str(), l_shaderTypeName.c_str(), D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&l_shaderBuffer, &l_errorMessage);
	if (FAILED(result))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (l_errorMessage)
		{
			OutputShaderErrorMessage(l_errorMessage, DXRenderingSystemNS::g_DXWindowSystemSingletonComponent->m_hwnd, l_shaderName.c_str());
		}
		// If there was nothing in the error message then it simply could not find the shader file itself.
		else
		{
			MessageBox(DXRenderingSystemNS::g_DXWindowSystemSingletonComponent->m_hwnd, l_shaderName.c_str(), "Missing Shader File", MB_OK);
			g_pCoreSystem->getLogSystem()->printLog("Error: Shader creation failed: cannot find shader!");
		}

		return nullptr;
	}
	g_pCoreSystem->getLogSystem()->printLog("DXRenderingSystem: innoShader: " + l_shaderName + " Shader has been compiled.");
	return l_shaderBuffer;
}

void DXRenderingSystemNS::OutputShaderErrorMessage(ID3D10Blob * errorMessage, HWND hwnd, const std::string & shaderFilename)
{
	char* compileErrors;
	unsigned long long bufferSize, i;
	std::stringstream errorSStream;

	// Get a pointer to the error message text buffer.
	compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	bufferSize = errorMessage->GetBufferSize();

	// Write out the error message.
	for (i = 0; i < bufferSize; i++)
	{
		errorSStream << compileErrors[i];
	}

	// Release the error message.
	errorMessage->Release();
	errorMessage = 0;

	MessageBox(DXRenderingSystemNS::g_DXWindowSystemSingletonComponent->m_hwnd, errorSStream.str().c_str(), shaderFilename.c_str(), MB_OK);
	g_pCoreSystem->getLogSystem()->printLog("DXRenderingSystem: innoShader: " + shaderFilename + " compile error: " + errorSStream.str() + "\n -- --------------------------------------------------- -- ");
}

DXMeshDataComponent* DXRenderingSystemNS::initializeMeshDataComponent(MeshDataComponent * rhs)
{
	if (rhs->m_objectStatus == objectStatus::ALIVE)
	{
		return DXRenderingSystemNS::getDXMeshDataComponent(rhs->m_parentEntity);
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
		result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateBuffer(&vertexBufferDesc, &vertexData, &l_ptr->m_vertexBuffer);
		if (FAILED(result))
		{
			g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create vbo!");
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
		result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateBuffer(&indexBufferDesc, &indexData, &l_ptr->m_indexBuffer);
		if (FAILED(result))
		{
			g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create ibo!");
			return nullptr;
		}
		l_ptr->m_objectStatus = objectStatus::ALIVE;
		rhs->m_objectStatus = objectStatus::ALIVE;
		return l_ptr;
	}
}

DXTextureDataComponent* DXRenderingSystemNS::initializeTextureDataComponent(TextureDataComponent * rhs)
{
	if (rhs->m_objectStatus == objectStatus::ALIVE)
	{
		return DXRenderingSystemNS::getDXTextureDataComponent(rhs->m_parentEntity);
	}
	else
	{
		auto l_ptr = DXRenderingSystemNS::addDXTextureDataComponent(rhs->m_parentEntity);

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
		hResult = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateTexture2D(&textureDesc, NULL, &l_texture);
		if (FAILED(hResult))
		{
			g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create texture!");
			return nullptr;
		}

		unsigned int rowPitch;
		rowPitch = (rhs->m_textureWidth * 4) * sizeof(unsigned char);
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->UpdateSubresource(l_texture, 0, NULL, rhs->m_textureData[0], rowPitch, 0);

		// Setup the shader resource view description.
		// Create the shader resource view for the texture.
		hResult = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_device->CreateShaderResourceView(l_texture, &srvDesc, &l_textureView);
		if (FAILED(hResult))
		{
			g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't create shader resource view for texture!");
			return nullptr;
		}

		// Generate mipmaps for this texture.
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->GenerateMips(l_ptr->m_textureView);

		l_ptr->m_objectStatus = objectStatus::ALIVE;
		rhs->m_objectStatus = objectStatus::ALIVE;

		return l_ptr;
	}
}

DXMeshDataComponent* DXRenderingSystemNS::addDXMeshDataComponent(EntityID rhs)
{
	DXMeshDataComponent* newMesh = g_pCoreSystem->getMemorySystem()->spawn<DXMeshDataComponent>();
	newMesh->m_parentEntity = rhs;
	auto l_meshMap = &DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_meshMap;
	l_meshMap->emplace(std::pair<EntityID, DXMeshDataComponent*>(rhs, newMesh));
	return newMesh;
}

DXTextureDataComponent* DXRenderingSystemNS::addDXTextureDataComponent(EntityID rhs)
{
	DXTextureDataComponent* newTexture = g_pCoreSystem->getMemorySystem()->spawn<DXTextureDataComponent>();
	newTexture->m_parentEntity = rhs;
	auto l_textureMap = &DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_textureMap;
	l_textureMap->emplace(std::pair<EntityID, DXTextureDataComponent*>(rhs, newTexture));
	return newTexture;
}

DXMeshDataComponent * DXRenderingSystemNS::getDXMeshDataComponent(EntityID rhs)
{
	auto result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_meshMap.find(rhs);
	if (result != DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_meshMap.end())
	{
		return result->second;
	}
	else
	{
		return nullptr;
	}
}

DXTextureDataComponent * DXRenderingSystemNS::getDXTextureDataComponent(EntityID rhs)
{
	auto result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_textureMap.find(rhs);
	if (result != DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_textureMap.end())
	{
		return result->second;
	}
	else
	{
		return nullptr;
	}
}

void DXRenderingSystemNS::prepareRenderingData()
{
	// camera matrices
	auto l_mainCamera = GameSystemSingletonComponent::getInstance().m_CameraComponents[0];
	auto l_mainCameraTransformComponent = g_pCoreSystem->getGameSystem()->get<TransformComponent>(l_mainCamera->m_parentEntity);

	auto l_p = l_mainCamera->m_projectionMatrix;
	auto l_r =
		InnoMath::getInvertRotationMatrix(
			l_mainCameraTransformComponent->m_globalTransformVector.m_rot
		);
	auto l_t =
		InnoMath::getInvertTranslationMatrix(
			l_mainCameraTransformComponent->m_globalTransformVector.m_pos
		);	
	DXRenderingSystemNS::m_CamProj = l_p;
	DXRenderingSystemNS::m_CamRot = l_r;
	DXRenderingSystemNS::m_CamTrans = l_t;
	DXRenderingSystemNS::m_CamViewProj = l_p * l_r * l_t;

	for (auto& l_visibleComponent : RenderingSystemSingletonComponent::getInstance().m_inFrustumVisibleComponents)
	{
		if (l_visibleComponent->m_visiblilityType == visiblilityType::STATIC_MESH)
		{
			for (auto& l_graphicData : l_visibleComponent->m_modelMap)
			{
				auto l_DXMDC = DXRenderingSystemNS::m_initializedMeshComponents.find(l_graphicData.first);
				if (l_DXMDC != DXRenderingSystemNS::m_initializedMeshComponents.end())
				{
					auto l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(l_graphicData.first);
					if (l_MDC)
					{
						mat4 m = g_pCoreSystem->getGameSystem()->get<TransformComponent>(l_visibleComponent->m_parentEntity)->m_globalTransformMatrix.m_transformationMat;
						auto mvp = DXRenderingSystemNS::m_CamViewProj * m;
						DXRenderingSystemNS::m_MeshComponentRenderingQueue.push(std::tuple<size_t, mat4, DXMeshDataComponent*>(l_MDC->m_indicesSize, std::move(mvp), l_DXMDC->second));
					}
				}
			}
		}
	}
}

void DXRenderingSystemNS::updateGeometryPass()
{
	// Set the vertex and pixel shaders that will be used to render this triangle.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->VSSetShader(
		DXGeometryRenderPassSingletonComponent::getInstance().m_vertexShader, 
		NULL, 
		0);
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->PSSetShader(
		DXGeometryRenderPassSingletonComponent::getInstance().m_pixelShader, 
		NULL, 
		0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set the vertex input layout.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->IASetInputLayout(DXGeometryRenderPassSingletonComponent::getInstance().m_layout);

	// Set the render buffers to be the render target.
	// Bind the render target view array and depth stencil buffer to the output render pipeline.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->OMSetRenderTargets(
		DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViews.size(), 
		&DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViews[0], 
		&DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilView[0]);

	// Set the viewport.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->RSSetViewports(
		1, 
		&DXGeometryRenderPassSingletonComponent::getInstance().m_viewport);

	// Clear the render buffers.
	for (auto i : DXGeometryRenderPassSingletonComponent::getInstance().m_renderTargetViews)
	{
		DXRenderingSystemNS::cleanRTV(vec4(0.0f, 0.0f, 0.0f, 1.0f), i);
	}
	DXRenderingSystemNS::cleanDSV(DXGeometryRenderPassSingletonComponent::getInstance().m_depthStencilView);

	// draw
	while (DXRenderingSystemNS::m_MeshComponentRenderingQueue.size() > 0)
	{
		auto l_tuple = DXRenderingSystemNS::m_MeshComponentRenderingQueue.front();

		updateShaderParameter(shaderType::VERTEX, DXFinalRenderPassSingletonComponent::getInstance().m_matrixBuffer, &std::get<mat4>(l_tuple));
		drawMesh(std::get<size_t>(l_tuple), std::get<DXMeshDataComponent*>(l_tuple));

		DXRenderingSystemNS::m_MeshComponentRenderingQueue.pop();
	}
}

void DXRenderingSystemNS::updateFinalBlendPass()
{
	// Set the vertex and pixel shaders that will be used to render this triangle.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->VSSetShader(DXFinalRenderPassSingletonComponent::getInstance().m_vertexShader, NULL, 0);
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->PSSetShader(DXFinalRenderPassSingletonComponent::getInstance().m_pixelShader, NULL, 0);

	// Set the sampler state in the pixel shader.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->PSSetSamplers(0, 1, &DXFinalRenderPassSingletonComponent::getInstance().m_sampleState);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set the vertex input layout.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->IASetInputLayout(DXFinalRenderPassSingletonComponent::getInstance().m_layout);

	// Set the viewport.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->RSSetViewports(
		1, 
		&DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_viewport);

	// Clear the render buffers.
	DXRenderingSystemNS::cleanRTV(vec4(0.0f, 0.0f, 0.0f, 0.0f), DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_renderTargetView);
	DXRenderingSystemNS::cleanDSV(DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_depthStencilView);
}

void DXRenderingSystemNS::drawMesh(EntityID rhs)
{
	auto l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(rhs);
	if (l_MDC)
	{
		drawMesh(l_MDC);
	}
}

void DXRenderingSystemNS::drawMesh(MeshDataComponent * MDC)
{
	auto l_DXMDC = DXRenderingSystemNS::getDXMeshDataComponent(MDC->m_parentEntity);
	if (l_DXMDC)
	{
		if (MDC->m_objectStatus == objectStatus::ALIVE && l_DXMDC->m_objectStatus == objectStatus::ALIVE)
		{
			drawMesh(MDC->m_indicesSize, l_DXMDC);
		}
	}
}

void DXRenderingSystemNS::drawMesh(size_t indicesSize, DXMeshDataComponent * DXMDC)
{
	unsigned int stride;
	unsigned int offset;

	// Set vertex buffer stride and offset.
	stride = sizeof(Vertex);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->IASetVertexBuffers(0, 1, &DXMDC->m_vertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->IASetIndexBuffer(DXMDC->m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Render the triangle.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->DrawIndexed((UINT)indicesSize, 0, 0);
}

void DXRenderingSystemNS::updateShaderParameter(shaderType shaderType, ID3D11Buffer * matrixBuffer, mat4* parameterValue)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	DirectX::XMMATRIX* dataPtr;
	unsigned int bufferNumber;

	// Lock the constant buffer so it can be written to.
	result = DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->Map(matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		g_pCoreSystem->getLogSystem()->printLog("Error: DXRenderingSystem: can't lock the shader matrix buffer!");
		DXRenderingSystemNS::m_objectStatus = objectStatus::STANDBY;
		return;
	}

	dataPtr = (DirectX::XMMATRIX*)mappedResource.pData;

	*dataPtr = *(DirectX::XMMATRIX*)parameterValue;

	// Unlock the constant buffer.
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->Unmap(matrixBuffer, 0);

	bufferNumber = 0;

	switch (shaderType)
	{
	case shaderType::VERTEX:
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->VSSetConstantBuffers(bufferNumber, 1, &matrixBuffer);
		break;
	case shaderType::GEOMETRY:
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->GSSetConstantBuffers(bufferNumber, 1, &matrixBuffer);
		break;
	case shaderType::FRAGMENT:
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->PSSetConstantBuffers(bufferNumber, 1, &matrixBuffer);
		break;
	default:
		break;
	}
}

void DXRenderingSystemNS::cleanRTV(vec4 color, ID3D11RenderTargetView* RTV)
{
	float l_color[4];

	// Setup the color to clear the buffer to.
	l_color[0] = color.x;
	l_color[1] = color.y;
	l_color[2] = color.z;
	l_color[3] = color.w;

	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->ClearRenderTargetView(RTV, l_color);
}

void DXRenderingSystemNS::cleanDSV(ID3D11DepthStencilView* DSV)
{
	DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_deviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void DXRenderingSystemNS::swapBuffer()
{
	// Present the back buffer to the screen since rendering is complete.
	if (DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_vsync_enabled)
	{
		// Lock to screen refresh rate.
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain->Present(1, 0);
	}
	else
	{
		// Present as fast as possible.
		DXRenderingSystemNS::g_DXRenderingSystemSingletonComponent->m_swapChain->Present(0, 0);
	}
}