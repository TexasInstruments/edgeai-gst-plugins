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

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_miso_debug_category

/* TIOVX Miso Pad */

typedef struct _GstTIOVXMisoPadPrivate
{
  GstAggregatorPad parent;

  guint pool_size;
  vx_reference *exemplar;
  gint graph_param_id;
  gint node_param_id;

  GstBufferPool *buffer_pool;
} GstTIOVXMisoPadPrivate;

enum
{
  PROP_PAD_0,
  PROP_PAD_POOL_SIZE,
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_pad_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXMisoPad, gst_tiovx_miso_pad,
    GST_TYPE_AGGREGATOR_PAD, G_ADD_PRIVATE (GstTIOVXMisoPad)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_miso_pad_debug_category,
        "tiovxmisopad", 0, "debug category for TIOVX miso pad class"));

static void
gst_tiovx_miso_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMisoPad *pad = GST_TIOVX_MISO_PAD (object);
  GstTIOVXMisoPadPrivate *priv = NULL;

  priv = gst_tiovx_miso_pad_get_instance_private (pad);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_POOL_SIZE:
      g_value_set_int (value, priv->pool_size);
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
  GstTIOVXMisoPadPrivate *priv = NULL;

  priv = gst_tiovx_miso_pad_get_instance_private (pad);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_POOL_SIZE:
      priv->pool_size = g_value_get_int (value);
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
  GstTIOVXMisoPadPrivate *priv = NULL;

  priv = gst_tiovx_miso_pad_get_instance_private (self);

  if (priv->exemplar) {
    vxReleaseReference (priv->exemplar);
    priv->exemplar = NULL;
  }

  if (priv->buffer_pool) {
    gst_object_unref (priv->buffer_pool);
    priv->buffer_pool = NULL;
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
  GstTIOVXMisoPadPrivate *priv = NULL;

  priv = gst_tiovx_miso_pad_get_instance_private (tiovx_miso_pad);

  priv->pool_size = DEFAULT_POOL_SIZE;
  priv->graph_param_id = -1;
  priv->node_param_id = -1;
  priv->exemplar = NULL;
}

void
gst_tiovx_miso_pad_set_params (GstTIOVXMisoPad * pad, vx_reference * exemplar,
    gint graph_param_id, gint node_param_id)
{
  GstTIOVXMisoPadPrivate *priv = NULL;

  g_return_if_fail (pad);
  g_return_if_fail (exemplar);
  g_return_if_fail (graph_param_id >= 0);
  g_return_if_fail (node_param_id >= 0);

  priv = gst_tiovx_miso_pad_get_instance_private (pad);


  GST_OBJECT_LOCK (pad);

  if (priv->exemplar) {
    vxReleaseReference (priv->exemplar);
    priv->exemplar = NULL;
  }


  priv->exemplar = exemplar;
  priv->graph_param_id = graph_param_id;
  priv->node_param_id = node_param_id;

  GST_OBJECT_UNLOCK (pad);
}

/* TIOVX Miso */

static void gst_tiovx_miso_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

typedef struct _GstTIOVXMisoPrivate
{
  GstTIOVXContext *tiovx_context;
  vx_context context;
  vx_graph graph;
  vx_node node;

  gboolean is_eos;
} GstTIOVXMisoPrivate;

#define GST_TIOVX_MISO_DEFINE_CUSTOM_CODE \
  G_ADD_PRIVATE (GstTIOVXMiso); \
  GST_DEBUG_CATEGORY_INIT (gst_tiovx_miso_debug_category, "tiovxmiso", 0, "debug category for the tiovxmiso element"); \
  G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,  gst_tiovx_miso_child_proxy_init);

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXMiso, gst_tiovx_miso,
    GST_TYPE_AGGREGATOR, GST_TIOVX_MISO_DEFINE_CUSTOM_CODE);

static void gst_tiovx_miso_finalize (GObject * obj);
static GstFlowReturn gst_tiovx_miso_aggregate (GstAggregator * aggregator,
    gboolean timeout);
static GstFlowReturn gst_tiovx_miso_create_output_buffer (GstTIOVXMiso *
    tiovx_miso, GstBuffer ** outbuf);
static gboolean gst_tiovx_miso_decide_allocation (GstAggregator * self,
    GstQuery * query);
static gboolean gst_tiovx_miso_start (GstAggregator * self);
static gboolean gst_tiovx_miso_stop (GstAggregator * self);
static gboolean gst_tiovx_miso_propose_allocation (GstAggregator * self,
    GstAggregatorPad * pad, GstQuery * decide_query, GstQuery * query);
