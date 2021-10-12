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

#define MAX_PIPELINE_SIZE 300
#define VALID_FORMATS 4
#define INVALID_FORMATS 5
#define DCC_FILE "/opt/imaging/imx390/dcc_ldc"
#define SENSOR "SENSOR_SONY_IMX390_UB953_D3"
#define RESOLUTIONS 15
#define MAX_RESOLUTION 8192
/* This is required due to an issue in the modules for smaller resolutions */
#define MIN_RESOLUTION 200
#define DEFAULT_STATE_CHANGES 1

static const int default_image_width = 1920;
static const int default_image_height = 1080;

static const gchar *gst_valid_formats[] = {
  "GRAY8",
  "GRAY16_LE",
  "NV12",
  "UYVY",
};

static const gchar *gst_invalid_formats[] = {
  "RGB",
  "RGBx",
  "NV21",
  "YUY2",
  "I420",
};

GST_START_TEST (test_formats)
{
  gchar pipeline[MAX_PIPELINE_SIZE] = "";
  gint format = 0;

  /* Test valid formats */
  for (format = 0; format < VALID_FORMATS; format++) {
    g_snprintf (pipeline, MAX_PIPELINE_SIZE,
        "videotestsrc is-live=true ! video/x-raw,format=%s,width=%d,height=%d ! tiovxldc in-pool-size=4 out-pool-size=4 dcc-file=%s sensor-name=%s ! fakesink async=false",
        gst_valid_formats[format], default_image_width,
        default_image_height, DCC_FILE, SENSOR);
    test_states_change_success (pipeline, DEFAULT_STATE_CHANGES);
  }

  /* Test invalid formats */
  for (format = 0; format < INVALID_FORMATS; format++) {
    g_snprintf (pipeline, MAX_PIPELINE_SIZE,
        "videotestsrc is-live=true ! video/x-raw,format=%s,width=%d,height=%d ! tiovxldc in-pool-size=4 out-pool-size=4 dcc-file=%s sensor-name=%s ! fakesink async=false",
        gst_invalid_formats[format], default_image_width,
        default_image_height, DCC_FILE, SENSOR);
    test_create_pipeline_fail (pipeline);
  }

}

GST_END_TEST;

GST_START_TEST (test_resolutions)
{
  gchar pipeline[MAX_PIPELINE_SIZE] = "";
  gint width = 0;
  gint height = 0;
  gint resolution = 0;

  /* Test random resolutions */
  for (resolution = 0; resolution < RESOLUTIONS; resolution++) {
    width = g_random_int_range (MIN_RESOLUTION, MAX_RESOLUTION);
    height = g_random_int_range (MIN_RESOLUTION, MAX_RESOLUTION);
    g_snprintf (pipeline, MAX_PIPELINE_SIZE,
        "videotestsrc is-live=true ! video/x-raw,format=NV12,width=%d,height=%d ! tiovxldc in-pool-size=4 out-pool-size=4 dcc-file=%s sensor-name=%s ! fakesink async=false",
        width, height, DCC_FILE, SENSOR);
    g_print ("WIDTH: %d, HEIGHT: %d\n", width, height);
    test_states_change_success (pipeline, DEFAULT_STATE_CHANGES);
  }

  /* Test max resolution */
  g_snprintf (pipeline, MAX_PIPELINE_SIZE,
      "videotestsrc is-live=true ! video/x-raw,format=NV12,width=%d,height=%d ! tiovxldc in-pool-size=4 out-pool-size=4 dcc-file=%s sensor-name=%s ! fakesink async=false",
      MAX_RESOLUTION, MAX_RESOLUTION, DCC_FILE, SENSOR);
  g_print ("WIDTH: %d, HEIGHT: %d\n", width, height);
  test_states_change_success (pipeline, DEFAULT_STATE_CHANGES);

  /* Test min resolution */
  g_snprintf (pipeline, MAX_PIPELINE_SIZE,
      "videotestsrc is-live=true ! video/x-raw,format=NV12,width=%d,height=%d ! tiovxldc in-pool-size=4 out-pool-size=4 dcc-file=%s sensor-name=%s ! fakesink async=false",
      MIN_RESOLUTION, MIN_RESOLUTION, DCC_FILE, SENSOR);
  g_print ("WIDTH: %d, HEIGHT: %d\n", width, height);
  test_states_change_success (pipeline, DEFAULT_STATE_CHANGES);

  /* Test invalid resolutions */
  width = 0;
  height = 0;
  g_print ("WIDTH: %d, HEIGHT: %d\n", width, height);
  g_snprintf (pipeline, MAX_PIPELINE_SIZE,
      "videotestsrc is-live=true ! video/x-raw,format=NV12,width=%d,height=%d ! tiovxldc in-pool-size=4 out-pool-size=4 dcc-file=%s sensor-name=%s ! fakesink async=false",
      width, height, DCC_FILE, SENSOR);
  test_create_pipeline_fail (pipeline);

  width = MAX_RESOLUTION + g_random_int_range (1, MAX_RESOLUTION);
  height = MAX_RESOLUTION + g_random_int_range (1, MAX_RESOLUTION);
  g_print ("WIDTH: %d, HEIGHT: %d\n", width, height);
  g_snprintf (pipeline, MAX_PIPELINE_SIZE,
      "videotestsrc is-live=true ! video/x-raw,format=NV12,width=%d,height=%d ! tiovxldc in-pool-size=4 out-pool-size=4 dcc-file=%s sensor-name=%s ! fakesink async=false",
      width, height, DCC_FILE, SENSOR);
  test_create_pipeline_fail (pipeline);

}

GST_END_TEST;

static Suite *
gst_tiovx_ldc_suite (void)
{
  Suite *suite = suite_create ("tiovxldc");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_formats);
  tcase_add_test (tc, test_resolutions);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_ldc);
