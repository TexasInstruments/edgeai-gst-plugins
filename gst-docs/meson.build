hotdoc_p = find_program('hotdoc', required: get_option('doc'))
if not hotdoc_p.found()
    message('Hotdoc not found, not building the documentation')
    subdir_done()
endif
hotdoc = import('hotdoc')

c_sources = [
      '../gst-libs/gst/tiovx/gsttiovx.h',
      '../gst-libs/gst/tiovx/gsttiovxbufferpool.h',
      '../gst-libs/gst/tiovx/gsttiovxcontext.h',
      '../gst-libs/gst/tiovx/gsttiovxmeta.h',
      '../gst-libs/gst/tiovx/gsttiovxpad.h',
      '../gst-libs/gst/tiovx/gsttiovxsimo.h',
      '../gst-libs/gst/tiovx/gsttiovxsiso.h',
      '../gst-libs/gst/tiovx/gsttiovxutils.h',
      '../ext/tiovx/gsttiovx.h',
      '../ext/tiovx/gsttiovxcolorconvert.h',
      '../ext/tiovx/gsttiovxmultiscaler.h',
      '../ext/tiovx/gsttiovxmultiscalerpad.h',
      '../ext/tiovx/gsttiovxmultiscalerpad.h',
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
    languages: ['c'],
    c_smart_index: true,
    install: true,
    dependencies: [gst_dep]
  )
endif
