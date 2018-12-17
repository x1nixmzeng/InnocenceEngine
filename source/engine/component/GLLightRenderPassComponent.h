#pragma once
#include "../common/InnoType.h"
#include "GLFrameBufferComponent.h"
#include "GLShaderProgramComponent.h"
#include "TextureDataComponent.h"
#include "GLTextureDataComponent.h"

class GLLightRenderPassComponent
{
public:
	~GLLightRenderPassComponent() {};
	
	static GLLightRenderPassComponent& get()
	{
		static GLLightRenderPassComponent instance;
		return instance;
	}

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	GLRenderPassComponent* m_GLRPC;

	GLShaderProgramComponent* m_GLSPC;

#ifdef CookTorrance
	ShaderFilePaths m_shaderFilePaths = { "GL4.0//lightPassCookTorranceVertex.sf" , "", "GL4.0//lightPassCookTorranceFragment.sf" };
#elif BlinnPhong
	ShaderFilePaths m_shaderFilePaths = { "GL4.0//lightPassBlinnPhongVertex.sf" , "", "GL4.0//lightPassBlinnPhongFragment.sf" };
#endif

	GLuint m_uni_geometryPassRT0;
	GLuint m_uni_geometryPassRT1;
	GLuint m_uni_geometryPassRT2;
	GLuint m_uni_geometryPassRT3;
	GLuint m_uni_geometryPassRT4;
	GLuint m_uni_geometryPassRT5;
	GLuint m_uni_geometryPassRT6;
	GLuint m_uni_geometryPassRT7;

	GLuint m_uni_shadowMap_0;
	GLuint m_uni_shadowMap_1;
	GLuint m_uni_shadowMap_2;
	GLuint m_uni_shadowMap_3;
	std::vector<GLuint> m_uni_shadowSplitAreas;

	GLuint m_uni_brdfLUT;
	GLuint m_uni_brdfMSLUT;

	GLuint m_uni_viewPos;

	GLuint m_uni_dirLight_direction;
	GLuint m_uni_dirLight_color;

	std::vector<GLuint> m_uni_pointLights_position;
	std::vector<GLuint> m_uni_pointLights_attenuationRadius;
	std::vector<GLuint> m_uni_pointLights_luminance;

	std::vector<GLuint> m_uni_sphereLights_position;
	std::vector<GLuint> m_uni_sphereLights_sphereRadius;
	std::vector<GLuint> m_uni_sphereLights_luminance;

	GLuint m_uni_isEmissive;
private:
	GLLightRenderPassComponent() {};
};
