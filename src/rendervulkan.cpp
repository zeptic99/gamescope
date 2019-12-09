// Initialize Vulkan and composite stuff with a compute queue

#include "rendervulkan.hpp"
#include "main.hpp"

#include "composite.h"

PFN_vkGetMemoryFdKHR dyn_vkGetMemoryFdKHR;

const VkApplicationInfo appInfo = {
	.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
	.pApplicationName = "steamcompmgr",
	.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
	.pEngineName = "just some code",
	.engineVersion = VK_MAKE_VERSION(1, 0, 0),
	.apiVersion = VK_API_VERSION_1_0,
};

VkInstance instance = VK_NULL_HANDLE;

VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
uint32_t queueFamilyIndex;
VkQueue queue;
VkDevice device = VK_NULL_HANDLE;

struct VkPhysicalDeviceMemoryProperties memoryProperties;

CVulkanTexture outputImage[2];

std::unordered_map<VulkanTexture_t, CVulkanTexture *> g_mapVulkanTextures;
std::atomic<VulkanTexture_t> g_nMaxVulkanTexHandle;

#define MAX_DEVICE_COUNT 8
#define MAX_QUEUE_COUNT 8

#define VK_STRUCTURE_TYPE_WSI_IMAGE_CREATE_INFO_MESA (VkStructureType)1000001002
#define VK_STRUCTURE_TYPE_WSI_MEMORY_ALLOCATE_INFO_MESA (VkStructureType)1000001003

struct wsi_image_create_info {
	VkStructureType sType;
	const void *pNext;
	bool scanout;
	
	uint32_t modifier_count;
	const uint64_t *modifiers;
};

struct wsi_memory_allocate_info {
	VkStructureType sType;
	const void *pNext;
	bool implicit_sync;
};

struct {
	uint32_t DRMFormat;
	VkFormat vkFormat;
} s_DRMVKFormatTable[] = {
	{ DRM_FORMAT_XRGB8888, VK_FORMAT_A8B8G8R8_UNORM_PACK32 },
	{ DRM_FORMAT_ARGB8888, VK_FORMAT_A8B8G8R8_UNORM_PACK32 },
	{ DRM_FORMAT_RGBA8888, VK_FORMAT_R8G8B8A8_UNORM },
	{ DRM_FORMAT_INVALID, VK_FORMAT_UNDEFINED },
};

static inline uint32_t VulkanFormatToDRM( VkFormat vkFormat )
{
	for ( int i = 0; s_DRMVKFormatTable[i].vkFormat != VK_FORMAT_UNDEFINED; i++ )
	{
		if ( s_DRMVKFormatTable[i].vkFormat == vkFormat )
		{
			return s_DRMVKFormatTable[i].DRMFormat;
		}
	}
	
	return DRM_FORMAT_INVALID;
}

static inline VkFormat DRMFormatToVulkan( uint32_t nDRMFormat )
{
	for ( int i = 0; s_DRMVKFormatTable[i].vkFormat != VK_FORMAT_UNDEFINED; i++ )
	{
		if ( s_DRMVKFormatTable[i].DRMFormat == nDRMFormat )
		{
			return s_DRMVKFormatTable[i].vkFormat;
		}
	}
	
	return VK_FORMAT_UNDEFINED;
}

int32_t findMemoryType( VkMemoryPropertyFlags properties, uint32_t requiredTypeBits )
{
	for ( uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++ )
	{
		if ( ( ( 1 << i ) & requiredTypeBits ) == 0 )
			continue;
		
		if ( ( properties & memoryProperties.memoryTypes[ i ].propertyFlags ) != properties )
			continue;
		
		return i;
	}
	
	return -1;
}

