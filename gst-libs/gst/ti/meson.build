# Define source code
gst_ti_lib_sources = [
  'gsttidloutmeta.c',
]

gst_ti_lib_headers = [
  'gsttidloutmeta.h',
]

# Build library
gst_ti_lib = library('gstti-@0@'.format(api_version),
  gst_ti_lib_sources,
  c_args : c_args,
  version : api_version,
  include_directories : [configinc],
  install : true,
  dependencies : [plugin_deps],
)

plugin_dir = meson.current_build_dir()

# Define the library as an internal dependency to the current build
gst_internal_dep += declare_dependency(link_with: gst_ti_lib,
  dependencies : [plugin_deps])

pkgconfig.generate(gst_ti_lib, install_dir : plugins_pkgconfig_install_dir)
