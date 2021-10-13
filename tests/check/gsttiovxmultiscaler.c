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

static const gchar *test_pipelines[] = {
  "videotestsrc is-live=true ! video/x-raw,format=NV12,width=1280,height=720 ! tiovxmultiscaler name=multi",
  "videotestsrc is-live=true ! video/x-raw,width=1280,height=720 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=640,height=480 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1280,height=720 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink  multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1280,height=720 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink  multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink  multi.src_2"
      "! video/x-raw,width=320,height=240 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1280,height=720 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink  multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink  multi.src_2"
      "! video/x-raw,width=320,height=240 ! fakesink  multi.src_3"
      "! video/x-raw,width=800,height=600 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1280,height=720 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink  multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink  multi.src_2"
      "! video/x-raw,width=320,height=240 ! fakesink  multi.src_3"
      "! video/x-raw,width=800,height=600 ! fakesink  multi.src_4"
      "! video/x-raw,width=320,height=180 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink  multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink  multi.src_2"
      "! video/x-raw,width=320,height=240 ! fakesink  multi.src_3"
      "! video/x-raw,width=640,height=480 ! fakesink  multi.src_4"
      "! video/x-raw,width=1280,height=720 ! fakesink  multi.src_5"
      "! video/x-raw,width=1280,height=720 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=640,height=480 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1280,height=720 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=100,height=100 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1280,height=720 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=100,height=480 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1280,height=720 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=640,height=100 ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! "
      "tiovxmultiscaler name=multi multi. ! video/x-raw,width=640,height=480 ! queue ! "
      "fakesink ",
  "videotestsrc is-live=true ! video/x-raw,format=GRAY8,width=1920,height=1080 ! "
      "tiovxmultiscaler name=multi multi. ! video/x-raw,width=640,height=480 ! queue ! "
      "fakesink ",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! "
      "tiovxmultiscaler name=multi multi. ! video/x-raw,format=GRAY8,width=640,height=480 ! queue ! "
      "fakesink ",
  "videotestsrc is-live=true ! video/x-raw, width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0 ! "
      "video/x-raw,format=GRAY8,width=1280,height=720 ! queue ! fakesink  multi.src_1 ! "
      "video/x-raw,width=640,height=480 ! queue ! fakesink ",
  "videotestsrc is-live=true ! video/x-raw,format=GRAY8,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0 ! "
      "video/x-raw,format=GRAY8,width=1280,height=720 ! queue ! fakesink  multi.src_1 ! "
      "video/x-raw,width=640,height=480 ! queue ! fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,format=GRAY16_LE,width=1920,height=1080 ! "
      "tiovxmultiscaler name=multi multi. ! video/x-raw,format=NV12,width=640,height=480 ! queue ! "
      "fakesink ",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw,format=GRAY8,width=1920,height=1080 ! "
      "tiovxmultiscaler name=multi multi.src_0 ! video/x-raw,format=GRAY8,width=1280,height=720 ! queue ! "
      "fakesink  multi.src_1 ! video/x-raw,format=NV12,width=640,height=480 ! queue ! fakesink ",
  NULL,
};

enum
{
  /* Pipelines names */
  TEST_ZERO_PADS = 0,
  TEST_ONE_PAD,
  TEST_TWO_PADS,
  TEST_THREE_PADS,
  TEST_FOUR_PADS,
  TEST_FIVE_PADS,
  TEST_SIX_PADS,
  TEST_UPSCALING,
  TEST_LOW_RES,
  TEST_LOW_RES_WIDTH,
  TEST_LOW_RES_HEIGHT,
  TEST_NO_FORMAT_SPECIFIED,
  TEST_NO_FORMAT_SRC_CAPS_GRAY8,
  TEST_NO_FORMAT_SINK_CAPS_GRAY8,
  TEST_2_OUT_FORMAT_1_SRC_CAPS_GRAY8,
  TEST_2_OUT_FORMAT_SINK_CAPS_1_SRC_CAPS_GRAY8,
  TEST_DIFF_FORMAT_INPUT_OUTPUT,
  TEST_DIFF_FORMAT_INPUT_2_OUTPUTS,
};

