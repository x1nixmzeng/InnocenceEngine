#pragma once
#include "../common/InnoType.h"
#include "GLRenderPassComponent.h"
#include "GLShaderProgramComponent.h"

class GLShadowRenderPassComponent
{
public:
	~GLShadowRenderPassComponent() {};

	static GLShadowRenderPassComponent& get()
	{
		static GLShadowRenderPassComponent instance;
		return instance;
	}

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	std::vector<GLRenderPassComponent*> m_GLRPCs;

	GLShaderProgramComponent* m_SPC;

	GLuint m_shadowPass_uni_p;
	GLuint m_shadowPass_uni_v;
	GLuint m_shadowPass_uni_m;
private:
	GLShadowRenderPassComponent() {};
};
