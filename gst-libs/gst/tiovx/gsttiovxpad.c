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

#include "gsttiovxpad.h"

#include "gsttiovxbufferpool.h"
#include "gsttiovxmeta.h"
#include "gsttiovxutils.h"

#define MIN_BUFFER_POOL_SIZE 2
#define MAX_BUFFER_POOL_SIZE 16
#define DEFAULT_BUFFER_POOL_SIZE MIN_BUFFER_POOL_SIZE

enum
{
  PROP_BUFFER_POOL_SIZE = 1,
};

/**
 * SECTION:gsttiovxpad
 * @short_description: GStreamer pad for GstTIOVX based elements
 *
 * This class implements a GStreamer standard buffer pool for GstTIOVX
 * based elements.
 */

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_pad_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_pad_debug_category

typedef struct _GstTIOVXPadPrivate
{
  GstPad base;

  GstBufferPool *buffer_pool;

  vx_reference exemplar;
  guint pool_size;
} GstTIOVXPadPrivate;

G_DEFINE_TYPE_WITH_CODE (GstTIOVXPad, gst_tiovx_pad,
    GST_TYPE_PAD, G_ADD_PRIVATE (GstTIOVXPad)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_pad_debug_category,
        "tiovxpad", 0, "debug category for TIOVX pad class"));

/* prototypes */
static void gst_tiovx_pad_finalize (GObject * object);
static gboolean gst_tiovx_pad_configure_pool (GstTIOVXPad * self,
    GstCaps * caps, GstVideoInfo * info);
static void gst_tiovx_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_tiovx_pad_class_init (GstTIOVXPadClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gst_tiovx_pad_set_property;
  object_class->get_property = gst_tiovx_pad_get_property;

  g_object_class_install_property (object_class, PROP_BUFFER_POOL_SIZE,
      g_param_spec_uint ("pool-size", "Buffer pool size",
          "Size of the buffer pool",
          MIN_BUFFER_POOL_SIZE, MAX_BUFFER_POOL_SIZE, DEFAULT_BUFFER_POOL_SIZE,
          G_PARAM_READWRITE));

  object_class->finalize = gst_tiovx_pad_finalize;
}

static void
gst_tiovx_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXPad *self = GST_TIOVX_PAD (object);
  GstTIOVXPadPrivate *priv = NULL;

  GST_LOG_OBJECT (self, "set_property");

  priv = gst_tiovx_pad_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_BUFFER_POOL_SIZE:
      priv->pool_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXPad *self = GST_TIOVX_PAD (object);
  GstTIOVXPadPrivate *priv = NULL;

  GST_LOG_OBJECT (self, "get_property");

  priv = gst_tiovx_pad_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_BUFFER_POOL_SIZE:
      g_value_set_uint (value, priv->pool_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}


static void
gst_tiovx_pad_init (GstTIOVXPad * self)
{
  GstTIOVXPadPrivate *priv = NULL;

  priv = gst_tiovx_pad_get_instance_private (self);

  priv->buffer_pool = NULL;
  priv->exemplar = NULL;
  priv->pool_size = DEFAULT_BUFFER_POOL_SIZE;
}

void
gst_tiovx_pad_set_exemplar (GstTIOVXPad * self, const vx_reference exemplar)
{
  GstTIOVXPadPrivate *priv = NULL;

  priv = gst_tiovx_pad_get_instance_private (self);

  g_return_if_fail (self);
  g_return_if_fail (exemplar);

  priv->exemplar = exemplar;
}

gboolean
gst_tiovx_pad_peer_query_allocation (GstTIOVXPad * self, GstCaps * caps)
{
  GstTIOVXPadPrivate *priv = NULL;
  GstQuery *query = NULL;
  gint npool = 0;
  gboolean ret = FALSE;
  GstPad *peer = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (caps, FALSE);

  priv = gst_tiovx_pad_get_instance_private (self);

  query = gst_query_new_allocation (caps, TRUE);

  peer = gst_pad_get_peer (GST_PAD (self));
  ret = gst_pad_query (peer, query);

  if (!ret) {
    GST_INFO_OBJECT (self, "Unable to query pad peer");
  }
  gst_object_unref (peer);

  /* Always remove the current pool, we will either create a new one or get it from downstream */
  if (NULL != priv->buffer_pool) {
    gst_object_unref (priv->buffer_pool);
    priv->buffer_pool = NULL;
  }

  /* Look for the first TIOVX buffer if present */
  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (GST_TIOVX_IS_BUFFER_POOL (pool)) {
      priv->buffer_pool = pool;
      break;
    } else {
      gst_object_unref (pool);
    }
  }

  if (NULL == priv->buffer_pool) {
    GstVideoInfo info;

    priv->buffer_pool = g_object_new (GST_TIOVX_TYPE_BUFFER_POOL, NULL);

    ret = gst_tiovx_pad_configure_pool (self, caps, &info);

    if (!ret) {
      GST_ERROR_OBJECT (self, "Unable to configure pool");
      gst_object_unref (priv->buffer_pool);
      priv->buffer_pool = NULL;
      goto unref_query;
    }
  }

  ret = TRUE;

unref_query:
  gst_query_unref (query);

  return ret;
}

