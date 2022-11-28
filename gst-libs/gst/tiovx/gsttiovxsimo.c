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

#include "gsttiovxsimo.h"

#include <stdio.h>

#include "gsttiovx.h"
#include "gsttiovxbufferutils.h"
#include "gsttiovxcontext.h"
#include "gsttiovxpad.h"
#include "gsttiovxqueueableobject.h"
#include "gsttiovxutils.h"

#define DEFAULT_NUM_CHANNELS (1)

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_simo_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_simo_debug_category

typedef struct _GstTIOVXSimoPrivate
{
  vx_context context;
  vx_graph graph;
  vx_node node;

  guint num_channels;
  GstTIOVXPad *sinkpad;
  GList *srcpads;
  GList *queueable_objects;

  GstTIOVXContext *tiovx_context;
} GstTIOVXSimoPrivate;

static GstElementClass *parent_class = NULL;
static gint private_offset = 0;
static void gst_tiovx_simo_class_init (GstTIOVXSimoClass * klass);
static void gst_tiovx_simo_init (GstTIOVXSimo * simo,
    GstTIOVXSimoClass * klass);

static void gst_tiovx_simo_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

GType
gst_tiovx_simo_get_type (void)
{
  static gsize tiovx_simo_type = 0;

  if (g_once_init_enter (&tiovx_simo_type)) {
    GType _type;
    static const GTypeInfo tiovx_simo_info = {
      sizeof (GstTIOVXSimoClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_tiovx_simo_class_init,
      NULL,
      NULL,
      sizeof (GstTIOVXSimo),
      0,
      (GInstanceInitFunc) gst_tiovx_simo_init,
    };
    _type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstTIOVXSimo", &tiovx_simo_info, G_TYPE_FLAG_ABSTRACT);
    private_offset =
        g_type_add_instance_private (_type, sizeof (GstTIOVXSimoPrivate));
    {
      const GInterfaceInfo g_implement_interface_info = {
        (GInterfaceInitFunc) gst_tiovx_simo_child_proxy_init
      };
      g_type_add_interface_static (_type, GST_TYPE_CHILD_PROXY,
          &g_implement_interface_info);
    }
    g_once_init_leave (&tiovx_simo_type, _type);
  }

  return tiovx_simo_type;
}

static gpointer
gst_tiovx_simo_get_instance_private (GstTIOVXSimo * self)
{
  return (G_STRUCT_MEMBER_P (self, private_offset));
}

static gboolean gst_tiovx_simo_modules_init (GstTIOVXSimo * self,
    GstCaps * sink_caps, GList * src_caps_list);
static gboolean gst_tiovx_simo_modules_deinit (GstTIOVXSimo * self);
static gboolean gst_tiovx_simo_start (GstTIOVXSimo * self);
static gboolean gst_tiovx_simo_stop (GstTIOVXSimo * self);

static void gst_tiovx_simo_finalize (GObject * object);

static GstStateChangeReturn
gst_tiovx_simo_change_state (GstElement * element, GstStateChange transition);
static gboolean gst_tiovx_simo_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_tiovx_simo_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstFlowReturn gst_tiovx_simo_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static GstPad *gst_tiovx_simo_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * name_templ, const GstCaps * caps);
static void gst_tiovx_simo_release_pad (GstElement * element, GstPad * pad);

static gboolean gst_tiovx_simo_set_caps (GstTIOVXSimo * self,
    GstPad * pad, GstCaps * sink_caps, GList * src_caps_list);
static GstCaps *gst_tiovx_simo_default_get_sink_caps (GstTIOVXSimo * self,
    GstCaps * filter, GList * src_caps_list, GList * src_pads);
static GstCaps *gst_tiovx_simo_default_get_src_caps (GstTIOVXSimo * self,
    GstCaps * filter, GstCaps * sink_caps, GstTIOVXPad *src_pad);
static GList *gst_tiovx_simo_default_fixate_caps (GstTIOVXSimo * self,
    GstCaps * sink_caps, GList * src_caps_list);
static gboolean gst_tiovx_simo_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_tiovx_simo_trigger_downstream_pads (GList * srcpads);
static gboolean
gst_tiovx_simo_pads_to_vx_references (GstTIOVXSimo * simo, GList * pads,
    GstBuffer ** buffer_list);
static GstFlowReturn
gst_tiovx_simo_push_buffers (GstTIOVXSimo * simo, GList * pads,
    GstBuffer ** buffer_list);
static GstFlowReturn gst_tiovx_simo_process_graph (GstTIOVXSimo * self);
static void
gst_tiovx_simo_free_buffer_list (GstBuffer ** buffer_list, gint list_length);

guint
gst_tiovx_simo_get_num_pads (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = NULL;
  guint result = 0;

  g_return_val_if_fail (GST_IS_TIOVX_SIMO (self), FALSE);

  priv = gst_tiovx_simo_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  result = g_list_length (priv->srcpads);
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_tiovx_simo_class_init (GstTIOVXSimoClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  if (private_offset != 0)
    g_type_class_adjust_private_offset (klass, &private_offset);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_simo_finalize);

  klass->get_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_default_get_sink_caps);
  klass->get_src_caps = GST_DEBUG_FUNCPTR (gst_tiovx_simo_default_get_src_caps);
  klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_tiovx_simo_default_fixate_caps);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_change_state);
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_release_pad);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_simo_debug_category, "tiovxsimo", 0,
      "tiovxsimo element");

  parent_class = g_type_class_peek_parent (klass);
}

