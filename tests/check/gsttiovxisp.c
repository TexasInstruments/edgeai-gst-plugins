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

#define DCC_IMX390_ID (390)
#define DCC_IMX390_ISP_FILE "/opt/imaging/imx390/dcc_viss.bin"
#define DCC_IMX390_2A_FILE "/opt/imaging/imx390/dcc_2a.bin"

#define TIOVXISP_NUM_DIMS_SUPPORTED 3
/* TIOVXISP element can only support one output for the moment. */
#define TIOVXISP_MAX_SUPPORTED_PADS 1

typedef struct
{
  const guint min;
  const guint max;
} Range;

/* Supported formats */
#define TIOVXISP_INPUT_FORMATS_ARRAY_SIZE 2
static const gchar *tiovxisp_input_formats[TIOVXISP_INPUT_FORMATS_ARRAY_SIZE] = {
  "bggr",
  "gbrg",
/* FIXME: These formats halts the board.
  "grbg",
  "rggb",
  "bggr16",
  "gbrg16",
  "grbg16",
  "rggb16",
  */
};

#define TIOVXISP_OUTPUT_FORMATS_ARRAY_SIZE 1
static const gchar *tiovxisp_output_formats[TIOVXISP_OUTPUT_FORMATS_ARRAY_SIZE]
    = {
  "NV12",
};

/* Supported resolutions.
 * FIXME: TIOVX nodes doesn't support high input resolution for the moment.
 * Open issue #115. High input resolutions fails allocating TIVX memory.
*/
static const Range tiovxisp_width = { 1, 2048 };
static const Range tiovxisp_height = { 1, 1080 };

/* Supported pool size */
static const Range tiovxisp_pool_size = { 2, 16 };

/* Supported DCC files */
typedef struct
{
  const guint id;
  const gchar *isp;
  const gchar *dcc_2a;
} DCC;

/* DCC IMX390 */
static const DCC tiovxisp_dcc_imx390 =
    { DCC_IMX390_ID, DCC_IMX390_ISP_FILE, DCC_IMX390_2A_FILE };

#define TIOVXISP_DCC_ARRAY_SIZE 1
static const DCC *tiovxisp_dcc[TIOVXISP_DCC_ARRAY_SIZE] = {
  &tiovxisp_dcc_imx390,
};

/* Supported AE num of skip frames */
static const Range tiovxisp_ae_num_skip_frames = { 0, 4294967295 };

/* Supported analog gain */
static const Range tiovxisp_analog_gain = { 0, 4294967295 };

/* Supported AWB num of skip frames */
static const Range tiovxisp_awb_num_skip_frames = { 0, 4294967295 };

/* Supported AWB num of skip frames */
static const Range tiovxisp_color_temperature = { 0, 4294967295 };

/* Supported exposure time */
static const Range tiovxisp_exposure_time = { 0, 4294967295 };

/* Supported MSB bit that has data */
static const Range tiovxisp_format_msb = { 1, 16 };

/* Supported lines interleaved */
static const gboolean tiovxisp_lines_interleaved[] = { FALSE, TRUE };

/* Supported meta height after */
static const Range tiovxisp_meta_height_after = { 0, 8192 };

/* Supported meta height before */
static const Range tiovxisp_meta_height_before = { 0, 8192 };

/* Supported number of exposures */
static const Range tiovxisp_num_exposures = { 1, 4 };

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
  const Range *width_range;
  const Range *height_range;
  const guint *framerate;
  const gchar **formats;
  const Range *pool_size_range;
} PadTemplate;

