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

#include "gsttiovxisp.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_viss_module.h"

#define DEFAULT_NUM_CHANNELS 1
#define MAX_SUPPORTED_OUTPUTS 1
#define DEFAULT_TIOVX_SENSOR_ID "SENSOR_SONY_IMX390_UB953_D3"

static const int input_param_id = 3;
static const int output2_param_id = 6;
static const int ae_awb_result_param_id = 1;
static const int h3a_stats_param_id = 9;

/* Properties definition */
enum
{
  PROP_0,
  PROP_DCC_CONFIG_FILE,
};

/* Formats definition */
#define TIOVX_ISP_SUPPORTED_FORMATS_SRC "{ GRAY8, GRAY16_LE, NV12, I420 }"
#define TIOVX_ISP_SUPPORTED_FORMATS_SINK "{ bggr, gbrg, grbg, rggb }"
#define TIOVX_ISP_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_ISP_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TIOVX_ISP_STATIC_CAPS_SRC                            \
  "video/x-raw, "                                            \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SRC ", "  \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT ", "                \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Sink caps */
#define TIOVX_ISP_STATIC_CAPS_SINK                           \
  "video/x-bayer, "                                          \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT ", "                \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_ISP_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_ISP_STATIC_CAPS_SRC)
    );

struct _GstTIOVXISP
{
  GstTIOVXSimo element;
  gchar *dcc_config_file;
  gchar *sensor_id;
  SensorObj sensor_obj;
  TIOVXVISSModuleObj viss_obj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_isp_debug);
#define GST_CAT_DEFAULT gst_tiovx_isp_debug

#define gst_tiovx_isp_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXISP, gst_tiovx_isp,
    GST_TYPE_TIOVX_SIMO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_isp_debug,
        "tiovxisp", 0, "debug category for the tiovxisp element"));

static void gst_tiovx_isp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void gst_tiovx_isp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_tiovx_isp_finalize (GObject * obj);

static gboolean gst_tiovx_isp_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean gst_tiovx_isp_configure_module (GstTIOVXSimo * simo);

static gboolean gst_tiovx_isp_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    GList ** queueable_objects);

static gboolean gst_tiovx_isp_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static GstCaps *gst_tiovx_isp_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list);

static GstCaps *gst_tiovx_isp_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps);

static GList *gst_tiovx_isp_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean
gst_tiovx_isp_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction);

static gboolean gst_tiovx_isp_deinit_module (GstTIOVXSimo * simo);

static gboolean
gst_tiovx_isp_set_dcc_file (GstTIOVXISP * src, const gchar * location);

/* Initialize the plugin's class */
static void
gst_tiovx_isp_class_init (GstTIOVXISPClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstTIOVXSimoClass *gsttiovxsimo_class = GST_TIOVX_SIMO_CLASS (klass);
  GstPadTemplate *src_temp = NULL;
  GstPadTemplate *sink_temp = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX ISP",
      "Filter",
      "Image Signal Processing using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  src_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, src_temp);

  sink_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, sink_temp);

  gobject_class->set_property = gst_tiovx_isp_set_property;
  gobject_class->get_property = gst_tiovx_isp_get_property;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_isp_finalize);

  g_object_class_install_property (gobject_class, PROP_DCC_CONFIG_FILE,
      g_param_spec_string ("dcc-file", "DCC File",
          "TIOVX DCC tuning binary file for the given image sensor",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  gsttiovxsimo_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_init_module);

  gsttiovxsimo_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_configure_module);

  gsttiovxsimo_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_get_node_info);

  gsttiovxsimo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_create_graph);

  gsttiovxsimo_class->get_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_get_sink_caps);

  gsttiovxsimo_class->get_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_get_src_caps);

  gsttiovxsimo_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_fixate_caps);

  gsttiovxsimo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_deinit_module);

  gsttiovxsimo_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_compare_caps);
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_isp_init (GstTIOVXISP * self)
{
  self->dcc_config_file = NULL;
  /* TODO: this should be a property */
  self->sensor_id = g_strdup (DEFAULT_TIOVX_SENSOR_ID);
}

static void
gst_tiovx_isp_finalize (GObject * obj)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (obj);

  GST_LOG_OBJECT (self, "finalize");

  /* Free internal strings */
  g_free (self->dcc_config_file);
  self->dcc_config_file = NULL;
  g_free (self->sensor_id);
  self->sensor_id = NULL;

  G_OBJECT_CLASS (gst_tiovx_isp_parent_class)->finalize (obj);
}

static gboolean
gst_tiovx_isp_set_dcc_file (GstTIOVXISP * self, const gchar * location)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (location, FALSE);

  g_free (self->dcc_config_file);

  self->dcc_config_file = g_strdup (location);

  return TRUE;
}

