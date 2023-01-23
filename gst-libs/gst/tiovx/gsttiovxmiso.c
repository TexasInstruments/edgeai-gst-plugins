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

#define GST_BUFFER_OFFSET_FIXED_VALUE -1
#define GST_BUFFER_OFFSET_END_FIXED_VALUE -1
#define DEFAULT_NUM_CHANNELS 1
#define MAX_BUFFER_DEPTH_SINK_PADS 4

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_miso_debug_category

#define GST_TIOVX_MISO_GET_CLASS(obj)  \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TIOVX_MISO, \
                              GstTIOVXMisoClass))
/* TIOVX Miso Pad */

typedef struct _GstTIOVXMisoPadPrivate
{
  GstTIOVXPad parent;

  gboolean repeat_after_eos;
  gboolean is_eos;
  GList *buffers;
} GstTIOVXMisoPadPrivate;

enum
{
  PROP_PAD_0,
  PROP_PAD_REPEAT_AFTER_EOS,
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_miso_pad_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXMisoPad, gst_tiovx_miso_pad,
    GST_TYPE_TIOVX_PAD, G_ADD_PRIVATE (GstTIOVXMisoPad)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_miso_pad_debug_category,
        "tiovxmisopad", 0, "debug category for TIOVX miso pad class"));

static void
gst_tiovx_miso_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMisoPad *pad = GST_TIOVX_MISO_PAD (object);
  GstTIOVXMisoPadPrivate *priv = gst_tiovx_miso_pad_get_instance_private (pad);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
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
gst_tiovx_miso_pad_class_init (GstTIOVXMisoPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_tiovx_miso_pad_set_property;
  gobject_class->get_property = gst_tiovx_miso_pad_get_property;

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

  priv->repeat_after_eos = DEFAULT_REPEAT_AFTER_EOS;
  priv->is_eos = FALSE;
  priv->buffers = NULL;
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
  gint num_channels;

  GList *queueable_objects;
  GstTIOVXPad *srcpad;
  GList *sinkpads;

} GstTIOVXMisoPrivate;

#define GST_TIOVX_MISO_DEFINE_CUSTOM_CODE \
  G_ADD_PRIVATE (GstTIOVXMiso); \
  GST_DEBUG_CATEGORY_INIT (gst_tiovx_miso_debug_category, "tiovxmiso", 0, "debug category for the tiovxmiso element"); \
  G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,  gst_tiovx_miso_child_proxy_init);

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXMiso, gst_tiovx_miso,
    GST_TYPE_ELEMENT, GST_TIOVX_MISO_DEFINE_CUSTOM_CODE);

static void gst_tiovx_miso_finalize (GObject * obj);
static GstFlowReturn gst_tiovx_miso_aggregate (GstTIOVXMiso * self);
static gboolean gst_tiovx_miso_start (GstTIOVXMiso * self);
static gboolean gst_tiovx_miso_stop (GstTIOVXMiso * self);
static GList *gst_tiovx_miso_get_sink_caps_list (GstTIOVXMiso * self);
static gboolean gst_tiovx_miso_set_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps);
static GstCaps *gst_tiovx_miso_default_get_sink_caps (GstTIOVXMiso * self,
    GstCaps * filter, GstCaps * src_caps, GstTIOVXPad * sink_pad);
static GstCaps *gst_tiovx_miso_default_get_src_caps (GstTIOVXMiso * self,
    GstCaps * filter, GList * sink_caps_list, GList *sink_pads);
static GstCaps *gst_tiovx_miso_default_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels);
static gboolean gst_tiovx_miso_modules_init (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps);
static GstPad *gst_tiovx_miso_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps);
static void gst_tiovx_miso_release_pad (GstElement * element, GstPad * pad);
static GstCaps *intersect_with_template_caps (GstCaps * caps, GstPad * pad);
static GstStateChangeReturn
gst_tiovx_miso_change_state (GstElement * element, GstStateChange transition);
static gboolean gst_tiovx_miso_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_tiovx_miso_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstFlowReturn gst_tiovx_miso_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static gboolean gst_tiovx_miso_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_tiovx_miso_process_graph (GstTIOVXMiso * self);
static gboolean gst_tiovx_miso_trigger_downstream_pads (GstTIOVXPad * srcpad);

