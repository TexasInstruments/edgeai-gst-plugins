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

#include "gsttiovx.h"
#include "gsttiovxbufferpool.h"
#include "gsttiovxbufferutils.h"
#include "gsttiovxbufferpoolutils.h"
#include "gsttiovxcontext.h"
#include "gsttiovxtensorbufferpool.h"
#include "gsttiovxutils.h"

#define DEFAULT_POOL_SIZE MIN_POOL_SIZE
#define DEFAULT_PARAM_INDEX 0

enum
{
  PROP_0,
  PROP_IN_POOL_SIZE,
  PROP_OUT_POOL_SIZE,
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_siso_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_siso_debug_category

typedef struct _GstTIOVXSisoPrivate
{
  GstCaps *in_caps;
  GstCaps *out_caps;
  GstTIOVXContext *tiovx_context;
  vx_context context;
  vx_graph graph;
  vx_node node;
  vx_object_array input;
  vx_object_array output;
  vx_reference input_ref;
  vx_reference output_ref;

  guint in_pool_size;
  guint out_pool_size;
  guint in_param_index;
  guint out_param_index;
  guint num_channels;

  GstBufferPool *sink_buffer_pool;

} GstTIOVXSisoPrivate;

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXSiso, gst_tiovx_siso,
    GST_TYPE_BASE_TRANSFORM, G_ADD_PRIVATE (GstTIOVXSiso)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_siso_debug_category, "tiovxsiso", 0,
        "debug category for tiovxsiso base class"));

static void gst_tiovx_siso_finalize (GObject * obj);
static gboolean gst_tiovx_siso_stop (GstBaseTransform * trans);
static gboolean gst_tiovx_siso_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_tiovx_siso_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_tiovx_siso_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_siso_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_tiovx_siso_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static gboolean gst_tiovx_siso_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);

static gboolean gst_tiovx_siso_is_subclass_complete (GstTIOVXSiso * self);
static gboolean gst_tiovx_siso_modules_init (GstTIOVXSiso * self);
static gboolean gst_tiovx_siso_modules_deinit (GstTIOVXSiso * self);
static vx_status gst_tiovx_siso_process_graph (GstTIOVXSiso * self);

static void
gst_tiovx_siso_class_init (GstTIOVXSisoClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_tiovx_siso_set_property;
  gobject_class->get_property = gst_tiovx_siso_get_property;

  g_object_class_install_property (gobject_class, PROP_IN_POOL_SIZE,
      g_param_spec_uint ("in-pool-size", "Input Pool Size",
          "Number of buffers to allocate in input pool", MIN_POOL_SIZE,
          MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_OUT_POOL_SIZE,
      g_param_spec_uint ("out-pool-size", "Output Pool Size",
          "Number of buffers to allocate in output pool", MIN_POOL_SIZE,
          MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_siso_finalize);

  /* TODO: Verify passthrough on same caps */
  base_transform_class->passthrough_on_same_caps = TRUE;
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_tiovx_siso_stop);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_tiovx_siso_set_caps);
  base_transform_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_decide_allocation);
  base_transform_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_propose_allocation);
  base_transform_class->transform =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_transform);
}

static void
gst_tiovx_siso_init (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);

  priv->in_caps = NULL;
  priv->out_caps = NULL;
  priv->context = NULL;
  priv->graph = NULL;
  priv->node = NULL;
  priv->input = NULL;
  priv->output = NULL;
  priv->in_pool_size = DEFAULT_POOL_SIZE;
  priv->out_pool_size = DEFAULT_POOL_SIZE;
  priv->in_param_index = DEFAULT_PARAM_INDEX;
  priv->out_param_index = DEFAULT_PARAM_INDEX;
  priv->num_channels = DEFAULT_NUM_CHANNELS;

  priv->sink_buffer_pool = NULL;

  /* App common init */
  GST_DEBUG_OBJECT (self, "Running TIOVX common init");
  priv->tiovx_context = gst_tiovx_context_new ();

  /* Create OpenVX Context */
  GST_DEBUG_OBJECT (self, "Creating context");
  priv->context = vxCreateContext ();

  if (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context)) {
    tivxHwaLoadKernels (priv->context);
    tivxImgProcLoadKernels (priv->context);
    tivxEdgeaiImgProcLoadKernels (priv->context);
  }

  return;
}

