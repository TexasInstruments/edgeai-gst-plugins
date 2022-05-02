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


static const gchar *pipelines_caps_negotiation_fail[] = {
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=640 ! tiovxpyramid ! application/x-pyramid-tiovx, width=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=[320, 640] ! tiovxpyramid ! application/x-pyramid-tiovx, width=1280 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, height=1920 ! tiovxpyramid ! application/x-pyramid-tiovx, width=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, width=3840 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, format=GRAY8 ! tiovxpyramid ! application/x-pyramid-tiovx, width=[320, 640], format=GRAY16_LE ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, levels=40 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, scale=0.1 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, width=1024, height=1024, scale=0.25, levels=6 ! fakesink",
  NULL,
};

static const gchar *pipelines_caps_negotiation_success[] = {
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=640 ! tiovxpyramid ! application/x-pyramid-tiovx, height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=[320, 640] ! tiovxpyramid ! application/x-pyramid-tiovx, height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=[320, 640] ! tiovxpyramid ! application/x-pyramid-tiovx, width=480, height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=[320, 640] ! tiovxpyramid ! application/x-pyramid-tiovx, width=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, height=320 ! tiovxpyramid ! application/x-pyramid-tiovx, width=[320, 640] ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, scale=0.25 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, levels=1, scale=0.25 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, format=GRAY8 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, format=GRAY16_LE ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! tiovxpyramid ! application/x-pyramid-tiovx, levels=6, scale=0.9, format=GRAY8 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=640, format=GRAY8 ! tiovxpyramid ! application/x-pyramid-tiovx, height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=640, format=GRAY16_LE ! tiovxpyramid ! application/x-pyramid-tiovx, height=480 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=640, format={NV12,GRAY16_LE} ! tiovxpyramid ! application/x-pyramid-tiovx, height=480, format=GRAY8 ! fakesink",
  "videotestsrc is-live=true num-buffers=5 ! video/x-raw, width=640, format={NV12,GRAY16_LE} ! tiovxpyramid ! application/x-pyramid-tiovx, height=480, format=GRAY16_LE ! fakesink",
  NULL,
};

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
gst_tiovx_pyramid_suite (void)
{
  Suite *suite = suite_create ("tiovxpyramid");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_caps_negotiation_success);
  tcase_add_test (tc, test_caps_negotiation_fail);
  return suite;
}

GST_CHECK_MAIN (gst_tiovx_pyramid);