static void
gst_tiovx_isp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_CONFIG_FILE:
      gst_tiovx_isp_set_dcc_file (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_isp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_CONFIG_FILE:
      g_value_set_string (value, self->dcc_config_file);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_isp_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list)
{
  GstTIOVXISP *self = NULL;
  GstVideoInfo in_info = { };
  GstVideoInfo out_info = { };
  gboolean ret = FALSE;
  vx_status status = VX_FAILURE;
  GstCaps *src_caps = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);
  g_return_val_if_fail (sink_caps, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  self = GST_TIOVX_ISP (simo);

  tiovx_querry_sensor (&self->sensor_obj);
  tiovx_init_sensor (&self->sensor_obj, self->sensor_id);

  snprintf (self->viss_obj.dcc_config_file_path, TIVX_FILEIO_FILE_PATH_LENGTH,
      "%s", self->dcc_config_file);

  /* Initialize the input parameters */
  if (!gst_video_info_from_caps (&in_info, sink_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from input caps: %"
        GST_PTR_FORMAT, sink_caps);
    goto out;
  }

  self->viss_obj.input.bufq_depth = DEFAULT_NUM_CHANNELS;
  self->viss_obj.input.params.width = GST_VIDEO_INFO_WIDTH (&in_info);
  self->viss_obj.input.params.height = GST_VIDEO_INFO_HEIGHT (&in_info);
  /* TODO: this information should not be hardcoded and instead be obtained from
   * the query of the sensor
   */
  self->viss_obj.input.params.num_exposures = 1;
  self->viss_obj.input.params.line_interleaved = vx_false_e;
  self->viss_obj.input.params.format[0].pixel_container = TIVX_RAW_IMAGE_16_BIT;
  self->viss_obj.input.params.format[0].msb = 11;
  self->viss_obj.input.params.meta_height_before = 0;
  self->viss_obj.input.params.meta_height_after = 0;

  self->viss_obj.ae_awb_result_bufq_depth = DEFAULT_NUM_CHANNELS;

  GST_INFO_OBJECT (self,
      "Input parameters: \n  Width: %d \n  Height: %d \n  Pool size: %d",
      self->viss_obj.input.params.width, self->viss_obj.input.params.height,
      self->viss_obj.input.bufq_depth);

  /* Initialize the output parameters.
   * TODO: Only output for 12 or 8 bit is enabled, so only output2
   * parameters are specified.
   */
  self->viss_obj.output_select[0] = TIOVX_VISS_MODULE_OUTPUT_NA;
  self->viss_obj.output_select[1] = TIOVX_VISS_MODULE_OUTPUT_NA;
  self->viss_obj.output_select[2] = TIOVX_VISS_MODULE_OUTPUT_EN;
  self->viss_obj.output_select[3] = TIOVX_VISS_MODULE_OUTPUT_NA;
  self->viss_obj.output_select[4] = TIOVX_VISS_MODULE_OUTPUT_NA;

  if (MAX_SUPPORTED_OUTPUTS < g_list_length (src_caps_list)) {
    GST_ERROR_OBJECT (self,
        "This element currently supports just one output: %d", status);
    goto out;
  }

  src_caps = (GstCaps *) src_caps_list->data;
  if (!gst_video_info_from_caps (&out_info, src_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from output caps: %"
        GST_PTR_FORMAT, src_caps);
    goto out;
  }

  self->viss_obj.output2.bufq_depth = DEFAULT_NUM_CHANNELS;
  self->viss_obj.output2.color_format =
      gst_format_to_vx_format (out_info.finfo->format);
  self->viss_obj.output2.width = GST_VIDEO_INFO_WIDTH (&out_info);
  self->viss_obj.output2.height = GST_VIDEO_INFO_HEIGHT (&out_info);

  self->viss_obj.h3a_stats_bufq_depth = DEFAULT_NUM_CHANNELS;

  GST_INFO_OBJECT (self, "Initializing scaler object");
  status = tiovx_viss_module_init (context, &self->viss_obj, &self->sensor_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_isp_configure_module (GstTIOVXSimo * simo)
{
  GstTIOVXISP *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_ISP (simo);

  GST_DEBUG_OBJECT (self, "Release buffer ISP");
  status = tiovx_viss_module_release_buffers (&self->viss_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure release buffer failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_isp_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    GList ** queueable_objects)
{
  GstTIOVXISP *self = NULL;
  GList *l = NULL;
  GstTIOVXSimoQueueable *queueable_object = NULL;
  gint graph_parameter_index = 0;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);

  self = GST_TIOVX_ISP (simo);

  *node = self->viss_obj.node;

  /* ae_awb results & h3a stats aren't input or outputs, these are added as queueable_objects */
  queueable_object =
      GST_TIOVX_SIMO_QUEUEABLE (g_object_new (GST_TYPE_TIOVX_SIMO_QUEUEABLE,
          NULL));
  gst_tiovx_simo_queueable_set_params (queueable_object,
      (vx_reference *) & self->viss_obj.ae_awb_result_handle[0],
      graph_parameter_index, ae_awb_result_param_id);
  graph_parameter_index++;
  *queueable_objects = g_list_append (*queueable_objects, queueable_object);

  queueable_object =
      GST_TIOVX_SIMO_QUEUEABLE (g_object_new (GST_TYPE_TIOVX_SIMO_QUEUEABLE,
          NULL));
  gst_tiovx_simo_queueable_set_params (queueable_object,
      (vx_reference *) & self->viss_obj.h3a_stats_handle[0],
      graph_parameter_index, h3a_stats_param_id);
  graph_parameter_index++;
  *queueable_objects = g_list_append (*queueable_objects, queueable_object);

  /* Set input parameters */
  gst_tiovx_pad_set_params (sink_pad,
      (vx_reference) self->viss_obj.input.image_handle[0],
      graph_parameter_index, input_param_id);
  graph_parameter_index++;

  /* Set output parameters, currently only output2 is supported */
  for (l = src_pads; l != NULL; l = g_list_next (l)) {
    GstTIOVXPad *src_pad = (GstTIOVXPad *) l->data;

    /* Set output parameters */
    gst_tiovx_pad_set_params (src_pad,
        (vx_reference) self->viss_obj.output2.image_handle[0],
        graph_parameter_index, output2_param_id);
    graph_parameter_index++;
  }

  return TRUE;
}

static gboolean
gst_tiovx_isp_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph)
{
  GstTIOVXISP *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_ISP (simo);

  GST_DEBUG_OBJECT (self, "Creating scaler graph");
  /* TODO: target is hardcoded */
  status =
      tiovx_viss_module_create (graph, &self->viss_obj, NULL, NULL,
      TIVX_TARGET_VPAC_VISS1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Finished creating viss graph");

  ret = TRUE;

out:
  return ret;
}

static GstCaps *
gst_tiovx_isp_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list)
{
  GstCaps *sink_caps = NULL;
  GstCaps *template_caps = NULL;
  GList *l = NULL;
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

  /* Input and output dimensions should match, remove format from src caps to intersect */
  for (l = src_caps_list; l != NULL; l = g_list_next (l)) {
    GstCaps *src_caps = gst_caps_copy ((GstCaps *) l->data);
    GstCaps *tmp = NULL;

    for (i = 0; i < gst_caps_get_size (src_caps); i++) {
      GstStructure *st = gst_caps_get_structure (src_caps, i);

      gst_structure_set_name (st, "video/x-bayer");
      gst_structure_remove_fields (st, "format", NULL);
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
gst_tiovx_isp_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps)
{
  GstCaps *src_caps = NULL;
  GstCaps *template_caps = NULL;
  GstCaps *sink_caps_copy = NULL;
  GstStructure *sink_st = NULL;
  gint i = 0;

  g_return_val_if_fail (simo, NULL);
  g_return_val_if_fail (sink_caps, NULL);

  GST_DEBUG_OBJECT (simo,
      "Computing src caps based on sink caps %" GST_PTR_FORMAT " and filter %"
      GST_PTR_FORMAT, sink_caps, filter);

  template_caps = gst_static_pad_template_get_caps (&src_template);

  /* Incoming caps are bayer, we'll change the name to x-raw and drop the format
   * so that we can intersect
   */
  sink_caps_copy = gst_caps_copy (sink_caps);
  for (i = 0; i < gst_caps_get_size (sink_caps_copy); i++) {
    sink_st = gst_caps_get_structure (sink_caps_copy, i);
    gst_structure_set_name (sink_st, "video/x-raw");
    gst_structure_remove_fields (sink_st, "format", NULL);
  }

  src_caps = gst_caps_intersect (template_caps, sink_caps_copy);

  gst_caps_unref (template_caps);
  gst_caps_unref (sink_caps_copy);

  if (filter) {
    GstCaps *tmp = src_caps;
    src_caps = gst_caps_intersect (src_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_INFO_OBJECT (simo,
      "Resulting supported src caps by TIOVX isp node: %"
      GST_PTR_FORMAT, src_caps);

  return src_caps;
}

static GList *
gst_tiovx_isp_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list)
{
  GList *l = NULL;
  GstStructure *sink_structure = NULL;
  GList *result_caps_list = NULL;
  gint width = 0;
  gint height = 0;
  const gchar *format = NULL;

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
gst_tiovx_isp_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  return FALSE;
}

static gboolean
gst_tiovx_isp_deinit_module (GstTIOVXSimo * simo)
{
  GstTIOVXISP *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_ISP (simo);

  /* Delete graph */
  status = tiovx_viss_module_delete (&self->viss_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    goto out;
  }

  status = tiovx_viss_module_deinit (&self->viss_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}
