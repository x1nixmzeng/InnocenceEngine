#pragma once
#include "../common/InnoType.h"
#include "../exports/InnoSystem_Export.h"
#include "../common/InnoClassTemplate.h"

INNO_INTERFACE ITimeSystem
{
public:
	INNO_CLASS_INTERFACE_NON_COPYABLE(ITimeSystem);

	INNO_SYSTEM_EXPORT virtual bool setup() = 0;
	INNO_SYSTEM_EXPORT virtual bool initialize() = 0;
	INNO_SYSTEM_EXPORT virtual bool update() = 0;
	INNO_SYSTEM_EXPORT virtual bool terminate() = 0;

	INNO_SYSTEM_EXPORT virtual const long long getStartTime() = 0;
	INNO_SYSTEM_EXPORT virtual const long long getDeltaTime() = 0;
	INNO_SYSTEM_EXPORT virtual const long long getCurrentTime() = 0;
	INNO_SYSTEM_EXPORT virtual const std::tuple<int, unsigned, unsigned> getCivilFromDays(int z) = 0;
	INNO_SYSTEM_EXPORT virtual const TimeData getCurrentTimeInLocal(unsigned int timezone_adjustment = 8) = 0;

	INNO_SYSTEM_EXPORT virtual ObjectStatus getStatus() = 0;
};
