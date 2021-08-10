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
#include "gsttiovxutils.h"

#include <gst/video/video.h>
#include <TI/j7.h>

#define DEFAULT_POOL_SIZE MIN_POOL_SIZE
#define MAX_NUMBER_OF_PLANES 4

/* TIOVX Miso Pad */

struct _GstTIOVXMisoPad
{
  GstAggregatorPad parent;

  guint pool_size;
  vx_reference *exemplar;
  gint param_id;

  GstBufferPool *buffer_pool;
};

enum
{
  PROP_PAD_0,
  PROP_PAD_POOL_SIZE,
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_pad_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXMisoPad, gst_tiovx_miso_pad,
    GST_TYPE_AGGREGATOR_PAD,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_miso_pad_debug_category,
        "tiovxmisopad", 0, "debug category for TIOVX miso pad class"));

static void
gst_tiovx_miso_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMisoPad *pad = GST_TIOVX_MISO_PAD (object);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_POOL_SIZE:
      g_value_set_int (value, pad->pool_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (pad);
}

static void
gst_tiovx_miso_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMisoPad *pad = GST_TIOVX_MISO_PAD (object);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_POOL_SIZE:
      pad->pool_size = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (pad);
}

static void
gst_tiovx_miso_pad_finalize (GObject * obj)
{
  GstTIOVXMisoPad *self = GST_TIOVX_MISO_PAD (obj);

  if (self->exemplar) {
    vxReleaseReference (self->exemplar);
    self->exemplar = NULL;
  }

  if (self->buffer_pool) {
    gst_object_unref (self->buffer_pool);
    self->buffer_pool = NULL;
  }
}

static void
gst_tiovx_miso_pad_class_init (GstTIOVXMisoPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_tiovx_miso_pad_set_property;
  gobject_class->get_property = gst_tiovx_miso_pad_get_property;
  gobject_class->finalize = gst_tiovx_miso_pad_finalize;

  g_object_class_install_property (gobject_class, PROP_PAD_POOL_SIZE,
      g_param_spec_int ("pool-size", "Pool size",
          "Pool size of the internal buffer pool", MIN_POOL_SIZE, MAX_POOL_SIZE,
          DEFAULT_POOL_SIZE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_tiovx_miso_pad_init (GstTIOVXMisoPad * tiovx_miso_pad)
{
  tiovx_miso_pad->pool_size = DEFAULT_POOL_SIZE;
  tiovx_miso_pad->param_id = -1;
  tiovx_miso_pad->exemplar = NULL;
}

void
gst_tiovx_miso_pad_set_params (GstTIOVXMisoPad * pad, vx_reference * exemplar,
    gint param_id)
{
  g_return_if_fail (pad);
  g_return_if_fail (exemplar);
  g_return_if_fail (param_id >= 0);

  GST_OBJECT_LOCK (pad);

  if (pad->exemplar) {
    vxReleaseReference (pad->exemplar);
    pad->exemplar = NULL;
  }

  pad->exemplar = exemplar;
  pad->param_id = param_id;

  GST_OBJECT_UNLOCK (pad);
}

/* TIOVX Miso */

enum
{
  PROP_0,
  PROP_OUT_POOL_SIZE,
};


GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_miso_debug_category

typedef struct _GstTIOVXMisoPrivate
{
  GstVideoInfo out_info;
  GstTIOVXContext *tiovx_context;
  vx_context context;
  vx_graph graph;
  vx_node node;
  guint out_pool_size;
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
static gboolean gst_tiovx_miso_start (GstAggregator * self);
static gboolean gst_tiovx_miso_stop (GstAggregator * self);
static void gst_tiovx_miso_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_miso_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_tiovx_miso_propose_allocation (GstAggregator * self,
    GstAggregatorPad * pad, GstQuery * decide_query, GstQuery * query);
static GList *gst_tiovx_miso_get_sink_caps_list (GstTIOVXMiso * self);
static GstCaps *gst_tiovx_miso_default_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps);
static gboolean gst_tiovx_miso_modules_init (GstTIOVXMiso * self);
GstCaps *gst_tiovx_miso_fixate_src_caps (GstAggregator * self, GstCaps * caps);
static gboolean
gst_tiovx_miso_negotiated_src_caps (GstAggregator * self, GstCaps * caps);

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
  aggregator_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_propose_allocation);

  aggregator_class->fixate_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_fixate_src_caps);
  aggregator_class->negotiated_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_negotiated_src_caps);

  aggregator_class->start = GST_DEBUG_FUNCPTR (gst_tiovx_miso_start);
  aggregator_class->stop = GST_DEBUG_FUNCPTR (gst_tiovx_miso_stop);

  klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_tiovx_miso_default_fixate_caps);
}

