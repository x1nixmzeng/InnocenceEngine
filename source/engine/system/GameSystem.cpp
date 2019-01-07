#include "GameSystem.h"
#include "../common/config.h"
#include "../component/GameSystemComponent.h"

#include "ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE InnoGameSystemNS
{
	bool setup();

	std::string getEntityName(EntityID entityID);

	void sortTransformComponentsVector();

	void updateTransform();

	EntityID createEntity(const std::string & entityName);

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;

	static GameSystemComponent* g_GameSystemComponent;
	IGameInstance* m_gameInstance;
}

std::string InnoGameSystemNS::getEntityName(EntityID entityID)
{
	auto result = std::find_if(
		InnoGameSystemNS::g_GameSystemComponent->m_enitityNameMap.begin(),
		InnoGameSystemNS::g_GameSystemComponent->m_enitityNameMap.end(),
		[&](auto& val)-> bool {
		return val.first == entityID;
	}
	);

	if (result == InnoGameSystemNS::g_GameSystemComponent->m_enitityNameMap.end())
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "GameSystem: can't find entity name by ID " + entityID + " !");
		return "AbnormalEntity";
	}

	return result->second;
}

bool InnoGameSystemNS::setup()
{
	g_GameSystemComponent = &GameSystemComponent::get();

	// setup root TransformComponent
	g_GameSystemComponent->m_rootTransformComponent = new TransformComponent();
	g_GameSystemComponent->m_rootTransformComponent->m_parentTransformComponent = nullptr;

	g_GameSystemComponent->m_rootTransformComponent->m_parentEntity = createEntity("RootTransform");

	g_GameSystemComponent->m_rootTransformComponent->m_localTransformMatrix = InnoMath::TransformVectorToTransformMatrix(g_GameSystemComponent->m_rootTransformComponent->m_localTransformVector);
	g_GameSystemComponent->m_rootTransformComponent->m_globalTransformVector = g_GameSystemComponent->m_rootTransformComponent->m_localTransformVector;
	g_GameSystemComponent->m_rootTransformComponent->m_globalTransformMatrix = g_GameSystemComponent->m_rootTransformComponent->m_localTransformMatrix;

	m_objectStatus = ObjectStatus::ALIVE;

	if (!m_gameInstance->setup())
	{
		return false;
	}

	return true;
}

INNO_SYSTEM_EXPORT bool InnoGameSystem::setup()
{
	if (!InnoGameSystemNS::setup())
	{
		return false;
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "GameInstance setup finished.");

	return true;
}

void InnoGameSystemNS::sortTransformComponentsVector()
{
	//construct the hierarchy tree
	for (auto i : InnoGameSystemNS::g_GameSystemComponent->m_TransformComponents)
	{
		if (i->m_parentTransformComponent)
		{
			i->m_transformHierarchyLevel = i->m_parentTransformComponent->m_transformHierarchyLevel + 1;
		}
	}
	//from top to bottom
	std::sort(g_GameSystemComponent->m_TransformComponents.begin(), g_GameSystemComponent->m_TransformComponents.end(), [&](TransformComponent* a, TransformComponent* b)
	{
		return a->m_transformHierarchyLevel < b->m_transformHierarchyLevel;
	});
}

void InnoGameSystemNS::updateTransform()
{
	std::for_each(InnoGameSystemNS::g_GameSystemComponent->m_TransformComponents.begin(), InnoGameSystemNS::g_GameSystemComponent->m_TransformComponents.end(), [&](TransformComponent* val)
	{
		val->m_localTransformMatrix = InnoMath::TransformVectorToTransformMatrix(val->m_localTransformVector);
		val->m_globalTransformVector = InnoMath::LocalTransformVectorToGlobal(val->m_localTransformVector, val->m_parentTransformComponent->m_globalTransformVector, val->m_parentTransformComponent->m_globalTransformMatrix);
		val->m_globalTransformMatrix = InnoMath::TransformVectorToTransformMatrix(val->m_globalTransformVector);
	});
}