guint
gst_tiovx_miso_get_num_pads (GstTIOVXMiso * self)
{
  GstTIOVXMisoPrivate *priv = NULL;
  guint result = 0;

  g_return_val_if_fail (GST_IS_TIOVX_MISO (self), FALSE);

  priv = gst_tiovx_miso_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  result = g_list_length (priv->sinkpads);
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_tiovx_miso_class_init (GstTIOVXMisoClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_miso_finalize);

  klass->get_sink_caps =
    GST_DEBUG_FUNCPTR (gst_tiovx_miso_default_get_sink_caps);
  klass->get_src_caps = GST_DEBUG_FUNCPTR (gst_tiovx_miso_default_get_src_caps);
  klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_tiovx_miso_default_fixate_caps);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_change_state);
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_release_pad);
}

static void
gst_tiovx_miso_init (GstTIOVXMiso * self)
{
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  GstTIOVXMisoClass *klass = GST_TIOVX_MISO_GET_CLASS (self);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *pad_template = NULL;

  GST_DEBUG_OBJECT (self, "gst_tiovx_miso_init");

  pad_template = gst_element_class_get_pad_template (gstelement_class, "src");
  g_return_if_fail (pad_template != NULL);

  priv->srcpad =
      GST_TIOVX_PAD (gst_pad_new_from_template (pad_template, "src"));
  if (!GST_TIOVX_IS_PAD (priv->srcpad)) {
    GST_ERROR_OBJECT (self, "Requested pad from template isn't a TIOVX pad");
    return;
  }

  gst_pad_set_query_function (GST_PAD (priv->srcpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_src_query));
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD (priv->srcpad));
  gst_pad_set_active (GST_PAD (priv->srcpad), FALSE);

  priv->context = NULL;
  priv->graph = NULL;
  priv->node = NULL;
  priv->num_channels = DEFAULT_NUM_CHANNELS;
  priv->queueable_objects = NULL;
  priv->sinkpads = NULL;

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
    tivxImgProcUnLoadKernels (priv->context);
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

