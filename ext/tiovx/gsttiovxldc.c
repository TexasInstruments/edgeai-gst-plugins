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

#include "gsttiovxldc.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_ldc_module.h"

#define DEFAULT_NUM_CHANNELS 1
#define GST_TYPE_TIOVX_LDC_TARGET (gst_tiovx_ldc_target_get_type())
#define DEFAULT_TIOVX_LDC_TARGET TIVX_TARGET_VPAC_LDC_ID

static const int input_param_id = 6;
static const int output_param_id_start = 7;

/* Properties definition */
enum
{
  PROP_0,
  PROP_DCC_CONFIG_FILE,
  PROP_SENSOR_NAME,
  PROP_TARGET,
};

/* Target definition */
enum
{
  TIVX_TARGET_VPAC_LDC_ID = 0,
};

static GType
gst_tiovx_ldc_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_LDC_ID, "VPAC LDC1", TIVX_TARGET_VPAC_LDC1},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXLDCTarget", targets);
  }
  return target_type;
}

/* Formats definition */
#define TIOVX_LDC_SUPPORTED_FORMATS_SRC "{ GRAY8, GRAY16_LE, NV12, UYVY }"
#define TIOVX_LDC_SUPPORTED_FORMATS_SINK "{ GRAY8, GRAY16_LE, NV12, UYVY }"
#define TIOVX_LDC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_LDC_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TIOVX_LDC_STATIC_CAPS_SRC 				\
  "video/x-raw, "						\
  "format = (string) " TIOVX_LDC_SUPPORTED_FORMATS_SRC ", "	\
  "width = " TIOVX_LDC_SUPPORTED_WIDTH ", "			\
  "height = " TIOVX_LDC_SUPPORTED_HEIGHT ", "			\
  "framerate = " GST_VIDEO_FPS_RANGE

/* Sink caps */
#define TIOVX_LDC_STATIC_CAPS_SINK 				\
  "video/x-raw, "						\
  "format = (string) " TIOVX_LDC_SUPPORTED_FORMATS_SINK ", "	\
  "width = " TIOVX_LDC_SUPPORTED_WIDTH ", "			\
  "height = " TIOVX_LDC_SUPPORTED_HEIGHT ", "			\
  "framerate = " GST_VIDEO_FPS_RANGE

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_LDC_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_LDC_STATIC_CAPS_SRC)
    );

struct _GstTIOVXLDC
{
  GstTIOVXSimo element;
  gint target_id;
  gchar *dcc_config_file;
  gchar *sensor_name;
  gchar *uri;
  TIOVXLDCModuleObj obj;
  SensorObj sensorObj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_ldc_debug);
#define GST_CAT_DEFAULT gst_tiovx_ldc_debug

#define gst_tiovx_ldc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXLDC, gst_tiovx_ldc,
    GST_TYPE_TIOVX_SIMO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_ldc_debug,
        "tiovxldc", 0, "debug category for the tiovxldc element");
    );

static void
gst_tiovx_ldc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_ldc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tiovx_ldc_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean gst_tiovx_ldc_configure_module (GstTIOVXSimo * simo);

static gboolean gst_tiovx_ldc_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads);

static gboolean gst_tiovx_ldc_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static gboolean
gst_tiovx_ldc_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction);

static gboolean gst_tiovx_ldc_deinit_module (GstTIOVXSimo * simo);

static GList *gst_tiovx_ldc_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list);

static void gst_tiovx_ldc_finalize (GObject * obj);

static const gchar *target_id_to_target_name (gint target_id);

static gboolean
gst_tiovx_ldc_set_dcc_file (GstTIOVXLDC * src, const gchar * location);

