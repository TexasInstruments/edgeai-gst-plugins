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

#include "gsttiovxmosaic.h"

#include <tiovx_img_mosaic_module.h>

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxmiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

static const int k_output_param_id = 1;
static const int k_input_param_id_start = 3;

static const int k_window_downscaling_max_ratio = 4;

/* TIOVX Mosaic Pad */

#define GST_TIOVX_TYPE_MOSAIC_PAD (gst_tiovx_mosaic_pad_get_type())
G_DECLARE_FINAL_TYPE (GstTIOVXMosaicPad, gst_tiovx_mosaic_pad,
    GST_TIOVX, MOSAIC_PAD, GstTIOVXMisoPad);

struct _GstTIOVXMosaicPadClass
{
  GstTIOVXMisoPadClass parent_class;
};

#define MIN_DIM_VALUE 0
#define MAX_DIM_VALUE G_MAXUINT32
#define DEFAULT_DIM_VALUE 0

#define MIN_START_VALUE 0
#define MAX_START_VALUE G_MAXUINT32
#define DEFAULT_START_VALUE 0

enum
{
  PROP_STARTX = 1,
  PROP_STARTY,
  PROP_WIDTH,
  PROP_HEIGHT,
};

struct _GstTIOVXMosaicPad
{
  GstTIOVXMisoPad base;
  guint startx;
  guint starty;
  guint width;
  guint height;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_mosaic_pad_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXMosaicPad, gst_tiovx_mosaic_pad,
    GST_TYPE_TIOVX_MISO_PAD,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_mosaic_pad_debug_category,
        "tiovxmosaicpad", 0, "debug category for TIOVX mosaic pad class"));

static void
gst_tiovx_mosaic_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_mosaic_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_tiovx_mosaic_pad_class_init (GstTIOVXMosaicPadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_tiovx_mosaic_pad_set_property;
  gobject_class->get_property = gst_tiovx_mosaic_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_STARTX,
      g_param_spec_uint ("startx", "Start X",
          "Starting X coordinate of the image",
          MIN_START_VALUE, MAX_START_VALUE, DEFAULT_START_VALUE,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_STARTY,
      g_param_spec_uint ("starty", "Start Y",
          "Starting Y coordinate of the image", MIN_START_VALUE,
          MAX_START_VALUE, DEFAULT_START_VALUE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_uint ("width", "Width", "Width of the image.\n"
          "Cannot be smaller than 1/4 of the input width or larger than the input width.\n"
          "Set to 0 to default to the input width.", MIN_DIM_VALUE,
          MAX_DIM_VALUE, DEFAULT_DIM_VALUE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_uint ("height", "Height", "Height of the image.\n"
          "Cannot be smaller than 1/4 of the input hieght or larger than the input height.\n"
          "Set to 0 to default to the input height.",
          MIN_DIM_VALUE, MAX_DIM_VALUE, DEFAULT_DIM_VALUE, G_PARAM_READWRITE));
}

static void
gst_tiovx_mosaic_pad_init (GstTIOVXMosaicPad * self)
{
  GST_DEBUG_OBJECT (self, "gst_tiovx_mosaic_pad_init");

  self->startx = DEFAULT_START_VALUE;
  self->starty = DEFAULT_START_VALUE;
  self->width = DEFAULT_DIM_VALUE;
  self->height = DEFAULT_DIM_VALUE;
}

static void
gst_tiovx_mosaic_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMosaicPad *self = GST_TIOVX_MOSAIC_PAD (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_STARTX:
      self->startx = g_value_get_uint (value);
      break;
    case PROP_STARTY:
      self->starty = g_value_get_uint (value);
      break;
    case PROP_WIDTH:
      self->width = g_value_get_uint (value);
      break;
    case PROP_HEIGHT:
      self->height = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_mosaic_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMosaicPad *self = GST_TIOVX_MOSAIC_PAD (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_STARTX:
      g_value_set_uint (value, self->startx);
      break;
    case PROP_STARTY:
      g_value_set_uint (value, self->starty);
      break;
    case PROP_WIDTH:
      g_value_set_enum (value, self->width);
      break;
    case PROP_HEIGHT:
      g_value_set_enum (value, self->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

/* TIOVX Mosaic */

/* Target definition */
enum
{
  TIVX_TARGET_VPAC_MSC1_ID = 0,
  TIVX_TARGET_VPAC_MSC2_ID,
  TIVX_TARGET_VPAC_MSC1_AND_2_ID,
};

/* The mosaic ignores the actual target string, we define this to have a
 * correct documentation in the properties
 */
static const gchar TIVX_TARGET_VPAC_MSC1_AND_MSC2[] = "VPAC_MSC1_AND_MSC2";

#define GST_TYPE_TIOVX_MOSAIC_TARGET (gst_tiovx_mosaic_target_get_type())
static GType
gst_tiovx_mosaic_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_MSC1_ID, "VPAC MSC1", TIVX_TARGET_VPAC_MSC1},
    {TIVX_TARGET_VPAC_MSC2_ID, "VPAC MSC2", TIVX_TARGET_VPAC_MSC2},
    {TIVX_TARGET_VPAC_MSC1_AND_2_ID, "VPAC MSC1 and VPAC MSC2",
        TIVX_TARGET_VPAC_MSC1_AND_MSC2},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXMosaicTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_MOSAIC_TARGET TIVX_TARGET_VPAC_MSC1_AND_2_ID

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
};

/* Formats definition */
#define TIOVX_MOSAIC_SUPPORTED_FORMATS_SRC "{ NV12, GRAY8, GRAY16_LE }"
#define TIOVX_MOSAIC_SUPPORTED_FORMATS_SINK "{ NV12, GRAY8, GRAY16_LE }"
#define TIOVX_MOSAIC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_MOSAIC_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TIOVX_MOSAIC_STATIC_CAPS_SRC \
  "video/x-raw, "                           \
  "format = (string) " TIOVX_MOSAIC_SUPPORTED_FORMATS_SRC ", "                    \
  "width = " TIOVX_MOSAIC_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_MOSAIC_SUPPORTED_HEIGHT ", "                  \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Sink caps */
#define TIOVX_MOSAIC_STATIC_CAPS_SINK \
  "video/x-raw, "                           \
  "format = (string) " TIOVX_MOSAIC_SUPPORTED_FORMATS_SINK ", "                   \
  "width = " TIOVX_MOSAIC_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_MOSAIC_SUPPORTED_HEIGHT ", "                  \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_MOSAIC_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_MOSAIC_STATIC_CAPS_SRC)
    );

struct _GstTIOVXMosaic
{
  GstTIOVXMiso parent;

  TIOVXImgMosaicModuleObj obj;

  gint target_id;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_mosaic_debug);
#define GST_CAT_DEFAULT gst_tiovx_mosaic_debug

#define gst_tiovx_mosaic_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXMosaic, gst_tiovx_mosaic,
    GST_TIOVX_MISO_TYPE, GST_DEBUG_CATEGORY_INIT (gst_tiovx_mosaic_debug,
        "tiovxmosaic", 0, "debug category for the tiovxmosaic element"));