static GstStateChangeReturn
gst_tiovx_miso_change_state (GstElement * element, GstStateChange transition)
{
  GstTIOVXMiso *self = NULL;
  gboolean ret = FALSE;
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;
  GstTIOVXMisoClass *klass = NULL;

  self = GST_TIOVX_MISO (element);
  klass = GST_TIOVX_MISO_GET_CLASS (self);

  GST_DEBUG_OBJECT (self, "gst_tiovx_miso_change_state");

  switch (transition) {
      /* "Start" transition */
    case GST_STATE_CHANGE_NULL_TO_READY:
      ret = gst_tiovx_miso_start (self);
      if (!ret) {
        GST_DEBUG_OBJECT (self,
            "Failed to init module in NULL to READY transition");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (klass)->change_state (element, transition);

  switch (transition) {
      /* "Stop" transition */
    case GST_STATE_CHANGE_READY_TO_NULL:
      ret = gst_tiovx_miso_stop (self);
      if (!ret) {
        GST_DEBUG_OBJECT (self,
            "Failed to deinit module in READY to NULL transition");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  return result;
}

static GstFlowReturn
gst_tiovx_miso_process_graph (GstTIOVXMiso * self)
{
  GstTIOVXMisoPrivate *priv = NULL;
  GList *l = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  vx_status status = VX_FAILURE;
  vx_reference dequeued_object = NULL;
  uint32_t num_refs = 0;
  gint graph_param_id = -1;
  gint node_param_id = -1;
  vx_reference *exemplar = NULL;
  vx_object_array array = NULL;

  priv = gst_tiovx_miso_get_instance_private (self);

  /* Enqueueing parameters */
  GST_LOG_OBJECT (self, "Enqueueing parameters");

  gst_tiovx_pad_get_params (priv->srcpad, &array, &exemplar, &graph_param_id,
      &node_param_id);
  status =
      vxGraphParameterEnqueueReadyRef (priv->graph, graph_param_id, exemplar,
          1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output enqueue failed %" G_GINT32_FORMAT, status);
    goto exit;
  }
  GST_LOG_OBJECT (self, "Enqueued output  array of refs: %p\t with graph id: %d",
      exemplar, graph_param_id);

  for (l = priv->sinkpads; l; l = g_list_next (l)) {
    GstTIOVXPad *pad = GST_TIOVX_PAD(l->data);

    gst_tiovx_pad_get_params (pad, &array, &exemplar, &graph_param_id,
        &node_param_id);
    status =
        vxGraphParameterEnqueueReadyRef (priv->graph, graph_param_id, exemplar,
            1);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Input enqueue failed %" G_GINT32_FORMAT, status);
      goto exit;
    }

    GST_LOG_OBJECT (self, "Enqueued input array of refs: %p\t with graph id: %d",
        exemplar, graph_param_id);
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);
    gint graph_param_id = -1;
    gint node_param_id = -1;
    vx_object_array array = NULL;
    vx_reference *exemplar = NULL;

    gst_tiovx_queueable_get_params (queueable_object, &array, &exemplar,
        &graph_param_id, &node_param_id);
    GST_LOG_OBJECT (self,
        "Enqueueing queueable array of refs: %p\t with graph id: %d", exemplar,
        graph_param_id);
    status =
        vxGraphParameterEnqueueReadyRef (priv->graph, graph_param_id, exemplar,
        1);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Queueable enqueue failed %" G_GINT32_FORMAT,
          status);
      goto exit;
    }
  }

  /* Processing graph */
  GST_LOG_OBJECT (self, "Processing graph");
  status = vxScheduleGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Schedule graph failed %" G_GINT32_FORMAT, status);
    goto exit;
  }
  status = vxWaitGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Wait graph failed %" G_GINT32_FORMAT, status);
    goto exit;
  }

  /* Dequeueing parameters */
  GST_LOG_OBJECT (self, "Dequeueing parameters");
  gst_tiovx_pad_get_params (priv->srcpad, &array, &exemplar, &graph_param_id,
      &node_param_id);
  GST_LOG_OBJECT (self,
      "Dequeueing output array of refs: %p\t with graph id: %d",
      exemplar, graph_param_id);

  status =
      vxGraphParameterDequeueDoneRef (priv->graph, graph_param_id,
          &dequeued_object, 1, &num_refs);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output dequeue failed %" G_GINT32_FORMAT, status);
    goto exit;
  }

  for (l = priv->sinkpads; l; l = g_list_next (l)) {
    GstTIOVXPad *pad = GST_TIOVX_PAD(l->data);;

    gst_tiovx_pad_get_params (pad, &array, &exemplar, &graph_param_id,
        &node_param_id);

    GST_LOG_OBJECT (self,
        "Dequeueing input array of refs: %p\t with graph id: %d",
        exemplar, graph_param_id);
    status =
        vxGraphParameterDequeueDoneRef (priv->graph, graph_param_id,
        &dequeued_object, 1, &num_refs);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Input enqueue failed %" G_GINT32_FORMAT, status);
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
    GST_LOG_OBJECT (self,
        "Dequeueing queueable array of refs: %p\t with graph id: %d", exemplar,
        graph_param_id);
    status =
        vxGraphParameterDequeueDoneRef (priv->graph, graph_param_id,
        &dequeued_object, 1, &num_refs);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Queueable dequeue failed %" G_GINT32_FORMAT,
          status);
      goto exit;
    }
  }

  ret = GST_FLOW_OK;

exit:
  return ret;
}

