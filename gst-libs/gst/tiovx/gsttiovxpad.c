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
#include "gsttiovxbufferpoolutils.h"
#include "gsttiovxbufferutils.h"
#include "gsttiovxutils.h"

#define DEFAULT_NUM_CHANNELS 1
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

  vx_object_array array;
  vx_reference exemplar;
  gint graph_param_id;
  gint node_param_id;

  guint pool_size;
  gint num_channels;
} GstTIOVXPadPrivate;

G_DEFINE_TYPE_WITH_CODE (GstTIOVXPad, gst_tiovx_pad,
    GST_TYPE_PAD, G_ADD_PRIVATE (GstTIOVXPad)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_pad_debug_category,
        "tiovxpad", 0, "debug category for TIOVX pad class"));

/* prototypes */
static void gst_tiovx_pad_finalize (GObject * object);
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
  GstTIOVXPadPrivate *priv = gst_tiovx_pad_get_instance_private (self);

  GST_LOG_OBJECT (self, "set_property");

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
  GstTIOVXPadPrivate *priv = gst_tiovx_pad_get_instance_private (self);

  GST_LOG_OBJECT (self, "get_property");

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
  GstTIOVXPadPrivate *priv = gst_tiovx_pad_get_instance_private (self);

  priv->buffer_pool = NULL;
  priv->array = NULL;
  priv->exemplar = NULL;
  priv->graph_param_id = -1;
  priv->node_param_id = -1;
  priv->pool_size = DEFAULT_BUFFER_POOL_SIZE;
  priv->num_channels = DEFAULT_NUM_CHANNELS;
}

void
gst_tiovx_pad_set_exemplar (GstTIOVXPad * self, vx_reference exemplar)
{
  GstTIOVXPadPrivate *priv = NULL;

  g_return_if_fail (self);

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
  GstBufferPool *pool = NULL;
  gint num_channels = DEFAULT_NUM_CHANNELS;

  g_return_val_if_fail (self, ret);
  g_return_val_if_fail (caps, ret);

  priv = gst_tiovx_pad_get_instance_private (self);

  query = gst_query_new_allocation (caps, TRUE);

  peer = gst_pad_get_peer (GST_PAD (self));
  ret = gst_pad_query (peer, query);

  if (!ret) {
    GST_INFO_OBJECT (self, "Unable to query pad peer");
  }
  gst_object_unref (peer);

  if (!gst_structure_get_int (gst_caps_get_structure (caps, 0), "num-channels",
          &num_channels)) {
    num_channels = DEFAULT_NUM_CHANNELS;
  }
  priv->num_channels = num_channels;

  /* Look for the first TIOVX buffer if present */
  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (NULL == pool) {
      GST_DEBUG_OBJECT (self, "No pool in query position: %d, ignoring", npool);
    } else if (GST_TIOVX_IS_BUFFER_POOL (pool)) {
      break;
    } else {
      gst_object_unref (pool);
      pool = NULL;
    }
  }

  if (NULL == pool) {
    gsize size = 0;

    size = gst_tiovx_get_size_from_exemplar (priv->exemplar);
    if (0 >= size) {
      GST_ERROR_OBJECT (self, "Failed to get size from exemplar");
      goto unref_query;
    }

    ret =
        gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, priv->pool_size,
        priv->exemplar, size, priv->num_channels, &pool);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Unable to configure pool");
      goto unref_query;
    }
  }

  /* Unref the old stored pool */
  if (NULL != priv->buffer_pool) {
    gst_object_unref (priv->buffer_pool);
  }
  /* Assign the new pool to the internal value */
  priv->buffer_pool = pool;

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
  gboolean ret = FALSE;
  gsize size = 0;
  gint num_channels = DEFAULT_NUM_CHANNELS;

  g_return_val_if_fail (self, ret);
  g_return_val_if_fail (query, ret);

  priv = gst_tiovx_pad_get_instance_private (self);

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
  if (NULL == caps) {
    GST_ERROR_OBJECT (self, "Unable to parse caps from query");
    ret = FALSE;
    goto out;
  }

  size = gst_tiovx_get_size_from_exemplar (priv->exemplar);
  if (0 >= size) {
    GST_ERROR_OBJECT (self, "Failed to get size from input");
    ret = FALSE;
    goto out;
  }

  if (!gst_structure_get_int (gst_caps_get_structure (caps, 0), "num-channels",
          &num_channels)) {
    num_channels = DEFAULT_NUM_CHANNELS;
  }
  priv->num_channels = num_channels;

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, priv->pool_size,
      priv->exemplar, size, priv->num_channels, &priv->buffer_pool);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to configure pool");
    goto out;
  }

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
      *buffer, priv->exemplar, caps, priv->pool_size, priv->num_channels);

  if (caps) {
    gst_caps_unref (caps);
  }

  if (NULL == *buffer) {
    GST_ERROR_OBJECT (pad, "Unable to validate buffer");
    goto exit;
  }

  if ((tmp_buffer != *buffer) && (NULL != tmp_buffer)) {
    gst_buffer_unref (tmp_buffer);
    tmp_buffer = NULL;
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
  vx_object_array array = NULL;
  vx_reference image = NULL;
  vx_status status = VX_FAILURE;
  vx_size num_channels = 0;
  guint i = 0;

  g_return_val_if_fail (self, flow_return);
  g_return_val_if_fail (buffer, flow_return);

  priv = gst_tiovx_pad_get_instance_private (self);

  if (!gst_buffer_pool_set_active (GST_BUFFER_POOL (priv->buffer_pool), TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
    flow_return = GST_FLOW_ERROR;
    goto exit;
  }
  flow_return =
      gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (priv->buffer_pool),
      buffer, params);
  if (GST_FLOW_OK != flow_return) {
    GST_ERROR_OBJECT (self, "Unable to acquire buffer from pool: %d",
        flow_return);
    goto exit;
  }

  /* Ensure that the exemplar & the meta have the same data */
  array =
      gst_tiovx_get_vx_array_from_buffer (GST_CAT_DEFAULT, priv->exemplar,
      *buffer);

  status =
      vxQueryObjectArray (array, VX_OBJECT_ARRAY_NUMITEMS, &num_channels,
      sizeof (num_channels));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Query of number of channels in input buffer failed %" G_GINT32_FORMAT,
        status);
    goto exit;
  }

  if (num_channels != priv->num_channels) {
    GST_ERROR_OBJECT (self, "Incompatible number of channels received");
    goto exit;
  }

  for (i = 0; i < num_channels; i++) {
    vx_reference gst_ref = NULL;
    vx_reference modules_ref = NULL;

    gst_ref = vxGetObjectArrayItem (array, i);
    modules_ref = vxGetObjectArrayItem (priv->array, i);

    status = gst_tiovx_transfer_handle (GST_CAT_DEFAULT, gst_ref, modules_ref);
    vxReleaseReference (&gst_ref);
    vxReleaseReference (&modules_ref);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Error in input handle transfer %" G_GINT32_FORMAT, status);
      goto exit;
    }
  }

  vxReleaseReference (&image);
