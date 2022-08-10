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
#include <gst-libs/gst/tiovx/gsttiovxsimo.h>
#include <gst-libs/gst/tiovx/gsttiovxutils.h>

#include <TI/tivx.h>

#define TEST_SIMO_OUTPUT 1
#define TEST_SIMO_NUM_CHANNELS 1

/* Start of Dummy SIMO element */

#define GST_TYPE_TEST_TIOVX_SIMO            (gst_tiovx_test_simo_get_type ())
#define GST_TIOVX_TEST_SIMO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TEST_TIOVX_SIMO, GstTestTIOVXSimo))
#define GST_TIOVX_TEST_SIMO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TEST_TIOVX_SIMO, GstTestTIOVXSimoClass))
#define GST_TIOVX_TEST_SIMO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TEST_TIOVX_SIMO, GstTestTIOVXSimoClass))

static const int input_param_id = 0;
static const int output_param_id_start = 1;

typedef struct _GstTestTIOVXSimo GstTestTIOVXSimo;
typedef struct _GstTestTIOVXSimoClass GstTestTIOVXSimoClass;

static GType gst_tiovx_test_simo_get_type (void);

/* Src caps */
#define TIOVX_TEST_SIMO_STATIC_CAPS_SRC \
  "video/x-raw, "                           \
  "format = (string) NV12, "                   \
  "width = [1 , 8192], "                    \
  "height = [1 , 8192]"

struct _GstTestTIOVXSimo
{
  GstTIOVXSimo parent;

  vx_object_array input;
  vx_object_array output[TEST_SIMO_OUTPUT];
  vx_reference input_ref;
  vx_reference output_ref[TEST_SIMO_OUTPUT];

  vx_node node;
};

struct _GstTestTIOVXSimoClass
{
  GstTIOVXSimoClass parent_class;
};

#define gst_tiovx_test_simo_parent_class parent_class
G_DEFINE_TYPE (GstTestTIOVXSimo, gst_tiovx_test_simo, GST_TYPE_TIOVX_SIMO);

static void
gst_test_tiovx_simo_create_vx_object_array (GstTestTIOVXSimo * self,
    vx_context context, GstCaps * caps, vx_object_array * array,
    vx_reference * reference)
{
  GstVideoInfo info;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (self, "Unable to get video info from caps");
    return;
  }

  *reference = (vx_reference) vxCreateImage (context, info.width,
      info.height, gst_format_to_vx_format (info.finfo->format));
  *array = vxCreateObjectArray (context, *reference, TEST_SIMO_NUM_CHANNELS);
}

static gboolean
gst_tiovx_test_simo_init_module (GstTIOVXSimo * element, vx_context context,
    GstTIOVXPad * sink_pad, GList * src_pads, GstCaps * sink_caps,
    GList * src_caps_list, guint num_channels)
{
  GstTestTIOVXSimo *test_simo = GST_TIOVX_TEST_SIMO (element);
  GList *l = NULL;
  gint i = 0;

  gst_test_tiovx_simo_create_vx_object_array (test_simo, context,
      sink_caps, &test_simo->input, &test_simo->input_ref);

  for (l = src_caps_list, i = 0; l; l = l->next, i++) {
    GstCaps *src_caps = (GstCaps *) l->data;

    gst_test_tiovx_simo_create_vx_object_array (test_simo, context, src_caps,
        &test_simo->output[i], &test_simo->output_ref[i]);
  }


  return TRUE;
}

static gboolean
gst_tiovx_test_simo_get_node_info (GstTIOVXSimo * element, vx_node * node,
    GstTIOVXPad * sink_pad, GList * src_pads, GList ** queueable_objects)
{
  GstTestTIOVXSimo *test_simo = GST_TIOVX_TEST_SIMO (element);
  GList *l = NULL;

  *node = test_simo->node;

  gst_tiovx_pad_set_params (sink_pad, test_simo->input, test_simo->input_ref,
      input_param_id, input_param_id);

  for (l = src_pads; l != NULL; l = l->next) {
    GstTIOVXPad *src_pad = (GstTIOVXPad *) l->data;
    gint i = g_list_position (src_pads, l);

    /* Set output exemplar */
    gst_tiovx_pad_set_params (src_pad, test_simo->output[i],
        test_simo->output_ref[i], output_param_id_start + i,
        output_param_id_start + i);

  }

  return TRUE;
}

