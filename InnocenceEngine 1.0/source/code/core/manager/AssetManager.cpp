
#include "../../main/stdafx.h"
#include "AssetManager.h"
#define STB_IMAGE_IMPLEMENTATION    
#include "../third-party/stb_image.h"

AssetManager::AssetManager()
{
}

AssetManager::~AssetManager()
{
}

void AssetManager::initialize()
{
}

void AssetManager::update()
{
}

void AssetManager::shutdown()
{
}

std::string AssetManager::loadShader(const std::string & shaderFileName) const
{
	std::ifstream file;
	file.open(("../res/shaders/" + shaderFileName).c_str());
	std::stringstream shaderStream;
	std::string output;

	shaderStream << file.rdbuf();
	output = shaderStream.str();
	file.close();

	return output;
}

void AssetManager::importModel(const std::string& fileName) const
{
	// read file via ASSIMP
	Assimp::Importer l_assImporter;
	const aiScene* l_assScene = l_assImporter.ReadFile("../res/models/" + fileName, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
	// check for errors
	if (!l_assScene || l_assScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !l_assScene->mRootNode)
	{
		LogManager::getInstance().printLog("ERROR:ASSIMP: " + std::string{ l_assImporter.GetErrorString() });
		return;
	}
	if (l_assImporter.ReadFile("../res/models/" + fileName.substr(0, fileName.find(".")) + ".innoModel", aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace) != nullptr)
	{
		LogManager::getInstance().printLog("Model: " + fileName + " has been already imported.");
		return;
	}
	Assimp::Exporter l_assExporter;
	l_assExporter.Export(l_assScene, "assbin", "../res/models/" + fileName.substr(0, fileName.find(".")) + ".innoModel", 0u, 0);
	LogManager::getInstance().printLog("Model: " + fileName + " is imported.");
}

void AssetManager::loadModel(const std::string & fileName, VisibleComponent & visibleComponent)
{
	loadModelImpl(fileName, visibleComponent);
}

void AssetManager::loadModelImpl(const std::string & fileName, VisibleComponent & visibleComponent)
{
	auto l_loadedAssetData = m_loadedAssetMap.find(fileName);
	// check if this file has already loaded
	if (l_loadedAssetData != m_loadedAssetMap.end())
	{
		assignloadedModel(l_loadedAssetData->second, visibleComponent);
		LogManager::getInstance().printLog("innoModel: " + fileName + " is already loaded, successfully assigned loaded graphic data IDs.");
	}
	else
	{
		loadModelFromDisk(fileName, visibleComponent);
	}
}

void AssetManager::assignloadedModel(graphicDataMap& loadedGraphicDataMap, VisibleComponent & visibleComponent)
{
	visibleComponent.setGraphicDataMap(loadedGraphicDataMap);
}

void AssetManager::loadModelFromDisk(const std::string& fileName, VisibleComponent& visibleComponent)
{
	// read file via ASSIMP
	Assimp::Importer l_assImporter;
	const aiScene* l_assScene = l_assImporter.ReadFile("../res/models/" + fileName, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
	// check for errors
	if (!l_assScene || l_assScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !l_assScene->mRootNode)
	{
		LogManager::getInstance().printLog("ERROR:ASSIMP: " + std::string{ l_assImporter.GetErrorString() });
		return;
	}
	// record that file's been loaded 
	m_loadedAssetMap.emplace(fileName, graphicDataMap());

	// find corresponded graphicDataMap
	auto l_graphicDataMap = &m_loadedAssetMap.find(fileName)->second;

	// only need last part of file name without subfix as material's subfolder name
	std::string l_fileName = fileName.substr(fileName.find_last_of('/') + 1, fileName.find_last_of('.') - fileName.find_last_of('/') - 1);

	//check if root node has mesh attached, btw there SHOULD NOT BE ANY MESH ATTACHED TO ROOT NODE!!!
	if (l_assScene->mRootNode->mNumMeshes > 0)
	{
		processAssimpNode(l_fileName, l_assScene->mRootNode, l_assScene, l_graphicDataMap, visibleComponent);
	}
	for (auto i = (unsigned int)0; i < l_assScene->mRootNode->mNumChildren; i++)
	{
		if (l_assScene->mRootNode->mChildren[i]->mNumMeshes > 0)
		{
			processAssimpNode(l_fileName, l_assScene->mRootNode->mChildren[i], l_assScene, l_graphicDataMap, visibleComponent);
		}
	}

	visibleComponent.setGraphicDataMap(*l_graphicDataMap);

	LogManager::getInstance().printLog("innoModel: " + fileName + " is loaded.");
}


void AssetManager::processAssimpNode(const std::string& fileName, aiNode * node, const aiScene * scene, graphicDataMap* graphicDataMap, VisibleComponent & visibleComponent)
{
	// process each mesh located at the current node
	for (auto i = (unsigned int)0; i < node->mNumMeshes; i++)
	{
		auto l_meshDataID = RenderingManager::getInstance().addMeshData();

		graphicDataMap->emplace(l_meshDataID, textureDataMap());

		processSingleAssimpMesh(scene->mMeshes[node->mMeshes[i]], l_meshDataID, visibleComponent.getMeshDrawMethod());

		// process material if there was anyone
		if (scene->mMeshes[node->mMeshes[i]]->mMaterialIndex > 0)
		{
			auto l_textureDataMap = &graphicDataMap->find(l_meshDataID)->second;

			processSingleAssimpMaterial(fileName, scene->mMaterials[scene->mMeshes[node->mMeshes[i]]->mMaterialIndex], visibleComponent.getTextureWrapMethod(), l_textureDataMap);
		}
	}
}

void AssetManager::processSingleAssimpMesh(aiMesh*mesh, meshDataID meshDataID, meshDrawMethod meshDrawMethod) const
{
	auto lastMeshData = &RenderingManager::getInstance().getMeshData(meshDataID);

	for (auto i = (unsigned int)0; i < mesh->mNumVertices; i++)
	{
		addVertexData(mesh, i, lastMeshData);
	}

	// now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
	for (auto i = (unsigned int)0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		// retrieve all indices of the face and store them in the indices vector
		for (auto j = (unsigned int)0; j < face.mNumIndices; j++)
		{
			lastMeshData->getIntices().emplace_back(face.mIndices[j]);
		}
	}
	lastMeshData->setMeshDrawMethod(meshDrawMethod);
	lastMeshData->initialize();
	lastMeshData->sendDataToGPU();
	LogManager::getInstance().printLog("innoMesh is loaded.");
}

void AssetManager::addVertexData(aiMesh * aiMesh, int vertexIndex, MeshData * meshData) const
{
	GLVertexData vertexData;
	// positions
	vertexData.m_pos.x = aiMesh->mVertices[vertexIndex].x;
	vertexData.m_pos.y = aiMesh->mVertices[vertexIndex].y;
	vertexData.m_pos.z = aiMesh->mVertices[vertexIndex].z;

	// texture coordinates
	if (aiMesh->mTextureCoords[0])
	{
		// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
		// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
		vertexData.m_texCoord.x = aiMesh->mTextureCoords[0][vertexIndex].x;
		vertexData.m_texCoord.y = aiMesh->mTextureCoords[0][vertexIndex].y;
	}
	else
	{
		vertexData.m_texCoord.x = 0.0f;
		vertexData.m_texCoord.y = 0.0f;
	}

	// normals
	if (aiMesh->mNormals)
	{
		vertexData.m_normal.x = aiMesh->mNormals[vertexIndex].x;
		vertexData.m_normal.y = aiMesh->mNormals[vertexIndex].y;
		vertexData.m_normal.z = aiMesh->mNormals[vertexIndex].z;
	}
	else
	{
		vertexData.m_normal.x = 0.0f;
		vertexData.m_normal.y = 0.0f;
		vertexData.m_normal.z = 0.0f;
	}

	// tangent
	if (aiMesh->mTangents)
	{
		vertexData.m_tangent.x = aiMesh->mTangents[vertexIndex].x;
		vertexData.m_tangent.y = aiMesh->mTangents[vertexIndex].y;
		vertexData.m_tangent.z = aiMesh->mTangents[vertexIndex].z;
	}
	else
	{
		vertexData.m_tangent.x = 0.0f;
		vertexData.m_tangent.y = 0.0f;
		vertexData.m_tangent.z = 0.0f;
	}


	// bitangent
	if (aiMesh->mBitangents)
	{
		vertexData.m_bitangent.x = aiMesh->mBitangents[vertexIndex].x;
		vertexData.m_bitangent.y = aiMesh->mBitangents[vertexIndex].y;
		vertexData.m_bitangent.z = aiMesh->mBitangents[vertexIndex].z;
	}
	else
	{
		vertexData.m_bitangent.x = 0.0f;
		vertexData.m_bitangent.y = 0.0f;
		vertexData.m_bitangent.z = 0.0f;
	}

	meshData->getVertices().emplace_back(vertexData);
}

void AssetManager::processSingleAssimpMaterial(const std::string& fileName, aiMaterial * aiMaterial, textureWrapMethod textureWrapMethod, textureDataMap* textureDataMap) const
{
	for (auto i = (unsigned int)0; i < aiTextureType_UNKNOWN; i++)
	{
		if (aiMaterial->GetTextureCount(aiTextureType(i)) > 0)
		{
			loadTexture(fileName, aiMaterial, aiTextureType(i), textureWrapMethod, textureDataMap);
		}
	}
}

void AssetManager::loadTexture(const std::string& fileName, aiMaterial * aiMaterial, aiTextureType aiTextureType, textureWrapMethod textureWrapMethod, textureDataMap* textureDataMap) const
{
	aiString l_AssString;
	for (auto i = (unsigned int)0; i < aiMaterial->GetTextureCount(aiTextureType); i++)
	{
		aiMaterial->GetTexture(aiTextureType, i, &l_AssString);
		// set local path, remove slash
		std::string l_localPath;
		if (std::string(l_AssString.C_Str()).find_last_of('//') != std::string::npos)
		{
			l_localPath = std::string(l_AssString.C_Str()).substr(std::string(l_AssString.C_Str()).find_last_of("//"));
		}
		else if (std::string(l_AssString.C_Str()).find_last_of('\\') != std::string::npos)
		{
			l_localPath = std::string(l_AssString.C_Str()).substr(std::string(l_AssString.C_Str()).find_last_of("\\"));
		}
		else
		{
			l_localPath = std::string(l_AssString.C_Str());
		}

		// load image
		int width, height, nrChannels;
		stbi_set_flip_vertically_on_load(true);
		auto data = stbi_load(("../res/textures/" + fileName + "//" + l_localPath).c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			auto l_textureDataID = RenderingManager::getInstance().addTextureData();
			textureDataMap->emplace(textureType(aiTextureType), l_textureDataID);
			auto lastTextureData = &RenderingManager::getInstance().getTextureData(l_textureDataID);
			lastTextureData->setTextureType(textureType(aiTextureType));
			lastTextureData->setTextureWrapMethod(textureWrapMethod);
			lastTextureData->initialize();
			lastTextureData->sendDataToGPU(aiTextureType - 1, nrChannels, width, height, data);
			LogManager::getInstance().printLog("innoTexture: " + l_localPath + " is loaded.");
		}
		else
		{
			LogManager::getInstance().printLog("ERROR:STBI: Failed to load texture: " + l_localPath);
		}
		//stbi_image_free(data);
	}
}

void AssetManager::loadTexture(const std::string & fileName, textureType textureType, VisibleComponent & visibleComponent) const
{
	int width, height, nrChannels;
	// load image
	stbi_set_flip_vertically_on_load(true);
	auto *data = stbi_load(("../res/textures/" + fileName).c_str(), &width, &height, &nrChannels, 0);
	if (data)
	{
		auto id = RenderingManager::getInstance().addTextureData();
		auto lastTextureData = &RenderingManager::getInstance().getTextureData(id);
		lastTextureData->setTextureType(textureType);
		lastTextureData->setTextureWrapMethod(visibleComponent.getTextureWrapMethod());
		lastTextureData->initialize();
		lastTextureData->sendDataToGPU(0, nrChannels, width, height, data);
		visibleComponent.addTextureData(id, textureType);
		LogManager::getInstance().printLog("innoTexture: " + fileName + " is loaded.");
	}
	else
	{
		LogManager::getInstance().printLog("ERROR::STBI:: Failed to load texture: " + fileName);
	}
	//stbi_image_free(data);
}

void AssetManager::loadTexture(const std::vector<std::string>& fileName, VisibleComponent & visibleComponent) const
{
	int width, height, nrChannels;
	for (auto i = (unsigned int)0; i < fileName.size(); i++)
	{
		// load image, do not flip texture
		stbi_set_flip_vertically_on_load(false);
		auto *data = stbi_load(("../res/textures/" + fileName[i]).c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			auto id = RenderingManager::getInstance().addTextureData();
			auto lastTextureData = &RenderingManager::getInstance().getTextureData(id);
			lastTextureData->setTextureType(textureType::CUBEMAP);
			lastTextureData->setTextureWrapMethod(visibleComponent.getTextureWrapMethod());
			lastTextureData->initialize();
			lastTextureData->sendDataToGPU(i, nrChannels, width, height, data);
			visibleComponent.addTextureData(id, textureType::CUBEMAP);
			LogManager::getInstance().printLog("innoTexture: " + fileName[i] + " is loaded.");
		}
		else
		{
			LogManager::getInstance().printLog("ERROR::STBI:: Failed to load texture: " + ("../res/textures/" + fileName[i]));
		}
		//stbi_image_free(data);
	}
}


void AssetManager::addUnitCube(VisibleComponent & visibleComponent) const
{
	auto id = RenderingManager::getInstance().addMeshData();
	auto lastMeshData = &RenderingManager::getInstance().getMeshData(id);
	lastMeshData->addUnitCube();
	lastMeshData->initialize();
	lastMeshData->sendDataToGPU();
	visibleComponent.addMeshData(id);
}

void AssetManager::addUnitSphere(VisibleComponent & visibleComponent) const
{
	auto id = RenderingManager::getInstance().addMeshData();
	auto lastMeshData = &RenderingManager::getInstance().getMeshData(id);
	lastMeshData->addUnitSphere();
	lastMeshData->initialize();
	lastMeshData->sendDataToGPU();
	visibleComponent.addMeshData(id);
}

void AssetManager::addUnitQuad(VisibleComponent & visibleComponent) const
{
	auto id = RenderingManager::getInstance().addMeshData();
	auto lastMeshData = &RenderingManager::getInstance().getMeshData(id);
	lastMeshData->addUnitQuad();
	lastMeshData->initialize();
	lastMeshData->sendDataToGPU();
	visibleComponent.addMeshData(id);
}