static GstFlowReturn
gst_tiovx_miso_aggregate (GstTIOVXMiso * self)
{
  GstTIOVXMisoClass *klass = GST_TIOVX_MISO_GET_CLASS (self);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
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

  ret = gst_tiovx_pad_acquire_buffer (priv->srcpad, &outbuf, NULL);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to acquire output buffer");
    goto exit;
  }

  /* Ensure valid references in the inputs */
  for (l = priv->sinkpads; l; l = g_list_next (l)) {
    GstTIOVXPad *pad = l->data;
    GstTIOVXMisoPad *miso_pad = GST_TIOVX_MISO_PAD (pad);
    GstTIOVXMisoPadPrivate *miso_pad_priv =
        gst_tiovx_miso_pad_get_instance_private (miso_pad);
    GstBuffer *in_buffer = NULL;
    GstClockTime tmp_pts = 0;
    GstClockTime tmp_dts = 0;
    GstClockTime tmp_duration = 0;
    gboolean pad_is_eos = FALSE;

    pad_is_eos = miso_pad_priv->is_eos;
    all_pads_eos &= pad_is_eos;


    if (pad_is_eos && miso_pad_priv->repeat_after_eos) {
      eos = FALSE;
      GST_LOG_OBJECT (pad, "ignoring EOS and re-using previous buffer");
      continue;
    } else if (pad_is_eos && !miso_pad_priv->repeat_after_eos) {
      eos = TRUE;
      break;
    }

    in_buffer = (GstBuffer *)(miso_pad_priv->buffers->data);
    miso_pad_priv->buffers = g_list_remove_link (miso_pad_priv->buffers,
        miso_pad_priv->buffers);
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

      ret = gst_tiovx_pad_chain (GST_PAD(pad), GST_OBJECT(self), &in_buffer);
      if (GST_FLOW_OK != ret) {
        GST_ERROR_OBJECT (pad, "Pad's chain function failed");
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

  if (NULL != klass->preprocess) {
    subclass_ret = klass->preprocess (self);
    if (!subclass_ret) {
      GST_ERROR_OBJECT (self, "Subclass preprocess failed");
      goto unref_output;
    }
  }

  /* Graph processing */
  ret = gst_tiovx_miso_process_graph (self);
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

  ret = gst_pad_push (GST_PAD(priv->srcpad), outbuf);

exit:
  return ret;

unref_output:
  if (outbuf) {
    gst_buffer_unref (outbuf);
  }
  return ret;
}

static gboolean
gst_tiovx_miso_start (GstTIOVXMiso * self)
{
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (self, "start");

  /* Create OpenVX Context */
  GST_DEBUG_OBJECT (self, "Creating context");
  priv->context = vxCreateContext ();

  if (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context)) {
    tivxHwaLoadKernels (priv->context);
    tivxImgProcLoadKernels (priv->context);
  }

  ret = TRUE;

  return ret;
}