typedef struct
{
  const DCC **dcc;
  const Range *ae_num_skip_frames_range;
  const Range *analog_gain_range;
  const Range *awb_num_skip_frames_range;
  const Range *color_temperature_range;
  const Range *exposure_time_range;
  const Range *format_msb_range;
  const gboolean *lines_interleaved;
  const Range *meta_height_after_range;
  const Range *meta_height_before_range;
  const Range *num_exposures_range;
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
  element->sink_pad.width_range = &tiovxisp_width;
  element->sink_pad.height_range = &tiovxisp_height;
  element->sink_pad.pool_size_range = &tiovxisp_pool_size;

  element->src_pads.formats = tiovxisp_output_formats;
  element->src_pads.width_range = &tiovxisp_width;
  element->src_pads.height_range = &tiovxisp_height;
  element->src_pads.pool_size_range = &tiovxisp_pool_size;

  element->properties.dcc = tiovxisp_dcc;
  element->properties.ae_num_skip_frames_range = &tiovxisp_ae_num_skip_frames;
  element->properties.analog_gain_range = &tiovxisp_analog_gain;
  element->properties.awb_num_skip_frames_range = &tiovxisp_awb_num_skip_frames;
  element->properties.color_temperature_range = &tiovxisp_color_temperature;
  element->properties.exposure_time_range = &tiovxisp_exposure_time;
  element->properties.format_msb_range = &tiovxisp_format_msb;
  element->properties.lines_interleaved = tiovxisp_lines_interleaved;
  element->properties.meta_height_after_range = &tiovxisp_meta_height_after;
  element->properties.meta_height_before_range = &tiovxisp_meta_height_before;
  element->properties.num_exposures_range = &tiovxisp_num_exposures;
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

/*
 * FIXME: Need it due to:
 * Open issue #123. Odd resolution values will get VX_ERROR_INVALID_DIMENSION error for NV12 format.
 */
static inline const gint
gst_tiovx_isp_get_int_range_pair_value (gint begin, gint end)
{
  gint value = 0;

  /* Residue equals to 1 means odd value */
  do {
    value = g_random_int_range (begin, end);
  } while ((1 == value % 2));

  return value;
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
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    width =
        gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->
        min, element.sink_pad.width_range->max);
    height =
        gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
        min, element.sink_pad.height_range->max);
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    g_string_printf (src_caps, "video/x-raw");

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sensor-dcc-id=%d ! %s ! fakesink",
        sink_src->str, dcc_2a, dcc_id, src_caps->str);

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
  gint dcc_random = 0;
  const gchar *dcc_2a = NULL;
  guint dcc_id = 0;

  gst_tiovx_isp_modeling_init (&element);

  /* Sink */
  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);
  blocksize =
      gst_tiovx_isp_get_blocksize (width, height, GST_VIDEO_FORMAT_UNKNOWN);
  /* Add invalid input format */
  g_string_printf (sink_caps, "video/x-bayer,width=%d,height=%d,format=%d",
      width, height, GST_VIDEO_FORMAT_UNKNOWN);
  g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
      blocksize, sink_caps->str);

  /* Properties */
  /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
  dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
  dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
  dcc_id = element.properties.dcc[dcc_random]->id;

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sensor-dcc-id=%d ! fakesink",
      sink_src->str, dcc_2a, dcc_id);

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
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

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

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sensor-dcc-id=%d ! %s ! fakesink",
        sink_src->str, dcc_2a, dcc_id, src_caps->str);

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
  gint dcc_random = 0;
  const gchar *dcc_2a = NULL;
  guint dcc_id = 0;

  gst_tiovx_isp_modeling_init (&element);

  /* Sink pad */
  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);
  blocksize =
      gst_tiovx_isp_get_blocksize (width, height, element.sink_pad.formats[i]);

  g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
      element.sink_pad.formats[i], width, height);
  g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
      blocksize, sink_caps->str);

  /* Src pad */
  /* Add upscaled output attempt */
  g_string_printf (src_caps, "video/x-raw,width=%d,height=%d", width + 1,
      height + 1);

  /* Properties */
  /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
  dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
  dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
  dcc_id = element.properties.dcc[dcc_random]->id;

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sensor-dcc-id=%d ! %s ! fakesink",
      sink_src->str, dcc_2a, dcc_id, src_caps->str);

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
  gint dcc_random = 0;
  const gchar *dcc_2a = NULL;
  guint dcc_id = 0;

  gst_tiovx_isp_modeling_init (&element);

  /* Sink pad */
  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);
  blocksize =
      gst_tiovx_isp_get_blocksize (width, height, element.sink_pad.formats[i]);

  g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
      element.sink_pad.formats[i], width, height);
  g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
      blocksize, sink_caps->str);

  /* Src pad */
  /* Add downscaled output attempt */
  g_string_printf (src_caps, "video/x-raw,width=%d,height=%d", width - 1,
      height - 1);

  /* Properties */
  /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
  dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
  dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
  dcc_id = element.properties.dcc[dcc_random]->id;

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sensor-dcc-id=%d ! %s ! fakesink",
      sink_src->str, dcc_2a, dcc_id, src_caps->str);

  test_create_pipeline_fail (pipeline->str);
}