static const gchar *target_id_to_target_name (gint target_id);
static void
gst_tiovx_mosaic_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_mosaic_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_tiovx_mosaic_init_module (GstTIOVXMiso * agg,
    vx_context context, GList * sink_pads_list, GstPad * src_pad);
static gboolean gst_tiovx_mosaic_create_graph (GstTIOVXMiso * agg,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_mosaic_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node);
static gboolean gst_tiovx_mosaic_configure_module (GstTIOVXMiso * agg);
static gboolean gst_tiovx_mosaic_release_buffer (GstTIOVXMiso * agg);
static gboolean gst_tiovx_mosaic_deinit_module (GstTIOVXMiso * agg);
static GstCaps *gst_tiovx_mosaic_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps);

static void
gst_tiovx_mosaic_class_init (GstTIOVXMosaicClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXMisoClass *gsttiovxmiso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxmiso_class = GST_TIOVX_MISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Mosaic",
      "Filter",
      "Mosaic using the TIOVX Modules API", "RidgeRun <support@ridgerun.com>");

  gobject_class->set_property = gst_tiovx_mosaic_set_property;
  gobject_class->get_property = gst_tiovx_mosaic_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_MOSAIC_TARGET,
          DEFAULT_TIOVX_MOSAIC_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &src_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sink_template, GST_TIOVX_TYPE_MOSAIC_PAD);

  gsttiovxmiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_mosaic_init_module);

  gsttiovxmiso_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_mosaic_configure_module);

  gsttiovxmiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_mosaic_get_node_info);

  gsttiovxmiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_mosaic_create_graph);

  gsttiovxmiso_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_mosaic_fixate_caps);

  gsttiovxmiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_mosaic_release_buffer);

  gsttiovxmiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_mosaic_deinit_module);
}

