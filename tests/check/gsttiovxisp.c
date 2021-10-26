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

#include "test_utils.h"

#define TIOVXISP_STATE_CHANGE_ITERATIONS 5
#define DCC_FILE "/opt/imaging/imx390/dcc_viss_wdr.bin"

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

GST_START_TEST (test_dummy)
{
  TIOVXISPModeled element = { 0 };

  gst_tiovx_isp_modeling_init (&element);
}

GST_END_TEST;

static Suite *
gst_tiovx_isp_suite (void)
{
  Suite *suite = suite_create ("tiovxisp");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_dummy);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_isp);
