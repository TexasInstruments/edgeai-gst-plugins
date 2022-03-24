/*
 * Copyright (c) [2022] Texas Instruments Incorporated
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

#define NUM_BUFFERS_BEFORE_RENEGOTIATION 3
#define NUM_RENEGOTIATION_ATTEMPTS 10

#define MAX_PIPELINE_SIZE 300
#define SINK_FORMATS 4
#define SRC_FORMATS_SUCCESS 2
#define SRC_FORMATS_FAIL 2
#define NUM_STATE_TRANSITIONS 5

static const int kImageWidth = 640;
static const int kImageHeight = 480;

static const gchar *gst_sink_formats[] = {
  "RGB",
  "NV12",
  "NV21",
  "I420"
};

static const gchar *gst_src_formats_success[SINK_FORMATS][SRC_FORMATS_SUCCESS] = {
  {"NV12", NULL},
  {"RGB", "I420"},
  {"RGB", "I420"},
  {"NV12", NULL}
};

static const gchar *gst_src_formats_fail[SINK_FORMATS][SRC_FORMATS_FAIL] = {
  {"UYVY", "YUY2"},
  {"UYVY", "YUY2"},
  {"UYVY", "YUY2"},
  {"UYVY", "YUY2"},
};

static const gchar *pipelines_caps_negotiation_fail[] = {
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxdlcolorconvert ! video/x-raw,width=640,height=480 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320 ! tiovxdlcolorconvert ! video/x-raw,width=640 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,height=240 ! tiovxdlcolorconvert ! video/x-raw,height=480 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxdlcolorconvert ! video/x-raw,height=480 ! fakesink ",
  NULL
};

static const gchar *pipelines_caps_negotiation_success[] = {
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=240 ! tiovxdlcolorconvert ! video/x-raw,width=320,height=240 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxdlcolorconvert ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxdlcolorconvert ! video/x-raw,width=320,height=240 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxdlcolorconvert ! video/x-raw,width=500,height=240 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxdlcolorconvert ! video/x-raw,width=640,height=240 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640] ! tiovxdlcolorconvert ! video/x-raw,width=320 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=240 ! tiovxdlcolorconvert ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxdlcolorconvert ! video/x-raw,width=320,height=240 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxdlcolorconvert ! video/x-raw,width=320,height=320 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxdlcolorconvert ! video/x-raw,width=320,height=480 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,height=[240, 480] ! tiovxdlcolorconvert ! video/x-raw,height=320 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=320,height=[240, 480] ! tiovxdlcolorconvert ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=[240, 480] ! tiovxdlcolorconvert ! video/x-raw,width=320,height=240 ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=[240, 480] ! tiovxdlcolorconvert ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,width=[320, 640],height=[240, 480] ! tiovxdlcolorconvert ! video/x-raw,width=[320, 640],height=[240, 480] ! fakesink ",
  NULL,
};

GST_START_TEST (test_state_change_success)
{
  gchar pipeline[MAX_PIPELINE_SIZE] = "";
  gint sink_format = 0;
  gint src_format = 0;

  for (sink_format = 0; sink_format < SINK_FORMATS; sink_format++) {
    for (src_format = 0; src_format < SRC_FORMATS_SUCCESS; src_format++) {

      if (NULL == gst_src_formats_success[sink_format][src_format]) {
        continue;
      }
      if (NULL == gst_sink_formats[sink_format]) {
        continue;
      }

      g_snprintf (pipeline, MAX_PIPELINE_SIZE,
          "videotestsrc is-live=true ! video/x-raw,format=%s,width=%d,height=%d ! tiovxdlcolorconvert in-pool-size=4 out-pool-size=4 ! video/x-raw,format=%s,width=%d,height=%d ! fakesink",
          gst_sink_formats[sink_format], kImageWidth, kImageHeight,
          gst_src_formats_success[sink_format][src_format], kImageWidth,
          kImageHeight);
      test_states_change_async (pipeline, NUM_STATE_TRANSITIONS);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_pad_creation_fail)
{
  gchar pipeline[MAX_PIPELINE_SIZE] = "";
  gint sink_format = 0;
  gint src_format = 0;

  for (sink_format = 0; sink_format < SINK_FORMATS; sink_format++) {
    for (src_format = 0; src_format < SRC_FORMATS_FAIL; src_format++) {

      if (NULL == gst_src_formats_fail[sink_format][src_format]) {
        continue;
      }
      if (NULL == gst_sink_formats[sink_format]) {
        continue;
      }

      g_snprintf (pipeline, MAX_PIPELINE_SIZE,
          "videotestsrc is-live=true ! video/x-raw,format=%s,width=%d,height=%d ! tiovxdlcolorconvert in-pool-size=4 out-pool-size=4 ! video/x-raw,format=%s,width=%d,height=%d ! fakesink ",
          gst_sink_formats[sink_format], kImageWidth, kImageHeight,
          gst_src_formats_fail[sink_format][src_format], kImageWidth,
          kImageHeight);
      test_create_pipeline_fail (pipeline);
    }
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

GST_START_TEST (test_caps_renegotiation)
{
  const gchar *desc =
      "videotestsrc is-live=true ! capsfilter name=caps caps=video/x-raw,width=320,height=240,format=NV12 ! tiovxdlcolorconvert ! video/x-raw,format=RGB ! fakesink name=fakesink signal-handoffs=true";
  GstElement *pipeline = NULL;
  GstElement *caps = NULL;
  GstElement *fakesink = NULL;
  GstCaps *caps_320x240 = NULL;
  GstCaps *caps_640x480 = NULL;
  gboolean caps_switch = TRUE;
  BufferCounter buffer_counter = { 0 };
  guint i = 0;

  /* Initialize renegotation structure */
  buffer_counter.buffer_counter = 0;
  buffer_counter.buffer_limit = NUM_BUFFERS_BEFORE_RENEGOTIATION;
  g_mutex_init (&buffer_counter.mutex);
  g_cond_init (&buffer_counter.cond);
  pipeline = test_create_pipeline (desc);

  /* Play pipeline */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL,
          GST_CLOCK_TIME_NONE), GST_STATE_CHANGE_SUCCESS);

  caps = gst_bin_get_by_name (GST_BIN (pipeline), "caps");
  fail_unless (NULL != caps);
  fakesink = gst_bin_get_by_name (GST_BIN (pipeline), "fakesink");
  fail_unless (NULL != fakesink);

  /* Connect to fakesink signal to count number of buffers */
  g_signal_connect (fakesink, "handoff", G_CALLBACK (buffer_counter_func),
      &buffer_counter);

  caps_320x240 =
      gst_caps_from_string ("video/x-raw,width=320,height=240,format=NV12");
  caps_640x480 =
      gst_caps_from_string ("video/x-raw,width=640,height=480,format=NV12");

  for (i = 0; i < NUM_RENEGOTIATION_ATTEMPTS; i++) {
    g_mutex_lock (&buffer_counter.mutex);
    g_cond_wait (&buffer_counter.cond, &buffer_counter.mutex);
    g_mutex_unlock (&buffer_counter.mutex);

    if (caps_switch) {
      g_object_set (G_OBJECT (caps), "caps", caps_640x480, NULL);
    } else {
      g_object_set (G_OBJECT (caps), "caps", caps_320x240, NULL);
    }

    caps_switch = !caps_switch;
  }

  /* Stop pipeline */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_caps_unref (caps_320x240);
  gst_caps_unref (caps_640x480);
  g_cond_clear (&buffer_counter.cond);
  g_mutex_clear (&buffer_counter.mutex);
}

GST_END_TEST;

static Suite *
gst_dl_tiovx_color_convert_suite (void)
{
  Suite *suite = suite_create ("tiovxdlcolorconvert");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_state_change_success);
  tcase_add_test (tc, test_pad_creation_fail);
  tcase_add_test (tc, test_caps_negotiation_fail);
  tcase_add_test (tc, test_caps_negotiation_success);
  tcase_add_test (tc, test_caps_renegotiation);

  return suite;
}

GST_CHECK_MAIN (gst_dl_tiovx_color_convert);