static void
gst_tiovx_siso_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (object);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_IN_POOL_SIZE:
      priv->in_pool_size = g_value_get_uint (value);
      break;
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
gst_tiovx_siso_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (object);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_IN_POOL_SIZE:
      g_value_set_uint (value, priv->in_pool_size);
      break;
    case PROP_OUT_POOL_SIZE:
      g_value_set_uint (value, priv->out_pool_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_siso_stop (GstBaseTransform * trans)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);
  gint i = 0;
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "stop");

  if (NULL == priv->graph) {
    GST_WARNING_OBJECT (self,
        "Trying to deinit modules but initialization was not completed, ignoring...");
    ret = TRUE;
    goto exit;
  }

  for (i = 0; i < priv->num_channels; i++) {
    vx_reference ref = NULL;

    ref = vxGetObjectArrayItem (priv->input, i);
    if (VX_SUCCESS != gst_tiovx_empty_exemplar (ref)) {
      GST_WARNING_OBJECT (self, "Failed to empty input exemplar");
    }
    vxReleaseReference (&ref);

    ref = vxGetObjectArrayItem (priv->output, i);
    if (VX_SUCCESS != gst_tiovx_empty_exemplar (ref)) {
      GST_WARNING_OBJECT (self, "Failed to empty output exemplar");
    }
    vxReleaseReference (&ref);
  }

  ret = gst_tiovx_siso_modules_deinit (self);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to deinit TIOVX module"), (NULL));
  }

exit:
  return ret;
}

static void
gst_tiovx_siso_finalize (GObject * obj)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (obj);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);

  GST_LOG_OBJECT (self, "finalize");

  g_return_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) priv->context));

  if (NULL != priv->sink_buffer_pool) {
    gst_object_unref (priv->sink_buffer_pool);
  }

  if (NULL != priv->in_caps) {
    gst_caps_unref (priv->in_caps);
  }

  if (NULL != priv->out_caps) {
    gst_caps_unref (priv->out_caps);
  }

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
  }

  G_OBJECT_CLASS (gst_tiovx_siso_parent_class)->finalize (obj);
}

static gboolean
gst_tiovx_siso_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);
  GstTIOVXSisoClass *klass = GST_TIOVX_SISO_GET_CLASS (self);
  gint in_num_channels = 0, out_num_channels = 0;
  gboolean ret = TRUE;

  GST_LOG_OBJECT (self, "set_caps");

  if (NULL == klass->compare_caps) {
    GST_WARNING_OBJECT (self,
        "Subclass did not implement compare_caps method.");
  } else {
    if (priv->in_caps && priv->out_caps
        && klass->compare_caps (self, priv->in_caps, incaps, GST_PAD_SINK)
        && klass->compare_caps (self, priv->out_caps, outcaps, GST_PAD_SRC)
        && priv->graph) {
      GST_INFO_OBJECT (self,
          "Caps haven't changed and graph has already been initialized");
      /* We'll replace the caps either way in case there are changed not considered in the subclass */
      gst_caps_replace (&priv->in_caps, incaps);
      gst_caps_replace (&priv->out_caps, outcaps);
      goto exit;
    }
  }

  if (priv->graph) {
    GST_INFO_OBJECT (self,
        "Module already initialized, calling deinit before reinitialization");
    ret = gst_tiovx_siso_modules_deinit (self);
    if (!ret) {
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
          ("Unable to deinit TIOVX module"), (NULL));
      goto exit;
    }
  }

  if (gst_base_transform_is_passthrough (trans)) {
    GST_INFO_OBJECT (self,
        "Set as passthrough, ignoring additional configuration");
    return TRUE;
  }

  gst_caps_replace (&priv->in_caps, incaps);
  gst_caps_replace (&priv->out_caps, outcaps);

  if (!gst_structure_get_int (gst_caps_get_structure (incaps, 0),
          "num-channels", &in_num_channels)) {
    in_num_channels = DEFAULT_NUM_CHANNELS;
  }
  if (!gst_structure_get_int (gst_caps_get_structure (outcaps, 0),
          "num-channels", &out_num_channels)) {
    out_num_channels = DEFAULT_NUM_CHANNELS;
  }

  if (in_num_channels != out_num_channels) {
    GST_ERROR_OBJECT (self, "Different number of channels in input and output");
    ret = FALSE;
    goto exit;
  }

  priv->num_channels = in_num_channels;

  ret = gst_tiovx_siso_modules_init (self);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to init TIOVX module"), (NULL));
  }

