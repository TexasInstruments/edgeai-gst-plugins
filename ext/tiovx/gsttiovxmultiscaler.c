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

#include "gsttiovxmultiscaler.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "app_scaler_module.h"

/* TODO: Implement method to choose number of channels dynamically */
#define DEFAULT_NUM_CHANNELS 1

/* Target definition */
enum
{
  TIVX_TARGET_VPAC_MSC1_ID = 0,
  TIVX_TARGET_VPAC_MSC2_ID,
};

#define GST_TYPE_TIOVX_MULTI_SCALER_TARGET (gst_tiovx_multi_scaler_target_get_type())
static GType
gst_tiovx_multi_scaler_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_MSC1_ID, "VPAC MSC1", TIVX_TARGET_VPAC_MSC1},
    {TIVX_TARGET_VPAC_MSC2_ID, "VPAC MSC2", TIVX_TARGET_VPAC_MSC2},
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
#define TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC "{NV12}"
#define TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SINK "{NV12}"

/* Src caps */
#define TIOVX_MULTI_SCALER_STATIC_CAPS_SRC GST_VIDEO_CAPS_MAKE (TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC)

/* Sink caps */
#define TIOVX_MULTI_SCALER_STATIC_CAPS_SINK GST_VIDEO_CAPS_MAKE (TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SINK)

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_MULTI_SCALER_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_MULTI_SCALER_STATIC_CAPS_SRC)
    );

struct _GstTIOVXMultiScaler
{
  GstTIOVXSimo element;
  ScalerObj obj;
  gint interpolation_method;
  gint target_id;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_multi_scaler_debug);
#define GST_CAT_DEFAULT gst_tiovx_multi_scaler_debug

#define gst_tiovx_multi_scaler_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXMultiScaler, gst_tiovx_multi_scaler,
    GST_TIOVX_SIMO_TYPE);

static void
gst_tiovx_multi_scaler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_multi_scaler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tiovx_multi_scaler_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean gst_tiovx_multi_scaler_configure_module (GstTIOVXSimo * simo);

static gboolean gst_tiovx_multi_scaler_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    vx_reference * input_ref, vx_reference ** output_refs);

static gboolean gst_tiovx_multi_scaler_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static GstCaps *gst_tiovx_multi_scaler_get_caps (GstTIOVXSimo * self,
    GstCaps * filter, GList * src_caps_list);

static GList *gst_tiovx_multi_scaler_fixate_caps (GstTIOVXSimo * self,
    GstCaps * sink_caps, GList * src_caps_list);

void gst_tiovx_intersect_src_caps (gpointer data, gpointer filter);

static gboolean gst_tiovx_multi_scaler_deinit_module (GstTIOVXSimo * simo);

/* Initialize the plugin's class */
static void
gst_tiovx_multi_scaler_class_init (GstTIOVXMultiScalerClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSimoClass *gsttiovxsimo_class = NULL;
  GstPadTemplate *src_temp = NULL;
  GstPadTemplate *sink_temp = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsimo_class = GST_TIOVX_SIMO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX MultiScaler",
      "Filter",
      "Multi scaler using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  src_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TIOVX_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, src_temp);

  sink_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TIOVX_TYPE_PAD);
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

  gsttiovxsimo_class->get_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_get_caps);

  gsttiovxsimo_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_fixate_caps);

  gsttiovxsimo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_deinit_module);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_multi_scaler_debug,
      "tiovxmultiscaler", 0, "debug category for the tiovxmultiscaler element");
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
    GList * src_caps_list)
{
  GstTIOVXMultiScaler *self = NULL;
  ScalerObj *multiscaler = NULL;
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
  multiscaler->input.bufq_depth = DEFAULT_NUM_CHANNELS;
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
    multiscaler->output[i].bufq_depth = DEFAULT_NUM_CHANNELS;
    /* TODO: Improve this logic. We need to retrieve the index from the GstTIOVXPad */
    multiscaler->output[i].graph_parameter_index = i + 1;

    GST_INFO_OBJECT (self,
        "Output %d parameters: \n  Width: %d \n  Height: %d \n  Pool size: %d",
        i, multiscaler->output[i].width, multiscaler->output[i].height,
        multiscaler->output[i].bufq_depth);
  }

  /* Initialize general parameters */
  GST_OBJECT_LOCK (GST_OBJECT (self));
  multiscaler->method = self->interpolation_method;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
  multiscaler->num_ch = DEFAULT_NUM_CHANNELS;
  multiscaler->num_outputs = g_list_length (src_caps_list);

  GST_INFO_OBJECT (self, "Initializing scaler object");
  status = app_init_scaler (context, multiscaler);
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
  status = app_update_scaler_filter_coeffs (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure filter coefficients failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Release buffer scaler");
  status = app_release_buffer_scaler (&self->obj);
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
    GstTIOVXPad * sink_pad, GList * src_pads, vx_reference * input_ref,
    vx_reference ** output_refs)
{
  GstTIOVXMultiScaler *self = NULL;
  GList *l = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);
  g_return_val_if_fail (input_ref, FALSE);
  g_return_val_if_fail (output_refs, FALSE);


  self = GST_TIOVX_MULTI_SCALER (simo);

  *node = self->obj.node;

  /* Set input exemplar */
  gst_tiovx_pad_set_exemplar (sink_pad,
      (vx_reference) self->obj.input.image_handle[0]);

  *input_ref = (vx_reference) self->obj.input.image_handle[0];

  *output_refs = g_malloc (sizeof (vx_reference) * g_list_length (src_pads));
  for (l = src_pads; l != NULL; l = l->next) {
    GstTIOVXPad *src_pad = (GstTIOVXPad *) l->data;
    gint i = g_list_position (src_pads, l);
    /* Set output exemplar */
    gst_tiovx_pad_set_exemplar (src_pad,
        (vx_reference) self->obj.output[i].image_handle[0]);

    *(output_refs[i]) = (vx_reference) self->obj.output[i].image_handle[0];
  }

  return TRUE;
}

