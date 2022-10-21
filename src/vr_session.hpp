#pragma once

#include <vector>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#include "../subprojects/openvr/headers/openvr.h"
#pragma GCC diagnostic pop

bool vr_init(int argc, char **argv);

bool vrsession_init();
bool vrsession_visible();
void vrsession_present( vr::VRVulkanTextureData_t *pTextureData );

void vrsession_append_instance_exts( std::vector<const char *>& exts );
void vrsession_append_device_exts( VkPhysicalDevice physDev, std::vector<const char *>& exts );

bool vrsession_framesync( uint32_t timeoutMS );
void vrsession_update_touch_mode();

void vrsession_title( const char *title );
bool vrsession_ime_init();
