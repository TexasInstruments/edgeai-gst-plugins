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
#include <gst-libs/gst/tiovx/gsttiovxbufferpoolutils.h>
#include <gst-libs/gst/tiovx/gsttiovximagebufferpool.h>
#include <gst-libs/gst/tiovx/gsttiovxutils.h>

#include <gst/check/gstcheck.h>
#include <TI/tivx.h>
/* App init has to be after tiovx.h */
#include <app_init.h>


static const int kImageWidth = 640;
static const int kImageHeight = 480;
static const vx_df_image kTIOVXImageFormat = VX_DF_IMAGE_UYVY;
static const char *kGstImageFormat = "UYVY";

static const int kMinBuffers = 1;
static const int kMaxBuffers = 4;

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

static void
init (GstTIOVXPad ** pad, vx_context * context, vx_reference * reference)
{
  vx_status status;

  *pad = g_object_new (GST_TYPE_TIOVX_PAD, NULL);
  fail_if (!*pad, "Unable to create a TIOVX pad");

  *context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) * context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  *reference =
      (vx_reference) vxCreateImage (*context, kImageWidth, kImageHeight,
      kTIOVXImageFormat);

  gst_tiovx_pad_set_exemplar (*pad, *reference);
}

static void
deinit (GstTIOVXPad * pad, vx_context context, vx_reference reference)
{
  gst_object_unref (pad);

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

static void
query_allocation (GstTIOVXPad * pad)
{
  GstCaps *caps = NULL;
  GstQuery *query = NULL;
  gint npool = 0;
  gboolean found_tiovx_pool = FALSE;
  gboolean ret = FALSE;

  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, kGstImageFormat,
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight, NULL);
  query = gst_query_new_allocation (caps, TRUE);

  ret = gst_tiovx_pad_query (GST_PAD (pad), NULL, query);
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

  gst_caps_unref (caps);
}

static void
initialize_tiovx_buffer_pool (GstBufferPool ** buffer_pool)
{
  GstStructure *conf = NULL;
  GstCaps *caps = NULL;
  GstVideoInfo info;
  gboolean ret = FALSE;
  vx_context context;
  vx_status status;
  vx_reference reference;

  *buffer_pool = g_object_new (GST_TYPE_TIOVX_IMAGE_BUFFER_POOL, NULL);

  conf = gst_buffer_pool_get_config (*buffer_pool);
  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, kGstImageFormat,
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight, NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  ret = gst_video_info_from_caps (&info, caps);
  fail_if (!ret, "Unable to get video info from caps");

  reference =
      (vx_reference) vxCreateImage (context, kImageWidth, kImageHeight,
      kTIOVXImageFormat);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);

  gst_buffer_pool_config_set_params (conf, caps, GST_VIDEO_INFO_SIZE (&info),
      kMinBuffers, kMaxBuffers);
  ret = gst_buffer_pool_set_config (*buffer_pool, conf);
  gst_caps_unref (caps);
  fail_if (FALSE == ret, "Buffer pool configuration failed");

  ret = gst_buffer_pool_set_active (*buffer_pool, TRUE);
  fail_if (FALSE == ret, "Can't set the pool to active");
}

static void
initialize_non_tiovx_buffer_pool (GstBufferPool ** buffer_pool)
{
  GstStructure *conf = NULL;
  GstCaps *caps = NULL;
  GstVideoInfo info;
  gboolean ret = FALSE;

  *buffer_pool = g_object_new (GST_TYPE_BUFFER_POOL, NULL);

  conf = gst_buffer_pool_get_config (*buffer_pool);
  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, kGstImageFormat,
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight, NULL);

  ret = gst_video_info_from_caps (&info, caps);
  fail_if (!ret, "Unable to get video info from caps");

  gst_buffer_pool_config_set_params (conf, caps, GST_VIDEO_INFO_SIZE (&info),
      kMinBuffers, kMaxBuffers);
  ret = gst_buffer_pool_set_config (*buffer_pool, conf);
  gst_caps_unref (caps);
  fail_if (FALSE == ret, "Buffer pool configuration failed");

  ret = gst_buffer_pool_set_active (*buffer_pool, TRUE);
  fail_if (FALSE == ret, "Can't set the pool to active");
}

static void
start_pad (GstTIOVXPad * pad)
{
  GstEvent *event = NULL;
  GstSegment *segment = NULL;
  gboolean ret = FALSE;

  ret = gst_pad_set_active (GST_PAD (pad), TRUE);
  fail_if (FALSE == ret, "Unable to set pad to active");

  event = gst_event_new_stream_start ("stream_id");

  ret = gst_pad_send_event (GST_PAD (pad), event);
  fail_if (FALSE == ret, "Unable to send start event to the pad");

  segment = gst_segment_new ();
  segment->format = GST_FORMAT_DEFAULT;

  event = gst_event_new_segment (segment);

  ret = gst_pad_send_event (GST_PAD (pad), event);
  fail_if (FALSE == ret, "Unable to send segment event to the pad");
}


