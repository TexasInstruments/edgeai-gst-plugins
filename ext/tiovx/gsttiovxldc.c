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

#define GST_TYPE_TIOVX_LDC_TARGET (gst_tiovx_ldc_target_get_type())
#define DEFAULT_TIOVX_LDC_TARGET TIVX_TARGET_VPAC_LDC_ID
#define MAX_SRC_PADS 1
#define INPUT_PARAM_ID 6
#define OUTPUT_PARAM_ID_START 7
#define DEFAULT_LDC_TABLE_WIDTH 1920
#define DEFAULT_LDC_TABLE_HEIGHT 1080
#define DEFAULT_LDC_DS_FACTOR 2
#define DEFAULT_LDC_BLOCK_WIDTH 64
#define DEFAULT_LDC_BLOCK_HEIGHT 64
#define DEFAULT_LDC_PIXEL_PAD 1
#define DEFAULT_LDC_INIT_X 0
#define DEFAULT_LDC_INIT_Y 0

/* Properties definition */
enum
{
  PROP_0,
  PROP_DCC_CONFIG_FILE,
  PROP_LUT_FILE,
  PROP_SENSOR_NAME,
  PROP_LDC_TABLE_WIDTH,
  PROP_LDC_TABLE_HEIGHT,
  PROP_LDC_DS_FACTOR,
  PROP_LDC_BLOCK_WIDTH,
  PROP_LDC_BLOCK_HEIGHT,
  PROP_LDC_PIXEL_PAD,
  PROP_LDC_INIT_X,
  PROP_LDC_INIT_Y,
  PROP_TARGET,
};

/* Target definition */
enum
{
  TIVX_TARGET_VPAC_LDC_ID = 0,
#if defined(SOC_J784S4)
  TIVX_TARGET_VPAC2_LDC_ID,
#endif
};

static GType
gst_tiovx_ldc_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_LDC_ID, "VPAC LDC1", TIVX_TARGET_VPAC_LDC1},
#if defined(SOC_J784S4)
    {TIVX_TARGET_VPAC2_LDC_ID, "VPAC LDC1", TIVX_TARGET_VPAC2_LDC1},
#endif
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXLDCTarget", targets);
  }
  return target_type;
}

/* Formats definition */
#define TIOVX_LDC_SUPPORTED_FORMATS "{ GRAY8, GRAY16_LE, NV12, UYVY }"
#define TIOVX_LDC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_LDC_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_LDC_SUPPORTED_CHANNELS "[1 , 16]"

/* Supported caps, the same at the input and output */
#define TIOVX_LDC_STATIC_SUPPORTED_CAPS                 \
  "video/x-raw, "                                       \
  "format = (string) " TIOVX_LDC_SUPPORTED_FORMATS ", " \
  "width = " TIOVX_LDC_SUPPORTED_WIDTH ", "             \
  "height = " TIOVX_LDC_SUPPORTED_HEIGHT                \
  "; "                                                  \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "  \
  "format = (string) " TIOVX_LDC_SUPPORTED_FORMATS ", " \
  "width = " TIOVX_LDC_SUPPORTED_WIDTH ", "             \
  "height = " TIOVX_LDC_SUPPORTED_HEIGHT ", "           \
  "num-channels = " TIOVX_LDC_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_LDC_STATIC_SUPPORTED_CAPS)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_LDC_STATIC_SUPPORTED_CAPS)
    );

struct _GstTIOVXLDC
{
  GstTIOVXSimo element;
  gint target_id;
  gchar *dcc_config_file;
  gchar *lut_file;
  gchar *sensor_name;
  guint ldc_table_width;
  guint ldc_table_height;
  guint ldc_ds_factor;
  guint out_block_width;
  guint out_block_height;
  guint pixel_pad;
  guint ldc_init_x;
  guint ldc_init_y;
  TIOVXLDCModuleObj obj;
  SensorObj sensorObj;
  gint num_src_pads;
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
    GstCaps * sink_caps, GList * src_caps_list, guint num_channels);

static gboolean gst_tiovx_ldc_configure_module (GstTIOVXSimo * simo);

static gboolean gst_tiovx_ldc_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    GList ** queueable_objects);

static gboolean gst_tiovx_ldc_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static gboolean
gst_tiovx_ldc_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction);

static gboolean gst_tiovx_ldc_deinit_module (GstTIOVXSimo * simo);

static GList *gst_tiovx_ldc_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list);