GST_END_TEST;

GST_START_TEST (test_sink_pool_size)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  g_autoptr (GString) src_caps = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint pool_size = 0;
  guint i = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  pool_size =
      g_random_int_range (element.sink_pad.pool_size_range->min,
      element.sink_pad.pool_size_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    g_string_printf (src_caps, "video/x-raw,width=%d,height=%d", width, height);

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sensor-dcc-id=%d sink::pool-size=%d ! %s ! fakesink",
        sink_src->str, dcc_2a, dcc_id, pool_size, src_caps->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_src_pool_size)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint pool_size = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_pad = g_string_new ("");
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      /* Properties */
      pool_size =
          g_random_int_range (element.src_pads.pool_size_range->min,
          element.src_pads.pool_size_range->max);

      g_string_append_printf (src_pad, "src_%d::pool-size=%d ", j, pool_size);
      g_string_append_printf (src_src, "tiovxisp. ! fakesink ");
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sensor-dcc-id=%d %s %s",
        sink_src->str, dcc_2a, dcc_id, src_pad->str, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_ae_disabled)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint ae_disabled = 0;
  guint i = 0;
  guint j = 0;
  gboolean k = FALSE;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    for (k = FALSE; k <= TRUE; k++) {
      ae_disabled = k;

      g_string_printf (pipeline,
          "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sensor-dcc-id=%d ae-disabled=%d %s",
          sink_src->str, dcc_2a, dcc_id, ae_disabled, src_src->str);

      test_states_change_async (pipeline->str,
          TIOVXISP_STATE_CHANGE_ITERATIONS);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_format_msb)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint format_msb = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  format_msb =
      g_random_int_range (element.properties.format_msb_range->min,
      element.properties.format_msb_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d format-msb=%d %s",
        sink_src->str, dcc_2a, dcc_id, format_msb, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_ae_num_skip_frames)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint ae_num_skip_frames = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  ae_num_skip_frames =
      g_random_double_range (element.properties.ae_num_skip_frames_range->min,
      element.properties.ae_num_skip_frames_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d ae-num-skip-frames=%d %s",
        sink_src->str, dcc_2a, dcc_id, ae_num_skip_frames, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_analog_gain)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint analog_gain = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  analog_gain =
      g_random_double_range (element.properties.analog_gain_range->min,
      element.properties.analog_gain_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d analog-gain=%d %s",
        sink_src->str, dcc_2a, dcc_id, analog_gain, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_awb_disabled)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint awb_disabled = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  awb_disabled = g_random_boolean ();

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d awb-disabled=%d %s",
        sink_src->str, dcc_2a, dcc_id, awb_disabled, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_awb_num_skip_frames)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint awb_num_skip_frames = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  awb_num_skip_frames =
      g_random_double_range (element.properties.awb_num_skip_frames_range->min,
      element.properties.awb_num_skip_frames_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d awb-disabled=false awb-num-skip-frames=%d %s",
        sink_src->str, dcc_2a, dcc_id, awb_num_skip_frames, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_color_temperature)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint color_temperature = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  color_temperature =
      g_random_double_range (element.properties.color_temperature_range->min,
      element.properties.color_temperature_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d color-temperature=%u %s",
        sink_src->str, dcc_2a, dcc_id, color_temperature, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_exposure_time)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint exposure_time = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  exposure_time =
      g_random_double_range (element.properties.exposure_time_range->min,
      element.properties.exposure_time_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d exposure-time=%u %s",
        sink_src->str, dcc_2a, dcc_id, exposure_time, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_lines_interleaved)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  gboolean lines_interleaved = FALSE;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  lines_interleaved = g_random_boolean ();

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d lines-interleaved=%d %s",
        sink_src->str, dcc_2a, dcc_id, lines_interleaved, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_meta_height_after)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  gint meta_height_after = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  meta_height_after =
      g_random_int_range (element.properties.meta_height_after_range->min,
      element.properties.meta_height_after_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d meta-height-after=%d %s",
        sink_src->str, dcc_2a, dcc_id, meta_height_after, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_meta_height_before)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  gint meta_height_before = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  meta_height_before =
      g_random_int_range (element.properties.meta_height_before_range->min,
      element.properties.meta_height_before_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d meta-height-before=%d %s",
        sink_src->str, dcc_2a, dcc_id, meta_height_before, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_num_exposures)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  gint num_exposures = 0;
  guint i = 0;
  guint j = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  /* Properties */
  num_exposures =
      g_random_int_range (element.properties.num_exposures_range->min,
      element.properties.num_exposures_range->max + 1);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d num-exposures=%d %s",
        sink_src->str, dcc_2a, dcc_id, num_exposures, src_src->str);

    test_states_change_async (pipeline->str, TIOVXISP_STATE_CHANGE_ITERATIONS);
  }
}

