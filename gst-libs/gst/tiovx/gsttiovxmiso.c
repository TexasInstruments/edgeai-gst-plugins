/*
 * Copyright (c) [2021-2022] Texas Instruments Incorporated
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

#include "gsttiovx.h"
#include "gsttiovxbufferpool.h"
#include "gsttiovxbufferpoolutils.h"
#include "gsttiovxbufferutils.h"
#include "gsttiovxcontext.h"
#include "gsttiovxqueueableobject.h"
#include "gsttiovxutils.h"

#include <gst/video/video.h>

#define DEFAULT_REPEAT_AFTER_EOS TRUE

#define DEFAULT_POOL_SIZE MIN_POOL_SIZE
#define MAX_NUMBER_OF_PLANES 4
#define GST_BUFFER_OFFSET_FIXED_VALUE -1
#define GST_BUFFER_OFFSET_END_FIXED_VALUE -1
#define DEFAULT_NUM_CHANNELS 1

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_miso_debug_category

/* TIOVX Miso Pad */

typedef struct _GstTIOVXMisoPadPrivate
{
  GstAggregatorPad parent;

  guint pool_size;
  vx_object_array array;
  vx_reference *exemplar;
  gint graph_param_id;
  gint node_param_id;
  gint num_channels;

  GstBufferPool *buffer_pool;
  gboolean repeat_after_eos;
} GstTIOVXMisoPadPrivate;

enum
{
  PROP_PAD_0,
  PROP_PAD_POOL_SIZE,
  PROP_PAD_REPEAT_AFTER_EOS,
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_pad_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXMisoPad, gst_tiovx_miso_pad,
    GST_TYPE_AGGREGATOR_PAD, G_ADD_PRIVATE (GstTIOVXMisoPad)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_miso_pad_debug_category,
        "tiovxmisopad", 0, "debug category for TIOVX miso pad class"));


gint gst_tiovx_miso_pad_get_pool_size(GstTIOVXMisoPad *pad)
{
  GstTIOVXMisoPadPrivate *priv = gst_tiovx_miso_pad_get_instance_private (pad);

  return priv->pool_size;
}

static void
gst_tiovx_miso_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMisoPad *pad = GST_TIOVX_MISO_PAD (object);
  GstTIOVXMisoPadPrivate *priv = gst_tiovx_miso_pad_get_instance_private (pad);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_POOL_SIZE:
      g_value_set_int (value, priv->pool_size);
      break;
    case PROP_PAD_REPEAT_AFTER_EOS:
      g_value_set_boolean (value, priv->repeat_after_eos);
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
  GstTIOVXMisoPadPrivate *priv = gst_tiovx_miso_pad_get_instance_private (pad);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_POOL_SIZE:
      priv->pool_size = g_value_get_int (value);
      break;
    case PROP_PAD_REPEAT_AFTER_EOS:
      priv->repeat_after_eos = g_value_get_boolean (value);
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
  GstTIOVXMisoPadPrivate *priv = gst_tiovx_miso_pad_get_instance_private (self);

  if (priv->exemplar) {
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

  g_object_class_install_property (gobject_class, PROP_PAD_REPEAT_AFTER_EOS,
      g_param_spec_boolean ("repeat-after-eos",
          "Pads on EOS repeat the last buffer",
          "Flag to indicate if the pad will repeat the last buffer after an EOS is received. "
          "Only valid for sink pads", DEFAULT_REPEAT_AFTER_EOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_tiovx_miso_pad_init (GstTIOVXMisoPad * tiovx_miso_pad)
{
  GstTIOVXMisoPadPrivate *priv =
      gst_tiovx_miso_pad_get_instance_private (tiovx_miso_pad);

  priv->pool_size = DEFAULT_POOL_SIZE;
  priv->graph_param_id = -1;
  priv->node_param_id = -1;
  priv->exemplar = NULL;
  priv->repeat_after_eos = DEFAULT_REPEAT_AFTER_EOS;
  priv->num_channels = DEFAULT_NUM_CHANNELS;
}

void
gst_tiovx_miso_pad_set_params (GstTIOVXMisoPad * pad, vx_object_array array,
    vx_reference * exemplar, gint graph_param_id, gint node_param_id)
{
  GstTIOVXMisoPadPrivate *priv = NULL;

  g_return_if_fail (pad);
  g_return_if_fail (exemplar);

  priv = gst_tiovx_miso_pad_get_instance_private (pad);

  GST_OBJECT_LOCK (pad);

  if (priv->exemplar) {
    priv->exemplar = NULL;
  }

  priv->array = array;
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
  guint num_channels;

  GList *queueable_objects;

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
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels);
static gboolean gst_tiovx_miso_modules_init (GstTIOVXMiso * self);
GstCaps *gst_tiovx_miso_fixate_src_caps (GstAggregator * self, GstCaps * caps);
static gboolean
gst_tiovx_miso_negotiated_src_caps (GstAggregator * self, GstCaps * caps);
static GstPad *gst_tiovx_miso_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps);
static void gst_tiovx_miso_release_pad (GstElement * element, GstPad * pad);
static GstCaps *intersect_with_template_caps (GstCaps * caps, GstPad * pad);

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

  klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_tiovx_miso_default_fixate_caps);
}