exit:
  return ret;
}

static GstFlowReturn
gst_tiovx_siso_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);
  GstBuffer *original_buffer = NULL;
  vx_status status = VX_FAILURE;
  vx_object_array in_array = NULL;
  vx_object_array out_array = NULL;
  vx_size in_num_channels = 0;
  vx_size out_num_channels = 0;
  GstFlowReturn ret = GST_FLOW_ERROR;
  gint i = 0;

  original_buffer = inbuf;
  inbuf =
      gst_tiovx_validate_tiovx_buffer (GST_CAT_DEFAULT, &priv->sink_buffer_pool,
      inbuf, priv->input_ref, priv->in_caps, priv->in_pool_size,
      priv->num_channels);

  in_array =
      gst_tiovx_get_vx_array_from_buffer (GST_CAT_DEFAULT, priv->input_ref,
      inbuf);
  if (NULL == in_array) {
    GST_ERROR_OBJECT (self, "Input Buffer is not a TIOVX buffer");
    goto exit;
  }

  out_array =
      gst_tiovx_get_vx_array_from_buffer (GST_CAT_DEFAULT, priv->output_ref,
      outbuf);
  if (NULL == out_array) {
    GST_ERROR_OBJECT (self, "Output Buffer is not a TIOVX buffer");
    goto exit;
  }

  status =
      vxQueryObjectArray (in_array, VX_OBJECT_ARRAY_NUMITEMS, &in_num_channels,
      sizeof (vx_size));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Get number of channels in input buffer failed %" G_GINT32_FORMAT,
        status);
    goto exit;
  }

  status =
      vxQueryObjectArray (out_array, VX_OBJECT_ARRAY_NUMITEMS,
      &out_num_channels, sizeof (vx_size));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Get number of channels in output buffer failed %" G_GINT32_FORMAT,
        status);
    goto exit;
  }

  if ((in_num_channels != priv->num_channels)
      || (out_num_channels != priv->num_channels)) {
    GST_ERROR_OBJECT (self, "Incompatible number of channels received");
    goto exit;
  }

  /* Transfer handles */
  GST_LOG_OBJECT (self, "Transferring handles");
  for (i = 0; i < in_num_channels; i++) {
    vx_reference gst_in_ref = NULL;
    vx_reference modules_in_ref = NULL;
    vx_reference gst_out_ref = NULL;
    vx_reference modules_out_ref = NULL;

    gst_in_ref = vxGetObjectArrayItem (in_array, i);
    modules_in_ref = vxGetObjectArrayItem (priv->input, i);

    status =
        gst_tiovx_transfer_handle (GST_CAT_DEFAULT, gst_in_ref, modules_in_ref);
    vxReleaseReference (&gst_in_ref);
    vxReleaseReference (&modules_in_ref);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Error in input handle transfer %" G_GINT32_FORMAT, status);
      goto exit;
    }

    gst_out_ref = vxGetObjectArrayItem (out_array, i);
    modules_out_ref = vxGetObjectArrayItem (priv->output, i);
    status =
        gst_tiovx_transfer_handle (GST_CAT_DEFAULT, gst_out_ref,
        modules_out_ref);
    vxReleaseReference (&gst_out_ref);
    vxReleaseReference (&modules_out_ref);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Error in output handle transfer %" G_GINT32_FORMAT, status);
      goto exit;
    }
  }

  /* Graph processing */
  status = gst_tiovx_siso_process_graph (self);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph processing failed %" G_GINT32_FORMAT,
        status);
    goto exit;
  }

  ret = GST_FLOW_OK;

