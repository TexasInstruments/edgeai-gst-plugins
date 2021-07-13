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

#include <app_init.h>
#include <TI/j7.h>

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_simo_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_simo_debug_category

typedef struct _GstTIOVXSimoPrivate
{
  vx_context context;
  vx_graph graph;
  vx_node *node;
  vx_reference *input;
  vx_reference **output;
  gboolean init_completed;
  guint num_parameters;

  GstPad *sinkpad;

} GstTIOVXSimoPrivate;

G_DEFINE_TYPE_WITH_CODE (GstTIOVXSimo, gst_tiovx_simo,
    GST_TYPE_ELEMENT, G_ADD_PRIVATE (GstTIOVXSimo)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_simo_debug_category, "tiovxsimo", 0,
        "debug category for tiovxsimo base class"));

static gboolean gst_tiovx_simo_modules_init (GstTIOVXSimo * self);
static gboolean gst_tiovx_simo_modules_deinit (GstTIOVXSimo * self);

static gboolean gst_tiovx_simo_set_caps (GstTIOVXSimo * self,
    GstPad * pad, GstCaps * caps);

static GstCaps *gst_tiovx_simo_default_transform_caps (GstTIOVXSimo * self,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

static GstStateChangeReturn
gst_tiovx_simo_change_state (GstElement * element, GstStateChange transition);
static gboolean gst_tiovx_simo_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_tiovx_simo_sink_event_func (GstTIOVXSimo * self,
    GstEvent * event);

static void
gst_tiovx_simo_class_init (GstTIOVXSimoClass * klass)
{
  GstElementClass *gstelement_class;

  gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG ("gst_tiovx_simo_class_init");

  klass->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_default_transform_caps);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_change_state);
}

static void
gst_tiovx_simo_init (GstTIOVXSimo * self)
{
  GstPadTemplate *pad_template;
  GstTIOVXSimoClass *klass;
  GstTIOVXSimoPrivate *priv = NULL;

  GST_DEBUG ("gst_tiovx_simo_init");

  klass = GST_TIOVX_SIMO_GET_CLASS (self);
  priv = gst_tiovx_simo_get_instance_private (self);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
  priv->sinkpad = gst_pad_new_from_template (pad_template, "sink");
  gst_pad_set_event_function (priv->sinkpad,
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_sink_event));

  g_return_if_fail (pad_template != NULL);
}

static GstCaps *
gst_tiovx_simo_default_transform_caps (GstTIOVXSimo * self,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *ret;

  ret = NULL;

  if (NULL == caps)
    return NULL;

  return ret;
}

static GstStateChangeReturn
gst_tiovx_simo_change_state (GstElement * element, GstStateChange transition)
{
  GstTIOVXSimo *self;

  GST_DEBUG ("gst_tiovx_simo_change_state");

  self = GST_TIOVX_SIMO (element);

  switch (transition) {
      /* "Start" transition */
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_tiovx_simo_modules_init (self);
      break;
      /* "Stop" transition */
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_tiovx_simo_modules_deinit (self);
      break;
    default:
      break;
  }

  return GST_STATE_CHANGE_SUCCESS;
}

static vx_status
add_graph_parameter_by_node_index (GstTIOVXSimo * self,
    vx_uint32 parameter_index, vx_graph_parameter_queue_params_t params_list[],
    vx_reference * handler)
{
  return VX_SUCCESS;
}

static gboolean
gst_tiovx_simo_modules_init (GstTIOVXSimo * self)
{
  GST_DEBUG ("gst_ti_ovx_simo_modules_init");

  return TRUE;
}

static gboolean
gst_tiovx_simo_modules_deinit (GstTIOVXSimo * self)
{
  GstTIOVXSimoPrivate *priv = NULL;
  GstTIOVXSimoClass *klass = NULL;
  gboolean ret = FALSE;

  GST_DEBUG ("gst_ti_ovx_simo_modules_deinit");

  g_return_val_if_fail (self, FALSE);

  priv = gst_tiovx_simo_get_instance_private (self);
  klass = GST_TIOVX_SIMO_GET_CLASS (self);

  if (!priv->init_completed) {
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
    GST_ERROR_OBJECT (self, "Subclass init module failed");
  }

free_common:
  tivxHwaUnLoadKernels (priv->context);

  vxReleaseGraph (&priv->graph);
  vxReleaseContext (&priv->context);

  tivxHostDeInit ();
  tivxDeInit ();
  appCommonDeInit ();

  priv->init_completed = FALSE;

exit:
  return ret;
}

static gboolean
gst_tiovx_simo_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTIOVXSimo *self;
  gboolean ret = TRUE;

  self = GST_TIOVX_SIMO (parent);

  ret = gst_tiovx_simo_sink_event_func (self, event);

  return ret;
}


