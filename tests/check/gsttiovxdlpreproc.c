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
 * *	No reverse engineering, decompilation, or disassembly of this software
 *      is permitted with respect to any software provided in binary form.
 *
 * *	Any redistribution and use are licensed by TI for use only with TI Devices.
 *
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
 *
 * *	Any redistribution and use of the source code, including any resulting
 *      derivative works, are licensed by TI for use only with TI Devices.
 *
 * *	Any redistribution and use of any object code compiled from the source
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
#include <gst/video/video-format.h>

#include "test_utils.h"

static const gchar *test_pipelines[] = {
  "videotestsrc ! tiovxdlpreproc ! application/x-tensor-tiovx ! fakesink",
  NULL,
};

enum
{
  /* Pipelines names */
  PIPELINE_SIMPLE = 0,
};

/* Supported formats */
#define TIOVXDLPREPROC_FORMATS_NUMBER 4
static const gchar *tiovxdlpreproc_formats[TIOVXDLPREPROC_FORMATS_NUMBER + 1] = {
  "RGB",
  "BGR",
  "NV12",
  "NV21",
  NULL,
};

/* Supported dimensions */
#define TIOVXDLPREPROC_DIMENSIONS_RANGE_VALUE 1
static const uint tiovxdlpreproc_width[TIOVXDLPREPROC_DIMENSIONS_RANGE_VALUE +
    1] = { 1, 8192 };
static const uint tiovxdlpreproc_height[TIOVXDLPREPROC_DIMENSIONS_RANGE_VALUE +
    1] = { 1, 8192 };

/* Supported framerate */
#define TIOVXDLPREPROC_FRAMERATE_RANGE_VALUE 1
static const uint tiovxdlpreproc_framerate[TIOVXDLPREPROC_DIMENSIONS_RANGE_VALUE
    + 1] = { 1, 2147483647 };

/* Supported data-type */
#define TIOVXDLPREPROC_DATA_TYPE_NUMBER 7
static const gchar *tiovxdlpreproc_data_type[TIOVXDLPREPROC_DATA_TYPE_NUMBER +
    1] = {
  "int8",
  "uint8",
  "int16",
  "uint16",
  "int32",
  "uint32",
  "float32",
  NULL,
};

/* Supported channel-order */
#define TIOVXDLPREPROC_CHANNEL_ORDER_NUMBER 2
static const gchar
    * tiovxdlpreproc_channel_order[TIOVXDLPREPROC_CHANNEL_ORDER_NUMBER + 1] = {
  "nchw",
  "nhwc",
  NULL,
};

/* Supported tensor-format */
#define TIOVXDLPREPROC_TENSOR_FORMAT_NUMBER 2
static const gchar
    * tiovxdlpreproc_tensor_format[TIOVXDLPREPROC_TENSOR_FORMAT_NUMBER + 1] = {
  "rgb",
  "bgr",
  NULL,
};

/* Supported mean range */
#define TIOVXDLPREPROC_MEAN_NUMBER 1
static const gdouble
    tiovxdlpreproc_mean[TIOVXDLPREPROC_MEAN_NUMBER + 1] = { 0.0, 255.0 };

/* Supported scale range */
#define TIOVXDLPREPROC_SCALE_NUMBER 1
static const gdouble
    tiovxdlpreproc_scale[TIOVXDLPREPROC_SCALE_NUMBER + 1] = { 0.0, 1.0 };

typedef struct
{
  const uint *width;
  const uint *height;
  const uint *framerate;
  const gchar **formats;
} PadTemplateSink;

typedef struct
{
  const gchar **data_type;
  const gchar **channel_order;
  const gchar **tensor_format;
} PadTemplateSrc;

typedef struct
{
  PadTemplateSrc src_pad;
  PadTemplateSink sink_pad;

  const gdouble *mean;
  const gdouble *scale;
} TIOVXDLPreProcModeled;

static const void gst_tiovx_dl_pre_proc_modeling_init (TIOVXDLPreProcModeled *
    element);

