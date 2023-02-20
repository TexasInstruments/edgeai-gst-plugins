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
#include "config.h"
#endif

#include "gsttiovxmultiscaler.h"

#include "gsttiovxmultiscalerpad.h"
#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_multi_scaler_module.h"

/* TODO: Implement method to choose number of channels dynamically */
#define DEFAULT_NUM_CHANNELS 1

static const int input_param_id = 0;
static const int output_param_id_start = 1;

/* Target definition */
enum
{
  TIVX_TARGET_VPAC_MSC1_ID = 0,
  TIVX_TARGET_VPAC_MSC2_ID,
#if defined(SOC_J784S4)
  TIVX_TARGET_VPAC2_MSC1_ID,
  TIVX_TARGET_VPAC2_MSC2_ID,
#endif
};

#define GST_TYPE_TIOVX_MULTI_SCALER_TARGET (gst_tiovx_multi_scaler_target_get_type())
static GType
gst_tiovx_multi_scaler_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_MSC1_ID, "VPAC MSC1", TIVX_TARGET_VPAC_MSC1},
    {TIVX_TARGET_VPAC_MSC2_ID, "VPAC MSC2", TIVX_TARGET_VPAC_MSC2},
#if defined(SOC_J784S4)
    {TIVX_TARGET_VPAC2_MSC1_ID, "VPAC2 MSC1", TIVX_TARGET_VPAC2_MSC1},
    {TIVX_TARGET_VPAC2_MSC2_ID, "VPAC2 MSC2", TIVX_TARGET_VPAC2_MSC2},
#endif
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXMultiScalerTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_MULTI_SCALER_TARGET TIVX_TARGET_VPAC_MSC1_ID

/* Interpolation Method definition */
#define GST_TYPE_TIOVX_MULTI_SCALER_INTERPOLATION_METHOD (gst_tiovx_multi_scaler_interpolation_method_get_type())
static GType
gst_tiovx_multi_scaler_interpolation_method_get_type (void)
{
  static GType interpolation_method_type = 0;

  static const GEnumValue interpolation_methods[] = {
    {VX_INTERPOLATION_BILINEAR, "Bilinear", "bilinear"},
    {VX_INTERPOLATION_NEAREST_NEIGHBOR, "Nearest Neighbor", "nearest-neighbor"},
    {TIVX_VPAC_MSC_INTERPOLATION_GAUSSIAN_32_PHASE, "Gaussian 32 Phase",
        "gaussian-32-phase"},
    {0, NULL, NULL},
  };

  if (!interpolation_method_type) {
    interpolation_method_type =
        g_enum_register_static ("GstTIOVXMultiScalerInterpolationMethod",
        interpolation_methods);
  }
  return interpolation_method_type;
}

#define DEFAULT_TIOVX_MULTI_SCALER_INTERPOLATION_METHOD VX_INTERPOLATION_BILINEAR

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
  PROP_INTERPOLATION_METHOD,
};

/* Formats definition */
#define TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC "{ NV12, GRAY8, GRAY16_LE }"
#define TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SINK "{ NV12, GRAY8, GRAY16_LE }"
#define TIOVX_MULTI_SCALER_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_MULTI_SCALER_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_MULTI_SCALER_SUPPORTED_CHANNELS "[1 , 16]"

#define TIOVX_MULTI_SCALER_STATIC_CAPS_IMAGE                         \
  "video/x-raw, "                                                    \
  "format = (string) " TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_MULTI_SCALER_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_MULTI_SCALER_SUPPORTED_HEIGHT ", "               \
  "framerate = " GST_VIDEO_FPS_RANGE                                 \
  "; "                                                               \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "               \
  "format = (string) " TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_MULTI_SCALER_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_MULTI_SCALER_SUPPORTED_HEIGHT ", "               \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                            \
  "num-channels = " TIOVX_MULTI_SCALER_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_MULTI_SCALER_STATIC_CAPS_IMAGE)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_MULTI_SCALER_STATIC_CAPS_IMAGE)
    );

struct _GstTIOVXMultiScaler
{
  GstTIOVXSimo element;
  TIOVXMultiScalerModuleObj obj;
  gint interpolation_method;
  gint target_id;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_multi_scaler_debug);
#define GST_CAT_DEFAULT gst_tiovx_multi_scaler_debug

#define gst_tiovx_multi_scaler_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXMultiScaler, gst_tiovx_multi_scaler,
    GST_TYPE_TIOVX_SIMO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_multi_scaler_debug,
        "tiovxmultiscaler", 0,
        "debug category for the tiovxmultiscaler element"));

