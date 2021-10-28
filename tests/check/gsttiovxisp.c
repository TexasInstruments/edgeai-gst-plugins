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

#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "test_utils.h"

#define TIOVXISP_STATE_CHANGE_ITERATIONS 1
#define DCC_FILE "/opt/imaging/imx390/dcc_viss_wdr.bin"
#define TIOVXISP_NUM_DIMS_SUPPORTED 3

/* Supported formats */
#define TIOVXISP_INPUT_FORMATS_ARRAY_SIZE 8
static const gchar *tiovxisp_input_formats[TIOVXISP_INPUT_FORMATS_ARRAY_SIZE] = {
  "bggr",
  "gbrg",
  "grbg",
  "rggb",
  "bggr16",
  "gbrg16",
  "grbg16",
  "rggb16",
};

#define TIOVXISP_OUTPUT_FORMATS_ARRAY_SIZE 1
static const gchar *tiovxisp_output_formats[TIOVXISP_OUTPUT_FORMATS_ARRAY_SIZE]
    = {
  "NV12",
};

/* Supported resolutions */
static const guint tiovxisp_width[] = { 1, 8192 };
static const guint tiovxisp_height[] = { 1, 8192 };

/* Supported pool size */
static const guint tiovxisp_pool_size[] = { 2, 16 };

/* Supported DCC file */
#define TIOVXISP_DCC_FILE_ARRAY_SIZE 2
static const gchar *tiovxisp_dcc_file[TIOVXISP_DCC_FILE_ARRAY_SIZE] = {
  NULL,
  DCC_FILE,
};

/* Supported MSB bit that has data */
static const guint tiovxisp_format_msb[] = { 1, 16 };

/* Supported lines interleaved */
static const gboolean tiovxisp_lines_interleaved[] = { FALSE, TRUE };

/* Supported meta height after */
static const guint tiovxisp_meta_height_after[] = { 0, 8192 };

/* Supported meta height before */
static const guint tiovxisp_meta_height_before[] = { 0, 8192 };

/* Supported number of exposures */
static const guint tiovxisp_num_exposures[] = { 1, 4 };

/* Supported sensor IDs */
#define TIOVXISP_SENSOR_ID_ARRAY_SIZE 1
static const gchar *tiovxisp_sensor_id[TIOVXISP_SENSOR_ID_ARRAY_SIZE] = {
  "SENSOR_SONY_IMX390_UB953_D3",
};

/* Supported targets */
#define TIOVXISP_TARGET_ARRAY_SIZE 1
static const gchar *tiovxisp_target[TIOVXISP_TARGET_ARRAY_SIZE] = {
  "VPAC_VISS1",
};

typedef struct
{
  const guint *width;
  const guint *height;
  const guint *framerate;
  const gchar **formats;
  const guint *pool_size;
} PadTemplate;

typedef struct
{
  const gchar **dcc_file;
  const guint *format_msb;
  const gboolean *lines_interleaved;
  const guint *meta_height_after;
  const guint *meta_height_before;
  const guint *num_exposures;
  const gchar **sensor_id;
  const gchar **target;
} Properties;

typedef struct
{
  PadTemplate src_pads;
  PadTemplate sink_pad;
  Properties properties;
} TIOVXISPModeled;

static const void gst_tiovx_isp_modeling_init (TIOVXISPModeled * element);

static const void
gst_tiovx_isp_modeling_init (TIOVXISPModeled * element)
{
  element->sink_pad.formats = tiovxisp_input_formats;
  element->sink_pad.width = tiovxisp_width;
  element->sink_pad.height = tiovxisp_height;
  element->sink_pad.pool_size = tiovxisp_pool_size;

  element->src_pads.formats = tiovxisp_output_formats;
  element->src_pads.width = tiovxisp_width;
  element->src_pads.height = tiovxisp_height;
  element->src_pads.pool_size = tiovxisp_pool_size;

  element->properties.dcc_file = tiovxisp_dcc_file;
  element->properties.format_msb = tiovxisp_format_msb;
  element->properties.lines_interleaved = tiovxisp_lines_interleaved;
  element->properties.meta_height_after = tiovxisp_meta_height_after;
  element->properties.meta_height_before = tiovxisp_meta_height_before;
  element->properties.num_exposures = tiovxisp_num_exposures;
  element->properties.sensor_id = tiovxisp_sensor_id;
  element->properties.target = tiovxisp_target;
}

static inline const guint
gst_tiovx_isp_get_blocksize (const guint width,
    const guint height, const gchar * formats)
{
  guint bits_per_pixel = 0;
  guint blocksize = 0;

  bits_per_pixel = gst_tiovx_bayer_get_bits_per_pixel (formats);
  blocksize = width * height * bits_per_pixel;

  return blocksize;
}

