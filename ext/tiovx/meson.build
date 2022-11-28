# Define source code
gst_plugin_sources = [
  'gsttiovx.c',
  'gsttiovxdelay.c',
  'gsttiovxdemux.c',
  'gsttiovxisp.c',
  'gsttiovxldc.c',
  'gsttiovxmemalloc.c',
  'gsttiovxmosaic.c',
  'gsttiovxmultiscaler.c',
  'gsttiovxmultiscalerpad.c',
  'gsttiovxmux.c',
  'gsttiovxpyramid.c',
  'gsttiovxdlcolorconvert.c',
  'gsttiovxdlcolorblend.c',
  'gsttiovxdlpreproc.c'
]

if SOC == 'j721e' or SOC == 'j721s2' or SOC == 'j784s4'
  gst_plugin_sources += [
    'gsttiovxcolorconvert.c',
    'gsttiovxdof.c',
    'gsttiovxdofviz.c',
    'gsttiovxsde.c',
    'gsttiovxsdeviz.c',
  ]
endif

if get_option('dl-plugins').enabled()
  gst_plugin_sources += [
    'gsttidlinferer.cpp',
    'gsttidlpostproc.cpp',
    'gsttiperfoverlay.cpp',
  ]
endif

gst_plugin_headers = [
  'gsttiovxdelay.h',
  'gsttiovxdemux.h',
  'gsttiovxisp.h',
  'gsttiovxldc.h',
  'gsttiovxmemalloc.h',
  'gsttiovxmosaic.h',
  'gsttiovxmultiscaler.h',
  'gsttiovxmultiscalerpad.h',
  'gsttiovxmux.h',
  'gsttiovxpyramid.h',
  'gsttiovxdlcolorconvert.h',
  'gsttiovxdlcolorblend.h',
  'gsttiovxdlpreproc.h'
]

if SOC == 'j721e' or SOC == 'j721s2' or SOC == 'j784s4'
  gst_plugin_headers += [
    'gsttiovxcolorconvert.h',
    'gsttiovxdof.h',
    'gsttiovxdofviz.h',
    'gsttiovxsde.h',
    'gsttiovxsdeviz.h'
  ]
endif

if get_option('dl-plugins').enabled()
  gst_plugin_sources += [
    'gsttidlinferer.h',
    'gsttidlpostproc.h',
    'gsttiperfoverlay.h',
  ]
endif

# Build plugin library
gst_plugin = library('gsttiovx',
  gst_plugin_sources,
  c_args : c_args,
  cpp_args : cpp_args,
  include_directories : [configinc],
  install : true,
  install_dir : plugins_install_dir,
  dependencies : [plugin_deps, gst_internal_dep] ,
)
