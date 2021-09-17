
# common utilities for tests
test_utils = declare_dependency(
  dependencies : [gst_internal_dep],
  sources : ['test_utils.c']
)


# name, condition when to skip the test, extra dependencies and extra files
gst_tests = [
  ['libs/gsttiovxallocator', false, [gst_internal_dep, test_utils],  [] ],
  ['libs/gsttiovxbufferpool', false, [gst_internal_dep, test_utils],  [] ],
  ['libs/gsttiovxmiso', false, [gst_internal_dep, test_utils],  [] ],
  ['libs/gsttiovxpad', false, [gst_internal_dep, test_utils],  [] ],
  ['libs/gsttiovxtensorbufferpool', false, [gst_internal_dep, test_utils],  [] ],
  ['gsttiovxmultiscaler', false, [gst_internal_dep, test_utils],  [] ],
  ['gsttiovxcolorconvert', false, [gst_internal_dep, test_utils],  [] ],
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
    test(test_name, exe, env: env, timeout : 60, is_parallel: false)
  endif
endforeach
