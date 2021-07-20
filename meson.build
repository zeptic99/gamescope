project(
  'gamescope',
  'cpp',
  meson_version: '>=0.54.0',
  default_options: ['warning_level=2', 'force_fallback_for=wlroots,libliftoff'],
)

add_project_arguments([
  '-DWLR_USE_UNSTABLE',
], language: 'cpp')

cppc = meson.get_compiler('cpp')

add_project_arguments(cppc.get_supported_arguments([
  '-Wno-unused-parameter',
  '-Wno-missing-field-initializers',
]), language: 'cpp')

dep_x11 = dependency('x11')
dep_xdamage = dependency('xdamage')
dep_xcomposite = dependency('xcomposite')
dep_xrender = dependency('xrender')
dep_xext = dependency('xext')
dep_xfixes = dependency('xfixes')
dep_xxf86vm = dependency('xxf86vm')
dep_xtst = dependency('xtst')
dep_xres = dependency('xres')

drm_dep = dependency('libdrm')
vulkan_dep = dependency('vulkan')

wayland_server = dependency('wayland-server')
wayland_protos = dependency('wayland-protocols', version: '>=1.17')
xkbcommon = dependency('xkbcommon')
thread_dep = dependency('threads')
cap_dep = dependency('libcap')
sdl_dep = dependency('SDL2')

wlroots_dep = dependency(
  'wlroots',
  version: ['>= 0.13.0', '< 0.14.0'],
  fallback: ['wlroots', 'wlroots'],
  default_options: ['default_library=static', 'examples=false', 'xwayland=enabled'],
)

wlroots_has_xwayland = wlroots_dep.get_variable(pkgconfig: 'have_xwayland', internal: 'have_xwayland') == 'true'
if not wlroots_has_xwayland
  error('Cannot use wlroots built without Xwayland support')
endif

shadercompiler = find_program('glslangValidator')

spirv_shader = custom_target('shader_target',
  output : 'composite.h',
  input : 'src/composite.comp',
  command : [
    shadercompiler, '-V', '-e', 'main', '--vn',
    'composite_spv', '@INPUT@', '-o', '@OUTPUT@'
  ],
  install : false,
)

liftoff_dep = dependency(
  'libliftoff',
  version: ['>= 0.1.0', '< 0.2.0'],
  fallback: ['libliftoff', 'liftoff'],
  default_options: ['default_library=static'],
)

subdir('protocol')

executable(
  'gamescope',
  [
    'src/steamcompmgr.cpp',
    'src/main.cpp',
    'src/wlserver.cpp',
    'src/drm.cpp',
    'src/cvt.cpp',
    'src/sdlwindow.cpp',
    'src/vblankmanager.cpp',
    'src/rendervulkan.cpp',
    spirv_shader,
    gamescope_xwayland_proto_files,
  ],
  dependencies: [
    dep_x11, dep_xdamage, dep_xcomposite, dep_xrender, dep_xext, dep_xfixes,
    dep_xxf86vm, dep_xres, drm_dep, wayland_server, wayland_protos,
    xkbcommon, thread_dep, sdl_dep, wlroots_dep,
    vulkan_dep, liftoff_dep, dep_xtst, cap_dep
  ],
  install: true,
)
