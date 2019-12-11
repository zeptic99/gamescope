// Initialize Vulkan and composite stuff with a compute queue

#pragma once

#include "drm.hpp"

#ifndef C_SIDE

#include <unordered_map>
#include <vector>

extern "C" {
#endif
	
#include <vulkan/vulkan.h>
#include <wlr/render/dmabuf.h>
#include <drm_fourcc.h>

#ifndef C_SIDE
	
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

extern std::vector< const char * > g_vecSDLInstanceExts;

#endif

typedef uint32_t VulkanTexture_t;

#define k_nMaxLayers 4

struct VulkanPipeline_t
{
	struct LayerBinding_t
	{
		VulkanTexture_t tex;
		bool bFilter;
		bool bBlackBorder;
	} layerBindings[ k_nMaxLayers ];
};

struct Composite_t
{
	float flLayerCount;

	struct
	{
		float flScaleX, flScaleY;
		float flOffsetX, flOffsetY;
		float flOpacity;
	} layers[ k_nMaxLayers ];
};

int vulkan_init(void);

VulkanTexture_t vulkan_create_texture_from_dmabuf( struct wlr_dmabuf_attributes *pDMA );
void vulkan_free_texture( VulkanTexture_t vulkanTex );

bool vulkan_composite( struct Composite_t *pComposite, struct VulkanPipeline_t *pPipeline );
uint32_t vulkan_get_last_composite_fbid( void );

void vulkan_present_to_window( void );

#ifndef C_SIDE
}
#endif
