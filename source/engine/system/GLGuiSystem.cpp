#include "GLGuiSystem.h"
#include "../component/WindowSystemSingletonComponent.h"
#include "../component/GLWindowSystemSingletonComponent.h"
#include "../component/GLEnvironmentRenderPassSingletonComponent.h"
#include "../component/GLShadowRenderPassSingletonComponent.h"
#include "../component/GLGeometryRenderPassSingletonComponent.h"
#include "../component/GLTerrainRenderPassSingletonComponent.h"
#include "../component/GLLightRenderPassSingletonComponent.h"
#include "../component/GLFinalRenderPassSingletonComponent.h"
#include "../component/RenderingSystemSingletonComponent.h"
#include "../component/GLRenderingSystemSingletonComponent.h"
#include "../component/AssetSystemSingletonComponent.h"
#include "../component/GameSystemSingletonComponent.h"

#include "../third-party/ImGui/imgui.h"
#include "../third-party/ImGui/imgui_impl_glfw_gl3.h"

#if defined INNO_PLATFORM_WIN64 || defined INNO_PLATFORM_WIN32
#include <experimental/filesystem>
#endif

#include "ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

struct RenderingConfig
{
	bool useTAA = false;
	bool useBloom = false;
	bool useZoom = false;
	bool drawTerrain = false;
	bool drawSky = false;
	bool drawOverlapWireframe = false;
	bool showRenderPassResult = false;
};

struct GameConfig
{
	bool pauseGameUpdate = false;
};

class ImGuiWrapper
{
public:
	~ImGuiWrapper() {};

	static ImGuiWrapper& getInstance()
	{
		static ImGuiWrapper instance;
		return instance;
	}
	void setup();
	void initialize();
	void update();
	void terminate();
	void zoom(bool zoom, ImTextureID textureID, ImVec2 renderTargetSize);
	void setRenderingConfig();
private:
	ImGuiWrapper() {};
};

INNO_PRIVATE_SCOPE GLGuiSystemNS
{
	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
}

INNO_SYSTEM_EXPORT bool GLGuiSystem::setup()
{
	ImGuiWrapper::getInstance().setup();
	GLGuiSystemNS::m_objectStatus = ObjectStatus::ALIVE;
	return true;
}

INNO_SYSTEM_EXPORT bool GLGuiSystem::initialize()
{
	ImGuiWrapper::getInstance().initialize();
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "GLGuiSystem has been initialized.");
	return true;
}

INNO_SYSTEM_EXPORT bool GLGuiSystem::update()
{
	ImGuiWrapper::getInstance().update();
	return true;
}

INNO_SYSTEM_EXPORT bool GLGuiSystem::terminate()
{
	GLGuiSystemNS::m_objectStatus = ObjectStatus::STANDBY;
	ImGuiWrapper::getInstance().terminate();

	GLGuiSystemNS::m_objectStatus = ObjectStatus::SHUTDOWN;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "GLGuiSystem has been terminated.");
	return true;
}

INNO_SYSTEM_EXPORT ObjectStatus GLGuiSystem::getStatus()
{
	return GLGuiSystemNS::m_objectStatus;
}

void ImGuiWrapper::setup()
{
	// Setup Dear ImGui binding
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
	ImGui_ImplGlfwGL3_Init(GLWindowSystemSingletonComponent::getInstance().m_window, true);

	// Setup style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Load Fonts
	io.Fonts->AddFontFromFileTTF("..//res//fonts//FreeSans.otf", 16.0f);
}

void ImGuiWrapper::initialize()
{
}