static gboolean
gst_tiovx_miso_stop (GstTIOVXMiso * self)
{
  GstTIOVXMisoClass *klass = GST_TIOVX_MISO_GET_CLASS (self);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  GList *l = NULL;
  gboolean ret = FALSE;
  vx_object_array array = NULL;
  vx_reference *exemplar = NULL;
  gint graph_param_id = 0, node_param_id = 0;
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
  gst_tiovx_pad_get_params (priv->srcpad, &array, &exemplar, &graph_param_id,
      &node_param_id);
  for (i = 0; i < priv->num_channels; i++) {
    vx_reference ref = NULL;

    ref = vxGetObjectArrayItem (array, i);
    if (VX_SUCCESS != gst_tiovx_empty_exemplar (ref)) {
      GST_WARNING_OBJECT (self, "Failed to empty output exemplar");
    }
    vxReleaseReference (&ref);
  }

  for (l = priv->sinkpads; l; l = g_list_next (l)) {
    GstTIOVXPad *pad = GST_TIOVX_PAD (l->data);
    gst_tiovx_pad_get_params (pad, &array, &exemplar, &graph_param_id,
        &node_param_id);

    for (i = 0; i < priv->num_channels; i++) {
      vx_reference ref = NULL;

      ref = vxGetObjectArrayItem (array, i);
      if (VX_SUCCESS != gst_tiovx_empty_exemplar (ref)) {
        GST_WARNING_OBJECT (self,
            "Failed to empty output in pad: %" GST_PTR_FORMAT, pad);
      }
      vxReleaseReference (&ref);
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

    for (i = 0; i < priv->num_channels; i++) {
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
  GstTIOVXMisoPrivate *priv = NULL;

  GList *sink_caps_list = NULL;
  GList *node = NULL;

  g_return_val_if_fail (self, NULL);

  priv = gst_tiovx_miso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "Generating sink caps list");

  for (node = priv->sinkpads; node; node = g_list_next (node)) {
    GstPad *sink_pad = GST_PAD (node->data);
    GstCaps *peer_caps = gst_pad_peer_query_caps (sink_pad, NULL);
    GstCaps *pad_caps = NULL;

    pad_caps = intersect_with_template_caps (peer_caps, sink_pad);

    gst_caps_unref (peer_caps);

    GST_DEBUG_OBJECT (self, "Caps from %s:%s peer: %" GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (sink_pad), pad_caps);
    /* Insert at the end of the sink caps list */
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

static GstFlowReturn
gst_tiovx_miso_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstTIOVXMiso *self = NULL;
  GstTIOVXMisoPrivate *priv = NULL;
  GstTIOVXMisoPad *miso_pad = NULL;
  GstTIOVXMisoPadPrivate *pad_priv = NULL;
  GList *node = NULL;
  gboolean aggregate = TRUE;

  miso_pad = GST_TIOVX_MISO_PAD (pad);
  pad_priv = gst_tiovx_miso_pad_get_instance_private (miso_pad);
  self = GST_TIOVX_MISO (parent);
  priv = gst_tiovx_miso_get_instance_private (self);

  pad_priv->buffers = g_list_append (pad_priv->buffers, buffer);

  for (node = priv->sinkpads; node; node = g_list_next (node)) {
    GstTIOVXPad *sink_pad = node->data;
    miso_pad = GST_TIOVX_MISO_PAD (sink_pad);
    pad_priv = gst_tiovx_miso_pad_get_instance_private (miso_pad);

    if (g_list_length (pad_priv->buffers) == 0) {
      aggregate = FALSE;
    } else if (g_list_length (pad_priv->buffers) >=
        MAX_BUFFER_DEPTH_SINK_PADS) {
      aggregate = TRUE;
      break;
    }
  }

  if (aggregate) {
    ret = gst_tiovx_miso_aggregate (self);
  } else {
    ret = GST_FLOW_OK;
  }

  return ret;
}

static gboolean
gst_tiovx_miso_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (parent);
  GstTIOVXMisoClass *klass = GST_TIOVX_MISO_GET_CLASS (self);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  GstTIOVXMisoPad *miso_pad = GST_TIOVX_MISO_PAD (pad);
  GstTIOVXMisoPadPrivate *pad_priv =
    gst_tiovx_miso_pad_get_instance_private (miso_pad);
  GList *caps_node = NULL;
  gboolean ret = FALSE;
  GstPad *src_pad = GST_PAD (priv->srcpad);
  GstCaps *peer_caps = NULL;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *src_caps = NULL;
      GList *sink_caps_list = NULL;
      GstCaps *fixated_caps = NULL;

      sink_caps_list = gst_tiovx_miso_get_sink_caps_list (self);

      for (caps_node = sink_caps_list; caps_node;
          caps_node = g_list_next (caps_node)) {
        GstCaps *sink_caps = (GstCaps *) caps_node->data;
        if (!gst_caps_is_fixed(sink_caps)) {
          g_list_free_full (sink_caps_list, (GDestroyNotify) gst_caps_unref);
          GST_DEBUG_OBJECT (self, "All sink caps are not fixated");
          gst_event_unref (event);
          ret = gst_pad_event_default (GST_PAD (pad), parent, event);
          return ret;
        }
      }

      peer_caps = gst_pad_peer_query_caps (src_pad, NULL);
      src_caps = intersect_with_template_caps (peer_caps, src_pad);
      gst_caps_unref (peer_caps);

      /* Should return the fixated caps the element will use on the src pad */
      fixated_caps = klass->fixate_caps (self, sink_caps_list, src_caps, &priv->num_channels);
      if (NULL == fixated_caps) {
        GST_ERROR_OBJECT (self, "Subclass did not fixate caps");
        gst_event_unref (event);
        break;
      }

      ret =
          gst_tiovx_miso_set_caps (self, sink_caps_list, fixated_caps);
      if (!ret) {
        GST_ERROR_OBJECT (self, "Set caps method failed");
        gst_event_unref (event);
        break;
      }

      /* Discard previous list of sink caps and src caps*/
      g_list_free_full (sink_caps_list, (GDestroyNotify) gst_caps_unref);
      gst_caps_unref(src_caps);

      /* Notify peer pads downstream about fixated caps in this pad */
      gst_pad_push_event (src_pad, gst_event_new_caps (fixated_caps));

      gst_caps_unref(fixated_caps);
      gst_event_unref (event);

      ret = gst_tiovx_miso_trigger_downstream_pads (priv->srcpad);
      break;
    }
    case GST_EVENT_EOS:
      pad_priv->is_eos = TRUE;
      ret = gst_pad_event_default (GST_PAD (pad), parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      pad_priv->is_eos = FALSE;
      ret = gst_pad_event_default (GST_PAD (pad), parent, event);
      break;
    default:
      ret = gst_pad_event_default (GST_PAD (pad), parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_tiovx_miso_modules_init (GstTIOVXMiso * self, GList * sink_caps_list,
    GstCaps * src_caps)
{
  GstTIOVXMisoClass *klass = NULL;
  GstTIOVXMisoPrivate *priv = NULL;
  GstTIOVXPad *pad = NULL;
  GList *l = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  vx_graph_parameter_queue_params_t *params_list = NULL;
  guint num_pads = 0;
  gint graph_param_id = -1;
  gint node_param_id = -1;
  vx_reference *exemplar = NULL;
  vx_object_array array = NULL;
  gint num_parameters = 0;

  g_return_val_if_fail (self, ret);
  g_return_val_if_fail (sink_caps_list, ret);
  g_return_val_if_fail (src_caps, ret);

  priv = gst_tiovx_miso_get_instance_private (self);
  klass = GST_TIOVX_MISO_GET_CLASS (self);

  num_pads = gst_tiovx_miso_get_num_pads (self);

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
  ret =
      klass->init_module (self, priv->context, priv->sinkpads, priv->srcpad,
          priv->num_channels);
  if (!ret) {
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
  ret = klass->create_graph (self, priv->context, priv->graph);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass create graph failed");
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Get node info");
  if (NULL == klass->get_node_info) {
    GST_ERROR_OBJECT (self, "Subclass did not implement get_node_info method");
    goto free_graph;
  }
  ret =
      klass->get_node_info (self, priv->sinkpads, priv->srcpad, &priv->node,
          &priv->queueable_objects);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass get node info failed");
    goto free_graph;
  }

  if (NULL == priv->node) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: node missing");
    goto free_graph;
  }
  if (0 == num_pads) {
    GST_ERROR_OBJECT (self,
        "Incomplete info from subclass: number of graph parameters is 0");
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Setting up parameters");
  /* Parameters equals, number of input pads, a single output pad and all queueable objects */
  num_parameters = num_pads + 1 + g_list_length (priv->queueable_objects);
  params_list = g_malloc0 (num_parameters * sizeof (*params_list));
  if (NULL == params_list) {
    GST_ERROR_OBJECT (self, "Could not allocate memory for parameters list");
    goto free_graph;
  }

  gst_tiovx_pad_get_params (priv->srcpad, &array, &exemplar, &graph_param_id,
      &node_param_id);
  status =
      add_graph_parameter_by_node_index (gst_tiovx_miso_debug_category,
      G_OBJECT (self), priv->graph, priv->node, graph_param_id, node_param_id,
      params_list, exemplar);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Setting output parameter failed, vx_status %" G_GINT32_FORMAT, status);
    goto free_parameters_list;
  }

  for (l = priv->sinkpads; l; l = g_list_next (l)) {
    pad = GST_TIOVX_PAD (l->data);
    gst_tiovx_pad_get_params (pad, &array, &exemplar, &graph_param_id,
        &node_param_id);
    status =
        add_graph_parameter_by_node_index (gst_tiovx_miso_debug_category,
        G_OBJECT (self), priv->graph, priv->node, graph_param_id, node_param_id,
        params_list, exemplar);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Setting input parameter failed, vx_status %" G_GINT32_FORMAT,
          status);
      goto free_parameters_list;
    }
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);

    gst_tiovx_queueable_get_params (queueable_object, &array, &exemplar,
        &graph_param_id, &node_param_id);
    status =
        add_graph_parameter_by_node_index (gst_tiovx_miso_debug_category,
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
      VX_GRAPH_SCHEDULE_MODE_QUEUE_MANUAL, num_parameters, params_list);
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
  if (NULL == klass->configure_module) {
    GST_LOG_OBJECT (self,
        "Subclass did not implement configure node method. Skipping node configuration");
  } else {
    ret = klass->configure_module (self);
    if (!ret) {
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
  klass->deinit_module (self);

exit:
  return ret;
}

static GstPad *
gst_tiovx_miso_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstPad *newpad;
  GstTIOVXMiso *self = GST_TIOVX_MISO (element);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);

  newpad = (GstPad *)
      GST_ELEMENT_CLASS (gst_tiovx_miso_parent_class)->request_new_pad (element,
      templ, req_name, caps);
  if (newpad == NULL)
    goto could_not_create;
  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));

  GST_OBJECT_LOCK (self);
  priv->sinkpads = g_list_append (priv->sinkpads, gst_object_ref (newpad));

  gst_pad_set_event_function (newpad,
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_sink_event));
  gst_pad_set_query_function (newpad,
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_sink_query));
  gst_pad_set_chain_function (newpad,
      GST_DEBUG_FUNCPTR (gst_tiovx_miso_chain));
  GST_OBJECT_UNLOCK (self);

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

