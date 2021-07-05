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

#include <gst-libs/gst/tiovx/gsttiovxsiso.h>

GST_START_TEST (test_mock_push_buffer_fail)
{
  GstHarness *h;
  GstBuffer *in_buf;
  GstFlowReturn ret;

  const gchar *incaps =
      "video/x-raw,format=RGB,width=640,height=480,framerate=30/1";
  const gchar *outcaps =
      "video/x-raw,format=RGB,width=640,height=480,framerate=30/1";
  const gsize size = 640 * 480;

  h = gst_harness_new ("tiovxsisomock");

  gst_harness_set (h, "tiovxsisomock", "create_node_fail", TRUE);

  /* Define caps */
  gst_harness_set_src_caps_str (h, outcaps);
  gst_harness_set_sink_caps_str (h, incaps);

  /* Create a dummy buffer */
  in_buf = gst_harness_create_buffer (h, size);

  /* Push the buffer */
  ret = gst_harness_push (h, in_buf);
  fail_if (GST_FLOW_ERROR != ret);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_mock_configure_node_fail)
{
  GstHarness *h;
  GstBuffer *in_buf;
  GstFlowReturn ret;

  const gchar *incaps =
      "video/x-raw,format=RGB,width=640,height=480,framerate=30/1";
  const gchar *outcaps =
      "video/x-raw,format=RGB,width=640,height=480,framerate=30/1";
  const gsize size = 640 * 480;

  h = gst_harness_new ("tiovxsisomock");

  gst_harness_set (h, "tiovxsisomock", "configure_node_fail", TRUE);

  /* Define caps */
  gst_harness_set_src_caps_str (h, outcaps);
  gst_harness_set_sink_caps_str (h, incaps);

  /* Create a dummy buffer */
  in_buf = gst_harness_create_buffer (h, size);

  /* Push the buffer */
  ret = gst_harness_push (h, in_buf);
  fail_if (GST_FLOW_ERROR != ret);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_mock_push_buffer_success)
{
  GstHarness *h;
  GstBuffer *in_buf;
  const gchar *incaps =
      "video/x-raw,format=RGB,width=640,height=480,framerate=30/1";
  const gchar *outcaps =
      "video/x-raw,format=RGB,width=640,height=480,framerate=30/1";
  const gsize size = 640 * 480;

  h = gst_harness_new ("tiovxsisomock");

  gst_harness_set (h, "tiovxsisomock", "create_node_fail", FALSE);

  /* Define caps */
  gst_harness_set_src_caps_str (h, outcaps);
  gst_harness_set_sink_caps_str (h, incaps);

  /* Create a dummy buffer */
  in_buf = gst_harness_create_buffer (h, size);

  /* Push the buffer */
  gst_harness_push (h, in_buf);

  /* cleanup */
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
gst_ti_ovx_siso_mock_suite (void)
{
  Suite *suite = suite_create ("tiovxsisomock");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_mock_push_buffer_fail);
  tcase_add_test (tc, test_mock_configure_node_fail);
  tcase_add_test (tc, test_mock_push_buffer_success);

  return suite;
}

GST_CHECK_MAIN (gst_ti_ovx_siso_mock);
