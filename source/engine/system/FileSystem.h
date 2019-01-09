#pragma once
#include "IFileSystem.h"

class InnoFileSystem : INNO_IMPLEMENT IFileSystem
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(InnoFileSystem);

	INNO_SYSTEM_EXPORT bool setup() override;
	INNO_SYSTEM_EXPORT bool initialize() override;
	INNO_SYSTEM_EXPORT bool update() override;
	INNO_SYSTEM_EXPORT bool terminate() override;

	INNO_SYSTEM_EXPORT ObjectStatus getStatus() override;

	INNO_SYSTEM_EXPORT std::string loadTextFile(const std::string& fileName) override;

	INNO_SYSTEM_EXPORT bool loadDefaultScene() override;
	INNO_SYSTEM_EXPORT bool loadScene(const std::string& fileName) override;
	INNO_SYSTEM_EXPORT bool saveScene(const std::string& fileName) override;

	INNO_SYSTEM_EXPORT bool addSceneLoadingCallback(std::function<void()>* functor) override;

	INNO_SYSTEM_EXPORT bool convertAsset(const std::string & fileName, const std::string & exportPath) override;
};