static const void
gst_tiovx_dl_pre_proc_modeling_init (TIOVXDLPreProcModeled * element)
{
  element->sink_pad.formats = tiovxdlpreproc_formats;
  element->sink_pad.width = tiovxdlpreproc_width;
  element->sink_pad.height = tiovxdlpreproc_height;
  element->sink_pad.framerate = tiovxdlpreproc_framerate;

  element->src_pad.data_type = tiovxdlpreproc_data_type;
  element->src_pad.channel_order = tiovxdlpreproc_channel_order;
  element->src_pad.tensor_format = tiovxdlpreproc_tensor_format;

  element->mean = tiovxdlpreproc_mean;
  element->scale = tiovxdlpreproc_scale;
}

GST_START_TEST (test_state_transitions)
{
  test_states_change (test_pipelines[PIPELINE_SIMPLE]);
}

GST_END_TEST;

GST_START_TEST (test_state_transitions_fail)
{
  test_states_change (test_pipelines[PIPELINE_SIMPLE]);
}

GST_END_TEST;

GST_START_TEST (test_state_change_foreach_upstream_format)
{
  TIOVXDLPreProcModeled element = { 0 };
  uint i = 0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  for (i = 0; i < TIOVXDLPREPROC_FORMATS_NUMBER; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) upstream_caps = g_string_new ("");
    g_autoptr (GString) downstream_caps = g_string_new ("");

    /* Upstream caps */
    g_string_printf (upstream_caps, "video/x-raw,format=%s",
        element.sink_pad.formats[i]);

    /* Downstream caps */
    g_string_printf (downstream_caps, "application/x-tensor-tiovx");

    g_string_printf (pipeline,
        "videotestsrc ! %s ! tiovxdlpreproc ! %s ! fakesink ",
        upstream_caps->str, downstream_caps->str);

    test_states_change (pipeline->str);

    GST_DEBUG
        ("test_state_change_foreach_upstream_format pipeline description: %"
        GST_PTR_FORMAT, pipeline);
  }
}

GST_END_TEST;

GST_START_TEST (test_state_change_foreach_upstream_format_fail)
{
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  g_autoptr (GString) downstream_caps = g_string_new ("");

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,format=%d",
      GST_VIDEO_FORMAT_UNKNOWN);

  /* Downstream caps */
  g_string_printf (downstream_caps, "application/x-tensor-tiovx");

  g_string_printf (pipeline,
      "videotestsrc ! %s ! tiovxdlpreproc ! %s ! fakesink",
      upstream_caps->str, downstream_caps->str);

  g_assert_true (NULL != test_create_pipeline_fail (pipeline->str));

  GST_DEBUG
      ("test_state_change_foreach_upstream_format pipeline description: %"
      GST_PTR_FORMAT, pipeline);
}

GST_END_TEST;

GST_START_TEST (test_state_change_dimensions)
{
  TIOVXDLPreProcModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  g_autoptr (GString) downstream_caps = g_string_new ("");
  GRand *g_rand = NULL;
  const uint tiovx_preproc_image_max_size_root = 3500;
  gint32 width = 0;
  gint32 height = 0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  g_rand = g_rand_new ();
  width =
      g_rand_int_range (g_rand, element.sink_pad.width[0],
      element.sink_pad.width[1]);
  height =
      g_rand_int_range (g_rand, element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Make sure the random image is no bigger of what TIOVX can allocate */
  if ((width * height) >
      (tiovx_preproc_image_max_size_root * tiovx_preproc_image_max_size_root)) {
    int delta = 0;
    if (width > tiovx_preproc_image_max_size_root) {
      delta = width - tiovx_preproc_image_max_size_root;
    }

    if (width % 2 == 0) {
      width -= delta;
    } else {
      height -= delta;
    }
  }

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,width=%d,height=%d", width,
      height);

  /* Downstream caps */
  g_string_printf (downstream_caps,
      "application/x-tensor-tiovx,width=%d,height=%d", width, height);

  g_string_printf (pipeline,
      "videotestsrc ! %s ! tiovxdlpreproc ! %s ! fakesink ",
      upstream_caps->str, downstream_caps->str);

  test_states_change (pipeline->str);

  GST_DEBUG
      ("test_state_change_dimensions pipeline description: %"
      GST_PTR_FORMAT, pipeline);

  g_rand_free (g_rand);
}

GST_END_TEST;

