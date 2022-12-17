#define VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#include "vkroots.h"
#include <X11/Xlib-xcb.h>
#include "gamescope-xwayland-client-protocol.h"

#include <cstdio>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <optional>

namespace GamescopeWSILayer {

  static bool contains(const std::vector<const char *> vec, const char *lookupValue) {
    return std::find_if(vec.begin(), vec.end(),
      [=](const char* value) { return !std::strcmp(value, lookupValue); }) != vec.end();
  }

  struct GamescopeInstanceData {
    wl_display* display;
    wl_compositor* compositor;
    gamescope_xwayland* gamescope;
  };
  VKROOTS_DEFINE_SYNCHRONIZED_MAP_TYPE(GamescopeInstance, VkInstance);

  struct GamescopeSurfaceData {
    VkInstance instance;
    wl_surface* surface;

    xcb_connection_t* connection;
    xcb_window_t window;
  };
  VKROOTS_DEFINE_SYNCHRONIZED_MAP_TYPE(GamescopeSurface, VkSurfaceKHR);

  struct GamescopeSwapchainData {
    VkSurfaceKHR surface;
  };
  VKROOTS_DEFINE_SYNCHRONIZED_MAP_TYPE(GamescopeSwapchain, VkSwapchainKHR);

  class VkInstanceOverrides {
  public:
    static VkResult CreateInstance(
            PFN_vkCreateInstance   pfnCreateInstanceProc,
      const VkInstanceCreateInfo*  pCreateInfo,
      const VkAllocationCallbacks* pAllocator,
            VkInstance*            pInstance) {
      // If we are an app running under gamescope and we aren't gamescope itself,
      // then setup our state for xwayland bypass.
      if (!isRunningUnderGamescope() || isAppInfoGamescope(pCreateInfo->pApplicationInfo))
        return pfnCreateInstanceProc(pCreateInfo, pAllocator, pInstance);

      auto enabledExts = std::vector<const char*>(
        pCreateInfo->ppEnabledExtensionNames,
        pCreateInfo->ppEnabledExtensionNames + pCreateInfo->enabledExtensionCount);

      if (!contains(enabledExts, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME))
        enabledExts.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);

      VkInstanceCreateInfo createInfo = *pCreateInfo;
      createInfo.enabledExtensionCount   = uint32_t(enabledExts.size());
      createInfo.ppEnabledExtensionNames = enabledExts.data();

      VkResult result = pfnCreateInstanceProc(&createInfo, pAllocator, pInstance);
      if (result != VK_SUCCESS)
        return result;

      wl_display *display = wl_display_connect(gamescopeWaylandSocket());
      if (!display) {
        fprintf(stderr, "[Gamescope WSI] Failed to connect to gamescope socket: %s\n", gamescopeWaylandSocket());
        return VK_ERROR_INCOMPATIBLE_DRIVER;
      }
      wl_registry *registry = wl_display_get_registry(display);

      {
        auto state = GamescopeInstance::create(*pInstance, GamescopeInstanceData {
          .display = display,
        });
        wl_registry_add_listener(registry, &s_registryListener, reinterpret_cast<void *>(state.get()));
      }
      wl_display_dispatch(display);
      wl_display_roundtrip(display);
      wl_registry_destroy(registry);

      return result;
    }

    static void DestroyInstance(
      const vkroots::VkInstanceDispatch* pDispatch,
            VkInstance                   instance,
      const VkAllocationCallbacks*       pAllocator) {
      if (auto state = GamescopeInstance::get(instance)) {
        wl_display_disconnect(state->display);
      }
      GamescopeInstance::remove(instance);
      pDispatch->DestroyInstance(instance, pAllocator);
    }