static GstCaps *
gst_tiovx_miso_default_get_sink_caps (GstTIOVXMiso *self,
    GstCaps *filter, GstCaps *src_caps, GstTIOVXPad *sink_pad)
{
  GstCaps *ret = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (src_caps, FALSE);

  ret = gst_caps_copy (src_caps);

  if (filter) {
    GstCaps *tmp = ret;
    ret = gst_caps_intersect (ret, filter);
    gst_caps_unref (tmp);
  }

  return ret;
}

static GstCaps *
gst_tiovx_miso_default_get_src_caps (GstTIOVXMiso *self,
    GstCaps *filter, GList *sink_caps_list, GList *sink_pads)
{
  GstCaps *ret = NULL;
  GstCaps *tmp = NULL;
  GList *node = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (sink_caps_list, FALSE);

  /* Loop through the list of sink pads caps and by default fully
   * intersect the list of source caps with the filter */
  ret = gst_caps_ref (filter);
  for (node = sink_caps_list; node; node = g_list_next (node)) {
    GstCaps *sink_caps = (GstCaps *) node->data;

    tmp = gst_caps_intersect_full (ret, sink_caps, GST_CAPS_INTERSECT_FIRST);
    GST_DEBUG_OBJECT (self,
        "src and filter caps intersected %" GST_PTR_FORMAT, ret);
    gst_caps_unref (ret);
    ret = tmp;
  }

  return ret;
}