GST_START_TEST (test_state_change_dimensions_with_upscale_fail)
{
  TIOVXDLPreProcModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  g_autoptr (GString) downstream_caps = g_string_new ("");
  gint32 width = 0;
  gint32 height = 0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,width=%d,height=%d", width,
      height);

  /* Downstream caps */
  g_string_printf (downstream_caps,
      "application/x-tensor-tiovx,tensor-width=%d,tensor-height=%d", width + 1,
      height + 1);

  g_string_printf (pipeline,
      "videotestsrc ! %s ! tiovxdlpreproc ! %s ! fakesink ",
      upstream_caps->str, downstream_caps->str);

  test_states_change_fail (pipeline->str);
}

GST_END_TEST;

GST_START_TEST (test_state_change_dimensions_with_downscale_fail)
{
  TIOVXDLPreProcModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  g_autoptr (GString) downstream_caps = g_string_new ("");
  gint32 width = 0;
  gint32 height = 0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,width=%d,height=%d", width,
      height);

  /* Downstream caps */
  g_string_printf (downstream_caps,
      "application/x-tensor-tiovx,tensor-width=%d,tensor-height=%d", width - 1,
      height - 1);

  g_string_printf (pipeline,
      "videotestsrc ! %s ! tiovxdlpreproc ! %s ! fakesink ",
      upstream_caps->str, downstream_caps->str);

  test_states_change_fail (pipeline->str);
}

GST_END_TEST;

GST_START_TEST (test_state_change_for_framerate)
{
  TIOVXDLPreProcModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  g_autoptr (GString) downstream_caps = g_string_new ("");
  long int framerate = 0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  framerate =
      g_random_int_range (element.sink_pad.framerate[0],
      element.sink_pad.framerate[1]);

  /* Upstream caps */
  g_string_printf (upstream_caps, "video/x-raw,framerate=%ld/1", framerate);

  /* Downstream caps */
  g_string_printf (downstream_caps, "application/x-tensor-tiovx");

  g_string_printf (pipeline,
      "videotestsrc ! %s ! tiovxdlpreproc ! %s ! fakesink ",
      upstream_caps->str, downstream_caps->str);

  test_states_change (pipeline->str);

  GST_DEBUG
      ("test_state_change_foreach_upstream_format pipeline description: %"
      GST_PTR_FORMAT, pipeline);
}

GST_END_TEST;

GST_START_TEST (test_state_change_foreach_data_type)
{
  TIOVXDLPreProcModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) properties = g_string_new ("");
  uint i = 0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  for (i = 0; i < TIOVXDLPREPROC_DATA_TYPE_NUMBER; i++) {
    /* Properties */
    g_string_printf (properties, "data-type=%s", element.src_pad.data_type[i]);

    g_string_printf (pipeline,
        "videotestsrc ! video/x-raw ! tiovxdlpreproc %s ! application/x-tensor-tiovx ! fakesink ",
        properties->str);

    test_states_change (pipeline->str);

    GST_DEBUG
        ("test_state_change_foreach_data_type pipeline description: %"
        GST_PTR_FORMAT, pipeline);
  }
}

GST_END_TEST;

GST_START_TEST (test_state_change_foreach_data_type_fail)
{
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) properties = g_string_new ("");

  /* Properties */
  g_string_printf (properties, "data-type=%p", NULL);

  g_string_printf (pipeline,
      "videotestsrc ! video/x-raw ! tiovxdlpreproc %s ! application/x-tensor-tiovx ! fakesink ",
      properties->str);

  g_assert_true (NULL != test_create_pipeline_fail (pipeline->str));
}

GST_END_TEST;

GST_START_TEST (test_state_change_foreach_channel_order)
{
  TIOVXDLPreProcModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) properties = g_string_new ("");
  uint i = 0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  for (i = 0; i < TIOVXDLPREPROC_CHANNEL_ORDER_NUMBER; i++) {
    /* Properties */
    g_string_printf (properties, "channel-order=%s",
        element.src_pad.channel_order[i]);

    g_string_printf (pipeline,
        "videotestsrc ! video/x-raw ! tiovxdlpreproc %s ! application/x-tensor-tiovx ! fakesink ",
        properties->str);

    test_states_change (pipeline->str);

    GST_DEBUG
        ("test_state_change_foreach_channel_order pipeline description: %"
        GST_PTR_FORMAT, pipeline);
  }
}