static void
gst_tiovx_miso_init (GstTIOVXMiso * self)
{
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  GstAggregator *aggregator = GST_AGGREGATOR (self);
  GstElement *element = GST_ELEMENT (self);

  priv->context = NULL;
  priv->graph = NULL;
  priv->node = NULL;
  priv->num_channels = DEFAULT_NUM_CHANNELS;
  priv->queueable_objects = NULL;

  /* App common init */
  GST_DEBUG_OBJECT (self, "Running TIOVX common init");
  priv->tiovx_context = gst_tiovx_context_new ();

  gst_child_proxy_child_added (GST_CHILD_PROXY (element),
      G_OBJECT (aggregator->srcpad), GST_OBJECT_NAME (aggregator->srcpad));

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
    tivxImgProcUnLoadKernels (priv->context);
    tivxEdgeaiImgProcUnLoadKernels (priv->context);
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
  GstBuffer *original_buffer = NULL;
  GstTIOVXMisoPadPrivate *priv = NULL;
  vx_size num_channels = 0;
  GstCaps *caps = NULL;
  gboolean ret = FALSE;
  vx_status status = VX_FAILURE;
  gint i = 0;

  g_return_val_if_fail (pad, FALSE);

  priv = gst_tiovx_miso_pad_get_instance_private (pad);

  if (NULL == buffer) {
    GST_ERROR_OBJECT (pad,
        "Unable to validate pad exemplar, invalid buffer pointer");
    goto exit;
  }

  caps = gst_pad_get_current_caps (GST_PAD (pad));

  original_buffer = buffer;
  buffer =
      gst_tiovx_validate_tiovx_buffer (GST_CAT_DEFAULT,
      &priv->buffer_pool, buffer, *priv->exemplar, caps, priv->pool_size,
      priv->num_channels);
  gst_caps_unref (caps);
  if (NULL == buffer) {
    GST_ERROR_OBJECT (pad, "Unable to validate buffer");
    goto exit;
  }

  array =
      gst_tiovx_get_vx_array_from_buffer (GST_CAT_DEFAULT, *priv->exemplar,
      buffer);

  if (NULL == array) {
    GST_ERROR_OBJECT (pad, "Failed getting array from buffer");
    goto exit;
  }

  status =
      vxQueryObjectArray (array, VX_OBJECT_ARRAY_NUMITEMS, &num_channels,
      sizeof (num_channels));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (pad,
        "Get number of channels in input buffer failed %" G_GINT32_FORMAT,
        status);
    goto exit;
  }

  for (i = 0; i < num_channels; i++) {
    vx_reference gst_ref = vxGetObjectArrayItem (array, i);
    vx_reference modules_ref = NULL;

    if (priv->array) {
      modules_ref = vxGetObjectArrayItem (priv->array, i);
    } else {
      modules_ref = priv->exemplar[i];
    }

    status = gst_tiovx_transfer_handle (GST_CAT_DEFAULT, gst_ref, modules_ref);
    vxReleaseReference (&gst_ref);
    if (priv->array) {
      vxReleaseReference (&modules_ref);
    }
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (pad,
          "Error in input handle transfer %" G_GINT32_FORMAT, status);
      goto exit;
    }
  }

  ret = TRUE;

exit:
  if ((original_buffer != buffer) && (NULL != buffer)) {
    gst_buffer_unref (buffer);
    buffer = NULL;
  }

  return ret;
}

