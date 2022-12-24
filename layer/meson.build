vkroots_dep = dependency('vkroots')
dep_xcb = dependency('xcb')
dep_x11_xcb = dependency('x11-xcb')
wayland_client = dependency('wayland-client')

gamescope_wsi_layer = shared_library('VkLayer_FROG_gamescope_wsi', 'VkLayer_FROG_gamescope_wsi.cpp', protocols_client_src,
  dependencies     : [ vkroots_dep, dep_xcb, dep_x11, dep_x11_xcb, wayland_client ],
  override_options : [ 'cpp_std=c++20' ],
  install          : true )

install_data(
  files('VkLayer_FROG_gamescope_wsi.json'),
  install_dir : join_paths(get_option('datadir'), 'vulkan', 'implicit_layer.d'),
)