void ImGuiWrapper::update()
{
	auto l_renderTargetSize = ImVec2((float)WindowSystemSingletonComponent::getInstance().m_windowResolution.x / 4.0f, (float)WindowSystemSingletonComponent::getInstance().m_windowResolution.y / 4.0f);
#ifdef DEBUG
#ifndef INNO_PLATFORM_LINUX64

	static RenderingConfig l_renderingConfig;
	static GameConfig l_gameConfig;

	ImGui_ImplGlfwGL3_NewFrame();
	{
		ImGui::Begin("Profiler", 0, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Checkbox("Use TAA", &l_renderingConfig.useTAA);
		ImGui::Checkbox("Use Bloom", &l_renderingConfig.useBloom);
		ImGui::Checkbox("Use zoom", &l_renderingConfig.useZoom);
		ImGui::Checkbox("Draw terrain", &l_renderingConfig.drawTerrain);
		ImGui::Checkbox("Draw sky", &l_renderingConfig.drawSky);
		ImGui::Checkbox("Draw overlap wireframe", &l_renderingConfig.drawOverlapWireframe);
		ImGui::Checkbox("Show render pass result", &l_renderingConfig.showRenderPassResult);
		ImGui::Checkbox("Pause game update", &l_gameConfig.pauseGameUpdate);
		const char* items[] = { "GPass", "TerrainPass", "LPass" };
		static int item_current = 0;
		ImGui::Combo("Choose shader", &item_current, items, IM_ARRAYSIZE(items));
		if (ImGui::Button("Reload Shader"))
		{
			RenderingSystemSingletonComponent::getInstance().f_reloadShader(RenderPassType(item_current));
		}
		ImGui::End();

		RenderingSystemSingletonComponent::getInstance().m_useTAA = l_renderingConfig.useTAA;
		RenderingSystemSingletonComponent::getInstance().m_useBloom = l_renderingConfig.useBloom;
		RenderingSystemSingletonComponent::getInstance().m_drawTerrain = l_renderingConfig.drawTerrain;
		RenderingSystemSingletonComponent::getInstance().m_drawSky = l_renderingConfig.drawSky;
		RenderingSystemSingletonComponent::getInstance().m_drawOverlapWireframe = l_renderingConfig.drawOverlapWireframe;

		GameSystemSingletonComponent::getInstance().m_pauseGameUpdate = l_gameConfig.pauseGameUpdate;

		if (l_renderingConfig.showRenderPassResult)
		{
			ImGui::Begin("Geometry Pass", 0, ImGuiWindowFlags_AlwaysAutoResize);
			{
				{
					ImGui::BeginChild("World Space Position(RGB) + Metallic(A)", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("World Space Position(RGB) + Metallic(A)");
					ImGui::Image(ImTextureID((GLuint64)GLGeometryRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("World Space Normal(RGB) + Roughness(A)", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("World Space Normal(RGB) + Roughness(A)");
					ImGui::Image(ImTextureID((GLuint64)GLGeometryRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[1]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
				{
					ImGui::BeginChild("Albedo(RGB) + Ambient Occlusion(A)", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Albedo(RGB) + Ambient Occlusion(A)");
					ImGui::Image(ImTextureID((GLuint64)GLGeometryRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[2]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("Screen Space Motion Vector", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Screen Space Motion Vector");
					ImGui::Image(ImTextureID((GLuint64)GLGeometryRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[3]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
				{
					ImGui::BeginChild("Light Space Position 0", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Light Space Position 0");
					ImGui::Image(ImTextureID((GLuint64)GLGeometryRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[4]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("Light Space Position 1", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Light Space Position 1");
					ImGui::Image(ImTextureID((GLuint64)GLGeometryRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[5]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
				{
					ImGui::BeginChild("Light Space Position 2", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Light Space Position 2");
					ImGui::Image(ImTextureID((GLuint64)GLGeometryRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[6]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("Light Space Position 3", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Light Space Position 3");
					ImGui::Image(ImTextureID((GLuint64)GLGeometryRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[7]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
			}
			ImGui::End();

			ImGui::Begin("Terrain Pass", 0, ImGuiWindowFlags_AlwaysAutoResize);
			{
				ImGui::Image(ImTextureID((GLuint64)GLTerrainRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
				zoom(l_renderingConfig.useZoom, ImTextureID((GLuint64)GLTerrainRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize);
			}
			ImGui::End();

			ImGui::Begin("Light Pass", 0, ImGuiWindowFlags_AlwaysAutoResize);
			{
				ImGui::Image(ImTextureID((GLuint64)GLLightRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
				zoom(l_renderingConfig.useZoom, ImTextureID((GLuint64)GLLightRenderPassSingletonComponent::getInstance().m_GLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize);
			}
			ImGui::End();

			ImGui::Begin("Final Pass", 0, ImGuiWindowFlags_AlwaysAutoResize);
			{
				{
					ImGui::BeginChild("Sky Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Sky Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_skyPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("Pre TAA Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Pre TAA Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_preTAAPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
				{
					ImGui::BeginChild("TAA Ping Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("TAA Ping Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_TAAPingPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("TAA Sharpen Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("TAA Sharpen Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_TAASharpenPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
				{
					ImGui::BeginChild("Bloom Extract Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Bloom Extract Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_bloomExtractPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("Bloom Blur Ping Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Bloom Blur Ping Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_bloomBlurPingPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
				{
					ImGui::BeginChild("Motion Blur Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Motion Blur Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_motionBlurPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("Billboard Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Billboard Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_billboardPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
				{
					ImGui::BeginChild("Debugger Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Debugger Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_debuggerPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();

					ImGui::SameLine();

					ImGui::BeginChild("Final Blend Pass", l_renderTargetSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
					ImGui::Text("Final Blend Pass");
					ImGui::Image(ImTextureID((GLuint64)GLFinalRenderPassSingletonComponent::getInstance().m_finalBlendPassGLRPC->m_GLTDCs[0]->m_TAO), l_renderTargetSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
					ImGui::EndChild();
				}
			}
			ImGui::End();

			auto l_shadowPassWindowSize = ImVec2(128.0, 128.0);
			ImGui::Begin("Shadow Pass", 0, ImGuiWindowFlags_AlwaysAutoResize);
			ImGui::BeginChild("Shadow Pass Depth Buffer 0", l_shadowPassWindowSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::Text("Depth Buffer 0");
			ImGui::Image(ImTextureID((GLuint64)GLShadowRenderPassSingletonComponent::getInstance().m_GLTDCs[0]->m_TAO), l_shadowPassWindowSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("Shadow Pass Depth Buffer 1", l_shadowPassWindowSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::Text("Depth Buffer 1");
			ImGui::Image(ImTextureID((GLuint64)GLShadowRenderPassSingletonComponent::getInstance().m_GLTDCs[1]->m_TAO), l_shadowPassWindowSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
			ImGui::EndChild();
			ImGui::BeginChild("Shadow Pass Depth Buffer 2", l_shadowPassWindowSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::Text("Depth Buffer 2");
			ImGui::Image(ImTextureID((GLuint64)GLShadowRenderPassSingletonComponent::getInstance().m_GLTDCs[2]->m_TAO), l_shadowPassWindowSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("Shadow Pass Depth Buffer 3", l_shadowPassWindowSize, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
			ImGui::Text("Depth Buffer 3");
			ImGui::Image(ImTextureID((GLuint64)GLShadowRenderPassSingletonComponent::getInstance().m_GLTDCs[3]->m_TAO), l_shadowPassWindowSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
			ImGui::EndChild();
			ImGui::End();

			auto l_BRDFLUT = ImVec2(128.0, 128.0);
			ImGui::Begin("BRDF lookup table", 0, ImGuiWindowFlags_AlwaysAutoResize);
			{
				ImGui::BeginChild("IBL LUT", l_BRDFLUT, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
				ImGui::Image(ImTextureID((GLuint64)GLEnvironmentRenderPassSingletonComponent::getInstance().m_BRDFSplitSumLUTPassGLTDC->m_TAO), l_BRDFLUT, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
				zoom(l_renderingConfig.useZoom, ImTextureID((GLuint64)GLEnvironmentRenderPassSingletonComponent::getInstance().m_BRDFSplitSumLUTPassGLTDC->m_TAO), l_BRDFLUT);
				ImGui::EndChild();

				ImGui::SameLine();

				ImGui::BeginChild("Multi-Scattering LUT", l_BRDFLUT, true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
				ImGui::Image(ImTextureID((GLuint64)GLEnvironmentRenderPassSingletonComponent::getInstance().m_BRDFMSAverageLUTPassGLTDC->m_TAO), l_BRDFLUT, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
				zoom(l_renderingConfig.useZoom, ImTextureID((GLuint64)GLEnvironmentRenderPassSingletonComponent::getInstance().m_BRDFMSAverageLUTPassGLTDC->m_TAO), l_BRDFLUT);
				ImGui::EndChild();
			}
			ImGui::End();
		}

		{
			auto l_iconSize = ImVec2(32.0f, 32.0f);

			std::function<ImTextureID(const IconType iconType)> f_getTextID =
				[&](const IconType iconType) -> ImTextureID
			{
				switch (iconType)
				{
				case IconType::OBJ:
					return ImTextureID((GLuint64)GLRenderingSystemSingletonComponent::getInstance().m_iconTemplate_OBJ->m_TAO); break;
				case IconType::PNG:
					return ImTextureID((GLuint64)GLRenderingSystemSingletonComponent::getInstance().m_iconTemplate_PNG->m_TAO); break;
				case IconType::SHADER:
					return ImTextureID((GLuint64)GLRenderingSystemSingletonComponent::getInstance().m_iconTemplate_SHADER->m_TAO); break;
				case IconType::UNKNOWN:
					return ImTextureID((GLuint64)GLRenderingSystemSingletonComponent::getInstance().m_iconTemplate_UNKNOWN->m_TAO); break;
				default:
					return nullptr; break;
				}
			};

			std::function<void(const AssetMetadata* assetMetadata)> f_assetBuilder =
				[&](const AssetMetadata* assetMetadata)
			{
				ImGui::Image(f_getTextID(assetMetadata->iconType), l_iconSize, ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));
				ImGui::SameLine();
				ImGui::Text((assetMetadata->fileName + assetMetadata->extension).c_str());
			};

			std::function<void(const DirectoryMetadata* directoryMetadata)> f_directoryTreeBuilder =
				[&](const DirectoryMetadata* directoryMetadata)
			{
				if (ImGui::TreeNode(directoryMetadata->directoryName.c_str()))
				{
					for (auto& i : directoryMetadata->childrenDirectories)
					{
						f_directoryTreeBuilder(&i);
					}
					for (auto& i : directoryMetadata->childrenAssets)
					{
						f_assetBuilder(&i);
					}
					ImGui::TreePop();
				}
			};

			ImGui::Begin("File Explorer", 0, ImGuiWindowFlags_AlwaysAutoResize);

			auto& x = AssetSystemSingletonComponent::getInstance().m_rootDirectoryMetadata;

			f_directoryTreeBuilder(&AssetSystemSingletonComponent::getInstance().m_rootDirectoryMetadata);

			ImGui::End();
		}

		// Rendering
		glViewport(0, 0, (GLsizei)WindowSystemSingletonComponent::getInstance().m_windowResolution.x, (GLsizei)WindowSystemSingletonComponent::getInstance().m_windowResolution.y);
		ImGui::Render();
		ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());
#else
	// @TODO: Linux ImGui WIP
#endif
#else
	// @TODO: handle GUI component
	ImGui::Begin("Main Menu", 0, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Button("Start");
	ImGui::End();
	glViewport(0, 0, (GLsizei)WindowSystemSingletonComponent::getInstance().m_windowResolution.x, (GLsizei)WindowSystemSingletonComponent::getInstance().m_windowResolution.y);
	ImGui::Render();
	ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());
#endif
	}
}

void ImGuiWrapper::terminate()
{
}

void ImGuiWrapper::zoom(bool zoom, ImTextureID textureID, ImVec2 renderTargetSize)
{
	if (zoom)
	{
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		ImVec2 pos = ImGui::GetCursorScreenPos();
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			float region_sz = 32.0f;
			float region_x = io.MousePos.x - pos.x - region_sz * 0.5f; if (region_x < 0.0f) region_x = 0.0f; else if (region_x > renderTargetSize.x - region_sz) region_x = renderTargetSize.x - region_sz;
			float region_y = pos.y - io.MousePos.y - region_sz * 0.5f; if (region_y < 0.0f) region_y = 0.0f; else if (region_y > renderTargetSize.y - region_sz) region_y = renderTargetSize.y - region_sz;
			float zoom = 4.0f;
			ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
			ImGui::Text("Max: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);
			ImVec2 uv0 = ImVec2((region_x) / renderTargetSize.x, (region_y + region_sz) / renderTargetSize.y);
			ImVec2 uv1 = ImVec2((region_x + region_sz) / renderTargetSize.x, (region_y) / renderTargetSize.y);
			ImGui::Image(textureID, ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, ImColor(255, 255, 255, 255), ImColor(255, 255, 255, 128));
			ImGui::EndTooltip();
		}
	}
}
