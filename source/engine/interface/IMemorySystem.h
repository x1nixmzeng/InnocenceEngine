#pragma once
#include "common/stdafx.h"
#include "ISystem.h"

class IMemorySystem : public ISystem
{
public:
	virtual ~IMemorySystem() {};

	virtual void* allocate(unsigned long size) = 0;
	virtual void free(void* ptr) = 0;
	template <typename T> T * spawn()
	{
		return new(allocate(sizeof(T))) T();
	};
	template <typename T> void destroy(T *p)
	{
		reinterpret_cast<T *>(p)->~T();
		free(p);
	};
	virtual void dumpToFile(const std::string& fileName) const = 0;
};