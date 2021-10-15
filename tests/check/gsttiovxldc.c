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

#define TIOVXLDC_STATE_CHANGE_ITERATIONS 5
#define MAX_PIPELINE_SIZE 300
#define DCC_FILE "/opt/imaging/imx390/dcc_ldc_wdr.bin"

/* Supported dimensions.
 * FIXME: TIOVX node doesn't support low either high input resolution for the moment.
*/
#define MIN_RESOLUTION 240
#define MAX_RESOLUTION 2048

#define DEFAULT_IMAGE_WIDTH 1920
#define DEFAULT_IMAGE_HEIGHT 1080

static const gchar *gst_valid_formats[] = {
  "GRAY8",
  "GRAY16_LE",
  "NV12",
  "UYVY",
  NULL,
};

static const gchar *gst_invalid_formats[] = {
  "RGB",
  "RGBx",
  "NV21",
  "YUY2",
  "I420",
  NULL,
};

static const gchar *pipelines_caps_negotiation_fail[] = {
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,format=NV12 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=640,format=GRAY8 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxldc dcc-file=" DCC_FILE
      " ! video/x-raw,width=640,format=RGB ! fakesink",
  NULL,
};

static const gchar *pipelines_caps_negotiation_success[] = {
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=320,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240,format=GRAY8 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=320,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=320,height=240,format=GRAY8 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=640,height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240,format=GRAY8 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=640,height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=640,height=480,format=GRAY8 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240,format=GRAY8 ! tiovxldc dcc-file="
      DCC_FILE " ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxldc dcc-file=" DCC_FILE
      " ! video/x-raw,width=320,height=240,format=GRAY8 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxldc dcc-file=" DCC_FILE
      " ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=320,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=[320, 640],height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxldc dcc-file="
      DCC_FILE " ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=[240, 480] ! tiovxldc dcc-file="
      DCC_FILE " ! video/x-raw,width=[320, 640],height=[240, 480] ! fakesink",
  NULL,
};

GST_START_TEST (test_formats)
{
  const gchar pipeline_structure[] =
      "videotestsrc is-live=true ! video/x-raw,format=%s,width=%d,height=%d ! tiovxldc dcc-file=%s ! fakesink";
  gchar pipeline[MAX_PIPELINE_SIZE] = "";
  const gchar *format = NULL;
  guint i = 0;

  /* Test valid formats */
  format = gst_valid_formats[i];
  while (NULL != format) {
    g_snprintf (pipeline, MAX_PIPELINE_SIZE,
        pipeline_structure, format, DEFAULT_IMAGE_WIDTH, DEFAULT_IMAGE_HEIGHT,
        DCC_FILE);
    test_states_change_async (pipeline, TIOVXLDC_STATE_CHANGE_ITERATIONS);
    i++;
    format = gst_valid_formats[i];
  }

  /* Test invalid formats */
  i = 0;
  format = gst_invalid_formats[i];
  while (NULL != format) {
    g_snprintf (pipeline, MAX_PIPELINE_SIZE,
        pipeline_structure, format, DEFAULT_IMAGE_WIDTH, DEFAULT_IMAGE_HEIGHT,
        DCC_FILE);
    test_create_pipeline_fail (pipeline);
    i++;
    format = gst_invalid_formats[i];
  }

}

GST_END_TEST;

GST_START_TEST (test_caps_negotiation_fail)
{
  const gchar *pipeline = NULL;
  guint i = 0;
  pipeline = pipelines_caps_negotiation_fail[i];

  while (NULL != pipeline) {
    test_create_pipeline_fail (pipeline);
    i++;
    pipeline = pipelines_caps_negotiation_fail[i];
  }
}

GST_END_TEST;

GST_START_TEST (test_caps_negotiation_success)
{
  const gchar *pipeline = NULL;
  guint i = 0;

  pipeline = pipelines_caps_negotiation_success[i];

  while (NULL != pipeline) {
    test_states_change_async (pipeline, 1);
    i++;
    pipeline = pipelines_caps_negotiation_success[i];
  }
}