static void
gst_tiovx_miso_init (GstTIOVXMiso * self)
{
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  gst_video_info_init (&priv->out_info);
  priv->context = NULL;
  priv->graph = NULL;
  priv->node = NULL;
  priv->out_pool_size = DEFAULT_POOL_SIZE;

  /* App common init */
  GST_DEBUG_OBJECT (self, "Running TIOVX common init");
  priv->tiovx_context = gst_tiovx_context_new ();

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

  /* Release context */
  if (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context)) {
    tivxHwaUnLoadKernels (priv->context);
    vxReleaseContext (&priv->context);
  }

  /* App common deinit */
  GST_DEBUG_OBJECT (self, "Running TIOVX common deinit");
  if (priv->tiovx_context) {
    g_object_unref (priv->tiovx_context);
    priv->tiovx_context = NULL;
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
  for (l = GST_ELEMENT (aggregator)->sinkpads; l; l = g_list_next (l)) {
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
    pool = NULL;
    ret = GST_FLOW_OK;
  } else {
    GST_ERROR_OBJECT (tiovx_miso,
        "An output buffer can only be created from a buffer pool");
  }

exit:
  return ret;
}

static gboolean
gst_tiovx_miso_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * agg_pad, GstQuery * decide_query, GstQuery * query)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  GstTIOVXMisoPad *tiovx_miso_pad = GST_TIOVX_MISO_PAD (agg_pad);
  GstCaps *caps = NULL;
  GstVideoInfo info;
  vx_reference reference = NULL;
  gsize size = 0;
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "Propose allocation");

  caps = gst_pad_get_current_caps (GST_PAD (agg_pad));

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (self, "Unable to get video info from caps");
    return FALSE;
  }

  size = GST_VIDEO_INFO_SIZE (&info);

  /* If the pad doesn't have an exemplar, we'll create a temporary one.
   * We'll add the final one after the caps have been negotiated
   */
  if (tiovx_miso_pad->exemplar) {
    reference = *tiovx_miso_pad->exemplar;
  } else {
    reference = (vx_reference) vxCreateImage (priv->context, info.width,
        info.height, gst_format_to_vx_format (info.finfo->format));
  }

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, tiovx_miso_pad->pool_size,
      &reference, size, &tiovx_miso_pad->buffer_pool);

  if (!tiovx_miso_pad->exemplar) {
    vxReleaseReference (&reference);
    reference = NULL;
  }

  return ret;
}

static gboolean
gst_tiovx_miso_decide_allocation (GstAggregator * agg, GstQuery * query)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
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
    pool = NULL;
  }

  if (pool_needed) {
    GstCaps *caps = NULL;
    GstVideoInfo info;
    gsize size = 0;

    caps = gst_pad_get_current_caps (agg->srcpad);

    if (!gst_video_info_from_caps (&info, caps)) {
      GST_ERROR_OBJECT (self, "Unable to get video info from caps");
      return FALSE;
    }

    size = GST_VIDEO_INFO_SIZE (&info);

    ret =
        gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query,
        GST_TIOVX_MISO_PAD (agg->srcpad)->pool_size,
        GST_TIOVX_MISO_PAD (agg->srcpad)->exemplar, size,
        &GST_TIOVX_MISO_PAD (agg->srcpad)->buffer_pool);
  }

  return ret;
}

static gboolean
gst_tiovx_miso_start (GstAggregator * agg)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "start");

  /* Create OpenVX Context */
  GST_DEBUG_OBJECT (self, "Creating context");
  priv->context = vxCreateContext ();

  if (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context)) {
    tivxHwaLoadKernels (priv->context);
  }

  return TRUE;
}

static gboolean
gst_tiovx_miso_stop (GstAggregator * agg)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoClass *klass = NULL;
  GstTIOVXMisoPrivate *priv = NULL;
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (self, "stop");

  priv = gst_tiovx_miso_get_instance_private (self);
  klass = GST_TIOVX_MISO_GET_CLASS (agg);

  if ((NULL == priv->graph)
      || (VX_SUCCESS != vxGetStatus ((vx_reference) priv->graph))) {
    GST_WARNING_OBJECT (self,
        "Trying to deinit modules but initialization was not completed, skipping...");
    ret = TRUE;
    goto free_common;
  }

  if (!klass->deinit_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement deinit_module method");
    goto release_graph;
  }
  ret = klass->deinit_module (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass deinit module failed");
  }

release_graph:
  vxReleaseGraph (&priv->graph);

free_common:
  priv->node = NULL;
  priv->graph = NULL;
  return TRUE;
}

