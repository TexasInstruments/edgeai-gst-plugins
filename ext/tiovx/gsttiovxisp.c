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
#include "gst-libs/gst/tiovx/gsttiovxallocator.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxqueueableobject.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_viss_module.h"
#include "ti_2a_wrapper.h"

#include <stdio.h>

static const guint default_num_channels = 1;
static const guint max_supported_outputs = 1;
static const char default_tiovx_sensor_id[] = "SENSOR_SONY_IMX219_RPI";
#define GST_TYPE_TIOVX_ISP_TARGET (gst_tiovx_isp_target_get_type())
#define DEFAULT_TIOVX_ISP_TARGET TIVX_TARGET_VPAC_VISS1_ID

static const gint min_num_exposures = 1;
static const gint default_num_exposures = 1;
static const gint max_num_exposures = 4;

static const gint min_format_msb = 1;
static const gint default_format_msb = 7;
static const gint max_format_msb = 16;

static const gint min_meta_height_before = 0;
static const gint default_meta_height_before = 0;
static const gint max_meta_height_before = 8192;

static const gint min_meta_height_after = 0;
static const gint default_meta_height_after = 0;
static const gint max_meta_height_after = 8192;

static const gboolean default_lines_interleaved = FALSE;

static const int input_param_id = 3;
static const int output2_param_id = 6;
static const int ae_awb_result_param_id = 1;
static const int h3a_stats_param_id = 9;

static const guint8 default_h3a_aew_af_desc_status = 1;

static const gboolean default_ae_disabled = FALSE;
static const gboolean default_awb_disabled = FALSE;
static const guint default_sensor_dcc_id = 219;
static const guint default_ae_num_skip_frames = 9;
static const guint default_awb_num_skip_frames = 9;
static const guint default_sensor_img_format = 0;       /* BAYER = 0x0, Rest unsupported */
static const guint default_analog_gain = 1000;
static const guint default_color_temperature = 5000;
static const guint default_exposure_time = 33333;

enum
{
  TI_2A_WRAPPER_SENSOR_IMG_PHASE_BGGR = 0,
  TI_2A_WRAPPER_SENSOR_IMG_PHASE_GBRG = 1,
  TI_2A_WRAPPER_SENSOR_IMG_PHASE_GRBG = 2,
  TI_2A_WRAPPER_SENSOR_IMG_PHASE_RGGB = 3,
};

/* Properties definition */
enum
{
  PROP_0,
  PROP_DCC_ISP_CONFIG_FILE,
  PROP_DCC_2A_CONFIG_FILE,
  PROP_SENSOR_ID,
  PROP_TARGET,
  PROP_NUM_EXPOSURES,
  PROP_LINE_INTERLEAVED,
  PROP_FORMAT_MSB,
  PROP_META_HEIGHT_BEFORE,
  PROP_META_HEIGHT_AFTER,
  PROP_AE_DISABLED,
  PROP_AWB_DISABLED,
  PROP_SENSOR_DCC_ID,
  PROP_AE_NUM_SKIP_FRAMES,
  PROP_AWB_NUM_SKIP_FRAMES,
  PROP_ANALOG_GAIN,
  PROP_COLOR_TEMPERATURE,
  PROP_EXPOSURE_TIME,
};

/* Target definition */
enum
{
  TIVX_TARGET_VPAC_VISS1_ID = 0,
};

static GType
gst_tiovx_isp_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_VISS1_ID, "VPAC VISS1", TIVX_TARGET_VPAC_VISS1},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXISPTarget", targets);
  }
  return target_type;
}

/* Formats definition */
#define TIOVX_ISP_SUPPORTED_FORMATS_SRC "{NV12}"
#define TIOVX_ISP_SUPPORTED_FORMATS_SINK "{ bggr, gbrg, grbg, rggb, bggr16, gbrg16, grbg16, rggb16 }"
#define TIOVX_ISP_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_ISP_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TIOVX_ISP_STATIC_CAPS_SRC                            \
  "video/x-raw, "                                            \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SRC ", "  \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT

