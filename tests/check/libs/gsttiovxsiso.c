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
#include <gst-libs/gst/tiovx/gsttiovxsiso.h>
#include <gst-libs/gst/tiovx/gsttiovxutils.h>

#include <TI/tivx.h>

/* Start of Dummy SISO element */

#define TIOVX_TEST_SISO_STATIC_CAPS \
  "video/x-raw, "                   \
  "format = (string) NV12, "        \
  "width = [1 , 8192], "            \
  "height = [1 , 8192]"

#define GST_TYPE_TEST_TIOVX_SISO            (gst_test_tiovx_siso_get_type ())
#define GST_TEST_TIOVX_SISO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TEST_TIOVX_SISO, GstTestTIOVXSiso))
#define GST_TEST_TIOVX_SISO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TEST_TIOVX_SISO, GstTestTIOVXSisoClass))
#define GST_TEST_TIOVX_SISO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TEST_TIOVX_SISO, GstTestTIOVXSisoClass))

typedef struct _GstTestTIOVXSiso GstTestTIOVXSiso;
typedef struct _GstTestTIOVXSisoClass GstTestTIOVXSisoClass;

static GType gst_test_tiovx_siso_get_type (void);

static const int test_tiovx_siso_input_param_index = 0;
static const int test_tiovx_siso_output_param_index = 1;

struct _GstTestTIOVXSiso
{
  GstTIOVXSiso parent;

  vx_object_array input;
  vx_object_array output;
  vx_reference input_ref;
  vx_reference output_ref;
  guint input_param_id;
  guint output_param_id;

  vx_node node;
};

struct _GstTestTIOVXSisoClass
{
  GstTIOVXSisoClass parent_class;
};

#define gst_test_tiovx_siso_parent_class parent_class
G_DEFINE_TYPE (GstTestTIOVXSiso, gst_test_tiovx_siso, GST_TYPE_TIOVX_SISO);

static void
gst_test_tiovx_siso_create_vx_object_array (GstTestTIOVXSiso * trans,
    vx_context context, GstCaps * caps, vx_object_array * array,
    vx_reference * exemplar)
{
  GstVideoInfo info;
  vx_image image = NULL;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (trans, "Unable to get video info from caps");
    return;
  }

  image =
      vxCreateImage (context, info.width, info.height,
      gst_format_to_vx_format (info.finfo->format));

  *exemplar = (vx_reference) image;
  *array = vxCreateObjectArray (context, (vx_reference) image, 1);
}

static gboolean
gst_test_tiovx_siso_init_module (GstTIOVXSiso * trans, vx_context context,
    GstCaps * in_caps, GstCaps * out_caps, guint num_channels)
{
  GstTestTIOVXSiso *test_siso = GST_TEST_TIOVX_SISO (trans);

  gst_test_tiovx_siso_create_vx_object_array (test_siso, context, in_caps,
      &test_siso->input, &test_siso->input_ref);

  gst_test_tiovx_siso_create_vx_object_array (test_siso, context,
      out_caps, &test_siso->output, &test_siso->output_ref);

  return TRUE;
}

static gboolean
gst_test_tiovx_siso_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node, guint * input_param_index,
    guint * output_param_index)
{
  GstTestTIOVXSiso *test_siso = GST_TEST_TIOVX_SISO (trans);

  *node = test_siso->node;
  *input = test_siso->input;
  *output = test_siso->output;
  *input_ref = test_siso->input_ref;
  *output_ref = test_siso->output_ref;

  *input_param_index = test_tiovx_siso_input_param_index;
  *output_param_index = test_tiovx_siso_output_param_index;

  return TRUE;
}

static gboolean
gst_test_tiovx_siso_create_graph (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph)
{
  GstTestTIOVXSiso *test_siso = GST_TEST_TIOVX_SISO (trans);
  vx_parameter parameter = NULL;
  vx_status status = VX_FAILURE;
  guint i = 0;

  test_siso->node =
      tivxVpacMscScaleNode (graph, (vx_image) test_siso->input,
      (vx_image) test_siso->output, NULL, NULL, NULL, NULL);

  parameter = vxGetParameterByIndex (test_siso->node, i);
  status = vxAddParameterToGraph (graph, parameter);
  if (status == VX_SUCCESS) {
    status = vxReleaseParameter (&parameter);
  }
  i++;

  parameter = vxGetParameterByIndex (test_siso->node, i);
  status = vxAddParameterToGraph (graph, parameter);
  if (status == VX_SUCCESS) {
    status = vxReleaseParameter (&parameter);
  }

  return TRUE;
}

