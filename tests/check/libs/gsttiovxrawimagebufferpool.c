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

#include <gst-libs/gst/tiovx/gsttiovxallocator.h>
#include <gst-libs/gst/tiovx/gsttiovxbufferpoolutils.h>
#include <gst-libs/gst/tiovx/gsttiovxrawimagebufferpool.h>
#include <gst-libs/gst/tiovx/gsttiovxrawimagemeta.h>
#include <gst-libs/gst/tiovx/gsttiovxutils.h>
#include <gst/check/gstcheck.h>

#include <app_init.h>

static const gint kImageWidth = 640;
static const gint kImageHeight = 480;
static const gint kImageNumExposures = 1;
static const gboolean kImageLineInterleaved = FALSE;
static const gint kImageMetaHeightBefore = 20;
static const gint kImageMetaHeightAfter = 20;
static const char *kGstImageFormat = "rggb";

static const int kSize = kImageWidth * kImageHeight * 2;
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

  tiovx_pool = g_object_new (GST_TYPE_TIOVX_RAW_IMAGE_BUFFER_POOL, NULL);

  return GST_BUFFER_POOL (tiovx_pool);

err_exit:
  return NULL;
}

static vx_reference
create_raw_image (vx_context context)
{
  tivx_raw_image_create_params_t raw_image_params = { };
  tivx_raw_image_format_t TIOVXImageFormat = { };
  vx_reference reference = NULL;

  TIOVXImageFormat.pixel_container =
      gst_format_to_tivx_raw_format (kGstImageFormat);

  raw_image_params.width = kImageWidth;
  raw_image_params.height = kImageHeight;
  raw_image_params.num_exposures = kImageNumExposures;
  raw_image_params.line_interleaved = kImageLineInterleaved;
  raw_image_params.format[0] = TIOVXImageFormat;
  raw_image_params.meta_height_before = kImageMetaHeightBefore;
  raw_image_params.meta_height_after = kImageMetaHeightAfter;

  reference = (vx_reference) tivxCreateRawImage (context, &raw_image_params);

  return reference;
}

GST_START_TEST (test_new_buffer)
{
  GstBufferPool *pool = get_pool ();
  GstBuffer *buf = NULL;
  GstTIOVXRawImageMeta *meta = NULL;
  gboolean ret = FALSE;
  tivx_raw_image image = NULL;
  unsigned int img_width = 0, img_height = 0;
  vx_context context;
  vx_reference reference;
  vx_status status;

  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("video/x-bayer",
      "format", G_TYPE_STRING, kGstImageFormat,
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight,
      NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference = create_raw_image (context);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);

  gst_buffer_pool_config_set_params (conf, caps, kSize, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  gst_caps_unref (caps);
  fail_if (FALSE == ret, "Buffer pool configuration failed");

  gst_buffer_pool_set_active (pool, TRUE);
  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);
  fail_if (NULL == buf, "No buffer has been returned");

  /* Check for a valid vx_image */
  meta =
      (GstTIOVXRawImageMeta *) gst_buffer_get_meta (buf,
      GST_TYPE_TIOVX_RAW_IMAGE_META_API);
  image = (tivx_raw_image) vxGetObjectArrayItem (meta->array, 0);

  tivxQueryRawImage ((tivx_raw_image) image, TIVX_RAW_IMAGE_WIDTH,
      &img_width, sizeof (img_width));
  tivxQueryRawImage ((tivx_raw_image) image, TIVX_RAW_IMAGE_HEIGHT,
      &img_height, sizeof (img_height));
  fail_if (kImageWidth != img_width,
      "Stored vx_image has the incorrect image width. Expected: %ud\t Got: %ud",
      kImageWidth, img_width);
  fail_if (kImageHeight != img_height,
      "Stored vx_height has the incorrect image height. Expected: %ud\t Got: %ud",
      kImageHeight, img_height);

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
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_FAILURE;

  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = NULL;


  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference = create_raw_image (context);

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
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_FAILURE;

  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "UNKNOWN",
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight,
      NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference = create_raw_image (context);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, kSize, kMinBuffers,
      kMaxBuffers);
  ret = gst_buffer_pool_set_config (pool, conf);
  fail_if (ret == TRUE, "Config didn't ignore invalid caps");

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

