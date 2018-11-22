#pragma once
#include "../common/InnoType.h"
#include "../common/InnoMath.h"

class MeshDataComponent
{
public:
	MeshDataComponent() {};
	~MeshDataComponent() {};

	objectStatus m_objectStatus = objectStatus::SHUTDOWN;
	EntityID m_parentEntity = 0;

	meshType m_meshType = meshType::NORMAL;
	std::vector<Vertex> m_vertices;
	std::vector<Index> m_indices;
	size_t m_indicesSize = 0;
	meshDrawMethod m_meshDrawMethod = meshDrawMethod::TRIANGLE;
};

