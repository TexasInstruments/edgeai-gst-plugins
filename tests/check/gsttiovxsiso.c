#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/video/video.h>

#include <gst-libs/gst/tiovx/gsttiovxsiso.h>

#define MOCK_TYPE

G_BEGIN_DECLS
#define GST_TIOVX_SISO_MOCK_TYPE (gst_tiovx_siso_mock_get_type ())
G_DECLARE_FINAL_TYPE (GstTIOVXSisoMock, gst_tiovx_siso_mock, GST,
    TIOVX_SISO_MOCK, GstTIOVXSiso)

G_END_DECLS
GST_DEBUG_CATEGORY_STATIC (gst_tiovx_siso_mock_debug_category);

     struct _GstTIOVXSisoMock
     {
       GstElement parent;

       GstPad *sinkpad;
       GstPad *srcpad;

       GstBufferPool *bufferPool;
     };

G_DEFINE_TYPE_WITH_CODE (GstTIOVXSisoMock, gst_tiovx_siso_mock,
    GST_TIOVX_SISO_TYPE,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_siso_mock_debug_category,
        "tiovxsisomock", 0, "debug category for tiovxsisomock element"));

     static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

     static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

     static gboolean gst_tiovx_siso_mock_create_node (GstTIOVXSiso * trans,
    vx_graph graph, vx_node node, vx_reference input, vx_reference output);

     static gboolean gst_tiovx_siso_mock_get_exemplar_refs (GstTIOVXSiso *
    trans, GstVideoInfo * in_caps_info, GstVideoInfo * out_caps_info,
    vx_reference input, vx_reference output);

     static void gst_tiovx_siso_mock_class_init (GstTIOVXSisoMockClass * klass)
{
  GstTIOVXSisoClass *tiovx_siso_class = GST_TIOVX_SISO_CLASS (klass);

  gst_element_class_add_pad_template ((GstElementClass *) klass,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_add_pad_template ((GstElementClass *) klass,
      gst_static_pad_template_get (&src_factory));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "TIOVX SISO MOCK", "Filter/Video",
      "TIOVX SISO MOCK element for testing purposes.",
      "Jafet Chaves <jafet.chaves@ridgerun.com>");

  tiovx_siso_class->create_node =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_mock_create_node);
  tiovx_siso_class->get_exemplar_refs =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_mock_get_exemplar_refs);
}

static void
gst_tiovx_siso_mock_init (GstTIOVXSisoMock * self)
{

}

static gboolean
gst_tiovx_siso_mock_create_node (GstTIOVXSiso * trans, vx_graph graph,
    vx_node node, vx_reference input, vx_reference output)
{
  return TRUE;
}

static gboolean
gst_tiovx_siso_mock_get_exemplar_refs (GstTIOVXSiso * trans,
    GstVideoInfo * in_caps_info, GstVideoInfo * out_caps_info,
    vx_reference input, vx_reference output)
{
  return TRUE;
}

GST_START_TEST (test_passthrough_on_same_caps)
{

}

GST_END_TEST;

static Suite *
gst_tiovx_siso_mock_suite (void)
{
  Suite *suite = suite_create ("tiovxsisomock");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_passthrough_on_same_caps);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_siso_mock);
