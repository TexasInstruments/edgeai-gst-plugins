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
#include <VX/vx_types.h>

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"

#include "app_color_convert_module.h"

#define MIN_POOL_SIZE 2
/* TODO: Implement TARGET property */
#define DEFAULT_TARGET TIVX_TARGET_DSP1
/* TODO: Implement method to choose number of channels dynamically */
#define DEFAULT_NUM_CH 1

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_color_convert_debug);
#define GST_CAT_DEFAULT gst_tiovx_color_convert_debug

#define TIOVX_COLOR_CONVERT_SUPPORTED_FORMATS_SRC "{RGB, RGBx, NV12, I420, Y444}"
#define TIOVX_COLOR_CONVERT_SUPPORTED_FORMATS_SINK "{RGB, RGBx, NV12, NV21, UYVY, YUY2, I420}"

/* Src caps */
#define TIOVX_COLOR_CONVERT_STATIC_CAPS_SRC GST_VIDEO_CAPS_MAKE (TIOVX_COLOR_CONVERT_SUPPORTED_FORMATS_SRC)

/* Sink caps */
#define TIOVX_COLOR_CONVERT_STATIC_CAPS_SINK GST_VIDEO_CAPS_MAKE (TIOVX_COLOR_CONVERT_SUPPORTED_FORMATS_SINK)

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_COLOR_CONVERT_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_COLOR_CONVERT_STATIC_CAPS_SRC)
    );

struct _GstTIOVXVideoConvert
{
  GstTIOVXSiso element;
  ColorConvertObj *obj;
};

#define gst_tiovx_color_convert_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXVideoConvert, gst_tiovx_color_convert,
    GST_TIOVX_SISO_TYPE);

static void
gst_tiovx_color_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_color_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_tiovx_color_convert_dispose (GObject * obj);

static gboolean gst_tiovx_color_convert_init_module (GstTIOVXSiso * trans,
    vx_context context, GstVideoInfo * in_info, GstVideoInfo * out_info,
    guint in_pool_size, guint out_pool_size);
static gboolean gst_tiovx_color_convert_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_color_convert_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node);
static gboolean gst_tiovx_color_convert_release_buffer (GstTIOVXSiso * trans);
static gboolean gst_tiovx_color_convert_deinit_module (GstTIOVXSiso * trans);

static gboolean gst_tiovx_color_convert_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);
static GstCaps *gst_tiovx_color_convert_transform_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * filter);

static enum vx_df_image_e
map_gst_video_format_to_vx_format (GstVideoFormat gst_format);

static enum vx_df_image_e
map_gst_video_format_to_vx_format (GstVideoFormat gst_format)
{
  enum vx_df_image_e vx_format = VX_DF_IMAGE_VIRT;

  switch (gst_format) {
    case GST_VIDEO_FORMAT_RGB:
      vx_format = VX_DF_IMAGE_RGB;
      break;
    case GST_VIDEO_FORMAT_RGBx:
      vx_format = VX_DF_IMAGE_RGBX;
      break;
    case GST_VIDEO_FORMAT_NV12:
      vx_format = VX_DF_IMAGE_NV12;
      break;
    case GST_VIDEO_FORMAT_NV21:
      vx_format = VX_DF_IMAGE_NV21;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      vx_format = VX_DF_IMAGE_UYVY;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      vx_format = VX_DF_IMAGE_YUYV;
      break;
    case GST_VIDEO_FORMAT_I420:
      vx_format = VX_DF_IMAGE_IYUV;
      break;
    case GST_VIDEO_FORMAT_Y444:
      vx_format = VX_DF_IMAGE_YUV4;
      break;
    default:
      vx_format = -1;
      break;
  }

  return vx_format;
}

