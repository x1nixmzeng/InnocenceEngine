#pragma once
#include "../common/InnoType.h"
#include "../exports/InnoSystem_Export.h"
#include "../common/InnoClassTemplate.h"

INNO_INTERFACE IRenderingSystem
{
public:
	INNO_CLASS_INTERFACE_NON_COPYABLE(IRenderingSystem);

	INNO_SYSTEM_EXPORT virtual bool setup() = 0;
	INNO_SYSTEM_EXPORT virtual bool initialize() = 0;
	INNO_SYSTEM_EXPORT virtual bool update() = 0;
	INNO_SYSTEM_EXPORT virtual bool terminate() = 0;

	INNO_SYSTEM_EXPORT virtual objectStatus getStatus() = 0;

	INNO_SYSTEM_EXPORT virtual bool resize() = 0;
};