static GstFlowReturn
gst_tiovx_miso_process_graph (GstAggregator * agg)
{
  GstTIOVXMiso *tiovx_miso = NULL;
  GstTIOVXMisoPrivate *priv = NULL;
  GstTIOVXMisoPad *pad = NULL;
  GstTIOVXMisoPadPrivate *pad_priv = NULL;
  GList *l = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  vx_status status = VX_FAILURE;
  vx_reference dequeued_object = NULL;
  uint32_t num_refs = 0;

  g_return_val_if_fail (agg, ret);

  tiovx_miso = GST_TIOVX_MISO (agg);
  priv = gst_tiovx_miso_get_instance_private (tiovx_miso);

  /* Enqueueing parameters */
  GST_LOG_OBJECT (agg, "Enqueueing parameters");

  pad = GST_TIOVX_MISO_PAD (agg->srcpad);
  pad_priv = gst_tiovx_miso_pad_get_instance_private (pad);

  status =
      vxGraphParameterEnqueueReadyRef (priv->graph, pad_priv->graph_param_id,
      pad_priv->exemplar, 1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (agg, "Output enqueue failed %" G_GINT32_FORMAT, status);
    goto exit;
  }
  GST_LOG_OBJECT (agg, "Enqueued output  array of refs: %p\t with graph id: %d",
      pad_priv->exemplar, pad_priv->graph_param_id);

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    pad = l->data;
    pad_priv = gst_tiovx_miso_pad_get_instance_private (pad);

    if (pad_priv->graph_param_id == -1 || pad_priv->node_param_id == -1)
      continue;

    status =
        vxGraphParameterEnqueueReadyRef (priv->graph, pad_priv->graph_param_id,
        pad_priv->exemplar, 1);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (agg, "Input enqueue failed %" G_GINT32_FORMAT, status);
      goto exit;
    }

    GST_LOG_OBJECT (agg, "Enqueued input array of refs: %p\t with graph id: %d",
        pad_priv->exemplar, pad_priv->graph_param_id);
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);
    gint graph_param_id = -1;
    gint node_param_id = -1;
    vx_object_array array = NULL;
    vx_reference *exemplar = NULL;

    gst_tiovx_queueable_get_params (queueable_object, &array, &exemplar,
        &graph_param_id, &node_param_id);
    GST_LOG_OBJECT (agg,
        "Enqueueing queueable array of refs: %p\t with graph id: %d", exemplar,
        graph_param_id);
    status =
        vxGraphParameterEnqueueReadyRef (priv->graph, graph_param_id, exemplar,
        1);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (agg, "Queueable enqueue failed %" G_GINT32_FORMAT,
          status);
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
  GST_LOG_OBJECT (agg,
      "Dequeueing output array of refs: %p\t with graph id: %d",
      pad_priv->exemplar, pad_priv->graph_param_id);

  status =
      vxGraphParameterDequeueDoneRef (priv->graph, pad_priv->graph_param_id,
      &dequeued_object, 1, &num_refs);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (agg, "Output dequeue failed %" G_GINT32_FORMAT, status);
    goto exit;
  }

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    pad = l->data;
    pad_priv = gst_tiovx_miso_pad_get_instance_private (pad);

    if (pad_priv->graph_param_id == -1 || pad_priv->node_param_id == -1)
      continue;

    GST_LOG_OBJECT (agg,
        "Dequeueing input array of refs: %p\t with graph id: %d",
        pad_priv->exemplar, pad_priv->graph_param_id);
    status =
        vxGraphParameterDequeueDoneRef (priv->graph, pad_priv->graph_param_id,
        &dequeued_object, 1, &num_refs);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (agg, "Input enqueue failed %" G_GINT32_FORMAT, status);
      goto exit;
    }
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);
    gint graph_param_id = -1;
    gint node_param_id = -1;
    vx_object_array array = NULL;
    vx_reference *exemplar = NULL;

    gst_tiovx_queueable_get_params (queueable_object, &array, &exemplar,
        &graph_param_id, &node_param_id);
    GST_LOG_OBJECT (agg,
        "Dequeueing queueable array of refs: %p\t with graph id: %d", exemplar,
        graph_param_id);
    status =
        vxGraphParameterDequeueDoneRef (priv->graph, graph_param_id,
        &dequeued_object, 1, &num_refs);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (agg, "Queueable dequeue failed %" G_GINT32_FORMAT,
          status);
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
  GstTIOVXMisoClass *klass = GST_TIOVX_MISO_GET_CLASS (self);
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GList *l = NULL;
  gboolean subclass_ret = FALSE;
  GstClockTime pts = GST_CLOCK_TIME_NONE;
  GstClockTime dts = GST_CLOCK_TIME_NONE;
  GstClockTime duration = 0;
  gboolean all_pads_eos = TRUE;
  gboolean eos = FALSE;

  GST_LOG_OBJECT (self, "TIOVX Miso aggregate");

  ret = gst_tiovx_miso_create_output_buffer (self, &outbuf);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to acquire output buffer");
    goto exit;
  }

  if (!gst_tiovx_miso_buffer_to_valid_pad_exemplar (GST_TIOVX_MISO_PAD
          (agg->srcpad), outbuf)) {
    GST_ERROR_OBJECT (self, "Unable transfer data to output exemplar");
    goto unref_output;
  }

  /* Ensure valid references in the inputs */
  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *pad = l->data;
    GstTIOVXMisoPad *miso_pad = GST_TIOVX_MISO_PAD (pad);
    GstTIOVXMisoPadPrivate *miso_pad_priv =
        gst_tiovx_miso_pad_get_instance_private (miso_pad);
    GstBuffer *in_buffer = NULL;
    GstClockTime tmp_pts = 0;
    GstClockTime tmp_dts = 0;
    GstClockTime tmp_duration = 0;
    gboolean pad_is_eos = FALSE;

    pad_is_eos = gst_aggregator_pad_is_eos (pad);
    all_pads_eos &= pad_is_eos;


    if (pad_is_eos && miso_pad_priv->repeat_after_eos) {
      eos = FALSE;
      GST_LOG_OBJECT (pad, "ignoring EOS and re-using previous buffer");
      continue;
    } else if (pad_is_eos && !miso_pad_priv->repeat_after_eos) {
      eos = TRUE;
      break;
    }

    in_buffer = gst_aggregator_pad_peek_buffer (pad);
    if (in_buffer) {
      tmp_pts = GST_BUFFER_PTS (in_buffer);
      tmp_dts = GST_BUFFER_DTS (in_buffer);
      tmp_duration = GST_BUFFER_DURATION (in_buffer);

      /* Find the smallest timestamp and the largest duration */
      if (tmp_pts < pts) {
        pts = tmp_pts;
      }
      if (tmp_dts < dts) {
        dts = tmp_dts;
      }
      if (tmp_duration > duration) {
        duration = tmp_duration;
      }

      if (!gst_tiovx_miso_buffer_to_valid_pad_exemplar (GST_TIOVX_MISO_PAD
              (pad), in_buffer)) {
        GST_ERROR_OBJECT (pad, "Unable transfer data to input pad: %p exemplar",
            pad);
        ret = GST_FLOW_ERROR;
        goto unref_output;
      }

      if (NULL != in_buffer) {
        gst_buffer_unref (in_buffer);
        in_buffer = NULL;
      }
    } else {
      GST_LOG_OBJECT (pad, "pad: %" GST_PTR_FORMAT " has no buffers", pad);
    }
  }

  if (all_pads_eos || eos) {
    ret = GST_FLOW_EOS;
    goto unref_output;
  }

  /* Assign the smallest timestamp and the largest duration */
  GST_BUFFER_PTS (outbuf) = pts;
  GST_BUFFER_DTS (outbuf) = dts;
  GST_BUFFER_DURATION (outbuf) = duration;
  /* The offset and offset end is used to indicate a "buffer number", should be
   * monotically increasing. For now we are not messing with this and it is
   * assigned to -1 */
  GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET_FIXED_VALUE;
  GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_END_FIXED_VALUE;

  if (GST_BUFFER_PTS_IS_VALID (outbuf)) {
    GST_AGGREGATOR_PAD (agg->srcpad)->segment.position =
        GST_BUFFER_PTS (outbuf);
  }

  if (NULL != klass->preprocess) {
    subclass_ret = klass->preprocess (self);
    if (!subclass_ret) {
      GST_ERROR_OBJECT (self, "Subclass preprocess failed");
      goto unref_output;
    }
  }

  /* Graph processing */
  ret = gst_tiovx_miso_process_graph (agg);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to process graph");
    goto unref_output;
  }

  if (NULL != klass->postprocess) {
    subclass_ret = klass->postprocess (self);
    if (!subclass_ret) {
      GST_ERROR_OBJECT (self, "Subclass postprocess failed");
      goto unref_output;
    }
  }

  if (NULL != klass->add_outbuf_meta) {
    subclass_ret = klass->add_outbuf_meta (self, outbuf);
    if (!subclass_ret) {
      GST_ERROR_OBJECT (self, "Subclass add_outbuf_meta failed");
      goto unref_output;
    }
  }

  gst_aggregator_finish_buffer (agg, outbuf);

  /* Mark all input buffers as read  */
  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *pad = l->data;
    GstBuffer *in_buffer = NULL;

    in_buffer = gst_aggregator_pad_pop_buffer (pad);
    if (in_buffer) {
      gst_buffer_unref (in_buffer);
    }
  }

