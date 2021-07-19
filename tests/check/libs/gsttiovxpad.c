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
 * *	Any redistribution and use are licensed by TI for use only with TI
 * Devices.
 *
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are
 * met:
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

#include <gst-libs/gst/tiovx/gsttiovxpad.h>
#include <gst-libs/gst/tiovx/gsttiovxbufferpool.h>
#include <gst/check/gstcheck.h>
#include <TI/tivx.h>
/* App init has to be after tiovx.h */
#include <app_init.h>


static const int kImageWidth = 640;
static const int kImageHeight = 480;
static const char *kGstImageFormat = "UYVY";

static gboolean
start_tiovx (void)
{
  gboolean ret = FALSE;

  if (0 != appCommonInit ()) {
    goto out;
  }
  tivxInit ();
  tivxHostInit ();

  ret = TRUE;

out:
  return ret;
}

static gboolean
test_notify_function (GstElement * element)
{
  return TRUE;
}

GST_START_TEST (test_query_sink_allocation)
{
  GstCaps *caps = NULL;
  GstQuery *query = NULL;
  GstTIOVXPad *pad = NULL;
  gboolean found_tiovx_pool = FALSE;
  gint npool = 0;
  gboolean ret = FALSE;

  fail_if (!start_tiovx (), "Unable to initialize TIOVX");

  pad = gst_tiovx_pad_new (GST_PAD_SINK);
  fail_if (!pad, "Unable to create a TIOVX pad");

  gst_tiovx_pad_install_notify (GST_PAD (pad), test_notify_function, NULL);

  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, kGstImageFormat,
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight, NULL);
  query = gst_query_new_allocation (caps, TRUE);

  ret = gst_pad_query (GST_PAD (pad), query);
  fail_if (!ret, "Unable to query pad");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (GST_TIOVX_IS_BUFFER_POOL (pool)) {
      found_tiovx_pool = TRUE;
      break;
    }
  }
  fail_if (!found_tiovx_pool, "Query returned without a pool");
}

GST_END_TEST;

static Suite *
gst_buffer_pool_suite (void)
{
  Suite *s = suite_create ("GstBufferPool");
  TCase *tc_chain = tcase_create ("buffer_pool tests");

  tcase_set_timeout (tc_chain, 0);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_query_sink_allocation);

  return s;
}

GST_CHECK_MAIN (gst_buffer_pool);