static void
gst_tiovx_mosaic_init (GstTIOVXMosaic * self)
{
  memset (&self->obj, 0, sizeof (self->obj));

  self->target_id = DEFAULT_TIOVX_MOSAIC_TARGET;
}

static void
gst_tiovx_mosaic_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMosaic *self = GST_TIOVX_MOSAIC (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
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
gst_tiovx_mosaic_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMosaic *self = GST_TIOVX_MOSAIC (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_mosaic_check_dimension (GstTIOVXMosaic * self,
    const guint input_value, vx_uint32 * output_value,
    const gint dimension_value, const gchar * dimension_name)
{
  g_return_if_fail (self);
  g_return_if_fail (output_value);
  g_return_if_fail (dimension_name);

  if (input_value == 0) {
    GST_DEBUG_OBJECT (self, "Pad %s is 0, default to image %s: %d",
        dimension_name, dimension_name, dimension_value);
    *output_value = dimension_value;
  } else if ((input_value >= dimension_value / k_window_downscaling_max_ratio)
      && (input_value <= dimension_value)) {
    *output_value = input_value;
    dimension_value, dimension_name);
  } else if (input_value < dimension_value / k_window_downscaling_max_ratio) {
    GST_WARNING_OBJECT (self,
        "Pad %s: %d is less than 1/%d of input %s: %d, setting 1/4 of input %s: %d",
        dimension_name, input_value, k_window_downscaling_max_ratio,
        dimension_name, dimension_value, dimension_name,
        dimension_value / k_window_downscaling_max_ratio);
    *output_value = dimension_value / k_window_downscaling_max_ratio;
  } else if (input_value > dimension_value) {
    GST_WARNING_OBJECT (self,
        "Pad %s: %d is larger than input %s: %d, setting input %s",
        dimension_name, input_value, dimension_name, dimension_value,
        dimension_name);
    *output_value = dimension_value;
  }
}

static gboolean
    gst_tiovx_mosaic_init_module (GstTIOVXMiso * agg, vx_context context,
    GList * sink_pads_list, GstPad * src_pad)
{
  GstTIOVXMosaic *self = NULL;
  TIOVXImgMosaicModuleObj *mosaic = NULL;
  GstCaps *caps = NULL;
  GList *l = NULL;
  gboolean ret = FALSE;
  GstVideoInfo video_info = {
  };
  gint i = 0;
  gint num_inputs = 0;
  gint target_id = -1;
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_MOSAIC (agg);
  mosaic = &self->obj;

  tivxImgMosaicParamsSetDefaults (&mosaic->params);

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target_id = self->target_id;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  switch (target_id) {
    case TIVX_TARGET_VPAC_MSC1_ID:
      mosaic->params.num_msc_instances = 1;
      mosaic->params.msc_instance = 0;
      break;
    case TIVX_TARGET_VPAC_MSC2_ID:
      mosaic->params.num_msc_instances = 1;
      mosaic->params.msc_instance = 1;
      break;
    case TIVX_TARGET_VPAC_MSC1_AND_2_ID:
      /* Default value is already to use both */
      break;
    default:
      GST_WARNING_OBJECT (self, "Invalid target provided, using defaults");
      break;
  }


  /* We only support a single channel */
  mosaic->num_channels = 1;
  /* Initialize the output parameters */
  for (l = sink_pads_list; l != NULL; l = g_list_next (l)) {
    GstTIOVXMisoPad *sink_pad = GST_TIOVX_MISO_PAD (l->data);
    GstTIOVXMosaicPad *mosaic_sink_pad = NULL;

    mosaic_sink_pad = GST_TIOVX_MOSAIC_PAD (sink_pad);
    caps = gst_pad_get_current_caps (GST_PAD (sink_pad));
    if (!gst_video_info_from_caps (&video_info, caps)) {
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps);
      gst_caps_unref (caps);
      return ret;
    }
    gst_caps_unref (caps);
    mosaic->inputs[i].width = GST_VIDEO_INFO_WIDTH (&video_info);
    mosaic->inputs[i].height = GST_VIDEO_INFO_HEIGHT (&video_info);
    mosaic->inputs[i].color_format =
        gst_format_to_vx_format (video_info.finfo->format);
    mosaic->inputs[i].bufq_depth = DEFAULT_NUM_CHANNELS;
    mosaic->inputs[i].graph_parameter_index = i;
    mosaic->params.windows[i].startX = mosaic_sink_pad->startx;
    mosaic->params.windows[i].startY = mosaic_sink_pad->starty;

    gst_tiovx_mosaic_check_dimension (self, mosaic_sink_pad->width,
        &mosaic->params.windows[i].width,
        GST_VIDEO_INFO_WIDTH (&video_info), "width");
    gst_tiovx_mosaic_check_dimension (self, mosaic_sink_pad->height,
        &mosaic->params.windows[i].height,
        GST_VIDEO_INFO_HEIGHT (&video_info), "height");

    mosaic->params.windows[i].input_select = i;
    /* We only support a single channel */
    mosaic->params.windows[i].channel_select = 0;
    GST_INFO_OBJECT (self,
        "Input %d parameters: \n\tWidth: %d \n\tHeight: %d \n\tNum channels: %d\n\tStart X: %d\n\tStart Y: %d\n\tOutput Width: %d \n\tOutput Height: %d",
        i, mosaic->inputs[i].width, mosaic->inputs[i].height,
        mosaic->inputs[i].bufq_depth, mosaic->params.windows[i].startX,
        mosaic->params.windows[i].startY, mosaic->params.windows[i].width,
        mosaic->params.windows[i].height);
    num_inputs++;
    i++;
  }


  mosaic->params.num_windows = num_inputs;
  mosaic->num_inputs = num_inputs;
  /* Initialize the output parameters */
  caps = gst_pad_get_current_caps (src_pad);
  if (!gst_video_info_from_caps (&video_info, caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
        GST_PTR_FORMAT, caps);
    gst_caps_unref (caps);
    goto out;
  }
  gst_caps_unref (caps);
  mosaic->out_width = GST_VIDEO_INFO_WIDTH (&video_info);
  mosaic->out_height = GST_VIDEO_INFO_HEIGHT (&video_info);
  mosaic->out_bufq_depth = DEFAULT_NUM_CHANNELS;
  mosaic->color_format = gst_format_to_vx_format (video_info.finfo->format);
  mosaic->output_graph_parameter_index = i;

  /* Number of time to clear the output buffer before it gets reused */
  mosaic->params.clear_count = 2;
  GST_INFO_OBJECT (self,
      "Output parameters: \n  Width: %d \n  Height: %d \n  Number of Channels: %d",
      mosaic->out_width, mosaic->out_height, mosaic->out_bufq_depth);
  GST_INFO_OBJECT (self, "Initializing mosaic object");
  status = tiovx_img_mosaic_module_init (context, mosaic);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto out;
  }


  ret = TRUE;
