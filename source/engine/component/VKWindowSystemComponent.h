#pragma once
#include "../common/InnoType.h"
#include "../system/GLHeaders.h"

#include "vulkan/vulkan.h"
#define GLFW_INCLUDE_VULKAN

class VKWindowSystemComponent
{
public:
	~VKWindowSystemComponent() {};
	
	static VKWindowSystemComponent& get()
	{
		static VKWindowSystemComponent instance;

		return instance;
	}

	ObjectStatus m_objectStatus = ObjectStatus::SHUTDOWN;
	EntityID m_parentEntity;

	GLFWwindow* m_window;

private:
	VKWindowSystemComponent() {};
};
