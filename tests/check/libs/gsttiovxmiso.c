/*
 * Copyright (c) [2021] Texas Instruments Incorporated
 *
 * All rights reserved not granted herein.
 *
 * Limited License.
 *
 * Texas Instruments Incorporated grants a world-wide, royalty-free,
 * non-exclusive license under copyrights and patents it now or hereafter
 * owns or controls to make, have made, use, import, offer to sell and sell
 * ("Utilize") this software subject to the terms herein.  With respect to
 * the foregoing patent license, such license is granted  solely to the extent
 * that any such patent is necessary to Utilize the software alone.
 * The patent license shall not apply to any combinations which include
 * this software, other than combinations with devices manufactured by or
 * for TI (“TI Devices”).  No hardware patent is licensed hereunder.
 *
 * Redistributions must preserve existing copyright notices and reproduce
 * this license (including the above copyright notice and the disclaimer
 * and (if applicable) source code license limitations below) in the
 * documentation and/or other materials provided with the distribution
 *
 * Redistribution and use in binary form, without modification, are permitted
 * provided that the following conditions are met:
 *
 * *    No reverse engineering, decompilation, or disassembly of this software
 *      is permitted with respect to any software provided in binary form.
 *
 * *    Any redistribution and use are licensed by TI for use only with TI Devices.
 *
 * *    Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
 *
 * *    Any redistribution and use of the source code, including any resulting
 *      derivative works, are licensed by TI for use only with TI Devices.
 *
 * *    Any redistribution and use of any object code compiled from the source
 *      code and any resulting derivative works, are licensed by TI for use
 *      only with TI Devices.
 *
 * Neither the name of Texas Instruments Incorporated nor the names of its
 * suppliers may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * DISCLAIMER.
 *
 * THIS SOFTWARE IS PROVIDED BY TI AND TI’S LICENSORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TI AND TI’S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

#include <gst-libs/gst/tiovx/gsttiovxbufferpool.h>
#include <gst-libs/gst/tiovx/gsttiovxmiso.h>
#include <gst-libs/gst/tiovx/gsttiovxutils.h>

#include <TI/tivx.h>

#define TEST_MISO_INPUT 1
#define TEST_MISO_NUM_CHANNELS 1

/* Start of Dummy MISO element */

#define GST_TYPE_TEST_TIOVX_MISO            (gst_test_tiovx_miso_get_type ())
#define GST_TEST_TIOVX_MISO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TEST_TIOVX_MISO, GstTestTIOVXMiso))
#define GST_TEST_TIOVX_MISO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TEST_TIOVX_MISO, GstTestTIOVXMisoClass))
#define GST_TEST_TIOVX_MISO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TEST_TIOVX_MISO, GstTestTIOVXMisoClass))

typedef struct _GstTestTIOVXMiso GstTestTIOVXMiso;
typedef struct _GstTestTIOVXMisoClass GstTestTIOVXMisoClass;

static GType gst_test_tiovx_miso_get_type (void);

struct _GstTestTIOVXMiso
{
  GstTIOVXMiso parent;

  vx_object_array input[TEST_MISO_INPUT];
  vx_object_array output;
  vx_reference input_ref[TEST_MISO_INPUT];
  vx_reference output_ref;

  guint input_param_id[TEST_MISO_INPUT];
  guint output_param_id;

  vx_node node;
};

struct _GstTestTIOVXMisoClass
{
  GstTIOVXMisoClass parent_class;
};

#define gst_test_tiovx_miso_parent_class parent_class
G_DEFINE_TYPE (GstTestTIOVXMiso, gst_test_tiovx_miso, GST_TYPE_TIOVX_MISO);

static void
gst_test_tiovx_miso_create_vx_object_array (GstTestTIOVXMiso * agg,
    vx_context context, GstPad * pad, vx_object_array * array,
    vx_reference * reference)
{
  GstCaps *caps = NULL;
  GstVideoInfo info;

  caps = gst_pad_get_current_caps (pad);

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (agg, "Unable to get video info from caps");
    return;
  }

  *reference = (vx_reference) vxCreateImage (context, info.width,
      info.height, gst_format_to_vx_format (info.finfo->format));
  *array = vxCreateObjectArray (context, *reference, TEST_MISO_NUM_CHANNELS);
}

static gboolean
gst_test_tiovx_miso_init_module (GstTIOVXMiso * agg, vx_context context,
    GList * sink_pads_list, GstPad * src_pad, guint num_channels)
{
  GstTestTIOVXMiso *test_miso = GST_TEST_TIOVX_MISO (agg);
  GList *l = NULL;
  guint i = 0;

  for (l = sink_pads_list, i = 0; l; l = l->next, i++) {
    GstPad *pad = GST_PAD (l->data);

    gst_test_tiovx_miso_create_vx_object_array (test_miso, context, pad,
        &test_miso->input[i], &test_miso->input_ref[i]);
  }


  gst_test_tiovx_miso_create_vx_object_array (test_miso, context,
      GST_AGGREGATOR (agg)->srcpad, &test_miso->output, &test_miso->output_ref);

  return TRUE;
}

static gboolean
gst_test_tiovx_miso_get_node_info (GstTIOVXMiso * agg, GList * sink_pads_list,
    GstPad * src_pad, vx_node * node, GList ** queueable_objects)
{
  GstTestTIOVXMiso *test_miso = GST_TEST_TIOVX_MISO (agg);
  GList *l = NULL;
  gint i = 0;

  for (l = sink_pads_list, i = 0; l; l = l->next, i++) {
    GstTIOVXMisoPad *pad = GST_TIOVX_MISO_PAD (l->data);

    gst_tiovx_miso_pad_set_params (pad,
        test_miso->input[i], &test_miso->input_ref[i], i, i);
  }

  gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (GST_AGGREGATOR
          (agg)->srcpad), test_miso->output, &test_miso->output_ref, i, i);

  *node = test_miso->node;

  return TRUE;
}