static void
gst_tiovx_simo_init (GstTIOVXSimo * self, GstTIOVXSimoClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);
  GstPadTemplate *pad_template = NULL;
  vx_status status = VX_FAILURE;

  GST_DEBUG_OBJECT (self, "gst_tiovx_simo_init");

  pad_template = gst_element_class_get_pad_template (gstelement_class, "sink");
  g_return_if_fail (pad_template != NULL);

  priv->sinkpad =
      GST_TIOVX_PAD (gst_pad_new_from_template (pad_template, "sink"));
  if (!GST_TIOVX_IS_PAD (priv->sinkpad)) {
    GST_ERROR_OBJECT (self, "Requested pad from template isn't a TIOVX pad");
    return;
  }

  gst_pad_set_event_function (GST_PAD (priv->sinkpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_sink_event));
  gst_pad_set_query_function (GST_PAD (priv->sinkpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_sink_query));
  gst_pad_set_chain_function (GST_PAD (priv->sinkpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_chain));
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD (priv->sinkpad));
  gst_pad_set_active (GST_PAD (priv->sinkpad), FALSE);

  /* Initialize all private member data */
  priv->context = NULL;
  priv->graph = NULL;
  priv->node = NULL;

  priv->num_channels = DEFAULT_NUM_CHANNELS;

  priv->srcpads = NULL;

  priv->tiovx_context = NULL;
  priv->queueable_objects = NULL;

  priv->tiovx_context = gst_tiovx_context_new ();
  if (NULL == priv->tiovx_context) {
    GST_ERROR_OBJECT (self, "Failed to do common initialization");
  }

  priv->context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) priv->context);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Context creation failed, vx_status %" G_GINT32_FORMAT, status);
    return;
  }

  tivxHwaLoadKernels (priv->context);
  tivxImgProcLoadKernels (priv->context);
  tivxEdgeaiImgProcLoadKernels (priv->context);

  return;
}

static gboolean
gst_tiovx_simo_start (GstTIOVXSimo * self)
{
  GST_DEBUG_OBJECT (self, "Starting SIMO");

  return TRUE;
}