static gboolean
gst_test_tiovx_siso_release_buffer (GstTIOVXSiso * trans)
{
  return TRUE;
}

static gboolean
gst_test_tiovx_siso_deinit_module (GstTIOVXSiso * trans, vx_context context)
{
  GstTestTIOVXSiso *test_siso = GST_TEST_TIOVX_SISO (trans);

  vxReleaseObjectArray (&test_siso->input);
  vxReleaseObjectArray (&test_siso->output);

  return TRUE;
}

static gboolean
gst_test_tiovx_siso_compare_caps (GstTIOVXSiso * trans, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  return TRUE;
}

static void
gst_test_tiovx_siso_class_init (GstTestTIOVXSisoClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstTIOVXSisoClass *siso_class = GST_TIOVX_SISO_CLASS (klass);

  static GstStaticPadTemplate src_template =
      GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      GST_STATIC_CAPS (TIOVX_TEST_SISO_STATIC_CAPS));

  static GstStaticPadTemplate sink_template =
      GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS (TIOVX_TEST_SISO_STATIC_CAPS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_element_class_set_static_metadata (gstelement_class, "TIOVXSiso",
      "Testing", "Testing siso", "RidgeRun <support@ridgerun.com>");

  base_transform_class->passthrough_on_same_caps = FALSE;

  siso_class->init_module = GST_DEBUG_FUNCPTR (gst_test_tiovx_siso_init_module);
  siso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_siso_get_node_info);
  siso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_siso_create_graph);
  siso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_siso_deinit_module);
  siso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_siso_release_buffer);
  siso_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_test_tiovx_siso_compare_caps);
}

static void
gst_test_tiovx_siso_init (GstTestTIOVXSiso * self)
{

}

static gboolean
gst_test_tiovx_siso_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "testtiovxsiso", GST_RANK_NONE,
      GST_TYPE_TEST_TIOVX_SISO);
}

static gboolean
gst_test_tiovx_siso_plugin_register (void)
{
  return gst_plugin_register_static (GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "testtiovxsiso",
      "Combine buffers",
      gst_test_tiovx_siso_plugin_init,
      VERSION, GST_LICENSE_UNKNOWN, PACKAGE, GST_PACKAGE_NAME,
      GST_PACKAGE_ORIGIN);
}

/* End of Dummy SIMO element */

static void
initialize_harness_and_element (GstHarness ** h)
{
  *h = gst_harness_new ("testtiovxsiso");
  fail_if (NULL == *h, "Unable to create Test TIOVXSiso harness");

  /* we must specify a caps before pushing buffers */
  gst_harness_set_sink_caps_str (*h,
      "video/x-raw, format=NV12, width=320, height=240");
  gst_harness_set_src_caps_str (*h,
      "video/x-raw, format=NV12, width=320, height=240");
}

GST_START_TEST (test_success)
{
  GstHarness *h = NULL;
  GstBuffer *in_buf = NULL;
  GstBuffer *out_buf = NULL;

  initialize_harness_and_element (&h);

  /* create a buffer of the appropiate size */
  in_buf = gst_harness_create_buffer (h, 320 * 240 * 1.5);

  /* push the buffer into the queue */
  gst_harness_push (h, in_buf);

  /* pull the buffer from the queue */
  out_buf = gst_harness_pull (h);

  fail_if (out_buf == NULL);

  /* cleanup */
  gst_buffer_unref (out_buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite;
  TCase *tc;

  gst_test_tiovx_siso_plugin_register ();

  suite = suite_create ("tiovxsiso");
  tc = tcase_create ("tc");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_success);

  return suite;
}

GST_CHECK_MAIN (gst_state);
