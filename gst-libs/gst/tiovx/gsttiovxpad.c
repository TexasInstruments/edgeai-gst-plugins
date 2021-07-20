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

/**
 * SECTION:gsttiovxpad
 * @short_description: GStreamer pad for GstTIOVX based elements
 *
 * This class implements a GStreamer standard buffer pool for GstTIOVX
 * based elements.
 */

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_pad_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_pad_debug_category

struct _GstTIOVXPad
{
  GstPad base;

  GstTIOVXBufferPool *buffer_pool;

    gboolean (*notify_function) (GstElement * notify_element);
  GstElement *notify_element;

    gboolean (*chain_function) (GstElement * chain_element);
  GstElement *chain_element;

  vx_reference exemplar;
  guint min_buffers;
  guint max_buffers;
};

G_DEFINE_TYPE_WITH_CODE (GstTIOVXPad, gst_tiovx_pad,
    GST_TYPE_PAD,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_pad_debug_category,
        "tiovxpad", 0, "debug category for TIOVX pad class"));

/* prototypes */
static gboolean gst_tiovx_pad_query_func (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_tiovx_pad_chain_func (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static void gst_tiovx_pad_finalize (GObject * object);

static void
gst_tiovx_pad_class_init (GstTIOVXPadClass * klass)
{
  GObjectClass *o_class = G_OBJECT_CLASS (klass);

  o_class->finalize = gst_tiovx_pad_finalize;
}

static void
gst_tiovx_pad_init (GstTIOVXPad * this)
{
  this->buffer_pool = NULL;
  this->chain_function = NULL;
  this->chain_element = NULL;
  this->notify_function = NULL;
  this->notify_element = NULL;
  this->exemplar = NULL;
  this->min_buffers = 0;
  this->max_buffers = 0;
}

GstTIOVXPad *
gst_tiovx_pad_new (const GstPadDirection direction)
{
  GstTIOVXPad *pad = g_object_new (GST_TIOVX_TYPE_PAD, NULL);

  pad->base.direction = direction;

  if (GST_PAD_SINK == direction) {
    gst_pad_set_chain_function ((GstPad *) pad, gst_tiovx_pad_chain_func);
  }
  gst_pad_set_query_function ((GstPad *) pad, gst_tiovx_pad_query_func);

  return pad;
}

void
gst_tiovx_pad_set_exemplar (GstTIOVXPad * pad, const vx_reference exemplar)
{
  GstTIOVXPad *tiovx_pad = GST_TIOVX_PAD (pad);

  tiovx_pad->exemplar = exemplar;
}

void
gst_tiovx_pad_set_num_buffers (GstTIOVXPad * pad, const guint min_buffers,
    const guint max_buffers)
{
  GstTIOVXPad *tiovx_pad = GST_TIOVX_PAD (pad);

  tiovx_pad->min_buffers = min_buffers;
  tiovx_pad->max_buffers = max_buffers;
}

gboolean
gst_tiovx_pad_trigger (GstTIOVXPad * pad, GstCaps * caps)
{
  GstTIOVXPad *tiovx_pad = GST_TIOVX_PAD (pad);
  GstQuery *query = NULL;
  gint npool = 0;
  gboolean ret = FALSE;

  g_return_val_if_fail (pad, FALSE);

  query = gst_query_new_allocation (caps, TRUE);

  ret = gst_pad_peer_query (GST_PAD (pad), query);
  if (!ret) {
    GST_ERROR_OBJECT (pad, "Unable to query pad peer");
    goto unref_query;
  }

  /* Always remove the current pool, we will either create a new one or get it from downstream */
  if (NULL != tiovx_pad->buffer_pool) {
    gst_object_unref (tiovx_pad->buffer_pool);
    tiovx_pad->buffer_pool = NULL;
  }

  /* Look for the first TIOVX buffer if present */
  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (GST_TIOVX_IS_BUFFER_POOL (pool)) {
      tiovx_pad->buffer_pool = GST_TIOVX_BUFFER_POOL (pool);
      break;
    }
  }

  if (NULL == tiovx_pad->buffer_pool) {
    tiovx_pad->buffer_pool = g_object_new (GST_TIOVX_TYPE_BUFFER_POOL, NULL);
  }

  ret = TRUE;

unref_query:
  gst_query_unref (query);

  return ret;
}

void
gst_tiovx_pad_install_notify (GstTIOVXPad * pad,
    gboolean (*notify_function) (GstElement * element), GstElement * element)
{
  GstTIOVXPad *self = GST_TIOVX_PAD (pad);

  self->notify_function = notify_function;
  self->notify_element = element;
}

static gboolean
gst_tiovx_pad_process_allocation_query (GstTIOVXPad * pad, GstQuery * query)
{
  GstTIOVXPad *tiovx_pad = GST_TIOVX_PAD (pad);
  GstStructure *config = NULL;
  GstVideoInfo info;
  GstCaps *caps = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (pad, FALSE);
  g_return_val_if_fail (query, FALSE);

  if (NULL == tiovx_pad->exemplar) {
    GST_ERROR_OBJECT (pad,
        "Cannot process allocation query without an exemplar");
    goto out;
  }

  if (NULL != tiovx_pad->buffer_pool) {
    GST_DEBUG_OBJECT (pad, "Freeing current pool");
    gst_object_unref (tiovx_pad->buffer_pool);
    tiovx_pad->buffer_pool = NULL;
  }

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_ERROR_OBJECT (pad, "Unable to parse caps from query");
    ret = FALSE;
    goto out;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (pad, "Unable to get video info from caps");
    return FALSE;
  }

  tiovx_pad->buffer_pool = g_object_new (GST_TIOVX_TYPE_BUFFER_POOL, NULL);

  config =
      gst_buffer_pool_get_config (GST_BUFFER_POOL (tiovx_pad->buffer_pool));

  gst_buffer_pool_config_set_exemplar (config, tiovx_pad->exemplar);
  gst_buffer_pool_config_set_params (config, caps, GST_VIDEO_INFO_SIZE (&info),
      tiovx_pad->min_buffers, tiovx_pad->max_buffers);

  if (!gst_buffer_pool_set_config (GST_BUFFER_POOL (tiovx_pad->buffer_pool),
          config)) {
    GST_ERROR_OBJECT (pad, "Unable to set pool configuration");
    gst_object_unref (tiovx_pad->buffer_pool);
    tiovx_pad->buffer_pool = NULL;
    goto out;
  }

  gst_query_add_allocation_pool (query,
      GST_BUFFER_POOL (tiovx_pad->buffer_pool), GST_VIDEO_INFO_SIZE (&info),
      tiovx_pad->min_buffers, tiovx_pad->max_buffers);

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_pad_query_func (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXPad *tiovx_pad = GST_TIOVX_PAD (pad);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
      GST_DEBUG_OBJECT (pad, "Received allocation query");
      if ((NULL != tiovx_pad->notify_function)
          || (NULL != tiovx_pad->notify_element)) {
        /* Notify the TIOVX element that it can start the downstream query allocation */
        tiovx_pad->notify_function (tiovx_pad->notify_element);

        /* Start this pad's allocation query */
        ret = gst_tiovx_pad_process_allocation_query (tiovx_pad, query);
      } else {
        GST_ERROR_OBJECT (pad,
            "No notify function or element to notify installed");
      }
      break;
    default:
      GST_DEBUG_OBJECT (pad, "Received non-allocation query");
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

static GstBuffer *
gst_tiovx_pad_copy_buffer (GstTIOVXPad * pad, GstTIOVXBufferPool * pool,
    GstBuffer * in_buffer)
{
  GstBuffer *out_buffer = NULL;
  GstTIOVXMeta *meta = NULL;
  GstMemory *in_memory = NULL;
  GstCaps *in_caps = NULL;
  GstStructure *in_config = NULL;
  GstVideoInfo *caps_info = NULL;
  GstMapInfo in_info;
  gboolean ret = FALSE;
  gint i = 0;

  g_return_val_if_fail (pool, NULL);
  g_return_val_if_fail (in_buffer, NULL);

  in_config = gst_buffer_pool_get_config (in_buffer->pool);

  ret =
      gst_buffer_pool_config_get_params (in_config, &in_caps, NULL, NULL, NULL);
  if (!ret) {
    GST_ERROR_OBJECT (pad, "Unable to retrieve caps from input buffer");
    goto unref_config;
  }

  if (!gst_video_info_from_caps (caps_info, in_caps)) {
    GST_ERROR_OBJECT (pad, "Unable to parse input caps info");
    goto unref_config;
  }

  in_memory = gst_buffer_get_memory (in_buffer, 0);
  if (NULL == in_memory) {
    GST_ERROR_OBJECT (pad, "Unable to get memory from input buffer");
    goto out;
  }

  ret = gst_memory_map (in_memory, &in_info, GST_MAP_READ);
  if (FALSE == ret) {
    GST_ERROR_OBJECT (pad, "Unable map input memory");
    goto free_inmem;
  }

  meta =
      (GstTIOVXMeta *) gst_buffer_get_meta (out_buffer,
      GST_TIOVX_META_API_TYPE);
  if (NULL == meta) {
    GST_ERROR_OBJECT (pad, "Unable to acquire TIOVX meta");
    goto free_inmem;
  }

  for (i = 0; i < caps_info->finfo->n_planes; i++) {
    memmove (meta->image_info.data + meta->image_info.plane_offset[i],
        in_info.data + caps_info->finfo->poffset[i],
        meta->image_info.plane_sizes[i]);
  }

free_inmem:
  gst_object_unref (in_memory);

  /* The in_buffer is no longer needed, it has been coopied to our TIOVX buffer */
  gst_buffer_unref (in_buffer);

unref_config:
  gst_object_unref (in_config);
out:
  return out_buffer;
}

static gboolean
gst_tiovx_pad_chain_func (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstTIOVXPad *tiovx_pad = GST_TIOVX_PAD (pad);

  if (buffer->pool != GST_BUFFER_POOL (tiovx_pad->buffer_pool)) {
    if (GST_TIOVX_IS_BUFFER_POOL (buffer->pool)) {
      /* We'll replace our pool by the buffer's */
      gst_object_unref (tiovx_pad->buffer_pool);

      tiovx_pad->buffer_pool = GST_TIOVX_BUFFER_POOL (buffer->pool);
    } else {
      /* Copy the buffer */
      buffer =
          gst_tiovx_pad_copy_buffer (tiovx_pad, tiovx_pad->buffer_pool, buffer);
    }
  }

  return tiovx_pad->chain_function (tiovx_pad->chain_element);
}

static void
gst_tiovx_pad_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_tiovx_pad_parent_class)->finalize (object);
}