exit:
  if ((original_buffer != inbuf) && inbuf) {
    gst_buffer_unref (inbuf);
    inbuf = NULL;
  }

  return ret;
}

static gboolean
gst_tiovx_siso_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);
  GstBufferPool *pool = NULL;
  gboolean ret = TRUE;
  gint npool = 0;
  gboolean pool_needed = TRUE;

  GST_LOG_OBJECT (self, "Decide allocation");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (NULL == pool) {
      GST_DEBUG_OBJECT (self, "No pool in query position: %d, ignoring", npool);
      gst_query_remove_nth_allocation_pool (query, npool);
      continue;
    }

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
    /* Create our own pool if a TIOVX was not found.
       We use output vx_reference to decide a pool to use downstream. */
    gsize size = 0;

    size = gst_tiovx_get_size_from_exemplar (priv->output_ref);
    if (0 >= size) {
      GST_ERROR_OBJECT (self, "Failed to get size from exemplar");
      ret = FALSE;
      goto exit;
    }

    ret =
        gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, priv->out_pool_size,
        priv->output_ref, size, priv->num_channels, &pool);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to add new pool in decide allocation");
      return ret;
    }

    ret = gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), TRUE);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
      goto exit;
    }
    gst_object_unref (pool);
    pool = NULL;
  }

exit:
  return ret;
}

static gboolean
gst_tiovx_siso_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);
  GstBufferPool *pool = NULL;
  gsize size = 0;
  gboolean ret = TRUE;

  GST_LOG_OBJECT (self, "Propose allocation");

  if (gst_base_transform_is_passthrough (trans)) {
    GST_INFO_OBJECT (self, "Set as passthrough, doing passthrough query");

    ret =
        GST_BASE_TRANSFORM_CLASS
        (gst_tiovx_siso_parent_class)->propose_allocation (trans, decide_query,
        query);
    goto exit;
  }
  /* We use input vx_reference to propose a pool upstream */
  size = gst_tiovx_get_size_from_exemplar (priv->input_ref);
  if (0 >= size) {
    GST_ERROR_OBJECT (self, "Failed to get size from input");
    ret = FALSE;
    goto exit;
  }

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, priv->in_pool_size,
      priv->input_ref, size, priv->num_channels, &pool);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to add new pool in propose allocation");
    return ret;
  }
  /* Unref the old stored pool */
  if (NULL != priv->sink_buffer_pool) {
    gst_object_unref (priv->sink_buffer_pool);
  }
  /* Assign the new pool to the internal value */
  priv->sink_buffer_pool = GST_BUFFER_POOL (pool);

exit:
  return ret;
}

/* Private functions */

static gboolean
gst_tiovx_siso_is_subclass_complete (GstTIOVXSiso * self)
{
  GstTIOVXSisoClass *klass = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);

  klass = GST_TIOVX_SISO_GET_CLASS (self);

  if (NULL == klass->init_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement init_module method.");
    goto exit;
  }

  if (NULL == klass->create_graph) {
    GST_ERROR_OBJECT (self, "Subclass did not implement create_graph method.");
    goto exit;
  }

  if (NULL == klass->get_node_info) {
    GST_ERROR_OBJECT (self, "Subclass did not implement get_node_info method");
    goto exit;
  }

  if (NULL == klass->release_buffer) {
    GST_ERROR_OBJECT (self,
        "Subclass did not implement release_buffer method.");
    goto exit;
  }

  if (NULL == klass->deinit_module) {
    GST_ERROR_OBJECT (self, "Subclass did not implement deinit_module method.");
    goto exit;
  }

  ret = TRUE;

