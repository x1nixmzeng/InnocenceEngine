#include "InnoApplication.h"
#include "../system/CoreSystem.h"

namespace InnoApplication
{
	objectStatus m_objectStatus = objectStatus::SHUTDOWN;

	ICoreSystem* g_pCoreSystem;
	std::unique_ptr<InnoCoreSystem> m_pCoreSystem;
}

bool InnoApplication::setup(void* hInstance, void* hPrevInstance, char* pScmdline, int nCmdshow)
{	
	m_pCoreSystem = std::make_unique<InnoCoreSystem>();
	g_pCoreSystem = m_pCoreSystem.get();

	if (g_pCoreSystem)
	{
		if (!g_pCoreSystem->setup())
		{
			return false;
		}

		if (!g_pCoreSystem->getTimeSystem()->setup())
		{
			return false;
		}
		g_pCoreSystem->getLogSystem()->printLog("TimeSystem setup finished.");

		if (!g_pCoreSystem->getLogSystem()->setup())
		{
			return false;
		}
		g_pCoreSystem->getLogSystem()->printLog("LogSystem setup finished.");

		if (!g_pCoreSystem->getMemorySystem()->setup())
		{
			return false;
		}
		g_pCoreSystem->getLogSystem()->printLog("MemorySystem setup finished.");

		if (!g_pCoreSystem->getTaskSystem()->setup())
		{
			return false;
		}
		g_pCoreSystem->getLogSystem()->printLog("TaskSystem setup finished.");

		//if (!InnoGameSystem::setup())
		//{
		//	return false;
		//}
		//InnoLogSystem::printLog("GameSystem setup finished.");

		//if (!InnoAssetSystem::setup())
		//{
		//	return false;
		//}
		//InnoLogSystem::printLog("AssetSystem setup finished.");

		//if (!InnoPhysicsSystem::setup())
		//{
		//	return false;
		//}
		//InnoLogSystem::printLog("PhysicsSystem setup finished.");

		//if (!InnoVisionSystem::setup(hInstance, hPrevInstance, pScmdline, nCmdshow))
		//{
		//	return false;
		//}
		//InnoLogSystem::printLog("VisionSystem setup finished.");

		m_objectStatus = objectStatus::ALIVE;

		g_pCoreSystem->getLogSystem()->printLog("Engine setup finished.");
		return true;
	}
	else
	{
		return false;
	}
}

bool InnoApplication::initialize()
{
	// @TODO: return value check
	if (!g_pCoreSystem->getTimeSystem()->initialize())
	{
		return false;
	}

	if (!g_pCoreSystem->getLogSystem()->initialize())
	{
		return false;
	}

	if (!g_pCoreSystem->getMemorySystem()->initialize())
	{
		return false;
	}

	if (!g_pCoreSystem->getTaskSystem()->initialize())
	{
		return false;
	}

	//InnoGameSystem::initialize();
	//InnoAssetSystem::initialize();
	//InnoPhysicsSystem::initialize();
	//InnoVisionSystem::initialize();

	//InnoLogSystem::printLog("Engine has been initialized.");
	return true;
}

bool InnoApplication::update()
{
	// @TODO: return value check
	if (!g_pCoreSystem->getTimeSystem()->update())
	{
		return false;
	}

	if (!g_pCoreSystem->getLogSystem()->update())
	{
		return false;
	}

	if (!g_pCoreSystem->getMemorySystem()->update())
	{
		return false;
	}

	if (!g_pCoreSystem->getTaskSystem()->update())
	{
		return false;
	}

	//InnoGameSystem::update();
	//InnoAssetSystem::update();
	//InnoPhysicsSystem::update();

	//if (InnoVisionSystem::getStatus() == objectStatus::ALIVE)
	//{	
	//	InnoVisionSystem::update();
	//	InnoGameSystem::saveComponentsCapture();
	//	return true;
	//}
	//else
	//{
	//	m_objectStatus = objectStatus::STANDBY;
	//	InnoLogSystem::printLog("Engine is stand-by.");
	//	return false;
	//}
	return true;
}

bool InnoApplication::terminate()
{
	// @TODO: return value check
	//InnoVisionSystem::terminate();
	//InnoPhysicsSystem::terminate();

	//InnoAssetSystem::terminate();
	//InnoGameSystem::terminate();

	if (!g_pCoreSystem->getTaskSystem()->terminate())
	{
		return false;
	}

	if (!g_pCoreSystem->getMemorySystem()->terminate())
	{
		return false;
	}

	if (!g_pCoreSystem->getLogSystem()->terminate())
	{
		return false;
	}

	if (!g_pCoreSystem->getTimeSystem()->terminate())
	{
		return false;
	}

	m_objectStatus = objectStatus::SHUTDOWN;
	return true;
}

objectStatus InnoApplication::getStatus()
{
	return m_objectStatus;
}