    static VkBool32 GetPhysicalDeviceXcbPresentationSupportKHR(
      const vkroots::VkInstanceDispatch* pDispatch,
            VkPhysicalDevice             physicalDevice,
            uint32_t                     queueFamilyIndex,
            xcb_connection_t*            connection,
            xcb_visualid_t               visual_id) {
      auto gamescopeInstance = GamescopeInstance::get(pDispatch->Instance);
      if (!gamescopeInstance)
        return pDispatch->GetPhysicalDeviceXcbPresentationSupportKHR(physicalDevice, queueFamilyIndex, connection, visual_id);

      return GetPhysicalDeviceGamescopePresentationSupport(pDispatch, gamescopeInstance, physicalDevice, queueFamilyIndex);
    }

    static VkBool32 GetPhysicalDeviceXlibPresentationSupportKHR(
      const vkroots::VkInstanceDispatch* pDispatch,
            VkPhysicalDevice             physicalDevice,
            uint32_t                     queueFamilyIndex,
            Display*                     dpy,
            VisualID                     visualID) {
      auto gamescopeInstance = GamescopeInstance::get(pDispatch->Instance);
      if (!gamescopeInstance)
        return pDispatch->GetPhysicalDeviceXlibPresentationSupportKHR(physicalDevice, queueFamilyIndex, dpy, visualID);

      return GetPhysicalDeviceGamescopePresentationSupport(pDispatch, gamescopeInstance, physicalDevice, queueFamilyIndex);
    }

    static VkResult CreateXcbSurfaceKHR(
      const vkroots::VkInstanceDispatch* pDispatch,
            VkInstance                   instance,
      const VkXcbSurfaceCreateInfoKHR*   pCreateInfo,
      const VkAllocationCallbacks*       pAllocator,
            VkSurfaceKHR*                pSurface) {
      auto gamescopeInstance = GamescopeInstance::get(instance);
      if (!gamescopeInstance)
        return pDispatch->CreateXcbSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);