static GList *gst_tiovx_miso_get_sink_caps_list (GstTIOVXMiso * self);
static GstCaps *gst_tiovx_miso_default_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps);
static gboolean gst_tiovx_miso_modules_init (GstTIOVXMiso * self);
GstCaps *gst_tiovx_miso_fixate_src_caps (GstAggregator * self, GstCaps * caps);
static gboolean
gst_tiovx_miso_negotiated_src_caps (GstAggregator * self, GstCaps * caps);
static gsize gst_tiovx_miso_default_get_size_from_caps (GstTIOVXMiso * agg,
    GstCaps * caps);
static vx_reference gst_tiovx_miso_default_get_reference_from_caps (GstTIOVXMiso
    * agg, GstCaps * caps);
static vx_status
add_graph_pool_parameter_by_node_index (GstTIOVXMiso * self,
    vx_uint32 graph_parameter_index, vx_uint32 node_parameter_index,
    vx_graph_parameter_queue_params_t params_list[],
    vx_reference * image_reference_list, guint ref_list_size);
static gboolean gst_tiovx_miso_sink_event (GstAggregator * aggregator,
    GstAggregatorPad * aggregator_pad, GstEvent * event);
static GstPad *gst_tiovx_miso_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps);
static void gst_tiovx_miso_release_pad (GstElement * element, GstPad * pad);

static void
gst_tiovx_miso_class_init (GstTIOVXMisoClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAggregatorClass *aggregator_class = GST_AGGREGATOR_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_miso_finalize);

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_release_pad);

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
  aggregator_class->sink_event = GST_DEBUG_FUNCPTR (gst_tiovx_miso_sink_event);

  klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_tiovx_miso_default_fixate_caps);
  klass->get_size_from_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_default_get_size_from_caps);
  klass->get_reference_from_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_default_get_reference_from_caps);
}

static void
gst_tiovx_miso_init (GstTIOVXMiso * self)
{
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  priv->context = NULL;
  priv->graph = NULL;
  priv->node = NULL;

  /* App common init */
  GST_DEBUG_OBJECT (self, "Running TIOVX common init");
  priv->tiovx_context = gst_tiovx_context_new ();

  return;
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

static gboolean
gst_tiovx_miso_buffer_to_valid_pad_exemplar (GstTIOVXMisoPad * pad,
    GstBuffer * buffer)
{
  vx_object_array array = NULL;
  vx_reference buffer_reference = NULL;
  GstBuffer *original_buffer = NULL;
  GstTIOVXMisoPadPrivate *priv = NULL;
  GstCaps *caps = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (pad, FALSE);
  g_return_val_if_fail (buffer, FALSE);

  priv = gst_tiovx_miso_pad_get_instance_private (pad);

  caps = gst_pad_get_current_caps (GST_PAD (pad));

  original_buffer = buffer;
  buffer =
      gst_tiovx_validate_tiovx_buffer (GST_CAT_DEFAULT,
      &priv->buffer_pool, buffer, priv->exemplar, caps, priv->pool_size);
  gst_caps_unref (caps);
  if (!buffer) {
    GST_ERROR_OBJECT (pad, "Unable to validate buffer");
    goto exit;
  }

  array =
      gst_tiovx_get_vx_array_from_buffer (GST_CAT_DEFAULT, priv->exemplar,
      buffer);
  buffer_reference = vxGetObjectArrayItem (array, 0);

  gst_tiovx_transfer_handle (GST_CAT_DEFAULT, buffer_reference,
      *priv->exemplar);

  vxReleaseReference (&buffer_reference);

  ret = TRUE;

exit:
  if ((original_buffer != buffer) && buffer) {
    gst_buffer_unref (buffer);
    buffer = NULL;
  }

  return ret;
}

static GstFlowReturn
gst_tiovx_miso_process_graph (GstAggregator * agg)
{
  GstTIOVXMiso *tiovx_miso = GST_TIOVX_MISO (agg);
  GstTIOVXMisoPrivate *priv = NULL;
  GstTIOVXMisoPad *pad = NULL;
  GstTIOVXMisoPadPrivate *pad_priv = NULL;
  GList *l = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  vx_status status = VX_FAILURE;
  uint32_t num_refs = 0;

  g_return_val_if_fail (agg, ret);

  priv = gst_tiovx_miso_get_instance_private (tiovx_miso);

  /* Enqueueing parameters */
  GST_LOG_OBJECT (agg, "Enqueueing parameters");

  pad = GST_TIOVX_MISO_PAD (agg->srcpad);
  pad_priv = gst_tiovx_miso_pad_get_instance_private (pad);

  status =
      vxGraphParameterEnqueueReadyRef (priv->graph, pad_priv->graph_param_id,
      (vx_reference *) pad_priv->exemplar, 1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (agg, "Output enqueue failed %" G_GINT32_FORMAT, status);
    goto exit;
  }

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    pad = l->data;
    pad_priv = gst_tiovx_miso_pad_get_instance_private (pad);

    status =
        vxGraphParameterEnqueueReadyRef (priv->graph, pad_priv->graph_param_id,
        (vx_reference *) pad_priv->exemplar, 1);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (agg, "Input enqueue failed %" G_GINT32_FORMAT, status);
      goto exit;
    }
  }

  /* Processing graph */
  GST_LOG_OBJECT (agg, "Processing graph");
  status = vxScheduleGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (agg, "Schedule graph failed %" G_GINT32_FORMAT, status);
    goto exit;
  }
  status = vxWaitGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (agg, "Wait graph failed %" G_GINT32_FORMAT, status);
    goto exit;
  }

  /* Dequeueing parameters */
  GST_LOG_OBJECT (agg, "Dequeueing parameters");
  pad = GST_TIOVX_MISO_PAD (agg->srcpad);
  pad_priv = gst_tiovx_miso_pad_get_instance_private (pad);
  status =
      vxGraphParameterDequeueDoneRef (priv->graph, pad_priv->graph_param_id,
      (vx_reference *) pad_priv->exemplar, 1, &num_refs);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (agg, "Output dequeue failed %" G_GINT32_FORMAT, status);
    goto exit;
  }

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    pad = l->data;
    pad_priv = gst_tiovx_miso_pad_get_instance_private (pad);
    status =
        vxGraphParameterDequeueDoneRef (priv->graph, pad_priv->graph_param_id,
        (vx_reference *) pad_priv->exemplar, 1, &num_refs);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (agg, "Input enqueue failed %" G_GINT32_FORMAT, status);
      goto exit;
    }
  }

  ret = GST_FLOW_OK;