static GstCaps *gst_tiovx_ldc_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps, GstTIOVXPad *src_pad);

static GstCaps *gst_tiovx_ldc_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list, GList *src_pads);

static void gst_tiovx_ldc_finalize (GObject * obj);

static const gchar *target_id_to_target_name (gint target_id);

static GstPad *gst_tiovx_ldc_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);

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
      "Filter/Effect/Video",
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

  g_object_class_install_property (gobject_class, PROP_LUT_FILE,
      g_param_spec_string ("lut-file", "LUT File",
          "TIOVX LUT file to cofigure LDC via mesh image",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SENSOR_NAME,
      g_param_spec_string ("sensor-name", "Sensor Name",
          "TIOVX camera sensor name. Below are the supported sensors\n"
          "                              SENSOR_SONY_IMX390_UB953_D3\n"
          "                              SENSOR_ONSEMI_AR0820_UB953_LI\n"
          "                              SENSOR_ONSEMI_AR0233_UB953_MARS\n"
          "                              SENSOR_SONY_IMX219_RPI\n"
          "                              SENSOR_OV2312_UB953_LI",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_LDC_TABLE_WIDTH,
      g_param_spec_uint ("ldc-table-width", "LDC Table width",
          "LDC Table width of LUT", 0, G_MAXUINT,
          DEFAULT_LDC_TABLE_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LDC_TABLE_HEIGHT,
      g_param_spec_uint ("ldc-table-height", "LDC Table height",
          "LDC Table width of LUT", 0, G_MAXUINT,
          DEFAULT_LDC_TABLE_HEIGHT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LDC_DS_FACTOR,
      g_param_spec_uint ("ldc-ds-factor", "LDC Downscale factor",
          "LDC Downscaling factor to used for LUT, power of 2", 0, 7,
          DEFAULT_LDC_DS_FACTOR, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LDC_BLOCK_WIDTH,
      g_param_spec_uint ("out-block-width", "LDC out block width",
          "LDC Output block width (must be multiple of 8)", 8, 255,
          DEFAULT_LDC_BLOCK_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LDC_BLOCK_HEIGHT,
      g_param_spec_uint ("out-block-height", "LDC out block height",
          "LDC Output block height (must be multiple of 8)", 8, 255,
          DEFAULT_LDC_BLOCK_HEIGHT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LDC_PIXEL_PAD,
      g_param_spec_uint ("pixel-pad", "LDC pixel padding",
          "LDC Pixel Padding", 0, 15,
          DEFAULT_LDC_PIXEL_PAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LDC_INIT_X,
      g_param_spec_uint ("ldc-init-x", "LDC start x coordinate",
          "LDC Output starting x-coordinate (must be multiple of 8)", 0, 8191,
          DEFAULT_LDC_INIT_X, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LDC_INIT_Y,
      g_param_spec_uint ("ldc-init-y", "LDC start y coordinate",
          "LDC Output starting y-coordinate (must be multiple of 8)", 0, 8191,
          DEFAULT_LDC_INIT_X, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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
  gsttiovxsimo_class->get_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_src_caps);
  gsttiovxsimo_class->get_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_sink_caps);
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_request_new_pad);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_ldc_finalize);

}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_ldc_init (GstTIOVXLDC * self)
{
  self->dcc_config_file = NULL;
  self->lut_file = NULL;
  self->sensor_name = NULL;
  self->ldc_table_width = DEFAULT_LDC_TABLE_WIDTH;
  self->ldc_table_height = DEFAULT_LDC_TABLE_HEIGHT;
  self->ldc_ds_factor = DEFAULT_LDC_DS_FACTOR;
  self->out_block_width = DEFAULT_LDC_BLOCK_WIDTH;
  self->out_block_height = DEFAULT_LDC_BLOCK_HEIGHT;
  self->pixel_pad = DEFAULT_LDC_PIXEL_PAD;
  self->ldc_init_x = DEFAULT_LDC_INIT_X;
  self->ldc_init_y = DEFAULT_LDC_INIT_Y;
  self->target_id = 0;
  self->num_src_pads = 0;
  memset (&self->obj, 0, sizeof (self->obj));
  memset (&self->sensorObj, 0, sizeof (self->sensorObj));
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
      g_free (self->dcc_config_file);
      self->dcc_config_file = g_value_dup_string (value);
      break;
    case PROP_LUT_FILE:
      g_free (self->lut_file);
      self->lut_file = g_value_dup_string (value);
      break;
    case PROP_SENSOR_NAME:
      g_free (self->sensor_name);
      self->sensor_name = g_value_dup_string (value);
      break;
    case PROP_LDC_TABLE_WIDTH:
      self->ldc_table_width = g_value_get_uint (value);
      break;
    case PROP_LDC_TABLE_HEIGHT:
      self->ldc_table_height = g_value_get_uint (value);
      break;
    case PROP_LDC_DS_FACTOR:
      self->ldc_ds_factor = g_value_get_uint (value);
      break;
    case PROP_LDC_BLOCK_WIDTH:
      self->out_block_width = g_value_get_uint (value);
      break;
    case PROP_LDC_BLOCK_HEIGHT:
      self->out_block_height = g_value_get_uint (value);
      break;
    case PROP_LDC_PIXEL_PAD:
      self->pixel_pad = g_value_get_uint (value);
      break;
    case PROP_LDC_INIT_X:
      self->ldc_init_x = g_value_get_uint (value);
      break;
    case PROP_LDC_INIT_Y:
      self->ldc_init_y = g_value_get_uint (value);
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
    case PROP_LUT_FILE:
      g_value_set_string (value, self->lut_file);
      break;
    case PROP_SENSOR_NAME:
      g_value_set_string (value, self->sensor_name);
      break;
    case PROP_LDC_TABLE_WIDTH:
      g_value_set_uint (value, self->ldc_table_width);
      break;
    case PROP_LDC_TABLE_HEIGHT:
      g_value_set_uint (value, self->ldc_table_height);
      break;
    case PROP_LDC_DS_FACTOR:
      g_value_set_uint (value, self->ldc_ds_factor);
      break;
    case PROP_LDC_BLOCK_WIDTH:
      g_value_set_uint (value, self->out_block_width);
      break;
    case PROP_LDC_BLOCK_HEIGHT:
      g_value_set_uint (value, self->out_block_height);
      break;
    case PROP_LDC_PIXEL_PAD:
      g_value_set_uint (value, self->pixel_pad);
      break;
    case PROP_LDC_INIT_X:
      g_value_set_uint (value, self->ldc_init_x);
      break;
    case PROP_LDC_INIT_Y:
      g_value_set_uint (value, self->ldc_init_y);
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
gst_tiovx_ldc_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list, guint num_channels)
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
  ldc->table_width = self->ldc_table_width;
  ldc->table_height = self->ldc_table_height;
  ldc->ds_factor = self->ldc_ds_factor;
  ldc->out_block_width = self->out_block_width;
  ldc->out_block_height = self->out_block_height;
  ldc->pixel_pad = self->pixel_pad;
  ldc->init_x = self->ldc_init_x;
  ldc->init_y = self->ldc_init_y;

  status = tiovx_querry_sensor (sensorObj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "tiovx query sensor error: %d", status);
    goto out;
  }

  GST_OBJECT_LOCK (GST_OBJECT (self));
  status = tiovx_init_sensor (sensorObj, self->sensor_name);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "tiovx init sensor error: %d", status);
    goto out;
  }

  if (NULL != self->dcc_config_file)
    ldc->ldc_mode = TIOVX_MODULE_LDC_OP_MODE_DCC_DATA;
  else if (NULL != self->lut_file)
    ldc->ldc_mode = TIOVX_MODULE_LDC_OP_MODE_MESH_IMAGE;
  else {
    GST_ERROR_OBJECT (self, "No DCC config/LUT file");
    goto out;
  }

  ldc->en_output1 = 0;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  snprintf (ldc->dcc_config_file_path, TIVX_FILEIO_FILE_PATH_LENGTH, "%s",
      self->dcc_config_file);
  snprintf (ldc->lut_file_path, TIVX_FILEIO_FILE_PATH_LENGTH, "%s",
      self->lut_file);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  /* Initialize the input parameters */
  if (!gst_video_info_from_caps (&in_info, sink_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
        GST_PTR_FORMAT, sink_caps);
    goto deinit_sensor;
  }

  /* LDC uses sensorObj's num_cameras_enabled instead of a num_channels variable */
  sensorObj->num_cameras_enabled = num_channels;

  ldc->input.bufq_depth = 1;
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

  ldc->output0.bufq_depth = 1;
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
  GstTIOVXLDC *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = TRUE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_LDC (simo);

  GST_DEBUG_OBJECT (self, "Release buffers for ldc module");
  status = tiovx_ldc_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure release buffer failed with error: %d", status);
    ret = FALSE;
  }

  return ret;
}

static gboolean
gst_tiovx_ldc_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    GList ** queueable_objects)
{
  GstTIOVXLDC *self = NULL;
  GstTIOVXPad *src_pad = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);

  self = GST_TIOVX_LDC (simo);

  /* Set input parameters */
  gst_tiovx_pad_set_params (sink_pad,
      self->obj.input.arr[0], (vx_reference) self->obj.input.image_handle[0],
      self->obj.input.graph_parameter_index, INPUT_PARAM_ID);

  src_pad = (GstTIOVXPad *) src_pads->data;
  gst_tiovx_pad_set_params (src_pad,
      self->obj.output0.arr[0],
      (vx_reference) self->obj.output0.image_handle[0],
      self->obj.output0.graph_parameter_index, OUTPUT_PARAM_ID_START);

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

  /* TODO: Validate resolution requirements */
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

