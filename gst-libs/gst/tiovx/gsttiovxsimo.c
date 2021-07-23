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

#include <app_init.h>
#include <TI/j7.h>

#include "gsttiovxcontext.h"

#define MIN_BATCH_SIZE 1
#define MAX_BATCH_SIZE 8
#define DEFAULT_BATCH_SIZE MIN_BATCH_SIZE

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_simo_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_simo_debug_category

typedef struct _GstTIOVXSimoPrivate
{
  vx_context context;
  vx_graph graph;
  vx_node node;
  vx_reference *input_refs;
  vx_reference **output_refs;

  guint next_pad_index;
  guint num_pads;
  guint in_batch_size;
  GstTIOVXPad *sinkpad;
  GList *srcpads;
  GHashTable *out_batch_sizes;
  GList *src_caps_list;

  GstTIOVXContext *tiovx_context;
} GstTIOVXSimoPrivate;

static gint private_offset = 0;
static void gst_tiovx_simo_class_init (GstTIOVXSimoClass * klass);
static void gst_tiovx_simo_init (GstTIOVXSimo * simo,
    GstTIOVXSimoClass * klass);

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
    GstCaps * sink_caps);
static gboolean gst_tiovx_simo_start (GstTIOVXSimo * self);
static gboolean gst_tiovx_simo_stop (GstTIOVXSimo * self);

static void gst_tiovx_simo_finalize (GObject * object);

static GstStateChangeReturn
gst_tiovx_simo_change_state (GstElement * element, GstStateChange transition);
static gboolean gst_tiovx_simo_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstPad *gst_tiovx_simo_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * unused, const GstCaps * caps);
static void gst_tiovx_simo_release_pad (GstElement * element, GstPad * pad);

static gboolean gst_tiovx_simo_set_caps (GstTIOVXSimo * self,
    GstPad * pad, GstCaps * sink_caps);
static GstCaps *gst_tiovx_simo_default_get_caps (GstTIOVXSimo * self,
    GstCaps * filter, GList * src_caps_list);
static GList *gst_tiovx_simo_default_fixate_caps (GstTIOVXSimo * self,
    GstCaps * sink_caps, GList * src_caps_list);
static gboolean gst_tiovx_simo_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

guint
gst_tiovx_simo_get_num_pads (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = NULL;
  guint result = 0;

  g_return_val_if_fail (GST_IS_TIOVX_SIMO (self), FALSE);

  priv = gst_tiovx_simo_get_instance_private (self);

  GST_OBJECT_LOCK (self);
  result = priv->num_pads;
  GST_OBJECT_UNLOCK (self);

  return result;
}

static void
gst_tiovx_simo_class_init (GstTIOVXSimoClass * klass)
{
  GstElementClass *gstelement_class = NULL;
  GObjectClass *gobject_class = NULL;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_simo_finalize);

  klass->get_caps = GST_DEBUG_FUNCPTR (gst_tiovx_simo_default_get_caps);
  klass->fixate_caps = GST_DEBUG_FUNCPTR (gst_tiovx_simo_default_fixate_caps);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_change_state);
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_release_pad);
}

static void
gst_tiovx_simo_init (GstTIOVXSimo * self, GstTIOVXSimoClass * klass)
{
  GstPadTemplate *pad_template = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  vx_status status = VX_FAILURE;

  GST_DEBUG_OBJECT (self, "gst_tiovx_simo_init");

  gstelement_class = GST_ELEMENT_CLASS (klass);
  priv = gst_tiovx_simo_get_instance_private (self);

  pad_template = gst_element_class_get_pad_template (gstelement_class, "sink");
  g_return_if_fail (pad_template != NULL);

  priv->sinkpad =
      GST_TIOVX_PAD (gst_pad_new_from_template (pad_template, "sink"));

  gst_pad_set_event_function (GST_PAD (priv->sinkpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_sink_event));
  gst_pad_set_query_function (GST_PAD (priv->sinkpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_query));
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD (priv->sinkpad));
  gst_pad_set_active (GST_PAD (priv->sinkpad), FALSE);

  /* Initialize all private member data */
  priv->context = NULL;
  priv->graph = NULL;
  priv->node = NULL;
  priv->input_refs = NULL;
  priv->output_refs = NULL;

  priv->next_pad_index = 0;
  priv->num_pads = 0;
  priv->in_batch_size = 1;

  priv->sinkpad = NULL;
  priv->srcpads = NULL;
  priv->src_caps_list = NULL;

  priv->tiovx_context = NULL;

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

  return;
}

