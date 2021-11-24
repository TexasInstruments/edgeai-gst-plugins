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

#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "test_utils.h"
#include <TI/tivx.h>

#define TIOVXDLCOLORBLEND_NUM_DIMS_SUPPORTED 3
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

#define TIOVXDLCOLORBLEND_DATA_TYPE_ENUM_ARRAY_SIZE 7
static const enum vx_type_e
    tiovxdlcolorblend_data_type_enum
    [TIOVXDLCOLORBLEND_DATA_TYPE_ENUM_ARRAY_SIZE] = {
  VX_TYPE_INT8,
  VX_TYPE_UINT8,
  VX_TYPE_INT16,
  VX_TYPE_UINT16,
  VX_TYPE_INT32,
  VX_TYPE_UINT32,
  VX_TYPE_FLOAT32,
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

/* Supported pool size */
static const guint tiovxdlcolorblend_pool_size[] = { 2, 16 };

typedef struct
{
  const guint *width;
  const guint *height;
  const guint *pool_size;
  const gchar **formats;
} PadTemplateSink;

typedef struct
{
  const guint *width;
  const guint *height;
  const guint *pool_size;
  const gchar **formats;
} PadTemplateSrc;

typedef struct
{
  const guint *width;
  const guint *height;
  const guint *pool_size;
  const gchar **data_type;
  const enum vx_type_e *data_type_enum;
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
  element->sink_pad.pool_size = tiovxdlcolorblend_pool_size;

  element->tensor_pad.data_type = tiovxdlcolorblend_data_type;
  element->tensor_pad.data_type_enum = tiovxdlcolorblend_data_type_enum;
  element->tensor_pad.width = tiovxdlcolorblend_width;
  element->tensor_pad.height = tiovxdlcolorblend_height;
  element->tensor_pad.pool_size = tiovxdlcolorblend_pool_size;

  element->src_pad.formats = tiovxdlcolorblend_formats;
  element->src_pad.width = tiovxdlcolorblend_width;
  element->src_pad.height = tiovxdlcolorblend_height;
  element->src_pad.pool_size = tiovxdlcolorblend_pool_size;

  element->num_classes = tiovxdlcolorblend_num_classes;
  element->target = tiovxdlcolorblend_targets;
}

static inline const guint
gst_tiovx_dl_color_blend_get_tensor_blocksize (const guint tensor_width,
    const guint tensor_height, enum vx_type_e data_type)
{
  guint data_type_width = 0;
  guint tensor_blocksize = 0;

  data_type_width = gst_tiovx_tensor_get_tensor_bit_depth (data_type);
  tensor_blocksize = tensor_width * tensor_height * data_type_width;

  return tensor_blocksize;
}

GST_START_TEST (test_foreach_format)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint i = 0;
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;

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
    tensor_width =
        g_random_int_range (element.tensor_pad.width[0],
        element.tensor_pad.width[1]);
    tensor_height =
        g_random_int_range (element.tensor_pad.height[0],
        element.tensor_pad.height[1]);
    tensor_blocksize =
        gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
        tensor_height, data_type);
    g_string_printf (tensor_caps,
        "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
        data_type, tensor_width, tensor_height);
    g_string_printf (tensor_src,
        "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
        tensor_blocksize, tensor_caps->str);

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
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;

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
  tensor_width =
      g_random_int_range (element.tensor_pad.width[0],
      element.tensor_pad.width[1]);
  tensor_height =
      g_random_int_range (element.tensor_pad.height[0],
      element.tensor_pad.height[1]);
  tensor_blocksize =
      gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
      tensor_height, data_type);
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, tensor_width, tensor_height);
  g_string_printf (tensor_src,
      "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
      tensor_blocksize, tensor_caps->str);

  g_string_printf (pipeline,
      "tiovxdlcolorblend data-type=%d name=blend %s %s blend.src ! fakesink",
      data_type, sink_src->str, tensor_src->str);

  g_assert_true (NULL != test_create_pipeline_fail (pipeline->str));
}