/* Initialize the plugin's class */
static void
gst_tiovx_ldc_class_init (GstTIOVXLDCClass * klass)
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
      "TIOVX LDC",
      "Filter",
      "Lens Distortion Correction using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  src_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, src_temp);

  sink_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, sink_temp);

  gobject_class->set_property = gst_tiovx_ldc_set_property;
  gobject_class->get_property = gst_tiovx_ldc_get_property;

  g_object_class_install_property (gobject_class, PROP_DCC_CONFIG_FILE,
      g_param_spec_string ("dcc-file", "DCC File",
          "TIOVX DCC configuration binary file to be used by this element",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SENSOR_NAME,
      g_param_spec_string ("sensor-name", "Sensor Name",
          "TIOVX camera sensor name",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_LDC_TARGET,
          DEFAULT_TIOVX_LDC_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gsttiovxsimo_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_init_module);

  gsttiovxsimo_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_configure_module);

  gsttiovxsimo_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_node_info);

  gsttiovxsimo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_create_graph);

  gsttiovxsimo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_deinit_module);

  gsttiovxsimo_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_compare_caps);

  gsttiovxsimo_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_fixate_caps);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_ldc_finalize);

}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_ldc_init (GstTIOVXLDC * self)
{
  self->dcc_config_file = NULL;
  self->sensor_name = NULL;
  self->uri = NULL;
  self->target_id = 0;
}

static void
gst_tiovx_ldc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXLDC *self = GST_TIOVX_LDC (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_CONFIG_FILE:
      gst_tiovx_ldc_set_dcc_file (self, g_value_get_string (value));
      break;
    case PROP_SENSOR_NAME:
      g_free (self->sensor_name);
      self->sensor_name = g_value_dup_string (value);
      break;
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_ldc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXLDC *self = GST_TIOVX_LDC (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_CONFIG_FILE:
      g_value_set_string (value, self->dcc_config_file);
      break;
    case PROP_SENSOR_NAME:
      g_value_set_string (value, self->sensor_name);
      break;
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_ldc_set_dcc_file (GstTIOVXLDC * self, const gchar * location)
{
  g_free (self->dcc_config_file);
  g_free (self->uri);

  /* clear the filename if we get a NULL */
  if (location == NULL) {
    self->dcc_config_file = NULL;
    self->uri = NULL;
  } else {
    self->dcc_config_file = g_strdup (location);
    self->uri = gst_filename_to_uri (location, NULL);
  }

  return TRUE;
}

static gboolean
gst_tiovx_ldc_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list)
{
  GstTIOVXLDC *self = NULL;
  TIOVXLDCModuleObj *ldc = NULL;
  GstCaps *src_caps = NULL;
  SensorObj *sensorObj = NULL;
  GstVideoInfo in_info = { };
  GstVideoInfo out_info = { };
  gboolean ret = FALSE;
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);
  g_return_val_if_fail (sink_caps, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  self = GST_TIOVX_LDC (simo);

  ldc = &self->obj;
  sensorObj = &self->sensorObj;

  /* Initialize general parameters */
  status = tiovx_querry_sensor (sensorObj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "tiovx query sensor error: %d", status);
    goto out;
  }

  status = tiovx_init_sensor (sensorObj, self->sensor_name);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "tiovx init sensor error: %d", status);
    goto deinit_sensor;
  }

  ldc->ldc_mode = TIOVX_MODULE_LDC_OP_MODE_DCC_DATA;
  ldc->en_output1 = 0;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  snprintf (ldc->dcc_config_file_path, TIVX_FILEIO_FILE_PATH_LENGTH, "%s",
      self->dcc_config_file);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  /* Initialize the input parameters */
  if (!gst_video_info_from_caps (&in_info, sink_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
        GST_PTR_FORMAT, sink_caps);
    goto deinit_sensor;
  }

  ldc->input.bufq_depth = DEFAULT_NUM_CHANNELS;
  ldc->input.color_format = gst_format_to_vx_format (in_info.finfo->format);
  ldc->input.width = GST_VIDEO_INFO_WIDTH (&in_info);
  ldc->input.height = GST_VIDEO_INFO_HEIGHT (&in_info);
  ldc->input.graph_parameter_index = 0;

  GST_INFO_OBJECT (self,
      "Input parameters: \n  Width: %d \n  Height: %d \n  Pool size: %d",
      ldc->input.width, ldc->input.height, ldc->input.bufq_depth);

  /* Initialize the output parameters */
  src_caps = (GstCaps *) src_caps_list->data;
  if (!gst_video_info_from_caps (&out_info, src_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
        GST_PTR_FORMAT, src_caps);
    goto deinit_sensor;
  }

  ldc->output0.bufq_depth = DEFAULT_NUM_CHANNELS;
  ldc->output0.color_format = gst_format_to_vx_format (out_info.finfo->format);
  ldc->output0.width = GST_VIDEO_INFO_WIDTH (&out_info);
  ldc->output0.height = GST_VIDEO_INFO_HEIGHT (&out_info);
  ldc->output0.graph_parameter_index = 1;

  /* Initialize modules */
  GST_INFO_OBJECT (self, "Initializing ldc object");
  status = tiovx_ldc_module_init (context, ldc, sensorObj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto deinit_sensor;
  }

  ret = TRUE;
  goto out;