static gboolean
gst_tiovx_simo_modules_init (GstTIOVXSimo * self, GstCaps * sink_caps,
    GList * src_caps_list)
{
  GstTIOVXSimoClass *klass = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
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
  g_return_val_if_fail (sink_caps, ret);

  priv = gst_tiovx_simo_get_instance_private (self);
  klass = GST_TIOVX_SIMO_GET_CLASS (self);

  num_pads = gst_tiovx_simo_get_num_pads (self);

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
      klass->init_module (self, priv->context, priv->sinkpad, priv->srcpads,
      sink_caps, src_caps_list, priv->num_channels);
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
      klass->get_node_info (self, &priv->node, priv->sinkpad, priv->srcpads,
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
  /* Parameters equals, number of output pads, a single input pad and all queueable objects */
  num_parameters = num_pads + 1 + g_list_length (priv->queueable_objects);
  params_list = g_malloc0 (num_parameters * sizeof (*params_list));
  if (NULL == params_list) {
    GST_ERROR_OBJECT (self, "Could not allocate memory for parameters list");
    goto free_graph;
  }

  gst_tiovx_pad_get_params (priv->sinkpad, &array, &exemplar, &graph_param_id,
      &node_param_id);
  status =
      add_graph_parameter_by_node_index (gst_tiovx_simo_debug_category,
      G_OBJECT (self), priv->graph, priv->node, graph_param_id, node_param_id,
      params_list, exemplar);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Setting input parameter failed, vx_status %" G_GINT32_FORMAT, status);
    goto free_parameters_list;
  }

  for (l = priv->srcpads; l; l = g_list_next (l)) {
    pad = GST_TIOVX_PAD (l->data);
    gst_tiovx_pad_get_params (pad, &array, &exemplar, &graph_param_id,
        &node_param_id);
    status =
        add_graph_parameter_by_node_index (gst_tiovx_simo_debug_category,
        G_OBJECT (self), priv->graph, priv->node, graph_param_id, node_param_id,
        params_list, exemplar);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Setting output parameter failed, vx_status %" G_GINT32_FORMAT,
          status);
      goto free_parameters_list;
    }
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);

    gst_tiovx_queueable_get_params (queueable_object, &array, &exemplar,
        &graph_param_id, &node_param_id);
    status =
        add_graph_parameter_by_node_index (gst_tiovx_simo_debug_category,
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

static gboolean
gst_tiovx_simo_stop (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);
  gboolean ret = FALSE;
  GstTIOVXPad *pad = NULL;
  GList *l = NULL;
  vx_object_array array = NULL;
  vx_reference *exemplar = NULL;
  gint graph_param_id = 0, node_param_id = 0;
  gint i = 0;

  GST_DEBUG_OBJECT (self, "gst_tiovx_simo_modules_deinit");

  if ((NULL == priv->graph)
      || (VX_SUCCESS != vxGetStatus ((vx_reference) priv->graph))) {
    GST_WARNING_OBJECT (self,
        "Trying to deinit modules but initialization was not completed, skipping...");
    ret = TRUE;
    goto free_common;
  }

  gst_tiovx_pad_get_params (priv->sinkpad, &array, &exemplar, &graph_param_id,
      &node_param_id);
  for (i = 0; i < priv->num_channels; i++) {
    vx_reference ref = NULL;

    ref = vxGetObjectArrayItem (array, i);
    if (VX_SUCCESS != gst_tiovx_empty_exemplar (ref)) {
      GST_WARNING_OBJECT (self, "Failed to empty input exemplar");
    }
    vxReleaseReference (&ref);
  }

  for (l = priv->srcpads; l; l = g_list_next (l)) {
    pad = GST_TIOVX_PAD (l->data);
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

  ret = gst_tiovx_simo_modules_deinit (self);
  if (!ret) {
    GST_WARNING_OBJECT (self, "Failed to deinit module");
    goto free_common;
  }

  ret = TRUE;

free_common:
  priv->node = NULL;
  priv->graph = NULL;

  return ret;
}

static void
gst_tiovx_simo_finalize (GObject * gobject)
{
  GstTIOVXSimo *self = GST_TIOVX_SIMO (gobject);
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);

  GST_LOG_OBJECT (self, "finalize");

  if (priv->context) {
    tivxHwaUnLoadKernels (priv->context);
    tivxImgProcUnLoadKernels (priv->context);
    tivxEdgeaiImgProcUnLoadKernels (priv->context);
    vxReleaseContext (&priv->context);
    priv->context = NULL;
  }

  if (priv->tiovx_context) {
    g_object_unref (priv->tiovx_context);
  }

  if (priv->srcpads) {
    g_list_free_full (priv->srcpads, (GDestroyNotify) gst_object_unref);
  }

  priv->srcpads = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static GstStateChangeReturn
gst_tiovx_simo_change_state (GstElement * element, GstStateChange transition)
{
  GstTIOVXSimo *self = NULL;
  gboolean ret = FALSE;
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  self = GST_TIOVX_SIMO (element);

  GST_DEBUG_OBJECT (self, "gst_tiovx_simo_change_state");

  switch (transition) {
      /* "Start" transition */
    case GST_STATE_CHANGE_NULL_TO_READY:
      ret = gst_tiovx_simo_start (self);
      if (!ret) {
        GST_DEBUG_OBJECT (self,
            "Failed to init module in NULL to READY transition");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
      /* "Stop" transition */
    case GST_STATE_CHANGE_READY_TO_NULL:
      ret = gst_tiovx_simo_stop (self);
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

static GstPad *
gst_tiovx_simo_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name_templ, const GstCaps * caps)
{
  GstTIOVXSimo *self = GST_TIOVX_SIMO (element);
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);
  guint name_index = 0;
  GstPad *src_pad = NULL;
  gchar *name = NULL;

  GST_DEBUG_OBJECT (self, "requesting pad");

  g_return_val_if_fail (templ, NULL);

  GST_OBJECT_LOCK (self);

  if (NULL == name_templ) {
    name_templ = "src_%u";
  }

  /* The user may request the pad in two ways:
   * - src_%u: We need to assign a free index
   * - src_N: We need to create a pad with the given name
   */

  /* src_%u: No index provided, need to compute one */
  if (0 == sscanf (name_templ, "src_%u", &name_index)) {
    GList *iter = priv->srcpads;

    name_index = 0;
    for (; iter; iter = g_list_next (iter)) {
      GstPad *pad = GST_PAD (iter->data);
      const gchar *pad_name = GST_OBJECT_NAME (pad);
      guint used_index = 0;

      sscanf (pad_name, "src_%u", &used_index);
      if (used_index >= name_index) {
        name_index = used_index + 1;
      }
    }

    GST_INFO_OBJECT (self, "Requested new pad, assigned an index of %u",
        name_index);
    name = g_strdup_printf ("src_%u", name_index);
  } else {
    GList *iter = priv->srcpads;

    for (; iter; iter = g_list_next (iter)) {
      GstPad *pad = GST_PAD (iter->data);
      const gchar *pad_name = GST_OBJECT_NAME (pad);
      guint used_index = 0;

      sscanf (pad_name, "src_%u", &used_index);
      if (used_index == name_index) {
        GST_ERROR_OBJECT (self, "A pad with index %u is already in use",
            name_index);
        goto unlock;
      }
    }

    GST_INFO_OBJECT (self, "Requested pad index %u is free", name_index);
    name = g_strdup (name_templ);
  }

  src_pad = gst_pad_new_from_template (templ, name);
  if (NULL == src_pad) {
    GST_ERROR_OBJECT (self, "Failed to create source pad");
    goto free_name;
  }

  GST_OBJECT_UNLOCK (self);

  gst_element_add_pad (GST_ELEMENT_CAST (self), src_pad);
  gst_pad_set_active (src_pad, TRUE);

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (src_pad),
      GST_OBJECT_NAME (src_pad));

  GST_OBJECT_LOCK (self);
  priv->srcpads = g_list_append (priv->srcpads, gst_object_ref (src_pad));

  gst_pad_set_query_function (GST_PAD (src_pad),
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_src_query));

free_name:
  g_free (name);

unlock:
  GST_OBJECT_UNLOCK (self);
  return src_pad;
}

