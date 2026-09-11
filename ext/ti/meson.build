# Define source code
gst_ti_plugin_sources = [
  'gstti.c',
]

# Sources that need tiovx/edgeai headers — not available on am62d
if SOC != 'am62d'
  gst_ti_plugin_sources += [
    'gstticolorconvert.c',
    'gsttiscaler.c',
    'gsttiperfoverlay.cpp',
    'gsttimosaic.c',
  ]
endif

if get_option('dl-plugins').enabled() and SOC != 'am62d'
  gst_ti_plugin_sources += [
    'gsttidlpreproc.cpp',
    'gsttidlinferer.cpp',
    'gsttidlpostproc.cpp',
  ]
endif

if get_option('tvm-plugins').enabled()
  gst_ti_plugin_sources += [
    'gsttitvm.cpp',
  ]
endif

if get_option('stft-plugins').enabled()
  gst_ti_plugin_sources += [
    '../../utility/gsttirpmsgctx.cpp',
    'gstdspkernel.cpp',
  ]
endif

gst_ti_plugin_headers = []

if SOC != 'am62d'
  gst_ti_plugin_headers += [
    'gstticolorconvert.h',
    'gsttiscaler.h',
    'gsttiperfoverlay.h',
    'gsttimosaic.h',
  ]
endif

if get_option('dl-plugins').enabled() and SOC != 'am62d'
  gst_ti_plugin_headers += [
    'gsttidlpreproc.h',
    'gsttidlinferer.h',
    'gsttidlpostproc.h',
  ]
endif

if get_option('tvm-plugins').enabled()
  gst_ti_plugin_headers += [
    'gsttitvm.h',
  ]
endif

if get_option('stft-plugins').enabled()
  gst_ti_plugin_headers += [
    '../../utility/gsttirpmsgctx.h',
    'gstdspkernel.h',
  ]
endif

# Include utility directory for gsttirpmsgctx headers
utility_inc = include_directories('../../utility')

# Build plugin library
gst_ti_plugin = library('gstti',
  gst_ti_plugin_sources,
  c_args : c_args,
  cpp_args : cpp_args,
  include_directories : [configinc, utility_inc],
  install : true,
  install_dir : plugins_install_dir,
  dependencies : [plugin_deps, gst_internal_dep] ,
)