GST_START_TEST (test_pads_success)
{
  test_create_pipeline (test_pipelines[TEST_ZERO_PADS]);

  test_create_pipeline (test_pipelines[TEST_ONE_PAD]);

  test_create_pipeline (test_pipelines[TEST_TWO_PADS]);

  test_create_pipeline (test_pipelines[TEST_THREE_PADS]);

  test_create_pipeline (test_pipelines[TEST_FOUR_PADS]);

  test_create_pipeline (test_pipelines[TEST_FIVE_PADS]);
}

GST_END_TEST;

GST_START_TEST (test_pads_fail)
{
  test_create_pipeline_fail (test_pipelines[TEST_SIX_PADS]);
}

GST_END_TEST;

GST_START_TEST (test_state_transitions)
{
  test_states_change_success (test_pipelines[TEST_ZERO_PADS]);

  test_states_change_success (test_pipelines[TEST_ONE_PAD]);

  test_states_change_success (test_pipelines[TEST_TWO_PADS]);

  test_states_change_success (test_pipelines[TEST_THREE_PADS]);

  test_states_change_success (test_pipelines[TEST_FOUR_PADS]);

  test_states_change_success (test_pipelines[TEST_FIVE_PADS]);

  test_states_change_success (test_pipelines[TEST_NO_FORMAT_SPECIFIED]);

  test_states_change_success (test_pipelines[TEST_NO_FORMAT_SRC_CAPS_GRAY8]);

  test_states_change_success (test_pipelines[TEST_NO_FORMAT_SINK_CAPS_GRAY8]);

  test_states_change_success (test_pipelines[TEST_2_OUT_FORMAT_1_SRC_CAPS_GRAY8]);

  test_states_change_success (test_pipelines[TEST_2_OUT_FORMAT_SINK_CAPS_1_SRC_CAPS_GRAY8]);
}

GST_END_TEST;

GST_START_TEST (test_caps_fail)
{
  test_create_pipeline_fail (test_pipelines[TEST_UPSCALING]);
  test_create_pipeline_fail (test_pipelines[TEST_LOW_RES]);
  test_create_pipeline_fail (test_pipelines[TEST_LOW_RES_WIDTH]);
  test_create_pipeline_fail (test_pipelines[TEST_LOW_RES_HEIGHT]);
  test_create_pipeline_fail (test_pipelines[TEST_DIFF_FORMAT_INPUT_OUTPUT]);
  test_create_pipeline_fail (test_pipelines[TEST_DIFF_FORMAT_INPUT_2_OUTPUTS]);
}

GST_END_TEST;

GST_START_TEST (test_request_release_pads)
{
  GError *error = NULL;
  const gchar *desc = test_pipelines[TEST_ZERO_PADS];
  GstElement *pipeline = NULL;
  GstElement *ms = NULL;
  GstPad *pad = NULL;

  pipeline = gst_parse_launch (desc, &error);
  fail_if (error != NULL, error->message);

  ms = gst_bin_get_by_name (GST_BIN (pipeline), "multi");
  fail_if (ms == NULL, "Unable to find multiscaler in the pipeline");

  pad = gst_element_get_request_pad (ms, "src_0");
  fail_if (pad == NULL, "Unable to request pad from multiscaler");

  gst_element_release_request_pad (ms, pad);
  gst_object_unref (pad);

  pad = gst_element_get_request_pad (ms, "src_0");
  fail_if (pad == NULL, "Unable to request pad from multiscaler");

  gst_element_release_request_pad (ms, pad);
  gst_object_unref (pad);
}

GST_END_TEST;

GST_START_TEST (test_request_repeated_pads)
{
  GError *error = NULL;
  const gchar *desc = test_pipelines[TEST_ZERO_PADS];
  GstElement *pipeline = NULL;
  GstElement *ms = NULL;
  GstPad *pad = NULL;
  GstPad *pad2 = NULL;

  pipeline = gst_parse_launch (desc, &error);
  fail_if (error != NULL, error->message);

  ms = gst_bin_get_by_name (GST_BIN (pipeline), "multi");
  fail_if (ms == NULL, "Unable to find multiscaler in the pipeline");

  pad = gst_element_get_request_pad (ms, "src_0");
  fail_if (pad == NULL, "Unable to request pad from multiscaler");

  ASSERT_CRITICAL(pad2 = gst_element_get_request_pad (ms, "src_0"));
  fail_if (pad2 != NULL, "Was able to request repeated pad");
  
  gst_element_release_request_pad (ms, pad);
  gst_object_unref (pad);
}