static void
gst_tiovx_simo_release_pad (GstElement * element, GstPad * pad)
{
  GstTIOVXSimo *self = GST_TIOVX_SIMO (element);
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);
  GList *node = NULL;

  GST_OBJECT_LOCK (self);

  node = g_list_find (priv->srcpads, pad);
  g_return_if_fail (node);

  priv->srcpads = g_list_remove (priv->srcpads, pad);
  gst_object_unref (pad);

  GST_OBJECT_UNLOCK (self);

  gst_child_proxy_child_removed (GST_CHILD_PROXY (self), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT_CAST (self), pad);
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

static GList *
gst_tiovx_simo_get_src_caps_list (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = NULL;

  GList *src_caps_list = NULL;
  GList *node = NULL;

  g_return_val_if_fail (self, NULL);

  priv = gst_tiovx_simo_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "Generating src caps list");

  for (node = priv->srcpads; node; node = g_list_next (node)) {
    GstPad *src_pad = GST_PAD (node->data);
    GstCaps *peer_caps = gst_pad_peer_query_caps (src_pad, NULL);
    GstCaps *pad_caps = NULL;

    pad_caps = intersect_with_template_caps (peer_caps, src_pad);

    gst_caps_unref (peer_caps);

    GST_DEBUG_OBJECT (self, "Caps from %s:%s peer: %" GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (src_pad), pad_caps);
    /* Insert at the end of the src caps list */
    src_caps_list = g_list_insert (src_caps_list, pad_caps, -1);
  }

  return src_caps_list;
}

static GstCaps *
gst_tiovx_simo_default_get_sink_caps (GstTIOVXSimo * self,
    GstCaps * filter, GList * src_caps_list, GList * src_pads)
{
  GstCaps *ret = NULL;
  GstCaps *tmp = NULL;
  GList *node = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  /* Loop through the list of src pads caps and by default fully
   * intersect the list of source caps with the filter */
  ret = gst_caps_ref (filter);
  for (node = src_caps_list; node; node = g_list_next (node)) {
    GstCaps *src_caps = (GstCaps *) node->data;

    tmp = gst_caps_intersect_full (ret, src_caps, GST_CAPS_INTERSECT_FIRST);
    GST_DEBUG_OBJECT (self,
        "src and filter caps intersected %" GST_PTR_FORMAT, ret);
    gst_caps_unref (ret);
    ret = tmp;
  }

  return ret;
}

static GstCaps *
gst_tiovx_simo_default_get_src_caps (GstTIOVXSimo * self,
    GstCaps * filter, GstCaps * sink_caps, GstTIOVXPad *src_pad)
{
  GstCaps *ret = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (sink_caps, FALSE);

  ret = gst_caps_copy (sink_caps);

  if (filter) {
    GstCaps *tmp = ret;
    ret = gst_caps_intersect (ret, filter);
    gst_caps_unref (tmp);
  }

  return ret;
}

static gboolean
gst_tiovx_simo_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXSimo *self = GST_TIOVX_SIMO (parent);
  GstTIOVXSimoClass *klass = GST_TIOVX_SIMO_GET_CLASS (self);
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);
  GstCaps *sink_caps = NULL;
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter;
      GList *src_caps_list = NULL;

      if (NULL == priv->srcpads) {
        break;
      }

      gst_query_parse_caps (query, &filter);
      filter = intersect_with_template_caps (filter, pad);

      src_caps_list = gst_tiovx_simo_get_src_caps_list (self);
      if (NULL == src_caps_list) {
        GST_ERROR_OBJECT (self, "Get src caps list method failed");
        break;
      }

      /* Should return the caps the element supports on the sink pad */
      sink_caps = klass->get_sink_caps (self, filter, src_caps_list,
          priv->srcpads);
      if (NULL == sink_caps) {
        GST_ERROR_OBJECT (self, "Get caps method failed");
        break;
      }

      ret = TRUE;

      /* The query response should be the supported caps in the sink
       * from get_caps */
      gst_query_set_caps_result (query, sink_caps);
      gst_caps_unref (sink_caps);
      if (filter) {
        gst_caps_unref (filter);
      }
      g_list_free_full (src_caps_list, (GDestroyNotify) gst_caps_unref);

      break;
    }
    default:
      ret = gst_tiovx_pad_query (GST_PAD (priv->sinkpad), parent, query);
      break;
  }

  return ret;
}

