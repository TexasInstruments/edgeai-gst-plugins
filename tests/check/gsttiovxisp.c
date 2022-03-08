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

#define DCC_IMX219_ID (219)
#define DCC_IMX219_ISP_FILE "/opt/imaging/imx219/dcc_viss.bin"
#define DCC_IMX219_2A_FILE "/opt/imaging/imx219/dcc_2a.bin"

#define TIOVXISP_NUM_DIMS_SUPPORTED 3
/* TIOVXISP element can only support one output for the moment. */
#define TIOVXISP_MAX_SUPPORTED_PADS 1

static const gint default_num_exposures = 1;

typedef struct
{
  const guint min;
  const guint max;
} Range;

/* Supported formats */
#define TIOVXISP_INPUT_FORMATS_ARRAY_SIZE 16
static const gchar *tiovxisp_input_formats[TIOVXISP_INPUT_FORMATS_ARRAY_SIZE] = {
  "bggr",
  "gbrg",
  "grbg",
  "rggb",
  "bggr10",
  "gbrg10",
  "grbg10",
  "rggb10",
  "bggr12",
  "gbrg12",
  "grbg12",
  "rggb12",
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

/* DCC IMX219 */
static const DCC tiovxisp_dcc_imx219 =
    { DCC_IMX219_ID, DCC_IMX219_ISP_FILE, DCC_IMX219_2A_FILE };

#define TIOVXISP_DCC_ARRAY_SIZE 1
static const DCC *tiovxisp_dcc[TIOVXISP_DCC_ARRAY_SIZE] = {
  &tiovxisp_dcc_imx219,
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
/*
 * FIXME: Open issue #144. Number of exposures of four, fails as an invalid number.
 * This range should go from 1-4.
 */
static const Range tiovxisp_num_exposures = { 1, 3 };

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
  element->properties.target = tiovxisp_target;
}

static inline const guint
gst_tiovx_isp_get_blocksize (const guint width,
    const guint height, const gchar * formats, const gint num_exposures)
{
  guint bits_per_pixel = 0;
  guint blocksize = 0;

  bits_per_pixel = gst_tiovx_bayer_get_bits_per_pixel (formats);
  blocksize = width * height * bits_per_pixel * num_exposures;

  return blocksize;
}

/*
 * FIXME: Needed it due to:
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

/* Get the maximum possible MSB for a given format */
static inline const guint
gst_tiovx_isp_get_format_msb (const gchar * formats)
{
  guint format_msb = 0;
  enum tivx_raw_image_pixel_container_e tivx_raw_format = -1;

  g_return_val_if_fail (formats, 0);

  tivx_raw_format = gst_format_to_tivx_raw_format (formats);
  switch (tivx_raw_format) {
    case TIVX_RAW_IMAGE_8_BIT:
      format_msb = 8;
      break;
    case TIVX_RAW_IMAGE_16_BIT:
      format_msb = 16;
      break;
    default:
      format_msb = 0;
      break;
  }

  return format_msb;
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

    /* Sink pad */
    width =
        gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.
        width_range->min, element.sink_pad.width_range->max);
    height =
        gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.
        height_range->min, element.sink_pad.height_range->max);
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height, element.sink_pad.formats[i],
        default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s ! %s ! fakesink",
        sink_src->str, dcc_2a, src_caps->str);

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
  gint dcc_random = 0;
  const gchar *dcc_2a = NULL;

  gst_tiovx_isp_modeling_init (&element);

  /* Sink */
  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.height_range->
      min, element.sink_pad.height_range->max);
  /* Add invalid input format */
  g_string_printf (sink_caps, "video/x-bayer,width=%d,height=%d,format=%d",
      width, height, GST_VIDEO_FORMAT_UNKNOWN);
  g_string_printf (sink_src, "filesrc location=/dev/zero ! %s", sink_caps->str);

  /* Properties */
  /* Pick one DCC input for every pipeline; DCC 2A cannot be mocked */
  dcc_random = g_random_int_range (0, TIOVXISP_DCC_ARRAY_SIZE);
  dcc_2a = element.properties.dcc[dcc_random]->dcc_2a;

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s ! fakesink",
      sink_src->str, dcc_2a);

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

    format = element.sink_pad.formats[i];
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height, format,
        default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s ! %s ! fakesink",
        sink_src->str, dcc_2a, src_caps->str);

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

  gst_tiovx_isp_modeling_init (&element);

  /* Sink pad */
  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.
      height_range->min, element.sink_pad.height_range->max);
  blocksize =
      gst_tiovx_isp_get_blocksize (width, height, element.sink_pad.formats[i],
      default_num_exposures);

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

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s ! %s ! fakesink",
      sink_src->str, dcc_2a, src_caps->str);

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

  gst_tiovx_isp_modeling_init (&element);

  /* Sink pad */
  width =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.width_range->min,
      element.sink_pad.width_range->max);
  height =
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.
      height_range->min, element.sink_pad.height_range->max);
  blocksize =
      gst_tiovx_isp_get_blocksize (width, height, element.sink_pad.formats[i],
      default_num_exposures);

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

  g_string_printf (pipeline,
      "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s ! %s ! fakesink",
      sink_src->str, dcc_2a, src_caps->str);

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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s sink::pool-size=%d ! %s ! fakesink",
        sink_src->str, dcc_2a, pool_size, src_caps->str);

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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s %s %s",
        sink_src->str, dcc_2a, src_pad->str, src_src->str);

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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    for (k = FALSE; k <= TRUE; k++) {
      ae_disabled = k;

      g_string_printf (pipeline,
          "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s ae-disabled=%d %s",
          sink_src->str, dcc_2a, ae_disabled, src_src->str);

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

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    const gchar *format = NULL;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);
    format = element.sink_pad.formats[i];
    g_string_printf (sink_caps, "video/x-bayer,format=%s,width=%d,height=%d",
        format, width, height);
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

    format_msb =
        g_random_int_range (element.properties.format_msb_range->min,
        gst_tiovx_isp_get_format_msb (format));

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s format-msb=%d %s",
        sink_src->str, dcc_2a, format_msb, src_src->str);

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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s ae-num-skip-frames=%d %s",
        sink_src->str, dcc_2a, ae_num_skip_frames, src_src->str);

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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s analog-gain=%d %s",
        sink_src->str, dcc_2a, analog_gain, src_src->str);

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

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    gboolean k = 0;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    for (k = FALSE; k <= TRUE; k++) {
      awb_disabled = k;
      g_string_printf (pipeline,
          "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s awb-disabled=%d %s",
          sink_src->str, dcc_2a, awb_disabled, src_src->str);

      test_states_change_async (pipeline->str,
          TIOVXISP_STATE_CHANGE_ITERATIONS);
    }
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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s awb-disabled=false awb-num-skip-frames=%d %s",
        sink_src->str, dcc_2a, awb_num_skip_frames, src_src->str);

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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s color-temperature=%u %s",
        sink_src->str, dcc_2a, color_temperature, src_src->str);

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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s exposure-time=%u %s",
        sink_src->str, dcc_2a, exposure_time, src_src->str);

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

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;
    gboolean k = FALSE;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    for (k = FALSE; k <= TRUE; k++) {
      lines_interleaved = k;
      g_string_printf (pipeline,
          "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s lines-interleaved=%d %s",
          sink_src->str, dcc_2a, lines_interleaved, src_src->str);

      test_states_change_async (pipeline->str,
          TIOVXISP_STATE_CHANGE_ITERATIONS);
    }
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
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.
      height_range->min, element.sink_pad.height_range->max);

  /* Properties */
  meta_height_after =
      g_random_int_range (element.properties.meta_height_after_range->min,
      height);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s meta-height-after=%d %s",
        sink_src->str, dcc_2a, meta_height_after, src_src->str);

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
      gst_tiovx_isp_get_int_range_pair_value (element.sink_pad.
      height_range->min, element.sink_pad.height_range->max);

  /* Properties */
  meta_height_before =
      g_random_int_range (element.properties.meta_height_before_range->min,
      height);

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s meta-height-before=%d %s",
        sink_src->str, dcc_2a, meta_height_before, src_src->str);

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

  for (i = 0; i < TIOVXISP_INPUT_FORMATS_ARRAY_SIZE; i++) {
    g_autoptr (GString) src_src = g_string_new ("");
    gint dcc_random = 0;
    const gchar *dcc_2a = NULL;

    /* Properties */
    num_exposures =
        g_random_int_range (element.properties.num_exposures_range->min,
        element.properties.num_exposures_range->max + 1);

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], num_exposures);

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

    g_string_printf (pipeline,
        "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s num-exposures=%d %s",
        sink_src->str, dcc_2a, num_exposures, src_src->str);

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

    /* Sink pad */
    blocksize =
        gst_tiovx_isp_get_blocksize (width, height,
        element.sink_pad.formats[i], default_num_exposures);

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

    for (k = 0; k < TIOVXISP_TARGET_ARRAY_SIZE; k++) {
      const gchar *target = 0;

      target = element.properties.target[k];
      g_string_printf (pipeline,
          "%s ! tiovxisp name=tiovxisp dcc-isp-file=/dev/zero dcc-2a-file=%s target=%s %s",
          sink_src->str, dcc_2a, target, src_src->str);

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
  /*
   * FIXME: Open issue #149. Certain format-msb values will give SIGSEGV
   */
  tcase_skip_broken_test (tc, test_format_msb);
  /*
   * FIXME: This test halts the board.
   */
  tcase_skip_broken_test (tc, test_ae_num_skip_frames);
  tcase_skip_broken_test (tc, test_ae_disabled);
  tcase_add_test (tc, test_analog_gain);

  /*
   * FIXME: This test halts the board.
   */
  tcase_skip_broken_test (tc, test_awb_num_skip_frames);
  tcase_skip_broken_test (tc, test_awb_disabled);
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