/* Sink caps */
#define TIOVX_ISP_STATIC_CAPS_SINK                           \
  "video/x-bayer, "                                          \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT

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
  gchar *dcc_isp_config_file;
  gchar *dcc_2a_config_file;
  gchar *sensor_id;
  gint target_id;
  SensorObj sensor_obj;

  gint num_exposures;
  gboolean line_interleaved;
  gint format_msb;
  gint meta_height_before;
  gint meta_height_after;

  /* TI_2A_wrapper settings */
  gboolean ae_disabled;
  gboolean awb_disabled;
  guint sensor_dcc_id;
  guint ae_num_skip_frames;
  guint awb_num_skip_frames;
  guint analog_gain;
  guint color_temperature;
  guint exposure_time;

  GstTIOVXAllocator *user_data_allocator;

  GstMemory *aewb_memory;
  GstMemory *h3a_stats_memory;

  TIOVXVISSModuleObj viss_obj;

  TI_2A_wrapper ti_2a_wrapper;
  sensor_config_get sensor_in_data;
  sensor_config_set sensor_out_data;
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

static gboolean gst_tiovx_isp_preprocess (GstTIOVXSimo * self);

static gboolean
gst_tiovx_isp_set_dcc_file (GstTIOVXISP * self, gchar ** dcc_file,
    const gchar * location);

static gboolean gst_tiovx_isp_allocate_user_data_objects (GstTIOVXISP * src);

