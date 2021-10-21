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

#define TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS 1

/* Supported formats */
#define TIOVXDLCOLORBLEND_FORMATS_ARRAY_SIZE 2
static const gchar
    * tiovxdlcolorblend_formats[TIOVXDLCOLORBLEND_FORMATS_ARRAY_SIZE] = {
  "RGB",
  "NV12",
};

/* Supported dimensions.
 * FIXME: TIOVX nodes doesn't support high input resolution for the moment.
*/
static const guint tiovxdlcolorblend_width[] = { 1, 2048 };
static const guint tiovxdlcolorblend_height[] = { 1, 1080 };

/* Supported data-type */
#define TIOVXDLCOLORBLEND_DATA_TYPE_ARRAY_SIZE 7
static const gchar
    * tiovxdlcolorblend_data_type[TIOVXDLCOLORBLEND_DATA_TYPE_ARRAY_SIZE] = {
  "int8",
  "uint8",
  "int16",
  "uint16",
  "int32",
  "uint32",
  "float32",
};

/* Supported num-classes */
static const guint64 tiovxdlcolorblend_num_classes[] = { 0, 4294967295U };

/* Supported targets */
#define TIOVXDLCOLORBLEND_TARGETS_ARRAY_SIZE 2
static const gchar
    * tiovxdlcolorblend_targets[TIOVXDLCOLORBLEND_TARGETS_ARRAY_SIZE] = {
  "DSP-1",
  "DSP-2",
};

typedef struct
{
  const guint *width;
  const guint *height;
  const gchar **formats;
} PadTemplateSink;

typedef struct
{
  const guint *width;
  const guint *height;
  const gchar **formats;
} PadTemplateSrc;

typedef struct
{
  const guint *width;
  const guint *height;
  const gchar **data_type;
} PadTemplateTensor;

typedef struct
{
  PadTemplateSrc src_pad;
  PadTemplateSink sink_pad;
  PadTemplateTensor tensor_pad;

  const guint64 *num_classes;
  const gchar **target;
} TIOVXDLColorBlendModeled;

static const void
gst_tiovx_dl_color_blend_modeling_init (TIOVXDLColorBlendModeled * element);

static const void
gst_tiovx_dl_color_blend_modeling_init (TIOVXDLColorBlendModeled * element)
{
  element->sink_pad.formats = tiovxdlcolorblend_formats;
  element->sink_pad.width = tiovxdlcolorblend_width;
  element->sink_pad.height = tiovxdlcolorblend_height;

  element->tensor_pad.data_type = tiovxdlcolorblend_data_type;
  element->tensor_pad.width = tiovxdlcolorblend_width;
  element->tensor_pad.height = tiovxdlcolorblend_height;

  element->num_classes = tiovxdlcolorblend_num_classes;
  element->target = tiovxdlcolorblend_targets;
}

GST_START_TEST (test_foreach_format)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint i = 0;
  guint data_type = 3;

  gst_tiovx_dl_color_blend_modeling_init (&element);

  for (i = 0; i < TIOVXDLCOLORBLEND_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) sink_caps = g_string_new ("");
    g_autoptr (GString) sink_src = g_string_new ("");
    g_autoptr (GString) tensor_caps = g_string_new ("");
    g_autoptr (GString) tensor_src = g_string_new ("");

    /* Sink pad */
    g_string_printf (sink_caps, "video/x-raw,format=%s",
        element.sink_pad.formats[i]);
    g_string_printf (sink_src, "videotestsrc is-live=true ! %s ! blend.sink",
        sink_caps->str);

    /* Tensor pad */
    g_string_printf (tensor_caps,
        "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
        data_type, 320, 240);
    g_string_printf (tensor_src,
        "multifilesrc loop=true location=/dev/null ! %s ! blend.tensor",
        tensor_caps->str);

    g_string_printf (pipeline,
        "tiovxdlcolorblend data-type=%d name=blend %s %s blend.src ! fakesink",
        data_type, sink_src->str, tensor_src->str);

    test_states_change_async (pipeline->str,
        TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_foreach_format_fail)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint data_type = 3;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  /* Sink pad */
  g_string_printf (sink_caps, "video/x-raw,format=%d",
      GST_VIDEO_FORMAT_UNKNOWN);
  g_string_printf (sink_src, "videotestsrc is-live=true ! %s ! blend.sink",
      sink_caps->str);

  /* Tensor pad */
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, 320, 240);
  g_string_printf (tensor_src,
      "multifilesrc loop=true location=/dev/null ! %s ! blend.tensor",
      tensor_caps->str);

  g_string_printf (pipeline,
      "tiovxdlcolorblend data-type=%d name=blend %s %s blend.src ! fakesink",
      data_type, sink_src->str, tensor_src->str);

  g_assert_true (NULL != test_create_pipeline_fail (pipeline->str));
}