exit:
  return ret;
}

static GstFlowReturn
gst_tiovx_miso_aggregate (GstAggregator * agg, gboolean timeout)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GList *l = NULL;

  GST_DEBUG_OBJECT (self, "TIOVX Miso aggregate");

  if (priv->is_eos) {
    return GST_FLOW_EOS;
  }

  ret = gst_tiovx_miso_create_output_buffer (self, &outbuf);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to acquire output buffer");
    goto exit;
  }

  if (!gst_tiovx_miso_buffer_to_valid_pad_exemplar (GST_TIOVX_MISO_PAD
          (agg->srcpad), outbuf)) {
    GST_ERROR_OBJECT (self, "Unable transfer data to output exemplar");
    goto exit;
  }

  /* Ensure valid references in the inputs */
  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *pad = l->data;
    GstBuffer *in_buffer = NULL;

    in_buffer = gst_aggregator_pad_peek_buffer (pad);

    if (!in_buffer) {
      GST_ERROR_OBJECT (self, "No input buffer in pad: %" GST_PTR_FORMAT, pad);
      goto finish_buffer;
    }

    if (!gst_tiovx_miso_buffer_to_valid_pad_exemplar (GST_TIOVX_MISO_PAD (pad),
            in_buffer)) {
      GST_ERROR_OBJECT (self, "Unable transfer data to input pad: %p exemplar",
          pad);
      goto exit;
    }

    gst_buffer_unref (in_buffer);
  }

  /* Graph processing */
  ret = gst_tiovx_miso_process_graph (agg);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to process graph");
    goto exit;
  }

finish_buffer:
  gst_aggregator_finish_buffer (agg, outbuf);

exit:

  /* Marking the input buffers as used */
  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *pad = l->data;

    gst_aggregator_pad_drop_buffer (pad);
  }

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
  GstTIOVXMisoPad *tiovx_miso_pad = GST_TIOVX_MISO_PAD (agg_pad);
  GstTIOVXMisoPadPrivate *pad_priv = NULL;
  GstTIOVXMisoClass *klass = NULL;
  GstBufferPool *pool = NULL;
  GstCaps *caps = NULL;
  vx_reference reference = NULL;
  gsize size = 0;
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "Propose allocation");

  klass = GST_TIOVX_MISO_GET_CLASS (self);

  pad_priv = gst_tiovx_miso_pad_get_instance_private (tiovx_miso_pad);

  gst_query_parse_allocation (query, &caps, NULL);

  size = klass->get_size_from_caps (self, caps);
  if (0 == size) {
    GST_ERROR_OBJECT (self, "Unable to get size from caps");
    goto exit;
  }

  /* If the pad doesn't have an exemplar, we'll create a temporary one.
   * We'll add the final one after the caps have been negotiated
   */
  if (pad_priv->exemplar) {
    reference = *pad_priv->exemplar;
  } else {
    GST_INFO_OBJECT (self, "Using temporary reference for configuration");
    reference = klass->get_reference_from_caps (self, caps);
  }

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, pad_priv->pool_size,
      &reference, size, &pool);

  if (pad_priv->buffer_pool) {
    gst_object_unref (pad_priv->buffer_pool);
    pad_priv->buffer_pool = NULL;
  }

  pad_priv->buffer_pool = pool;

  if (!pad_priv->exemplar) {
    vxReleaseReference (&reference);
    reference = NULL;
  }