static gboolean
gst_test_tiovx_miso_create_graph (GstTIOVXMiso * agg, vx_context context,
    vx_graph graph)
{
  GstTestTIOVXMiso *test_miso = GST_TEST_TIOVX_MISO (agg);
  vx_parameter parameter = NULL;
  vx_status status = VX_FAILURE;
  guint i = 0;

  test_miso->node =
      tivxVpacMscScaleNode (graph, (vx_image) test_miso->input[0],
      (vx_image) test_miso->output, NULL, NULL, NULL, NULL);

  for (i = 0; i < TEST_MISO_INPUT; i++) {
    parameter = vxGetParameterByIndex (test_miso->node, i);
    status = vxAddParameterToGraph (graph, parameter);
    if (status == VX_SUCCESS) {
      status = vxReleaseParameter (&parameter);
    }
  }
  parameter = vxGetParameterByIndex (test_miso->node, i);
  status = vxAddParameterToGraph (graph, parameter);
  if (status == VX_SUCCESS) {
    status = vxReleaseParameter (&parameter);
  }

  return TRUE;
}

static gboolean
gst_test_tiovx_miso_configure_module (GstTIOVXMiso * agg)
{
  return TRUE;
}

static gboolean
gst_test_tiovx_miso_release_buffer (GstTIOVXMiso * agg)
{
  return TRUE;
}

static gboolean
gst_test_tiovx_miso_deinit_module (GstTIOVXMiso * agg)
{
  GstTestTIOVXMiso *test_miso = GST_TEST_TIOVX_MISO (agg);
  gint i = 0;

  for (i = 0; i < TEST_MISO_INPUT; i++) {
    vxReleaseObjectArray (&test_miso->input[i]);
  }
  vxReleaseObjectArray (&test_miso->output);

  return TRUE;
}

static void
gst_test_tiovx_miso_class_init (GstTestTIOVXMisoClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstTIOVXMisoClass *miso_class = GST_TIOVX_MISO_CLASS (klass);

  static GstStaticPadTemplate _src_template =
      GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY);

  static GstStaticPadTemplate _sink_template =
      GST_STATIC_PAD_TEMPLATE ("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
      GST_STATIC_CAPS_ANY);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &_src_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &_sink_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_set_static_metadata (gstelement_class, "TIOVXMiso",
      "Testing", "Testing miso", "RidgeRun <support@ridgerun.com>");

  miso_class->init_module = GST_DEBUG_FUNCPTR (gst_test_tiovx_miso_init_module);
  miso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_miso_get_node_info);
  miso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_miso_create_graph);
  miso_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_miso_configure_module);
  miso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_miso_deinit_module);
  miso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_miso_release_buffer);
}

static void
gst_test_tiovx_miso_init (GstTestTIOVXMiso * self)
{

}

static gboolean
gst_test_tiovx_miso_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "testtiovxmiso", GST_RANK_NONE,
      GST_TYPE_TEST_TIOVX_MISO);
}

static gboolean
gst_test_tiovx_miso_plugin_register (void)
{
  return gst_plugin_register_static (GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "testtiovxmiso",
      "Combine buffers",
      gst_test_tiovx_miso_plugin_init,
      VERSION, GST_LICENSE_UNKNOWN, PACKAGE, GST_PACKAGE_NAME,
      GST_PACKAGE_ORIGIN);
}

/* End of Dummy SIMO element */

static void
initialize_harness_and_element (GstHarness ** h, GstElement ** dummy_siso)
{
  *dummy_siso = gst_element_factory_make ("testtiovxmiso", NULL);
  *h = gst_harness_new_with_element (*dummy_siso, "sink_%u", "src");
  fail_if (NULL == *h, "Unable to create Test TIOVXMiso harness");

  /* we must specify a caps before pushing buffers */
  gst_harness_set_src_caps_str (*h,
      "video/x-raw, format=NV12, width=320, height=240");
  gst_harness_set_sink_caps_str (*h,
      "video/x-raw, format=NV12, width=[320, 640], height=[240, 480]");
}

GST_START_TEST (test_success)
{
  GstElement *dummy_miso = NULL;
  GstHarness *h = NULL;
  GstBuffer *in_buf = NULL;
  GstBuffer *out_buf = NULL;

  initialize_harness_and_element (&h, &dummy_miso);

  /* create a buffer of size 42 */
  in_buf = gst_harness_create_buffer (h, 320 * 240 * 4);

  /* push the buffer into the queue */
  gst_harness_push (h, in_buf);

  /* pull the buffer from the queue */
  out_buf = gst_harness_pull (h);

  fail_if (out_buf == NULL);
  fail_if (!GST_TIOVX_IS_BUFFER_POOL (out_buf->pool));

  /* cleanup */
  gst_buffer_unref (out_buf);
  gst_harness_teardown (h);
  gst_object_unref (dummy_miso);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite;
  TCase *tc;

  gst_tiovx_init_debug ();

  gst_test_tiovx_miso_plugin_register ();

  suite = suite_create ("tiovxmiso");
  tc = tcase_create ("tc");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_success);

  return suite;
}

GST_CHECK_MAIN (gst_state);
