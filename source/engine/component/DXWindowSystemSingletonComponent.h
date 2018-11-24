#pragma once
#include "../common/InnoType.h"
#include <windows.h>
#include <windowsx.h>

class DXWindowSystemSingletonComponent
{
public:
	~DXWindowSystemSingletonComponent() {};
	
	static DXWindowSystemSingletonComponent& getInstance()
	{
		static DXWindowSystemSingletonComponent instance;

		return instance;
	}

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	HINSTANCE m_hInstance;
	PSTR m_pScmdline;
	int m_nCmdshow;
	LPCSTR m_applicationName;
	HWND m_hwnd;

private:
	DXWindowSystemSingletonComponent() {};
};
