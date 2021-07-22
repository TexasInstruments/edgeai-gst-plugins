# Define source code
gst_plugin_sources = [
  'gsttiovx.c',
  'gsttransform.c',
  'gsttiovxcolorconvert.c',
]

gst_plugin_headers = [
  'gsttransform.h',
  'gsttiovxcolorconvert.h',
]

# Build plugin library
gst_plugin = library('gsttiovx',
  gst_plugin_sources,
  c_args : c_args,
  include_directories : [configinc],
  install : true,
  install_dir : plugins_install_dir,
  dependencies : [plugin_deps, gst_internal_dep] ,
)
