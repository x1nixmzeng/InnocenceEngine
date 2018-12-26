#include "GLRenderingSystemUtilities.h"
#include "GLGeometryRenderingPassUtilities.h"
#include "../component/GLGeometryRenderPassComponent.h"
#include "../component/GLTerrainRenderPassComponent.h"
#include "../component/GameSystemComponent.h"
#include "../component/RenderingSystemComponent.h"
#include "../component/GLRenderingSystemComponent.h"

#include "ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE GLRenderingSystemNS
{
	void initializeOpaquePass();
	void initializeOpaquePassShaders();
	void bindOpaquePassUniformLocations(GLShaderProgramComponent* rhs);

	void initializeTransparentPass();
	void initializeTransparentPassShaders();
	void bindTransparentPassUniformLocations(GLShaderProgramComponent* rhs);

	void initializeTerrainPass();
	void initializeTerrainPassShaders();
	void bindTerrainPassUniformLocations(GLShaderProgramComponent* rhs);

	void updateGeometryRenderPass();
	void updateOpaqueRenderPass();
	void updateTransparentRenderPass();

	void updateTerrainRenderPass();
}

void GLRenderingSystemNS::initializeGeometryPass()
{
	initializeOpaquePass();
	initializeTransparentPass();
	initializeTerrainPass();
}

void GLRenderingSystemNS::initializeOpaquePass()
{
	GLGeometryRenderPassComponent::get().m_opaquePass_GLRPC = addGLRenderPassComponent(8, GLRenderingSystemComponent::get().deferredPassFBDesc, GLRenderingSystemComponent::get().deferredPassTextureDesc);

	// UBO
	auto l_UBO = generateUBO(sizeof(GPassCameraUBOData));
	GLGeometryRenderPassComponent::get().m_cameraUBO = l_UBO;

	l_UBO = generateUBO(sizeof(GPassLightUBOData));
	GLGeometryRenderPassComponent::get().m_lightUBO = l_UBO;

	l_UBO = generateUBO(sizeof(GPassMeshUBOData));
	GLGeometryRenderPassComponent::get().m_meshUBO = l_UBO;

	l_UBO = generateUBO(sizeof(GPassTextureUBOData));
	GLGeometryRenderPassComponent::get().m_textureUBO = l_UBO;

	initializeOpaquePassShaders();
}

void GLRenderingSystemNS::initializeOpaquePassShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(0);

	initializeGLShaderProgramComponent(rhs, GLGeometryRenderPassComponent::get().m_opaquePass_shaderFilePaths);

	bindOpaquePassUniformLocations(rhs);

	GLGeometryRenderPassComponent::get().m_opaquePass_GLSPC = rhs;
}

void GLRenderingSystemNS::bindOpaquePassUniformLocations(GLShaderProgramComponent* rhs)
{
	bindUniformBlock(GLGeometryRenderPassComponent::get().m_cameraUBO, sizeof(GPassCameraUBOData), rhs->m_program, "cameraUBO", 0);

	bindUniformBlock(GLGeometryRenderPassComponent::get().m_lightUBO, sizeof(GPassLightUBOData), rhs->m_program, "lightUBO", 1);

	bindUniformBlock(GLGeometryRenderPassComponent::get().m_meshUBO, sizeof(GPassMeshUBOData), rhs->m_program, "meshUBO", 2);

	bindUniformBlock(GLGeometryRenderPassComponent::get().m_textureUBO, sizeof(GPassTextureUBOData), rhs->m_program, "textureUBO", 3);

#ifdef CookTorrance
	updateTextureUniformLocations(rhs->m_program, GLGeometryRenderPassComponent::get().m_textureUniformNames);
#elif BlinnPhong
	// @TODO: texture uniforms
#endif
}

void GLRenderingSystemNS::initializeTransparentPass()
{
	GLGeometryRenderPassComponent::get().m_transparentPass_GLRPC = addGLRenderPassComponent(1, GLRenderingSystemComponent::get().deferredPassFBDesc, GLRenderingSystemComponent::get().deferredPassTextureDesc);

	initializeTransparentPassShaders();
}

void GLRenderingSystemNS::initializeTransparentPassShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(0);

	initializeGLShaderProgramComponent(rhs, GLGeometryRenderPassComponent::get().m_transparentPass_shaderFilePaths);

	bindTransparentPassUniformLocations(rhs);

	GLGeometryRenderPassComponent::get().m_transparentPass_GLSPC = rhs;
}

void GLRenderingSystemNS::bindTransparentPassUniformLocations(GLShaderProgramComponent* rhs)
{
	bindUniformBlock(GLGeometryRenderPassComponent::get().m_cameraUBO, sizeof(GPassCameraUBOData), rhs->m_program, "cameraUBO", 0);

	bindUniformBlock(GLGeometryRenderPassComponent::get().m_meshUBO, sizeof(GPassMeshUBOData), rhs->m_program, "meshUBO", 1);

	GLGeometryRenderPassComponent::get().m_transparentPass_uni_albedo = getUniformLocation(
		rhs->m_program,
		"uni_albedo");
	GLGeometryRenderPassComponent::get().m_transparentPass_uni_viewPos = getUniformLocation(
		rhs->m_program,
		"uni_viewPos");
}

