# Define source code
gst_lib_sources = [
  'gsttiovxallocator.c',
  'gsttiovxbufferpool.c',
  'gsttiovxbufferpoolutils.c',
  'gsttiovxbufferutils.c',
  'gsttiovxcontext.c',
  'gsttiovximagebufferpool.c',
  'gsttiovximagemeta.c',
  'gsttiovxmiso.c',
  'gsttiovxmuxmeta.c',
  'gsttiovxpad.c',
  'gsttiovxpyramidbufferpool.c',
  'gsttiovxpyramidmeta.c',
  'gsttiovxqueueableobject.c',
  'gsttiovxrawimagebufferpool.c',
  'gsttiovxrawimagemeta.c',
  'gsttiovxsensormeta.c',
  'gsttiovxsimo.c',
  'gsttiovxsiso.c',
  'gsttiovxtensorbufferpool.c',
  'gsttiovxtensormeta.c',
  'gsttiovxutils.c',
  'gsttidloutmeta.c',
]

gst_lib_headers = [
  'gsttiovx.h',
  'gsttiovxallocator.h',
  'gsttiovxbufferpool.h',
  'gsttiovxbufferpoolutils.h',
  'gsttiovxbufferutils.h',
  'gsttiovxcontext.h',
  'gsttiovximagebufferpool.h',
  'gsttiovximagemeta.h',
  'gsttiovxmiso.h',
  'gsttiovxmuxmeta.h',
  'gsttiovxpad.h',
  'gsttiovxpyramidbufferpool.h',
  'gsttiovxpyramidmeta.h',
  'gsttiovxqueueableobject.h',
  'gsttiovxrawimagebufferpool.h',
  'gsttiovxrawimagemeta.h',
  'gsttiovxsimo.h',
  'gsttiovxsiso.h',
  'gsttiovxsensormeta.h',
  'gsttiovxtensorbufferpool.h',
  'gsttiovxtensormeta.h',
  'gsttiovxutils.h',
  'gsttidloutmeta.h',
]

# Build library
gst_lib = library('gsttiovx-@0@'.format(api_version),
  gst_lib_sources,
  c_args : c_args,
  version : api_version,
  include_directories : [configinc],
  install : true,
  dependencies : [plugin_deps, tiovx_deps, ti_vision_apps_dep],
)

plugin_dir = meson.current_build_dir()

# Define the library as an internal dependency to the current build
gst_internal_dep = declare_dependency(link_with: gst_lib,
  dependencies : [plugin_deps, tiovx_deps, ti_vision_apps_dep])

pkgconfig.generate(gst_lib, install_dir : plugins_pkgconfig_install_dir)
