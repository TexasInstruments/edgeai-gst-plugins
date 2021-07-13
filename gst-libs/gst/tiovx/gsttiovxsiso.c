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

#include "gsttiovxbufferpool.h"
#include "gsttiovxmeta.h"
#include "gsttiovxsiso.h"

#include <app_init.h>
#include <gst/video/video.h>
#include <TI/j7.h>

#define MIN_POOL_SIZE 2
#define MAX_POOL_SIZE 8
#define DEFAULT_POOL_SIZE MIN_POOL_SIZE
/* TODO: Implement method to choose number of channels dynamically */
#define DEFAULT_NUM_CHANNELS 1
#define MAX_NUMBER_OF_PLANES 4

enum
{
  PROP_0,
  PROP_POOL_SIZE,
};


GST_DEBUG_CATEGORY_STATIC (gst_ti_ovx_siso_debug_category);
#define GST_CAT_DEFAULT gst_ti_ovx_siso_debug_category

typedef struct _GstTIOVXSisoPrivate
{
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  vx_context context;
  vx_graph graph;
  vx_node *node;
  vx_reference *input;
  vx_reference *output;
  gboolean init_completed;
  guint pool_size;
  guint num_channels;
} GstTIOVXSisoPrivate;

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXSiso, gst_ti_ovx_siso,
    GST_TYPE_BASE_TRANSFORM, G_ADD_PRIVATE (GstTIOVXSiso)
    GST_DEBUG_CATEGORY_INIT (gst_ti_ovx_siso_debug_category, "tiovxsiso", 0,
        "debug category for tiovxsiso base class"));

static void gst_ti_ovx_siso_finalize (GObject * obj);
static gboolean gst_ti_ovx_siso_stop (GstBaseTransform * trans);
static gboolean gst_ti_ovx_siso_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_ti_ovx_siso_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_ti_ovx_siso_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ti_ovx_siso_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_ti_ovx_siso_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static gboolean gst_ti_ovx_siso_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);

static gboolean gst_ti_ovx_siso_modules_init (GstTIOVXSiso * self);
static gboolean gst_ti_ovx_siso_modules_deinit (GstTIOVXSiso * self);
static vx_status gst_ti_ovx_siso_graph_processing (GstTIOVXSiso * self);
static gboolean gst_ti_ovx_siso_add_new_pool (GstTIOVXSiso * self,
    GstQuery * query, guint num_buffers, vx_reference * exemplar,
    GstVideoInfo * info);
static vx_status gst_ti_ovx_siso_transfer_handle (GstTIOVXSiso * self,
    vx_reference src, vx_reference dest);

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

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_finalize);

  /* TODO: Verify passthrough on same caps */
  base_transform_class->passthrough_on_same_caps = TRUE;
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_stop);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_set_caps);
  base_transform_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_decide_allocation);
  base_transform_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_propose_allocation);
  base_transform_class->transform =
      GST_DEBUG_FUNCPTR (gst_ti_ovx_siso_transform);
}

static void
gst_ti_ovx_siso_init (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);
  vx_status status = VX_FAILURE;

  priv->init_completed = FALSE;
  priv->pool_size = DEFAULT_POOL_SIZE;
  priv->num_channels = DEFAULT_NUM_CHANNELS;

  /* App common init */
  GST_DEBUG_OBJECT (self, "Running TIOVX common init");
  if (0 != appCommonInit ()) {
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

  tivxHwaLoadKernels (priv->context);
  goto exit;

free_common:
  tivxHostDeInit ();
  tivxDeInit ();
  appCommonDeInit ();
exit:
  return;
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

static void
gst_ti_ovx_siso_finalize (GObject * obj)
{
  GstTIOVXSiso *self = GST_TI_OVX_SISO (obj);
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);

  g_return_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context));

  tivxHwaUnLoadKernels (priv->context);
  vxReleaseContext (&priv->context);

  /* App common deinit */
  GST_DEBUG_OBJECT (self, "Running TIOVX common deinit");
  tivxHostDeInit ();
  tivxDeInit ();
  appCommonDeInit ();
}