deinit_sensor:
  tiovx_deinit_sensor (sensorObj);
out:
  return ret;
}

static gboolean
gst_tiovx_ldc_configure_module (GstTIOVXSimo * simo)
{
  return TRUE;
}

static gboolean
gst_tiovx_ldc_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads)
{
  GstTIOVXLDC *self = NULL;
  GstTIOVXPad *src_pad = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);

  self = GST_TIOVX_LDC (simo);

  /* Set input parameters */
  gst_tiovx_pad_set_params (sink_pad,
      (vx_reference) self->obj.input.image_handle[0],
      self->obj.input.graph_parameter_index, input_param_id);

  src_pad = (GstTIOVXPad *) src_pads->data;
  gst_tiovx_pad_set_params (src_pad,
      (vx_reference) self->obj.output0.image_handle[0],
      self->obj.output0.graph_parameter_index, output_param_id_start);

  *node = self->obj.node;

  return TRUE;
}

static gboolean
gst_tiovx_ldc_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph)
{
  GstTIOVXLDC *self = NULL;
  TIOVXLDCModuleObj *ldc = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = TRUE;
  const gchar *target = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_LDC (simo);

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  ldc = &self->obj;

  GST_DEBUG_OBJECT (self, "Creating ldc graph");
  status = tiovx_ldc_module_create (graph, ldc, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Finished creating ldc graph");

out:
  return ret;
}

static gboolean
gst_tiovx_ldc_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
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

  if ((video_info1.width == video_info2.width) &&
      (video_info1.height == video_info2.height) &&
      (video_info1.finfo->format == video_info2.finfo->format)
      ) {
    ret = TRUE;
  }

out:
  return ret;
}

static gboolean
gst_tiovx_ldc_deinit_module (GstTIOVXSimo * simo)
{
  GstTIOVXLDC *self = NULL;
  TIOVXLDCModuleObj *ldc = NULL;
  gboolean ret = TRUE;
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_LDC (simo);
  ldc = &self->obj;

  /* Delete graph */
  status = tiovx_ldc_module_delete (ldc);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    ret = FALSE;
    goto out;
  }

  tiovx_deinit_sensor (ldc->sensorObj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  status = tiovx_ldc_module_deinit (ldc);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    ret = FALSE;
    goto out;
  }
out:
  return ret;
}

static GList *
gst_tiovx_ldc_fixate_caps (GstTIOVXSimo * simo,
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


static const gchar *
target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_ldc_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static void
gst_tiovx_ldc_finalize (GObject * obj)
{

  GstTIOVXLDC *self = GST_TIOVX_LDC (obj);

  GST_LOG_OBJECT (self, "finalize");

  g_free (self->dcc_config_file);
  g_free (self->uri);
  g_free (self->sensor_name);

  G_OBJECT_CLASS (gst_tiovx_ldc_parent_class)->finalize (obj);
}