static gboolean
gst_tiovx_simo_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXSimo *self = GST_TIOVX_SIMO (parent);
  GstTIOVXSimoClass *klass = GST_TIOVX_SIMO_GET_CLASS (self);
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstPad *sink_pad = GST_PAD (priv->sinkpad);
      GstCaps *filter = NULL;
      GstCaps *sink_caps = NULL;
      GstCaps *filtered_sink_caps = NULL;
      GstCaps *src_caps = NULL;

      gst_query_parse_caps (query, &filter);
      filter = intersect_with_template_caps (filter, pad);

      sink_caps = gst_pad_peer_query_caps (sink_pad, filter);
      filtered_sink_caps = intersect_with_template_caps (sink_caps, sink_pad);
      gst_caps_unref (sink_caps);

      /* Should return the caps the element supports on the src pad */
      src_caps = klass->get_src_caps (self, filter, filtered_sink_caps,
          (GstTIOVXPad *)pad);
      gst_caps_unref (filtered_sink_caps);

      if (NULL == src_caps) {
        GST_ERROR_OBJECT (self, "Get src caps method failed");
        break;
      }

      ret = TRUE;

      /* The query response should be the supported caps in the sink
       * from get_caps */
      gst_query_set_caps_result (query, src_caps);
      gst_caps_unref (src_caps);
      break;
    }
    default:
    {
      ret = gst_pad_query_default (pad, parent, query);
      break;
    }
  }

  return ret;
}

static gboolean
gst_tiovx_simo_trigger_downstream_pads (GList * srcpads)
{
  GstCaps *peer_caps = NULL;
  GList *src_pads_sublist = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (srcpads, FALSE);

  src_pads_sublist = srcpads;
  while (NULL != src_pads_sublist) {
    GstPad *src_pad = NULL;
    GList *next = g_list_next (src_pads_sublist);

    src_pad = GST_PAD (src_pads_sublist->data);
    if (NULL == src_pad) {
      goto exit;
    }

    /* Ask peer for what should the source caps (sink caps in the other end) be */
    peer_caps = gst_pad_get_current_caps (src_pad);

    if (NULL == peer_caps) {
      goto exit;
    }

    gst_tiovx_pad_peer_query_allocation (GST_TIOVX_PAD (src_pad), peer_caps);

    gst_caps_unref (peer_caps);

    src_pads_sublist = next;
  }

  ret = TRUE;

exit:
  return ret;
}

static gboolean
gst_tiovx_simo_set_caps (GstTIOVXSimo * self, GstPad * pad, GstCaps * sink_caps,
    GList * src_caps_list)
{
  GstTIOVXSimoClass *klass = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  gint in_num_channels = DEFAULT_NUM_CHANNELS;
  gint out_num_channels = DEFAULT_NUM_CHANNELS;
  GList *node = NULL;

  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (pad, FALSE);
  g_return_val_if_fail (sink_caps, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  klass = GST_TIOVX_SIMO_GET_CLASS (self);
  priv = gst_tiovx_simo_get_instance_private (self);

  if (NULL == klass->compare_caps) {
    GST_WARNING_OBJECT (self,
        "Subclass did not implement compare_caps method.");
  } /* Caps have not changed, skip module reinitialization */
  else {
    gboolean caps_unchanged = TRUE;
    GstCaps *current_sink_caps = NULL;

    current_sink_caps = gst_pad_get_current_caps (GST_PAD (priv->sinkpad));

    if (current_sink_caps) {
      caps_unchanged =
          klass->compare_caps (self, current_sink_caps, sink_caps,
          GST_PAD_SINK);

      if (caps_unchanged) {
        GST_INFO_OBJECT (self,
            "Caps haven't changed and graph has already been initialized, skipping initialization...");
        gst_caps_unref (current_sink_caps);

        ret = TRUE;
        return ret;
      }
      gst_caps_unref (current_sink_caps);
    }
  }

  GST_DEBUG_OBJECT (pad, "have new caps %p %" GST_PTR_FORMAT, sink_caps,
      sink_caps);

  if (!gst_structure_get_int (gst_caps_get_structure (sink_caps, 0),
          "num-channels", &in_num_channels)) {
    in_num_channels = 1;
  }
  for (node = src_caps_list; node; node = g_list_next (node)) {
    GstCaps *src_caps = (GstCaps *) node->data;

    if (!gst_structure_get_int (gst_caps_get_structure (src_caps, 0),
            "num-channels", &out_num_channels)) {
      out_num_channels = 1;
    }

    g_return_val_if_fail (in_num_channels == out_num_channels, FALSE);
  }

  priv->num_channels = in_num_channels;

  ret = gst_tiovx_simo_modules_init (self, sink_caps, src_caps_list);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to init TIOVX module"), (NULL));
  }

  return ret;
}

