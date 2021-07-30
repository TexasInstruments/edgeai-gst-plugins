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
#include <gst/check/gstharness.h>
#include <TI/tivx.h>

#include "gst-libs/gst/tiovx/gsttiovxcontext.h"
#include "test_utils.h"

#define MAX_PIPELINE_SIZE 300
#define MAX_CAPS_SIZE 200
#define SINK_FORMATS 7
#define SRC_FORMATS 4
#define PASSTHROUGHT_FORMATS 4

static const int kImageWidth = 640;
static const int kImageHeight = 480;

static const gchar *gst_sink_formats[] = {
  "RGB",
  "RGBx",
  "NV12",
  "NV21",
  "UYVY",
  "YUY2",
  "I420"
};

static const gchar *passthrough_formats[] = {
  "RGB",
  "RGBx",
  "NV12",
  "I420"
};

static const gchar *gst_src_formats[SINK_FORMATS][SRC_FORMATS] = {
  {"RGBx", "NV12", "I420", "Y444"},
  {"RGB", "NV12", "I420", "Y444"},
  {"RGB", "RGBx", "I420", "Y444"},
  {"RGB", "RGBx", "I420", "Y444"},
  {"RGB", "RGBx", "NV12", "I420"},
  {"RGB", "RGBx", "NV12", "I420"},
  {"RGB", "RGBx", "NV12", "Y444"}

};

GST_START_TEST (test_bypass_on_same_caps)
{
  GstHarness *h;
  GstBuffer *in_buf;
  GstBuffer *out_buf;
  gchar caps[MAX_CAPS_SIZE] = "";
  const gsize size = 640 * 480 * 3;
  gint passthrough_format;

  for (passthrough_format = 0; passthrough_format < PASSTHROUGHT_FORMATS;
      passthrough_format++) {
    g_snprintf (caps, MAX_CAPS_SIZE,
        "video/x-raw,format=%s,width=640,height=480,framerate=30/1",
        passthrough_formats[passthrough_format]);

    h = gst_harness_new ("tiovxcolorconvert");
    gst_harness_set (h, "tiovxcolorconvert", "in-pool-size", 4, NULL);
    gst_harness_set (h, "tiovxcolorconvert", "out-pool-size", 4, NULL);
    /* Define caps */
    gst_harness_set_src_caps_str (h, caps);
    gst_harness_set_sink_caps_str (h, caps);

    /* Create a dummy buffer */
    in_buf = gst_harness_create_buffer (h, size);

    /* Push the buffer */
    gst_harness_push (h, in_buf);

    /* Pull out the buffer */
    out_buf = gst_harness_pull (h);

    /* validate the buffer in is the same as buffer out */
    fail_unless (in_buf == out_buf);

    /* cleanup */
    gst_buffer_unref (out_buf);
    gst_harness_teardown (h);
  }
}

GST_END_TEST;

GST_START_TEST (test_state_change)
{
  gchar pipeline[MAX_PIPELINE_SIZE] = "";
  gint sink_format;
  gint src_format;

  for (sink_format = 0; sink_format < SINK_FORMATS; sink_format++) {
    for (src_format = 0; src_format < SRC_FORMATS; src_format++) {
      g_snprintf (pipeline, MAX_PIPELINE_SIZE,
          "videotestsrc is-live=true num-buffers=5 ! video/x-raw,format=%s,width=%d,height=%d ! tiovxcolorconvert in-pool-size=4 out-pool-size=4 ! video/x-raw,format=%s,width=%d,height=%d ! fakesink async=false",
          gst_sink_formats[sink_format], kImageWidth, kImageHeight,
          gst_src_formats[sink_format][src_format], kImageWidth, kImageHeight);
      test_states_change_success (pipeline);
    }
  }
}

GST_END_TEST;

static Suite *
gst_tiovx_color_convert_suite (void)
{
  Suite *suite = suite_create ("tiovxcolorconvert");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_bypass_on_same_caps);
  tcase_add_test (tc, test_state_change);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_color_convert);
