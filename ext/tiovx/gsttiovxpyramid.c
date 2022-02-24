/*
 * Copyright (c) [2022] Texas Instruments Incorporated
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

#include "gsttiovxpyramid.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_pyramid_module.h"

#define PYRAMID_INPUT_PARAM_INDEX 0
#define PYRAMID_OUTPUT_PARAM_INDEX 1

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
};

/* Formats definition */
#define TIOVX_PYRAMID_SUPPORTED_FORMATS "{GRAY8, GRAY16_LE}"
#define TIOVX_PYRAMID_SUPPORTED_WIDTH "[1 , 1088]"
#define TIOVX_PYRAMID_SUPPORTED_HEIGHT "[1 , 1920]"
#define TIOVX_PYRAMID_SUPPORTED_LEVELS "[1 , 32]"
#define TIOVX_PYRAMID_SUPPORTED_SCALE "[1/4 , 1/1]"
#define TIOVX_PYRAMID_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_PYRAMID_STATIC_CAPS_SRC                                   \
  "application/x-pyramid-tiovx, "                                       \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "             \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "                         \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "                       \
  "levels = " TIOVX_PYRAMID_SUPPORTED_LEVELS ", "                       \
  "scale = " TIOVX_PYRAMID_SUPPORTED_SCALE                              \
  "; "                                                                  \
  "application/x-pyramid-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "  \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "             \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "                         \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "                       \
  "levels = " TIOVX_PYRAMID_SUPPORTED_LEVELS ", "                       \
  "scale = " TIOVX_PYRAMID_SUPPORTED_SCALE ", "                         \
  "num-channels = " TIOVX_PYRAMID_SUPPORTED_CHANNELS \

/* Sink caps */
#define TIOVX_PYRAMID_STATIC_CAPS_SINK                       \
  "video/x-raw, "                                            \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "  \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "              \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "            \
  "framerate = " GST_VIDEO_FPS_RANGE                         \
  "; "                                                       \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "       \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "  \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "              \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "            \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                    \
  "num-channels = " TIOVX_PYRAMID_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_PYRAMID_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_PYRAMID_STATIC_CAPS_SRC)
    );

struct _GstTIOVXPyramid
{
  GstTIOVXSiso element;
  TIOVXPyramidModuleObj obj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_pyramid_debug);
#define GST_CAT_DEFAULT gst_tiovx_pyramid_debug

#define gst_tiovx_pyramid_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXPyramid, gst_tiovx_pyramid, GST_TYPE_TIOVX_SISO);

static gboolean gst_tiovx_pyramid_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels);
static gboolean gst_tiovx_pyramid_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_pyramid_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node,
    guint * input_param_index, guint * output_param_index);
static gboolean gst_tiovx_pyramid_release_buffer (GstTIOVXSiso * trans);
static gboolean gst_tiovx_pyramid_deinit_module (GstTIOVXSiso * trans,
    vx_context context);
static gboolean gst_tiovx_pyramid_compare_caps (GstTIOVXSiso * trans,
    GstCaps * caps1, GstCaps * caps2, GstPadDirection direction);

/* Initialize the plugin's class */
static void
gst_tiovx_pyramid_class_init (GstTIOVXPyramidClass * klass)
{
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSisoClass *gsttiovxsiso_class = NULL;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsiso_class = GST_TIOVX_SISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Pyramid",
      "Filter/Converter/Video",
      "Converts video frames to a vx_pyramid representation using the TIOVX Modules API",
      "RidgeRun support@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gsttiovxsiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_init_module);
  gsttiovxsiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_create_graph);
  gsttiovxsiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_get_node_info);
  gsttiovxsiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_release_buffer);
  gsttiovxsiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_deinit_module);
  gsttiovxsiso_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_compare_caps);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_pyramid_debug,
      "tiovxpyramid", 0, "TIOVX Pyramid element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_pyramid_init (GstTIOVXPyramid * self)
{
  memset (&self->obj, 0, sizeof self->obj);
}


/* GstTIOVXSiso Functions */