static void
gst_tiovx_multi_scaler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_multi_scaler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tiovx_multi_scaler_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list, guint num_channels);

static gboolean gst_tiovx_multi_scaler_configure_module (GstTIOVXSimo * simo);

static gboolean gst_tiovx_multi_scaler_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    GList ** queueable_objects);

static gboolean gst_tiovx_multi_scaler_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static GstCaps *gst_tiovx_multi_scaler_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list, GList *src_pads);
static GstCaps *gst_tiovx_multi_scaler_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps, GstTIOVXPad *src_pad);

static GList *gst_tiovx_multi_scaler_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list);

void gst_tiovx_intersect_src_caps (gpointer data, gpointer filter);

static gboolean gst_tiovx_multi_scaler_deinit_module (GstTIOVXSimo * simo);

static const gchar *target_id_to_target_name (gint target_id);

static gboolean
gst_tiovx_multi_scaler_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction);

/* Initialize the plugin's class */
static void
gst_tiovx_multi_scaler_class_init (GstTIOVXMultiScalerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstTIOVXSimoClass *gsttiovxsimo_class = GST_TIOVX_SIMO_CLASS (klass);
  GstPadTemplate *src_temp = NULL;
  GstPadTemplate *sink_temp = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX MultiScaler",
      "Filter",
      "Multi scaler using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  src_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_TIOVX_MULTISCALER_PAD);
  gst_element_class_add_pad_template (gstelement_class, src_temp);

  sink_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TYPE_TIOVX_MULTISCALER_PAD);
  gst_element_class_add_pad_template (gstelement_class, sink_temp);

  gobject_class->set_property = gst_tiovx_multi_scaler_set_property;
  gobject_class->get_property = gst_tiovx_multi_scaler_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_MULTI_SCALER_TARGET,
          DEFAULT_TIOVX_MULTI_SCALER_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTERPOLATION_METHOD,
      g_param_spec_enum ("interpolation-method", "Interpolation Method",
          "Interpolation method to use by the scaler",
          GST_TYPE_TIOVX_MULTI_SCALER_INTERPOLATION_METHOD,
          DEFAULT_TIOVX_MULTI_SCALER_INTERPOLATION_METHOD,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gsttiovxsimo_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_init_module);

  gsttiovxsimo_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_configure_module);

  gsttiovxsimo_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_get_node_info);

  gsttiovxsimo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_create_graph);

  gsttiovxsimo_class->get_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_get_sink_caps);

  gsttiovxsimo_class->get_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_get_src_caps);

  gsttiovxsimo_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_fixate_caps);

  gsttiovxsimo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_deinit_module);

  gsttiovxsimo_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_compare_caps);
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_multi_scaler_init (GstTIOVXMultiScaler * self)
{
  self->target_id = DEFAULT_TIOVX_MULTI_SCALER_TARGET;
  self->interpolation_method = DEFAULT_TIOVX_MULTI_SCALER_INTERPOLATION_METHOD;
}