exit:
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
    GstBufferPool *pool = NULL;
    GstTIOVXMisoPadPrivate *pad_priv = NULL;
    GstTIOVXMisoClass *klass = NULL;
    GstCaps *caps = NULL;
    gsize size = 0;

    pad_priv =
        gst_tiovx_miso_pad_get_instance_private (GST_TIOVX_MISO_PAD
        (agg->srcpad));

    klass = GST_TIOVX_MISO_GET_CLASS (self);
    gst_query_parse_allocation (query, &caps, NULL);

    size = klass->get_size_from_caps (self, caps);
    if (0 == size) {
      GST_ERROR_OBJECT (self, "Unable to get size from caps");
      return FALSE;
    }

    ret =
        gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query,
        pad_priv->pool_size, pad_priv->exemplar, size, &pool);

    if (pad_priv->buffer_pool) {
      gst_object_unref (pad_priv->buffer_pool);
      pad_priv->buffer_pool = NULL;
    }
    pad_priv->buffer_pool = pool;
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

  priv->is_eos = FALSE;

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
  GstTIOVXMisoPadPrivate *pad_priv = NULL;
  GstTIOVXMisoPad *miso_pad = NULL;
  GstTIOVXMisoClass *klass = NULL;
  GstTIOVXMisoPrivate *priv = NULL;
  GList *l = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  vx_graph_parameter_queue_params_t *params_list = NULL;
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
    pad_priv =
        gst_tiovx_miso_pad_get_instance_private (GST_TIOVX_MISO_PAD (pad));

    if ((0 > pad_priv->graph_param_id) || (0 > pad_priv->node_param_id)
        || (NULL == pad_priv->exemplar)) {
      GST_ERROR_OBJECT (self,
          "Incomplete info from subclass: input information not set to pad: %"
          GST_PTR_FORMAT, pad);
      goto free_graph;
    }
  }

  pad_priv =
      gst_tiovx_miso_pad_get_instance_private (GST_TIOVX_MISO_PAD
      (agg->srcpad));
  if ((0 > pad_priv->graph_param_id) || (0 > pad_priv->node_param_id)
      || (NULL == pad_priv->exemplar)) {
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
    pad_priv = gst_tiovx_miso_pad_get_instance_private (miso_pad);

    status =
        add_graph_pool_parameter_by_node_index (self, pad_priv->graph_param_id,
        pad_priv->node_param_id, params_list, pad_priv->exemplar, 1);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Setting input parameter failed, vx_status %" G_GINT32_FORMAT,
          status);
      goto free_parameters_list;
    }
  }

  miso_pad = GST_TIOVX_MISO_PAD (agg->srcpad);
  pad_priv = gst_tiovx_miso_pad_get_instance_private (miso_pad);
  status =
      add_graph_pool_parameter_by_node_index (self, pad_priv->graph_param_id,
      pad_priv->node_param_id, params_list, pad_priv->exemplar, 1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Setting input parameter failed, vx_status %" G_GINT32_FORMAT, status);
    goto free_parameters_list;
  }

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

  /* Release buffer. This is needed in order to free resources allocated by vxVerifyGraph function */
  ret = klass->release_buffer (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass release buffer failed");
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

  /*
   * We'll call reconfiguration on all upstream pads. This will force a propose
   * allocation which should now be using the subclass correct reference.
   */
  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstPad *pad = l->data;
    GstEvent *event = NULL;

    event = gst_event_new_reconfigure ();
    gst_pad_push_event (pad, event);
  }

  ret = TRUE;

exit:
  return ret;
}

static gsize
gst_tiovx_miso_default_get_size_from_caps (GstTIOVXMiso * agg, GstCaps * caps)
{
  GstVideoInfo info;

  g_return_val_if_fail (agg, 0);
  g_return_val_if_fail (caps, 0);

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (agg, "Unable to get video info from caps");
    return 0;
  }

  return GST_VIDEO_INFO_SIZE (&info);
}