static gboolean
gst_tiovx_pyramid_init_module (GstTIOVXSiso * trans, vx_context context,
    GstCaps * in_caps, GstCaps * out_caps, guint num_channels)
{
  GstTIOVXPyramid *self = NULL;
  vx_status status = VX_SUCCESS;
  TIOVXPyramidModuleObj *pyramid = NULL;
  GstVideoInfo in_info;
  gboolean ret = FALSE;
  const GstStructure *pyramid_s = NULL;
  gint levels = 0;
  gdouble scale = 0;
  gint width = 0;
  gint height = 0;
  const gchar *gst_format_str = NULL;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (in_caps, FALSE);
  g_return_val_if_fail (out_caps, FALSE);
  g_return_val_if_fail (num_channels >= MIN_NUM_CHANNELS, FALSE);
  g_return_val_if_fail (num_channels <= MAX_NUM_CHANNELS, FALSE);

  self = GST_TIOVX_PYRAMID (trans);

  GST_INFO_OBJECT (self, "Init module");

  /* Get info from input caps */
  if (!gst_video_info_from_caps (&in_info, in_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get video info from input caps");
    goto exit;
  }

  /* Get info from output caps */
  pyramid_s = gst_caps_get_structure (out_caps, 0);
  if (!pyramid_s
      || !gst_structure_has_name (pyramid_s, "application/x-pyramid-tiovx")) {
    GST_ERROR_OBJECT (self, "Failed to get pyramid info from output caps");
    goto exit;
  }

  if (!gst_structure_get_int (pyramid_s, "levels", &levels)) {
    GST_ERROR_OBJECT (self, "Levels not found in pyramid caps");
    goto exit;
  }
  if (!gst_structure_get_double (pyramid_s, "scale", &scale)) {
    GST_ERROR_OBJECT (self, "Scale not found in pyramid caps");
    goto exit;
  }
  if (!gst_structure_get_int (pyramid_s, "width", &width)) {
    GST_ERROR_OBJECT (self, "Width not found in pyramid caps");
    goto exit;
  }
  if (!gst_structure_get_int (pyramid_s, "height", &height)) {
    GST_ERROR_OBJECT (self, "Height not found in pyramid caps");
    goto exit;
  }
  gst_format_str = gst_structure_get_string (pyramid_s, "format");
  format = gst_video_format_from_string (gst_format_str);
  if (GST_VIDEO_FORMAT_UNKNOWN == format) {
    GST_ERROR_OBJECT (self, "Format not found in pyramid caps");
    goto exit;
  }

  /* Configure pyramid */
  pyramid = &self->obj;
  pyramid->num_channels = num_channels;
  pyramid->input.bufq_depth = num_channels;
  pyramid->output.bufq_depth = num_channels;
  pyramid->width = GST_VIDEO_INFO_WIDTH (&in_info);
  pyramid->height = GST_VIDEO_INFO_HEIGHT (&in_info);

  /* Configure input */
  pyramid->input.color_format = gst_format_to_vx_format (in_info.finfo->format);

  /* Configure output */
  pyramid->output.levels = levels;
  pyramid->output.scale = scale;
  pyramid->output.color_format = gst_format_to_vx_format (format);

  status = tiovx_pyramid_module_init (context, pyramid);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto exit;
  }

  ret = TRUE;
exit:
  return ret;
}

static gboolean
gst_tiovx_pyramid_create_graph (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph)
{
  GstTIOVXPyramid *self = NULL;
  vx_status status = VX_SUCCESS;
  const char *target = TIVX_TARGET_VPAC_MSC1;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  self = GST_TIOVX_PYRAMID (trans);

  GST_INFO_OBJECT (self, "Create graph");

  if (NULL == target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    goto exit;
  }

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  status = tiovx_pyramid_module_create (graph, &self->obj, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto exit;
  }

  ret = TRUE;

exit:
  return ret;
}

static gboolean
gst_tiovx_pyramid_release_buffer (GstTIOVXSiso * trans)
{
  GstTIOVXPyramid *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_PYRAMID (trans);

  GST_INFO_OBJECT (self, "Release buffer");

  status = tiovx_pyramid_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_pyramid_deinit_module (GstTIOVXSiso * trans, vx_context context)
{
  GstTIOVXPyramid *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_PYRAMID (trans);

  GST_INFO_OBJECT (self, "Deinit module");

  status = tiovx_pyramid_module_delete (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    return FALSE;
  }

  status = tiovx_pyramid_module_deinit (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_pyramid_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node,
    guint * input_param_index, guint * output_param_index)
{
  GstTIOVXPyramid *self = NULL;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_PYRAMID (trans);

  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.node), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.input.image_handle[0]), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.output.pyramid_handle[0]), FALSE);

  GST_INFO_OBJECT (self, "Get node info from module");

  *node = self->obj.node;
  *input = (vx_reference *) & self->obj.input.image_handle[0];
  *output = (vx_reference *) & self->obj.output.pyramid_handle[0];

  *input_param_index = PYRAMID_INPUT_PARAM_INDEX;
  *output_param_index = PYRAMID_OUTPUT_PARAM_INDEX;

  return TRUE;
}

static gboolean
gst_tiovx_pyramid_compare_caps (GstTIOVXSiso * trans, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  GstVideoInfo video_info1;
  GstVideoInfo video_info2;
  gboolean ret = FALSE;

  g_return_val_if_fail (caps1, FALSE);
  g_return_val_if_fail (caps2, FALSE);
  g_return_val_if_fail (GST_PAD_UNKNOWN != direction, FALSE);

  /* Compare image fields if sink pad */
  if (GST_PAD_SINK == direction) {
    if (!gst_video_info_from_caps (&video_info1, caps1)) {
      GST_ERROR_OBJECT (trans, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps1);
      goto out;
    }

    if (!gst_video_info_from_caps (&video_info2, caps2)) {
      GST_ERROR_OBJECT (trans, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps2);
      goto out;
    }

    if ((video_info1.width == video_info2.width) &&
        (video_info1.height == video_info2.height) &&
        (video_info1.finfo->format == video_info2.finfo->format)
        ) {
      ret = TRUE;
    }
  }

  /* Compare pyramid fields if src pad */
  if (GST_PAD_SRC == direction) {
    if (gst_caps_is_equal (caps1, caps2)) {
      ret = TRUE;
    }
  }

out:
  return ret;
}
