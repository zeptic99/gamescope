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
	
#include <unordered_map>

class CVulkanTexture
{
public:
	bool BInit(uint32_t width, uint32_t height, VkFormat format, bool bFlippable, bool bTextureable, wlr_dmabuf_attributes *pDMA = nullptr );
	
	bool m_bInitialized = false;

	VkImage m_vkImage = VK_NULL_HANDLE;
	VkDeviceMemory m_vkImageMemory = VK_NULL_HANDLE;
	
	VkImageView m_vkImageView = VK_NULL_HANDLE;
	
	bool m_bFlippable = false;
	
	wlr_dmabuf_attributes m_DMA = {};
	uint32_t m_FBID = 0;
};

#endif

typedef uint32_t VulkanTexture_t;

int vulkan_init(void);

VulkanTexture_t vulkan_create_texture_from_dmabuf( struct wlr_dmabuf_attributes *pDMA );
void vulkan_free_texture( VulkanTexture_t vulkanTex );


#ifndef C_SIDE
}
#endif