static gboolean
gst_tiovx_test_simo_create_graph (GstTIOVXSimo * element, vx_context context,
    vx_graph graph)
{
  GstTestTIOVXSimo *test_simo = GST_TIOVX_TEST_SIMO (element);
  vx_parameter parameter = NULL;
  vx_status status = VX_FAILURE;
  guint i = 0;

  test_simo->node =
      tivxVpacMscScaleNode (graph, (vx_image) test_simo->input,
      (vx_image) test_simo->output[0], NULL, NULL, NULL, NULL);

  parameter = vxGetParameterByIndex (test_simo->node, i);
  status = vxAddParameterToGraph (graph, parameter);
  if (status == VX_SUCCESS) {
    status = vxReleaseParameter (&parameter);
  }

  for (i = 0; i < TEST_SIMO_OUTPUT; i++) {
    parameter = vxGetParameterByIndex (test_simo->node, i + 1);
    status = vxAddParameterToGraph (graph, parameter);
    if (status == VX_SUCCESS) {
      status = vxReleaseParameter (&parameter);
    }
  }

  return TRUE;
}

static gboolean
gst_tiovx_test_simo_configure_module (GstTIOVXSimo * element)
{
  return TRUE;
}

static gboolean
gst_tiovx_test_simo_deinit_module (GstTIOVXSimo * element)
{
  GstTestTIOVXSimo *test_simo = GST_TIOVX_TEST_SIMO (element);
  gint i = 0;

  vxReleaseObjectArray (&test_simo->input);

  for (i = 0; i < TEST_SIMO_OUTPUT; i++) {
    vxReleaseObjectArray (&test_simo->output[i]);
  }

  return TRUE;
}

static gboolean
gst_tiovx_test_simo_compare_caps (GstTIOVXSimo * element, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  return TRUE;
}

static GList *
gst_tiovx_test_simo_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list)
{
  GList *l = NULL;
  GstStructure *sink_structure = NULL;
  GList *result_caps_list = NULL;
  gint width = 0;
  gint height = 0;
  const gchar *format = NULL;

  g_return_val_if_fail (sink_caps, NULL);
  g_return_val_if_fail (gst_caps_is_fixed (sink_caps), NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_DEBUG_OBJECT (simo, "Fixating src caps from sink caps %" GST_PTR_FORMAT,
      sink_caps);

  sink_structure = gst_caps_get_structure (sink_caps, 0);

  if (!gst_structure_get_int (sink_structure, "width", &width)) {
    GST_ERROR_OBJECT (simo, "Width is missing in sink caps");
    return NULL;
  }

  if (!gst_structure_get_int (sink_structure, "height", &height)) {
    GST_ERROR_OBJECT (simo, "Height is missing in sink caps");
    return NULL;
  }

  format = gst_structure_get_string (sink_structure, "format");
  if (!format) {
    GST_ERROR_OBJECT (simo, "Format is missing in sink caps");
    return NULL;
  }

  for (l = src_caps_list; l != NULL; l = l->next) {
    GstCaps *src_caps = (GstCaps *) l->data;
    GstStructure *src_st = gst_caps_get_structure (src_caps, 0);
    GstCaps *new_caps = gst_caps_fixate (gst_caps_ref (src_caps));
    GstStructure *new_st = gst_caps_get_structure (new_caps, 0);
    const GValue *vwidth = NULL, *vheight = NULL, *vformat = NULL;

    vwidth = gst_structure_get_value (src_st, "width");
    vheight = gst_structure_get_value (src_st, "height");
    vformat = gst_structure_get_value (src_st, "format");

    gst_structure_set_value (new_st, "width", vwidth);
    gst_structure_set_value (new_st, "height", vheight);
    gst_structure_set_value (new_st, "format", vformat);

    gst_structure_fixate_field_nearest_int (new_st, "width", width);
    gst_structure_fixate_field_nearest_int (new_st, "height", height);
    gst_structure_fixate_field_string (new_st, "format", format);

    GST_DEBUG_OBJECT (simo, "Fixated %" GST_PTR_FORMAT " into %" GST_PTR_FORMAT,
        src_caps, new_caps);

    result_caps_list = g_list_append (result_caps_list, new_caps);
  }

  return result_caps_list;
}

static void
gst_tiovx_test_simo_class_init (GstTestTIOVXSimoClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstTIOVXSimoClass *simo_class = GST_TIOVX_SIMO_CLASS (klass);

  static GstStaticPadTemplate _src_template =
      GST_STATIC_PAD_TEMPLATE ("src_%u", GST_PAD_SRC, GST_PAD_REQUEST,
      GST_STATIC_CAPS (TIOVX_TEST_SIMO_STATIC_CAPS_SRC));

  static GstStaticPadTemplate _sink_template =
      GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &_src_template, GST_TYPE_TIOVX_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &_sink_template, GST_TYPE_TIOVX_PAD);

  gst_element_class_set_static_metadata (gstelement_class, "TIOVXSimo",
      "Testing", "Testing simo", "RidgeRun <support@ridgerun.com>");

  simo_class->init_module = GST_DEBUG_FUNCPTR (gst_tiovx_test_simo_init_module);
  simo_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_test_simo_get_node_info);
  simo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_test_simo_create_graph);
  simo_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_test_simo_configure_module);
  simo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_test_simo_deinit_module);
  simo_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_test_simo_compare_caps);
  simo_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_tiovx_test_simo_fixate_caps);
}

