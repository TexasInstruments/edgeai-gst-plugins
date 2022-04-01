
# name, condition when to skip the test, extra dependencies and extra files
gst_tests = [
  ['libs/gsttiovxallocator', false, [gst_internal_dep],  [] ],
  ['libs/gsttiovximagebufferpool', false, [gst_internal_dep],  [] ],
  ['libs/gsttiovxmiso', false, [gst_internal_dep],  [] ],
  ['libs/gsttiovxpad', false, [gst_internal_dep],  [] ],
  ['libs/gsttiovxpyramidbufferpool', false, [gst_internal_dep],  [] ],
  ['libs/gsttiovxrawimagebufferpool', false, [gst_internal_dep],  [] ],
  ['libs/gsttiovxsimo', false, [gst_internal_dep],  [] ],
  ['libs/gsttiovxsiso', false, [gst_internal_dep],  [] ],
  ['libs/gsttiovxtensorbufferpool', false, [gst_internal_dep],  [] ],
  ['gsttiovxcolorconvert', false, [gst_internal_dep],  [] ],
  ['gsttiovxdemux', false, [gst_internal_dep],  [] ],
  ['gsttiovxdlcolorconvert', false, [gst_internal_dep],  [] ],
  ['gsttiovxdlcolorblend', false, [gst_internal_dep], [] ],
  ['gsttiovxdlpreproc', false, [gst_internal_dep],  [] ],
  ['gsttiovxisp', true, [gst_internal_dep],  [] ],
  ['gsttiovxldc', false, [gst_internal_dep],  [] ],
  ['gsttiovxmosaic', false, [gst_internal_dep],  [] ],
  ['gsttiovxmultiscaler', false, [gst_internal_dep],  [] ],
  ['gsttiovxmux', false, [gst_internal_dep],  [] ],
  ['gsttiovxpyramid', false, [gst_internal_dep],  [] ],
]

# Add C Definitions for tests
test_defines = [
  '-Dgsttiovxdefine="gsttiovxdefinename"',
]

# Define plugins path
plugins_dir = gst_dep.get_pkgconfig_variable('pluginsdir')

# Create environment object to  stores information about the environment
# variables set during tests.
# Define constant enviroment variable
env = environment()
env.set('GST_PLUGIN_SYSTEM_PATH_1_0', '')
env.set('CK_DEFAULT_TIMEOUT', '120')
env.set('GST_PLUGIN_PATH_1_0', plugin_dir + ':' + plugins_dir)

# Build and run tests
foreach t : gst_tests
  fname = '@0@.c'.format(t.get(0))
  test_name = t.get(0).underscorify()
  extra_sources = t.get(3, [ ])
  extra_deps = t.get(2, [ ])
  skip_test = t.get(1, false)
  if not skip_test
    exe = executable(test_name, fname, extra_sources,
      include_directories : [configinc],
      c_args : [c_args, test_defines],
      dependencies : [test_deps + extra_deps, gst_internal_dep],
    )

    # Define enviroment variable
    env.set('GST_REGISTRY', '@0@/@1@.registry'.format(meson.current_build_dir(), test_name))

    # Run tests
    test(test_name, exe, env: env, timeout : 10, is_parallel: false)
  endif
endforeach
