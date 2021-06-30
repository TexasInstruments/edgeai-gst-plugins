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
 * *    No reverse engineering, decompilation, or disassembly of this software
 *      is permitted with respect to any software provided in binary form.
 *
 * *    Any redistribution and use are licensed by TI for use only with TI Devices.
 *
 * *    Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
 *
 * *    Any redistribution and use of the source code, including any resulting
 *      derivative works, are licensed by TI for use only with TI Devices.
 *
 * *    Any redistribution and use of any object code compiled from the source
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

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/gst.h>

#include "gst-libs/gst/tiovx/gsttiovxsiso.h"
#include "test_utils.h"

static const gchar *test_pipes[] = {
  "videotestsrc ! video/x-raw,format=RGB ! gsttiovxvideoconvert ! video/x-raw,format=RGBx ! fakesink",
  "videotestsrc ! video/x-raw,width=1920,height=1080 ! gsttiovxvideoconvert ! video/x-raw,width=1080,height=720 ! fakesink ",
  "videotestsrc ! gsttiovxvideoconvert ! gsttiovxvideoconvert ! fakesink",
  NULL,
};

enum
{
  /* Pipelines names */
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES,
  TEST_BLOCK_RESOLUTION_CHANGE,
  TEST_MULTIPLE_INSTANCES_IN_PIPELINE,
};

GST_START_TEST (test_playing_to_null_multiple_times)
{
  test_states_change (test_pipes[TEST_PLAYING_TO_NULL_MULTIPLE_TIMES]);
}

GST_END_TEST;

GST_START_TEST (test_equal_sink_src_caps_bypassing)
{
  GstHarness *h = NULL;
  GstBuffer *in_buf = NULL;
  GstBuffer *out_buf = NULL;
  const gchar *caps = "video/x-raw,width=1920,height=1080";
  const gsize size = 1920 * 1080;

  h = gst_harness_new ("gsttiovxvideoconvert");

  /* Define the caps */
  gst_harness_set_src_caps_str (h, caps);
  gst_harness_set_sink_caps_str (h, caps);

  /* Create a dummy buffer */
  in_buf = gst_harness_create_buffer (h, size);

  /* Push the buffer */
  gst_harness_push (h, in_buf);

  /* Pull out the buffer */
  out_buf = gst_harness_pull (h);

  /* Check the buffers are equals therefore bypassing the video conversion */
  fail_unless (in_buf == out_buf);

  /* Cleanup */
  gst_buffer_unref (out_buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_block_resolution_change)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;

  pipeline =
      gst_parse_launch (test_pipes[TEST_BLOCK_RESOLUTION_CHANGE], &error);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  fail_if (error == NULL);
  assert_equals_int (3, error->code);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_support_caps_renegotiation)
{

}

GST_END_TEST;

GST_START_TEST (test_multiple_instances_in_pipeline)
{
  test_states_change (test_pipes[TEST_MULTIPLE_INSTANCES_IN_PIPELINE]);
}

GST_END_TEST;

static Suite *
gst_tiovx_video_convert_suite (void)
{
  Suite *suite = suite_create ("videoconvert_caps");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_playing_to_null_multiple_times);
  tcase_skip_broken_test (tc, test_equal_sink_src_caps_bypassing);
  tcase_skip_broken_test (tc, test_block_resolution_change);
  tcase_skip_broken_test (tc, test_support_caps_renegotiation);
  tcase_skip_broken_test (tc, test_multiple_instances_in_pipeline);

  return suite;
}

GST_CHECK_MAIN (gst_tiovx_video_convert);