static void
gst_tiovx_multi_scaler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMultiScaler *self = GST_TIOVX_MULTI_SCALER (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    case PROP_INTERPOLATION_METHOD:
      self->interpolation_method = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_multi_scaler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMultiScaler *self = GST_TIOVX_MULTI_SCALER (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    case PROP_INTERPOLATION_METHOD:
      g_value_set_enum (value, self->interpolation_method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_multi_scaler_init_module (GstTIOVXSimo * simo, vx_context context,
    GstTIOVXPad * sink_pad, GList * src_pads, GstCaps * sink_caps,
    GList * src_caps_list, guint num_channels)
{
  GstTIOVXMultiScaler *self = NULL;
  TIOVXMultiScalerModuleObj *multiscaler = NULL;
  GList *l = NULL;
  gboolean ret = TRUE;
  GstVideoInfo in_info = { };
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);
  g_return_val_if_fail (sink_caps, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);

  multiscaler = &self->obj;

  /* Initialize the input parameters */
  if (!gst_video_info_from_caps (&in_info, sink_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
        GST_PTR_FORMAT, sink_caps);
    ret = FALSE;
    goto out;
  }

  multiscaler->input.width = GST_VIDEO_INFO_WIDTH (&in_info);
  multiscaler->input.height = GST_VIDEO_INFO_HEIGHT (&in_info);
  multiscaler->color_format = gst_format_to_vx_format (in_info.finfo->format);
  multiscaler->input.bufq_depth = 1;
  multiscaler->input.graph_parameter_index = 0;

  GST_INFO_OBJECT (self,
      "Input parameters: \n  Width: %d \n  Height: %d \n  Pool size: %d",
      multiscaler->input.width, multiscaler->input.height,
      multiscaler->input.bufq_depth);

  /* Initialize the output parameters */
  for (l = src_caps_list; l != NULL; l = l->next) {
    GstCaps *src_caps = (GstCaps *) l->data;
    GstVideoInfo out_info = { };
    gint i = g_list_position (src_caps_list, l);

    if (!gst_video_info_from_caps (&out_info, src_caps)) {
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT, src_caps);
      ret = FALSE;
      goto out;
    }

    multiscaler->output[i].width = GST_VIDEO_INFO_WIDTH (&out_info);
    multiscaler->output[i].height = GST_VIDEO_INFO_HEIGHT (&out_info);
    multiscaler->output[i].color_format =
        gst_format_to_vx_format (out_info.finfo->format);
    multiscaler->output[i].bufq_depth = 1;
    /* TODO: Improve this logic. We need to retrieve the index from the GstTIOVXPad */
    multiscaler->output[i].graph_parameter_index = i + 1;

    GST_INFO_OBJECT (self,
        "Output %d parameters: \n  Width: %d \n  Height: %d \n  Pool size: %d",
        i, multiscaler->output[i].width, multiscaler->output[i].height,
        multiscaler->output[i].bufq_depth);
  }

  for (l = src_pads; l != NULL; l = l->next) {
    GstTIOVXMultiScalerPad *src_pad = (GstTIOVXMultiScalerPad *) l->data;
    gint i = g_list_position (src_pads, l);

    if (src_pad->roi_width == 0) {
      src_pad->roi_width = multiscaler->input.width - src_pad->roi_startx;
    }

    if (src_pad->roi_height == 0) {
      src_pad->roi_height = multiscaler->input.height - src_pad->roi_starty;
    }

    if (src_pad->roi_startx + src_pad->roi_width > multiscaler->input.width) {
      GST_ERROR_OBJECT (self, "ROI width exceeds the input image");
      ret = FALSE;
      goto out;
    }

    if (src_pad->roi_starty + src_pad->roi_height > multiscaler->input.height) {
      GST_ERROR_OBJECT (self, "ROI height exceeds the input image");
      ret = FALSE;
      goto out;
    }

    if (multiscaler->output[i].width > src_pad->roi_width ||
            multiscaler->output[i].height > src_pad->roi_height) {
      GST_ERROR_OBJECT (self, "Multiscaler does not support upscaling");
      ret = FALSE;
      goto out;
    }

    if (multiscaler->output[i].width < src_pad->roi_width/4 ||
            multiscaler->output[i].height < src_pad->roi_height/4) {
      GST_ERROR_OBJECT (self,
              "Multiscaler does not support downscaling by a factor > 4");
      ret = FALSE;
      goto out;
    }

    multiscaler->crop_params[i].crop_start_x = src_pad->roi_startx;
    multiscaler->crop_params[i].crop_start_y = src_pad->roi_starty;
    multiscaler->crop_params[i].crop_width = src_pad->roi_width;
    multiscaler->crop_params[i].crop_height = src_pad->roi_height;
  }

  /* Initialize general parameters */
  GST_OBJECT_LOCK (GST_OBJECT (self));
  multiscaler->interpolation_method = self->interpolation_method;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
  multiscaler->num_channels = num_channels;
  multiscaler->num_outputs = g_list_length (src_caps_list);

  GST_INFO_OBJECT (self, "Initializing scaler object");
  status = tiovx_multi_scaler_module_init (context, multiscaler);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    ret = FALSE;
  }

out:
  return ret;
}

