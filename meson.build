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

subdir('protocol')

executable(
    'steamcompmgr',
    'src/steamcompmgr.c',
    'src/main.cpp',
	'src/main.c',
	'src/bindings.c',
	'src/config.c',
	'src/cursor.c',
	'src/desktop.c',
	'src/ini.c',
	'src/input.c',
	'src/keyboard.c',
	'src/layer_shell.c',
	'src/rootston.c',
	'src/output.c',
	'src/render.c',
	'src/seat.c',
	'src/switch.c',
	'src/text_input.c',
	'src/view.c',
	'src/virtual_keyboard.c',
	'src/xdg_shell_v6.c',
	'src/xdg_shell.c',
	'src/xwayland.c',
    dependencies : [
        dep_x11, dep_xdamage, dep_xcomposite, dep_xrender, dep_xext, dep_gl,
        dep_xxf86vm, pixman_dep, drm_dep, wlroots_dep, protos, wayland_server,
        wayland_protos, libinput, xkbcommon, math, thread_dep, waffle_dep
    ],
)