static GList *
gst_tiovx_miso_get_sink_caps_list (GstTIOVXMiso * self)
{
  GstAggregator *agg = GST_AGGREGATOR (self);
  GList *sink_caps_list = NULL;
  GList *l = NULL;

  g_return_val_if_fail (self, NULL);

  GST_DEBUG_OBJECT (self, "Generating sink caps list");

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstPad *sink_pad = GST_PAD (l->data);
    GstCaps *pad_caps = NULL;

    pad_caps = gst_pad_get_current_caps (sink_pad);

    GST_DEBUG_OBJECT (self, "Caps from %s:%s peer: %" GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (sink_pad), pad_caps);
    /* Insert at the end of the src caps list */
    sink_caps_list = g_list_insert (sink_caps_list, pad_caps, -1);
  }

  return sink_caps_list;
}

static GstCaps *
gst_tiovx_miso_default_fixate_caps (GstTIOVXMiso * self, GList * sink_caps_list,
    GstCaps * src_caps)
{
  GstCaps *fixated_src_caps = NULL;

  GST_DEBUG_OBJECT (self, "Fixating caps");

  g_return_val_if_fail (src_caps, FALSE);
  g_return_val_if_fail (sink_caps_list, FALSE);


  fixated_src_caps = gst_caps_fixate (src_caps);

  return fixated_src_caps;
}

static gboolean
gst_tiovx_miso_modules_init (GstTIOVXMiso * self)
{
  GstAggregator *agg = GST_AGGREGATOR (self);
  GstTIOVXMisoPad *miso_pad = NULL;
  GstTIOVXMisoClass *klass = NULL;
  GstTIOVXMisoPrivate *priv = NULL;
  GList *l = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  vx_graph_parameter_queue_params_t *params_list = NULL;
  gint param_id = -1;
  guint num_pads = 0;

  g_return_val_if_fail (self, FALSE);

  priv = gst_tiovx_miso_get_instance_private (self);
  klass = GST_TIOVX_MISO_GET_CLASS (self);

  GST_DEBUG_OBJECT (self, "Initializing MISO module");

  status = vxGetStatus ((vx_reference) priv->context);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Context creation failed with error: %" G_GINT32_FORMAT, status);
    goto exit;
  }

  if (!klass->init_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement init_module method.");
    goto exit;
  }
  if (!klass->init_module (self, priv->context, GST_ELEMENT (self)->sinkpads,
          agg->srcpad)) {
    GST_ERROR_OBJECT (self, "Subclass init module failed");
    goto exit;
  }

  GST_DEBUG_OBJECT (self, "Creating graph");
  priv->graph = vxCreateGraph (priv->context);
  status = vxGetStatus ((vx_reference) priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Graph creation failed, vx_status %" G_GINT32_FORMAT, status);
    goto deinit_module;
  }

  GST_DEBUG_OBJECT (self, "Creating graph in subclass");
  if (!klass->create_graph) {
    GST_ERROR_OBJECT (self, "Subclass did not implement create_graph method.");
    goto free_graph;
  }
  if (!klass->create_graph (self, priv->context, priv->graph)) {
    GST_ERROR_OBJECT (self, "Subclass create graph failed");
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Get node info");
  if (!klass->get_node_info) {
    GST_ERROR_OBJECT (self, "Subclass did not implement get_node_info method");
    goto free_graph;
  }
  if (!klass->get_node_info (self, GST_ELEMENT (self)->sinkpads, agg->srcpad,
          &priv->node)) {
    GST_ERROR_OBJECT (self, "Subclass get node info failed");
    goto free_graph;
  }

  for (l = GST_ELEMENT (self)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *pad = l->data;

    if ((0 > GST_TIOVX_MISO_PAD (pad)->param_id)
        || (NULL == GST_TIOVX_MISO_PAD (pad)->exemplar)) {
      GST_ERROR_OBJECT (self,
          "Incomplete info from subclass: input information not set to pad: %"
          GST_PTR_FORMAT, pad);
      goto free_graph;
    }
  }

  if ((0 > GST_TIOVX_MISO_PAD (agg->srcpad)->param_id)
      || (NULL == GST_TIOVX_MISO_PAD (agg->srcpad)->exemplar)) {
    GST_ERROR_OBJECT (self,
        "Incomplete info from subclass: output information not set to pad: %"
        GST_PTR_FORMAT, agg->srcpad);
    goto free_graph;
  }

  if (!priv->node) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: node missing");
    goto free_graph;
  }

  /* Number of pads is num_sinkpads + 1 source pad */
  num_pads = 1 + g_list_length (GST_ELEMENT (self)->sinkpads);

  GST_DEBUG_OBJECT (self, "Setting up parameters");
  params_list = g_malloc0 (num_pads * sizeof (*params_list));
  if (NULL == params_list) {
    GST_ERROR_OBJECT (self, "Could not allocate memory for parameters list");
    goto free_graph;
  }

  for (l = GST_ELEMENT (self)->sinkpads; l; l = g_list_next (l)) {
    miso_pad = GST_TIOVX_MISO_PAD (l->data);

    param_id = miso_pad->param_id;

    params_list[param_id].graph_parameter_index = param_id;
    params_list[param_id].refs_list = miso_pad->exemplar;
    params_list[param_id].refs_list_size = 1;
  }

  miso_pad = GST_TIOVX_MISO_PAD (agg->srcpad);
  param_id = miso_pad->param_id;
  params_list[param_id].graph_parameter_index = param_id;
  params_list[param_id].refs_list = miso_pad->exemplar;
  params_list[param_id].refs_list_size = 1;

  GST_DEBUG_OBJECT (self, "Schedule Config");
  status = vxSetGraphScheduleConfig (priv->graph,
      VX_GRAPH_SCHEDULE_MODE_QUEUE_MANUAL, num_pads, params_list);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Graph schedule configuration failed, vx_status %" G_GINT32_FORMAT,
        status);
    goto free_parameters_list;
  }

  /* Parameters list has to be released even if the code doesn't fail */
  g_free (params_list);

  GST_DEBUG_OBJECT (self, "Verify graph");
  status = vxVerifyGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Graph verification failed, vx_status %" G_GINT32_FORMAT, status);
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Configure Module");
  if (!klass->configure_module) {
    GST_LOG_OBJECT (self,
        "Subclass did not implement configure node method. Skipping node configuration");
  } else {
    if (!klass->configure_module (self)) {
      GST_ERROR_OBJECT (self, "Subclass configure node failed");
      goto free_graph;
    }
  }

  GST_DEBUG_OBJECT (self, "Finish init module");
  ret = TRUE;
  goto exit;