static gboolean
gst_tiovx_multi_scaler_configure_module (GstTIOVXSimo * simo)
{
  GstTIOVXMultiScaler *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = TRUE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);

  GST_DEBUG_OBJECT (self, "Update filter coeffs");
  status = tiovx_multi_scaler_module_update_filter_coeffs (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure filter coefficients failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Update crop params");
  status = tiovx_multi_scaler_module_update_crop_params (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module update crop params failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Release buffer scaler");
  status = tiovx_multi_scaler_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure release buffer failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

out:
  return ret;
}

static gboolean
gst_tiovx_multi_scaler_get_node_info (GstTIOVXSimo * simo, vx_node * node,
    GstTIOVXPad * sink_pad, GList * src_pads, GList ** queueable_objects)
{
  GstTIOVXMultiScaler *self = NULL;
  GList *l = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);

  *node = self->obj.node;

  /* Set input parameters */
  gst_tiovx_pad_set_params (sink_pad,
      self->obj.input.arr[0], (vx_reference) self->obj.input.image_handle[0],
      self->obj.input.graph_parameter_index, input_param_id);

  for (l = src_pads; l != NULL; l = l->next) {
    GstTIOVXPad *src_pad = (GstTIOVXPad *) l->data;
    gint i = g_list_position (src_pads, l);

    /* Set output parameters */
    gst_tiovx_pad_set_params (src_pad,
        self->obj.output[i].arr[0],
        (vx_reference) self->obj.output[i].image_handle[0],
        self->obj.output[i].graph_parameter_index, output_param_id_start + i);
  }

  return TRUE;
}