void GLRenderingSystemNS::initializeTerrainPass()
{
	GLTerrainRenderPassComponent::get().m_GLRPC = addGLRenderPassComponent(1, GLRenderingSystemComponent::get().deferredPassFBDesc, GLRenderingSystemComponent::get().deferredPassTextureDesc);

	initializeTerrainPassShaders();
}

void GLRenderingSystemNS::initializeTerrainPassShaders()
{
	// shader programs and shaders
	auto rhs = addGLShaderProgramComponent(0);

	initializeGLShaderProgramComponent(rhs, GLTerrainRenderPassComponent::get().m_shaderFilePaths);

	bindTerrainPassUniformLocations(rhs);

	GLTerrainRenderPassComponent::get().m_GLSPC = rhs;
}

void GLRenderingSystemNS::bindTerrainPassUniformLocations(GLShaderProgramComponent* rhs)
{
	GLTerrainRenderPassComponent::get().m_terrainPass_uni_p_camera = getUniformLocation(
		rhs->m_program,
		"uni_p_camera");
	GLTerrainRenderPassComponent::get().m_terrainPass_uni_r_camera = getUniformLocation(
		rhs->m_program,
		"uni_r_camera");
	GLTerrainRenderPassComponent::get().m_terrainPass_uni_t_camera = getUniformLocation(
		rhs->m_program,
		"uni_t_camera");
	GLTerrainRenderPassComponent::get().m_terrainPass_uni_m = getUniformLocation(
		rhs->m_program,
		"uni_m");
	GLTerrainRenderPassComponent::get().m_terrainPass_uni_albedoTexture = getUniformLocation(
		rhs->m_program,
		"uni_albedoTexture");
	updateUniform(
		GLTerrainRenderPassComponent::get().m_terrainPass_uni_albedoTexture,
		0);
}

void GLRenderingSystemNS::updateGeometryRenderPass()
{
	updateOpaqueRenderPass();
	updateTransparentRenderPass();
	updateTerrainRenderPass();
}

void GLRenderingSystemNS::updateOpaqueRenderPass()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_CLAMP);

	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0xFF);

	//glEnable(GL_CULL_FACE);
	//glFrontFace(GL_CCW);
	//glCullFace(GL_BACK);

	// bind to framebuffer
	auto l_FBC = GLGeometryRenderPassComponent::get().m_opaquePass_GLRPC->m_GLFBC;
	bindFBC(l_FBC);

	activateShaderProgram(GLGeometryRenderPassComponent::get().m_opaquePass_GLSPC);

	updateUBO(GLGeometryRenderPassComponent::get().m_cameraUBO, GLRenderingSystemComponent::get().m_GPassCameraUBOData);

#ifdef CookTorrance
	//Cook-Torrance
	updateUBO(GLGeometryRenderPassComponent::get().m_lightUBO, GLRenderingSystemComponent::get().m_GPassLightUBOData);

	while (GLRenderingSystemComponent::get().m_GPassOpaqueRenderDataQueue.size() > 0)
	{
		auto l_renderPack = GLRenderingSystemComponent::get().m_GPassOpaqueRenderDataQueue.front();
		if (l_renderPack.visiblilityType == VisiblilityType::INNO_OPAQUE)
		{
			glStencilFunc(GL_ALWAYS, 0x01, 0xFF);

			// any normal?
			if (l_renderPack.m_GPassTextureUBOData.useNormalTexture)
			{
				activateTexture(l_renderPack.m_basicNormalGLTDC, 0);
			}
			// any albedo?
			if (l_renderPack.m_GPassTextureUBOData.useAlbedoTexture)
			{
				activateTexture(l_renderPack.m_basicAlbedoGLTDC, 1);
			}
			// any metallic?
			if (l_renderPack.m_GPassTextureUBOData.useMetallicTexture)
			{
				activateTexture(l_renderPack.m_basicMetallicGLTDC, 2);
			}
			// any roughness?
			if (l_renderPack.m_GPassTextureUBOData.useRoughnessTexture)
			{
				activateTexture(l_renderPack.m_basicRoughnessGLTDC, 3);
			}
			// any ao?
			if (l_renderPack.m_GPassTextureUBOData.useAOTexture)
			{
				activateTexture(l_renderPack.m_basicAOGLTDC, 4);
			}

			updateUBO(GLGeometryRenderPassComponent::get().m_meshUBO, l_renderPack.m_GPassMeshUBOData);
			updateUBO(GLGeometryRenderPassComponent::get().m_textureUBO, l_renderPack.m_GPassTextureUBOData);
			drawMesh(l_renderPack.indiceSize, l_renderPack.m_meshDrawMethod, l_renderPack.GLMDC);
		}
		else if (l_renderPack.visiblilityType == VisiblilityType::INNO_EMISSIVE)
		{
			glStencilFunc(GL_ALWAYS, 0x02, 0xFF);

			updateUBO(GLGeometryRenderPassComponent::get().m_meshUBO, l_renderPack.m_GPassMeshUBOData);
			updateUBO(GLGeometryRenderPassComponent::get().m_textureUBO, l_renderPack.m_GPassTextureUBOData);

			drawMesh(l_renderPack.indiceSize, l_renderPack.m_meshDrawMethod, l_renderPack.GLMDC);
		}
		else
		{
			glStencilFunc(GL_ALWAYS, 0x00, 0xFF);
		}
		GLRenderingSystemComponent::get().m_GPassOpaqueRenderDataQueue.pop();
	}

	//glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_CLAMP);
	glDisable(GL_DEPTH_TEST);

