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

#include <app_scaler_module.h>
#include <gst/check/gstharness.h>
#include <gst/check/gstcheck.h>
#include <gst/video/video-format.h>

#include <gst-libs/gst/tiovx/gsttiovxallocator.h>
#include <gst-libs/gst/tiovx/gsttiovxbufferpool.h>
#include <gst-libs/gst/tiovx/gsttiovxmeta.h>

#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "test_utils.h"

#include "app_init.h"

static const gchar *test_pipelines[] = {
  /* start pipeline */
  "videotestsrc ! tiovxmultiscaler ! fakesink",
  NULL,
};

enum
{
  /* Pipelines names */
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES,
};

GST_START_TEST (test_playing_to_null_multiple_times)
{
  test_states_change (test_pipelines[TEST_PLAYING_TO_NULL_MULTIPLE_TIMES]);
}

GST_END_TEST;

GST_START_TEST (test_init_module)
{
  GstElement *element = NULL;
  GstTIOVXSimoClass *simo_class = NULL;
  GstTIOVXSimo *simo = NULL;
  vx_context context = NULL;
  GstVideoInfo in_info = { };
  GstVideoInfo out_info = { };
  guint in_pool_size = 0;
  GHashTable *out_pool_sizes = NULL;
  vx_status status = VX_SUCCESS;
  gint ret = 0;
  guint i = 0;
  gint num_pads = 0;
  GstCaps *in_caps = NULL;
  GstCaps *out_caps = NULL;

  gst_init (NULL, NULL);
  element = gst_element_factory_make ("tiovxmultiscaler", "tiovxmultiscaler");

  simo_class = GST_TIOVX_SIMO_GET_CLASS (element);
  simo = GST_TIOVX_SIMO (element);
  in_pool_size = g_random_int_range (MIN_POOL_SIZE, MAX_POOL_SIZE);
  out_pool_sizes = g_hash_table_new (NULL, NULL);

  num_pads = gst_tiovx_simo_get_num_pads (simo);

  for (i = 0; i < num_pads; i++) {
    g_hash_table_insert (out_pool_sizes,
        GUINT_TO_POINTER (i), GUINT_TO_POINTER (MIN_POOL_SIZE));
  }
  ret = appCommonInit ();
  g_assert_true (0 == ret);

  tivxInit ();
  tivxHostInit ();
  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  g_assert_true (VX_SUCCESS == status);

  gst_video_info_init (&in_info);
  gst_video_info_init (&out_info);

  in_caps =
      gst_caps_from_string ("video/x-raw,width=1920,height=1080,format=NV12");
  out_caps =
      gst_caps_from_string ("video/x-raw,width=2040,height=1920,format=NV12");

  g_assert_true (gst_video_info_from_caps (&in_info, in_caps));
  g_assert_true (gst_video_info_from_caps (&out_info, out_caps));

  g_assert_true (GST_IS_TIOVX_SIMO_CLASS (simo_class));
  g_assert_true (GST_IS_TIOVX_SIMO (simo));

  /* Test the module init */
  ret =
      simo_class->init_module (simo, context, &in_info, &out_info, in_pool_size,
      out_pool_sizes);
  g_assert_true (ret);

  vxReleaseContext (&context);
  appCommonDeInit ();

  g_hash_table_unref (out_pool_sizes);
}

GST_END_TEST;

static Suite *
gst_state_suite (void)
{
  Suite *suite = suite_create ("tiovxmultiscaler");
  TCase *sucess_escenario = tcase_create ("sucess_escenario");

  suite_add_tcase (suite, sucess_escenario);
  tcase_skip_broken_test (sucess_escenario,
      test_playing_to_null_multiple_times);
  tcase_add_test (sucess_escenario, test_init_module);

  return suite;
}

GST_CHECK_MAIN (gst_state);