static vx_status
add_graph_pool_parameter_by_node_index (GstTIOVXSimo * self,
    vx_uint32 parameter_index, vx_graph_parameter_queue_params_t params_list[],
    vx_reference * image_reference_list, guint ref_list_size)
{
  GstTIOVXSimoPrivate *priv = NULL;
  vx_status status = VX_FAILURE;
  vx_parameter parameter = NULL;
  vx_graph graph = NULL;
  vx_node node = NULL;

  g_return_val_if_fail (self, VX_FAILURE);
  g_return_val_if_fail (image_reference_list, VX_FAILURE);
  g_return_val_if_fail (parameter_index >= 0, VX_FAILURE);
  g_return_val_if_fail (ref_list_size >= MIN_BATCH_SIZE, VX_FAILURE);

  priv = gst_tiovx_simo_get_instance_private (self);
  g_return_val_if_fail (priv, VX_FAILURE);
  g_return_val_if_fail (priv->graph, VX_FAILURE);
  g_return_val_if_fail (priv->node, VX_FAILURE);

  graph = priv->graph;
  node = priv->node;

  parameter = vxGetParameterByIndex (node, parameter_index);
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

  params_list[parameter_index].graph_parameter_index = parameter_index;
  params_list[parameter_index].refs_list_size = ref_list_size;
  params_list[parameter_index].refs_list = image_reference_list;

  status = VX_SUCCESS;

  return status;
}

static gboolean
gst_tiovx_simo_start (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = NULL;
  guint i = 0;

  priv = gst_tiovx_simo_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "gst_ti_ovx_simo_modules_init");

  priv->out_batch_sizes = g_hash_table_new (NULL, NULL);

  for (i = 0; i < priv->num_pads; i++) {
    g_hash_table_insert (priv->out_batch_sizes, GUINT_TO_POINTER (i),
        GUINT_TO_POINTER (DEFAULT_BATCH_SIZE));
  }

  return TRUE;
}