static GList *
gst_tiovx_simo_default_fixate_caps (GstTIOVXSimo * self, GstCaps * sink_caps,
    GList * src_caps_list)
{
  GList *node = NULL;
  GList *ret = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (sink_caps, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  for (node = src_caps_list; node; node = g_list_next (node)) {
    GstCaps *src_caps = (GstCaps *) node->data;
    GstCaps *src_caps_fixated = gst_caps_fixate (gst_caps_ref (src_caps));

    ret = g_list_append (ret, src_caps_fixated);
  }

  return ret;
}

static gboolean
gst_tiovx_simo_pads_to_vx_references (GstTIOVXSimo * self, GList * pads,
    GstBuffer ** buffer_list)
{
  GList *pads_sublist = NULL;
  GstFlowReturn flow_return = GST_FLOW_ERROR;
  gint i = 0;
  gboolean ret = FALSE;
  guint num_pads = 0;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (pads, FALSE);
  g_return_val_if_fail (buffer_list, FALSE);

  num_pads = gst_tiovx_simo_get_num_pads (self);
  pads_sublist = pads;

  while ((NULL != pads_sublist) && (i < num_pads)) {
    GstPad *pad = NULL;
    GList *next = g_list_next (pads_sublist);

    pad = GST_PAD (pads_sublist->data);
    g_return_val_if_fail (pad, FALSE);

    /* By calling this the pad's exemplars will have valid data */
    flow_return =
        gst_tiovx_pad_acquire_buffer (GST_TIOVX_PAD (pad), &(buffer_list[i]),
        NULL);
    if (GST_FLOW_OK != flow_return) {
      GST_ERROR_OBJECT (self, "Unable to acquire buffer from pad: %p", pad);
      goto exit;
    }

    pads_sublist = next;

    i++;
  }

  ret = TRUE;

exit:
  return ret;
}

static GstFlowReturn
gst_tiovx_simo_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstTIOVXSimoClass *klass = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  gboolean subclass_ret = FALSE;
  GstTIOVXSimo *self = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  vx_object_array in_array = NULL;
  GstBuffer **buffer_list = NULL;
  GstClockTime pts, dts, duration;
  guint64 offset, offset_end;
  vx_status status = VX_FAILURE;
  gint num_pads = 0;
  vx_size num_channels = 0;
  gint i = 0;
  vx_reference *exemplar = NULL;
  vx_object_array out_array = NULL;
  gint graph_param_id = -1, node_param_id = -1;

  self = GST_TIOVX_SIMO (parent);
  priv = gst_tiovx_simo_get_instance_private (self);
  klass = GST_TIOVX_SIMO_GET_CLASS (self);

  pts = GST_BUFFER_PTS (buffer);
  dts = GST_BUFFER_DTS (buffer);
  duration = GST_BUFFER_DURATION (buffer);
  offset = GST_BUFFER_OFFSET (buffer);
  offset_end = GST_BUFFER_OFFSET_END (buffer);

  /* Chain sink pads' TIOVXPad call, this ensures valid vx_reference in the buffers  */
  ret = gst_tiovx_pad_chain (pad, parent, &buffer);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (pad, "Pad's chain function failed");
    goto exit;
  }

  gst_tiovx_pad_get_params (priv->sinkpad, &out_array, &exemplar,
      &graph_param_id, &node_param_id);
  in_array =
      gst_tiovx_get_vx_array_from_buffer (GST_CAT_DEFAULT, *exemplar, buffer);

  status =
      vxQueryObjectArray (in_array, VX_OBJECT_ARRAY_NUMITEMS, &num_channels,
      sizeof (num_channels));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Get number of channels in input buffer failed %" G_GINT32_FORMAT,
        status);
    goto exit;
  }

  for (i = 0; i < num_channels; i++) {
    vx_reference gst_ref = NULL;
    vx_reference modules_ref = NULL;

    gst_ref = vxGetObjectArrayItem (in_array, i);
    modules_ref = vxGetObjectArrayItem (out_array, i);

    status = gst_tiovx_transfer_handle (GST_CAT_DEFAULT, gst_ref, modules_ref);
    vxReleaseReference (&gst_ref);
    vxReleaseReference (&modules_ref);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Error in input handle transfer %" G_GINT32_FORMAT, status);
      goto exit;
    }
  }

  num_pads = gst_tiovx_simo_get_num_pads (self);
  buffer_list = g_malloc0 (sizeof (GstBuffer *) * num_pads);

  if (!gst_tiovx_simo_pads_to_vx_references (self, priv->srcpads, buffer_list)) {
    GST_ERROR_OBJECT (self, "Unable to get references from pads");
    ret = GST_FLOW_ERROR;
    goto free_buffers;
  }

  if (NULL != klass->preprocess) {
    subclass_ret = klass->preprocess (self);
    if (!subclass_ret) {
      GST_ERROR_OBJECT (self, "Subclass preprocess failed");
      goto free_buffers;
    }
  }

  /* Graph processing */
  ret = gst_tiovx_simo_process_graph (self);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Graph processing failed %d", status);
    goto free_buffers;
  }

  if (NULL != klass->postprocess) {
    subclass_ret = klass->postprocess (self);
    if (!subclass_ret) {
      GST_ERROR_OBJECT (self, "Subclass postprocess failed");
      goto free_buffers;
    }
  }

  for (i = 0; i < num_pads; i++) {
    GST_BUFFER_PTS (buffer_list[i]) = pts;
    GST_BUFFER_DTS (buffer_list[i]) = dts;
    GST_BUFFER_DURATION (buffer_list[i]) = duration;
    GST_BUFFER_OFFSET (buffer_list[i]) = offset;
    GST_BUFFER_OFFSET_END (buffer_list[i]) = offset_end;
  }

  ret = gst_tiovx_simo_push_buffers (self, priv->srcpads, buffer_list);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to push all buffers to source pads: %d",
        ret);
  }

  goto free_buffer_list;