// @TODO: add a cache function for after-rendering business
INNO_SYSTEM_EXPORT void InnoGameSystem::saveComponentsCapture()
{
	std::for_each(InnoGameSystemNS::g_GameSystemComponent->m_TransformComponents.begin(), InnoGameSystemNS::g_GameSystemComponent->m_TransformComponents.end(), [&](TransformComponent* val)
	{
		val->m_globalTransformMatrix_prev = val->m_globalTransformMatrix;
	});
}

INNO_SYSTEM_EXPORT void InnoGameSystem::setGameInstance(IGameInstance * rhs)
{
	InnoGameSystemNS::m_gameInstance = rhs;
}

EntityID InnoGameSystemNS::createEntity(const std::string & entityName)
{
	auto result = std::find_if(
		InnoGameSystemNS::g_GameSystemComponent->m_enitityNameMap.begin(),
		InnoGameSystemNS::g_GameSystemComponent->m_enitityNameMap.end(),
		[&](auto& val) -> bool {
		return val.second == entityName;
	});

	if (result != InnoGameSystemNS::g_GameSystemComponent->m_enitityNameMap.end())
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "GameSystem: duplicated entity name!");
		return 0;
	}

	auto l_entityID = InnoMath::createEntityID();
	InnoGameSystemNS::g_GameSystemComponent->m_enitityNameMap.emplace(l_entityID, entityName);
	return l_entityID;
}


INNO_SYSTEM_EXPORT EntityID InnoGameSystem::createEntity(const std::string & entityName)
{
	return InnoGameSystemNS::createEntity(entityName);
}

INNO_SYSTEM_EXPORT std::string InnoGameSystem::getEntityName(const EntityID & entityID)
{
	return InnoGameSystemNS::getEntityName(entityID);
}

INNO_SYSTEM_EXPORT bool InnoGameSystem::initialize()
{
	InnoGameSystemNS::sortTransformComponentsVector();
	InnoGameSystemNS::updateTransform();

	if (!InnoGameSystemNS::m_gameInstance->initialize())
	{
		return false;
	}

	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "GameSystem has been initialized.");
	return true;
}

INNO_SYSTEM_EXPORT bool InnoGameSystem::update()
{
	auto temp = g_pCoreSystem->getTaskSystem()->submit([]()
	{
		InnoGameSystemNS::updateTransform();
	});
	InnoGameSystemNS::g_GameSystemComponent->m_asyncTask = &temp;

	if (!InnoGameSystemNS::m_gameInstance->update(GameSystemComponent::get().m_pauseGameUpdate))
	{
		return false;
	}

	return true;
}

INNO_SYSTEM_EXPORT bool InnoGameSystem::terminate()
{
	if (!InnoGameSystemNS::m_gameInstance->terminate())
	{
		return false;
	}

	delete InnoGameSystemNS::g_GameSystemComponent->m_rootTransformComponent;

	InnoGameSystemNS::m_objectStatus = ObjectStatus::SHUTDOWN;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "GameSystem has been terminated.");
	return true;
}