exit:
  return ret;

unref_output:
  if (outbuf) {
    gst_buffer_unref (outbuf);
  }
  return ret;
}

static GstFlowReturn
gst_tiovx_miso_create_output_buffer (GstTIOVXMiso * self, GstBuffer ** outbuf)
{
  GstAggregator *aggregator = NULL;
  GstBufferPool *pool;
  GstFlowReturn ret = GST_FLOW_ERROR;

  g_return_val_if_fail (self, ret);

  aggregator = GST_AGGREGATOR (self);

  pool = gst_aggregator_get_buffer_pool (aggregator);

  if (pool) {
    if (!gst_buffer_pool_is_active (pool)) {
      if (!gst_buffer_pool_set_active (pool, TRUE)) {
        GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
        goto exit;
      }
    }

    ret = gst_buffer_pool_acquire_buffer (pool, outbuf, NULL);
    gst_object_unref (pool);
    pool = NULL;
    ret = GST_FLOW_OK;
  } else {
    GST_ERROR_OBJECT (self,
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
  GstTIOVXMisoPadPrivate *pad_priv = NULL;
  GstBufferPool *pool = NULL;
  GstCaps *caps = NULL;
  vx_reference reference = NULL;
  gsize size = 0;
  gboolean ret = FALSE;
  gint num_channels = 0;

  GST_LOG_OBJECT (self, "Propose allocation");

  pad_priv = gst_tiovx_miso_pad_get_instance_private (tiovx_miso_pad);

  gst_query_parse_allocation (query, &caps, NULL);

  /* Ignore propose allocation if caps
     are not defined in query */
  if (!caps) {
    GST_WARNING_OBJECT (self,
        "Caps empty in query, ignoring propose allocation for pad %s",
        GST_PAD_NAME (GST_PAD (agg_pad)));
    goto exit;
  }


  if (!gst_structure_get_int (gst_caps_get_structure (caps, 0),
          "num-channels", &num_channels)) {
    num_channels = 1;
  }
  pad_priv->num_channels = num_channels;

  /* If the pad doesn't have an exemplar, we'll create a temporary one.
   * We'll add the final one after the caps have been negotiated
   */
  if (pad_priv->exemplar) {
    reference = *pad_priv->exemplar;
  } else {
    GST_INFO_OBJECT (self, "Using temporary reference for configuration");
    reference =
        gst_tiovx_get_exemplar_from_caps ((GObject *) self, GST_CAT_DEFAULT,
        priv->context, caps);
  }

  size = gst_tiovx_get_size_from_exemplar (reference);
  if (0 >= size) {
    GST_ERROR_OBJECT (self, "Failed to get size from exemplar");
    ret = FALSE;
    goto exit;
  }

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, pad_priv->pool_size,
      reference, size, pad_priv->num_channels, &pool);

  if (pad_priv->buffer_pool) {
    gst_object_unref (pad_priv->buffer_pool);
    pad_priv->buffer_pool = NULL;
  }

  pad_priv->buffer_pool = pool;

  if (NULL == pad_priv->exemplar) {
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
  GstTIOVXMisoPadPrivate *pad_priv = gst_tiovx_miso_pad_get_instance_private(
      GST_TIOVX_MISO_PAD(agg->srcpad));
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  GST_LOG_OBJECT (self, "Decide allocation");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *pool = NULL;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (NULL == pool) {
      GST_DEBUG_OBJECT (self, "No pool in query position: %d, ignoring", npool);
      gst_query_remove_nth_allocation_pool (query, npool);
      continue;
    }

    /* Use TIOVX pool if found */
    if (GST_TIOVX_IS_BUFFER_POOL (pool)) {
      GstStructure *config = NULL;
      GstCaps *caps = NULL;
      guint min_buffers = 0;
      guint max_buffers = 0;
      guint size = 0;

      config = gst_buffer_pool_get_config (pool);
      gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers);

      if (pad_priv->pool_size != max_buffers) {
        pad_priv->pool_size = max_buffers;

        if (priv->graph) {
          GST_INFO_OBJECT (self,
              "Out buffer pool size changed, Module already initialized, "
              "re-initializing");
          ret = gst_tiovx_miso_stop (agg);
          if (!ret) {
            GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
                ("Unable to deinit TIOVX module"), (NULL));
            ret = FALSE;
            goto exit;
          }
          if (!gst_tiovx_miso_modules_init (self)) {
            GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
                ("Unable to init TIOVX module"), (NULL));
            ret = FALSE;
            goto exit;
          }
        }
      }

      pool_needed = FALSE;

      GST_INFO_OBJECT (self, "TIOVX pool found, using this one: \"%s\"",
          GST_OBJECT_NAME (pool));
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
    GstCaps *caps = NULL;
    gsize size = 0;

    gst_query_parse_allocation (query, &caps, NULL);

    size = gst_tiovx_get_size_from_exemplar (*pad_priv->exemplar);
    if (0 >= size) {
      GST_ERROR_OBJECT (self, "Failed to get size from exemplar");
      ret = FALSE;
      goto exit;
    }

    ret =
        gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query,
        pad_priv->pool_size, *pad_priv->exemplar, size, pad_priv->num_channels,
        &pool);

    if (pad_priv->buffer_pool) {
      gst_object_unref (pad_priv->buffer_pool);
      pad_priv->buffer_pool = NULL;
    }
    pad_priv->buffer_pool = pool;
  }