bool CVulkanTexture::BInit( uint32_t width, uint32_t height, VkFormat format, bool bFlippable, bool bTextureable, wlr_dmabuf_attributes *pDMA /* = nullptr */ )
{
	VkResult res = VK_ERROR_INITIALIZATION_FAILED;

	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageUsageFlags usage = bTextureable ? VK_IMAGE_USAGE_SAMPLED_BIT : VK_IMAGE_USAGE_STORAGE_BIT;

	VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	
	// Possible extensions for below
	wsi_image_create_info wsiImageCreateInfo = {};
	VkExternalMemoryImageCreateInfo externalImageCreateInfo = {};
	
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	if ( pDMA != nullptr )
	{
		assert( format == DRMFormatToVulkan( pDMA->format ) );
	}
	
	if ( bFlippable == true )
	{
		wsiImageCreateInfo.sType = VK_STRUCTURE_TYPE_WSI_IMAGE_CREATE_INFO_MESA;
		wsiImageCreateInfo.scanout = VK_TRUE;
		wsiImageCreateInfo.pNext = imageInfo.pNext;
		
		imageInfo.pNext = &wsiImageCreateInfo;
	}
	
	if ( pDMA != nullptr )
	{
		externalImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
		externalImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
		externalImageCreateInfo.pNext = imageInfo.pNext;
		
		imageInfo.pNext = &externalImageCreateInfo;
	}
	
	if (vkCreateImage(device, &imageInfo, nullptr, &m_vkImage) != VK_SUCCESS) {
		return false;
	}
	
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, m_vkImage, &memRequirements);
	
	// Possible pNexts
	wsi_memory_allocate_info wsiAllocInfo = {};
	VkImportMemoryFdInfoKHR importMemoryInfo = {};
	VkExportMemoryAllocateInfo memory_export_info = {};
	VkMemoryDedicatedAllocateInfo memory_dedicated_info = {};
	
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(properties, memRequirements.memoryTypeBits );
	
	if ( bFlippable == true || pDMA != nullptr )
	{
		memory_dedicated_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
		memory_dedicated_info.image = m_vkImage;
		memory_dedicated_info.buffer = VK_NULL_HANDLE;
		memory_dedicated_info.pNext = allocInfo.pNext;
		
		allocInfo.pNext = &memory_dedicated_info;
	}
	
	if ( bFlippable == true )
	{
		wsiAllocInfo.sType = VK_STRUCTURE_TYPE_WSI_MEMORY_ALLOCATE_INFO_MESA;
		wsiAllocInfo.implicit_sync = true;
		wsiAllocInfo.pNext = allocInfo.pNext;
		
		allocInfo.pNext = &wsiAllocInfo;
		
		memory_export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
		memory_export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
		memory_export_info.pNext = allocInfo.pNext;
		
		allocInfo.pNext = &memory_export_info;
	}
	
	if ( pDMA != nullptr )
	{
		// Memory already provided by pDMA
		importMemoryInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
		importMemoryInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
		importMemoryInfo.fd = pDMA->fd[0];
		importMemoryInfo.pNext = allocInfo.pNext;
		
		allocInfo.pNext = &importMemoryInfo;
	}
	
	if (vkAllocateMemory(device, &allocInfo, nullptr, &m_vkImageMemory) != VK_SUCCESS) {
		return false;
	}
	
	res = vkBindImageMemory(device, m_vkImage, m_vkImageMemory, 0);
	
	if ( res != VK_SUCCESS )
		return false;
	
	if ( bFlippable == true )
	{
		// We assume we own the memory when doing this right now.
		// We could support the import scenario as well if needed
		assert( bTextureable == false );

		m_DMA.modifier = DRM_FORMAT_MOD_INVALID;
		m_DMA.n_planes = 1;
		m_DMA.width = width;
		m_DMA.height = height;
		m_DMA.format = VulkanFormatToDRM( format );

		const VkMemoryGetFdInfoKHR memory_get_fd_info = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
			.pNext = NULL,
			.memory = m_vkImageMemory,
			.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
		};
		res = dyn_vkGetMemoryFdKHR(device, &memory_get_fd_info, &m_DMA.fd[0]);
		
		if ( res != VK_SUCCESS )
			return false;
		
		const VkImageSubresource image_subresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.arrayLayer = 0,
		};
		VkSubresourceLayout image_layout;
		vkGetImageSubresourceLayout(device, m_vkImage, &image_subresource, &image_layout);
		

		m_DMA.stride[0] = image_layout.rowPitch;
		
		m_FBID = drm_fbid_from_dmabuf( &g_DRM, &m_DMA );
		
		if ( m_FBID == 0 )
			return false;
	}
	
	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = m_vkImage;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;
	
	res = vkCreateImageView(device, &createInfo, nullptr, &m_vkImageView);
	if ( res != VK_SUCCESS )
		return false;
	
	m_bInitialized = true;
	m_bFlippable = bFlippable;
	
	return true;
}


