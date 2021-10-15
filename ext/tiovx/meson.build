# Define source code
gst_plugin_sources = [
  'gsttiovx.c',
  'gsttiovxcolorconvert.c',
  'gsttiovxdlcolorblend.c',
  'gsttiovxdlpreproc.c',
  'gsttiovxisp.c',
  'gsttiovxldc.c',
  'gsttiovxmosaic.c',
  'gsttiovxmultiscaler.c',
  'gsttiovxmultiscalerpad.c',
]

gst_plugin_headers = [
  'gsttiovxcolorconvert.h',
  'gsttiovxdlcolorblend.h',
  'gsttiovxdlpreproc.h',
  'gsttiovxisp.h',
  'gsttiovxldc.h',
  'gsttiovxmosaic.h',
  'gsttiovxmultiscaler.h',
  'gsttiovxmultiscalerpad.h',
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