static vx_reference
gst_tiovx_miso_default_get_reference_from_caps (GstTIOVXMiso * agg,
    GstCaps * caps)
{
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (agg);
  GstVideoInfo info;

  g_return_val_if_fail (agg, 0);
  g_return_val_if_fail (caps, 0);

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (agg, "Unable to get video info from caps");
    return NULL;
  }

  return (vx_reference) vxCreateImage (priv->context, info.width,
      info.height, gst_format_to_vx_format (info.finfo->format));
}

static vx_status
add_graph_pool_parameter_by_node_index (GstTIOVXMiso * self,
    vx_uint32 graph_parameter_index, vx_uint32 node_parameter_index,
    vx_graph_parameter_queue_params_t params_list[],
    vx_reference * image_reference_list, guint ref_list_size)
{
  GstTIOVXMisoPrivate *priv = NULL;
  vx_status status = VX_FAILURE;
  vx_parameter parameter = NULL;
  vx_graph graph = NULL;
  vx_node node = NULL;

  g_return_val_if_fail (self, VX_FAILURE);
  g_return_val_if_fail (image_reference_list, VX_FAILURE);
  g_return_val_if_fail (graph_parameter_index >= 0, VX_FAILURE);
  g_return_val_if_fail (node_parameter_index >= 0, VX_FAILURE);

  priv = gst_tiovx_miso_get_instance_private (self);
  g_return_val_if_fail (priv, VX_FAILURE);
  g_return_val_if_fail (priv->graph, VX_FAILURE);
  g_return_val_if_fail (priv->node, VX_FAILURE);

  graph = priv->graph;
  node = priv->node;

  parameter = vxGetParameterByIndex (node, node_parameter_index);
  status = vxAddParameterToGraph (graph, parameter);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Add parameter to graph failed, vx_status %" G_GINT32_FORMAT, status);
    vxReleaseParameter (&parameter);
    return status;
  }

  status = vxReleaseParameter (&parameter);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Release parameter failed, vx_status %" G_GINT32_FORMAT, status);
    return status;
  }

  params_list[graph_parameter_index].graph_parameter_index =
      graph_parameter_index;
  params_list[graph_parameter_index].refs_list_size = ref_list_size;
  params_list[graph_parameter_index].refs_list = image_reference_list;

  status = VX_SUCCESS;

  return status;
}

static gboolean
gst_tiovx_miso_sink_event (GstAggregator * agg,
    GstAggregatorPad * agg_pad, GstEvent * event)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
    {
      priv->is_eos = TRUE;
    }
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (gst_tiovx_miso_parent_class)->sink_event
      (agg, agg_pad, event);
}

static GstPad *
gst_tiovx_miso_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstPad *newpad;
  newpad = (GstPad *)
      GST_ELEMENT_CLASS (gst_tiovx_miso_parent_class)->request_new_pad (element,
      templ, req_name, caps);
  if (newpad == NULL)
    goto could_not_create;
  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));
  return newpad;
could_not_create:
  {
    GST_DEBUG_OBJECT (element, "could not create/add pad");
    return NULL;
  }
}

static void
gst_tiovx_miso_release_pad (GstElement * element, GstPad * pad)
{
  GstTIOVXMiso *miso;
  miso = GST_TIOVX_MISO (element);
  GST_DEBUG_OBJECT (miso, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));
  gst_child_proxy_child_removed (GST_CHILD_PROXY (miso), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));
  GST_ELEMENT_CLASS (gst_tiovx_miso_parent_class)->release_pad (element, pad);
}

/* GstChildProxy implementation */
static GObject *
gst_tiovx_miso_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  GstTIOVXMiso *miso = GST_TIOVX_MISO (child_proxy);
  GObject *obj = NULL;
  GST_OBJECT_LOCK (miso);
  obj = g_list_nth_data (GST_ELEMENT_CAST (miso)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (miso);
  return obj;
}

static guint
gst_tiovx_miso_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstTIOVXMiso *miso = GST_TIOVX_MISO (child_proxy);
  GST_OBJECT_LOCK (miso);
  count = GST_ELEMENT_CAST (miso)->numsinkpads;
  GST_OBJECT_UNLOCK (miso);
  return count;
}

static void
gst_tiovx_miso_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;
  iface->get_child_by_index = gst_tiovx_miso_child_proxy_get_child_by_index;
  iface->get_children_count = gst_tiovx_miso_child_proxy_get_children_count;
}
