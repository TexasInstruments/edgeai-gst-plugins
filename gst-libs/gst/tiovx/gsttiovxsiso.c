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

#include "gsttiovxsiso.h"

#include <app_init.h>
#include <gst/video/video.h>
#include <TI/j7.h>

#define MIN_POOL_SIZE 2
#define MAX_POOL_SIZE 8
#define DEFAULT_POOL_SIZE MIN_POOL_SIZE

enum
{
  PROP_0,
  PROP_POOL_SIZE,
};


GST_DEBUG_CATEGORY_STATIC (gst_ti_ovx_siso_debug_category);
#define GST_CAT_DEFAULT gst_ti_ovx_siso_debug_category

typedef struct _GstTIOVXSisoPrivate
{
  GstVideoInfo in_caps_info;
  GstVideoInfo out_caps_info;
  vx_context context;
  vx_graph graph;
  vx_node *node;
  vx_reference *input;
  vx_reference *output;
  gboolean init_completed;
  guint pool_size;
} GstTIOVXSisoPrivate;

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXSiso, gst_ti_ovx_siso,
    GST_TYPE_BASE_TRANSFORM, G_ADD_PRIVATE (GstTIOVXSiso)
    GST_DEBUG_CATEGORY_INIT (gst_ti_ovx_siso_debug_category, "tiovxsiso", 0,
        "debug category for tiovxsiso base class"));

static gboolean gst_ti_ovx_siso_stop (GstBaseTransform * trans);
static gboolean gst_ti_ovx_siso_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_ti_ovx_siso_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_ti_ovx_siso_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ti_ovx_siso_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_ti_ovx_siso_modules_init (GstTIOVXSiso * priv,
    GstVideoInfo * in_info, GstVideoInfo * out_info);
static gboolean gst_ti_ovx_siso_modules_deinit (GstTIOVXSiso * priv);

static void
gst_ti_ovx_siso_class_init (GstTIOVXSisoClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_ti_ovx_siso_set_property;
  gobject_class->get_property = gst_ti_ovx_siso_get_property;

  g_object_class_install_property (gobject_class, PROP_POOL_SIZE,
      g_param_spec_uint ("pool-size", "Pool Size",
          "Number of buffers to allocate in pool", MIN_POOL_SIZE, MAX_POOL_SIZE,
          DEFAULT_POOL_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_stop);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_set_caps);
  base_transform_class->transform =
      GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_transform);
}

static void
gst_ti_ovx_siso_init (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);

  priv->init_completed = FALSE;
  priv->pool_size = DEFAULT_POOL_SIZE;
}

static void
gst_ti_ovx_siso_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXSiso *self = GST_TI_OVX_SISO (object);
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_POOL_SIZE:
      priv->pool_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_ti_ovx_siso_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXSiso *self = GST_TI_OVX_SISO (object);
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_POOL_SIZE:
      g_value_set_uint (value, priv->pool_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_ti_ovx_siso_stop (GstBaseTransform * trans)
{
  GstTIOVXSiso *self = GST_TI_OVX_SISO (trans);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "stop");

  ret = gst_ti_ovx_siso_modules_deinit (self);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to deinit TIOVX module"), (NULL));
  }

  return ret;
}

static gboolean
gst_ti_ovx_siso_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstTIOVXSiso *self;
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  gboolean ret = FALSE;

  self = GST_TI_OVX_SISO (trans);

  GST_LOG_OBJECT (self, "set_caps");

  if (!gst_video_info_from_caps (&in_info, incaps))
    return FALSE;
  if (!gst_video_info_from_caps (&out_info, outcaps))
    return FALSE;

  ret = gst_ti_ovx_siso_modules_init (self, &in_info, &out_info);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to init TIOVX module"), (NULL));
  }

  return ret;
}

static GstFlowReturn
gst_ti_ovx_siso_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  return GST_FLOW_OK;
}

/* Private functions */

static vx_status
add_graph_parameter_by_node_index (GstTIOVXSiso * self,
    vx_uint32 parameter_index, vx_graph_parameter_queue_params_t params_list[],
    vx_reference * handler)
{
  GstTIOVXSisoPrivate *priv;
  vx_parameter parameter;
  vx_graph graph;
  vx_node node;
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (self, VX_FAILURE);
  g_return_val_if_fail (handler, VX_FAILURE);
  g_return_val_if_fail (parameter_index >= 0, VX_FAILURE);

  priv = gst_ti_ovx_siso_get_instance_private (self);
  g_return_val_if_fail (priv, VX_FAILURE);
  g_return_val_if_fail (priv->graph, VX_FAILURE);
  g_return_val_if_fail (priv->node, VX_FAILURE);

  graph = priv->graph;
  node = *priv->node;

  parameter = vxGetParameterByIndex (node, parameter_index);
  status = vxAddParameterToGraph (graph, parameter);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Add parameter to graph failed");
    return status;
  }

  status = vxReleaseParameter (&parameter);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release parameter failed");
    return status;
  }

  params_list[parameter_index].graph_parameter_index = parameter_index;
  params_list[parameter_index].refs_list_size = priv->pool_size;
  params_list[parameter_index].refs_list = (vx_reference *) handler;

  return status;
}