GST_END_TEST;

GST_START_TEST (test_state_change_foreach_channel_order_fail)
{
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) properties = g_string_new ("");

  /* Properties */
  g_string_printf (properties, "channel-order=%p", NULL);

  g_string_printf (pipeline,
      "videotestsrc ! video/x-raw ! tiovxdlpreproc %s ! application/x-tensor-tiovx ! fakesink ",
      properties->str);

  g_assert_true (NULL != test_create_pipeline_fail (pipeline->str));
}

GST_END_TEST;

GST_START_TEST (test_state_change_foreach_tensor_format)
{
  TIOVXDLPreProcModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) properties = g_string_new ("");
  uint i = 0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  for (i = 0; i < TIOVXDLPREPROC_TENSOR_FORMAT_NUMBER; i++) {
    /* Properties */
    g_string_printf (properties, "tensor-format=%s",
        element.src_pad.tensor_format[i]);

    g_string_printf (pipeline,
        "videotestsrc ! video/x-raw ! tiovxdlpreproc %s ! application/x-tensor-tiovx ! fakesink ",
        properties->str);

    test_states_change (pipeline->str);

    GST_DEBUG
        ("test_state_change_foreach_channel_order pipeline description: %"
        GST_PTR_FORMAT, pipeline);
  }
}

GST_END_TEST;

GST_START_TEST (test_state_change_for_mean_and_scale)
{
  TIOVXDLPreProcModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) properties = g_string_new ("");
  float mean0 = 0.0;
  float mean1 = 0.0;
  float mean2 = 0.0;
  float scale0 = 0.0;
  float scale1 = 0.0;
  float scale2 = 0.0;

  gst_tiovx_dl_pre_proc_modeling_init (&element);

  mean0 = (float) g_random_double_range (element.mean[0], element.mean[1]);
  mean1 = (float) g_random_double_range (element.mean[0], element.mean[1]);
  mean2 = (float) g_random_double_range (element.mean[0], element.mean[1]);
  scale0 = (float) g_random_double_range (element.scale[0], element.scale[1]);
  scale1 = (float) g_random_double_range (element.scale[0], element.scale[1]);
  scale2 = (float) g_random_double_range (element.scale[0], element.scale[1]);

  /* Properties */
  g_string_printf (properties,
      "mean-0=%f mean-1=%f mean-2=%f scale-0=%f scale-1=%f scale-2=%f", mean0,
      mean1, mean2, scale0, scale1, scale2);

  g_string_printf (pipeline,
      "videotestsrc ! video/x-raw ! tiovxdlpreproc %s ! application/x-tensor-tiovx ! fakesink ",
      properties->str);

  test_states_change (pipeline->str);

  GST_DEBUG
      ("test_state_change_foreach_channel_order pipeline description: %"
      GST_PTR_FORMAT, pipeline);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("tiovxdlpreproc");
  TCase *tc = tcase_create ("tc");
  TCase *tc_properties = tcase_create ("tc_properties");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_state_transitions);
  tcase_add_test (tc, test_state_transitions_fail);
  tcase_add_test (tc, test_state_change_foreach_upstream_format);
  tcase_add_test (tc, test_state_change_foreach_upstream_format_fail);
  tcase_add_test (tc, test_state_change_dimensions);
  tcase_add_test (tc, test_state_change_dimensions_with_upscale_fail);
  tcase_add_test (tc, test_state_change_dimensions_with_downscale_fail);
  tcase_add_test (tc, test_state_change_for_framerate);

  suite_add_tcase (suite, tc_properties);
  tcase_add_test (tc_properties, test_state_change_foreach_data_type);
  tcase_add_test (tc_properties, test_state_change_foreach_data_type_fail);
  tcase_add_test (tc_properties, test_state_change_foreach_channel_order);
  tcase_add_test (tc_properties, test_state_change_foreach_channel_order_fail);
  tcase_add_test (tc_properties, test_state_change_foreach_tensor_format);
  tcase_add_test (tc_properties, test_state_change_for_mean_and_scale);

  return suite;
}

GST_CHECK_MAIN (gst_state);