GST_START_TEST (test_new_buffer_no_set_params)
{
  GstBufferPool *pool = get_pool ();
  gboolean ret = FALSE;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_FAILURE;

  GstStructure *conf = gst_buffer_pool_get_config (pool);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference = create_raw_image (context);

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

GST_START_TEST (test_external_allocator)
{
  GstBufferPool *pool = get_pool ();
  GstAllocator *allocator = g_object_new (GST_TYPE_TIOVX_ALLOCATOR, NULL);
  GstBuffer *buf = NULL;
  GstTIOVXRawImageMeta *meta = NULL;
  tivx_raw_image image = NULL;
  unsigned int img_width = 0, img_height = 0;
  gboolean ret = FALSE;
  vx_context context = NULL;
  vx_reference reference = NULL;
  vx_status status = VX_FAILURE;

  GstStructure *conf = gst_buffer_pool_get_config (pool);
  GstCaps *caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, kGstImageFormat,
      "width", G_TYPE_INT, kImageWidth,
      "height", G_TYPE_INT, kImageHeight,
      NULL);

  context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) context);
  fail_if (VX_SUCCESS != status, "Failed to create context");

  reference = create_raw_image (context);

  gst_tiovx_buffer_pool_config_set_exemplar (conf, reference);
  gst_buffer_pool_config_set_params (conf, caps, kSize, kMinBuffers,
      kMaxBuffers);
  gst_buffer_pool_config_set_allocator (conf, allocator, NULL);
  ret = gst_buffer_pool_set_config (pool, conf);

  fail_if (FALSE == ret,
      "Bufferpool configuration failed with external allocator");

  gst_caps_unref (caps);
  gst_object_unref (allocator);

  gst_buffer_pool_set_active (pool, TRUE);
  gst_buffer_pool_acquire_buffer (pool, &buf, NULL);
  fail_if (NULL == buf, "No buffer has been returned");

  /* Check for a valid vx_image */
  meta =
      (GstTIOVXRawImageMeta *) gst_buffer_get_meta (buf,
      GST_TYPE_TIOVX_RAW_IMAGE_META_API);
  image = (tivx_raw_image) vxGetObjectArrayItem (meta->array, 0);

  tivxQueryRawImage ((tivx_raw_image) image, TIVX_RAW_IMAGE_WIDTH,
      &img_width, sizeof (img_width));
  tivxQueryRawImage ((tivx_raw_image) image, TIVX_RAW_IMAGE_HEIGHT,
      &img_height, sizeof (img_height));
  fail_if (kImageWidth != img_width,
      "Stored vx_image has the incorrect image width. Expected: %ud\t Got: %ud",
      kImageWidth, img_width);
  fail_if (kImageHeight != img_height,
      "Stored vx_height has the incorrect image height. Expected: %ud\t Got: %ud",
      kImageHeight, img_height);

  gst_buffer_unref (buf);
  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);

  vxReleaseReference (&reference);
  vxReleaseContext (&context);
}

GST_END_TEST;

static Suite *
gst_tiovx_buffer_pool_suite (void)
{
  Suite *s = suite_create ("GstTIOVXBufferPool");
  TCase *tc_chain = tcase_create ("tiovx_buffer_pool tests");

  tcase_set_timeout (tc_chain, 0);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_new_buffer);
  tcase_add_test (tc_chain, test_new_buffer_empty_caps);
  tcase_add_test (tc_chain, test_new_buffer_invalid_caps);
  tcase_add_test (tc_chain, test_new_buffer_no_set_params);
  tcase_add_test (tc_chain, test_new_buffer_empty_exemplar);
  tcase_add_test (tc_chain, test_external_allocator);

  return s;
}

GST_CHECK_MAIN (gst_tiovx_buffer_pool);
