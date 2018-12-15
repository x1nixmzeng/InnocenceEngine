#pragma once
#include "../common/InnoType.h"
#include "GLFrameBufferComponent.h"
#include "GLShaderProgramComponent.h"
#include "TextureDataComponent.h"
#include "GLTextureDataComponent.h"
#include "GLRenderPassComponent.h"

class GLGeometryRenderPassSingletonComponent
{
public:
	~GLGeometryRenderPassSingletonComponent() {};
	
	static GLGeometryRenderPassSingletonComponent& getInstance()
	{
		static GLGeometryRenderPassSingletonComponent instance;
		return instance;
	}

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
	EntityID m_parentEntity;
	
	GLRenderPassComponent* m_GLRPC;

	GLShaderProgramComponent* m_GLSPC;

#ifdef CookTorrance
	ShaderFilePaths m_shaderFilePaths = { "GL4.0//geometryPassCookTorranceVertex.sf" , "", "GL4.0//geometryPassCookTorranceFragment.sf" };
#elif BlinnPhong
	ShaderFilePaths m_shaderFilePaths = { "GL4.0//geometryPassBlinnPhongVertex.sf" , "", "GL4.0//geometryPassBlinnPhongFragment.sf" };
#endif

	GLuint m_geometryPass_uni_p_camera_original;
	GLuint m_geometryPass_uni_p_camera_jittered;
	GLuint m_geometryPass_uni_r_camera;
	GLuint m_geometryPass_uni_t_camera;
	GLuint m_geometryPass_uni_r_camera_prev;
	GLuint m_geometryPass_uni_t_camera_prev;
	GLuint m_geometryPass_uni_m;
	GLuint m_geometryPass_uni_m_prev;

	GLuint m_geometryPass_uni_p_light_0;
	GLuint m_geometryPass_uni_p_light_1;
	GLuint m_geometryPass_uni_p_light_2;
	GLuint m_geometryPass_uni_p_light_3;
	GLuint m_geometryPass_uni_v_light;

	GLuint m_geometryPass_uni_normalTexture;
	GLuint m_geometryPass_uni_albedoTexture;
	GLuint m_geometryPass_uni_metallicTexture;
	GLuint m_geometryPass_uni_roughnessTexture;
	GLuint m_geometryPass_uni_aoTexture;

	GLuint m_geometryPass_uni_useNormalTexture;
	GLuint m_geometryPass_uni_useAlbedoTexture;
	GLuint m_geometryPass_uni_useMetallicTexture;
	GLuint m_geometryPass_uni_useRoughnessTexture;
	GLuint m_geometryPass_uni_useAOTexture;

	GLuint m_geometryPass_uni_albedo;
	GLuint m_geometryPass_uni_MRA;
private:
	GLGeometryRenderPassSingletonComponent() {};
};