GST_START_TEST (test_query_sink_allocation)
{
  GstTIOVXPad *pad = NULL;
  vx_context context;
  vx_reference reference;


  fail_if (!start_tiovx (), "Unable to initialize TIOVX");

  init (&pad, &context, &reference);

  query_allocation (pad);

  deinit (pad, context, reference);
}

GST_END_TEST;

GST_START_TEST (test_peer_query_allocation)
{
  GstCaps *caps = NULL;
  GstTIOVXPad *src_pad = NULL, *sink_pad = NULL;
  GstPadLinkReturn pad_link_return = GST_PAD_LINK_REFUSED;
  gboolean ret = FALSE;
  vx_context context;
  vx_reference reference;

  fail_if (!start_tiovx (), "Unable to initialize TIOVX");

  init (&sink_pad, &context, &reference);
  init (&src_pad, &context, &reference);

  GST_PAD_DIRECTION (sink_pad) = GST_PAD_SINK;
  GST_PAD_DIRECTION (src_pad) = GST_PAD_SRC;

  /* Perform allocation in the sink pad */
  gst_pad_set_query_function (GST_PAD (sink_pad), gst_tiovx_pad_query);

  pad_link_return = gst_pad_link (GST_PAD (src_pad), GST_PAD (sink_pad));
  fail_if (GST_PAD_LINK_OK != pad_link_return,
      "Unable to link src to sink pad");

  caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, kGstImageFormat,
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight, NULL);

  ret = gst_tiovx_pad_peer_query_allocation (src_pad, caps);
  fail_if (!ret, "Trigger pad failed");

  gst_caps_unref (caps);
  deinit (src_pad, context, reference);
  deinit (sink_pad, context, reference);
}

GST_END_TEST;

GST_START_TEST (test_push_tiovx_buffer)
{
  GstBuffer *buf = NULL;
  GstTIOVXPad *pad = NULL;
  GstFlowReturn flow_return = GST_FLOW_ERROR;
  vx_context context;
  vx_reference reference;

  fail_if (!start_tiovx (), "Unable to initialize TIOVX");

  init (&pad, &context, &reference);

  GST_PAD_DIRECTION (pad) = GST_PAD_SINK;

  query_allocation (pad);

  gst_tiovx_pad_acquire_buffer (pad, &buf, NULL);
  fail_if (NULL == buf, "No buffer has been returned");

  start_pad (pad);

  flow_return = gst_tiovx_pad_chain (GST_PAD (pad), NULL, &buf);
  fail_if (GST_FLOW_OK != flow_return, "Pushing buffer to pad failed: %d",
      flow_return);

  deinit (pad, context, reference);
}

GST_END_TEST;

GST_START_TEST (test_push_tiovx_buffer_external_pool)
{
  GstBufferPool *pool = NULL;
  GstBuffer *buf = NULL;
  GstTIOVXPad *pad = NULL;
  GstFlowReturn flow_return = GST_FLOW_ERROR;
  vx_context context;
  vx_reference reference;

  fail_if (!start_tiovx (), "Unable to initialize TIOVX");

  init (&pad, &context, &reference);
  initialize_tiovx_buffer_pool (&pool);

  GST_PAD_DIRECTION (pad) = GST_PAD_SINK;

  query_allocation (pad);

  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);
  fail_if (NULL == buf, "No buffer has been returned");

  start_pad (pad);

  flow_return = gst_tiovx_pad_chain (GST_PAD (pad), NULL, &buf);
  fail_if (GST_FLOW_OK != flow_return, "Pushing buffer to pad failed: %d",
      flow_return);

  deinit (pad, context, reference);
}

GST_END_TEST;

GST_START_TEST (test_push_non_tiovx_buffer)
{
  GstBufferPool *pool = NULL;
  GstBuffer *buf = NULL;
  GstTIOVXPad *pad = NULL;
  GstFlowReturn flow_return = GST_FLOW_ERROR;
  vx_context context;
  vx_reference reference;

  fail_if (!start_tiovx (), "Unable to initialize TIOVX");

  init (&pad, &context, &reference);
  initialize_non_tiovx_buffer_pool (&pool);

  GST_PAD_DIRECTION (pad) = GST_PAD_SINK;

  query_allocation (pad);

  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);
  fail_if (NULL == buf, "No buffer has been returned");

  start_pad (pad);

  flow_return = gst_tiovx_pad_chain (GST_PAD (pad), NULL, &buf);
  fail_if (GST_FLOW_OK != flow_return, "Pushing buffer to pad failed: %d",
      flow_return);

  deinit (pad, context, reference);
}

GST_END_TEST;

static Suite *
gst_tiovx_pad_suite (void)
{
  Suite *s = suite_create ("GstTIOVXPad");
  TCase *tc_chain = tcase_create ("gsttiovxpad tests");

  gst_tiovx_init_debug ();

  tcase_set_timeout (tc_chain, 0);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_query_sink_allocation);
  tcase_add_test (tc_chain, test_peer_query_allocation);
  tcase_add_test (tc_chain, test_push_tiovx_buffer);
  tcase_add_test (tc_chain, test_push_tiovx_buffer_external_pool);
  tcase_add_test (tc_chain, test_push_non_tiovx_buffer);

  return s;
}

GST_CHECK_MAIN (gst_tiovx_pad);