exit:
  return ret;
}

static gboolean
gst_tiovx_miso_start (GstAggregator * agg)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (self, "start");

  /* Create OpenVX Context */
  GST_DEBUG_OBJECT (self, "Creating context");
  priv->context = vxCreateContext ();

  if (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context)) {
    tivxHwaLoadKernels (priv->context);
    tivxImgProcLoadKernels (priv->context);
    tivxEdgeaiImgProcLoadKernels (priv->context);
  }

  ret = TRUE;

  return ret;
}

static gboolean
gst_tiovx_miso_stop (GstAggregator * agg)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoClass *klass = GST_TIOVX_MISO_GET_CLASS (agg);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  GstTIOVXMisoPadPrivate *pad_priv = NULL;
  GList *l = NULL;
  gboolean ret = FALSE;
  gint i = 0;

  GST_DEBUG_OBJECT (self, "stop");

  if ((NULL == priv->graph)
      || (VX_SUCCESS != vxGetStatus ((vx_reference) priv->graph))) {
    GST_WARNING_OBJECT (self,
        "Trying to deinit modules but initialization was not completed, skipping...");
    ret = TRUE;
    goto free_common;
  }

  /* Empty exemplars to avoid double handlers free */
  pad_priv =
      gst_tiovx_miso_pad_get_instance_private (GST_TIOVX_MISO_PAD
      (agg->srcpad));
  for (i = 0; i < pad_priv->num_channels; i++) {
    vx_reference ref = NULL;

    if (pad_priv->array) {
      ref = vxGetObjectArrayItem (pad_priv->array, i);
    } else {
      ref = pad_priv->exemplar[i];
    }
    if (VX_SUCCESS != gst_tiovx_empty_exemplar (ref)) {
      GST_WARNING_OBJECT (self, "Failed to empty output exemplar");
    }
    if (pad_priv->array) {
      vxReleaseReference (&ref);
    }
  }

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    pad_priv = gst_tiovx_miso_pad_get_instance_private (l->data);

    for (i = 0; i < pad_priv->num_channels; i++) {
      vx_reference ref = NULL;

      if (pad_priv->array) {
        ref = vxGetObjectArrayItem (pad_priv->array, i);
      } else {
        ref = pad_priv->exemplar[i];
      }
      if (VX_SUCCESS != gst_tiovx_empty_exemplar (ref)) {
        GST_WARNING_OBJECT (self,
            "Failed to empty input exemplar in pad: %" GST_PTR_FORMAT,
            GST_PAD (l->data));
      }
      if (pad_priv->array) {
        vxReleaseReference (&ref);
      }
    }
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);
    vx_object_array array = NULL;
    vx_reference *exemplar = NULL;
    gint graph_param_id = -1;
    gint node_param_id = -1;

    gst_tiovx_queueable_get_params (queueable_object, &array, &exemplar,
        &graph_param_id, &node_param_id);

    for (i = 0; i < pad_priv->num_channels; i++) {
      if (array) {
        vx_reference ref = NULL;

        ref = vxGetObjectArrayItem (array, i);
        if (VX_SUCCESS != gst_tiovx_empty_exemplar (ref)) {
          GST_WARNING_OBJECT (self,
              "Failed to empty exemplar in queueable: %" GST_PTR_FORMAT,
              queueable_object);
        }
        vxReleaseReference (&ref);
      }
    }
  }

  g_list_free_full (priv->queueable_objects, g_object_unref);
  priv->queueable_objects = NULL;

  if (NULL == klass->deinit_module) {
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
  GstAggregator *agg = NULL;
  GList *sink_caps_list = NULL;
  GList *l = NULL;

  g_return_val_if_fail (self, sink_caps_list);

  agg = GST_AGGREGATOR (self);

  GST_DEBUG_OBJECT (self, "Generating sink caps list");

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstPad *sink_pad = GST_PAD (l->data);
    GstCaps *pad_caps = NULL;
    GstCaps *peer_caps = NULL;

    pad_caps = gst_pad_get_current_caps (sink_pad);
    if (NULL == pad_caps) {
      peer_caps = gst_pad_peer_query_caps (sink_pad, NULL);
      pad_caps = intersect_with_template_caps (peer_caps, sink_pad);
      gst_caps_unref (peer_caps);
    }

    GST_DEBUG_OBJECT (self, "Caps from %s:%s peer: %" GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (sink_pad), pad_caps);
    /* Insert at the end of the src caps list */
    sink_caps_list = g_list_insert (sink_caps_list, pad_caps, -1);
  }

  return sink_caps_list;
}