GST_END_TEST;

GST_START_TEST (test_request_dynamic_pads_on_play)
{
  GError *error = NULL;
  const gchar *desc = test_pipelines[TEST_ZERO_PADS];
  GstElement *pipeline = NULL;
  GstElement *ms = NULL;
  GstPad *pad = NULL;
  GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;
  GstMessage *msg = NULL;
  GstBus *bus = NULL;

  pipeline = gst_parse_launch (desc, &error);
  fail_if (error != NULL, error->message);

  ms = gst_bin_get_by_name (GST_BIN (pipeline), "multi");
  fail_if (ms == NULL, "Unable to find multiscaler in the pipeline");

  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  fail_if (ret != GST_STATE_CHANGE_SUCCESS, "Unable to play pipeline");

  fail_if (msg != NULL, "Expected an error but got none");

  bus = gst_pipeline_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus,GST_SECOND*5,GST_MESSAGE_ERROR);
  gst_object_unref (bus);
  error = NULL;
  gst_message_parse_error (msg, &error, NULL);

  fail_unless_equals_int (GST_LIBRARY_ERROR_FAILED, error->code);
  fail_unless_equals_string ("Unable to init TIOVX module: 0", error->message);
						
  
  pad = gst_element_get_request_pad (ms, "src_0");
  fail_if (pad != NULL, "Pad request on a playing pipeline succeded, "
      "but should've failed");

  ret = gst_element_set_state (pipeline, GST_STATE_NULL);
  fail_if (ret != GST_STATE_CHANGE_SUCCESS, "Unable to stop pipeline");
}

GST_END_TEST;

GST_START_TEST (test_remove_dynamic_pads_on_play)
{
  GError *error = NULL;
  const gchar *desc = test_pipelines[TEST_ZERO_PADS];
  GstElement *pipeline = NULL;
  GstElement *ms = NULL;
  GstPad *pad = NULL;
  GstPad *pad2 = NULL;
  GstStateChangeReturn ret = GST_STATE_CHANGE_FAILURE;

  pipeline = gst_parse_launch (desc, &error);
  fail_if (error != NULL, error->message);

  ms = gst_bin_get_by_name (GST_BIN (pipeline), "multi");
  fail_if (ms == NULL, "Unable to find multiscaler in the pipeline");

  pad = gst_element_get_request_pad (ms, "src_0");
  fail_if (pad == NULL, "Unable to request pad from multiscaler");
  
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  fail_if (ret != GST_STATE_CHANGE_SUCCESS, "Unable to play pipeline");
  
  gst_element_release_request_pad (ms, pad);
  gst_object_unref (pad);

  pad2 = gst_element_get_static_pad (ms, "src_0");
  fail_if (pad2 != NULL, "Request pad was removed while playing, which is forbiden");
  gst_object_unref (pad2);
  
  ret = gst_element_set_state (pipeline, GST_STATE_NULL);
  fail_if (ret != GST_STATE_CHANGE_SUCCESS, "Unable to stop pipeline");
}

GST_END_TEST;


static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("tiovxmultiscaler");
  TCase *tc = tcase_create ("tc");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_pads_success);
  tcase_add_test (tc, test_pads_fail);
  /* tcase_add_test (tc, test_caps_fail); */
  /* tcase_add_test (tc, test_state_transitions); */
  /* tcase_add_test (tc, test_request_release_pads); */
  /* tcase_add_test (tc, test_request_repeated_pads); */
  /* tcase_add_test (tc, test_request_dynamic_pads_on_play); */
  /* tcase_add_test (tc, test_remove_dynamic_pads_on_play); */

  return suite;
}

GST_CHECK_MAIN (gst_state);