GST_END_TEST;

GST_START_TEST (test_target)
{
  TIOVXISPModeled element = { 0 };
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) sink_caps = g_string_new ("");
  g_autoptr (GString) sink_src = g_string_new ("");
  guint width = 0;
  guint height = 0;
  guint blocksize = 0;
  guint i = 0;
  guint j = 0;
  guint k = 0;

  gst_tiovx_isp_modeling_init (&element);

  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    guint dcc_id = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i]);

    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        element.sink_pad.formats[i], width, height);
    g_string_printf (sink_src, "filesrc location=/dev/zero blocksize=%d ! %s",
        blocksize, sink_caps->str);

    /* Src pad */
    /* Create multiple outputs */
    for (j = 0; j < g_random_int_range (1, TIOVXISP_MAX_SUPPORTED_PADS + 1);
        j++) {
      g_string_append_printf (src_src, "tiovxisp.src_%d ! fakesink ", j);
    }

    /* Properties */
    /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
    dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
    dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;
    dcc_id = element.properties.dcc[dcc_random]->id;

    for (k = 0; k < TIOVXISP_TARGET_ARRAY_SIZE; k++) {
      const gchar *target = 0;

      target = element.properties.target[k];
      g_string_printf (pipeline,
          "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s  sensor-dcc-id=%d target=%s %s",
          sink_src->str, dcc_2a, dcc_id, target, src_src->str);

      g_print ("pipeline => %s", pipeline->str);
      test_states_change_async (pipeline->str,
          TIOVXISP_STATE_CHANGE_ITERATIONS);
    }
  }
}

GST_END_TEST;

static Suite *
gst_tiovx_isp_suite (void)
{
  Suite *suite = suite_create ("tiovxisp");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);

  tcase_add_test (tc, test_foreach_format);
  tcase_add_test (tc, test_input_format_fail);
  tcase_add_test (tc, test_output_format_fail);
  tcase_add_test (tc, test_resolutions_with_upscale_fail);
  tcase_add_test (tc, test_resolutions_with_downscale_fail);
  tcase_add_test (tc, test_sink_pool_size);
  tcase_add_test (tc, test_src_pool_size);

  /* Properties */
  tcase_add_test (tc, test_ae_disabled);
  tcase_add_test (tc, test_format_msb);
  tcase_add_test (tc, test_ae_num_skip_frames);
  tcase_add_test (tc, test_analog_gain);
  tcase_add_test (tc, test_awb_disabled);
  tcase_add_test (tc, test_awb_num_skip_frames);
  tcase_add_test (tc, test_color_temperature);
  tcase_add_test (tc, test_exposure_time);
  tcase_add_test (tc, test_lines_interleaved);
  tcase_add_test (tc, test_meta_height_after);
  tcase_add_test (tc, test_meta_height_before);
  /*
   * FIXME: This test halts the board.
   */
  tcase_skip_broken_test (tc, test_num_exposures);
  tcase_add_test (tc, test_target);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_isp);