int init_device()
{
	uint32_t physicalDeviceCount;
	VkPhysicalDevice deviceHandles[MAX_DEVICE_COUNT];
	VkQueueFamilyProperties queueFamilyProperties[MAX_QUEUE_COUNT];
	
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0);
	physicalDeviceCount = physicalDeviceCount > MAX_DEVICE_COUNT ? MAX_DEVICE_COUNT : physicalDeviceCount;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceHandles);
	
	for (uint32_t i = 0; i < physicalDeviceCount; ++i)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, NULL);
		queueFamilyCount = queueFamilyCount > MAX_QUEUE_COUNT ? MAX_QUEUE_COUNT : queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, queueFamilyProperties);
		
		for (uint32_t j = 0; j < queueFamilyCount; ++j) {
			if ( queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT &&
				!(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT ) )
			{
				queueFamilyIndex = j;
				physicalDevice = deviceHandles[i];
			}
		}
		
		if (physicalDevice)
		{
			break;
		}
	}
	
	vkGetPhysicalDeviceMemoryProperties( physicalDevice, &memoryProperties );
	
	float queuePriorities = 1.0f;
	
	VkDeviceQueueCreateInfo queueCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = queueFamilyIndex,
		.queueCount = 1,
		.pQueuePriorities = &queuePriorities
	};
	
	std::vector< const char * > vecEnabledDeviceExtensions;
	vecEnabledDeviceExtensions.push_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
	vecEnabledDeviceExtensions.push_back( VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME );
	vecEnabledDeviceExtensions.push_back( VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME );
	
	VkDeviceCreateInfo deviceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCreateInfo,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = 0,
		.enabledExtensionCount = (uint32_t)vecEnabledDeviceExtensions.size(),
		.ppEnabledExtensionNames = vecEnabledDeviceExtensions.data(),
		.pEnabledFeatures = 0,
	};

	VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device);
	
	if ( result != VK_SUCCESS )
	{
		return false;
	}
	
	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
	
	if ( queue == VK_NULL_HANDLE )
		return false;
	
	return true;
}