static gboolean
gst_tiovx_simo_sink_event_func (GstTIOVXSimo * self, GstEvent * event)
{
  GstTIOVXSimoPrivate *priv = NULL;
  gboolean ret = TRUE;

  priv = gst_tiovx_simo_get_instance_private (self);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_tiovx_simo_set_caps (self, priv->sinkpad, caps);

      break;
    }
    default:
      break;
  }

  /* TODO: this event should be pass to the source(s) pad */
  gst_event_unref (event);

  return ret;
}

static gboolean
gst_tiovx_simo_set_caps (GstTIOVXSimo * self, GstPad * pad, GstCaps * incaps)
{
  GstCaps *outcaps = NULL;
  GstTIOVXSimoClass *klass = NULL;
  GstTIOVXSimoPrivate *priv = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  vx_graph_parameter_queue_params_t *params_list;
  guint parameter_index = 0;

  klass = GST_TIOVX_SIMO_GET_CLASS (self);
  priv = gst_tiovx_simo_get_instance_private (self);

  GST_DEBUG_OBJECT (pad, "have new caps %p %" GST_PTR_FORMAT, incaps, incaps);

  if (!klass->transform_caps) {
    GST_ERROR_OBJECT (self, "No transform caps method available");
    return FALSE;
  }
  /* This should provide the output caps supported in all src pads, note that it is
   * taken that the output caps are the same for all src pads, this can be viewed
   * as a limitation for now */
  outcaps = klass->transform_caps (self, GST_PAD_DIRECTION (pad), incaps, NULL);
  if (!outcaps || gst_caps_is_empty (outcaps)) {
    GST_ERROR_OBJECT (self, "Failed to obtain out caps");
    ret = FALSE;
    goto exit;
  }

  if (!gst_video_info_from_caps (&in_info, incaps))
    return FALSE;
  if (!gst_video_info_from_caps (&out_info, outcaps))
    return FALSE;

  if (0 != appCommonInit ()) {
    GST_ERROR_OBJECT (self, "App common init failed");
    ret = FALSE;
    goto exit;
  }

  tivxInit ();
  tivxHostInit ();

  priv->context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) priv->context);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Context creation failed");
    goto deinit_common;
  }

  tivxHwaLoadKernels (priv->context);

  if (!klass->init_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement init_module method.");
    goto free_context;
  }
  ret = klass->init_module (self, priv->context, &in_info, &out_info);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass init module failed");
    goto free_context;
  }

  priv->graph = vxCreateGraph (priv->context);
  status = vxGetStatus ((vx_reference) priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph creation failed");
    goto free_context;
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
  ret =
      klass->get_node_info (self, &priv->input, priv->output, &priv->node,
      priv->num_parameters);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass get node info failed");
    goto free_graph;
  }

  if (!priv->input) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: input missing");
    goto free_graph;
  }
  if (!priv->output) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: output missing");
    goto free_graph;
  }
  if (!priv->node) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: node missing");
    goto free_graph;
  }
  if (0 == priv->num_parameters) {
    GST_ERROR_OBJECT (self,
        "Incomplete info from subclass: number of graph parameters is 0");
    goto free_graph;
  }

  params_list = malloc (priv->num_parameters * sizeof (*params_list));
  if (NULL == params_list) {
    GST_ERROR_OBJECT (self, "Could not allocate memory for parameters list");
    goto free_graph;
  }

  status =
      add_graph_parameter_by_node_index (self, parameter_index, params_list,
      priv->input);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Setting input parameter failed");
    goto free_parameters_list;
  }

  /*Starts on 1 since input parameter was already set */
  for (parameter_index = 1; parameter_index < priv->num_parameters;
      parameter_index++) {
    status =
        add_graph_parameter_by_node_index (self, parameter_index, params_list,
        priv->output[parameter_index]);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Setting output parameter failed");
      goto free_parameters_list;
    }
  }

  status = vxSetGraphScheduleConfig (priv->graph,
      VX_GRAPH_SCHEDULE_MODE_QUEUE_MANUAL, priv->num_parameters, params_list);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph schedule configuration failed");
    goto free_parameters_list;
  }

  /* Parameters list has to be released even if the code doesn't fail */
  free (params_list);

  status = vxVerifyGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph verification failed");
    goto free_graph;
  }

  if (!klass->configure_module) {
    GST_ERROR_OBJECT (self,
        "Subclass did not implement configure node method.");
    goto free_graph;
  }
  ret = klass->configure_module (self, &priv->node);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass configure node failed");
    goto free_graph;
  }

  ret = TRUE;
  goto exit;

free_parameters_list:
  free (params_list);

free_graph:
  vxReleaseGraph (&priv->graph);

free_context:
  tivxHwaUnLoadKernels (priv->context);
  vxReleaseContext (&priv->context);

deinit_common:
  tivxHostDeInit ();
  tivxDeInit ();
  appCommonDeInit ();

exit:
  return ret;
}
