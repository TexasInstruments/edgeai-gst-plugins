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

#include <gst-libs/gst/tiovx/gsttiovxallocator.h>
#include <gst-libs/gst/tiovx/gsttiovxpyramidbufferpool.h>
#include <gst-libs/gst/tiovx/gsttiovxpyramidmeta.h>
#include <gst-libs/gst/tiovx/gsttiovxbufferpoolutils.h>
#include <gst-libs/gst/tiovx/gsttiovxutils.h>
#include <gst/check/gstcheck.h>

#include <app_init.h>

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_pyramid_buffer_pool_test_category);

static const int kPyramidLevels = 3;
static const double kPyramidScale = 0.5f;
static const int kPyramidWidth = 1280;
static const int kPyramidHeight = 960;
static const vx_df_image kTIOVXPyramidFormat = VX_DF_IMAGE_U8;
static const char *kStrPyramidFormat = "GRAY8";
static const GstVideoFormat kGstPyramidFormat = GST_VIDEO_FORMAT_GRAY8;
static const int kSize = kPyramidWidth * kPyramidHeight * (1 + 1 / 4 + 1 / 16);
static const int kMinBuffers = 1;
static const int kMaxBuffers = 4;

static GstBufferPool *
get_pool (void)
{
  GstTIOVXBufferPool *tiovx_pool;

  if (0 != appCommonInit ()) {
    goto err_exit;
  }
  tivxInit ();
  tivxHostInit ();

  tiovx_pool = g_object_new (GST_TYPE_TIOVX_PYRAMID_BUFFER_POOL, NULL);

  return GST_BUFFER_POOL (tiovx_pool);

err_exit:
  return NULL;
}

GST_START_TEST (test_new_buffer)
{
  GstBufferPool *pool = get_pool ();
  GstBuffer *buf = NULL;
  GstTIOVXPyramidMeta *meta = NULL;
  gboolean ret = FALSE;
  vx_pyramid pyramid = NULL;
  vx_size query_levels = 0;
  vx_float32 query_scale = 0;
  gint query_width = 0, query_height = 0;
  vx_df_image query_format = VX_DF_IMAGE_VIRT;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_FAILURE;

  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("application/x-pyramid-tiovx",
      "levels", G_TYPE_INT, kPyramidLevels,
      "scale", G_TYPE_DOUBLE, kPyramidScale,
      "format", G_TYPE_STRING, kStrPyramidFormat,
      "width", G_TYPE_INT, kPyramidWidth,
      "height", G_TYPE_INT, kPyramidHeight,
      NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreatePyramid (context, kPyramidLevels, kPyramidScale,
      kPyramidWidth, kPyramidHeight, kTIOVXPyramidFormat);


  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);

  gst_buffer_pool_config_set_params (conf, caps, kSize, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  gst_caps_unref (caps);
  fail_if (FALSE == ret, "Buffer pool configuration failed");

  gst_buffer_pool_set_active (pool, TRUE);
  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);
  fail_if (NULL == buf, "No buffer has been returned");

  /* Check for a valid vx_pyramid */
  meta =
      (GstTIOVXPyramidMeta *) gst_buffer_get_meta (buf,
      GST_TYPE_TIOVX_PYRAMID_META_API);
  pyramid = (vx_pyramid) vxGetObjectArrayItem (meta->array, 0);

  vxQueryPyramid (pyramid, VX_PYRAMID_LEVELS,
      &query_levels, sizeof (query_levels));
  vxQueryPyramid (pyramid, VX_PYRAMID_SCALE,
      &query_scale, sizeof (query_scale));
  vxQueryPyramid (pyramid, VX_PYRAMID_WIDTH,
      &query_width, sizeof (query_width));
  vxQueryPyramid (pyramid, VX_PYRAMID_HEIGHT,
      &query_height, sizeof (query_height));
  vxQueryPyramid (pyramid, VX_PYRAMID_FORMAT,
      &query_format, sizeof (query_format));

  fail_if (kPyramidLevels != query_levels,
      "Stored vx_pyramid has the incorrect pyramid levels. Expected: %d\t Got: %ld",
      kPyramidLevels, query_levels);
  fail_if (fabs (kPyramidScale - query_scale) >= FLT_EPSILON,
      "Stored vx_pyramid has the incorrect pyramid scale. Expected: %f\t Got: %f",
      kPyramidScale, query_scale);
  fail_if (kPyramidWidth != query_width,
      "Stored vx_pyramid has the incorrect pyramid width. Expected: %d\t Got: %d",
      kPyramidWidth, query_width);
  fail_if (kPyramidHeight != query_height,
      "Stored vx_pyramid has the incorrect pyramid height. Expected: %d\t Got: %d",
      kPyramidHeight, query_height);
  fail_if (kGstPyramidFormat != vx_format_to_gst_format (query_format),
      "Stored vx_pyramid has the incorrect pyramid format");

  fail_if (gst_buffer_get_size (buf) !=
      gst_tiovx_get_size_from_exemplar (reference),
      "Buffer size does not match exemplar size");
  gst_buffer_unref (buf);
  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_empty_caps)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context;
  vx_reference reference;
  vx_status status;

  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = NULL;

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreatePyramid (context, kPyramidLevels, kPyramidScale,
      kPyramidWidth, kPyramidHeight, kTIOVXPyramidFormat);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, kSize, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Config didn't ignore empty caps");

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_invalid_caps)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context;
  vx_reference reference;
  vx_status status;

  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("application/x-pyramid-tiovx",
      "levels", G_TYPE_INT, kPyramidLevels,
      "scale", G_TYPE_DOUBLE, kPyramidScale,
      "format", G_TYPE_STRING, "UNKNOWN",
      "width", G_TYPE_INT, kPyramidWidth,
      "height", G_TYPE_INT, kPyramidHeight,
      NULL);


  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreatePyramid (context, kPyramidLevels, kPyramidScale,
      kPyramidWidth, kPyramidHeight, kTIOVXPyramidFormat);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, kSize, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Config didn't ignore empty caps");

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_no_set_params)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context;
  vx_reference reference;
  vx_status status;

  GstStructure *conf = gst_buffer_pool_get_config (pool);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreatePyramid (context, kPyramidLevels, kPyramidScale,
      kPyramidWidth, kPyramidHeight, kTIOVXPyramidFormat);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Set config didn't ignore empty params");

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_empty_exemplar)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;

  GstStructure *conf = gst_buffer_pool_get_config (pool);

  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Set config didn't ignore empty exemplar");
}

