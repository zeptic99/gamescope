project('gamescope', ['c','cpp'])

add_project_arguments([
	'-DWLR_USE_UNSTABLE',
], language: ['c','cpp'])

dep_x11 = dependency('x11')
dep_xdamage = dependency('xdamage')
dep_xcomposite = dependency('xcomposite')
dep_xrender = dependency('xrender')
dep_xext = dependency('xext')
dep_xxf86vm = dependency('xxf86vm')
dep_xtst = dependency('xtst')

pixman_dep = dependency('pixman-1')
drm_dep = dependency('libdrm')
vulkan_dep = dependency('vulkan')

cc = meson.get_compiler('c')

wayland_server = dependency('wayland-server')
wayland_protos = dependency('wayland-protocols', version: '>=1.17')
pixman         = dependency('pixman-1')
libinput       = dependency('libinput')
xkbcommon      = dependency('xkbcommon')
math           = cc.find_library('m')
thread_dep = dependency('threads')
sdl_dep = dependency('SDL2')

wlroots_proj = subproject('wlroots')
wlroots_static_dep = wlroots_proj.get_variable('wlroots_static_dep')

shadercompiler = find_program('glslangValidator')
extratool = find_program('xxd')
shaderbuilder = find_program('src/shaderbuild.sh')

spirv_shader = custom_target('shader_target',
  output : 'composite.h',
  input : 'src/composite.comp.hlsl',
  command : [shaderbuilder, '@INPUT@', '@OUTPUT@'],
  install : false,
)

liftoff_proj = subproject('libliftoff')
libftoff_dep = liftoff_proj.get_variable('liftoff')

executable(
    'gamescope',
    'src/steamcompmgr.cpp',
    'src/main.cpp',
	'src/wlserver.c',
	'src/drm.cpp',
	'src/inputsdl.cpp',
	'src/vblankmanager.cpp',
	[ 'src/rendervulkan.cpp', spirv_shader ],
    dependencies : [
        dep_x11, dep_xdamage, dep_xcomposite, dep_xrender, dep_xext,
        dep_xxf86vm, pixman_dep, drm_dep, wayland_server, wayland_protos,
        libinput, xkbcommon, math, thread_dep, sdl_dep, wlroots_static_dep,
        vulkan_dep, libftoff_dep, dep_xtst
    ],
    install: true,
)
