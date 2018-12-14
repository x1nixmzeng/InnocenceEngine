#pragma once
#include "../common/InnoType.h"
#include "../common/ComponentHeaders.h"
#include "../common/InnoConcurrency.h"

class GameSystemSingletonComponent
{
public:
	~GameSystemSingletonComponent() {};
	
	static GameSystemSingletonComponent& getInstance()
	{
		static GameSystemSingletonComponent instance;
		return instance;
	}

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	// root TransformComponent
	TransformComponent* m_rootTransformComponent;

	// the AOS here
	std::vector<TransformComponent*> m_TransformComponents;
	std::vector<VisibleComponent*> m_VisibleComponents;
	std::vector<DirectionalLightComponent*> m_DirectionalLightComponents;
	std::vector<PointLightComponent*> m_PointLightComponents;
	std::vector<SphereLightComponent*> m_SphereLightComponents;
	std::vector<CameraComponent*> m_CameraComponents;
	std::vector<InputComponent*> m_InputComponents;
	std::vector<EnvironmentCaptureComponent*> m_EnvironmentCaptureComponents;
	std::vector<MaterialDataComponent*> m_MaterialDataComponents;
	
	std::unordered_map<EntityID, TransformComponent*> m_TransformComponentsMap;
	std::unordered_multimap<EntityID, VisibleComponent*> m_VisibleComponentsMap;
	std::unordered_multimap<EntityID, DirectionalLightComponent*> m_DirectionalLightComponentsMap;
	std::unordered_multimap<EntityID, PointLightComponent*> m_PointLightComponentsMap;
	std::unordered_multimap<EntityID, SphereLightComponent*> m_SphereLightComponentsMap;
	std::unordered_multimap<EntityID, CameraComponent*> m_CameraComponentsMap;
	std::unordered_multimap<EntityID, InputComponent*> m_InputComponentsMap;
	std::unordered_multimap<EntityID, EnvironmentCaptureComponent*> m_EnvironmentCaptureComponentsMap;
	std::unordered_multimap<EntityID, MaterialDataComponent*> m_MaterialDataComponentsMap;

	InnoFuture<void>* m_asyncTask;

	bool m_pauseGameUpdate = false;

private:
	GameSystemSingletonComponent() {};
};