static gboolean
gst_tiovx_miso_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (parent);
  GstTIOVXMisoClass *klass = GST_TIOVX_MISO_GET_CLASS (self);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  GstCaps *src_caps = NULL;
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter;
      GList *sink_caps_list = NULL;

      if (NULL == priv->sinkpads) {
        break;
      }

      gst_query_parse_caps (query, &filter);
      filter = intersect_with_template_caps (filter, pad);

      sink_caps_list = gst_tiovx_miso_get_sink_caps_list (self);
      if (NULL == sink_caps_list) {
        GST_ERROR_OBJECT (self, "Get sink caps list method failed");
        break;
      }

      /* Should return the caps the element supports on the sink pad */
      src_caps = klass->get_src_caps (self, filter, sink_caps_list,
          priv->sinkpads);
      if (NULL == src_caps) {
        GST_ERROR_OBJECT (self, "Get caps method failed");
        break;
      }

      ret = TRUE;

      /* The query response should be the supported caps in the src
       * from get_caps */
      gst_query_set_caps_result (query, src_caps);
      gst_caps_unref (src_caps);
      if (filter) {
        gst_caps_unref (filter);
      }
      g_list_free_full (sink_caps_list, (GDestroyNotify) gst_caps_unref);

      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

static gboolean
gst_tiovx_miso_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXMiso *self = GST_TIOVX_MISO (parent);
  GstTIOVXMisoClass *klass = GST_TIOVX_MISO_GET_CLASS (self);
  GstTIOVXMisoPrivate *priv = gst_tiovx_miso_get_instance_private (self);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstPad *src_pad = GST_PAD (priv->srcpad);
      GstCaps *filter = NULL;
      GstCaps *src_caps = NULL;
      GstCaps *filtered_src_caps = NULL;
      GstCaps *sink_caps = NULL;

      gst_query_parse_caps (query, &filter);
      filter = intersect_with_template_caps (filter, pad);

      src_caps = gst_pad_peer_query_caps (src_pad, filter);
      filtered_src_caps = intersect_with_template_caps (src_caps, src_pad);
      gst_caps_unref (sink_caps);

      /* Should return the caps the element supports on the sink pad */
      sink_caps = klass->get_sink_caps (self, filter, filtered_src_caps,
          (GstTIOVXPad *)pad);
      gst_caps_unref (filtered_src_caps);

      if (NULL == sink_caps) {
        GST_ERROR_OBJECT (self, "Get sink caps method failed");
        break;
      }

      ret = TRUE;

      /* The query response should be the supported caps in the sink
       * from get_caps */
      gst_query_set_caps_result (query, sink_caps);
      gst_caps_unref (sink_caps);
      break;
    }
    default:
    {
      ret = gst_tiovx_pad_query (pad, parent, query);
      break;
    }
  }

  return ret;
}

