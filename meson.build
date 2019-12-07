project('steamcompmgr', ['c','cpp'])

add_project_arguments([
	'-DWLR_USE_UNSTABLE',
], language: ['c','cpp'])

dep_x11 = dependency('x11')
dep_xdamage = dependency('xdamage')
dep_xcomposite = dependency('xcomposite')
dep_xrender = dependency('xrender')
dep_xext = dependency('xext')
dep_gl = dependency('GL')
dep_xxf86vm = dependency('xxf86vm')

pixman_dep = dependency('pixman-1')
drm_dep = dependency('libdrm')
wlroots_dep = dependency('wlroots')
vulkan_dep = dependency('vulkan')

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments([
	'-Wundef',
	'-Wlogical-op',
	'-Wmissing-include-dirs',
	'-Wold-style-definition',
	'-Wpointer-arith',
	'-Winit-self',
	'-Wstrict-prototypes',
	'-Wimplicit-fallthrough=2',
	'-Wendif-labels',
	'-Wstrict-aliasing=2',
	'-Woverflow',

	'-Wno-missing-braces',
	'-Wno-missing-field-initializers',
	'-Wno-unused-parameter',
]), language: 'c')


wayland_server = dependency('wayland-server')
wayland_protos = dependency('wayland-protocols', version: '>=1.17')
pixman         = dependency('pixman-1')
libinput       = dependency('libinput')
xkbcommon      = dependency('xkbcommon')
math           = cc.find_library('m')
thread_dep = dependency('threads')
waffle_dep = dependency('waffle-1')

wlroots_proj = subproject('wlroots')
wlroots_static_dep = wlroots_proj.get_variable('wlroots_static_dep')

executable(
    'steamcompmgr',
    'src/steamcompmgr.c',
    'src/main.cpp',
	'src/main.c',
	'src/wlserver.c',
	'src/drm.cpp',
	'src/rendervulkan.cpp',
    dependencies : [
        dep_x11, dep_xdamage, dep_xcomposite, dep_xrender, dep_xext, dep_gl,
        dep_xxf86vm, pixman_dep, drm_dep, wlroots_dep, wayland_server,
        wayland_protos, libinput, xkbcommon, math, thread_dep, waffle_dep,
        wlroots_static_dep, vulkan_dep
    ],
)
