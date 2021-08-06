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
 * *    No reverse engineering, decompilation, or disassembly of this software
 *      is permitted with respect to any software provided in binary form.
 *
 * *    Any redistribution and use are licensed by TI for use only with TI Devices.
 *
 * *    Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
 *
 * *    Any redistribution and use of the source code, including any resulting
 *      derivative works, are licensed by TI for use only with TI Devices.
 *
 * *    Any redistribution and use of any object code compiled from the source
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
#  include "config.h"
#endif

#include "gsttiovxmiso.h"

#include "gsttiovxbufferpool.h"
#include "gsttiovxcontext.h"
#include "gsttiovxmeta.h"
#include "gsttiovxutils.h"

#include <app_init.h>
#include <gst/video/video.h>
#include <TI/j7.h>

#define DEFAULT_POOL_SIZE MIN_POOL_SIZE
#define MAX_NUMBER_OF_PLANES 4

enum
{
  PROP_0,
  PROP_OUT_POOL_SIZE,
};


GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_miso_debug_category

typedef struct _GstTIOVXMisoPrivate
{
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  GstTIOVXContext *tiovx_context;
  vx_context context;
  vx_graph graph;
  vx_node node;
  GList *input;
  vx_reference *output;
  guint out_pool_size;
  guint num_channels;
} GstTIOVXMisoPrivate;

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXMiso, gst_tiovx_miso,
    GST_TYPE_AGGREGATOR, G_ADD_PRIVATE (GstTIOVXMiso)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_miso_debug_category, "tiovxmiso", 0,
        "debug category for tiovxmiso base class"));

static void gst_tiovx_miso_finalize (GObject * obj);
static GstFlowReturn gst_tiovx_miso_aggregate (GstAggregator * aggregator,
    gboolean timeout);
static GstFlowReturn gst_tiovx_miso_create_output_buffer (GstTIOVXMiso *
    tiovx_miso, GstBuffer ** outbuf);
static gboolean gst_tiovx_miso_decide_allocation (GstAggregator * self,
    GstQuery * query);
static void gst_tiovx_miso_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_miso_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);

static void
gst_tiovx_miso_class_init (GstTIOVXMisoClass * klass)
{
  GstAggregatorClass *aggregator_class = GST_AGGREGATOR_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_miso_finalize);
  gobject_class->set_property = gst_tiovx_miso_set_property;
  gobject_class->get_property = gst_tiovx_miso_get_property;

  g_object_class_install_property (gobject_class, PROP_OUT_POOL_SIZE,
      g_param_spec_uint ("out-pool-size", "Output Pool Size",
          "Number of buffers to allocate in output pool", MIN_POOL_SIZE,
          MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  aggregator_class->aggregate = GST_DEBUG_FUNCPTR (gst_tiovx_miso_aggregate);

  aggregator_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_decide_allocation);
}

static void
gst_tiovx_miso_init (GstTIOVXMiso * self)
{
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  gst_video_info_init (&priv->in_info);
  gst_video_info_init (&priv->out_info);
  priv->context = NULL;
  priv->graph = NULL;
  priv->node = NULL;
  priv->input = NULL;
  priv->output = NULL;
  priv->out_pool_size = DEFAULT_POOL_SIZE;
  priv->num_channels = DEFAULT_NUM_CHANNELS;

  /* App common init */
  GST_DEBUG_OBJECT (self, "Running TIOVX common init");
  priv->tiovx_context = gst_tiovx_context_new ();

  /* Create OpenVX Context */
  GST_DEBUG_OBJECT (self, "Creating context");
  priv->context = vxCreateContext ();

  if (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context)) {
    tivxHwaLoadKernels (priv->context);
  }

  return;
}