free_buffers:
  gst_tiovx_simo_free_buffer_list (buffer_list, num_pads);

free_buffer_list:
  g_free (buffer_list);
exit:
  if (NULL != buffer) {
    gst_buffer_unref (buffer);
    buffer = NULL;
  }

  return ret;
}


static gboolean
gst_tiovx_simo_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTIOVXSimo *self = GST_TIOVX_SIMO (parent);
  GstTIOVXSimoClass *klass = GST_TIOVX_SIMO_GET_CLASS (self);
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);
  GList *caps_node = NULL;
  GList *pads_node = NULL;
  gboolean ret = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *sink_caps = NULL;
      GList *src_caps_list = NULL;
      GList *fixated_list = NULL;

      gst_event_parse_caps (event, &sink_caps);

      src_caps_list = gst_tiovx_simo_get_src_caps_list (self);

      /* Should return the fixated caps the element will use on the src pads */
      fixated_list = klass->fixate_caps (self, sink_caps, src_caps_list);
      if (NULL == fixated_list) {
        GST_ERROR_OBJECT (self, "Subclass did not fixate caps");
        gst_event_unref (event);
        break;
      }

      /* Discard previous list of source caps */
      g_list_free_full (src_caps_list, (GDestroyNotify) gst_caps_unref);

      ret =
          gst_tiovx_simo_set_caps (self, GST_PAD (priv->sinkpad), sink_caps,
          fixated_list);
      if (!ret) {
        GST_ERROR_OBJECT (self, "Set caps method failed");
        gst_event_unref (event);
        break;
      }

      for (caps_node = fixated_list, pads_node = priv->srcpads;
          caps_node && pads_node;
          caps_node = g_list_next (caps_node), pads_node =
          g_list_next (pads_node)) {
        GstCaps *src_caps = (GstCaps *) caps_node->data;
        GstPad *src_pad = GST_PAD (pads_node->data);

        /* Notify peer pads downstream about fixated caps in this pad */
        gst_pad_push_event (src_pad, gst_event_new_caps (src_caps));
      }

      g_list_free_full (fixated_list, (GDestroyNotify) gst_caps_unref);

      gst_event_unref (event);

      ret = gst_tiovx_simo_trigger_downstream_pads (priv->srcpads);
      break;
    }
    default:
      ret = gst_pad_event_default (GST_PAD (pad), parent, event);
      break;
  }

  return ret;
}

static void
gst_tiovx_simo_free_buffer_list (GstBuffer ** buffer_list, gint list_length)
{
  gint i = 0;

  g_return_if_fail (buffer_list);
  g_return_if_fail (list_length >= 0);

  for (i = 0; i < list_length; i++) {
    if (NULL != buffer_list[i]) {
      gst_buffer_unref (buffer_list[i]);
      buffer_list[i] = NULL;
    }
  }
}

static GstFlowReturn
gst_tiovx_simo_push_buffers (GstTIOVXSimo * simo, GList * pads,
    GstBuffer ** buffer_list)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GList *pads_sublist = NULL;
  gint i = 0;

  g_return_val_if_fail (simo, GST_FLOW_ERROR);
  g_return_val_if_fail (pads, GST_FLOW_ERROR);
  g_return_val_if_fail (buffer_list, GST_FLOW_ERROR);

  pads_sublist = pads;
  while (NULL != pads_sublist) {
    GstFlowReturn push_return = GST_FLOW_ERROR;
    GstPad *pad = NULL;
    GList *next = g_list_next (pads_sublist);

    pad = GST_PAD (pads_sublist->data);
    g_return_val_if_fail (pad, FALSE);

    push_return = gst_pad_push (pad, buffer_list[i]);
    if (GST_FLOW_OK != push_return) {
      /* If one pad fails, don't exit immediately, attempt to push to all pads
       * but return a warning
       */
      GST_WARNING_OBJECT (simo, "Error pushing to pad: %" GST_PTR_FORMAT, pad);
    }
    buffer_list[i] = NULL;

    pads_sublist = next;
    i++;
  }

  return ret;
}