GST_START_TEST (test_foreach_format)
{
  TIOVXISPModeled element = { 0 };
  guint i = 0;

  gst_tiovx_isp_modeling_init (&element);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) pipeline = g_string_new ("");
    g_autoptr (GString) sink_caps = g_string_new ("");
    g_autoptr (GString) sink_src = g_string_new ("");
    g_autoptr (GString) src_caps = g_string_new ("");
    guint width = 0;
    guint height = 0;
    guint blocksize = 0;

    /* Sink pad */
    width =
        g_random_int_range (element.sink_pad.width[0],
        element.sink_pad.width[1]);
    height =
        g_random_int_range (element.sink_pad.height[0],
        element.sink_pad.height[1]);
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    g_string_printf (src_caps, "video/x-raw");

    g_string_printf (pipeline,
        "%s ! tiovxisp dcc-file=/dev/zero ! %s ! fakesink", sink_src->str,
        src_caps->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_input_format_fail)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;

  gst_tiovx_isp_modeling_init (&element);

  /* Sink */
  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);
  blocksize =
      gst_tiovx_isp_get_blocksize (width, height, GST_VIDEO_FORMAT_UNKNOWN);
  g_string_printf (sink_caps, "video/x-bayer,width=%d,height=%d,format=%d",
      width, height, GST_VIDEO_FORMAT_UNKNOWN);
  g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
      blocksize, sink_caps->str);

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-file=/dev/zero ! fakesink", sink_src->str);

  g_assert_true (NULL != test_create_pipeline_fail (pipeline->str));
}

GST_END_TEST;

GST_START_TEST (test_output_format_fail)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) src_caps = g_string_new ("");
  guint width = 0;
  guint height = 0;
  const gchar *format = NULL;
  guint blocksize = 0;
  guint i = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    format = element.sink_pad.formats[i];
    blocksize = gst_tiovx_isp_get_blocksize (width, height, format);

    /* Sink */
    g_string_printf (sink_caps, "video/x-bayer,width=%d,height=%d,format=%s",
        width, height, format);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src */
    /* Add invalid output format */
    g_string_printf (src_caps, "video/x-video,format=%d",
        GST_VIDEO_FORMAT_UNKNOWN);

    g_string_printf (pipeline,
        "%s ! tiovxisp dcc-file=/dev/zero ! %s ! fakesink", sink_src->str,
        src_caps->str);

    test_create_pipeline_fail (pipeline->str);
  }
}

GST_END_TEST;

GST_START_TEST (test_resolutions_with_upscale_fail)
{
  TIOVXISPModeled element = { 0 };
  guint i = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) src_caps = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;

  gst_tiovx_isp_modeling_init (&element);

  /* Sink pad */
  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);
  blocksize =
      gst_tiovx_isp_get_blocksize (width, height, element.sink_pad.formats[i]);

  g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
      element.sink_pad.formats[i], width, height);
  g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
      blocksize, sink_caps->str);

  /* Src pad */
  g_string_printf (src_caps, "video/x-raw,width=%d,height=%d", width + 1,
      height + 1);

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-file=/dev/zero ! %s ! fakesink", sink_src->str,
      src_caps->str);

  test_create_pipeline_fail (pipeline->str);
}

GST_END_TEST;

GST_START_TEST (test_resolutions_with_downscale_fail)
{
  TIOVXISPModeled element = { 0 };
  guint i = 0;

  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) src_caps = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;

  gst_tiovx_isp_modeling_init (&element);

  /* Sink pad */
  width =
      g_random_int_range (element.sink_pad.width[0], element.sink_pad.width[1]);
  height =
      g_random_int_range (element.sink_pad.height[0],
      element.sink_pad.height[1]);
  blocksize =
      gst_tiovx_isp_get_blocksize (width, height, element.sink_pad.formats[i]);

  g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
      element.sink_pad.formats[i], width, height);
  g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
      blocksize, sink_caps->str);

  /* Src pad */
  g_string_printf (src_caps, "video/x-raw,width=%d,height=%d", width - 1,
      height - 1);

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-file=/dev/zero ! %s ! fakesink", sink_src->str,
      src_caps->str);

  test_create_pipeline_fail (pipeline->str);
}

GST_END_TEST;

static Suite *
gst_tiovx_isp_suite (void)
{
  Suite *suite = suite_create ("tiovxisp");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);

  /*
   * Open issue #135. Bayer formats with BPP=1 aren't working properly
   */
  tcase_skip_broken_test (tc, test_foreach_format);
  tcase_add_test (tc, test_input_format_fail);
  tcase_add_test (tc, test_output_format_fail);
  tcase_add_test (tc, test_resolutions_with_upscale_fail);
  tcase_add_test (tc, test_resolutions_with_downscale_fail);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_isp);