static GstCaps *
gst_tiovx_miso_default_fixate_caps (GstTIOVXMiso * self, GList * sink_caps_list,
    GstCaps * src_caps, gint * num_channels)
{
  GstCaps *fixated_src_caps = NULL;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (sink_caps_list, NULL);
  g_return_val_if_fail (src_caps, NULL);
  g_return_val_if_fail (num_channels, NULL);

  GST_DEBUG_OBJECT (self, "Fixating caps");

  fixated_src_caps = gst_caps_fixate (src_caps);

  return fixated_src_caps;
}

static gboolean
gst_tiovx_miso_modules_init (GstTIOVXMiso * self)
{
  GstAggregator *agg = NULL;
  GstTIOVXMisoPadPrivate *pad_priv = NULL;
  GstTIOVXMisoPad *miso_pad = NULL;
  GstTIOVXMisoClass *klass = NULL;
  GstTIOVXMisoPrivate *priv = NULL;
  GList *l = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  vx_graph_parameter_queue_params_t *params_list = NULL;
  guint num_pads_with_param_id = 0;
  gint graph_param_id = -1;
  gint node_param_id = -1;

  g_return_val_if_fail (self, FALSE);

  agg = GST_AGGREGATOR (self);
  priv = gst_tiovx_miso_get_instance_private (self);
  klass = GST_TIOVX_MISO_GET_CLASS (self);

  GST_DEBUG_OBJECT (self, "Initializing MISO module");

  status = vxGetStatus ((vx_reference) priv->context);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Context creation failed with error: %" G_GINT32_FORMAT, status);
    goto exit;
  }

  if (NULL == klass->init_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement init_module method.");
    goto exit;
  }
  if (!klass->init_module (self, priv->context, GST_ELEMENT (self)->sinkpads,
          agg->srcpad, priv->num_channels)) {
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
  if (NULL == klass->create_graph) {
    GST_ERROR_OBJECT (self, "Subclass did not implement create_graph method.");
    goto free_graph;
  }
  if (!klass->create_graph (self, priv->context, priv->graph)) {
    GST_ERROR_OBJECT (self, "Subclass create graph failed");
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Get node info");
  if (NULL == klass->get_node_info) {
    GST_ERROR_OBJECT (self, "Subclass did not implement get_node_info method");
    goto free_graph;
  }
  if (!klass->get_node_info (self, GST_ELEMENT (self)->sinkpads, agg->srcpad,
          &priv->node, &priv->queueable_objects)) {
    GST_ERROR_OBJECT (self, "Subclass get node info failed");
    goto free_graph;
  }

  for (l = GST_ELEMENT (self)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *pad = l->data;
    pad_priv =
        gst_tiovx_miso_pad_get_instance_private (GST_TIOVX_MISO_PAD (pad));

    if ((0 > pad_priv->graph_param_id) && (0 > pad_priv->node_param_id)
        && (NULL != pad_priv->exemplar)) {
      GST_DEBUG_OBJECT (self,
          "Pad: %" GST_PTR_FORMAT "configured with exemplar only", pad);
    } else if ((0 > pad_priv->graph_param_id) || (0 > pad_priv->node_param_id)
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

  if (NULL == priv->node) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: node missing");
    goto free_graph;
  }

  /* Count how many pads have a valid graph and node id */
  num_pads_with_param_id = 0;
  for (l = GST_ELEMENT (self)->sinkpads; l; l = g_list_next (l)) {
    miso_pad = GST_TIOVX_MISO_PAD (l->data);
    pad_priv = gst_tiovx_miso_pad_get_instance_private (miso_pad);

    if ((pad_priv->graph_param_id != -1) && (pad_priv->node_param_id != -1))
      num_pads_with_param_id++;
  }
  /* We add one more for the source pad */
  num_pads_with_param_id++;
  /* Add list size for all queueables */
  num_pads_with_param_id += g_list_length (priv->queueable_objects);

  GST_DEBUG_OBJECT (self, "Setting up parameters");
  params_list = g_malloc0 (num_pads_with_param_id * sizeof (*params_list));
  if (NULL == params_list) {
    GST_ERROR_OBJECT (self, "Could not allocate memory for parameters list");
    goto free_graph;
  }

  for (l = GST_ELEMENT (self)->sinkpads; l; l = g_list_next (l)) {
    miso_pad = GST_TIOVX_MISO_PAD (l->data);
    pad_priv = gst_tiovx_miso_pad_get_instance_private (miso_pad);

    if (pad_priv->graph_param_id == -1 || pad_priv->node_param_id == -1)
      continue;

    status =
        add_graph_parameter_by_node_index (gst_tiovx_miso_pad_debug_category,
        G_OBJECT (self), priv->graph, priv->node, pad_priv->graph_param_id,
        pad_priv->node_param_id, params_list, pad_priv->exemplar);
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
      add_graph_parameter_by_node_index (gst_tiovx_miso_pad_debug_category,
      G_OBJECT (self), priv->graph, priv->node, pad_priv->graph_param_id,
      pad_priv->node_param_id, params_list, pad_priv->exemplar);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Setting output parameter failed, vx_status %" G_GINT32_FORMAT, status);
    goto free_parameters_list;
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);
    vx_object_array array = NULL;
    vx_reference *exemplar = NULL;

    gst_tiovx_queueable_get_params (queueable_object, &array, &exemplar,
        &graph_param_id, &node_param_id);
    status =
        add_graph_parameter_by_node_index (gst_tiovx_miso_pad_debug_category,
        G_OBJECT (self), priv->graph, priv->node, graph_param_id, node_param_id,
        params_list, exemplar);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Setting queueable parameter failed, vx_status %" G_GINT32_FORMAT,
          status);
      goto free_parameters_list;
    }
  }

  GST_DEBUG_OBJECT (self, "Schedule Config");
  status = vxSetGraphScheduleConfig (priv->graph,
      VX_GRAPH_SCHEDULE_MODE_QUEUE_MANUAL, num_pads_with_param_id, params_list);
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
  if (NULL == klass->release_buffer) {
    GST_ERROR_OBJECT (self,
        "Subclass did not implement release buffer method. Skipping node configuration");
    goto free_graph;
  }
  ret = klass->release_buffer (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass release buffer failed");
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Configure Module");
  if (NULL == klass->configure_module) {
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
  if (NULL == klass->deinit_module) {
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
  GstTIOVXMisoPrivate *priv = NULL;
  GstTIOVXMisoClass *klass = NULL;
  GList *sink_caps_list = NULL;
  GstCaps *fixated_caps = NULL;
  GstTIOVXMisoPadPrivate *src_pad_priv = NULL;
  gint num_channels = -1;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (src_caps, NULL);


  priv = gst_tiovx_miso_get_instance_private (self);
  klass = GST_TIOVX_MISO_GET_CLASS (self);

  sink_caps_list = gst_tiovx_miso_get_sink_caps_list (self);

  /* Should return the fixated caps the element will use on the src pads */
  fixated_caps =
      klass->fixate_caps (self, sink_caps_list, src_caps, &num_channels);
  if (NULL == fixated_caps) {
    GST_ERROR_OBJECT (self, "Subclass did not fixate caps");
    goto exit;
  }

  if (num_channels == -1) {
    if ((0 >= gst_caps_get_size (fixated_caps))
        || !gst_structure_get_int (gst_caps_get_structure (fixated_caps, 0),
            "num-channels", &num_channels)) {
      num_channels = 1;
    }
  }
  priv->num_channels = num_channels;


  src_pad_priv =
      gst_tiovx_miso_pad_get_instance_private (GST_TIOVX_MISO_PAD
      (agg->srcpad));
  if ((0 >= gst_caps_get_size (fixated_caps))
      || !gst_structure_get_int (gst_caps_get_structure (fixated_caps, 0),
          "num-channels", &src_pad_priv->num_channels)) {
    src_pad_priv->num_channels = 1;
  }

exit:
  return fixated_caps;
}

gboolean
gst_tiovx_miso_negotiated_src_caps (GstAggregator * agg, GstCaps * caps)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (agg);
  GstTIOVXMisoPrivate *priv = NULL;
  gboolean ret = FALSE;
  GList *l = NULL;

  GST_DEBUG_OBJECT (self, "Negotiated src caps");

  /* We are calling this manually to ensure that during module initialization
   * the src pad has all the information. Normally this would be called by
   * GstAggregator right after the negotiated_src_caps
   */
  if (!gst_caps_is_fixed (caps)) {
    GST_ERROR_OBJECT (self,
        "Unable to set Aggregator source caps. Negotiated source caps aren't fixed");
    goto exit;
  }
  gst_aggregator_set_src_caps (agg, caps);

  priv = gst_tiovx_miso_get_instance_private (self);

  if (priv->graph) {
    GST_INFO_OBJECT (self,
        "Module already initialized, calling deinit before reinitialization");
    ret = gst_tiovx_miso_stop (agg);
    if (!ret) {
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
          ("Unable to deinit TIOVX module"), (NULL));
      goto exit;
    }
  }

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
  GstTIOVXMiso *miso = GST_TIOVX_MISO (element);
  GST_DEBUG_OBJECT (miso, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));
  gst_child_proxy_child_removed (GST_CHILD_PROXY (miso), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));
  GST_ELEMENT_CLASS (gst_tiovx_miso_parent_class)->release_pad (element, pad);
}