      return CreateGamescopeSurface(pDispatch, gamescopeInstance, instance, pCreateInfo->connection, pCreateInfo->window, pAllocator, pSurface);
    }

    static VkResult CreateXlibSurfaceKHR(
      const vkroots::VkInstanceDispatch* pDispatch,
            VkInstance                   instance,
      const VkXlibSurfaceCreateInfoKHR*  pCreateInfo,
      const VkAllocationCallbacks*       pAllocator,
            VkSurfaceKHR*                pSurface) {
      auto gamescopeInstance = GamescopeInstance::get(instance);
      if (!gamescopeInstance)
        return pDispatch->CreateXlibSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);

      return CreateGamescopeSurface(pDispatch, gamescopeInstance, instance, XGetXCBConnection(pCreateInfo->dpy), xcb_window_t(pCreateInfo->window), pAllocator, pSurface);
    }

    static constexpr std::array<VkSurfaceFormat2KHR, 2> s_ExtraSurfaceFormat2s = {{
      { .surfaceFormat = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT, } },
      { .surfaceFormat = { VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT, } },
    }};

    static constexpr auto s_ExtraSurfaceFormats = []() {
      std::array<VkSurfaceFormatKHR, s_ExtraSurfaceFormat2s.size()> array;
      for (size_t i = 0; i < s_ExtraSurfaceFormat2s.size(); i++)
        array[i] = s_ExtraSurfaceFormat2s[i].surfaceFormat;
      return array;
    }();

    static VkResult GetPhysicalDeviceSurfaceFormatsKHR(
      const vkroots::VkInstanceDispatch* pDispatch,
            VkPhysicalDevice             physicalDevice,
            VkSurfaceKHR                 surface,
            uint32_t*                    pSurfaceFormatCount,
            VkSurfaceFormatKHR*          pSurfaceFormats) {
      return vkroots::helpers::append(
        pDispatch->GetPhysicalDeviceSurfaceFormatsKHR,
        s_ExtraSurfaceFormats,
        pSurfaceFormatCount,
        pSurfaceFormats,
        physicalDevice,
        surface);
    }

    static VkResult GetPhysicalDeviceSurfaceFormats2KHR(
      const vkroots::VkInstanceDispatch*     pDispatch,
            VkPhysicalDevice                 physicalDevice,
      const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo,
            uint32_t*                        pSurfaceFormatCount,
            VkSurfaceFormat2KHR*             pSurfaceFormats) {
      return vkroots::helpers::append(
        pDispatch->GetPhysicalDeviceSurfaceFormats2KHR,
        s_ExtraSurfaceFormat2s,
        pSurfaceFormatCount,
        pSurfaceFormats,
        physicalDevice,
        pSurfaceInfo);
    }

    static VkResult GetPhysicalDeviceSurfaceCapabilitiesKHR(
      const vkroots::VkInstanceDispatch*     pDispatch,
            VkPhysicalDevice                 physicalDevice,
            VkSurfaceKHR                     surface,
            VkSurfaceCapabilitiesKHR*        pSurfaceCapabilities) {
      auto gamescopeSurface = GamescopeSurface::get(surface);
      if (!gamescopeSurface)
        return pDispatch->GetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, pSurfaceCapabilities);

      VkResult res = VK_SUCCESS;
      if ((res = pDispatch->GetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, pSurfaceCapabilities)) != VK_SUCCESS)
        return res;

      VkExtent2D currentExtent = {};
      if ((res = getCurrentExtent(gamescopeSurface->connection, gamescopeSurface->window, &currentExtent)) != VK_SUCCESS)
        return res;

      pSurfaceCapabilities->currentExtent = currentExtent;
      pSurfaceCapabilities->minImageCount = getMinImageCount();

      return VK_SUCCESS;
    }

    static void DestroySurfaceKHR(
      const vkroots::VkInstanceDispatch* pDispatch,
            VkInstance                   instance,
            VkSurfaceKHR                 surface,
      const VkAllocationCallbacks*       pAllocator) {
      if (auto state = GamescopeSurface::get(surface)) {
        wl_surface_destroy(state->surface);
      }
      GamescopeSurface::remove(surface);
      pDispatch->DestroySurfaceKHR(instance, surface, pAllocator);
    }

    static VkResult EnumerateDeviceExtensionProperties(
      const vkroots::VkInstanceDispatch* pDispatch,
            VkPhysicalDevice             physicalDevice,
            const char*                  pLayerName,
            uint32_t*                    pPropertyCount,
            VkExtensionProperties*       pProperties) {
      static constexpr std::array<VkExtensionProperties, 1> s_LayerExposedExts = {{
        { VK_EXT_HDR_METADATA_EXTENSION_NAME,
          VK_EXT_HDR_METADATA_SPEC_VERSION },
      }};

      if (pLayerName) {
        if (!std::strncmp(pLayerName, "VK_LAYER_FROG_gamescope_wsi", VK_MAX_EXTENSION_NAME_SIZE)) {
          return vkroots::helpers::array(s_LayerExposedExts, pPropertyCount, pProperties);
        } else {
          return pDispatch->EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
        }
      }

      return vkroots::helpers::append(
        pDispatch->EnumerateDeviceExtensionProperties,
        s_LayerExposedExts,
        pPropertyCount,
        pProperties,
        physicalDevice,
        pLayerName);
    }

  private:
    static VkResult CreateGamescopeSurface(
      const vkroots::VkInstanceDispatch* pDispatch,
            GamescopeInstance&           gamescopeInstance,
            VkInstance                   instance,
            xcb_connection_t*            xcb_conn,
            xcb_window_t                 window,
      const VkAllocationCallbacks*       pAllocator,
            VkSurfaceKHR*                pSurface) {
      fprintf(stderr, "[Gamescope WSI] Creating Gamescope surface: xid: 0x%x\n", window);

      wl_surface* waylandSurface = wl_compositor_create_surface(gamescopeInstance->compositor);
      if (!waylandSurface) {
        fprintf(stderr, "[Gamescope WSI] Failed to create wayland surface - xid: 0x%x\n", window);
        return VK_ERROR_SURFACE_LOST_KHR;
      }

      gamescope_xwayland_override_window_content(gamescopeInstance->gamescope, waylandSurface, window);

      wl_display_flush(gamescopeInstance->display);

      VkWaylandSurfaceCreateInfoKHR waylandCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .display = gamescopeInstance->display,
        .surface = waylandSurface,
      };

      VkResult result = pDispatch->CreateWaylandSurfaceKHR(instance, &waylandCreateInfo, pAllocator, pSurface);
      if (result != VK_SUCCESS) {
        fprintf(stderr, "[Gamescope WSI] Failed to create Vulkan wayland surface - vr: %s xid: 0x%x\n", vkroots::helpers::enumString(result), window);
        return result;
      }

      fprintf(stderr, "[Gamescope WSI] Made gamescope surface for xid: 0x%x\n", window);
      GamescopeSurface::create(*pSurface, GamescopeSurfaceData {
        .instance   = instance,
        .surface    = waylandSurface,
        .connection = xcb_conn,
        .window     = window,
      });

      return result;
    }

    static VkBool32 GetPhysicalDeviceGamescopePresentationSupport(
      const vkroots::VkInstanceDispatch* pDispatch,
            GamescopeInstance&           gamescopeInstance,
            VkPhysicalDevice             physicalDevice,
            uint32_t                     queueFamilyIndex) {
      return pDispatch->GetPhysicalDeviceWaylandPresentationSupportKHR(physicalDevice, queueFamilyIndex, gamescopeInstance->display);
    }

    static const char* gamescopeWaylandSocket() {
      return std::getenv("GAMESCOPE_WAYLAND_DISPLAY");
    }

    static bool isAppInfoGamescope(const VkApplicationInfo *appInfo) {
      if (!appInfo || !appInfo->pApplicationName)
        return false;

      return !std::strcmp(appInfo->pApplicationName, "gamescope");
    }

    static bool isRunningUnderGamescope() {
      static bool s_isRunningUnderGamescope = []() -> bool {
        const char *gamescopeSocketName = gamescopeWaylandSocket();
        if (!gamescopeSocketName || !*gamescopeSocketName)
          return false;

        // Gamescope always unsets WAYLAND_SOCKET.
        // So if that is set, we know we cannot be running under Gamescope
        // and must be in a nested Wayland session inside of gamescope.
        const char *waylandSocketName = std::getenv("WAYLAND_DISPLAY");
        if (waylandSocketName && *waylandSocketName)
          return false;

        return true;
      }();

      return s_isRunningUnderGamescope;
    }

    static VkResult getCurrentExtent(xcb_connection_t* xcb_conn, xcb_window_t window, VkExtent2D* pExtent) {
      xcb_generic_error_t *err = nullptr;
      xcb_get_geometry_cookie_t geom_cookie = xcb_get_geometry(xcb_conn, window);
      xcb_get_geometry_reply_t* geom = xcb_get_geometry_reply(xcb_conn, geom_cookie, &err);
      if (!geom) {
        free(err);
        return VK_ERROR_SURFACE_LOST_KHR;
      }

      *pExtent = VkExtent2D{ geom->width, geom->height };
      free(geom);
      free(err);
      return VK_SUCCESS;
    }

    static uint32_t getMinImageCount() {
      const char *overrideStr = std::getenv("GAMESCOPE_WSI_MIN_IMAGE_COUNT");
      if (overrideStr && *overrideStr)
        return uint32_t(std::atoi(overrideStr));

      return 3;
    }

    static constexpr wl_registry_listener s_registryListener = {
      .global = [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
        auto instance = reinterpret_cast<GamescopeInstanceData *>(data);

        if (!std::strcmp(interface, wl_compositor_interface.name)) {
          instance->compositor = reinterpret_cast<wl_compositor *>(
            wl_registry_bind(registry, name, &wl_compositor_interface, version));
        } else if (!std::strcmp(interface, gamescope_xwayland_interface.name)) {
          instance->gamescope = reinterpret_cast<gamescope_xwayland *>(
            wl_registry_bind(registry, name, &gamescope_xwayland_interface, version));
        }
      },
      .global_remove = [](void* data, wl_registry* registry, uint32_t name) {
      },
    };

  };

  class VkDeviceOverrides {
  public:
    static void DestroySwapchainKHR(
      const vkroots::VkDeviceDispatch* pDispatch,
            VkDevice                   device,
            VkSwapchainKHR             swapchain,
      const VkAllocationCallbacks*     pAllocator) {
      GamescopeSwapchain::remove(swapchain);
      pDispatch->DestroySwapchainKHR(device, swapchain, pAllocator);
    }

    static VkResult CreateSwapchainKHR(
      const vkroots::VkDeviceDispatch* pDispatch,
            VkDevice                   device,
      const VkSwapchainCreateInfoKHR*  pCreateInfo,
      const VkAllocationCallbacks*     pAllocator,
            VkSwapchainKHR*            pSwapchain) {
      auto gamescopeSurface = GamescopeSurface::get(pCreateInfo->surface);

      VkSwapchainCreateInfoKHR swapchainInfo = *pCreateInfo;
      if (gamescopeSurface) {
        // If this is a gamescope surface
        // Force the colorspace to sRGB before sending to the driver.
        swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        fprintf(stderr, "[Gamescope WSI] Creating swapchain for wl_surface for xid: 0x%0x - colorspace: %s\n",
          gamescopeSurface->window, vkroots::helpers::enumString(pCreateInfo->imageColorSpace));
      }

      VkResult result = pDispatch->CreateSwapchainKHR(device, &swapchainInfo, pAllocator, pSwapchain);
      if (gamescopeSurface) {
        if (result == VK_SUCCESS) {
          GamescopeSwapchain::create(*pSwapchain, GamescopeSwapchainData{
            .surface = pCreateInfo->surface,
          });

          auto gamescopeInstance = GamescopeInstance::get(gamescopeSurface->instance);
          if (gamescopeInstance) {
            uint32_t imageCount = 0;
            pDispatch->GetSwapchainImagesKHR(device, *pSwapchain, &imageCount, nullptr);

            gamescope_xwayland_swapchain_feedback(
              gamescopeInstance->gamescope,
              gamescopeSurface->surface,
              imageCount,
              uint32_t(pCreateInfo->imageFormat),
              uint32_t(pCreateInfo->imageColorSpace),
              uint32_t(pCreateInfo->compositeAlpha),
              uint32_t(pCreateInfo->preTransform),
              uint32_t(pCreateInfo->presentMode),
              uint32_t(pCreateInfo->clipped));
          }
        } else {
          fprintf(stderr, "[Gamescope WSI] Failed to create swapchain - vr: %s xid: 0x%x\n", vkroots::helpers::enumString(result), gamescopeSurface->window);
        }
      }
      return result;
    }

    static void SetHdrMetadataEXT(
      const vkroots::VkDeviceDispatch* pDispatch,
            VkDevice                   device,
            uint32_t                   swapchainCount,
      const VkSwapchainKHR*            pSwapchains,
      const VkHdrMetadataEXT*          pMetadata) {
      fprintf(stderr, "[Gamescope WSI] Got SetHdrMetadataEXT!\n");
    }
  };

}

VKROOTS_DEFINE_LAYER_INTERFACES(GamescopeWSILayer::VkInstanceOverrides,
                                vkroots::NoOverrides,
                                GamescopeWSILayer::VkDeviceOverrides);

VKROOTS_IMPLEMENT_SYNCHRONIZED_MAP_TYPE(GamescopeWSILayer::GamescopeInstance);
VKROOTS_IMPLEMENT_SYNCHRONIZED_MAP_TYPE(GamescopeWSILayer::GamescopeSurface);
VKROOTS_IMPLEMENT_SYNCHRONIZED_MAP_TYPE(GamescopeWSILayer::GamescopeSwapchain);