GST_END_TEST;

GST_START_TEST (test_foreach_format_convertion_fail)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint i = 0;
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;

  const gchar *format = NULL;

  gst_tiovx_dl_color_blend_modeling_init (&element);

  for (i = 0; i < TIOVXDLCOLORBLEND_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) sink_caps = g_string_new ("");
    g_autoptr (GString) sink_src = g_string_new ("");
    g_autoptr (GString) tensor_caps = g_string_new ("");
    g_autoptr (GString) tensor_src = g_string_new ("");
    g_autoptr (GString) src_caps = g_string_new ("");

    /* Sink pad */
    g_string_printf (sink_caps, "video/x-raw,format=%s",
        element.sink_pad.formats[i]);
    g_string_printf (sink_src, "videotestsrc is-live=true ! %s ! blend.sink",
        sink_caps->str);

    /* Tensor pad */
    tensor_width =
        g_random_int_range (element.tensor_pad.width[0],
        element.tensor_pad.width[1]);
    tensor_height =
        g_random_int_range (element.tensor_pad.height[0],
        element.tensor_pad.height[1]);
    tensor_blocksize =
        gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
        tensor_height, data_type);
    g_string_printf (tensor_caps,
        "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
        data_type, tensor_width, tensor_height);
    g_string_printf (tensor_src,
        "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
        tensor_blocksize, tensor_caps->str);

    /* Src pad */
    /* Pick an output format that mismatches the input one */
    format =
        (TIOVXDLCOLORBLEND_FORMATS_ARRAY_SIZE - 1 ==
        i) ? element.src_pad.formats[0] : element.src_pad.formats[i + 1];

    g_string_printf (src_caps, "video/x-raw,format=%s", format);

    g_string_printf (pipeline,
        "tiovxdlcolorblend data-type=%d name=blend %s %s blend.src ! %s ! fakesink",
        data_type, sink_src->str, tensor_src->str, src_caps->str);

    test_states_change_async_fail_success (pipeline->str,
        TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_foreach_data_type)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint i = 0;

  gst_tiovx_dl_color_blend_modeling_init (&element);

  for (i = 0; i < TIOVXDLCOLORBLEND_DATA_TYPE_ENUM_ARRAY_SIZE; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) sink_caps = g_string_new ("");
    g_autoptr (GString) sink_src = g_string_new ("");
    g_autoptr (GString) tensor_caps = g_string_new ("");
    g_autoptr (GString) tensor_src = g_string_new ("");
    guint tensor_width = 0;
    guint tensor_height = 0;
    guint tensor_blocksize = 0;
    enum vx_type_e data_type = VX_TYPE_INVALID;

    /* Sink pad */
    g_string_printf (sink_caps, "video/x-raw,format=%s", "RGB");
    g_string_printf (sink_src, "videotestsrc is-live=true ! %s ! blend.sink",
        sink_caps->str);

    /* Tensor pad */
    tensor_width =
        g_random_int_range (element.tensor_pad.width[0],
        element.tensor_pad.width[1]);
    tensor_height =
        g_random_int_range (element.tensor_pad.height[0],
        element.tensor_pad.height[1]);
    data_type = element.tensor_pad.data_type_enum[i];
    tensor_blocksize =
        gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
        tensor_height, data_type);
    g_string_printf (tensor_caps,
        "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
        data_type, tensor_width, tensor_height);
    g_string_printf (tensor_src,
        "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
        tensor_blocksize, tensor_caps->str);

    g_string_printf (pipeline,
        "tiovxdlcolorblend data-type=%d name=blend %s %s blend.src ! fakesink",
        data_type, sink_src->str, tensor_src->str);

    test_states_change_async (pipeline->str,
        TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_foreach_target)
{
  TIOVXDLColorBlendModeled element = { 0 };
  guint i = 0;
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;

  gst_tiovx_dl_color_blend_modeling_init (&element);

  for (i = 0; i < TIOVXDLCOLORBLEND_TARGETS_ARRAY_SIZE; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) sink_src = g_string_new ("");
    g_autoptr (GString) tensor_caps = g_string_new ("");
    g_autoptr (GString) tensor_src = g_string_new ("");

    /* Sink pad */
    g_string_printf (sink_src, "videotestsrc is-live=true ! blend.sink");

    /* Tensor pad */
    tensor_width =
        g_random_int_range (element.tensor_pad.width[0],
        element.tensor_pad.width[1]);
    tensor_height =
        g_random_int_range (element.tensor_pad.height[0],
        element.tensor_pad.height[1]);
    tensor_blocksize =
        gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
        tensor_height, data_type);
    g_string_printf (tensor_caps,
        "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
        data_type, tensor_width, tensor_height);
    g_string_printf (tensor_src,
        "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
        tensor_blocksize, tensor_caps->str);

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
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;
  guint64 num_classes = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  /* Sink pad */
  g_string_printf (sink_src, "videotestsrc is-live=true ! blend.sink");

  /* Tensor pad */
  tensor_width =
      g_random_int_range (element.tensor_pad.width[0],
      element.tensor_pad.width[1]);
  tensor_height =
      g_random_int_range (element.tensor_pad.height[0],
      element.tensor_pad.height[1]);
  tensor_blocksize =
      gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
      tensor_height, data_type);
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, tensor_width, tensor_height);
  g_string_printf (tensor_src,
      "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
      tensor_blocksize, tensor_caps->str);

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
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;
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
  tensor_width =
      g_random_int_range (element.tensor_pad.width[0],
      element.tensor_pad.width[1]);
  tensor_height =
      g_random_int_range (element.tensor_pad.height[0],
      element.tensor_pad.height[1]);
  tensor_blocksize =
      gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
      tensor_height, data_type);
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, tensor_width, tensor_height);
  g_string_printf (tensor_src,
      "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
      tensor_blocksize, tensor_caps->str);

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
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;
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
  tensor_width =
      g_random_int_range (element.tensor_pad.width[0],
      element.tensor_pad.width[1]);
  tensor_height =
      g_random_int_range (element.tensor_pad.height[0],
      element.tensor_pad.height[1]);
  tensor_blocksize =
      gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
      tensor_height, data_type);
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, tensor_width, tensor_height);
  g_string_printf (tensor_src,
      "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
      tensor_blocksize, tensor_caps->str);

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
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;
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
  tensor_width =
      g_random_int_range (element.tensor_pad.width[0],
      element.tensor_pad.width[1]);
  tensor_height =
      g_random_int_range (element.tensor_pad.height[0],
      element.tensor_pad.height[1]);
  tensor_blocksize =
      gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
      tensor_height, data_type);
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, tensor_width, tensor_height);
  g_string_printf (tensor_src,
      "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
      tensor_blocksize, tensor_caps->str);

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

