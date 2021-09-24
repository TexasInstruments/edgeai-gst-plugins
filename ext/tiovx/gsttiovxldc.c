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
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_ldc_module.h"

#define DEFAULT_NUM_CHANNELS 1
#define DEFAULT_TIOVX_SENSOR_ID "SENSOR_SONY_IMX390_UB953_D3"
/* Properties definition */
enum
{
  PROP_0,
  PROP_DCC_CONFIG_FILE,
  PROP_SENSOR_ID,
};

/* Formats definition */
#define TIOVX_LDC_SUPPORTED_FORMATS_SRC "{ GRAY8, GRAY16_LE, NV12, YUYV, UYVY }"
#define TIOVX_LDC_SUPPORTED_FORMATS_SINK "{ GRAY8, GRAY16_LE, NV12, YUYV, UYVY }"
#define TIOVX_LDC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_LDC_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TIOVX_LDC_STATIC_CAPS_SRC \
  "video/x-raw, "							\
  "format = (string) " TIOVX_LDC_SUPPORTED_FORMATS_SRC ", "					\
  "width = " TIOVX_LDC_SUPPORTED_WIDTH ", "					\
  "height = " TIOVX_LDC_SUPPORTED_HEIGHT ", "					\
  "framerate = " GST_VIDEO_FPS_RANGE

/* Sink caps */
#define TIOVX_LDC_STATIC_CAPS_SINK \
  "video/x-raw, "							\
  "format = (string) " TIOVX_LDC_SUPPORTED_FORMATS_SINK ", "					\
  "width = " TIOVX_LDC_SUPPORTED_WIDTH ", "					\
  "height = " TIOVX_LDC_SUPPORTED_HEIGHT ", "					\
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
  gchar *sensor_id;
  TIOVXLDCModuleObj obj;
  SensorObj sensorObj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_ldc_debug);
#define GST_CAT_DEFAULT gst_tiovx_ldc_debug

#define gst_tiovx_ldc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXLDC, gst_tiovx_ldc,
    GST_TIOVX_SIMO_TYPE, GST_DEBUG_CATEGORY_INIT (gst_tiovx_ldc_debug,
        "tiovxldc", 0, "debug category for the tiovxldc element"););

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
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    vx_reference * input_ref, vx_reference ** output_refs);

static gboolean gst_tiovx_ldc_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static GstCaps *gst_tiovx_ldc_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list);

static GstCaps *gst_tiovx_ldc_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps);

static GList *gst_tiovx_ldc_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean
gst_tiovx_ldc_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction);

static gboolean gst_tiovx_ldc_deinit_module (GstTIOVXSimo * simo);

static gboolean
gst_tiovx_ldc_set_dcc_file (GstTIOVXLDC * src, const gchar * location);

/* Initialize the plugin's class */
static void
gst_tiovx_ldc_class_init (GstTIOVXLDCClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSimoClass *gsttiovxsimo_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsimo_class = GST_TIOVX_SIMO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX LDC",
      "Filter",
      "Lens Distortion Correction using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_tiovx_ldc_set_property;
  gobject_class->get_property = gst_tiovx_ldc_get_property;

  g_object_class_install_property (gobject_class, PROP_DCC_CONFIG_FILE,
      g_param_spec_string ("dcc-file", "DCC File",
          "TIOVX DCC configuration binary file to be used by this element",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SENSOR_ID,
      g_param_spec_string ("sensor-id", "Sensor ID",
          "TIOVX Sensor identifier",
          DEFAULT_TIOVX_SENSOR_ID,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  gsttiovxsimo_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_init_module);

  gsttiovxsimo_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_configure_module);

  gsttiovxsimo_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_node_info);

  gsttiovxsimo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_create_graph);

  gsttiovxsimo_class->get_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_sink_caps);

  gsttiovxsimo_class->get_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_src_caps);

  gsttiovxsimo_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_fixate_caps);

  gsttiovxsimo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_deinit_module);

  gsttiovxsimo_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_compare_caps);
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_ldc_init (GstTIOVXLDC * self)
{
  self->dcc_config_file = NULL;
  self->sensor_id = NULL;
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
    case PROP_SENSOR_ID:
      g_value_set_string (value, self->sensor_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_ldc_set_dcc_file (GstTIOVXLDC * src, const gchar * location)
{
  GstState state;

  /* the element must be stopped in order to do this */
  state = GST_STATE (src);
  if (state != GST_STATE_READY && state != GST_STATE_NULL) {
    goto wrong_state;
  }

  g_free (src->dcc_config_file);

  /* clear the filename if we get a NULL */
  if (location == NULL) {
    src->dcc_config_file = NULL;
  } else {
    src->dcc_config_file = g_strdup (location);
  }

  return TRUE;

  /* ERROR */
wrong_state:
  GST_WARNING_OBJECT (src,
      "Changing the `dcc-file' property on tiovxldc when a file is open is not supported.");
  return FALSE;
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
  gboolean ret = TRUE;
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
  tiovx_querry_sensor (sensorObj);
  tiovx_init_sensor (sensorObj, self->sensor_id);
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
    ret = FALSE;
    goto out;
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
    ret = FALSE;
    goto out;
  }

  ldc->output0.bufq_depth = DEFAULT_NUM_CHANNELS;
  ldc->output0.color_format = gst_format_to_vx_format (out_info.finfo->format);
  ldc->output0.width = GST_VIDEO_INFO_WIDTH (&out_info);
  ldc->output0.height = GST_VIDEO_INFO_HEIGHT (&out_info);
  ldc->output0.graph_parameter_index = 0;

  /* Initialize modules */
  GST_INFO_OBJECT (self, "Initializing ldc object");
  status = tiovx_ldc_module_init (context, ldc, sensorObj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    ret = FALSE;
  }
out:
  return ret;
}

static gboolean
gst_tiovx_ldc_configure_module (GstTIOVXSimo * simo)
{
  return FALSE;
}

static gboolean
gst_tiovx_ldc_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    vx_reference * input_ref, vx_reference ** output_refs)
{
  return FALSE;
}

static gboolean
gst_tiovx_ldc_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph)
{
  return FALSE;
}

static GstCaps *
gst_tiovx_ldc_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list)
{
  return NULL;
}

static GstCaps *
gst_tiovx_ldc_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps)
{
  return NULL;
}

static GList *
gst_tiovx_ldc_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list)
{
  return NULL;
}

static gboolean
gst_tiovx_ldc_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  return FALSE;
}

static gboolean
gst_tiovx_ldc_deinit_module (GstTIOVXSimo * simo)
{
  return FALSE;
}
