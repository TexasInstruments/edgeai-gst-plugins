# Define source code
gst_ti_plugin_sources = [
  'gstti.c',
  'gstticolorconvert.c',
  'gsttiscaler.c',
  'gsttiperfoverlay.cpp',
  'gsttimosaic.c',
]

if get_option('dl-plugins').enabled()
  gst_ti_plugin_sources += [
    'gsttipreproc.cpp',
    'gsttidlinferer.cpp',
    'gsttidlpostproc.cpp',
  ]
endif

gst_ti_plugin_headers = [
  'gstticolorconvert.h',
  'gsttiscaler.h',
  'gsttiperfoverlay.h',
  'gsttimosaic.h',
]

if get_option('dl-plugins').enabled()
  gst_ti_plugin_sources += [
    'gsttipreproc.h',
    'gsttidlinferer.h',
    'gsttidlpostproc.h',
  ]
endif

# Build plugin library
gst_ti_plugin = library('gstti',
  gst_ti_plugin_sources,
  c_args : c_args,
  cpp_args : cpp_args,
  include_directories : [configinc],
  install : true,
  install_dir : plugins_install_dir,
  dependencies : [plugin_deps, gst_internal_dep] ,
)