GST_START_TEST (test_sink_pool_size)
{
  TIOVXDLColorBlendModeled element = { 0 };
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;
  guint pool_size = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");
  g_autoptr (GString) src_caps = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  pool_size =
      g_random_int_range (element.sink_pad.pool_size[0],
      element.sink_pad.pool_size[1]);

  /* Sink pad */
  g_string_printf (sink_src, "videotestsrc is-live=true ! blend.sink");

  /* Tensor pad */
  tensor_width =
      g_random_int_range (element.tensor_pad.width[0],
      element.tensor_pad.width[1]);
  tensor_height =
      g_random_int_range (element.tensor_pad.height[0],
      element.tensor_pad.height[1]);
  tensor_blocksize =
      gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
      tensor_height, data_type);
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, tensor_width, tensor_height);
  g_string_printf (tensor_src,
      "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
      tensor_blocksize, tensor_caps->str);

  g_string_printf (pipeline,
      "tiovxdlcolorblend sink_0::pool-size=%d data-type=%d name=blend %s %s blend.src ! fakesink",
      pool_size, data_type, sink_src->str, tensor_src->str);

  test_states_change_async (pipeline->str,
      TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_src_pool_size)
{
  TIOVXDLColorBlendModeled element = { 0 };
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;
  guint pool_size = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  pool_size =
      g_random_int_range (element.src_pad.pool_size[0],
      element.src_pad.pool_size[1]);

  /* Sink pad */
  g_string_printf (sink_src, "videotestsrc is-live=true ! blend.sink");

  /* Tensor pad */
  tensor_width =
      g_random_int_range (element.tensor_pad.width[0],
      element.tensor_pad.width[1]);
  tensor_height =
      g_random_int_range (element.tensor_pad.height[0],
      element.tensor_pad.height[1]);
  tensor_blocksize =
      gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
      tensor_height, data_type);
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, tensor_width, tensor_height);
  g_string_printf (tensor_src,
      "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
      tensor_blocksize, tensor_caps->str);

  g_string_printf (pipeline,
      "tiovxdlcolorblend src::pool-size=%d data-type=%d name=blend %s %s blend.src ! fakesink",
      pool_size, data_type, sink_src->str, tensor_src->str);

  test_states_change_async (pipeline->str,
      TIOVXDLCOLORBLEND_STATES_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_tensor_pool_size)
{
  TIOVXDLColorBlendModeled element = { 0 };
  enum vx_type_e data_type = VX_TYPE_UINT8;
  guint tensor_width = 0;
  guint tensor_height = 0;
  guint tensor_blocksize = 0;
  guint pool_size = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) tensor_caps = g_string_new ("");
  g_autoptr (GString) tensor_src = g_string_new ("");

  gst_tiovx_dl_color_blend_modeling_init (&element);

  pool_size =
      g_random_int_range (element.tensor_pad.pool_size[0],
      element.tensor_pad.pool_size[1]);

  /* Sink pad */
  g_string_printf (sink_src, "videotestsrc is-live=true ! blend.sink");

  /* Tensor pad */
  tensor_width =
      g_random_int_range (element.tensor_pad.width[0],
      element.tensor_pad.width[1]);
  tensor_height =
      g_random_int_range (element.tensor_pad.height[0],
      element.tensor_pad.height[1]);
  tensor_blocksize =
      gst_tiovx_dl_color_blend_get_tensor_blocksize (tensor_width,
      tensor_height, data_type);
  g_string_printf (tensor_caps,
      "application/x-tensor-tiovx,data-type=%d,tensor-width=%d,tensor-height=%d",
      data_type, tensor_width, tensor_height);
  g_string_printf (tensor_src,
      "filesrc location=/dev/zero blocksize=%d ! %s ! blend.tensor",
      tensor_blocksize, tensor_caps->str);

  g_string_printf (pipeline,
      "tiovxdlcolorblend tensor::pool-size=%d data-type=%d name=blend %s %s blend.src ! fakesink",
      pool_size, data_type, sink_src->str, tensor_src->str);

  test_states_change_async (pipeline->str,
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
  tcase_add_test (tc, test_foreach_data_type);
  tcase_add_test (tc, test_foreach_target);

  /*
   * FIXME: Number of classes in mask test in combination with other tests halts the board.
   */
  tcase_skip_broken_test (tc, test_num_classes);

  tcase_add_test (tc, test_resolutions);
  tcase_add_test (tc, test_resolutions_with_upscale_fail);
  tcase_add_test (tc, test_resolutions_with_downscale_fail);
  tcase_add_test (tc, test_foreach_format_convertion_fail);
  tcase_add_test (tc, test_sink_pool_size);
  tcase_add_test (tc, test_src_pool_size);
  tcase_add_test (tc, test_tensor_pool_size);

  return suite;
}

GST_CHECK_MAIN (gst_state);