GST_END_TEST;

GST_START_TEST (test_create_new_pool)
{
  GstBufferPool *pool = NULL;
  vx_context context;
  vx_reference reference;
  vx_status status;

  fail_if (0 != appCommonInit (), "Failed to run common init");
  tivxInit ();
  tivxHostInit ();

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_pyramid_buffer_pool_test_category,
      "tiovxpyramidbufferpooltest", 0, "TIOVX PyramidBufferPool test");

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference =
      (vx_reference) vxCreatePyramid (context, kPyramidLevels, kPyramidScale,
      kPyramidWidth, kPyramidHeight, kTIOVXPyramidFormat);
  pool =
      gst_tiovx_create_new_pool (gst_tiovx_pyramid_buffer_pool_test_category,
      reference);

  fail_if (!GST_TIOVX_IS_PYRAMID_BUFFER_POOL (pool),
      "Pool is not of pyramid type");

  gst_object_unref (pool);
  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

static Suite *
gst_tiovx_pyramid_buffer_pool_suite (void)
{
  Suite *s = suite_create ("GstTIOVXPyramidBufferPool");
  TCase *tc_chain = tcase_create ("tiovx_pyramid_buffer_pool tests");

  tcase_set_timeout (tc_chain, 0);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_new_buffer);
  tcase_add_test (tc_chain, test_new_buffer_empty_caps);
  tcase_add_test (tc_chain, test_new_buffer_invalid_caps);
  tcase_add_test (tc_chain, test_new_buffer_no_set_params);
  tcase_add_test (tc_chain, test_new_buffer_empty_exemplar);
  tcase_add_test (tc_chain, test_create_new_pool);

  return s;
}

GST_CHECK_MAIN (gst_tiovx_pyramid_buffer_pool);