exit:
  return ret;
}

static gboolean
gst_tiovx_siso_modules_init (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = NULL;
  GstTIOVXSisoClass *klass = NULL;
  vx_graph_parameter_queue_params_t params_list[NUM_PARAMETERS];
  gboolean ret = FALSE;
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (self, FALSE);

  priv = gst_tiovx_siso_get_instance_private (self);
  klass = GST_TIOVX_SISO_GET_CLASS (self);

  if (!gst_tiovx_siso_is_subclass_complete (self)) {
    GST_ERROR_OBJECT (self, "Subclass implementation is incomplete");
    goto exit;
  }

  status = vxGetStatus ((vx_reference) priv->context);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Context creation failed with error: %" G_GINT32_FORMAT, status);
    goto exit;
  }

  /* Init subclass module */
  GST_DEBUG_OBJECT (self, "Calling init module");
  ret =
      klass->init_module (self, priv->context, priv->in_caps, priv->out_caps,
      priv->num_channels);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass init module failed");
    goto exit;
  }

  /* Create OpenVX Graph */
  GST_DEBUG_OBJECT (self, "Creating graph");
  priv->graph = vxCreateGraph (priv->context);
  status = vxGetStatus ((vx_reference) priv->graph);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph creation failed %" G_GINT32_FORMAT, status);
    goto deinit_module;
  }

  ret = klass->create_graph (self, priv->context, priv->graph);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass create graph failed");
    goto free_graph;
  }

  /* Set Graph parameters */
  GST_DEBUG_OBJECT (self, "Getting subclass node and exemplars");
  ret =
      klass->get_node_info (self, &priv->input, &priv->output, &priv->input_ref,
      &priv->output_ref, &priv->node, &priv->in_param_index,
      &priv->out_param_index);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass get node info failed");
    goto free_graph;
  }

  if (NULL == priv->input) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: input missing");
    goto free_graph;
  }
  if (NULL == priv->output) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: output missing");
    goto free_graph;
  }
  if (NULL == priv->node) {
    GST_ERROR_OBJECT (self, "Incomplete info from subclass: node missing");
    goto free_graph;
  }

  if (priv->in_param_index == priv->out_param_index) {
    GST_ERROR_OBJECT (self,
        "Input and output param index from subclass can't be equal");
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Setting up input parameter");
  status =
      add_graph_parameter_by_node_index (gst_tiovx_siso_debug_category,
      G_OBJECT (self), priv->graph, priv->node, INPUT_PARAMETER_INDEX,
      priv->in_param_index, params_list, &priv->input_ref);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input parameter failed %" G_GINT32_FORMAT, status);
    goto free_graph;
  }

  GST_DEBUG_OBJECT (self, "Setting up output parameter");
  status =
      add_graph_parameter_by_node_index (gst_tiovx_siso_debug_category,
      G_OBJECT (self), priv->graph, priv->node, OUTPUT_PARAMETER_INDEX,
      priv->out_param_index, params_list, &priv->output_ref);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output parameter failed %" G_GINT32_FORMAT,
        status);
    goto free_graph;
  }

  status = vxSetGraphScheduleConfig (priv->graph,
      VX_GRAPH_SCHEDULE_MODE_QUEUE_MANUAL, NUM_PARAMETERS, params_list);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Graph schedule configuration failed %" G_GINT32_FORMAT, status);
    goto free_graph;
  }

  /* Verify Graph */
  GST_DEBUG_OBJECT (self, "Verifying graph");
  status = vxVerifyGraph (priv->graph);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Graph verification failed %" G_GINT32_FORMAT,
        status);
    goto free_graph;
  }

  /* Release buffer. This is needed in order to free resources allocated by vxVerifyGraph function */
  ret = klass->release_buffer (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass release buffer failed");
    goto free_graph;
  }

  ret = TRUE;
  goto exit;

  /* Free resources in case of failure only. Otherwise they will be released at other moment */