out:
  return ret;
}

static gboolean
    gst_tiovx_mosaic_create_graph (GstTIOVXMiso * agg, vx_context context,
    vx_graph graph)
{
  GstTIOVXMosaic *self = NULL;
  TIOVXImgMosaicModuleObj *mosaic = NULL;
  vx_object_array input_arr_user[] = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  const gchar *target = NULL;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_MOSAIC (agg);
  mosaic = &self->obj;

  /* We read this value here, but the actual target will be done as part of the object params */
  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  /* Let's swap this by a valid value, even if ignored by the mosaic */
  if (TIVX_TARGET_VPAC_MSC1_AND_MSC2 == target) {
    target = TIVX_TARGET_VPAC_MSC2;
  }

  GST_DEBUG_OBJECT (self, "Creating mosaic graph");
  status =
      tiovx_img_mosaic_module_create (graph, mosaic,
      NULL, input_arr_user, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto exit;
  }

  ret = TRUE;
exit:
  return ret;
}

static gboolean
    gst_tiovx_mosaic_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node)
{
  GstTIOVXMosaic *mosaic = NULL;
  GList *l = NULL;
  gint i = 0;
  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);
  g_return_val_if_fail (node, FALSE);
  mosaic = GST_TIOVX_MOSAIC (agg);
  for (l = sink_pads_list; l; l = l->next) {
    GstAggregatorPad *pad = l->data;

    gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (pad),
        (vx_reference *) & mosaic->obj.inputs[i].image_handle[0],
        mosaic->obj.inputs[i].graph_parameter_index,
        k_input_param_id_start + i);
    i++;
  }

  gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (src_pad),
      (vx_reference *) & mosaic->obj.output_image[0],
      mosaic->obj.output_graph_parameter_index, k_output_param_id);

  *node = mosaic->obj.node;
  return TRUE;
}