/* Initialize the plugin's class */
static void
gst_tiovx_color_convert_class_init (GstTIOVXVideoConvertClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *trans_class = NULL;
  GstElementClass *gstelement_class;
  GstTIOVXSisoClass *gst_tiovx_siso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_tiovx_siso_class = GST_TIOVX_SISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "Video Convert",
      "Generic/Filter",
      "Converts video from one colorspace to another using the TIOVX Modules API",
      "RidgeRun support@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_tiovx_siso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_color_convert_init_module);
  gst_tiovx_siso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_color_convert_create_graph);
  gst_tiovx_siso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_color_convert_get_node_info);
  gst_tiovx_siso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_color_convert_release_buffer);
  gst_tiovx_siso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_color_convert_deinit_module);

  GST_BASE_TRANSFORM_CLASS (klass)->set_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_color_convert_set_caps);
  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_color_convert_transform_caps);

  gobject_class->set_property = gst_tiovx_color_convert_set_property;
  gobject_class->get_property = gst_tiovx_color_convert_get_property;
  gobject_class->dispose = gst_tiovx_color_convert_dispose;

  /* Disable processing if input & output caps are equal, i.e., no format convertion */
  trans_class->passthrough_on_same_caps = TRUE;
  trans_class->transform_ip_on_passthrough = FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_color_convert_debug,
      "gsttiovxvideoconvert", 0,
      "debug category for the gsttiovxvideoconvert element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_color_convert_init (GstTIOVXVideoConvert * filter)
{
  filter->obj = g_malloc (sizeof (ColorConvertObj));
}

static void
gst_tiovx_color_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXVideoConvert *filter = GST_TIOVX_COLOR_CONVERT (object);

  GST_LOG_OBJECT (filter, "set_property");

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tiovx_color_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXVideoConvert *filter = GST_TIOVX_COLOR_CONVERT (object);

  GST_LOG_OBJECT (filter, "get_property");

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_tiovx_color_convert_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstTIOVXVideoConvert *self = GST_TIOVX_COLOR_CONVERT (base);
  gboolean ret = TRUE;
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  GstVideoFormat in_format;
  GstVideoFormat out_format;

  GST_INFO_OBJECT (self, "Setting caps:\n   Input caps: %"
      GST_PTR_FORMAT "\n   Output caps: %" GST_PTR_FORMAT, incaps, outcaps);

  ret &= gst_video_info_from_caps (&in_info, incaps);
  ret &= gst_video_info_from_caps (&out_info, outcaps);

  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to get video info from caps");
    goto exit;
  }

  in_format = in_info.finfo->format;
  out_format = out_info.finfo->format;

  switch (in_format) {
    case GST_VIDEO_FORMAT_RGB:
      /* No restrictions */
      break;
    case GST_VIDEO_FORMAT_RGBx:
      /* No restrictions */
      break;
    case GST_VIDEO_FORMAT_NV12:
      /* No restrictions */
      break;
    case GST_VIDEO_FORMAT_NV21:
      if ((GST_VIDEO_FORMAT_NV12 == out_format)) {
        ret = FALSE;
      }
      break;
    case GST_VIDEO_FORMAT_UYVY:
      if ((GST_VIDEO_FORMAT_Y444 == out_format)) {
        ret = FALSE;
      }
      break;
    case GST_VIDEO_FORMAT_YUY2:
      if ((GST_VIDEO_FORMAT_Y444 == out_format)) {
        ret = FALSE;
      }
      break;
    case GST_VIDEO_FORMAT_I420:
      /* No restrictions */
      break;
    default:
      ret = FALSE;
  }

  if (!ret) {
    GST_ERROR_OBJECT (self,
        "Incompatible formats: %s to %s conversion is not supported",
        in_info.finfo->name, out_info.finfo->name);
    goto exit;
  } else {
    GST_INFO_OBJECT (self, " %s to %s conversion is supported",
        in_info.finfo->name, out_info.finfo->name);
  }

  ret =
      GST_BASE_TRANSFORM_CLASS
      (gst_tiovx_color_convert_parent_class)->set_caps (base, incaps, outcaps);

exit:
  return ret;
}

static GstCaps *
gst_tiovx_color_convert_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIOVXVideoConvert *self = GST_TIOVX_COLOR_CONVERT (base);
  GstCaps *clone_caps = NULL;
  gint i = 0;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  clone_caps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (clone_caps); i++) {
    GstStructure *st = gst_caps_get_structure (clone_caps, i);

    gst_structure_remove_field (st, "format");
  }

  clone_caps =
      GST_BASE_TRANSFORM_CLASS
      (gst_tiovx_color_convert_parent_class)->transform_caps (base, direction,
      clone_caps, filter);

  return clone_caps;
}