GST_END_TEST;

GST_START_TEST (test_foreach_data_type)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint i = 0;

  gst_tiovx_dl_color_blend_modeling_init (&element);

  for (i = 0; i < TIOVXDLCOLORBLEND_DATA_TYPE_ARRAY_SIZE; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) sink_caps = g_string_new ("");
    g_autoptr (GString) sink_src = g_string_new ("");
    g_autoptr (GString) tensor_caps = g_string_new ("");
    g_autoptr (GString) tensor_src = g_string_new ("");

    /* Sink pad */
    g_string_printf (sink_caps, "video/x-raw,format=%s", "RGB");
    g_string_printf (sink_src, "videotestsrc is-live=true ! %s ! blend.sink",
        sink_caps->str);

    /* Tensor pad */
    g_string_printf (tensor_caps,
        "application/x-tensor-tiovx,data-type=%s,tensor-width=%d,tensor-height=%d",
        element.tensor_pad.data_type[i], 320, 240);
    g_string_printf (tensor_src,
        "multifilesrc loop=true location=/dev/null ! %s ! blend.tensor",
        tensor_caps->str);

    g_string_printf (pipeline,
        "tiovxdlcolorblend data-type=%s name=blend %s %s blend.src ! fakesink",
        element.tensor_pad.data_type[i], sink_src->str, tensor_src->str);

    test_states_change_async (pipeline->str,
        TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_foreach_target)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint i = 0;
  guint data_type = 3;

  gst_tiovx_dl_color_blend_modeling_init (&element);

  for (i = 0; i < TIOVXDLCOLORBLEND_TARGETS_ARRAY_SIZE; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) sink_src = g_string_new ("");
    g_autoptr (GString) tensor_caps = g_string_new ("");
    g_autoptr (GString) tensor_src = g_string_new ("");

    /* Sink pad */
    g_string_printf (sink_src, "videotestsrc is-live=true ! blend.sink");

    /* Tensor pad */
    g_string_printf (tensor_caps,
        "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
        data_type, 320, 240);
    g_string_printf (tensor_src,
        "multifilesrc loop=true location=/dev/null ! %s ! blend.tensor",
        tensor_caps->str);

    g_string_printf (pipeline,
        "tiovxdlcolorblend target=%s data-type=%d name=blend %s %s blend.src ! fakesink",
        element.target[i], data_type, sink_src->str, tensor_src->str);

    test_states_change_async (pipeline->str,
        TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_num_classes)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint data_type = 3;
  guint64 num_classes = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  /* Sink pad */
  g_string_printf (sink_src, "videotestsrc is-live=true ! blend.sink");

  /* Tensor pad */
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, 320, 240);
  g_string_printf (tensor_src,
      "multifilesrc loop=true location=/dev/null ! %s ! blend.tensor",
      tensor_caps->str);

  num_classes =
      g_random_double_range (element.num_classes[0], element.num_classes[1]);

  g_string_printf (pipeline,
      "tiovxdlcolorblend num-classes=%ld data-type=%d name=blend %s %s blend.src ! fakesink",
      num_classes, data_type, sink_src->str, tensor_src->str);

  test_states_change_async (pipeline->str,
      TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_resolutions)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint data_type = 3;
  guint width = 0;
  guint height = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Sink pad */
  g_string_printf (sink_caps, "video/x-raw,format=%s,width=%d,height=%d",
      "RGB", width, height);
  g_string_printf (sink_src, "videotestsrc is-live=true ! %s ! blend.sink",
      sink_caps->str);

  /* Tensor pad */
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, 320, 240);
  g_string_printf (tensor_src,
      "multifilesrc loop=true location=/dev/null ! %s ! blend.tensor",
      tensor_caps->str);

  g_string_printf (pipeline,
      "tiovxdlcolorblend data-type=%d name=blend %s %s blend.src ! fakesink",
      data_type, sink_src->str, tensor_src->str);

  test_states_change_async (pipeline->str,
      TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_resolutions_with_upscale_fail)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint data_type = 3;
  guint width = 0;
  guint height = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");
  g_autoptr (GString) src_caps = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Sink pad */
  g_string_printf (sink_caps, "video/x-raw,format=%s,width=%d,height=%d",
      "RGB", width, height);
  g_string_printf (sink_src, "videotestsrc is-live=true ! %s ! blend.sink",
      sink_caps->str);

  /* Tensor pad */
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, 320, 240);
  g_string_printf (tensor_src,
      "multifilesrc loop=true location=/dev/null ! %s ! blend.tensor",
      tensor_caps->str);

  /* Src pad */
  g_string_printf (src_caps, "video/x-raw,width=%d,height=%d",
      width + 1, height + 1);

  g_string_printf (pipeline,
      "tiovxdlcolorblend data-type=%d name=blend %s %s blend.src ! %s ! fakesink",
      data_type, sink_src->str, tensor_src->str, src_caps->str);

  test_states_change_async_fail_success (pipeline->str,
      TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_resolutions_with_downscale_fail)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint data_type = 3;
  guint width = 0;
  guint height = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");
  g_autoptr (GString) src_caps = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  /* Sink pad */
  g_string_printf (sink_caps, "video/x-raw,format=%s,width=%d,height=%d",
      "RGB", width, height);
  g_string_printf (sink_src, "videotestsrc is-live=true ! %s ! blend.sink",
      sink_caps->str);

  /* Tensor pad */
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, 320, 240);
  g_string_printf (tensor_src,
      "multifilesrc loop=true location=/dev/null ! %s ! blend.tensor",
      tensor_caps->str);

  /* Src pad */
  g_string_printf (src_caps, "video/x-raw,width=%d,height=%d",
      width - 1, height - 1);

  g_string_printf (pipeline,
      "tiovxdlcolorblend data-type=%d name=blend %s %s blend.src ! %s ! fakesink",
      data_type, sink_src->str, tensor_src->str, src_caps->str);

  test_states_change_async_fail_success (pipeline->str,
      TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("tiovxdlcolorblend");
  TCase *tc = tcase_create ("tc");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_foreach_format);
  tcase_add_test (tc, test_foreach_format_fail);

  /*
   * FIXME: Element cannot parse string type for data-type property
   */
  tcase_skip_broken_test (tc, test_foreach_data_type);

  tcase_add_test (tc, test_foreach_target);
  tcase_add_test (tc, test_num_classes);
  tcase_add_test (tc, test_resolutions);

  /*
   * FIXME: Open issue #130 - TIOVXDLColorBlend: Upscaling/Downscaling is not failing the right way
   */
  tcase_skip_broken_test (tc, test_resolutions_with_upscale_fail);
  tcase_skip_broken_test (tc, test_resolutions_with_downscale_fail);

  return suite;
}

GST_CHECK_MAIN (gst_state);