static const gchar *target_id_to_target_name (gint target_id);

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

  g_object_class_install_property (gobject_class, PROP_DCC_ISP_CONFIG_FILE,
      g_param_spec_string ("dcc-isp-file", "DCC ISP File",
          "TIOVX DCC tuning binary file for the given image sensor",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_DCC_2A_CONFIG_FILE,
      g_param_spec_string ("dcc-2a-file", "DCC AE/AWB File",
          "TIOVX DCC tuning binary file for the given image sensor",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SENSOR_ID,
      g_param_spec_string ("sensor-id", "Sensor ID",
          "TIOVX camera sensor string ID",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_ISP_TARGET,
          DEFAULT_TIOVX_ISP_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_NUM_EXPOSURES,
      g_param_spec_int ("num-exposures", "Number of exposures",
          "Number of exposures for the incoming raw image",
          min_num_exposures, max_num_exposures, default_num_exposures,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_LINE_INTERLEAVED,
      g_param_spec_boolean ("lines-interleaved", "Interleaved lines",
          "Flag to indicate if lines are interleaved",
          default_lines_interleaved,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_FORMAT_MSB,
      g_param_spec_int ("format-msb", "Format MSB",
          "Flag indicating which is the most significant bit that still has data",
          min_format_msb, max_format_msb, default_format_msb,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_META_HEIGHT_BEFORE,
      g_param_spec_int ("meta-height-before", "Meta height before",
          "Number of lines at the beggining of the frame that have metadata",
          min_meta_height_before, max_meta_height_before,
          default_meta_height_before,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_META_HEIGHT_AFTER,
      g_param_spec_int ("meta-height-after", "Meta height after",
          "Number of lines at the end of the frame that have metadata",
          min_meta_height_after, max_meta_height_after,
          default_meta_height_after,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AE_DISABLED,
      g_param_spec_boolean ("ae-disabled", "Auto exposure disable",
          "Flag to set if the auto exposure algorithm is disabled",
          default_ae_disabled,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AWB_DISABLED,
      g_param_spec_boolean ("awb-disabled", "Auto white balance disable",
          "Flag to set if the auto white balance algorithm is disabled",
          default_awb_disabled,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SENSOR_DCC_ID,
      g_param_spec_uint ("sensor-dcc-id", "Sensor DCC ID",
          "Numerical ID that identifies the image sensor to capture images from",
          0, G_MAXUINT,
          default_sensor_dcc_id,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AE_NUM_SKIP_FRAMES,
      g_param_spec_uint ("ae-num-skip-frames", "AE number of skipped frames",
          "To indicate the AE algorithm how often to process frames, 0 means every frame.",
          0, G_MAXUINT,
          default_ae_num_skip_frames,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AWB_NUM_SKIP_FRAMES,
      g_param_spec_uint ("awb-num-skip-frames", "AWB number of skipped frames",
          "To indicate the AWB algorithm how often to process frames, 0 means every frame.",
          0, G_MAXUINT,
          default_awb_num_skip_frames,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_ANALOG_GAIN,
      g_param_spec_uint ("analog-gain", "Analog gain",
          "Analog gain",
          0, G_MAXUINT,
          default_analog_gain,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AE_NUM_SKIP_FRAMES,
      g_param_spec_uint ("color-temperature", "Color temperature",
          "Color temperature",
          0, G_MAXUINT,
          default_color_temperature,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AWB_NUM_SKIP_FRAMES,
      g_param_spec_uint ("exposure-time", "Exposure time",
          "Exposure time",
          0, G_MAXUINT,
          default_exposure_time,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
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

  gsttiovxsimo_class->preprocess = GST_DEBUG_FUNCPTR (gst_tiovx_isp_preprocess);
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_isp_init (GstTIOVXISP * self)
{
  self->dcc_isp_config_file = NULL;
  self->dcc_2a_config_file = NULL;
  self->sensor_id = g_strdup (default_tiovx_sensor_id);

  self->num_exposures = default_num_exposures;
  self->line_interleaved = default_lines_interleaved;
  self->format_msb = default_format_msb;
  self->meta_height_before = default_meta_height_before;
  self->meta_height_after = default_meta_height_after;

  self->aewb_memory = NULL;
  self->h3a_stats_memory = NULL;

  self->user_data_allocator = g_object_new (GST_TYPE_TIOVX_ALLOCATOR, NULL);

  self->ae_disabled = default_ae_disabled;
  self->awb_disabled = default_awb_disabled;
  self->sensor_dcc_id = default_sensor_dcc_id;
  self->ae_num_skip_frames = default_ae_num_skip_frames;
  self->awb_num_skip_frames = default_awb_num_skip_frames;
  self->analog_gain = default_analog_gain;
  self->color_temperature = default_color_temperature;
  self->exposure_time = default_exposure_time;
}

static void
gst_tiovx_isp_finalize (GObject * obj)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (obj);

  GST_LOG_OBJECT (self, "finalize");

  /* Free internal strings */
  g_free (self->dcc_isp_config_file);
  self->dcc_isp_config_file = NULL;
  g_free (self->dcc_2a_config_file);
  self->dcc_2a_config_file = NULL;
  g_free (self->sensor_id);
  self->sensor_id = NULL;

  if (NULL != self->aewb_memory) {
    gst_memory_unref (self->aewb_memory);
  }
  if (NULL != self->h3a_stats_memory) {
    gst_memory_unref (self->h3a_stats_memory);
  }
  if (self->user_data_allocator) {
    g_object_unref (self->user_data_allocator);
  }

  G_OBJECT_CLASS (gst_tiovx_isp_parent_class)->finalize (obj);
}

static gboolean
gst_tiovx_isp_set_dcc_file (GstTIOVXISP * self, gchar ** dcc_file,
    const gchar * location)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (location, FALSE);

  g_free (*dcc_file);

  *dcc_file = g_strdup (location);

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
    case PROP_DCC_ISP_CONFIG_FILE:
      gst_tiovx_isp_set_dcc_file (self, &self->dcc_isp_config_file,
          g_value_get_string (value));
      break;
    case PROP_DCC_2A_CONFIG_FILE:
      gst_tiovx_isp_set_dcc_file (self, &self->dcc_2a_config_file,
          g_value_get_string (value));
      break;
    case PROP_SENSOR_ID:
      g_free (self->sensor_id);
      self->sensor_id = g_value_dup_string (value);
      break;
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    case PROP_NUM_EXPOSURES:
      self->num_exposures = g_value_get_int (value);
      break;
    case PROP_LINE_INTERLEAVED:
      self->line_interleaved = g_value_get_boolean (value);
      break;
    case PROP_FORMAT_MSB:
      self->format_msb = g_value_get_int (value);
      break;
    case PROP_META_HEIGHT_BEFORE:
      self->meta_height_before = g_value_get_int (value);
      break;
    case PROP_META_HEIGHT_AFTER:
      self->meta_height_after = g_value_get_int (value);
      break;
    case PROP_AE_DISABLED:
      self->ae_disabled = g_value_get_boolean (value);
      break;
    case PROP_AWB_DISABLED:
      self->awb_disabled = g_value_get_boolean (value);
      break;
    case PROP_SENSOR_DCC_ID:
      self->sensor_dcc_id = g_value_get_uint (value);
      break;
    case PROP_AE_NUM_SKIP_FRAMES:
      self->ae_num_skip_frames = g_value_get_uint (value);
      break;
    case PROP_AWB_NUM_SKIP_FRAMES:
      self->awb_num_skip_frames = g_value_get_uint (value);
      break;
    case PROP_ANALOG_GAIN:
      self->analog_gain = g_value_get_uint (value);
      break;
    case PROP_COLOR_TEMPERATURE:
      self->color_temperature = g_value_get_uint (value);
      break;
    case PROP_EXPOSURE_TIME:
      self->exposure_time = g_value_get_uint (value);
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
    case PROP_DCC_ISP_CONFIG_FILE:
      g_value_set_string (value, self->dcc_isp_config_file);
      break;
    case PROP_DCC_2A_CONFIG_FILE:
      g_value_set_string (value, self->dcc_2a_config_file);
      break;
    case PROP_SENSOR_ID:
      g_value_set_string (value, self->sensor_id);
      break;
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    case PROP_NUM_EXPOSURES:
      g_value_set_int (value, self->num_exposures);
      break;
    case PROP_LINE_INTERLEAVED:
      g_value_set_boolean (value, self->line_interleaved);
      break;
    case PROP_FORMAT_MSB:
      g_value_set_int (value, self->format_msb);
      break;
    case PROP_META_HEIGHT_BEFORE:
      g_value_set_int (value, self->meta_height_before);
      break;
    case PROP_META_HEIGHT_AFTER:
      g_value_set_int (value, self->meta_height_after);
      break;
    case PROP_AE_DISABLED:
      g_value_set_boolean (value, self->ae_disabled);
      break;
    case PROP_AWB_DISABLED:
      g_value_set_boolean (value, self->awb_disabled);
      break;
    case PROP_SENSOR_DCC_ID:
      g_value_set_uint (value, self->sensor_dcc_id);
      break;
    case PROP_AE_NUM_SKIP_FRAMES:
      g_value_set_uint (value, self->ae_num_skip_frames);
      break;
    case PROP_AWB_NUM_SKIP_FRAMES:
      g_value_set_uint (value, self->awb_num_skip_frames);
      break;
    case PROP_ANALOG_GAIN:
      g_value_set_uint (value, self->analog_gain);
      break;
    case PROP_COLOR_TEMPERATURE:
      g_value_set_uint (value, self->color_temperature);
      break;
    case PROP_EXPOSURE_TIME:
      g_value_set_uint (value, self->exposure_time);
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
  guint32 ti_2a_wrapper_ret = 0;
  vx_status status = VX_FAILURE;
  GstCaps *src_caps = NULL;
  GstStructure *sink_caps_st = NULL;
  const gchar *format_str = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);
  g_return_val_if_fail (sink_caps, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);

  self = GST_TIOVX_ISP (simo);

  tiovx_querry_sensor (&self->sensor_obj);
  tiovx_init_sensor (&self->sensor_obj, self->sensor_id);

  if (NULL == self->dcc_isp_config_file) {
    GST_ERROR_OBJECT (self, "DCC ISP config file not specified");
    goto out;
  }

  if (NULL == self->dcc_2a_config_file) {
    GST_ERROR_OBJECT (self, "DCC AE/AWB config file not specified");
    goto out;
  }

  snprintf (self->viss_obj.dcc_config_file_path, TIVX_FILEIO_FILE_PATH_LENGTH,
      "%s", self->dcc_isp_config_file);

  /* Initialize the input parameters */
  if (!gst_video_info_from_caps (&in_info, sink_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from input caps: %"
        GST_PTR_FORMAT, sink_caps);
    goto out;
  }

  self->viss_obj.input.bufq_depth = default_num_channels;
  self->viss_obj.input.params.width = GST_VIDEO_INFO_WIDTH (&in_info);
  self->viss_obj.input.params.height = GST_VIDEO_INFO_HEIGHT (&in_info);
  /* TODO: currently the user has the responsability of setting this parameters
   * through properties. This should ideally be obtained through a sensor query or
   * through the caps
   */
  self->viss_obj.input.params.num_exposures = self->num_exposures;
  self->viss_obj.input.params.line_interleaved = self->line_interleaved;
  self->viss_obj.input.params.format[0].msb = self->format_msb;
  self->viss_obj.input.params.meta_height_before = self->meta_height_before;
  self->viss_obj.input.params.meta_height_after = self->meta_height_after;

  sink_caps_st = gst_caps_get_structure (sink_caps, 0);
  format_str = gst_structure_get_string (sink_caps_st, "format");

  if ((g_strcmp0 (format_str, "bggr16") == 0)
      || (g_strcmp0 (format_str, "gbrg16") == 0)
      || (g_strcmp0 (format_str, "grbg16") == 0)
      || (g_strcmp0 (format_str, "rggb16") == 0)
      ) {
    self->viss_obj.input.params.format[0].pixel_container =
        TIVX_RAW_IMAGE_16_BIT;
  } else if ((g_strcmp0 (format_str, "bggr") == 0)
      || (g_strcmp0 (format_str, "gbrg") == 0)
      || (g_strcmp0 (format_str, "grbg") == 0)
      || (g_strcmp0 (format_str, "rggb") == 0)
      ) {
    self->viss_obj.input.params.format[0].pixel_container =
        TIVX_RAW_IMAGE_8_BIT;
  } else {
    GST_ERROR_OBJECT (self, "Couldn't determine pixel container form caps");
    goto out;
  }

  self->viss_obj.ae_awb_result_bufq_depth = default_num_channels;

  GST_INFO_OBJECT (self,
      "Input parameters:\n"
      "\tWidth: %d\n"
      "\tHeight: %d\n"
      "\tPool size: %d\n"
      "\tNum exposures: %d\n"
      "\tLines interleaved: %d\n"
      "\tFormat pixel container: 0x%x\n"
      "\tFormat MSB: %d\n"
      "\tMeta height before: %d\n"
      "\tMeta height after: %d",
      self->viss_obj.input.params.width,
      self->viss_obj.input.params.height,
      self->viss_obj.input.bufq_depth,
      self->viss_obj.input.params.num_exposures,
      self->viss_obj.input.params.line_interleaved,
      self->viss_obj.input.params.format[0].pixel_container,
      self->viss_obj.input.params.format[0].msb,
      self->viss_obj.input.params.meta_height_before,
      self->viss_obj.input.params.meta_height_after);


  /* Initialize the output parameters.
   * TODO: Only output for 12 or 8 bit is enabled, so only output2
   * parameters are specified.
   */
  self->viss_obj.output_select[0] = TIOVX_VISS_MODULE_OUTPUT_NA;
  self->viss_obj.output_select[1] = TIOVX_VISS_MODULE_OUTPUT_NA;
  self->viss_obj.output_select[2] = TIOVX_VISS_MODULE_OUTPUT_EN;
  self->viss_obj.output_select[3] = TIOVX_VISS_MODULE_OUTPUT_NA;
  self->viss_obj.output_select[4] = TIOVX_VISS_MODULE_OUTPUT_NA;

  if (max_supported_outputs < g_list_length (src_caps_list)) {
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

  self->viss_obj.output2.bufq_depth = default_num_channels;
  self->viss_obj.output2.color_format =
      gst_format_to_vx_format (out_info.finfo->format);
  self->viss_obj.output2.width = GST_VIDEO_INFO_WIDTH (&out_info);
  self->viss_obj.output2.height = GST_VIDEO_INFO_HEIGHT (&out_info);

  GST_INFO_OBJECT (self,
      "Output parameters:\n"
      "\tWidth: %d\n"
      "\tHeight: %d\n"
      "\tPool size: %d",
      self->viss_obj.input.params.width,
      self->viss_obj.input.params.height, self->viss_obj.input.bufq_depth);

  self->viss_obj.h3a_stats_bufq_depth = default_num_channels;

  GST_INFO_OBJECT (self, "Initializing ISP object");
  status = tiovx_viss_module_init (context, &self->viss_obj, &self->sensor_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto out;
  }

  /* TI_2A_wrapper configuration */
  g_free (self->ti_2a_wrapper.config);
  self->ti_2a_wrapper.config = g_malloc0 (sizeof (*self->ti_2a_wrapper.config));

  self->ti_2a_wrapper.config->sensor_dcc_id = self->sensor_dcc_id;
  self->ti_2a_wrapper.config->sensor_img_format = default_sensor_img_format;

  if (NULL != g_strrstr (format_str, "bggr")) {
    self->ti_2a_wrapper.config->sensor_img_phase =
        TI_2A_WRAPPER_SENSOR_IMG_PHASE_BGGR;
  } else if (NULL != g_strrstr (format_str, "gbrg")) {
    self->ti_2a_wrapper.config->sensor_img_phase =
        TI_2A_WRAPPER_SENSOR_IMG_PHASE_GBRG;
  } else if (NULL != g_strrstr (format_str, "grbg")) {
    self->ti_2a_wrapper.config->sensor_img_phase =
        TI_2A_WRAPPER_SENSOR_IMG_PHASE_GRBG;
  } else if (NULL != g_strrstr (format_str, "rggb")) {
    self->ti_2a_wrapper.config->sensor_img_phase =
        TI_2A_WRAPPER_SENSOR_IMG_PHASE_RGGB;
  } else {
    GST_ERROR_OBJECT (self, "Couldn't determine sensor img phase from caps");
    goto out;
  }

  if (self->sensor_obj.sensor_exp_control_enabled
      || self->sensor_obj.sensor_gain_control_enabled) {
    self->ti_2a_wrapper.config->ae_mode = ALGORITHMS_ISS_AE_AUTO;
  } else {
    self->ti_2a_wrapper.config->ae_mode = ALGORITHMS_ISS_AE_DISABLED;
  }
  self->ti_2a_wrapper.config->awb_mode = ALGORITHMS_ISS_AWB_AUTO;

  self->ti_2a_wrapper.config->awb_num_skip_frames = self->awb_num_skip_frames;  /* 0 = Process every frame */
  self->ti_2a_wrapper.config->ae_num_skip_frames = self->ae_num_skip_frames;    /* 0 = Process every frame */
  self->ti_2a_wrapper.config->channel_id = 0;

  g_free (self->ti_2a_wrapper.nodePrms);
  self->ti_2a_wrapper.nodePrms =
      g_malloc0 (sizeof (*self->ti_2a_wrapper.nodePrms));

  g_free (self->ti_2a_wrapper.nodePrms->dcc_input_params);
  self->ti_2a_wrapper.nodePrms->dcc_input_params =
      g_malloc0 (sizeof (*self->ti_2a_wrapper.nodePrms->dcc_input_params));

  g_free (self->ti_2a_wrapper.nodePrms->dcc_output_params);
  self->ti_2a_wrapper.nodePrms->dcc_output_params =
      g_malloc0 (sizeof (*self->ti_2a_wrapper.nodePrms->dcc_output_params));

  self->ti_2a_wrapper.nodePrms->dcc_input_params->analog_gain =
      self->analog_gain;
  self->ti_2a_wrapper.nodePrms->dcc_input_params->cameraId =
      self->sensor_dcc_id;
  self->ti_2a_wrapper.nodePrms->dcc_input_params->color_temparature =
      self->color_temperature;
  self->ti_2a_wrapper.nodePrms->dcc_input_params->exposure_time =
      self->exposure_time;

  {
    FILE *dcc_2a_file = fopen (self->dcc_2a_config_file, "rb");
    long file_size = 0;
    void *file_buffer = NULL;

    if (NULL == dcc_2a_file) {
      GST_ERROR_OBJECT (self, "Unable to open 2A config file: %s",
          self->dcc_2a_config_file);
      goto out;
    }

    fseek (dcc_2a_file, 0, SEEK_END);
    file_size = ftell (dcc_2a_file);
    fseek (dcc_2a_file, 0, SEEK_SET);   /* same as rewind(f); */

    if (0 == file_size) {
      GST_ERROR_OBJECT (self, "File: %s has size of 0",
          self->dcc_2a_config_file);
      fclose (dcc_2a_file);
      goto out;
    }

    file_buffer = g_malloc0 (file_size);
    fread (file_buffer, 1, file_size, dcc_2a_file);
    fclose (dcc_2a_file);

    g_free (self->ti_2a_wrapper.nodePrms->dcc_input_params->dcc_buf);
    self->ti_2a_wrapper.nodePrms->dcc_input_params->dcc_buf = file_buffer;
    self->ti_2a_wrapper.nodePrms->dcc_input_params->dcc_buf_size = file_size;

    status = Dcc_Create (self->ti_2a_wrapper.nodePrms->dcc_output_params, NULL);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Error creating DCC for output params: %d",
          status);
      goto out;
    }

    status =
        dcc_update (self->ti_2a_wrapper.nodePrms->dcc_input_params,
        self->ti_2a_wrapper.nodePrms->dcc_output_params);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Error creating updating DCC from input to output: %d", status);
      goto out;
    }

    self->ti_2a_wrapper.ae_disabled = self->ae_disabled;
    self->ti_2a_wrapper.awb_disabled = self->awb_disabled;
    self->ti_2a_wrapper.dcc_status = status;
  }

  self->ti_2a_wrapper.nodePrms->dcc_id = self->sensor_dcc_id;
  self->ti_2a_wrapper.h3a_aew_af_desc_status = default_h3a_aew_af_desc_status;

  ti_2a_wrapper_ret = TI_2A_wrapper_create (&self->ti_2a_wrapper);
  if (ti_2a_wrapper_ret) {
    GST_ERROR_OBJECT (self, "Unable to create TI 2A wrapper: %d",
        ti_2a_wrapper_ret);
    goto out;
  }

  GST_INFO_OBJECT (self,
      "TI 2A parameters:\n"
      "\tSensor DCC ID: %d\n"
      "\tSensor img phase: %d\n"
      "\tSensor AWB Mode: %d\n"
      "\tSensor AE Mode: %d\n"
      "\tSensor AWB num skip frames: %d\n"
      "\tSensor AE num skip frames: %d\n"
      "\tAnalog Gain: %d\n"
      "\tColor Temperature: %d\n"
      "\tExposure Time: %d",
      self->ti_2a_wrapper.config->sensor_dcc_id,
      self->ti_2a_wrapper.config->sensor_img_phase,
      self->ti_2a_wrapper.config->awb_mode,
      self->ti_2a_wrapper.config->ae_mode,
      self->ti_2a_wrapper.config->awb_num_skip_frames,
      self->ti_2a_wrapper.config->ae_num_skip_frames,
      self->ti_2a_wrapper.nodePrms->dcc_input_params->analog_gain,
      self->ti_2a_wrapper.nodePrms->dcc_input_params->color_temparature,
      self->ti_2a_wrapper.nodePrms->dcc_input_params->exposure_time);

  ret = TRUE;

out:

  if (!ret) {
    tiovx_deinit_sensor (&self->sensor_obj);
  }

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

  ret = gst_tiovx_isp_allocate_user_data_objects (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to allocate user data objects");
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
  GstTIOVXQueueable *queueable_object = NULL;
  gint graph_parameter_index = 0;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);
  g_return_val_if_fail (queueable_objects, FALSE);

  self = GST_TIOVX_ISP (simo);

  *node = self->viss_obj.node;

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

  /* ae_awb results & h3a stats aren't input or outputs, these are added as queueable_objects */
  queueable_object =
      GST_TIOVX_QUEUEABLE (g_object_new (GST_TYPE_TIOVX_QUEUEABLE, NULL));
  gst_tiovx_queueable_set_params (queueable_object,
      (vx_reference *) & self->viss_obj.ae_awb_result_handle[0],
      graph_parameter_index, ae_awb_result_param_id);
  graph_parameter_index++;
  *queueable_objects = g_list_append (*queueable_objects, queueable_object);

  queueable_object =
      GST_TIOVX_QUEUEABLE (g_object_new (GST_TYPE_TIOVX_QUEUEABLE, NULL));
  gst_tiovx_queueable_set_params (queueable_object,
      (vx_reference *) & self->viss_obj.h3a_stats_handle[0],
      graph_parameter_index, h3a_stats_param_id);
  graph_parameter_index++;
  *queueable_objects = g_list_append (*queueable_objects, queueable_object);

  return TRUE;
}

static gboolean
gst_tiovx_isp_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph)
{
  GstTIOVXISP *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  const gchar *target = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_ISP (simo);

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  GST_DEBUG_OBJECT (self, "Creating ISP graph");
  status =
      tiovx_viss_module_create (graph, &self->viss_obj, NULL, NULL, target);
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
  guint32 ti_2a_wrapper_ret = 0;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_ISP (simo);

  ti_2a_wrapper_ret = TI_2A_wrapper_delete (&self->ti_2a_wrapper);
  if (ti_2a_wrapper_ret) {
    GST_ERROR_OBJECT (self, "Unable to delete TI 2A wrapper: %d",
        ti_2a_wrapper_ret);
  }

  g_free (self->ti_2a_wrapper.nodePrms->dcc_input_params->dcc_buf);
  self->ti_2a_wrapper.nodePrms->dcc_input_params->dcc_buf = NULL;
  g_free (self->ti_2a_wrapper.nodePrms->dcc_input_params);
  self->ti_2a_wrapper.nodePrms->dcc_input_params = NULL;
  g_free (self->ti_2a_wrapper.nodePrms->dcc_output_params);
  self->ti_2a_wrapper.nodePrms->dcc_output_params = NULL;
  g_free (self->ti_2a_wrapper.config);
  self->ti_2a_wrapper.config = NULL;
  g_free (self->ti_2a_wrapper.nodePrms);
  self->ti_2a_wrapper.nodePrms = NULL;

  gst_tiovx_empty_exemplar ((vx_reference) self->
      viss_obj.ae_awb_result_handle[0]);
  gst_tiovx_empty_exemplar ((vx_reference) self->viss_obj.h3a_stats_handle[0]);

  tiovx_deinit_sensor (&self->sensor_obj);

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

static gboolean
gst_tiovx_isp_allocate_single_user_data_object (GstTIOVXISP * self,
    GstMemory ** memory, vx_user_data_object user_data)
{
  vx_size data_size = 0;
  vx_status status = VX_FAILURE;
  GstTIOVXMemoryData *ti_memory = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (memory, FALSE);
  g_return_val_if_fail (user_data, FALSE);

  if (NULL != *memory) {
    gst_memory_unref (*memory);
  }

  status =
      vxQueryUserDataObject (user_data, VX_USER_DATA_OBJECT_SIZE, &data_size,
      sizeof (data_size));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Unable to query user data object size from exemplar: %p", user_data);
    goto out;
  }
  *memory =
      gst_allocator_alloc (GST_ALLOCATOR (self->user_data_allocator), data_size,
      NULL);
  if (!*memory) {
    GST_ERROR_OBJECT (self, "Unable to allocate memory");
    goto out;
  }

  ti_memory = gst_tiovx_memory_get_data (*memory);
  if (NULL == ti_memory) {
    GST_ERROR_OBJECT (self, "Unable retrieve TI memory");
    goto out;
  }

  /* User data objects have a single "plane" */
  status = tivxReferenceImportHandle ((vx_reference) user_data,
      (const void **) &ti_memory->mem_ptr.host_ptr,
      (const uint32_t *) &ti_memory->size, 1);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Unable to import handles to exemplar: %p",
        user_data);
    goto out;
  }

  ret = TRUE;

out:
  if (!ret && *memory) {
    gst_memory_unref (*memory);
  }

  return ret;
}

/**
 * gst_tiovx_isp_allocate_user_data_objects:
 *
 * This subclass has 2 inputs/outputs for the node that aren't inputs/outputs
 * for the overall plugin. This class allocates the memory for these elements.
 *
 * After calling this functions gst_memory_unref should be called on
 * self->aewb_memory and self->h3a_stats_memory
 */
static gboolean
gst_tiovx_isp_allocate_user_data_objects (GstTIOVXISP * self)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);

  GST_DEBUG_OBJECT (self, "Allocating user data objects");

  ret =
      gst_tiovx_isp_allocate_single_user_data_object (self, &self->aewb_memory,
      self->viss_obj.ae_awb_result_handle[0]);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to allocate data for AEWB user data");
    goto out;
  }

  ret =
      gst_tiovx_isp_allocate_single_user_data_object (self,
      &self->h3a_stats_memory, self->viss_obj.h3a_stats_handle[0]);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to allocate data for H3A stats user data");
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

  type = gst_tiovx_isp_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static void *
gst_tiovx_isp_user_data_object_get_memory (GstTIOVXISP * self,
    vx_reference reference)
{
  vx_status status = VX_FAILURE;
  void *virtAddr[1] = { NULL };
  vx_uint32 size[1];
  vx_uint32 numEntries;

  status = tivxReferenceExportHandle (reference,
      virtAddr, size, 1, &numEntries);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Unable to get object's memory");
  }

  return virtAddr[0];
}

static gboolean
gst_tiovx_isp_preprocess (GstTIOVXSimo * simo)
{
  GstTIOVXISP *self = NULL;
  tivx_h3a_data_t *h3a_data = NULL;
  tivx_ae_awb_params_t *ae_awb_result = NULL;
  int32_t ti_2a_wrapper_ret = 0;
  gboolean ret = FALSE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_ISP (simo);

  h3a_data =
      (tivx_h3a_data_t *) gst_tiovx_isp_user_data_object_get_memory (self,
      (vx_reference) self->viss_obj.h3a_stats_handle[0]);
  ae_awb_result =
      (tivx_ae_awb_params_t *) gst_tiovx_isp_user_data_object_get_memory (self,
      (vx_reference) self->viss_obj.ae_awb_result_handle[0]);

  ti_2a_wrapper_ret =
      TI_2A_wrapper_process (&self->ti_2a_wrapper, h3a_data,
      &self->sensor_in_data, ae_awb_result, &self->sensor_out_data);
  if (ti_2a_wrapper_ret) {
    GST_ERROR_OBJECT (self, "Unable to process TI 2A wrapper: %d",
        ti_2a_wrapper_ret);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}