static void
gst_tiovx_color_convert_dispose (GObject * obj)
{
  GstTIOVXVideoConvert *self = GST_TIOVX_COLOR_CONVERT (obj);

  if (self->obj) {
    g_free (self->obj);
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

/* TIOVXSISO Functions */

static gboolean
gst_tiovx_color_convert_init_module (GstTIOVXSiso * trans, vx_context context,
    GstVideoInfo * in_info, GstVideoInfo * out_info, guint in_pool_size,
    guint out_pool_size)
{
  GstTIOVXVideoConvert *self = GST_TIOVX_COLOR_CONVERT (trans);
  vx_status status = VX_SUCCESS;
  ColorConvertObj *colorconvert;

  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);
  g_return_val_if_fail (in_pool_size >= MIN_POOL_SIZE, FALSE);
  g_return_val_if_fail (out_pool_size >= MIN_POOL_SIZE, FALSE);

  GST_INFO_OBJECT (self, "Init module");

  if (GST_VIDEO_INFO_WIDTH (out_info) != GST_VIDEO_INFO_WIDTH (out_info)) {
    GST_ERROR_OBJECT (self, "Width mismatch between input and ouput");
    return FALSE;
  }

  if (GST_VIDEO_INFO_HEIGHT (out_info) != GST_VIDEO_INFO_HEIGHT (out_info)) {
    GST_ERROR_OBJECT (self, "Height mismatch between input and ouput");
    return FALSE;
  }

  /* Configure ColorConvertObj */
  colorconvert = self->obj;
  colorconvert->num_ch = DEFAULT_NUM_CH;
  colorconvert->input.bufq_depth = in_pool_size;
  colorconvert->output.bufq_depth = out_pool_size;

  colorconvert->input.graph_parameter_index = INPUT_PARAMETER_INDEX;
  colorconvert->output.graph_parameter_index = OUTPUT_PARAMETER_INDEX;

  colorconvert->input.color_format =
      map_gst_video_format_to_vx_format (in_info->finfo->format);
  colorconvert->output.color_format =
      map_gst_video_format_to_vx_format (out_info->finfo->format);

  if ((0 > colorconvert->input.color_format)
      || (0 > colorconvert->output.color_format)) {
    GST_ERROR_OBJECT (self, "Color format requested is not supported");
    return FALSE;
  }

  colorconvert->width = GST_VIDEO_INFO_WIDTH (in_info);
  colorconvert->height = GST_VIDEO_INFO_HEIGHT (in_info);

  status = app_init_color_convert (context, colorconvert);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_color_convert_create_graph (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph)
{
  GstTIOVXVideoConvert *self = GST_TIOVX_COLOR_CONVERT (trans);
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  GST_INFO_OBJECT (self, "Create graph");

  status =
      app_create_graph_color_convert (context, graph, self->obj, NULL,
      DEFAULT_TARGET);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_color_convert_release_buffer (GstTIOVXSiso * trans)
{
  GstTIOVXVideoConvert *self = GST_TIOVX_COLOR_CONVERT (trans);
  vx_status status = VX_SUCCESS;

  GST_INFO_OBJECT (self, "Release buffer");

  status = app_release_buffer_color_convert (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_color_convert_deinit_module (GstTIOVXSiso * trans)
{
  GstTIOVXVideoConvert *self = GST_TIOVX_COLOR_CONVERT (trans);
  vx_status status = VX_SUCCESS;

  GST_INFO_OBJECT (self, "Deinit module");

  status = app_delete_color_convert (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    return FALSE;
  }

  status = app_deinit_color_convert (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_color_convert_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node)
{
  GstTIOVXVideoConvert *self = GST_TIOVX_COLOR_CONVERT (trans);

  GST_INFO_OBJECT (self, "Get node info from module");

  g_return_val_if_fail (self->obj, FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj->node), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj->input.image_handle[0]), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj->output.image_handle[0]), FALSE);

  *node = self->obj->node;
  *input = (vx_reference *) & self->obj->input.image_handle[0];
  *output = (vx_reference *) & self->obj->output.image_handle[0];

  return TRUE;
}
