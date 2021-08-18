# Define source code
gst_plugin_sources = [
  'gsttiovx.c',
  'gsttiovxcolorconvert.c',
  'gsttiovxmultiscaler.c',
  'gsttiovxmultiscalerpad.c',
  'gsttiovxdlpreproc.c',
  'gsttiovxdlcolorblend.c',
]

gst_plugin_headers = [
  'gsttiovxcolorconvert.h',
  'gsttiovxmultiscaler.h',
  'gsttiovxmultiscalerpad.h',
  'gsttiovxdlpreproc.h',
  'gsttiovxdlcolorblend.h'
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