#elif BlinnPhong
#endif
}

void GLRenderingSystemNS::updateTransparentRenderPass()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_CLAMP);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC1_COLOR);

	// bind to framebuffer
	auto l_FBC = GLGeometryRenderPassComponent::get().m_transparentPass_GLRPC->m_GLFBC;
	bindFBC(l_FBC);

	copyDepthBuffer(GLGeometryRenderPassComponent::get().m_opaquePass_GLRPC->m_GLFBC, l_FBC);

	activateShaderProgram(GLGeometryRenderPassComponent::get().m_transparentPass_GLSPC);

	updateUBO(GLGeometryRenderPassComponent::get().m_cameraUBO, GLRenderingSystemComponent::get().m_GPassCameraUBOData);

	updateUniform(
		GLGeometryRenderPassComponent::get().m_transparentPass_uni_viewPos,
		GLRenderingSystemComponent::get().m_CamGlobalPos.x, GLRenderingSystemComponent::get().m_CamGlobalPos.y, GLRenderingSystemComponent::get().m_CamGlobalPos.z);

	while (GLRenderingSystemComponent::get().m_GPassTransparentRenderDataQueue.size() > 0)
	{
		auto l_renderPack = GLRenderingSystemComponent::get().m_GPassTransparentRenderDataQueue.front();

		updateUBO(GLGeometryRenderPassComponent::get().m_meshUBO, l_renderPack.m_GPassMeshUBOData);

		updateUniform(GLGeometryRenderPassComponent::get().m_transparentPass_uni_albedo, l_renderPack.meshCustomMaterial.albedo_r, l_renderPack.meshCustomMaterial.albedo_g, l_renderPack.meshCustomMaterial.albedo_b, l_renderPack.meshCustomMaterial.alpha);

		drawMesh(l_renderPack.indiceSize, l_renderPack.m_meshDrawMethod, l_renderPack.GLMDC);

		GLRenderingSystemComponent::get().m_GPassTransparentRenderDataQueue.pop();
	}

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_CLAMP);
	glDisable(GL_DEPTH_TEST);
}

void GLRenderingSystemNS::updateTerrainRenderPass()
{
	if (RenderingSystemComponent::get().m_drawTerrain)
	{
		glEnable(GL_DEPTH_TEST);

		// bind to framebuffer
		auto l_FBC = GLTerrainRenderPassComponent::get().m_GLRPC->m_GLFBC;
		bindFBC(l_FBC);

		copyDepthBuffer(GLGeometryRenderPassComponent::get().m_opaquePass_GLRPC->m_GLFBC, l_FBC);

		activateShaderProgram(GLTerrainRenderPassComponent::get().m_GLSPC);

		mat4 m = InnoMath::generateIdentityMatrix<float>();

		updateUniform(
			GLTerrainRenderPassComponent::get().m_terrainPass_uni_p_camera,
			GLRenderingSystemComponent::get().m_CamProjOriginal);
		updateUniform(
			GLTerrainRenderPassComponent::get().m_terrainPass_uni_r_camera,
			GLRenderingSystemComponent::get().m_CamRot);
		updateUniform(
			GLTerrainRenderPassComponent::get().m_terrainPass_uni_t_camera,
			GLRenderingSystemComponent::get().m_CamTrans);
		updateUniform(
			GLTerrainRenderPassComponent::get().m_terrainPass_uni_m,
			m);

		auto l_MDC = g_pCoreSystem->getAssetSystem()->getMeshDataComponent(MeshShapeType::TERRAIN);
		activateTexture(GLRenderingSystemComponent::get().m_basicAlbedoGLTDC, 0);

		drawMesh(l_MDC);

		glDisable(GL_DEPTH_TEST);
	}
	else
	{
		cleanFBC(GLTerrainRenderPassComponent::get().m_GLRPC->m_GLFBC);
	}
}