bool initvulkan2(void)
{
	VkDescriptorSetLayoutBinding descriptorSetLayoutBindings =
	{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	};
	
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = 1,
		&descriptorSetLayoutBindings
	};
	
	VkDescriptorSetLayout descriptorSetLayout;
	VkResult res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		0,
		0,
		1,
		&descriptorSetLayout,
		0,
		0
	};
	
	VkPipelineLayout pipelineLayout;
	res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, 0, &pipelineLayout);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = composite_spv_len;
	shaderModuleCreateInfo.pCode = (const uint32_t*)composite_spv;
	
	VkShaderModule shaderModule = VK_NULL_HANDLE;
	
	res = vkCreateShaderModule( device, &shaderModuleCreateInfo, nullptr, &shaderModule );
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	VkComputePipelineCreateInfo computePipelineCreateInfo = {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		0,
		0,
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			0,
			0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			shaderModule,
			"main",
			0
		},
		pipelineLayout,
		0,
		0
	};
	
	VkPipeline pipeline;
	res = vkCreateComputePipelines(device, 0, 1, &computePipelineCreateInfo, 0, &pipeline);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	VkCommandPoolCreateInfo commandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		0,
		0,
		queueFamilyIndex
	};
	
	VkDescriptorPoolSize descriptorPoolSize = {
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		1
	};
	
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		nullptr,
		.flags = 0,
		.maxSets = 10,
		.poolSizeCount = 1,
		&descriptorPoolSize
	};
	
	VkDescriptorPool descriptorPool;
	res = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, 0, &descriptorPool);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		descriptorPool,
		1,
		&descriptorSetLayout
	};
	
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	res = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
	
	if ( res != VK_SUCCESS || descriptorSet == VK_NULL_HANDLE )
	{
		return false;
	}
	
	VkDescriptorImageInfo imageInfo = {
		.imageView = outputImage[0].m_vkImageView,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};

	VkWriteDescriptorSet writeDescriptorSet = {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		descriptorSet,
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		&imageInfo,
		nullptr,
		nullptr
	};
	
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	
	VkCommandPool commandPool;
	res = vkCreateCommandPool(device, &commandPoolCreateInfo, 0, &commandPool);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		0,
		commandPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1
	};
	
	VkCommandBuffer commandBuffer;
	res = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	VkCommandBufferBeginInfo commandBufferBeginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		0
	};
	
	res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
							pipelineLayout, 0, 1, &descriptorSet, 0, 0);
	
	vkCmdDispatch(commandBuffer, g_nOutputWidth, g_nOutputHeight, 1);
	
	res = vkEndCommandBuffer(commandBuffer);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		0,
		0,
		0,
		0,
		1,
		&commandBuffer,
		0,
		0
	};
	
	res = vkQueueSubmit(queue, 1, &submitInfo, 0);
	
	if ( res != VK_SUCCESS )
	{
		return false;
	}
	
	vkQueueWaitIdle( queue );
	
	drm_atomic_commit( &g_DRM, outputImage[0].m_FBID, g_nOutputWidth, g_nOutputHeight, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_ATOMIC_ALLOW_MODESET );
	
	return true;
}

void fini_device()
{
	vkDestroyDevice(device, 0);
}

int vulkan_init(void)
{
	VkResult result = VK_ERROR_INITIALIZATION_FAILED;
	
	std::vector< const char * > vecEnabledInstanceExtensions;
	vecEnabledInstanceExtensions.push_back( VK_KHR_SURFACE_EXTENSION_NAME );
	vecEnabledInstanceExtensions.push_back( VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME );
	
	const VkInstanceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = (uint32_t)vecEnabledInstanceExtensions.size(),
		.ppEnabledExtensionNames = vecEnabledInstanceExtensions.data(),
	};

	result = vkCreateInstance(&createInfo, 0, &instance);
	
	if ( result != VK_SUCCESS )
		return 0;
	
	if ( init_device() != 1 )
	{
		return 0;
	}
	
	dyn_vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr( device, "vkGetMemoryFdKHR" );
	if ( dyn_vkGetMemoryFdKHR == nullptr )
		return 0;
	
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	if ( BIsNested() == false )
	{
		imageFormat = DRMFormatToVulkan( g_nDRMFormat );
		
		if ( imageFormat == VK_FORMAT_UNDEFINED )
		{
			return 0;
		}
	}
	
	bool bSuccess = outputImage[0].BInit( g_nOutputWidth, g_nOutputHeight, imageFormat, true, false );
	
	if ( bSuccess != true )
		return 0;
	
	bSuccess = outputImage[1].BInit( g_nOutputWidth, g_nOutputHeight, imageFormat, true, false );
	
	if ( bSuccess != true )
		return 0;
	
	if ( initvulkan2() != true )
	{
		return 0;
	}
	
	return 1;
}

VulkanTexture_t vulkan_create_texture_from_dmabuf( struct wlr_dmabuf_attributes *pDMA )
{
	VulkanTexture_t ret = 0;

	CVulkanTexture *pTex = new CVulkanTexture();
	
	if ( pTex->BInit( pDMA->width, pDMA->height, DRMFormatToVulkan( pDMA->format ), false, true ) == false )
	{
		delete pTex;
		return ret;
	}
	
	ret = ++g_nMaxVulkanTexHandle;
	g_mapVulkanTextures[ ret ] = pTex;
	
	return ret;
}

void vulkan_free_texture( VulkanTexture_t vulkanTex )
{
	if ( vulkanTex == 0 )
		return;

	assert( g_mapVulkanTextures[ vulkanTex ] != nullptr );
	
	// actually free something at some point
}