static gboolean
gst_ti_ovx_siso_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstTIOVXSiso *self = GST_TI_OVX_SISO (trans);
  gboolean ret = FALSE;
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);

  GST_LOG_OBJECT (self, "set_caps");

  if (!gst_video_info_from_caps (&priv->in_info, incaps))
    return FALSE;
  if (!gst_video_info_from_caps (&priv->out_info, outcaps))
    return FALSE;

  ret = gst_ti_ovx_siso_modules_init (self);
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
  GstTIOVXSiso *self = GST_TI_OVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);
  GstTIOVXMeta *in_meta = NULL;
  GstTIOVXMeta *out_meta = NULL;
  vx_status status = VX_FAILURE;
  vx_object_array in_array;
  vx_object_array out_array;
  vx_size in_num_channels;
  vx_size out_num_channels;
  vx_reference in_image;
  vx_reference out_image;
  GstFlowReturn ret = GST_FLOW_ERROR;

  in_meta =
      (GstTIOVXMeta *) gst_buffer_get_meta (inbuf, GST_TIOVX_META_API_TYPE);
  if (!in_meta) {
    GST_ERROR_OBJECT (self, "Input Buffer is not a TIOVX buffer");
    goto exit;
  }

  out_meta =
      (GstTIOVXMeta *) gst_buffer_get_meta (outbuf, GST_TIOVX_META_API_TYPE);
  if (!out_meta) {
    GST_ERROR_OBJECT (self, "Output Buffer is not a TIOVX buffer");
    goto exit;
  }

  in_array = in_meta->array;
  out_array = out_meta->array;

  status =
      vxQueryObjectArray (in_array, VX_OBJECT_ARRAY_NUMITEMS, &in_num_channels,
      sizeof (vx_size));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Get number of channels in input buffer failed");
    goto exit;
  }

  status =
      vxQueryObjectArray (out_array, VX_OBJECT_ARRAY_NUMITEMS,
      &out_num_channels, sizeof (vx_size));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Get number of channels in output buffer failed");
    goto exit;
  }

  if ((in_num_channels != priv->num_channels)
      || (out_num_channels != priv->num_channels)) {
    GST_ERROR_OBJECT (self, "Incompatible number of channels received");
    goto exit;
  }

  /* Currently, we support only 1 vx_image per array */
  in_image = vxGetObjectArrayItem (in_array, 0);
  out_image = vxGetObjectArrayItem (out_array, 0);

  /* Transfer handles */
  GST_LOG_OBJECT (self, "Transferring handles");
  gst_ti_ovx_siso_transfer_handle (self, in_image, *priv->input);
  gst_ti_ovx_siso_transfer_handle (self, out_image, *priv->output);

  /* Graph processing */
  status = gst_ti_ovx_siso_graph_processing (self);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph processing failed");
    goto free;
  }

  ret = GST_FLOW_OK;
free:
  vxReleaseReference (&in_image);
  vxReleaseReference (&out_image);
exit:
  return ret;
}

static gboolean
gst_ti_ovx_siso_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstTIOVXSiso *self = GST_TI_OVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);
  gboolean ret = TRUE;
  gint npool = 0;

  GST_LOG_OBJECT (self, "Decide allocation");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);
    GST_INFO_OBJECT (self, "Discarding downstream pool \"%s\"",
        GST_OBJECT_NAME (pool));
    gst_object_unref (pool);

    gst_query_remove_nth_allocation_pool (query, npool);
  }
  /* We use output vx_reference to decide a pool to use downstream */
  ret =
      gst_ti_ovx_siso_add_new_pool (self, query, priv->pool_size, priv->output,
      &priv->out_info);

  return ret;
}

static gboolean
gst_ti_ovx_siso_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstTIOVXSiso *self = GST_TI_OVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_ti_ovx_siso_get_instance_private (self);
  gboolean ret = TRUE;

  GST_LOG_OBJECT (self, "Propose allocation");

  /* We use input vx_reference to propose a pool upstream */
  ret =
      gst_ti_ovx_siso_add_new_pool (self, query, priv->pool_size, priv->input,
      &priv->in_info);

  return ret;
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
gst_ti_ovx_siso_modules_init (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = NULL;
  GstTIOVXSisoClass *klass = NULL;
  vx_graph_parameter_queue_params_t params_list[NUM_PARAMETERS];
  gboolean ret = FALSE;
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (self, FALSE);

  priv = gst_ti_ovx_siso_get_instance_private (self);
  klass = GST_TI_OVX_SISO_GET_CLASS (self);

  /* Init subclass module */
  GST_DEBUG_OBJECT (self, "Calling init module");
  if (!klass->init_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement init_module method.");
    goto error;
  }
  ret =
      klass->init_module (self, priv->context, &priv->in_info, &priv->out_info,
      priv->pool_size, priv->pool_size);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass init module failed");
    goto error;
  }

  /* Create OpenVx Graph */
  GST_DEBUG_OBJECT (self, "Creating graph");
  priv->graph = vxCreateGraph (priv->context);
  status = vxGetStatus ((vx_reference) priv->graph);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph creation failed");
    goto error;
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

  GST_DEBUG_OBJECT (self, "Setting up input parameter");
  status =
      add_graph_parameter_by_node_index (self, INPUT_PARAMETER_INDEX,
      params_list, priv->input);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input parameter failed");
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Setting up output parameter");
  status =
      add_graph_parameter_by_node_index (self, OUTPUT_PARAMETER_INDEX,
      params_list, priv->output);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output parameter failed");
    goto free_graph;
  }

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
  ret = TRUE;
  goto exit;

  /* Free resources in case of failure only. Otherwise they will be released at other moment */
free_graph:
  vxReleaseGraph (&priv->graph);
error:
  /* If we get to free something, it's because something failed */
  ret = FALSE;
exit:
  return ret;
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

  priv->init_completed = FALSE;

exit:
  return ret;
}

