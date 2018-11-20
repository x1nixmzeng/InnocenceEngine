#pragma once
#include "../common/InnoType.h"
#include "VisibleComponent.h"
#include<atomic>

//#define BlinnPhong
#define CookTorrance

class RenderingSystemSingletonComponent
{
public:
	~RenderingSystemSingletonComponent() {};
	
	static RenderingSystemSingletonComponent& getInstance()
	{
		static RenderingSystemSingletonComponent instance;
		return instance;
	}

	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	std::atomic<bool> m_canRender;
	bool m_shouldUpdateEnvironmentMap = true;
	bool m_isTAAPingPass = true;
	vec2 m_renderTargetSize = vec2(1280, 720);
	std::vector<VisibleComponent*> m_staticMeshVisibleComponents;
	std::vector<VisibleComponent*> m_emissiveVisibleComponents;
	std::vector<VisibleComponent*> m_selectedVisibleComponents;
	std::vector<VisibleComponent*> m_inFrustumVisibleComponents;
	std::vector<vec2> HaltonSampler;
	int currentHaltonStep = 0;
	int m_MSAAdepth = 4;

private:
	RenderingSystemSingletonComponent() {};
};