static gboolean gst_tiovx_mosaic_configure_module (GstTIOVXMiso * agg)
{
  return TRUE;
}

static gboolean gst_tiovx_mosaic_release_buffer (GstTIOVXMiso * agg)
{
  GstTIOVXMosaic *self = NULL;
  vx_status status = VX_SUCCESS;
  g_return_val_if_fail (agg, FALSE);
  self = GST_TIOVX_MOSAIC (agg);
  GST_INFO_OBJECT (self, "Release buffer");
  status = tiovx_img_mosaic_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean gst_tiovx_mosaic_deinit_module (GstTIOVXMiso * agg)
{
  GstTIOVXMosaic *self = NULL;
  TIOVXImgMosaicModuleObj *mosaic = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  g_return_val_if_fail (agg, FALSE);
  self = GST_TIOVX_MOSAIC (agg);
  mosaic = &self->obj;
  /* Delete graph */
  status = tiovx_img_mosaic_module_delete (mosaic);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    goto out;
  }

  status = tiovx_img_mosaic_module_deinit (mosaic);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    goto out;
  }

  ret = TRUE;
out:
  return ret;
}

static GstCaps *gst_tiovx_mosaic_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps)
{
  GstCaps *output_caps = NULL;
  GList *l = NULL;
  gint best_width = -1, best_height = -1;
  gint best_fps_n = -1, best_fps_d = -1;
  gdouble best_fps = 0.;
  GstStructure *s;
  g_return_val_if_fail (self, output_caps);
  g_return_val_if_fail (sink_caps_list, output_caps);
  g_return_val_if_fail (src_caps, output_caps);
  GST_INFO_OBJECT (self, "Fixating caps");
  output_caps = gst_caps_make_writable (src_caps);
  s = gst_caps_get_structure (output_caps, 0);
  GST_OBJECT_LOCK (self);
  for (l = GST_ELEMENT (self)->sinkpads; l; l = l->next) {
    GstPad *sink_pad = l->data;
    GstTIOVXMosaicPad *mosaic_pad = NULL;
    GstVideoInfo video_info = {
    };
    GstCaps *caps = NULL;
    gint this_width, this_height;
    guint width, height;
    gint fps_n, fps_d;
    gdouble cur_fps;
    if (!GST_TIOVX_IS_MOSAIC_PAD (sink_pad)) {
      continue;
    }
    mosaic_pad = GST_TIOVX_MOSAIC_PAD (sink_pad);
    caps = gst_pad_get_current_caps (sink_pad);
    if (!gst_video_info_from_caps (&video_info, caps)) {
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps);
      goto out;
    }
    gst_caps_unref (caps);
    fps_n = GST_VIDEO_INFO_FPS_N (&video_info);
    fps_d = GST_VIDEO_INFO_FPS_D (&video_info);

    gst_tiovx_mosaic_check_dimension (GST_TIOVX_MOSAIC (self),
        mosaic_pad->width, &width, GST_VIDEO_INFO_WIDTH (&video_info), "width");
    gst_tiovx_mosaic_check_dimension (GST_TIOVX_MOSAIC (self),
        mosaic_pad->height, &height, GST_VIDEO_INFO_HEIGHT (&video_info),
        "height");

    if (width == 0 || height == 0)
      continue;
    this_width = width + MAX (mosaic_pad->startx, 0);
    this_height = height + MAX (mosaic_pad->starty, 0);
    if (best_width < this_width)
      best_width = this_width;
    if (best_height < this_height)
      best_height = this_height;
    if (fps_d == 0)
      cur_fps = 0.0;
    else
      gst_util_fraction_to_double (fps_n, fps_d, &cur_fps);
    if (best_fps < cur_fps) {
      best_fps = cur_fps;
      best_fps_n = fps_n;
      best_fps_d = fps_d;
    }
  }
  GST_OBJECT_UNLOCK (self);
  if (best_fps_n <= 0 || best_fps_d <= 0 || best_fps == 0.0) {
    best_fps_n = 25;
    best_fps_d = 1;
    best_fps = 25.0;
  }


  gst_structure_fixate_field_nearest_int (s, "width", best_width);
  gst_structure_fixate_field_nearest_int (s, "height", best_height);
  gst_structure_fixate_field_nearest_fraction (s, "framerate", best_fps_n,
      best_fps_d);
  output_caps = gst_caps_fixate (output_caps);
out:
  return output_caps;
}


static const gchar *target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_mosaic_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}