GST_END_TEST;

GST_START_TEST (test_pad_addition_fail)
{
  const gchar pipeline[] =
      "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=1920,height=1080 ! tiovxldc dcc-file="
      DCC_FILE
      " name=ldc ldc.src_0 ! queue ! fakesink ldc.src_1 ! queue ! fakesink";

  test_create_pipeline_fail (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_resolutions)
{
  const gchar pipeline_structure[] =
      "videotestsrc is-live=true ! video/x-raw,format=GRAY8,width=%d,height=%d ! tiovxldc dcc-file=%s ! fakesink";
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");
  gint width = 0;
  gint height = 0;

  width = g_random_int_range (MIN_RESOLUTION, MAX_RESOLUTION);
  height = g_random_int_range (MIN_RESOLUTION, MAX_RESOLUTION);

  g_string_printf (pipeline, pipeline_structure, width, height, DCC_FILE);

  /* Test random resolutions */
  test_states_change_async (pipeline->str, TIOVXLDC_STATE_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_max_resolution)
{
  const gchar pipeline_structure[] =
      "videotestsrc is-live=true ! video/x-raw,format=UYVY,width=%d,height=%d ! tiovxldc dcc-file=%s ! fakesink";
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");

  g_string_printf (pipeline, pipeline_structure, MAX_RESOLUTION, MAX_RESOLUTION,
      DCC_FILE);

  test_states_change_async (pipeline->str, TIOVXLDC_STATE_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_min_resolution)
{
  const gchar pipeline_structure[] =
      "videotestsrc is-live=true ! video/x-raw,format=UYVY,width=%d,height=%d ! tiovxldc dcc-file=%s ! fakesink";
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");

  g_string_printf (pipeline, pipeline_structure, MIN_RESOLUTION, MIN_RESOLUTION,
      DCC_FILE);

  test_states_change_async (pipeline->str, TIOVXLDC_STATE_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_resolutions_fail)
{
  const gchar pipeline_structure[] =
      "videotestsrc is-live=true ! video/x-raw,format=GRAY8,width=%d,height=%d ! tiovxldc dcc-file=%s ! fakesink";
  g_autoptr (GString) pipeline = g_string_new ("");
  g_autoptr (GString) upstream_caps = g_string_new ("");

  g_string_printf (pipeline, pipeline_structure, -1, -1, DCC_FILE);

  test_create_pipeline_fail (pipeline->str);
}

GST_END_TEST;

GST_START_TEST (test_no_dcc_file_fail)
{
  const gchar *pipeline = "videotestsrc ! tiovxldc ! fakesink";

  test_states_change_async_fail_success (pipeline,
      TIOVXLDC_STATE_CHANGE_ITERATIONS);
}

GST_END_TEST;

GST_START_TEST (test_invalid_dcc_file_fail)
{
  const gchar *pipeline =
      "videotestsrc ! tiovxldc dcc-file=/invalid/dcc/file ! fakesink";

  test_states_change_async_fail_success (pipeline,
      TIOVXLDC_STATE_CHANGE_ITERATIONS);
}

GST_END_TEST;

static Suite *
gst_tiovx_ldc_suite (void)
{
  Suite *suite = suite_create ("tiovxldc");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_formats);
  tcase_add_test (tc, test_caps_negotiation_fail);
  tcase_add_test (tc, test_caps_negotiation_success);
  tcase_add_test (tc, test_pad_addition_fail);
  tcase_add_test (tc, test_resolutions);
  tcase_add_test (tc, test_max_resolution);
  tcase_add_test (tc, test_min_resolution);
  tcase_add_test (tc, test_resolutions_fail);
  tcase_add_test (tc, test_no_dcc_file_fail);
  tcase_add_test (tc, test_invalid_dcc_file_fail);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_ldc);
