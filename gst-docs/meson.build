hotdoc_p = find_program('hotdoc', required: get_option('doc'))
if not hotdoc_p.found()
    message('Hotdoc not found, not building the documentation')
    subdir_done()
endif
hotdoc = import('hotdoc')

# Hotdoc documentation
if not get_option('doc').disabled()
  message('Hotdoc enabled: Building hotdoc documentation.')
  hotdoc = import('hotdoc')
  hotdoc.generate_doc(project_name,
    project_version: project_version,
    sitemap: 'sitemap.txt',
    index: 'index.md',
    c_args : c_args,
    c_sources: ['../gst-libs/gst/tiovx/gsttiovx*.h'],
    c_smart_index: true,
    languages: ['c'],
    install: true,
    dependencies: [plugin_deps, gst_internal_dep],
  )
endif
