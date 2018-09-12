#pragma once
#include "../../common/InnoType.h"

namespace GLRenderingSystem
{
	void setup();
	void initialize();
	void update();
	void shutdown();

	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
};