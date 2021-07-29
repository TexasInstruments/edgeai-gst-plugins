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

#include "ext/tiovx/gsttiovxmultiscaler.h"

#include <app_init.h>
#include <app_scaler_module.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/video/video-format.h>

#include "gst-libs/gst/tiovx/gsttiovxallocator.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferpool.h"
#include "gst-libs/gst/tiovx/gsttiovxmeta.h"

#include "app_scaler_module.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "test_utils.h"

static const gchar *test_pipelines[] = {
  "videotestsrc is-live=true ! video/x-raw,format=NV12,width=1280,height=720 ! tiovxmultiscaler name=multi",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink async=false",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink async=false multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink async=false",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink async=false multi.src_1"
      "! video/x-raw,width=640,height=480! fakesink async=false multi.src_2"
      "! video/x-raw,width=320,height=240 ! queue ! fakesink async=false",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink async=false multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink async=false multi.src_2"
      "! video/x-raw,width=320,height=240 ! queue ! fakesink async=false multi.src_3"
      "! video/x-raw,width=640,height=480 ! queue ! fakesink async=false",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink async=false multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink async=false multi.src_2"
      "! video/x-raw,width=320,height=240 ! queue ! fakesink async=false multi.src_3"
      "! video/x-raw,width=640,height=480 ! queue ! fakesink async=false multi.src_4"
      "! video/x-raw,width=1280,height=720 ! queue ! fakesink async=false",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=1280,height=720 ! fakesink async=false multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink async=false multi.src_2"
      "! video/x-raw,width=320,height=240 ! queue ! fakesink async=false multi.src_3"
      "! video/x-raw,width=640,height=480 ! queue ! fakesink async=false multi.src_4"
      "! video/x-raw,width=1280,height=720 ! queue ! fakesink async=false multi.src_5"
      "! video/x-raw,width=1280,height=720",
  "videotestsrc is-live=true ! video/x-raw,width=1920,height=1080 ! tiovxmultiscaler name=multi multi.src_0"
      "! video/x-raw,width=3080,height=2040 ! fakesink async=false multi.src_1"
      "! video/x-raw,width=640,height=480 ! fakesink async=false",
  NULL,
};

enum
{
  /* Pipelines names */
  TEST_ZERO_PADS,
  TEST_ONE_PAD,
  TEST_TWO_PADS,
  TEST_THREE_PADS,
  TEST_FOUR_PADS,
  TEST_FIVE_PADS,
  TEST_SIX_PADS,
  TEST_UPSCALING,
};

GST_START_TEST (test_pads)
{
  test_states_change_success (test_pipelines[TEST_ONE_PAD]);

  test_states_change_success (test_pipelines[TEST_TWO_PADS]);

  test_states_change_success (test_pipelines[TEST_THREE_PADS]);

  test_states_change_success (test_pipelines[TEST_FOUR_PADS]);

  test_states_change_success (test_pipelines[TEST_FIVE_PADS]);
}

GST_END_TEST;

GST_START_TEST (test_pads_fail)
{
  test_states_change_fail (test_pipelines[TEST_ZERO_PADS]);

  test_states_change_fail (test_pipelines[TEST_SIX_PADS]);
}

GST_END_TEST;

GST_START_TEST (test_upscaling)
{
  test_states_change_fail (test_pipelines[TEST_UPSCALING]);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("tiovxmultiscaler");
  TCase *sucess_escenario = tcase_create ("sucess_escenario");

  suite_add_tcase (suite, sucess_escenario);
  tcase_add_test (sucess_escenario, test_pads);
  tcase_skip_broken_test (sucess_escenario, test_pads_fail);
  tcase_skip_broken_test (sucess_escenario, test_upscaling);

  return suite;
}

GST_CHECK_MAIN (gst_state);
