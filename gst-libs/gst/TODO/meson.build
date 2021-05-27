# Define source code
gst_lib_sources = [
  'gstTODO.c',
]

gst_lib_headers = [
  'gstTODO.h',
]

# Build library
gst_lib = library('gstTODO-@0@'.format(api_version),
  gst_lib_sources,
  c_args : c_args,
  version : api_version,
  include_directories : configinc,
  install : true,
  dependencies : [plugin_deps],
)

plugin_dir = meson.current_build_dir()

# Define the library as an internal dependency to the current build
gst_internal_dep = declare_dependency(link_with: gst_lib,
  dependencies : [plugin_deps])

pkgconfig.generate(gst_lib, install_dir : plugins_pkgconfig_install_dir)