static gboolean
gst_tiovx_pad_process_allocation_query (GstTIOVXPad * self, GstQuery * query)
{
  GstTIOVXPadPrivate *priv = NULL;
  GstCaps *caps = NULL;
  GstVideoInfo info;
  gboolean ret = FALSE;

  priv = gst_tiovx_pad_get_instance_private (self);

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (query, FALSE);

  if (NULL == priv->exemplar) {
    GST_ERROR_OBJECT (self,
        "Cannot process allocation query without an exemplar");
    goto out;
  }

  if (NULL != priv->buffer_pool) {
    GST_DEBUG_OBJECT (self, "Freeing current pool");
    gst_object_unref (priv->buffer_pool);
    priv->buffer_pool = NULL;
  }

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_ERROR_OBJECT (self, "Unable to parse caps from query");
    ret = FALSE;
    goto out;
  }

  priv->buffer_pool = g_object_new (GST_TIOVX_TYPE_BUFFER_POOL, NULL);

  ret = gst_tiovx_pad_configure_pool (self, caps, &info);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to configure pool");
    gst_object_unref (priv->buffer_pool);
    priv->buffer_pool = NULL;
    goto out;
  }

  gst_query_add_allocation_pool (query,
      GST_BUFFER_POOL (priv->buffer_pool), GST_VIDEO_INFO_SIZE (&info),
      priv->pool_size, priv->pool_size);

  ret = TRUE;

out:
  return ret;
}

gboolean
gst_tiovx_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXPad *self = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (pad, ret);
  g_return_val_if_fail (query, ret);

  self = GST_TIOVX_PAD (pad);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
      GST_DEBUG_OBJECT (self, "Received allocation query");
      ret = gst_tiovx_pad_process_allocation_query (self, query);
      break;
    default:
      GST_DEBUG_OBJECT (self, "Received non-allocation query");
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

GstFlowReturn
gst_tiovx_pad_chain (GstPad * pad, GstObject * parent, GstBuffer ** buffer)
{
  GstTIOVXPad *self = NULL;
  GstTIOVXPadPrivate *priv = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstBuffer *tmp_buffer = NULL;
  GstCaps *caps = NULL;

  g_return_val_if_fail (pad, ret);
  g_return_val_if_fail (buffer, ret);
  g_return_val_if_fail (*buffer, ret);

  GST_LOG_OBJECT (pad, "Received a buffer for chaining");

  self = GST_TIOVX_PAD (pad);
  priv = gst_tiovx_pad_get_instance_private (self);

  tmp_buffer = *buffer;

  caps = gst_pad_get_current_caps (pad);
  *buffer =
      gst_tiovx_validate_tiovx_buffer (GST_CAT_DEFAULT, &priv->buffer_pool,
      *buffer, &priv->exemplar, caps, priv->pool_size);
  gst_caps_unref (caps);
  if (!*buffer) {
    GST_ERROR_OBJECT (pad, "Unable to validate buffer");
    goto exit;
  }

  if (tmp_buffer != *buffer) {
    gst_buffer_unref (tmp_buffer);
  }

  ret = GST_FLOW_OK;

exit:
  return ret;
}

GstFlowReturn
gst_tiovx_pad_acquire_buffer (GstTIOVXPad * self, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstTIOVXPadPrivate *priv = NULL;
  GstFlowReturn flow_return = GST_FLOW_ERROR;
  GstTIOVXMeta *meta = NULL;
  vx_object_array array = NULL;
  vx_reference image = NULL;

  g_return_val_if_fail (self, flow_return);
  g_return_val_if_fail (buffer, flow_return);

  priv = gst_tiovx_pad_get_instance_private (self);

  flow_return =
      gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (priv->buffer_pool),
      buffer, params);
  if (GST_FLOW_OK != flow_return) {
    GST_ERROR_OBJECT (self, "Unable to acquire buffer from pool: %d",
        flow_return);
    goto exit;
  }

  /* Ensure that the exemplar & the meta have the same data */
  meta =
      (GstTIOVXMeta *) gst_buffer_get_meta (*buffer, GST_TIOVX_META_API_TYPE);

  array = meta->array;

  /* Currently, we support only 1 vx_image per array */
  image = vxGetObjectArrayItem (array, 0);

  gst_tiovx_transfer_handle (GST_CAT_DEFAULT, image, priv->exemplar);

  vxReleaseReference (&image);
exit:
  return flow_return;
}

static gboolean
gst_tiovx_pad_configure_pool (GstTIOVXPad * self, GstCaps * caps,
    GstVideoInfo * info)
{
  GstTIOVXPadPrivate *priv = NULL;
  GstStructure *config = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, ret);
  g_return_val_if_fail (caps, ret);
  g_return_val_if_fail (info, ret);

  priv = gst_tiovx_pad_get_instance_private (self);

  if (!gst_video_info_from_caps (info, caps)) {
    GST_ERROR_OBJECT (self, "Unable to get video info from caps");
    return FALSE;
  }

  config = gst_buffer_pool_get_config (GST_BUFFER_POOL (priv->buffer_pool));

  gst_tiovx_buffer_pool_config_set_exemplar (config, priv->exemplar);
  gst_buffer_pool_config_set_params (config, caps, GST_VIDEO_INFO_SIZE (info),
      priv->pool_size, priv->pool_size);

  if (!gst_buffer_pool_set_config (GST_BUFFER_POOL (priv->buffer_pool), config)) {
    GST_ERROR_OBJECT (self, "Unable to set pool configuration");
    goto out;
  }

  gst_buffer_pool_set_active (GST_BUFFER_POOL (priv->buffer_pool), TRUE);

  ret = TRUE;

out:
  return ret;
}

static void
gst_tiovx_pad_finalize (GObject * object)
{
  GstTIOVXPad *tiovx_pad = GST_TIOVX_PAD (object);
  GstTIOVXPadPrivate *priv = NULL;

  priv = gst_tiovx_pad_get_instance_private (tiovx_pad);

  if (NULL != priv->buffer_pool) {
    gst_object_unref (priv->buffer_pool);
  }

  G_OBJECT_CLASS (gst_tiovx_pad_parent_class)->finalize (object);
}