static gboolean
gst_tiovx_miso_trigger_downstream_pads (GstTIOVXPad * srcpad)
{
  GstCaps *peer_caps = NULL;
  gboolean ret = FALSE;
  GstPad *src_pad = NULL;

  g_return_val_if_fail (srcpad, FALSE);

  src_pad = GST_PAD (srcpad);

  /* Ask peer for what should the source caps (sink caps in the other end) be */
  peer_caps = gst_pad_get_current_caps (src_pad);

  if (NULL == peer_caps) {
    goto exit;
  }

  gst_tiovx_pad_peer_query_allocation (GST_TIOVX_PAD (src_pad), peer_caps);

  gst_caps_unref (peer_caps);

  ret = TRUE;

exit:
  return ret;
}

static gboolean
gst_tiovx_miso_set_caps (GstTIOVXMiso * self, GList * sink_caps_list,
    GstCaps * src_caps)
{
  GstTIOVXMisoClass *klass = NULL;
  GstTIOVXMisoPrivate *priv = NULL;
  gint in_num_channels = DEFAULT_NUM_CHANNELS;
  gint out_num_channels = DEFAULT_NUM_CHANNELS;
  GList *node = NULL, *pad_node = NULL;

  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (sink_caps_list, FALSE);
  g_return_val_if_fail (src_caps, FALSE);

  klass = GST_TIOVX_MISO_GET_CLASS (self);
  priv = gst_tiovx_miso_get_instance_private (self);

  if (NULL == klass->compare_caps) {
    GST_WARNING_OBJECT (self,
        "Subclass did not implement compare_caps method.");
  } /* Caps have not changed, skip module reinitialization */
  else {
    gboolean caps_unchanged = TRUE;
    GstCaps *current_sink_caps = NULL;

    for (node = sink_caps_list, pad_node = priv->sinkpads; node;
        node = g_list_next (node), pad_node = g_list_next (pad_node)) {
      GstCaps *sink_caps = (GstCaps *) node->data;
      current_sink_caps = gst_pad_get_current_caps (GST_PAD (pad_node->data));

      if (current_sink_caps) {
        caps_unchanged =
            klass->compare_caps (self, current_sink_caps, sink_caps,
            GST_PAD_SINK);
      }

      gst_caps_unref (current_sink_caps);

      if (!caps_unchanged) {
        break;
      }
    }

    if (caps_unchanged) {
      GST_INFO_OBJECT (self,
          "Caps haven't changed and graph has already been initialized, skipping initialization...");

      ret = TRUE;
      return ret;
    }
  }

  if (!gst_structure_get_int (gst_caps_get_structure (src_caps, 0),
          "num-channels", &out_num_channels)) {
    out_num_channels = 1;
  }
  for (node = sink_caps_list; node; node = g_list_next (node)) {
    GstCaps *sink_caps = (GstCaps *) node->data;

    if (!gst_structure_get_int (gst_caps_get_structure (sink_caps, 0),
            "num-channels", &in_num_channels)) {
      in_num_channels = 1;
    }

    g_return_val_if_fail (out_num_channels == in_num_channels, FALSE);
  }

  priv->num_channels = in_num_channels;

  ret = gst_tiovx_miso_modules_init (self, sink_caps_list, src_caps);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to init TIOVX module"), (NULL));
  }

  return ret;
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