#define spawnComponentImplDefi( className ) \
INNO_SYSTEM_EXPORT void InnoGameSystem::spawnComponent(className* rhs, const EntityID& parentEntity) \
{ \
	rhs->m_parentEntity = parentEntity; \
	InnoGameSystemNS::g_GameSystemComponent->m_##className##s.emplace_back(rhs); \
	InnoGameSystemNS::g_GameSystemComponent->m_##className##sMap.emplace(parentEntity, rhs); \
\
	auto indexOfTheComponent = InnoGameSystemNS::g_GameSystemComponent->m_##className##s.size(); \
	auto l_componentName = std::string(#className) + "_" + std::to_string(indexOfTheComponent); \
	auto l_componentType = InnoUtility::getComponentType<className>(); \
	auto l_componentMetaDataPair = componentMetadataPair(l_componentType, l_componentName); \
\
	auto result = InnoGameSystemNS::g_GameSystemComponent->m_enitityChildrenComponentsMetadataMap.find(parentEntity); \
	if (result != InnoGameSystemNS::g_GameSystemComponent->m_enitityChildrenComponentsMetadataMap.end()) \
	{ \
		auto l_componentMetadataMap = &result->second; \
		l_componentMetadataMap->emplace(rhs, l_componentMetaDataPair); \
	} \
	else \
	{ \
		auto l_componentMetadataMap = componentMetadataMap(); \
		l_componentMetadataMap.emplace(rhs, l_componentMetaDataPair); \
		InnoGameSystemNS::g_GameSystemComponent->m_enitityChildrenComponentsMetadataMap.emplace(parentEntity, std::move(l_componentMetadataMap)); \
	} \
}

spawnComponentImplDefi(TransformComponent)
spawnComponentImplDefi(VisibleComponent)
spawnComponentImplDefi(DirectionalLightComponent)
spawnComponentImplDefi(PointLightComponent)
spawnComponentImplDefi(SphereLightComponent)
spawnComponentImplDefi(CameraComponent)
spawnComponentImplDefi(InputComponent)
spawnComponentImplDefi(EnvironmentCaptureComponent)

INNO_SYSTEM_EXPORT std::string InnoGameSystem::getGameName()
{
	return std::string("GameInstance");
}

// @TODO: return multiple instances
#define getComponentImplDefi( className ) \
INNO_SYSTEM_EXPORT className* InnoGameSystem::get##className(const EntityID& parentEntity) \
{ \
	auto result = InnoGameSystemNS::g_GameSystemComponent->m_##className##sMap.find(parentEntity); \
	if (result != InnoGameSystemNS::g_GameSystemComponent->m_##className##sMap.end()) \
	{ \
		return result->second; \
	} \
	else \
	{ \
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "GameSystem : can't find " + std::string(#className) + " by EntityID: " + parentEntity + " !"); \
		return nullptr; \
	} \
}

getComponentImplDefi(TransformComponent)
getComponentImplDefi(VisibleComponent)
getComponentImplDefi(DirectionalLightComponent)
getComponentImplDefi(PointLightComponent)
getComponentImplDefi(SphereLightComponent)
getComponentImplDefi(CameraComponent)
getComponentImplDefi(InputComponent)
getComponentImplDefi(EnvironmentCaptureComponent)

INNO_SYSTEM_EXPORT void InnoGameSystem::registerButtonStatusCallback(InputComponent * inputComponent, ButtonData boundButton, std::function<void()>* function)
{
	auto l_kbuttonStatusCallbackVector = inputComponent->m_buttonStatusCallbackImpl.find(boundButton);
	if (l_kbuttonStatusCallbackVector != inputComponent->m_buttonStatusCallbackImpl.end())
	{
		l_kbuttonStatusCallbackVector->second.emplace_back(function);
	}
	else
	{
		inputComponent->m_buttonStatusCallbackImpl.emplace(boundButton, std::vector<std::function<void()>*>{function});
	}
}

INNO_SYSTEM_EXPORT TransformComponent * InnoGameSystem::getRootTransformComponent()
{
	return InnoGameSystemNS::g_GameSystemComponent->m_rootTransformComponent;
}

INNO_SYSTEM_EXPORT void InnoGameSystem::registerMouseMovementCallback(InputComponent * inputComponent, int mouseCode, std::function<void(float)>* function)
{
	auto l_mouseMovementCallbackVector = inputComponent->m_mouseMovementCallbackImpl.find(mouseCode);
	if (l_mouseMovementCallbackVector != inputComponent->m_mouseMovementCallbackImpl.end())
	{
		l_mouseMovementCallbackVector->second.emplace_back(function);
	}
	else
	{
		inputComponent->m_mouseMovementCallbackImpl.emplace(mouseCode, std::vector<std::function<void(float)>*>{function});
	}
}

INNO_SYSTEM_EXPORT ObjectStatus InnoGameSystem::getStatus()
{
	return InnoGameSystemNS::m_objectStatus;
}
