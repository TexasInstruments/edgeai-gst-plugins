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
#define DCC_FILE "/opt/imaging/imx390/dcc_ldc_wdr.bin"
#define DEFAULT_STATE_CHANGES 3

static const int default_image_width = 1920;
static const int default_image_height = 1080;

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
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=640,height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=640 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,height=240 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxcotiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.binlorconvert ! video/x-raw,width=640 ! fakesink",
  NULL,
};

static const gchar *pipelines_caps_negotiation_success[] = {
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=320,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=320,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=500,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=640,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=320 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=320,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=320,height=320 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=320,height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,height=[240, 480] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,height=320 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=[240, 480] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=320,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=[240, 480] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=[240, 480] ! tiovxldc dcc-file=/opt/imaging/imx390/dcc_ldc_wdr.bin ! video/x-raw,width=[320, 640],height=[240, 480] ! fakesink",
  NULL,
};

GST_START_TEST (test_formats)
{
  const gchar pipeline_structure[] =
      "videotestsrc is-live=true ! video/x-raw,format=%s,width=%d,height=%d ! tiovxldc dcc-file=%s in-pool-size=4 out-pool-size=4 ! fakesink";
  gchar pipeline[MAX_PIPELINE_SIZE] = "";
  const gchar *format = NULL;
  guint i = 0;

  /* Test valid formats */
  format = gst_valid_formats[i];
  while (NULL != format) {
    g_snprintf (pipeline, MAX_PIPELINE_SIZE,
        pipeline_structure, format, default_image_width, default_image_height,
        DCC_FILE);
    test_states_change_async (pipeline, DEFAULT_STATE_CHANGES);
    i++;
    format = gst_valid_formats[i];
  }

  /* Test invalid formats */
  i = 0;
  format = gst_invalid_formats[i];
  while (NULL != format) {
    g_snprintf (pipeline, MAX_PIPELINE_SIZE,
        pipeline_structure, format, default_image_width, default_image_height,
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

static Suite *
gst_tiovx_ldc_suite (void)
{
  Suite *suite = suite_create ("tiovxldc");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_formats);
  tcase_add_test (tc, test_caps_negotiation_fail);
  tcase_add_test (tc, test_caps_negotiation_success);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_ldc);