free_parameters_list:
  g_free (params_list);

free_graph:
  vxReleaseGraph (&priv->graph);
  priv->graph = NULL;

deinit_module:
  if (!klass->deinit_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement deinit_module method");
    goto exit;
  }
  if (!klass->deinit_module (self)) {
    GST_ERROR_OBJECT (self, "Subclass deinit module failed");
  }

exit:
  return ret;
}

GstCaps *
gst_tiovx_miso_fixate_src_caps (GstAggregator * agg, GstCaps * src_caps)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoClass *klass = NULL;
  GList *sink_caps_list = NULL;
  GstCaps *fixated_caps = NULL;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (src_caps, NULL);

  klass = GST_TIOVX_MISO_GET_CLASS (self);

  sink_caps_list = gst_tiovx_miso_get_sink_caps_list (self);

  /* Should return the fixated caps the element will use on the src pads */
  fixated_caps = klass->fixate_caps (self, sink_caps_list, src_caps);
  if (!fixated_caps) {
    GST_ERROR_OBJECT (self, "Subclass did not fixate caps");
    goto exit;
  }

exit:
  return fixated_caps;
}

gboolean
gst_tiovx_miso_negotiated_src_caps (GstAggregator * agg, GstCaps * caps)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  gboolean ret = FALSE;
  GList *l = NULL;

  GST_DEBUG_OBJECT (self, "Negotiated src caps");

  /* We are calling this manually to ensure that during module initialization
   * the src pad has all the information. Normally this would be called by
   * GstAggregator right after the negotiated_src_caps
   */
  gst_aggregator_set_src_caps (agg, caps);

  if (!gst_tiovx_miso_modules_init (self)) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to init TIOVX module"), (NULL));
    goto exit;
  }

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstTIOVXMisoPad *sink_pad = GST_TIOVX_MISO_PAD (l->data);
    GstCaps *caps = NULL;
    GstVideoInfo info;
    gsize size = 0;

    caps = gst_pad_get_current_caps (GST_PAD (sink_pad));

    if (!gst_video_info_from_caps (&info, caps)) {
      GST_ERROR_OBJECT (self, "Unable to get video info from caps");
      return FALSE;
    }

    size = GST_VIDEO_INFO_SIZE (&info);

    /* Reconfigure the pool now that we have the exemplar */
    if (!gst_tiovx_configure_pool (GST_CAT_DEFAULT, sink_pad->buffer_pool,
            sink_pad->exemplar, caps, size, sink_pad->pool_size)) {
      GST_ERROR_OBJECT (agg, "Unable to configure pool for: %" GST_PTR_FORMAT,
          sink_pad);
      goto exit;
    }
  }

  ret = TRUE;

exit:
  return ret;
}