exit:
  return flow_return;
}

vx_reference
gst_tiovx_pad_get_exemplar (GstTIOVXPad * pad)
{
  GstTIOVXPadPrivate *priv = NULL;

  g_return_val_if_fail (pad, NULL);

  priv = gst_tiovx_pad_get_instance_private (pad);
  return priv->exemplar;
}

void
gst_tiovx_pad_set_params (GstTIOVXPad * pad, vx_object_array array,
    vx_reference reference, gint graph_param_id, gint node_param_id)
{
  GstTIOVXPadPrivate *priv = NULL;

  g_return_if_fail (pad);
  g_return_if_fail (array);
  g_return_if_fail (reference);

  priv = gst_tiovx_pad_get_instance_private (pad);

  GST_OBJECT_LOCK (pad);
  priv->array = array;
  priv->exemplar = reference;
  priv->graph_param_id = graph_param_id;
  priv->node_param_id = node_param_id;

  GST_OBJECT_UNLOCK (pad);
}

void
gst_tiovx_pad_get_params (GstTIOVXPad * pad, vx_object_array * array,
    vx_reference ** reference, gint * graph_param_id, gint * node_param_id)
{
  GstTIOVXPadPrivate *priv = NULL;

  g_return_if_fail (pad);
  g_return_if_fail (array);
  g_return_if_fail (reference);
  g_return_if_fail (graph_param_id);
  g_return_if_fail (node_param_id);

  priv = gst_tiovx_pad_get_instance_private (pad);

  GST_OBJECT_LOCK (pad);

  *array = priv->array;
  *reference = &priv->exemplar;
  *graph_param_id = priv->graph_param_id;
  *node_param_id = priv->node_param_id;

  GST_OBJECT_UNLOCK (pad);
}

static void
gst_tiovx_pad_finalize (GObject * object)
{
  GstTIOVXPad *self = GST_TIOVX_PAD (object);
  GstTIOVXPadPrivate *priv = gst_tiovx_pad_get_instance_private (self);

  if (NULL != priv->buffer_pool) {
    gst_object_unref (priv->buffer_pool);
  }

  G_OBJECT_CLASS (gst_tiovx_pad_parent_class)->finalize (object);
}