static void
gst_tiovx_miso_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (object);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_OUT_POOL_SIZE:
      priv->out_pool_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_miso_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (object);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_OUT_POOL_SIZE:
      g_value_set_uint (value, priv->out_pool_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_miso_finalize (GObject * obj)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (obj);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  GST_LOG_OBJECT (self, "finalize");

  g_return_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context));

  vxReleaseReference (priv->output);
  g_free (priv->output);

  /* Release context */
  if (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context)) {
    tivxHwaUnLoadKernels (priv->context);
    vxReleaseContext (&priv->context);
  }

  /* App common deinit */
  GST_DEBUG_OBJECT (self, "Running TIOVX common deinit");
  if (priv->tiovx_context) {
    g_object_unref (priv->tiovx_context);
  }

  G_OBJECT_CLASS (gst_tiovx_miso_parent_class)->finalize (obj);
}

static GstFlowReturn
gst_tiovx_miso_aggregate (GstAggregator * aggregator, gboolean timeout)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (aggregator);
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GList *l = NULL;

  GST_DEBUG_OBJECT (self, "TIOVX Miso aggregate");

  ret = gst_tiovx_miso_create_output_buffer (self, &outbuf);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to acquire output buffer");
  }

  /* Not processing anything for now, just dropping input buffers and marking the output as ready */
  for (l = GST_ELEMENT (aggregator)->sinkpads; l; l = l->next) {
    GstAggregatorPad *pad = l->data;

    gst_aggregator_pad_drop_buffer (pad);
  }

  gst_aggregator_finish_buffer (aggregator, outbuf);

  return ret;
}

static GstFlowReturn
gst_tiovx_miso_create_output_buffer (GstTIOVXMiso * tiovx_miso,
    GstBuffer ** outbuf)
{
  GstAggregator *aggregator = GST_AGGREGATOR (tiovx_miso);
  GstBufferPool *pool;
  GstFlowReturn ret = GST_FLOW_ERROR;

  pool = gst_aggregator_get_buffer_pool (aggregator);

  if (pool) {
    if (!gst_buffer_pool_is_active (pool)) {
      if (!gst_buffer_pool_set_active (pool, TRUE)) {
        GST_ERROR_OBJECT (tiovx_miso, "Failed to activate bufferpool");
        goto exit;
      }
    }

    ret = gst_buffer_pool_acquire_buffer (pool, outbuf, NULL);
    gst_object_unref (pool);
    ret = GST_FLOW_OK;
  } else {
    GST_ERROR_OBJECT (tiovx_miso,
        "An output buffer can only be created from a buffer pool");
  }

exit:
  return ret;
}

static gboolean
gst_tiovx_miso_decide_allocation (GstAggregator * agg, GstQuery * query)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  gboolean ret = TRUE;
  gint npool = 0;
  gboolean pool_needed = TRUE;

  GST_LOG_OBJECT (self, "Decide allocation");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    /* Use TIOVX pool if found */
    if (GST_TIOVX_IS_BUFFER_POOL (pool)) {
      GST_INFO_OBJECT (self, "TIOVX pool found, using this one: \"%s\"",
          GST_OBJECT_NAME (pool));

      pool_needed = FALSE;
    } else {
      GST_INFO_OBJECT (self, "No TIOVX pool, discarding: \"%s\"",
          GST_OBJECT_NAME (pool));

      gst_query_remove_nth_allocation_pool (query, npool);
    }

    gst_object_unref (pool);
  }

  if (pool_needed) {
    /* Temporary code to create a valid exemplar, this responsability will be
       passed to the child class */
    GstCaps *caps = NULL;

    gst_query_parse_allocation (query, &caps, NULL);
    gst_video_info_from_caps (&priv->out_info, caps);

    priv->output = (vx_reference *) g_malloc0 (sizeof (vx_image));

    *priv->output =
        (vx_reference) vxCreateImage (priv->context, priv->out_info.width,
        priv->out_info.height,
        gst_format_to_vx_format (priv->out_info.finfo->format));

    /* Create our own pool if a TIOVX was not found.
       We use output vx_reference to decide a pool to use downstream. */
    ret =
        gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, priv->out_pool_size,
        priv->output, &priv->out_info);
  }

  return ret;
}