static void
gst_tiovx_test_simo_init (GstTestTIOVXSimo * self)
{

}

static gboolean
gst_tiovx_test_simo_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "testtiovxsimo", GST_RANK_NONE,
      GST_TYPE_TEST_TIOVX_SIMO);
}

static gboolean
gst_tiovx_test_simo_plugin_register (void)
{
  return gst_plugin_register_static (GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "testtiovxsimo",
      "Combine buffers",
      gst_tiovx_test_simo_plugin_init,
      VERSION, GST_LICENSE_UNKNOWN, PACKAGE, GST_PACKAGE_NAME,
      GST_PACKAGE_ORIGIN);
}

/* End of Dummy SIMO element */

static void
initialize_harness_and_element (GstHarness ** h, GstElement ** dummy_siso)
{
  *dummy_siso = gst_element_factory_make ("testtiovxsimo", NULL);
  *h = gst_harness_new_with_element (*dummy_siso, "sink", "src_%u");
  fail_if (NULL == *h, "Unable to create Test TIOVXSimo harness");

  /* we must specify a caps before pushing buffers */
  gst_harness_set_src_caps_str (*h,
      "video/x-raw, format=NV12, width=320, height=240");
  gst_harness_set_sink_caps_str (*h,
      "video/x-raw, format=NV12, width=[320, 640], height=[240, 480]");
}

GST_START_TEST (test_success)
{
  GstElement *dummy_simo = NULL;
  GstHarness *h = NULL;
  GstBuffer *in_buf = NULL;
  GstBuffer *out_buf = NULL;

  initialize_harness_and_element (&h, &dummy_simo);

  /* create a buffer of correct size */
  in_buf = gst_harness_create_buffer (h, 320 * 240 * 1.5);

  /* push the buffer into the queue */
  gst_harness_push (h, in_buf);

  /* pull the buffer from the queue */
  out_buf = gst_harness_pull (h);

  fail_if (out_buf == NULL);

  /* cleanup */
  gst_buffer_unref (out_buf);
  gst_harness_teardown (h);
  gst_object_unref (dummy_simo);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite;
  TCase *tc;

  gst_tiovx_test_simo_plugin_register ();

  suite = suite_create ("tiovxsimo");
  tc = tcase_create ("tc");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_success);

  return suite;
}

GST_CHECK_MAIN (gst_state);