static gboolean
gst_tiovx_simo_modules_init (GstTIOVXSimo * self, GstCaps * sink_caps)
{
  GstTIOVXSimoClass *klass = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  vx_graph_parameter_queue_params_t *params_list = NULL;
  guint i = 0;
  guint batch_size;

  priv = gst_tiovx_simo_get_instance_private (self);

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
  ret = klass->init_module (self, priv->context, priv->sinkpad, priv->srcpads);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass init module failed");
    goto exit;
  }

  priv->graph = vxCreateGraph (priv->context);
  status = vxGetStatus ((vx_reference) priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Graph creation failed, vx_status %" G_GINT32_FORMAT, status);
    goto deinit_module;
  }

  if (!klass->create_graph) {
    GST_ERROR_OBJECT (self, "Subclass did not implement create_graph method.");
    goto free_graph;
  }
  ret = klass->create_graph (self, priv->context, priv->graph);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass create graph failed");
    goto free_graph;
  }

  if (!klass->get_node_info) {
    GST_ERROR_OBJECT (self, "Subclass did not implement get_node_info method");
    goto free_graph;
  }
  ret = klass->get_node_info (self, &priv->node, priv->sinkpad, priv->srcpads);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass get node info failed");
    goto free_graph;
  }

  if (!priv->input_refs) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: input missing");
    goto free_graph;
  }
  if (!priv->output_refs) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: output missing");
    goto free_graph;
  }
  if (!priv->node) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: node missing");
    goto free_graph;
  }
  if (0 == priv->num_pads) {
    GST_ERROR_OBJECT (self,
        "Incomplete info from subclass: number of graph parameters is 0");
    goto free_graph;
  }

  params_list = g_malloc0 (priv->num_pads * sizeof (*params_list));
  if (NULL == params_list) {
    GST_ERROR_OBJECT (self, "Could not allocate memory for parameters list");
    goto free_graph;
  }

  status =
      add_graph_pool_parameter_by_node_index (self, i, params_list,
      priv->input_refs, priv->in_batch_size);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Setting input parameter failed, vx_status %" G_GINT32_FORMAT, status);
    goto free_parameters_list;
  }

  for (i = 0; i < priv->num_pads; i++) {
    if (g_hash_table_contains (priv->out_batch_sizes, GUINT_TO_POINTER (i))) {
      batch_size =
          GPOINTER_TO_UINT (g_hash_table_lookup (priv->out_batch_sizes,
              GUINT_TO_POINTER (i)));
    } else {
      GST_ERROR_OBJECT (self, "Fail to obtain output pool size");
      goto free_parameters_list;
    }
    /*Starts on 1 since input parameter was already set */
    status =
        add_graph_pool_parameter_by_node_index (self, i + 1, params_list,
        priv->output_refs[i], batch_size);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Setting output parameter failed, vx_status %" G_GINT32_FORMAT,
          status);
      goto free_parameters_list;
    }
  }

  status = vxSetGraphScheduleConfig (priv->graph,
      VX_GRAPH_SCHEDULE_MODE_QUEUE_MANUAL, priv->num_pads, params_list);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Graph schedule configuration failed, vx_status %" G_GINT32_FORMAT,
        status);
    goto free_parameters_list;
  }

  /* Parameters list has to be released even if the code doesn't fail */
  g_free (params_list);

  status = vxVerifyGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Graph verification failed, vx_status %" G_GINT32_FORMAT, status);
    goto free_graph;
  }

  if (!klass->configure_module) {
    GST_LOG_OBJECT (self,
        "Subclass did not implement configure node method. Skipping node configuration");
  } else {
    ret = klass->configure_module (self);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Subclass configure node failed");
      goto free_graph;
    }
  }

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
  ret = klass->deinit_module (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass deinit module failed");
  }

exit:
  return ret;
}

static gboolean
gst_tiovx_simo_stop (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = NULL;
  GstTIOVXSimoClass *klass = NULL;
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (self, "gst_ti_ovx_simo_modules_deinit");

  g_return_val_if_fail (self, FALSE);

  priv = gst_tiovx_simo_get_instance_private (self);
  klass = GST_TIOVX_SIMO_GET_CLASS (self);

  if (VX_SUCCESS != vxGetStatus ((vx_reference) priv->graph)) {
    GST_WARNING_OBJECT (self,
        "Trying to deinit modules but initialization was not completed, skipping...");
    ret = FALSE;
    goto exit;
  }

  if (!klass->deinit_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement deinit_module method");
    goto free_common;
  }
  ret = klass->deinit_module (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass deinit module failed");
  }

  g_hash_table_unref (priv->out_batch_sizes);

free_common:
  priv->node = NULL;
  vxReleaseGraph (&priv->graph);
  priv->graph = NULL;

exit:
  return ret;
}

static void
gst_tiovx_simo_finalize (GObject * gobject)
{
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSimo *self = NULL;
  GstTIOVXSimoClass *klass = NULL;
  GstTIOVXSimoPrivate *priv = NULL;

  self = GST_TIOVX_SIMO (gobject);

  klass = GST_TIOVX_SIMO_GET_CLASS (self);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  priv = gst_tiovx_simo_get_instance_private (self);

  if (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context)) {
    tivxHwaUnLoadKernels (priv->context);
    vxReleaseContext (&priv->context);
    priv->context = NULL;
  }

  if (priv->tiovx_context) {
    g_object_unref (priv->tiovx_context);
  }

  if (priv->src_caps_list) {
    g_list_free_full (priv->src_caps_list, (GDestroyNotify) gst_caps_unref);
  }

  if (priv->srcpads) {
    g_list_free_full (priv->srcpads, (GDestroyNotify) gst_caps_unref);
  }

  priv->src_caps_list = NULL;
  priv->srcpads = NULL;

  G_OBJECT_CLASS (gstelement_class)->finalize (gobject);
}