static gboolean
gst_tiovx_multi_scaler_create_graph (GstTIOVXSimo * simo, vx_context context,
    vx_graph graph)
{
  GstTIOVXMultiScaler *self = NULL;
  ScalerObj *multiscaler = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = TRUE;
  const gchar *default_target = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);

  GST_OBJECT_LOCK (self);
  /* TODO: Add real TARGET */
  default_target = TIVX_TARGET_VPAC_MSC1;
  GST_OBJECT_UNLOCK (self);

  multiscaler = &self->obj;

  GST_DEBUG_OBJECT (self, "Creating scaler graph");
  status =
      app_create_graph_scaler (context, graph, multiscaler, NULL,
      default_target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Finished creating scaler graph");

out:
  return ret;
}

static GstCaps *
gst_tiovx_multi_scaler_get_caps (GstTIOVXSimo * self,
    GstCaps * filter, GList * src_caps_list)
{
  GstCaps *sink_caps = NULL;
  GList *l = NULL;
  gint i = 0;

  g_return_val_if_fail (filter, NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_LOG_OBJECT (self, "get_caps");

  /*
   * Intersect with filter
   * Remove the w, h from structures
   */

  sink_caps = gst_caps_copy (filter);

  for (l = src_caps_list; l != NULL; l = l->next) {
    GstCaps *src_caps = (GstCaps *) l->data;
    GstCaps *tmp = NULL;

    for (i = 0; i < gst_caps_get_size (src_caps); i++) {
      GstStructure *structure = NULL;
      structure = gst_caps_get_structure (src_caps, i);
      gst_structure_remove_field (structure, "width");
      gst_structure_remove_field (structure, "height");
    }

    tmp = gst_caps_intersect (sink_caps, src_caps);
    gst_caps_unref (sink_caps);
    sink_caps = tmp;
  }

  return sink_caps;
}

static GList *
gst_tiovx_multi_scaler_fixate_caps (GstTIOVXSimo * self,
    GstCaps * sink_caps, GList * src_caps_list)
{
  GList *l = NULL;
  GstStructure *sink_structure = NULL;
  GList *result_caps_list = NULL;
  gint width = 0;
  gint height = 0;
  gint i = 0;

  g_return_val_if_fail (sink_caps, NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_LOG_OBJECT (self, "fixate_caps");

  sink_structure = gst_caps_get_structure (sink_caps, 0);

  if (!gst_structure_get_int (sink_structure, "width", &width)) {
    GST_ERROR_OBJECT (self, "Width is missing in sink caps");
    return NULL;
  }

  if (!gst_structure_get_int (sink_structure, "height", &height)) {
    GST_ERROR_OBJECT (self, "Height is missing in sink caps");
    return NULL;
  }

  for (l = src_caps_list; l != NULL; l = l->next) {
    GstCaps *src_caps = (GstCaps *) l->data;
    GstCaps *new_caps = NULL;

    for (i = 0; i < gst_caps_get_size (src_caps); i++) {
      GstStructure *src_structure = NULL;
      src_structure = gst_caps_get_structure (src_caps, i);
      gst_structure_fixate_field_nearest_int (src_structure, "width", width);
      gst_structure_fixate_field_nearest_int (src_structure, "height", height);
    }

    new_caps = gst_caps_copy (src_caps);

    result_caps_list = g_list_append (result_caps_list, new_caps);
  }

  return result_caps_list;
}

static gboolean
gst_tiovx_multi_scaler_deinit_module (GstTIOVXSimo * simo)
{
  GstTIOVXMultiScaler *self = NULL;
  ScalerObj *multiscaler = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = TRUE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);
  multiscaler = &self->obj;

  /* Delete graph */
  status = app_delete_scaler (multiscaler);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    ret = FALSE;
    goto out;
  }

  status = app_deinit_scaler (multiscaler);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

out:
  return ret;
}
