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

#include <gst/video/video.h>

#include "gsttiovxsiso.h"

#include <app_init.h>
#include <TI/j7.h>

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_siso_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_siso_debug_category

typedef struct _GstTIOVXSisoPrivate
{
  GstVideoInfo in_caps_info;
  GstVideoInfo out_caps_info;
  vx_context context;
  vx_graph graph;
  vx_node node;
  vx_reference input;
  vx_reference output;
  gboolean vx_node_created;
} GstTIOVXSisoPrivate;

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXSiso, gst_tiovx_siso,
    GST_TYPE_BASE_TRANSFORM, G_ADD_PRIVATE (GstTIOVXSiso)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_siso_debug_category, "tiovxsiso", 0,
        "debug category for tiovxsiso base class"));

static gboolean gst_tiovx_siso_start (GstBaseTransform * trans);
static gboolean gst_tiovx_siso_stop (GstBaseTransform * trans);
static gboolean gst_tiovx_siso_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_tiovx_siso_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);

static void
gst_tiovx_siso_class_init (GstTIOVXSisoClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_tiovx_siso_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_tiovx_siso_stop);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_tiovx_siso_set_caps);
  base_transform_class->transform =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_transform);
}

static void
gst_tiovx_siso_init (GstTIOVXSiso * self)
{
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);

  priv->vx_node_created = FALSE;
}

static gboolean
gst_tiovx_siso_start (GstBaseTransform * trans)
{
  vx_status status;

  GstTIOVXSiso *self = GST_TIOVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "start");

  tivxInit ();
  tivxHostInit ();

  priv->context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) priv->context);
  if (status != VX_SUCCESS) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create OpenVX context."), (NULL));
    return FALSE;
  }

  tivxHwaLoadKernels (priv->context);

  priv->graph = vxCreateGraph (priv->context);
  status = vxGetStatus ((vx_reference) priv->graph);
  if (status != VX_SUCCESS) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create OpenVX graph."), (NULL));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_siso_stop (GstBaseTransform * trans)
{
  GstTIOVXSiso *self = GST_TIOVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "stop");

  /* Release resources */
  vxReleaseNode (&priv->node);
  vxReleaseGraph (&priv->graph);
  tivxHwaUnLoadKernels (priv->context);
  vxReleaseContext (&priv->context);
  tivxHostDeInit ();
  tivxDeInit ();
  appCommonDeInit ();

  priv->vx_node_created = FALSE;

  return TRUE;
}

static gboolean
gst_tiovx_siso_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstTIOVXSiso *self;
  GstTIOVXSisoClass *gst_tiovx_siso_class;
  GstTIOVXSisoPrivate *priv;
  gboolean ret;

  self = GST_TIOVX_SISO (trans);
  gst_tiovx_siso_class = GST_TIOVX_SISO_GET_CLASS (self);
  priv = gst_tiovx_siso_get_instance_private (self);

  GST_LOG_OBJECT (self, "set_caps");

  ret = gst_video_info_from_caps (&priv->in_caps_info, incaps);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to get the input caps"), (NULL));
    return FALSE;
  }

  ret = gst_video_info_from_caps (&priv->out_caps_info, outcaps);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to get the output caps"), (NULL));
    return FALSE;
  }

  if (!gst_tiovx_siso_class->get_exemplar_refs) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Subclass did not implement get_exemplar_refs method."), (NULL));
    return FALSE;
  }
  /* Create input and output exemplars based on input/output resolution */
  ret =
      gst_tiovx_siso_class->get_exemplar_refs (self, &priv->in_caps_info,
      &priv->out_caps_info, priv->context, priv->input, priv->output);
  if (!ret) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to get the exemplar references from subclass"), (NULL));
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_tiovx_siso_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstTIOVXSiso *self;
  GstTIOVXSisoClass *gst_tiovx_siso_class;
  GstTIOVXSisoPrivate *priv;
  vx_status status;
  gboolean ret = GST_FLOW_OK;

  self = GST_TIOVX_SISO (trans);
  gst_tiovx_siso_class = GST_TIOVX_SISO_GET_CLASS (self);
  priv = gst_tiovx_siso_get_instance_private (self);

  GST_LOG_OBJECT (self, "transform");

  if (!gst_tiovx_siso_class->create_node) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Subclass did not implement create_node method."), (NULL));
    return GST_FLOW_ERROR;
  }
  /* The VX node has to be created just once */
  if (!priv->vx_node_created) {
    ret =
        gst_tiovx_siso_class->create_node (self, priv->context, priv->graph,
        priv->node, priv->input, priv->output);
    if (!ret) {
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
          ("Failure when creating VX node in subclass."), (NULL));
      return GST_FLOW_ERROR;
    }
    status = vxVerifyGraph (priv->graph);
    if (status != VX_SUCCESS) {
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
          ("Failure when verifying the VX graph."), (NULL));
      return GST_FLOW_ERROR;
    }
    /* Configuring the node is optional */
    if (!gst_tiovx_siso_class->configure_node) {
      ret =
          gst_tiovx_siso_class->configure_node (self, priv->context,
          priv->node);
      if (!ret) {
        GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
            ("Failure when configuring VX node in subclass."), (NULL));
        return GST_FLOW_ERROR;
      }
    }
    priv->vx_node_created = TRUE;
  }
  status = vxProcessGraph (priv->graph);
  if (status != VX_SUCCESS) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Failure when processing VX graph."), (NULL));
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}