static GstStateChangeReturn
gst_tiovx_simo_change_state (GstElement * element, GstStateChange transition)
{
  GstTIOVXSimo *self = NULL;
  GstTIOVXSimoClass *klass = NULL;
  GstElementClass *gstelement_class = NULL;
  gboolean ret = FALSE;
  GstStateChangeReturn result;

  self = GST_TIOVX_SIMO (element);
  klass = GST_TIOVX_SIMO_GET_CLASS (self);
  gstelement_class = GST_ELEMENT_CLASS (klass);

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

  result =
      GST_ELEMENT_CLASS (gstelement_class)->change_state (element, transition);

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
  GstTIOVXSimo *self = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  GList *src_pads_sublist = NULL;
  GstPad *src_pad = NULL;
  GstPad *item = NULL;
  gchar *name = NULL;
  gchar *object_name = NULL;
  guint tmpl_name_index = 0;
  guint name_index = 0;

  self = GST_TIOVX_SIMO (element);
  priv = gst_tiovx_simo_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "requesting pad");

  g_return_val_if_fail (templ, NULL);
  g_return_val_if_fail (name_templ, NULL);

  GST_OBJECT_LOCK (self);

  src_pads_sublist = priv->srcpads;

  /* Name template is of the form src_%u, the element assigns the index */
  if (strcmp (name_templ, "src_%u")) {
    guint current_highest = 0;
    GST_DEBUG_OBJECT (self, "Computing next index for pad name");

    /* Find highest index */
    current_highest = priv->next_pad_index;

    while (NULL != src_pads_sublist) {
      GList *next = g_list_next (src_pads_sublist);

      /* Higher (available) index found */
      if (name_index > current_highest) {
        current_highest = name_index;
        priv->next_pad_index = current_highest;
        break;
      }

      name_index++;

      src_pads_sublist = next;
    }

    current_highest = current_highest + 1;
    name_index = current_highest;
    priv->next_pad_index = current_highest;
  }
  /*Name template is of the form src_n, accept or reject provided name */
  if (sscanf (name_templ, "src_%u", &tmpl_name_index)) {
    GST_DEBUG_OBJECT (self, "Verifying provided pad name");
    while (NULL != src_pads_sublist) {
      guint object_name_index = 0;
      GList *next = g_list_next (src_pads_sublist);

      item = GST_PAD (src_pads_sublist->data);

      object_name = GST_OBJECT_NAME (item);

      sscanf (object_name, "src_%u", &object_name_index);

      if (tmpl_name_index == object_name_index) {
        GST_ERROR_OBJECT (self,
            "Failed, index in name template already in use");
        GST_OBJECT_UNLOCK (self);
        return NULL;
      }

      src_pads_sublist = next;
    }
    name_index = tmpl_name_index;
    priv->next_pad_index = tmpl_name_index;
  }
  /* Fail for any other case */
  else {
    GST_ERROR_OBJECT (self, "Incorrect pad name template format provided");
    GST_OBJECT_UNLOCK (self);
    return NULL;
  }

  name = g_strdup_printf ("src_%u", name_index);

  src_pad = gst_pad_new_from_template (templ, name);
  if (NULL == src_pad) {
    GST_ERROR_OBJECT (self, "Failed to obtain source pad from template");
    goto free_name_unlock;
  }

  if (g_list_find (priv->srcpads, src_pad)) {
    GST_ERROR_OBJECT (self, "pad %s is not unique in the pad list", name_templ);
    goto unref_src_pad;
  }

  priv->srcpads = g_list_append (priv->srcpads, src_pad);
  priv->num_pads++;

  g_free (name);

  GST_OBJECT_UNLOCK (self);

  gst_pad_set_active (src_pad, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (self), src_pad);

  return src_pad;