static gboolean
gst_ti_ovx_siso_modules_init (GstTIOVXSiso * self, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstTIOVXSisoPrivate *priv = NULL;
  GstTIOVXSisoClass *klass = NULL;
  vx_graph_parameter_queue_params_t params_list[NUM_PARAMETERS];
  gboolean ret = FALSE;
  int32_t status = 0;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  priv = gst_ti_ovx_siso_get_instance_private (self);
  klass = GST_TI_OVX_SISO_GET_CLASS (self);

  /* App common init */
  GST_DEBUG_OBJECT (self, "Running TIOVX common init");
  status = appCommonInit ();
  if (0 != status) {
    GST_ERROR_OBJECT (self, "App common init failed");
    goto exit;
  }

  tivxInit ();
  tivxHostInit ();

  /* Create OpenVx Context */
  GST_DEBUG_OBJECT (self, "Creating context");
  priv->context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) priv->context);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Context creation failed");
    goto free_common;
  }

  /* Init subclass module */
  GST_DEBUG_OBJECT (self, "Calling init module");
  if (!klass->init_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement init_module method.");
    goto free_context;
  }
  ret =
      klass->init_module (self, priv->context, in_info, out_info,
      priv->pool_size, priv->pool_size);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass init module failed");
    goto free_context;
  }

  /* Create OpenVx Graph */
  GST_DEBUG_OBJECT (self, "Creating graph");
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

  /* Set Graph parameters */
  GST_DEBUG_OBJECT (self, "Getting subclass node and exemplars");
  if (!klass->get_node_info) {
    GST_ERROR_OBJECT (self, "Subclass did not implement get_node_info method");
    goto free_graph;
  }
  ret = klass->get_node_info (self, &priv->input, &priv->output, &priv->node);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass create graph failed");
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

  GST_DEBUG_OBJECT (self, "Setting up input parameter");
  status =
      add_graph_parameter_by_node_index (self, INPUT_PARAMETER_INDEX,
      params_list, priv->input);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input parameter failed");
    goto free_graph;
  }
  /*TODO: Set parameter index in subclass */

  GST_DEBUG_OBJECT (self, "Setting up output parameter");
  status =
      add_graph_parameter_by_node_index (self, OUTPUT_PARAMETER_INDEX,
      params_list, priv->output);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output parameter failed");
    goto free_graph;
  }
  /*TODO: Set parameter index in subclass */

  status = vxSetGraphScheduleConfig (priv->graph,
      VX_GRAPH_SCHEDULE_MODE_QUEUE_MANUAL, NUM_PARAMETERS, params_list);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph schedule configuration failed");
    goto free_graph;
  }

  /* Verify Graph */
  GST_DEBUG_OBJECT (self, "Verifying graph");
  status = vxVerifyGraph (priv->graph);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph verification failed");
    goto free_graph;
  }

  /* Release buffer. This is needed in order to free resources allocated by vxVerifyGraph function */
  if (!klass->release_buffer) {
    GST_ERROR_OBJECT (self,
        "Subclass did not implement release_buffer method.");
    goto free_graph;
  }
  ret = klass->release_buffer (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass release buffer failed");
    goto free_graph;
  }

  priv->init_completed = TRUE;
exit:
  return ret;

  /* Free resources in case of failure only. Otherwise they will be released at other moment */
free_graph:
  vxReleaseGraph (&priv->graph);
free_context:
  vxReleaseContext (&priv->context);
free_common:
  tivxHostDeInit ();
  tivxDeInit ();
  appCommonDeInit ();

  /* If we get to free something, it's because something failed */
  ret = FALSE;
  goto exit;
}

static gboolean
gst_ti_ovx_siso_modules_deinit (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = NULL;
  GstTIOVXSisoClass *klass = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);

  priv = gst_ti_ovx_siso_get_instance_private (self);
  klass = GST_TI_OVX_SISO_GET_CLASS (self);

  if (!priv->init_completed) {
    GST_WARNING_OBJECT (self,
        "Trying to deinit modules but initialization was not completed, ignoring...");
    ret = TRUE;
    goto exit;
  }
  /* Deinit subclass module */
  GST_DEBUG_OBJECT (self, "Calling deinit module");
  if (!klass->deinit_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement deinit_module method.");
    goto free_common;
  }
  ret = klass->deinit_module (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass init module failed");
  }

free_common:
  GST_DEBUG_OBJECT (self, "Release graph and context");
  vxReleaseGraph (&priv->graph);
  vxReleaseContext (&priv->context);

  /* App common deinit */
  GST_DEBUG_OBJECT (self, "Running TIOVX common deinit");
  tivxHostDeInit ();
  tivxDeInit ();
  appCommonDeInit ();

  priv->init_completed = FALSE;

exit:
  return ret;
}
