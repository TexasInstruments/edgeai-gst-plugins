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

#include "gst-libs/gst/tiovx/gsttiovxbufferpool.h"
#include "gst-libs/gst/tiovx/gsttiovxcontext.h"
#include "test_utils.h"

#define MAX_PIPELINE_SIZE 250
#define SINK_FORMATS 7
#define SRC_FORMATS 5

static const int kImageWidth = 640;
static const int kImageHeight = 480;

static const int kMinBuffers = 2;
static const int kMaxBuffers = 4;

static const gchar *gst_sink_formats[] = {
  "RGB",
  "RGBx",
  "NV12",
  "NV21",
  "UYVY",
  "YUY2",
  "I420"
};

static const gchar *gst_src_formats[] = {
  "RGB",
  "RGBx",
  "NV12",
  "I420",
  "Y444"
};

static const vx_df_image vx_sink_formats[] = {
  VX_DF_IMAGE_RGB,
  VX_DF_IMAGE_RGBX,
  VX_DF_IMAGE_NV12,
  VX_DF_IMAGE_IYUV,
  VX_DF_IMAGE_YUV4
};

static void
initialize_tiovx_buffer_pool (GstBufferPool ** buffer_pool,
    const gchar * gst_format, vx_df_image vx_format)
{
  GstStructure *conf = NULL;
  GstCaps *caps = NULL;
  GstVideoInfo info;
  gboolean ret = FALSE;
  vx_context context;
  vx_status status;
  vx_reference reference;

  *buffer_pool = g_object_new (GST_TIOVX_TYPE_BUFFER_POOL, NULL);

  conf = gst_buffer_pool_get_config (*buffer_pool);
  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, gst_format,
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight, NULL);
  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  ret = gst_video_info_from_caps (&info, caps);
  fail_if (!ret, "Unable to get video info from caps");

  reference =
      (vx_reference) vxCreateImage (context, kImageWidth, kImageHeight,
      vx_format);

  gst_buffer_pool_config_set_exemplar (conf, reference);

  gst_buffer_pool_config_set_params (conf, caps, GST_VIDEO_INFO_SIZE (&info),
      kMinBuffers, kMaxBuffers);
  ret = gst_buffer_pool_set_config (*buffer_pool, conf);
  gst_caps_unref (caps);
  fail_if (FALSE == ret, "Buffer pool configuration failed");

  ret = gst_buffer_pool_set_active (*buffer_pool, TRUE);
  fail_if (FALSE == ret, "Can't set the pool to active");
}

GST_START_TEST (test_bypass_on_same_caps)
{
  GstHarness *h;
  GstBuffer *in_buf;
  GstBuffer *out_buf;
  const gchar *caps =
      "video/x-raw,format=RGB,width=640,height=480,framerate=30/1";
  const gsize size = 640 * 480 * 3;

  h = gst_harness_new ("tiovxcolorconvert");

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

GST_END_TEST;

GST_START_TEST (test_state_change)
{
  gchar pipeline[MAX_PIPELINE_SIZE] = "";
  gint sink_format;
  gint src_format;

  for (sink_format = 0; sink_format < SINK_FORMATS; sink_format++){
    for (src_format = 0; src_format < SRC_FORMATS; src_format++){

      g_snprintf (pipeline, MAX_PIPELINE_SIZE,
        "videotestsrc is-live=true num-buffers=5 ! video/x-raw,format=%s,width=%d,height=%d ! tiovxcolorconvert ! video/x-raw,format=%s,width=%d,height=%d ! fakesink async=false",
        gst_sink_formats[sink_format], kImageWidth, kImageHeight, gst_src_formats[src_format], kImageWidth, kImageHeight);

      test_states_change_success (pipeline);
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_RGB_to_NV12)
{
  GstHarness *h;
  GstBuffer *in_buf;
  GstFlowReturn ret;
  GstBufferPool *pool = NULL;

  const gchar *in_caps =
      "video/x-raw,format=RGB,width=640,height=480,framerate=30/1";
  const gchar *out_caps =
      "video/x-raw,format=NV12,width=640,height=480,framerate=30/1";

  GstTIOVXContext *tiovx_context = gst_tiovx_context_new ();

  h = gst_harness_new ("tiovxcolorconvert");

  initialize_tiovx_buffer_pool (&pool, gst_sink_formats[0], vx_sink_formats[0]);
  gst_buffer_pool_acquire_buffer (pool, &in_buf, NULL);

  /* Define caps */
  gst_harness_set_src_caps_str (h, in_caps);
  gst_harness_set_sink_caps_str (h, out_caps);

  /* Push the buffer */
  ret = gst_harness_push (h, in_buf);
  gst_harness_pull (h);

  fail_if (GST_FLOW_OK != ret);

  /* cleanup */
  g_object_unref (pool);
  g_object_unref (tiovx_context);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
gst_tiovx_color_convert_suite (void)
{
  Suite *suite = suite_create ("tiovxcolorconvert");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_bypass_on_same_caps);
  tcase_add_test (tc, test_RGB_to_NV12);
  tcase_add_test (tc, test_state_change);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_color_convert);