unref_src_pad:
  g_object_unref (src_pad);

free_name_unlock:
  g_free (name);
  GST_OBJECT_UNLOCK (self);

  return NULL;
}

static void
gst_tiovx_simo_release_pad (GstElement * element, GstPad * pad)
{
  GstTIOVXSimo *self = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  GList *src_pads_sublist = NULL;
  GstPad *src_pad = NULL;

  self = GST_TIOVX_SIMO (element);
  priv = gst_tiovx_simo_get_instance_private (self);

  GST_OBJECT_LOCK (self);

  src_pads_sublist = g_list_find (priv->srcpads, pad);
  if (src_pads_sublist) {
    src_pad = GST_PAD (src_pads_sublist->data);
    g_object_unref (src_pad);
  }
  priv->srcpads = g_list_remove (src_pads_sublist, pad);

  priv->num_pads--;

  GST_OBJECT_UNLOCK (self);

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT_CAST (self), pad);

  return;
}

static GList *
gst_tiovx_simo_get_src_caps_list (GstTIOVXSimo * self, GstCaps * filter)
{
  GstTIOVXSimoPrivate *priv = NULL;
  GstCaps *peer_caps = NULL;
  GList *src_caps_list = NULL;
  GList *src_pads_sublist = NULL;
  guint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (filter, NULL);

  priv = gst_tiovx_simo_get_instance_private (self);

  src_pads_sublist = priv->srcpads;
  while (NULL != src_pads_sublist) {
    GstPad *src_pad = NULL;
    GList *next = g_list_next (src_pads_sublist);

    src_pad = GST_PAD (src_pads_sublist->data);
    g_return_val_if_fail (src_pad, NULL);

    /* Ask peer for what should the source caps (sink caps in the other end) be */
    peer_caps = gst_pad_peer_query_caps (src_pad, filter);
    g_return_val_if_fail (peer_caps, NULL);

    /* Insert at the end of the src caps list */
    src_caps_list = g_list_insert (src_caps_list, peer_caps, -1);

    src_pads_sublist = next;
    i++;
  }

  return src_caps_list;
}

static GstCaps *
gst_tiovx_simo_default_get_caps (GstTIOVXSimo * self,
    GstCaps * filter, GList * src_caps_list)
{
  GstCaps *ret = NULL;
  GstCaps *tmp = NULL;
  GList *src_caps_sublist = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  /* Loop through the list of src pads caps and by default fully
   * intersect the list of source caps with the filter */
  src_caps_sublist = src_caps_list;
  ret = gst_caps_ref (filter);
  while (NULL != src_caps_sublist) {
    GstCaps *src_caps = NULL;
    GList *next = g_list_next (src_caps_sublist);

    src_caps = (GstCaps *) src_caps_sublist->data;
    g_return_val_if_fail (src_caps, NULL);

    tmp = gst_caps_intersect_full (ret, src_caps, GST_CAPS_INTERSECT_FIRST);
    GST_DEBUG_OBJECT (self,
        "src and filter caps intersected %" GST_PTR_FORMAT, ret);
    gst_caps_unref (ret);
    ret = tmp;

    src_caps_sublist = next;
  }

  return ret;
}

static gboolean
gst_tiovx_simo_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXSimo *self = NULL;
  GstTIOVXSimoClass *klass = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  GstCaps *sink_caps = NULL;
  gboolean ret = FALSE;

  self = GST_TIOVX_SIMO (parent);
  klass = GST_TIOVX_SIMO_GET_CLASS (self);
  priv = gst_tiovx_simo_get_instance_private (self);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter;
      GList *src_caps_list = NULL;

      if (pad != GST_PAD (priv->sinkpad)) {
        GST_ERROR_OBJECT (self, "Pad from query is not the element sink pad");
        goto exit;
      }

      gst_query_parse_caps (query, &filter);

      src_caps_list = gst_tiovx_simo_get_src_caps_list (self, filter);
      if (NULL == src_caps_list) {
        GST_ERROR_OBJECT (self, "Get src caps list method failed");
        goto exit;
      }

      /* Should return the caps the element supports on the sink pad */
      sink_caps = klass->get_caps (self, filter, src_caps_list);
      if (NULL == sink_caps) {
        GST_ERROR_OBJECT (self, "Get caps method failed");
      }

      ret = TRUE;

      /* The query response should be the supported caps in the sink
       * from get_caps */
      gst_query_set_caps_result (query, sink_caps);

      gst_caps_unref (sink_caps);

      priv->src_caps_list = src_caps_list;

    exit:
      break;
    }
    default:
      ret = gst_pad_query_default (GST_PAD (priv->sinkpad), parent, query);
      break;
  }

  return ret;
}

