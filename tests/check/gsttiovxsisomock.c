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

#include "gsttiovxsisomock.h"

enum
{
  PROP_0,
  PROP_CREATE_NODE_FAIL,
  PROP_CONFIGURE_NODE_FAIL,
};

struct _GstTIOVXSisoMock
{
  GstTIOVXSiso parent;

  GstPad *sinkpad;
  GstPad *srcpad;

  GstBufferPool *bufferPool;

  /* Properties */
  gboolean create_node_fail;
  gboolean configure_node_fail;

  tivx_vpac_msc_coefficients_t coeffs;

  vx_object_array input_arr[1];
  vx_object_array output_arr[1];
  vx_image input_img[1];
  vx_image output_img[1];
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_siso_mock_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXSisoMock, gst_tiovx_siso_mock,
    GST_TIOVX_SISO_TYPE,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_siso_mock_debug_category,
        "tiovxsisomock", 0, "debug category for tiovxsisomock element"));

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static void
gst_tiovx_siso_mock_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);

static void
gst_tiovx_siso_mock_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tiovx_siso_mock_get_exemplar_refs (GstTIOVXSiso *
    trans, GstVideoInfo * in_caps_info, GstVideoInfo * out_caps_info,
    vx_context context, vx_reference input, vx_reference output);

static gboolean gst_tiovx_siso_mock_create_node (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph, vx_node node, vx_reference input,
    vx_reference output);

static gboolean
gst_tiovx_siso_mock_configure_node (GstTIOVXSiso * trans, vx_context context,
    vx_node node);

static void
gst_tiovx_siso_mock_class_init (GstTIOVXSisoMockClass * klass)
{
  GstTIOVXSisoClass *tiovx_siso_class = GST_TIOVX_SISO_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_tiovx_siso_mock_set_property;
  gobject_class->get_property = gst_tiovx_siso_mock_get_property;

  g_object_class_install_property (gobject_class, PROP_CREATE_NODE_FAIL,
      g_param_spec_boolean ("create_node_fail", "Create Node Fail",
          "Testing flag to make the create node method fail", FALSE,
          (GParamFlags) G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_CONFIGURE_NODE_FAIL,
      g_param_spec_boolean ("configure_node_fail", "Configure Node Fail",
          "Testing flag to make the configure node method fail", FALSE,
          (GParamFlags) G_PARAM_READWRITE));

  gst_element_class_add_pad_template ((GstElementClass *) klass,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_add_pad_template ((GstElementClass *) klass,
      gst_static_pad_template_get (&src_factory));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "TIOVX SISO MOCK", "Filter/Video",
      "TIOVX SISO MOCK element for testing purposes.",
      "Jafet Chaves <jafet.chaves@ridgerun.com>");

  tiovx_siso_class->get_exemplar_refs =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_mock_get_exemplar_refs);
  tiovx_siso_class->create_node =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_mock_create_node);
  tiovx_siso_class->configure_node =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_mock_configure_node);
}

static void
gst_tiovx_siso_mock_init (GstTIOVXSisoMock * self)
{

}

static void
gst_tiovx_siso_mock_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXSisoMock *self = GST_TIOVX_SISO_MOCK (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_CREATE_NODE_FAIL:
      self->create_node_fail = g_value_get_boolean (value);
      break;
    case PROP_CONFIGURE_NODE_FAIL:
      self->configure_node_fail = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_siso_mock_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXSisoMock *self = GST_TIOVX_SISO_MOCK (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_CREATE_NODE_FAIL:
      g_value_set_boolean (value, self->create_node_fail);
      break;
    case PROP_CONFIGURE_NODE_FAIL:
      g_value_set_boolean (value, self->configure_node_fail);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}


static gboolean
gst_tiovx_siso_mock_get_exemplar_refs (GstTIOVXSiso * trans,
    GstVideoInfo * in_caps_info, GstVideoInfo * out_caps_info,
    vx_context context, vx_reference input, vx_reference output)
{
  GstTIOVXSisoMock *self = NULL;

  self = GST_TIOVX_SISO_MOCK (trans);

  GST_DEBUG_OBJECT (self, "gst_tiovx_siso_mock_get_exemplar_refs");

  return TRUE;
}

static gboolean
gst_tiovx_siso_mock_create_node (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph, vx_node node, vx_reference input, vx_reference output)
{
  GstTIOVXSisoMock *self = NULL;
  vx_status status;
  vx_image input_vx_image;
  vx_image output_vx_image;

  self = GST_TIOVX_SISO_MOCK (trans);

  GST_DEBUG_OBJECT (self, "gst_tiovx_siso_mock_create_node");

  if (self->create_node_fail) {
    GST_DEBUG_OBJECT (self, "gst_tiovx_siso_mock_create_node set to fail");
    return FALSE;
  }
  /* TODO: Use the caps, don't hardcode it */
  input_vx_image = vxCreateImage (context, 640, 480, VX_DF_IMAGE_NV12);
  status = vxGetStatus ((vx_reference) input_vx_image);
  if (status != VX_SUCCESS) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to create input VX image"), (NULL));
    return FALSE;
  }
  output_vx_image = vxCreateImage (context, 640, 480, VX_DF_IMAGE_NV12);
  status = vxGetStatus ((vx_reference) output_vx_image);
  if (status != VX_SUCCESS) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to create output VX image"), (NULL));
    return FALSE;
  }

  self->input_arr[0] =
      vxCreateObjectArray (context, (vx_reference) input_vx_image, 1);
  self->input_img[0] =
      (vx_image) vxGetObjectArrayItem ((vx_object_array) self->input_arr[0], 0);
  self->output_arr[0] =
      vxCreateObjectArray (context, (vx_reference) output_vx_image, 1);
  self->output_img[0] =
      (vx_image) vxGetObjectArrayItem ((vx_object_array) self->output_arr[0],
      0);

  node =
      tivxVpacMscScaleNode (graph, (vx_image) self->input_img[0],
      (vx_image) self->output_img[0], NULL, NULL, NULL, NULL);
  status = vxGetStatus ((vx_reference) node);
  if (status != VX_SUCCESS) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to create multiscaler VX node"), (NULL));
    return FALSE;
  }

  vxSetNodeTarget (node, VX_TARGET_STRING, TIVX_TARGET_VPAC_MSC1);
  vxSetReferenceName ((vx_reference) node, "ScalerNode");

  vxReleaseImage (&input_vx_image);
  vxReleaseImage (&output_vx_image);

  return TRUE;
}

static gboolean
gst_tiovx_siso_mock_configure_node (GstTIOVXSiso * trans, vx_context context,
    vx_node node)
{
  GstTIOVXSisoMock *self = NULL;

  self = GST_TIOVX_SISO_MOCK (trans);

  if (self->configure_node_fail) {
    GST_DEBUG_OBJECT (self, "gst_tiovx_siso_mock_configure_node set to fail");
    return FALSE;
  }

  return TRUE;
}