free_graph:
  vxReleaseGraph (&priv->graph);
deinit_module:
  if (!gst_tiovx_siso_modules_deinit (self)) {
    GST_ERROR_OBJECT (self, "Modules deinit failed");
  }
  /* If we get to free something, it's because something failed */
  ret = FALSE;
exit:
  return ret;
}

static gboolean
gst_tiovx_siso_modules_deinit (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = NULL;
  GstTIOVXSisoClass *klass = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);

  priv = gst_tiovx_siso_get_instance_private (self);
  klass = GST_TIOVX_SISO_GET_CLASS (self);

  /* Deinit subclass module */
  GST_DEBUG_OBJECT (self, "Calling deinit module");
  ret = klass->deinit_module (self, priv->context);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass deinit module failed");
  }

  GST_DEBUG_OBJECT (self, "Release graph and context");
  vxReleaseGraph (&priv->graph);

  return ret;
}

static vx_status
gst_tiovx_siso_process_graph (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = NULL;
  vx_status status = VX_FAILURE;
  uint32_t in_refs = 0;
  uint32_t out_refs = 0;
  vx_reference dequeued_input = NULL, dequeued_output = NULL;

  g_return_val_if_fail (self, VX_FAILURE);

  priv = gst_tiovx_siso_get_instance_private (self);

  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) priv->graph), VX_FAILURE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) priv->input), VX_FAILURE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) priv->output), VX_FAILURE);

  /* Enqueueing parameters */
  GST_LOG_OBJECT (self, "Enqueueing parameters");

  status =
      vxGraphParameterEnqueueReadyRef (priv->graph, INPUT_PARAMETER_INDEX,
      (vx_reference *) & priv->input_ref, 1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input enqueue failed %" G_GINT32_FORMAT, status);
    return VX_FAILURE;
  }
  GST_LOG_OBJECT (self,
      "Enqueued input: %p\t with graph id: %d\tnum channels: %d", priv->input,
      INPUT_PARAMETER_INDEX, priv->num_channels);

  status =
      vxGraphParameterEnqueueReadyRef (priv->graph, OUTPUT_PARAMETER_INDEX,
      (vx_reference *) & priv->output_ref, 1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output enqueue failed %" G_GINT32_FORMAT, status);
    return VX_FAILURE;
  }
  GST_LOG_OBJECT (self,
      "Enqueued output: %p\t with graph id: %d\tnum channels: %d", priv->output,
      OUTPUT_PARAMETER_INDEX, priv->num_channels);

  /* Processing graph */
  GST_LOG_OBJECT (self, "Processing graph");
  status = vxScheduleGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Schedule graph failed %" G_GINT32_FORMAT, status);
    return VX_FAILURE;
  }
  status = vxWaitGraph (priv->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Wait graph failed %" G_GINT32_FORMAT, status);
    return VX_FAILURE;
  }

  /* Dequeueing parameters */
  GST_LOG_OBJECT (self, "Dequeueing parameters");
  status =
      vxGraphParameterDequeueDoneRef (priv->graph, INPUT_PARAMETER_INDEX,
      (vx_reference *) & dequeued_input, 1, &in_refs);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Input dequeue failed %" G_GINT32_FORMAT, status);
    return VX_FAILURE;
  }


  status =
      vxGraphParameterDequeueDoneRef (priv->graph, OUTPUT_PARAMETER_INDEX,
      (vx_reference *) & dequeued_output, 1, &out_refs);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Output dequeue failed %" G_GINT32_FORMAT, status);
    return VX_FAILURE;
  }

  return VX_SUCCESS;
}