static gboolean
gst_tiovx_simo_set_caps (GstTIOVXSimo * self, GstPad * pad, GstCaps * sink_caps)
{
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (pad, "have new caps %p %" GST_PTR_FORMAT, sink_caps,
      sink_caps);

  ret = gst_tiovx_simo_modules_init (self, sink_caps);
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
  GList *src_caps_sublist = NULL;
  GList *ret = NULL;

  g_return_val_if_fail (sink_caps, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  src_caps_sublist = src_caps_list;
  while (NULL != src_caps_sublist) {
    GstCaps *src_caps = NULL;
    GstCaps *src_caps_fixated = NULL;
    GList *next = g_list_next (src_caps_sublist);

    src_caps = (GstCaps *) src_caps_sublist->data;
    g_return_val_if_fail (src_caps, NULL);

    src_caps_fixated = gst_caps_fixate (src_caps);
    ret = g_list_append (ret, src_caps_fixated);

    src_caps_sublist = next;
  }

  return ret;
}

static gboolean
gst_tiovx_simo_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTIOVXSimo *self = NULL;
  GstTIOVXSimoClass *klass = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  gboolean ret = FALSE;

  self = GST_TIOVX_SIMO (parent);
  klass = GST_TIOVX_SIMO_GET_CLASS (self);
  priv = gst_tiovx_simo_get_instance_private (self);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *sink_caps = NULL;
      GList *src_caps_list = NULL;
      GList *src_caps_sublist = NULL;
      GList *src_pads_sublist = NULL;

      gst_event_parse_caps (event, &sink_caps);

      /* Should return the fixated caps the element will use on the src pads */
      src_caps_list = klass->fixate_caps (self, sink_caps, priv->src_caps_list);
      if (!src_caps_list) {
        GST_ERROR_OBJECT (self, "Fixate caps method failed");
        goto exit;
      }
      /* Discard previous list of source caps */
      g_list_free_full (priv->src_caps_list, (GDestroyNotify) gst_caps_unref);
      priv->src_caps_list = src_caps_list;

      ret = gst_tiovx_simo_set_caps (self, GST_PAD (priv->sinkpad), sink_caps);
      if (!ret) {
        GST_ERROR_OBJECT (self, "Set caps method failed");
        goto exit;
      }

      src_pads_sublist = priv->srcpads;

      while (NULL != src_caps_list) {
        GstCaps *src_caps = NULL;
        GstPad *src_pad = NULL;

        GList *next_src_caps = g_list_next (src_caps_sublist);
        GList *next_src_pad = g_list_next (src_pads_sublist);

        src_caps = (GstCaps *) src_caps_sublist->data;
        if (!src_caps) {
          GST_ERROR_OBJECT (self, "Failed to obtain source caps from list");
          goto exit;
        }

        src_pad = (GstPad *) src_pads_sublist->data;
        if (!src_caps) {
          GST_ERROR_OBJECT (self, "Failed to obtain source pad from list");
          goto exit;
        }

        /* Notify peer pads downstream about fixated caps in this pad */
        gst_pad_push_event (src_pad, gst_event_new_caps (src_caps));

        src_caps_sublist = next_src_caps;
        src_pads_sublist = next_src_pad;
      }

    exit:
      gst_event_unref (event);

      break;
    }
    default:
      ret = gst_pad_event_default (GST_PAD (priv->sinkpad), parent, event);
      break;
  }

  return ret;
}