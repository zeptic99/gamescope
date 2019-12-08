// Initialize Vulkan and composite stuff with a compute queue

#pragma once

#include "drm.hpp"

#ifndef C_SIDE
extern "C" {
#endif
	
#include <vulkan/vulkan.h>
#include <wlr/render/dmabuf.h>
#include <drm_fourcc.h>

#ifndef C_SIDE
class CVulkanOutputImage
{
public:
	bool BInit(uint32_t width, uint32_t height, VkFormat format);

	VkImage m_vkImage;
	VkDeviceMemory m_vkImageMemory;
	
	VkImageView m_vkImageView;
	
	wlr_dmabuf_attributes m_DMA;
	uint32_t m_FBID;
};
#endif

int init_vulkan(void);

#ifndef C_SIDE
}
#endif
