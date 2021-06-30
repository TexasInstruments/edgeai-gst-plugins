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
 * *	No reverse engineering, decompilation, or disassembly of this software 
 *      is permitted with respect to any software provided in binary form.
 * 
 * *	Any redistribution and use are licensed by TI for use only with TI Devices.
 * 
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 * 
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
 * 
 * *	Any redistribution and use of the source code, including any resulting 
 *      derivative works, are licensed by TI for use only with TI Devices.
 * 
 * *	Any redistribution and use of any object code compiled from the source
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
#include "config.h"
#endif

#include "gsttiovxvideoconvert.h"

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>
#include <VX/vx.h>
#include <VX/vx_nodes.h>

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_video_convert_debug);
#define GST_CAT_DEFAULT gst_tiovx_video_convert_debug

#define TIOVX_VIDEO_CONVERT_SUPPORTED_FORMATS_SRC "{RGB, RGBx, NV12, NV21, UYVY, YUYV, IYUV}"
#define TIOVX_VIDEO_CONVERT_SUPPORTED_FORMATS_SINK "{RGB, RGBx, NV12, NV21, UYVY, YUYV, IYUV, YUV4}"

/* Src caps */
#define TIOVX_VIDEO_CONVERT_STATIC_CAPS_SRC GST_VIDEO_CAPS_MAKE (TIOVX_VIDEO_CONVERT_SUPPORTED_FORMATS_SRC)

/* Sink caps */
#define TIOVX_VIDEO_CONVERT_STATIC_CAPS_SINK GST_VIDEO_CAPS_MAKE (TIOVX_VIDEO_CONVERT_SUPPORTED_FORMATS_SINK)

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_VIDEO_CONVERT_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_VIDEO_CONVERT_STATIC_CAPS_SRC)
    );

#define gst_tiovx_video_convert_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXVideoConvert, gst_tiovx_video_convert,
    GST_TYPE_BASE_TRANSFORM);

static void
gst_tiovx_video_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_video_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tiovx_video_convert_create_node (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph, vx_node node, vx_reference input,
    vx_reference output);
static gboolean gst_tiovx_video_convert_get_exemplar_refs (GstTIOVXSiso * trans,
    GstVideoInfo * in_caps_info, GstVideoInfo * out_caps_info,
    vx_context context, vx_reference input, vx_reference output);
static GstCaps *gst_tiovx_video_convert_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

/* Initialize the plugin's class */
static void
gst_tiovx_video_convert_class_init (GstTIOVXVideoConvertClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstTIOVXSisoClass *gst_tiovx_siso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_tiovx_siso_class = GST_TIOVX_SISO_GET_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "Video Convert",
      "Generic/Filter",
      "Converts video from one colorspace to another using the OVX API",
      "Daniel Chaves daniel.chaves@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_tiovx_siso_class->create_node =
      GST_DEBUG_FUNCPTR (gst_tiovx_video_convert_create_node);
  gst_tiovx_siso_class->get_exemplar_refs =
      GST_DEBUG_FUNCPTR (gst_tiovx_video_convert_get_exemplar_refs);

  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_video_convert_transform_caps);

  gobject_class->set_property = gst_tiovx_video_convert_set_property;
  gobject_class->get_property = gst_tiovx_video_convert_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_video_convert_debug,
      "gsttiovxvideoconvert", 0,
      "debug category for the gsttiovxvideoconvert element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_video_convert_init (GstTIOVXVideoConvert * filter)
{
  filter->silent = FALSE;
}

static void
gst_tiovx_video_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXVideoConvert *filter = GST_TIOVX_VIDEO_CONVERT (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tiovx_video_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXVideoConvert *filter = GST_TIOVX_VIDEO_CONVERT (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_tiovx_video_convert_create_node (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph, vx_node node, vx_reference input, vx_reference output)
{
  GstTIOVXVideoConvert *self = NULL;
  vx_node _node = NULL;
  gboolean ret = TRUE;
  vx_status status = VX_SUCCESS;

  self = GST_TIOVX_VIDEO_CONVERT (trans);

  _node = vxColorConvertNode (graph, (vx_image) input, (vx_image) output);
  if (!_node) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Error, status = %d. ", status),
        ("Unable to perform format conversion."));
    ret = FALSE;
    goto out;
  }

  node = _node;

out:
  return ret;
}

static gboolean
gst_tiovx_video_convert_get_exemplar_refs (GstTIOVXSiso * trans,
    GstVideoInfo * in_caps_info, GstVideoInfo * out_caps_info,
    vx_context context, vx_reference input, vx_reference output)
{
  GstTIOVXVideoConvert *self = NULL;
  vx_image input_vx_image = NULL;
  vx_image output_vx_image = NULL;
  vx_status status = VX_SUCCESS;

  self = GST_TIOVX_VIDEO_CONVERT (trans);

  /* Input image */
  input_vx_image =
      vxCreateImage (context, in_caps_info->width, in_caps_info->height,
      in_caps_info->finfo->format);
  status = vxGetStatus ((vx_reference) input_vx_image);
  if (VX_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to create input VX image"), (NULL));
    return FALSE;
  }

  /* Output image */
  output_vx_image =
      vxCreateImage (context, in_caps_info->width, in_caps_info->height,
      in_caps_info->finfo->format);
  status = vxGetStatus ((vx_reference) output_vx_image);
  if (VX_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to create output VX image"), (NULL));
    return FALSE;
  }

  return status;
}

static GstCaps *
gst_tiovx_video_convert_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *clone_caps = NULL;
  gint i = 0;

  clone_caps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (clone_caps); i++) {
    GstStructure *st = gst_caps_get_structure (clone_caps, i);

    gst_structure_remove_field (st, "format");
  }

  clone_caps =
      GST_BASE_TRANSFORM_CLASS
      (gst_tiovx_video_convert_parent_class)->transform_caps (base, direction,
      clone_caps, filter);

  return clone_caps;
}
