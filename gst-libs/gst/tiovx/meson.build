# Define source code
gst_lib_sources = [
  'gsttiovxallocator.c',
  'gsttiovxbufferpool.c',
  'gsttiovxcontext.c',
  'gsttiovxmeta.c',
  'gsttiovxmiso.c',
  'gsttiovxpad.c',
  'gsttiovxsimo.c',
  'gsttiovxsiso.c',
  'gsttiovxtensorbufferpool.c',
  'gsttiovxtensormeta.c',
  'gsttiovxutils.c',
  'gsttiovxutils.c',
]

gst_lib_headers = [
  'gsttiovx.h',
  'gsttiovxallocator.h',
  'gsttiovxbufferpool.h',
  'gsttiovxcontext.h',
  'gsttiovxmeta.h',
  'gsttiovxmiso.h',
  'gsttiovxpad.h',
  'gsttiovxsimo.h',
  'gsttiovxsiso.h',
  'gsttiovxtensorbufferpool.h',
  'gsttiovxtensormeta.h',
  'gsttiovxutils.h',
  'gsttiovxutils.h',
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