static GstCaps *
intersect_with_template_caps (GstCaps * caps, GstPad * pad)
{
  GstCaps *template_caps = NULL;
  GstCaps *filtered_caps = NULL;

  g_return_val_if_fail (pad, NULL);

  if (caps) {
    template_caps = gst_pad_get_pad_template_caps (pad);

    filtered_caps = gst_caps_intersect (caps, template_caps);
    gst_caps_unref (template_caps);
  }

  return filtered_caps;
}

/* GstChildProxy implementation */
static GObject *
gst_tiovx_miso_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  return NULL;
}

static GObject *
gst_tiovx_miso_child_proxy_get_child_by_name (GstChildProxy *
    child_proxy, const gchar * name)
{
  GstTIOVXMiso *self = NULL;
  GObject *obj = NULL;

  self = GST_TIOVX_MISO (child_proxy);

  GST_OBJECT_LOCK (self);

  if (0 == strcmp (name, "src")) {
    /* Only one src pad for MISO class */
    obj = G_OBJECT (GST_AGGREGATOR (self)->srcpad);
    if (obj) {
      gst_object_ref (obj);
    }
  } else {                      /* sink pad case */
    GList *node = NULL;
    node = GST_ELEMENT_CAST (self)->sinkpads;
    for (; node; node = g_list_next (node)) {
      if (0 == strcmp (name, GST_OBJECT_NAME (node->data))) {
        obj = G_OBJECT (node->data);
        gst_object_ref (obj);
        break;
      }
    }
  }

  GST_OBJECT_UNLOCK (self);

  return obj;
}

static guint
gst_tiovx_miso_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  GstTIOVXMiso *self = NULL;
  guint count = 0;

  self = GST_TIOVX_MISO (child_proxy);

  GST_OBJECT_LOCK (self);
  /* Number of source pads (always 1) + number of sink pads */
  count = GST_ELEMENT_CAST (self)->numsinkpads + 1;
  GST_OBJECT_UNLOCK (self);
  GST_INFO_OBJECT (self, "Children Count: %d", count);

  return count;
}

static void
gst_tiovx_miso_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  iface->get_child_by_index = gst_tiovx_miso_child_proxy_get_child_by_index;
  iface->get_child_by_name = gst_tiovx_miso_child_proxy_get_child_by_name;
  iface->get_children_count = gst_tiovx_miso_child_proxy_get_children_count;
}