static gboolean
gst_tiovx_multi_scaler_create_graph (GstTIOVXSimo * simo, vx_context context,
    vx_graph graph)
{
  GstTIOVXMultiScaler *self = NULL;
  TIOVXMultiScalerModuleObj *multiscaler = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  const gchar *target = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  multiscaler = &self->obj;

  if (NULL == target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Creating scaler graph");
  status = tiovx_multi_scaler_module_create (graph, multiscaler, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Finished creating scaler graph");

  ret = TRUE;

out:
  return ret;
}

static void
gst_tivox_multi_scaler_compute_src_dimension (GstTIOVXSimo * simo,
    const GValue * dimension, GValue * out_value, guint roi_len)
{
  static const gint scale = 4;
  gint out_max = -1;
  gint out_min = -1;
  gint dim_max = -1;
  gint dim_min = -1;

  g_return_if_fail (simo);
  g_return_if_fail (dimension);
  g_return_if_fail (out_value);

  /* Because of HW limitations, the input cannot be:
   * - Smaller than the output (cannot upscale)
   * - 4 times bigger than the output (4x downscaling is the maximum)
   *
   * Given this, the output (src) range based on the input (sink) range looks like:
   *
   *     INT MAX +
   *             |
   *             |
   *     sinkmax | +------+ +-----+
   *             | | SINK | |     |
   *             | |      | |     |
   *     sinkmin | +------+ | SRC |
   *             |          |     |
   *      srcmin |          +-----+
   * (sinkmin/4) |
   *           0 +
   */
  if (roi_len) {
    dim_max = dim_min = roi_len;
  } else if (GST_VALUE_HOLDS_INT_RANGE (dimension)) {
    dim_max = gst_value_get_int_range_max (dimension);
    dim_min = gst_value_get_int_range_min (dimension);
  } else {
    dim_max = dim_min = g_value_get_int (dimension);
  }

  out_max = dim_max;
  out_min = 1.0 * dim_min / scale + 0.5;

  /* Minimum dimension is 1, 0 is invalid */
  if (0 == out_min) {
    out_min = 1;
  }

  GST_DEBUG_OBJECT (simo,
      "computed an output of [%d, %d] from an input of [%d, %d]", out_min,
      out_max, dim_min, dim_max);

  g_value_init (out_value, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (out_value, out_min, out_max);
}

static void
gst_tivox_multi_scaler_compute_sink_dimension (GstTIOVXSimo * simo,
    const GValue * dimension, GValue * out_value, guint roi_len)
{
  static const gint scale = 4;
  gint out_max = -1;
  gint out_min = -1;
  gint dim_max = -1;
  gint dim_min = -1;

  g_return_if_fail (simo);
  g_return_if_fail (dimension);
  g_return_if_fail (out_value);

  /* Because of HW limitations, the input cannot be:
   * - Smaller than the output (cannot upscale)
   * - 4 times bigger than the output (4x downscaling is the maximum)
   *
   * Given this, the input (sink) range based on the output (src) range looks like:
   *
   *     INT MAX +
   *             |
   *             |
   *     sinkmax | +------+
   * (4x srcmax) | |      |
   *             | | SINK |
   *      srcmax | |      | +-----+
   *             | |      | | SRC |
   *         min | +------+ +-----+
   *             |
   *           0 +
   */
  if (GST_VALUE_HOLDS_INT_RANGE (dimension)) {
    dim_max = gst_value_get_int_range_max (dimension);
    dim_min = gst_value_get_int_range_min (dimension);
  } else {
    dim_max = dim_min = g_value_get_int (dimension);
  }

  out_min = dim_min;

  if (G_MAXINT == dim_max || (G_MAXINT / scale) < dim_max || roi_len) {
    out_max = G_MAXINT;
  } else {
    out_max = dim_max * scale;
  }

  GST_DEBUG_OBJECT (simo,
      "computed an input of [%d, %d] from an output of [%d, %d]", out_min,
      out_max, dim_min, dim_max);

  g_value_init (out_value, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (out_value, out_min, out_max);
}

typedef void (*GstTIOVXDimFunc) (GstTIOVXSimo * simo,
    const GValue * dimension, GValue * out_value, guint roi_len);

static void
gst_tivox_multi_scaler_compute_named (GstTIOVXSimo * simo,
    GstStructure * structure, const gchar * name, guint roi_len,
    GstTIOVXDimFunc func)
{
  const GValue *input = NULL;
  GValue output = G_VALUE_INIT;

  g_return_if_fail (simo);
  g_return_if_fail (structure);
  g_return_if_fail (name);
  g_return_if_fail (func);

  input = gst_structure_get_value (structure, name);
  func (simo, input, &output, roi_len);
  gst_structure_set_value (structure, name, &output);

  g_value_unset (&output);
}

static GstCaps *
gst_tiovx_multi_scaler_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list, GList *src_pads)
{
  GstCaps *sink_caps = NULL;
  GstCaps *template_caps = NULL;
  GList *l = NULL;
  GList *p = NULL;
  gint i = 0;

  g_return_val_if_fail (simo, NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_DEBUG_OBJECT (simo,
      "Computing sink caps based on src caps and filter %"
      GST_PTR_FORMAT, filter);

  template_caps = gst_static_pad_template_get_caps (&sink_template);
  if (filter) {
    sink_caps = gst_caps_intersect (template_caps, filter);
  } else {
    sink_caps = gst_caps_copy (template_caps);
  }
  gst_caps_unref (template_caps);

  for (l = src_caps_list, p = src_pads; l != NULL;
      l = l->next, p = p->next) {
    GstCaps *src_caps = gst_caps_copy ((GstCaps *) l->data);
    GstCaps *tmp = NULL;
    GstTIOVXDimFunc func = gst_tivox_multi_scaler_compute_sink_dimension;
    GstTIOVXMultiScalerPad *src_pad = (GstTIOVXMultiScalerPad *) p->data;

    for (i = 0; i < gst_caps_get_size (src_caps); i++) {
      GstStructure *st = gst_caps_get_structure (src_caps, i);
      gst_tivox_multi_scaler_compute_named (simo, st, "width",
          src_pad->roi_width, func);
      gst_tivox_multi_scaler_compute_named (simo, st, "height",
          src_pad->roi_height, func);
    }

    tmp = gst_caps_intersect (sink_caps, src_caps);
    gst_caps_unref (sink_caps);
    gst_caps_unref (src_caps);
    sink_caps = tmp;
  }

  GST_DEBUG_OBJECT (simo, "result: %" GST_PTR_FORMAT, sink_caps);

  return sink_caps;
}

static GstCaps *
gst_tiovx_multi_scaler_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps, GstTIOVXPad *src_pad_tiovx)
{
  GstCaps *src_caps = NULL;
  GstCaps *template_caps = NULL;
  gint i = 0;
  GstTIOVXMultiScalerPad *src_pad;

  g_return_val_if_fail (simo, NULL);
  g_return_val_if_fail (sink_caps, NULL);

  GST_DEBUG_OBJECT (simo,
      "Computing src caps based on sink caps %" GST_PTR_FORMAT " and filter %"
      GST_PTR_FORMAT, sink_caps, filter);

  template_caps = gst_static_pad_template_get_caps (&src_template);
  src_caps = gst_caps_intersect (template_caps, sink_caps);
  gst_caps_unref (template_caps);
  src_pad = (GstTIOVXMultiScalerPad *) src_pad_tiovx;

  for (i = 0; i < gst_caps_get_size (src_caps); i++) {
    GstStructure *st = gst_caps_get_structure (src_caps, i);
    GstTIOVXDimFunc func = gst_tivox_multi_scaler_compute_src_dimension;

    gst_tivox_multi_scaler_compute_named (simo, st, "width",
        src_pad->roi_width, func);
    gst_tivox_multi_scaler_compute_named (simo, st, "height",
        src_pad->roi_height, func);
  }

  if (filter) {
    GstCaps *tmp = src_caps;
    src_caps = gst_caps_intersect (src_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_INFO_OBJECT (simo,
      "Resulting supported src caps by TIOVX multiscaler node: %"
      GST_PTR_FORMAT, src_caps);

  return src_caps;
}

static GList *
gst_tiovx_multi_scaler_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list)
{
  GList *l = NULL;
  GstStructure *sink_structure = NULL;
  GList *result_caps_list = NULL;
  gint width = 0;
  gint height = 0;
  const gchar *format = NULL;
  const GValue *vframerate = NULL;

  g_return_val_if_fail (sink_caps, NULL);
  g_return_val_if_fail (gst_caps_is_fixed (sink_caps), NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_DEBUG_OBJECT (simo, "Fixating src caps from sink caps %" GST_PTR_FORMAT,
      sink_caps);

  sink_structure = gst_caps_get_structure (sink_caps, 0);

  if (!gst_structure_get_int (sink_structure, "width", &width)) {
    GST_ERROR_OBJECT (simo, "Width is missing in sink caps");
    return NULL;
  }

  if (!gst_structure_get_int (sink_structure, "height", &height)) {
    GST_ERROR_OBJECT (simo, "Height is missing in sink caps");
    return NULL;
  }

  format = gst_structure_get_string (sink_structure, "format");
  if (NULL == format) {
    GST_ERROR_OBJECT (simo, "Format is missing in sink caps");
    return NULL;
  }

  vframerate = gst_structure_get_value (sink_structure, "framerate");
  if (NULL == vframerate) {
    GST_ERROR_OBJECT (simo, "Framerate is missing in sink caps");
    return NULL;
  }

  for (l = src_caps_list; l != NULL; l = l->next) {
    GstCaps *src_caps = (GstCaps *) l->data;
    GstStructure *src_st = gst_caps_get_structure (src_caps, 0);
    GstCaps *new_caps = gst_caps_fixate (gst_caps_ref (src_caps));
    GstStructure *new_st = gst_caps_get_structure (new_caps, 0);
    const GValue *vwidth = NULL, *vheight = NULL, *vformat = NULL;

    vwidth = gst_structure_get_value (src_st, "width");
    vheight = gst_structure_get_value (src_st, "height");
    vformat = gst_structure_get_value (src_st, "format");

    gst_structure_set_value (new_st, "width", vwidth);
    gst_structure_set_value (new_st, "height", vheight);
    gst_structure_set_value (new_st, "format", vformat);
    gst_structure_set_value (new_st, "framerate", vframerate);

    gst_structure_fixate_field_nearest_int (new_st, "width", width);
    gst_structure_fixate_field_nearest_int (new_st, "height", height);
    gst_structure_fixate_field_string (new_st, "format", format);

    GST_DEBUG_OBJECT (simo, "Fixated %" GST_PTR_FORMAT " into %" GST_PTR_FORMAT,
        src_caps, new_caps);

    result_caps_list = g_list_append (result_caps_list, new_caps);
  }

  return result_caps_list;
}

static gboolean
gst_tiovx_multi_scaler_deinit_module (GstTIOVXSimo * simo)
{
  GstTIOVXMultiScaler *self = NULL;
  TIOVXMultiScalerModuleObj *multiscaler = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = TRUE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);
  multiscaler = &self->obj;

  /* Delete graph */
  status = tiovx_multi_scaler_module_delete (multiscaler);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    ret = FALSE;
    goto out;
  }

  status = tiovx_multi_scaler_module_deinit (multiscaler);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

out:
  return ret;
}

static const gchar *
target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_multi_scaler_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static gboolean
gst_tiovx_multi_scaler_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  GstVideoInfo video_info1;
  GstVideoInfo video_info2;
  gboolean ret = FALSE;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (caps1, FALSE);
  g_return_val_if_fail (caps2, FALSE);
  g_return_val_if_fail (GST_PAD_UNKNOWN != direction, FALSE);

  if (!gst_video_info_from_caps (&video_info1, caps1)) {
    GST_ERROR_OBJECT (simo, "Failed to get info from caps: %"
        GST_PTR_FORMAT, caps1);
    goto out;
  }

  if (!gst_video_info_from_caps (&video_info2, caps2)) {
    GST_ERROR_OBJECT (simo, "Failed to get info from caps: %"
        GST_PTR_FORMAT, caps2);
    goto out;
  }

  if (video_info1.finfo->format == video_info2.finfo->format) {
    ret = TRUE;
  }

out:
  return ret;
}