static GstFlowReturn
gst_tiovx_simo_process_graph (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  vx_status status = VX_FAILURE;
  uint32_t in_refs = 0;
  uint32_t out_refs = 0;
  gint graph_param_id = -1;
  gint node_param_id = -1;
  vx_object_array array = NULL;
  vx_reference *exemplar = NULL;
  vx_reference dequeued_ref = NULL;
  GstTIOVXPad *pad = NULL;
  GList *l = NULL;

  g_return_val_if_fail (self, ret);

  priv = gst_tiovx_simo_get_instance_private (self);

  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) priv->graph), VX_FAILURE);


  /* Enqueueing parameters */
  GST_LOG_OBJECT (self, "Enqueueing parameters");

  gst_tiovx_pad_get_params (priv->sinkpad, &array, &exemplar, &graph_param_id,
      &node_param_id);
  GST_LOG_OBJECT (self,
      "Enqueueing input array of refs: %p\t with graph id: %d", exemplar,
      graph_param_id);
  status =
      vxGraphParameterEnqueueReadyRef (priv->graph, graph_param_id, exemplar,
      1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input enqueue failed %" G_GINT32_FORMAT, status);
    goto exit;
  }

  for (l = priv->srcpads; l; l = g_list_next (l)) {
    pad = GST_TIOVX_PAD (l->data);
    gst_tiovx_pad_get_params (pad, &array, &exemplar, &graph_param_id,
        &node_param_id);
    GST_LOG_OBJECT (self,
        "Enqueueing output array of refs: %p\t with graph id: %d", exemplar,
        graph_param_id);
    status =
        vxGraphParameterEnqueueReadyRef (priv->graph, graph_param_id, exemplar,
        1);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Output enqueue failed %" G_GINT32_FORMAT,
          status);
      goto exit;
    }
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);
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
  gst_tiovx_pad_get_params (priv->sinkpad, &array, &exemplar, &graph_param_id,
      &node_param_id);
  GST_LOG_OBJECT (self,
      "Dequeueing input array of refs: %p\t with graph id: %d", exemplar,
      graph_param_id);
  status =
      vxGraphParameterDequeueDoneRef (priv->graph, graph_param_id,
      (vx_reference *) & dequeued_ref, 1, &in_refs);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input dequeue failed %" G_GINT32_FORMAT, status);
    goto exit;
  }

  for (l = priv->srcpads; l; l = g_list_next (l)) {
    pad = GST_TIOVX_PAD (l->data);
    gst_tiovx_pad_get_params (pad, &array, &exemplar, &graph_param_id,
        &node_param_id);
    GST_LOG_OBJECT (self,
        "Dequeueing output array of refs: %p\t with graph id: %d", exemplar,
        graph_param_id);
    status =
        vxGraphParameterDequeueDoneRef (priv->graph, graph_param_id,
        (vx_reference *) & dequeued_ref, 1, &out_refs);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Output dequeue failed %" G_GINT32_FORMAT,
          status);
      goto exit;
    }
  }

  for (l = priv->queueable_objects; l; l = g_list_next (l)) {
    GstTIOVXQueueable *queueable_object = GST_TIOVX_QUEUEABLE (l->data);
    gst_tiovx_queueable_get_params (queueable_object, &array, &exemplar,
        &graph_param_id, &node_param_id);
    GST_LOG_OBJECT (self,
        "Dequeueing queueable array of refs: %p\t with graph id: %d", exemplar,
        graph_param_id);
    status =
        vxGraphParameterDequeueDoneRef (priv->graph, graph_param_id,
        (vx_reference *) & dequeued_ref, 1, &out_refs);
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

static gboolean
gst_tiovx_simo_modules_deinit (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = gst_tiovx_simo_get_instance_private (self);
  GstTIOVXSimoClass *klass = GST_TIOVX_SIMO_GET_CLASS (self);
  int ret = FALSE;

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

  ret = TRUE;

release_graph:
  vxReleaseGraph (&priv->graph);

  return ret;
}

/* GstChildProxy implementation */
static GObject *
gst_tiovx_simo_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  return NULL;
}

static GObject *
gst_tiovx_simo_child_proxy_get_child_by_name (GstChildProxy *
    child_proxy, const gchar * name)
{
  GstTIOVXSimo *self = NULL;
  GObject *obj = NULL;

  self = GST_TIOVX_SIMO (child_proxy);

  GST_OBJECT_LOCK (self);

  if (0 == strcmp (name, "sink")) {
    /* Only one sink pad for SIMO class */
    obj = g_list_nth_data (GST_ELEMENT_CAST (self)->sinkpads, 0);
    if (obj) {
      gst_object_ref (obj);
    }
  } else {                      /* src pad case */
    GList *node = NULL;
    node = GST_ELEMENT_CAST (self)->srcpads;
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
gst_tiovx_simo_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  GstTIOVXSimo *self = NULL;
  guint count = 0;

  self = GST_TIOVX_SIMO (child_proxy);

  GST_OBJECT_LOCK (self);
  /* Number of source pads + number of sink pads (always 1) */
  count = GST_ELEMENT_CAST (self)->numsrcpads + 1;
  GST_OBJECT_UNLOCK (self);
  GST_INFO_OBJECT (self, "Children Count: %d", count);

  return count;
}

static void
gst_tiovx_simo_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  iface->get_child_by_index = gst_tiovx_simo_child_proxy_get_child_by_index;
  iface->get_child_by_name = gst_tiovx_simo_child_proxy_get_child_by_name;
  iface->get_children_count = gst_tiovx_simo_child_proxy_get_children_count;
}