static vx_status
gst_ti_ovx_siso_graph_processing (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = NULL;
  vx_status status = VX_FAILURE;
  vx_image input_ret, output_ret;
  uint32_t in_refs, out_refs;

  g_return_val_if_fail (self, VX_FAILURE);

  priv = gst_ti_ovx_siso_get_instance_private (self);

  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) priv->graph), VX_FAILURE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) * priv->input), VX_FAILURE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) * priv->output), VX_FAILURE);

  /* Enqueueing parameters */
  GST_LOG_OBJECT (self, "Enqueueing parameters");

  status =
      vxGraphParameterEnqueueReadyRef (priv->graph, INPUT_PARAMETER_INDEX,
      (vx_reference *) priv->input, priv->num_channels);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input enqueue failed");
    return VX_FAILURE;
  }
  status =
      vxGraphParameterEnqueueReadyRef (priv->graph, OUTPUT_PARAMETER_INDEX,
      (vx_reference *) priv->output, priv->num_channels);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output enqueue failed");
    return VX_FAILURE;
  }

  /* Processing graph */
  GST_LOG_OBJECT (self, "Processing graph");
  status = vxScheduleGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Schedule graph failed");
    return VX_FAILURE;
  }
  status = vxWaitGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Wait graph failed");
    return VX_FAILURE;
  }

  /* Dequeueing parameters */
  GST_LOG_OBJECT (self, "Dequeueing parameters");
  status =
      vxGraphParameterDequeueDoneRef (priv->graph, INPUT_PARAMETER_INDEX,
      (vx_reference *) & input_ret, priv->num_channels, &in_refs);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input dequeue failed");
    return VX_FAILURE;
  }

  if (priv->num_channels != in_refs) {
    GST_ERROR_OBJECT (self, "Input returned an invalid number of channels");
    return VX_FAILURE;
  }

  status =
      vxGraphParameterDequeueDoneRef (priv->graph, OUTPUT_PARAMETER_INDEX,
      (vx_reference *) & output_ret, priv->num_channels, &out_refs);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output dequeue failed");
    return VX_FAILURE;
  }

  if (priv->num_channels != out_refs) {
    GST_ERROR_OBJECT (self, "Output returned an invalid number of channels");
    return VX_FAILURE;
  }

  return VX_SUCCESS;
}

static gboolean
gst_ti_ovx_siso_add_new_pool (GstTIOVXSiso * self, GstQuery * query,
    guint num_buffers, vx_reference * exemplar, GstVideoInfo * info)
{
  GstCaps *caps = NULL;
  GstStructure *config = NULL;
  GstBufferPool *pool = NULL;
  gsize size;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (query, FALSE);
  g_return_val_if_fail (info, FALSE);
  g_return_val_if_fail (exemplar, FALSE);
  g_return_val_if_fail (num_buffers >= MIN_POOL_SIZE, FALSE);

  pool = (GstBufferPool *) gst_tiovx_buffer_pool_new (*exemplar);

  if (!pool) {
    GST_ERROR_OBJECT (self, "Create TIOVX pool failed");
    return FALSE;
  }

  gst_query_parse_allocation (query, &caps, NULL);

  size = GST_VIDEO_INFO_SIZE (info);
  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, num_buffers,
      num_buffers);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Unable to set pool configuration");
    return FALSE;
  }

  GST_INFO_OBJECT (self, "Adding new TIOVX pool with %d buffers of %ld size",
      num_buffers, size);

  gst_query_add_allocation_pool (query, pool, size, num_buffers, num_buffers);

  return TRUE;
}

static vx_status
gst_ti_ovx_siso_transfer_handle (GstTIOVXSiso * self, vx_reference src,
    vx_reference dest)
{
  vx_status status = VX_SUCCESS;
  uint32_t num_entries;
  vx_size src_num_planes;
  vx_size dest_num_planes;
  void *addr[MAX_NUMBER_OF_PLANES];
  uint32_t bufsize[MAX_NUMBER_OF_PLANES];

  g_return_val_if_fail (self, VX_FAILURE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) src), VX_FAILURE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) dest), VX_FAILURE);

  status =
      vxQueryImage ((vx_image) dest, VX_IMAGE_PLANES, &dest_num_planes,
      sizeof (dest_num_planes));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Get number of planes in dest image failed");
    return status;
  }

  status =
      vxQueryImage ((vx_image) src, VX_IMAGE_PLANES, &src_num_planes,
      sizeof (src_num_planes));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Get number of planes in src image failed");
    return status;
  }

  if (src_num_planes != dest_num_planes) {
    GST_ERROR_OBJECT (self,
        "Incompatible number of planes in src and dest images. src: %ld and dest: %ld",
        src_num_planes, dest_num_planes);
    return VX_FAILURE;
  }

  status =
      tivxReferenceExportHandle (src, addr, bufsize, src_num_planes,
      &num_entries);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Export handle failed");
    return status;
  }

  GST_LOG_OBJECT (self, "Number of planes to transfer: %ld", src_num_planes);

  if (src_num_planes != num_entries) {
    GST_ERROR_OBJECT (self,
        "Incompatible number of planes and handles entries. planes: %ld and entries: %d",
        src_num_planes, num_entries);
    return VX_FAILURE;
  }

  status =
      tivxReferenceImportHandle (dest, (const void **) addr, bufsize,
      dest_num_planes);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Import handle failed");
    return status;
  }

  return status;
}
