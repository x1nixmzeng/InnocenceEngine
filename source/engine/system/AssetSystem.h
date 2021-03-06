#pragma once
#include "IAssetSystem.h"

class InnoAssetSystem : INNO_IMPLEMENT IAssetSystem
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(InnoAssetSystem);

	INNO_SYSTEM_EXPORT bool setup() override;
	INNO_SYSTEM_EXPORT bool initialize() override;
	INNO_SYSTEM_EXPORT bool update() override;
	INNO_SYSTEM_EXPORT bool terminate() override;

	INNO_SYSTEM_EXPORT ObjectStatus getStatus() override;

	INNO_SYSTEM_EXPORT void loadDefaultAssets() override;
	INNO_SYSTEM_EXPORT void loadAssetsForComponents() override;

	INNO_SYSTEM_EXPORT MeshDataComponent* addMeshDataComponent() override;
	INNO_SYSTEM_EXPORT MaterialDataComponent* addMaterialDataComponent() override;
	INNO_SYSTEM_EXPORT TextureDataComponent* addTextureDataComponent() override;
	INNO_SYSTEM_EXPORT MeshDataComponent* getMeshDataComponent(EntityID meshID) override;
	INNO_SYSTEM_EXPORT TextureDataComponent* getTextureDataComponent(EntityID textureID) override;
	INNO_SYSTEM_EXPORT MeshDataComponent* getMeshDataComponent(MeshShapeType MeshShapeType) override;
	INNO_SYSTEM_EXPORT TextureDataComponent* getTextureDataComponent(TextureUsageType TextureUsageType) override;
	INNO_SYSTEM_EXPORT TextureDataComponent* getTextureDataComponent(FileExplorerIconType iconType) override;
	INNO_SYSTEM_EXPORT TextureDataComponent* getTextureDataComponent(WorldEditorIconType iconType) override;
	INNO_SYSTEM_EXPORT bool removeMeshDataComponent(EntityID EntityID) override;
	INNO_SYSTEM_EXPORT bool removeTextureDataComponent(EntityID EntityID) override;
	INNO_SYSTEM_EXPORT bool releaseRawDataForMeshDataComponent(EntityID EntityID) override;
	INNO_SYSTEM_EXPORT bool releaseRawDataForTextureDataComponent(EntityID EntityID) override;
};