static GstCaps *
gst_tiovx_ldc_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps, GstTIOVXPad *src_pad)
{
  GstCaps *src_caps = NULL;
  GstCaps *sink_caps_copy = NULL;
  GstCaps *template_caps = NULL;
  gint i = 0;

  g_return_val_if_fail (simo, NULL);
  g_return_val_if_fail (sink_caps, NULL);

  GST_DEBUG_OBJECT (simo,
      "Computing src caps based on sink caps %" GST_PTR_FORMAT " and filter %"
      GST_PTR_FORMAT, sink_caps, filter);

  template_caps = gst_static_pad_template_get_caps (&src_template);

  /* Remove width and height from sink caps structures before intersecting with template */
  sink_caps_copy = gst_caps_copy (sink_caps);
  for (i = 0; i < gst_caps_get_size (sink_caps); i++) {
    GstStructure *st = gst_caps_get_structure (sink_caps_copy, i);
    gst_structure_remove_fields (st, "width", "height", NULL);
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
      "Resulting supported src caps by TIOVX LDC node: %"
      GST_PTR_FORMAT, src_caps);

  return src_caps;
}

static GstCaps *
gst_tiovx_ldc_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list, GList *src_pads)
{
  GstCaps *sink_caps = NULL;
  GstCaps *template_caps = NULL;
  GList *l = NULL;
  gint i = 0;

  g_return_val_if_fail (simo, NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_DEBUG_OBJECT (simo,
      "Computing sink caps based on template caps and filter %"
      GST_PTR_FORMAT, filter);

  template_caps = gst_static_pad_template_get_caps (&sink_template);
  if (filter) {
    sink_caps = gst_caps_intersect (template_caps, filter);
  } else {
    sink_caps = gst_caps_copy (template_caps);
  }
  gst_caps_unref (template_caps);

  for (l = src_caps_list; l != NULL; l = g_list_next (l)) {
    GstCaps *tmp = NULL;
    GstCaps *src_caps = gst_caps_copy ((GstCaps *) l->data);

    for (i = 0; i < gst_caps_get_size (sink_caps); i++) {
      GstStructure *src_st = gst_caps_get_structure (src_caps, i);
      gst_structure_remove_fields (src_st, "width", "height", NULL);
    }

    tmp = gst_caps_intersect (sink_caps, src_caps);
    gst_caps_unref (sink_caps);
    gst_caps_unref (src_caps);
    sink_caps = tmp;
  }

  GST_INFO_OBJECT (simo,
      "Resulting supported sink caps by TIOVX LDC node: %"
      GST_PTR_FORMAT, sink_caps);

  return sink_caps;
}

static GstPad *
gst_tiovx_ldc_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *pad = NULL;
  GstTIOVXLDC *self = GST_TIOVX_LDC (element);

  if (MAX_SRC_PADS > self->num_src_pads) {
    pad =
        GST_ELEMENT_CLASS (gst_tiovx_ldc_parent_class)->request_new_pad
        (element, templ, name, caps);
    self->num_src_pads++;
  } else {
    GST_ERROR_OBJECT (self, "Exceeded number of pads allowed");
  }

  return pad;
}

static void
gst_tiovx_ldc_finalize (GObject * obj)
{

  GstTIOVXLDC *self = GST_TIOVX_LDC (obj);

  GST_LOG_OBJECT (self, "finalize");

  g_free (self->dcc_config_file);
  g_free (self->sensor_name);
  self->dcc_config_file = NULL;
  self->sensor_name = NULL;

  G_OBJECT_CLASS (gst_tiovx_ldc_parent_class)->finalize (obj);
}
