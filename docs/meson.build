c_sources = [
  '../gst-libs/gst/tiovx/gsttiovx.c',
  '../ext/tiovx/gsttransform.c'
]

# Hotdoc documentation
if not get_option('doc').disabled()
  message('Hotdoc enabled: Building hotdoc documentation.')
  hotdoc = import('hotdoc')
  hotdoc.generate_doc(project_name,
    project_version: project_version,
    sitemap: 'sitemap.txt',
    index: 'index.md',
    c_sources: c_sources,
    c_smart_index: true,
    languages: ['c'],
    install: true,
  )
endif
