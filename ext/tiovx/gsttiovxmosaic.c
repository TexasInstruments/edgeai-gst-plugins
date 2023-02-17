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

#include "unistd.h"

#include <tiovx_img_mosaic_module.h>

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxallocator.h"
#include "gst-libs/gst/tiovx/gsttiovxmiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

static const int output_param_id = 1;
static const int input_param_id_start = 3;

static const int window_downscaling_max_ratio = 4;

#define MODULE_MAX_NUM_PLANES 4
static const gchar *default_tiovx_mosaic_background = "";

/* TIOVX Mosaic Pad */

#define GST_TYPE_TIOVX_MOSAIC_PAD (gst_tiovx_mosaic_pad_get_type())
G_DECLARE_FINAL_TYPE (GstTIOVXMosaicPad, gst_tiovx_mosaic_pad,
    GST_TIOVX, MOSAIC_PAD, GstTIOVXMisoPad);

struct _GstTIOVXMosaicPadClass
{
  GstTIOVXMisoPadClass parent_class;
};

static const gint min_enable_channel = -1;
static const gint default_enable_channel = -1;

static const gint min_dim_value = -1;
static const gint max_dim_value = G_MAXINT32;
static const gint default_dim_value = 0;

static const gint min_start_value = -1;
static const gint max_start_value = G_MAXINT32;
static const gint default_start_value = 0;

enum
{
  PROP_STARTX = 1,
  PROP_STARTY,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_CHANNELS,
};

struct _GstTIOVXMosaicPad
{
  GstTIOVXMisoPad base;
  gint enabled_channels[MAX_NUM_CHANNELS];
  gint startx[MAX_NUM_CHANNELS];
  gint starty[MAX_NUM_CHANNELS];
  gint width[MAX_NUM_CHANNELS];
  gint height[MAX_NUM_CHANNELS];

  gboolean enabled_channels_set;
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
static gboolean
gst_tiovx_mosaic_pad_set_properties_array (GstTIOVXMosaicPad * self,
    const GValue * array, gint * out_array);
static gboolean gst_tiovx_mosaic_pad_get_properties_array (GstTIOVXMosaicPad *
    self, GValue * array, gint * in_array);
static gint *gst_array_to_c_array (const GValue * gst_array, guint * length);
static void
c_array_to_gst_array (GValue * gst_array, const gint * c_array, guint length);

static void
gst_tiovx_mosaic_pad_class_init (GstTIOVXMosaicPadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_tiovx_mosaic_pad_set_property;
  gobject_class->get_property = gst_tiovx_mosaic_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_CHANNELS,
      gst_param_spec_array ("channels",
          "Enabled channels for this pad",
          "Array of channels to be displayed from this pad. If none are specified "
          "these are assumed to be <0, 1, 2, ...>. "
          "Usage example: <0, 0, 2>",
          g_param_spec_int ("enabled-channel", "Enabled channel",
              "Enabled channel to be displayed for this pad",
              min_enable_channel, MAX_NUM_CHANNELS, default_enable_channel,
              G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STARTX,
      gst_param_spec_array ("startx",
          "Start in the X axis",
          "Array of X start of each window for this pad. "
          "Windows affecting this pad are determined by the channels property. "
          "Usage example: <0, 320, 640>",
          g_param_spec_int ("startx-channel", "Start X for nth channel",
              "Starting X coordinate of the image for a given channel",
              min_start_value, max_start_value, default_start_value,
              G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STARTY,
      gst_param_spec_array ("starty",
          "Start in the Y axis",
          "Array of Y start of each window for this pad. "
          "Windows affecting this pad are determined by the channels property. "
          "Usage example: <0, 240, 480>",
          g_param_spec_int ("starty-channel", "Start Y for nth channel",
              "Starting Y coordinate of the image for a given channel",
              min_start_value, max_start_value, default_start_value,
              G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_WIDTH,
      gst_param_spec_array ("widths",
          "Widths for each channel",
          "Widths for each of the enabled channels. "
          "Cannot be smaller than 1/4 of the input width or larger than the input width. "
          "Defaults to the input width. "
          "Usage example: <320, 320, 160>",
          g_param_spec_int ("width", "Width",
              "Width for a channel in this pad.\n"
              "Width for a particular channel in this pad", min_dim_value,
              max_dim_value, default_dim_value,
              G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      gst_param_spec_array ("heights",
          "Heights for each channel",
          "Heights for each of the enabled channels. "
          "Cannot be smaller than 1/4 of the input height or larger than the input height. "
          "Default to the input height. "
          "Usage example: <240, 240, 120>",
          g_param_spec_int ("height", "Height",
              "Height for a channel in this pad.\n"
              "Height for a particular channel in this pad", min_dim_value,
              max_dim_value, default_dim_value,
              G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));
}

static void
gst_tiovx_mosaic_pad_init (GstTIOVXMosaicPad * self)
{
  gint i = 0;

  GST_DEBUG_OBJECT (self, "gst_tiovx_mosaic_pad_init");

  for (i = 0; i < MAX_NUM_CHANNELS; i++) {
    self->enabled_channels[i] = i;
    self->startx[i] = default_start_value;
    self->starty[i] = default_start_value;
    self->width[i] = default_dim_value;
    self->height[i] = default_dim_value;
  }

  self->enabled_channels_set = FALSE;
}

static void
gst_tiovx_mosaic_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMosaicPad *self = GST_TIOVX_MOSAIC_PAD (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_CHANNELS:
      gst_tiovx_mosaic_pad_set_properties_array (self, value,
          self->enabled_channels);
      self->enabled_channels_set = TRUE;
      break;
    case PROP_STARTX:
      gst_tiovx_mosaic_pad_set_properties_array (self, value, self->startx);
      break;
    case PROP_STARTY:
      gst_tiovx_mosaic_pad_set_properties_array (self, value, self->starty);
      break;
    case PROP_WIDTH:
      gst_tiovx_mosaic_pad_set_properties_array (self, value, self->width);
      break;
    case PROP_HEIGHT:
      gst_tiovx_mosaic_pad_set_properties_array (self, value, self->height);
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
    case PROP_CHANNELS:
      gst_tiovx_mosaic_pad_get_properties_array (self, value,
          self->enabled_channels);
      break;
    case PROP_STARTX:
      gst_tiovx_mosaic_pad_get_properties_array (self, value, self->startx);
      break;
    case PROP_STARTY:
      gst_tiovx_mosaic_pad_get_properties_array (self, value, self->starty);
      break;
    case PROP_WIDTH:
      gst_tiovx_mosaic_pad_get_properties_array (self, value, self->width);
      break;
    case PROP_HEIGHT:
      gst_tiovx_mosaic_pad_get_properties_array (self, value, self->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_mosaic_pad_get_properties_array (GstTIOVXMosaicPad * self,
    GValue * array, gint * in_array)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (array, FALSE);
  g_return_val_if_fail (in_array, FALSE);

  c_array_to_gst_array (array, in_array, MAX_NUM_CHANNELS);

  return TRUE;
}

static gboolean
gst_tiovx_mosaic_pad_set_properties_array (GstTIOVXMosaicPad * self,
    const GValue * array, gint * out_array)
{
  gint *c_array = NULL;
  guint c_array_length = 0;
  gint i = 0;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (array, FALSE);
  g_return_val_if_fail (out_array, FALSE);

  c_array = gst_array_to_c_array (array, &c_array_length);

  for (i = 0; i < MAX_NUM_CHANNELS; i++) {
    out_array[i] = -1;
  }

  for (i = 0; i < c_array_length; i++) {
    out_array[i] = c_array[i];
  }

  g_free (c_array);

  return TRUE;
}

/*
 * Function:  gst_array_to_c_array
 * --------------------
 * extracts values from an array of GValues and inserts them into an array of
 * gints. Output must be freed by the caller.
 *
 *  gst_array: input parameter, holds the GValues source array.
 *  length: output parameter, holds the length of the array.
 *
 *  returns: result array of gints.
 */
static gint *
gst_array_to_c_array (const GValue * gst_array, guint * length)
{
  gint *c_array = NULL;
  guint value = 0;
  guint i = 0;

  g_return_val_if_fail (gst_array, NULL);
  g_return_val_if_fail (length, NULL);

  *length = gst_value_array_get_size (gst_array);
  if (*length > MAX_NUM_CHANNELS) {
    *length = MAX_NUM_CHANNELS;
  }

  c_array = g_malloc (*length * sizeof (*c_array));

  for (i = 0; (i < *length) && (i < MAX_NUM_CHANNELS); i++) {
    value = g_value_get_int (gst_value_array_get_value (gst_array, i));
    c_array[i] = value;
  }

  return c_array;
}

/*
 * Function:  c_array_to_gst_array
 * --------------------
 * extracts values from an array of gints and inserts them into an array of
 * GValues.
 *
 *  gst_array: output parameter, holds the GValues result array.
 *  c_array: input parameter, holds the gint source array values.
 *  length: input parameter, holds the length of the input array.
 *
 *  returns: -
 */
static void
c_array_to_gst_array (GValue * gst_array, const gint * c_array, guint length)
{
  GValue value = G_VALUE_INIT;
  guint i = 0;

  g_return_if_fail (gst_array);
  g_return_if_fail (c_array);

  for (i = 0; i < length; i++) {
    g_value_init (&value, G_TYPE_UINT);
    g_value_set_uint (&value, c_array[i]);

    gst_value_array_append_value (gst_array, &value);
    g_value_unset (&value);
  }
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
  PROP_BACKGROUND,
};

/* Formats definition */
#define TIOVX_MOSAIC_SUPPORTED_FORMATS_SRC "{ NV12, GRAY8, GRAY16_LE }"
#define TIOVX_MOSAIC_SUPPORTED_FORMATS_SINK "{ NV12, GRAY8, GRAY16_LE }"
#define TIOVX_MOSAIC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_MOSAIC_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_MOSAIC_SUPPORTED_CHANNELS "[1 , 16]"

#define TIOVX_MOSAIC_STATIC_CAPS_SINK                          \
  "video/x-raw, "                                              \
  "format = (string) " TIOVX_MOSAIC_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_MOSAIC_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_MOSAIC_SUPPORTED_HEIGHT ", "               \
  "framerate = " GST_VIDEO_FPS_RANGE                           \
  "; "                                                         \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "         \
  "format = (string) " TIOVX_MOSAIC_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_MOSAIC_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_MOSAIC_SUPPORTED_HEIGHT ", "               \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                      \
  "num-channels = " TIOVX_MOSAIC_SUPPORTED_CHANNELS

#define TIOVX_MOSAIC_STATIC_CAPS_SRC                           \
  "video/x-raw, "                                              \
  "format = (string) " TIOVX_MOSAIC_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_MOSAIC_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_MOSAIC_SUPPORTED_HEIGHT ", "               \
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

static GstStaticPadTemplate background_template =
GST_STATIC_PAD_TEMPLATE ("background",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_MOSAIC_STATIC_CAPS_SRC)
    );

struct _GstTIOVXMosaic
{
  GstTIOVXMiso parent;

  TIOVXImgMosaicModuleObj obj;

  gint target_id;
  gchar *background;
  gboolean has_background_pad;
  gboolean has_background_image;

  GstTIOVXAllocator *user_data_allocator;
  GstMemory *background_image_memory;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_mosaic_debug);
#define GST_CAT_DEFAULT gst_tiovx_mosaic_debug

#define gst_tiovx_mosaic_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXMosaic, gst_tiovx_mosaic,
    GST_TYPE_TIOVX_MISO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_mosaic_debug,
        "tiovxmosaic", 0, "debug category for the tiovxmosaic element"));

static const gchar *target_id_to_target_name (gint target_id);
static void
gst_tiovx_mosaic_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_mosaic_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_tiovx_mosaic_init_module (GstTIOVXMiso * agg,
    vx_context context, GList * sink_pads_list, GstPad * src_pad,
    guint num_channels);
static gboolean gst_tiovx_mosaic_create_graph (GstTIOVXMiso * agg,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_mosaic_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects);
static gboolean gst_tiovx_mosaic_configure_module (GstTIOVXMiso * agg);
static gboolean gst_tiovx_mosaic_release_buffer (GstTIOVXMiso * agg);
static gboolean gst_tiovx_mosaic_deinit_module (GstTIOVXMiso * agg);
static GstCaps *gst_tiovx_mosaic_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels);
static void gst_tiovx_mosaic_finalize (GObject * object);

static gboolean gst_tiovx_mosaic_load_mosaic_module_objects (GstTIOVXMosaic *
    self);
static gboolean
gst_tiovx_mosaic_load_background_image (GstTIOVXMosaic * self,
    GstMemory ** memory, vx_image background_img);

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

  g_object_class_install_property (gobject_class, PROP_BACKGROUND,
      g_param_spec_string ("background", "Background",
          "Background image of the Mosaic to be used by this element",
          default_tiovx_mosaic_background,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &src_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sink_template, GST_TYPE_TIOVX_MOSAIC_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &background_template, GST_TYPE_TIOVX_MISO_PAD);

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

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_mosaic_finalize);
}

static void
gst_tiovx_mosaic_init (GstTIOVXMosaic * self)
{
  memset (&self->obj, 0, sizeof (self->obj));

  self->target_id = DEFAULT_TIOVX_MOSAIC_TARGET;
  self->background = g_strdup (default_tiovx_mosaic_background);
  self->has_background_pad = FALSE;
  self->has_background_image = FALSE;
  self->user_data_allocator = g_object_new (GST_TYPE_TIOVX_ALLOCATOR, NULL);
  self->background_image_memory = NULL;
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
    case PROP_BACKGROUND:
      g_free (self->background);
      self->background = g_value_dup_string (value);
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
    case PROP_BACKGROUND:
      g_value_set_string (value, self->background);
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
  } else if ((input_value >= dimension_value / window_downscaling_max_ratio)
      && (input_value <= dimension_value)) {
    *output_value = input_value;
  } else if (input_value < dimension_value / window_downscaling_max_ratio) {
    GST_WARNING_OBJECT (self,
        "Pad %s: %d is less than 1/%d of input %s: %d, setting 1/4 of input %s: %d",
        dimension_name, input_value, window_downscaling_max_ratio,
        dimension_name, dimension_value, dimension_name,
        dimension_value / window_downscaling_max_ratio);
    *output_value = dimension_value / window_downscaling_max_ratio;
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
    GList * sink_pads_list, GstPad * src_pad, guint num_channels)
{
  GstTIOVXMosaic *self = NULL;
  TIOVXImgMosaicModuleObj *mosaic = NULL;
  GstCaps *caps = NULL;
  GList *l = NULL;
  gboolean ret = FALSE;
  GstVideoInfo video_info = {
  };
  gint i = 0;
  guint num_enabled_windows = 0;
  gint num_inputs = 0;
  gint target_id = -1;
  gint enabled_channels[MAX_NUM_CHANNELS];
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_MOSAIC (agg);
  mosaic = &self->obj;

  self->has_background_image = (0 != g_strcmp0 (default_tiovx_mosaic_background,
          self->background));
  if (self->has_background_image) {
    if (F_OK != access (self->background, F_OK)) {
      GST_ERROR_OBJECT (self, "Invalid background property file path: %s",
          self->background);
      goto out;
    }
  }

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
  mosaic->num_channels = num_channels;
  /* Initialize the output parameters */
  for (l = sink_pads_list; l != NULL; l = g_list_next (l)) {
    GstTIOVXMisoPad *sink_pad = GST_TIOVX_MISO_PAD (l->data);
    GstTIOVXMosaicPad *mosaic_sink_pad = NULL;
    guint j = 0;

    if (!GST_TIOVX_IS_MOSAIC_PAD (sink_pad)) {
      /* Only the background can be a a non-mosaic sink pad */
      self->has_background_pad = TRUE;
      continue;
    }

    mosaic_sink_pad = GST_TIOVX_MOSAIC_PAD (sink_pad);
    caps = gst_pad_get_current_caps (GST_PAD (sink_pad));
    ret = gst_video_info_from_caps (&video_info, caps);
    gst_caps_unref (caps);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps);
      goto out;
    }

    mosaic->inputs[i].width = GST_VIDEO_INFO_WIDTH (&video_info);
    mosaic->inputs[i].height = GST_VIDEO_INFO_HEIGHT (&video_info);
    mosaic->inputs[i].color_format =
        gst_format_to_vx_format (video_info.finfo->format);
    mosaic->inputs[i].bufq_depth = 1;
    mosaic->inputs[i].graph_parameter_index = i;

    if (mosaic_sink_pad->enabled_channels_set) {
      for (j = 0; j < MAX_NUM_CHANNELS; j++) {
        enabled_channels[j] = mosaic_sink_pad->enabled_channels[j];
      }
    } else {
      for (j = 0; j < num_channels; j++) {
        enabled_channels[j] = j;
      }
      for (; j < MAX_NUM_CHANNELS; j++) {
        enabled_channels[j] = -1;
      }
    }

    for (j = 0; j < MAX_NUM_CHANNELS; j++) {
      if (enabled_channels[j] < 0) {
        continue;
      }

      if (enabled_channels[j] >= num_channels) {
        GST_WARNING_OBJECT (self, "Specified channel number: %d is larger than"
            "the max number of input channels, ignoring configuration",
            enabled_channels[j]);
        continue;
      }

      mosaic->params.windows[num_enabled_windows].startX =
          MAX (mosaic_sink_pad->startx[j], default_start_value);
      mosaic->params.windows[num_enabled_windows].startY =
          MAX (mosaic_sink_pad->starty[j], default_start_value);


      gst_tiovx_mosaic_check_dimension (self, mosaic_sink_pad->width[j],
          &mosaic->params.windows[num_enabled_windows].width,
          GST_VIDEO_INFO_WIDTH (&video_info), "width");
      gst_tiovx_mosaic_check_dimension (self, mosaic_sink_pad->height[j],
          &mosaic->params.windows[num_enabled_windows].height,
          GST_VIDEO_INFO_HEIGHT (&video_info), "height");

      mosaic->params.windows[num_enabled_windows].input_select = i;
      mosaic->params.windows[num_enabled_windows].channel_select =
          mosaic_sink_pad->enabled_channels[j];

      GST_INFO_OBJECT (self,
          "Window %d params:\n\tInput: %d\n\tChannel: %d\n\tStart X: %d\n\tStart Y: %d\n\tOutput Width: %d \n\tOutput Height: %d",
          num_enabled_windows,
          mosaic->params.windows[num_enabled_windows].input_select,
          mosaic->params.windows[num_enabled_windows].channel_select,
          mosaic->params.windows[num_enabled_windows].startX,
          mosaic->params.windows[num_enabled_windows].startY,
          mosaic->params.windows[num_enabled_windows].width,
          mosaic->params.windows[num_enabled_windows].height);

      num_enabled_windows++;
    }

    GST_INFO_OBJECT (self,
        "Input %d parameters: \n\tWidth: %d \n\tHeight: %d",
        i, mosaic->inputs[i].width, mosaic->inputs[i].height);

    num_inputs++;
    i++;
  }


  mosaic->params.num_windows = num_enabled_windows;
  mosaic->num_inputs = num_inputs;

  /* Initialize the output parameters */
  caps = gst_pad_get_current_caps (src_pad);
  ret = gst_video_info_from_caps (&video_info, caps);
  gst_caps_unref (caps);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
        GST_PTR_FORMAT, caps);
    goto out;
  }

  mosaic->out_width = GST_VIDEO_INFO_WIDTH (&video_info);
  mosaic->out_height = GST_VIDEO_INFO_HEIGHT (&video_info);
  mosaic->out_bufq_depth = 1;
  mosaic->color_format = gst_format_to_vx_format (video_info.finfo->format);
  mosaic->output_graph_parameter_index = i;
  i++;

  if (self->has_background_pad || self->has_background_image) {
    mosaic->background_graph_parameter_index = i;
    i++;
  }

  /* Number of times to clear the output buffer before it gets reused */
  mosaic->params.clear_count = gst_tiovx_miso_pad_get_pool_size(
      GST_TIOVX_MISO_PAD(src_pad));
  GST_INFO_OBJECT (self,
      "Output parameters: \n  Width: %d \n  Height: %d",
      mosaic->out_width, mosaic->out_height);
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
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
  };
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

  if (NULL == target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    goto exit;
  }

  GST_DEBUG_OBJECT (self, "Creating mosaic graph");
  if (self->has_background_pad || self->has_background_image) {
    status =
        tiovx_img_mosaic_module_create (graph, mosaic,
        mosaic->background_image[0], input_arr_user, target);
  } else {
    status =
        tiovx_img_mosaic_module_create (graph, mosaic,
        NULL, input_arr_user, target);
  }
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
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects)
{
  GstTIOVXMosaic *mosaic = NULL;
  GstTIOVXMisoPad *background_pad = NULL;
  GList *l = NULL;
  gint i = 0;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);
  g_return_val_if_fail (node, FALSE);

  mosaic = GST_TIOVX_MOSAIC (agg);

  for (l = sink_pads_list; l; l = l->next) {
    GstTIOVXMisoPad *pad = l->data;

    if (!GST_TIOVX_IS_MOSAIC_PAD (pad)) {
      background_pad = pad;
      continue;
    }

    gst_tiovx_miso_pad_set_params (pad, mosaic->obj.inputs[i].arr[0],
        (vx_reference *) & mosaic->obj.inputs[i].image_handle[0],
        mosaic->obj.inputs[i].graph_parameter_index, input_param_id_start + i);
    i++;
  }

  gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (src_pad),
      NULL, (vx_reference *) & mosaic->obj.output_image[0],
      mosaic->obj.output_graph_parameter_index, output_param_id);

  if (background_pad) {
    /* Background image isn't queued/dequeued use -1 to skip it */
    gst_tiovx_miso_pad_set_params (background_pad,
        NULL, (vx_reference *) & mosaic->obj.background_image[0], -1, -1);
  }

  *node = mosaic->obj.node;
  return TRUE;
}

static gboolean
gst_tiovx_mosaic_configure_module (GstTIOVXMiso * agg)
{
  GstTIOVXMosaic *self = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (agg, FALSE);

  self = GST_TIOVX_MOSAIC (agg);
  ret = gst_tiovx_mosaic_load_mosaic_module_objects (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to load Mosaic module objects");
    goto out;
  }

out:
  return ret;
}

static gboolean
gst_tiovx_mosaic_release_buffer (GstTIOVXMiso * agg)
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

static gboolean
gst_tiovx_mosaic_deinit_module (GstTIOVXMiso * agg)
{
  GstTIOVXMosaic *self = NULL;
  TIOVXImgMosaicModuleObj *mosaic = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;

  g_return_val_if_fail (agg, FALSE);

  self = GST_TIOVX_MOSAIC (agg);
  mosaic = &self->obj;

  if (mosaic->background_image[0]) {
    gst_tiovx_empty_exemplar ((vx_reference) mosaic->background_image[0]);
  }

  if (self->background_image_memory) {
    gst_memory_unref (self->background_image_memory);
    self->background_image_memory = NULL;
  }

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

  self->has_background_image = FALSE;
  self->has_background_pad = FALSE;

  ret = TRUE;
out:
  return ret;
}

static gboolean
gst_tiovx_mosaic_validate_candidate_dimension (GstTIOVXMiso * self,
    GstStructure * s, const gchar * dimension_name,
    const gint candidate_dimension)
{
  const GValue *dimension;
  gint dim_max = 0;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (s, FALSE);
  g_return_val_if_fail (dimension_name, FALSE);

  dimension = gst_structure_get_value (s, dimension_name);

  if (GST_VALUE_HOLDS_INT_RANGE (dimension)) {
    dim_max = gst_value_get_int_range_max (dimension);
  } else {
    dim_max = g_value_get_int (dimension);
  }

  /* Other cases are considered by gst_structure_fixate_field_nearest_int,
   * however if the maximum caps dimension is smaller than the minimum candidate dimension
   * we need to fail, since this will lock the HW.
   */
  ret = (candidate_dimension <= dim_max);

  if (!ret) {
    GST_ERROR_OBJECT (self,
        "Minimum required %s: %d is larger than maximum source caps %s: %d",
        dimension_name, candidate_dimension, dimension_name, dim_max);
  }

  return ret;
}

static void
gst_tiovx_mosaic_fixate_structure_fields (GstStructure * structure, gint width,
    gint height, gint fps_n, gint fps_d, const gchar * format)
{
  gst_structure_fixate_field_nearest_int (structure, "width", width);
  gst_structure_fixate_field_nearest_int (structure, "height", height);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", fps_n,
      fps_d);
  gst_structure_fixate_field_string (structure, "format", format);
}

static GstCaps *
gst_tiovx_mosaic_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels)
{
  GstCaps *output_caps = NULL, *candidate_output_caps = NULL;
  GList *l = NULL;
  gint best_width = -1, best_height = -1;
  gint best_fps_n = -1, best_fps_d = -1;
  const gchar *format_str = NULL;
  gdouble best_fps = 0.;
  GstStructure *candidate_output_structure = NULL;
  GstPad *background_pad = NULL;
  gboolean ret = FALSE;
  gint i = 0, ret_val = -1;

  g_return_val_if_fail (self, output_caps);
  g_return_val_if_fail (sink_caps_list, output_caps);
  g_return_val_if_fail (src_caps, output_caps);

  GST_INFO_OBJECT (self, "Fixating caps");

  candidate_output_caps = gst_caps_make_writable (gst_caps_copy (src_caps));

  GST_OBJECT_LOCK (self);
  for (l = GST_ELEMENT (self)->sinkpads; l; l = g_list_next (l)) {
    GstPad *sink_pad = l->data;
    GstTIOVXMosaicPad *mosaic_pad = NULL;
    GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
    GstVideoInfo video_info = {
    };
    GstCaps *caps = NULL;
    gint this_width = 0, this_height = 0;
    guint width = 0, height = 0;
    gint fps_n = 0, fps_d = 0;
    gdouble cur_fps = 0;
    guint j = 0;

    if (!GST_TIOVX_IS_MOSAIC_PAD (sink_pad)) {
      /* Only the background can be a a non-mosaic sink pad */
      background_pad = sink_pad;
      continue;
    }

    mosaic_pad = GST_TIOVX_MOSAIC_PAD (sink_pad);
    caps = gst_pad_get_current_caps (sink_pad);
    if (NULL == caps) {
      GST_ERROR_OBJECT (self, "Failed to get caps from pad: %"
          GST_PTR_FORMAT, sink_pad);
      goto out;
    }
    ret = gst_video_info_from_caps (&video_info, caps);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps);
      gst_caps_unref (caps);
      goto out;
    }
    gst_caps_unref (caps);

    /* Correct number of channels comes from the sinks */
    if ((0 >= gst_caps_get_size (caps))
        || !gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "num-channels", num_channels)) {
      *num_channels = 1;
    }

    if (format == GST_VIDEO_FORMAT_UNKNOWN) {
      format = video_info.finfo->format;
      format_str = video_info.finfo->name;
    } else if (format != video_info.finfo->format) {
      GST_ERROR_OBJECT (self, "All sink format should have same format");
      goto out;
    }

    fps_n = GST_VIDEO_INFO_FPS_N (&video_info);
    fps_d = GST_VIDEO_INFO_FPS_D (&video_info);

    for (j = 0; j < MAX_NUM_CHANNELS; j++) {
      if (mosaic_pad->enabled_channels[j] < 0) {
        continue;
      }

      gst_tiovx_mosaic_check_dimension (GST_TIOVX_MOSAIC (self),
          mosaic_pad->width[j], &width, GST_VIDEO_INFO_WIDTH (&video_info),
          "width");
      gst_tiovx_mosaic_check_dimension (GST_TIOVX_MOSAIC (self),
          mosaic_pad->height[j], &height, GST_VIDEO_INFO_HEIGHT (&video_info),
          "height");

      if (width == 0 || height == 0)
        continue;
      this_width = width + MAX (mosaic_pad->startx[j], 0);
      this_height = height + MAX (mosaic_pad->starty[j], 0);

      if (best_width < this_width) {
        best_width = this_width;
      }
      if (best_height < this_height) {
        best_height = this_height;
      }
    }

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

  if (background_pad) {
    GstVideoInfo video_info = { };
    GstCaps *caps = NULL;
    gint candidate_width = 0, candidate_height = 0;
    guint background_width = 0, background_height = 0;

    caps = gst_pad_get_current_caps (background_pad);
    ret = gst_video_info_from_caps (&video_info, caps);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT " in pad: %" GST_PTR_FORMAT, caps, background_pad);
      gst_caps_unref (caps);
      goto out;
    }
    gst_caps_unref (caps);

    background_width = GST_VIDEO_INFO_WIDTH (&video_info);
    background_height = GST_VIDEO_INFO_HEIGHT (&video_info);

    if ((best_width <= background_width) && (best_height <= background_height)) {
      best_width = background_width;
      best_height = background_height;
    } else {
      GST_ERROR_OBJECT (self,
          "Minimum required width and height for windows: (%d, %d) is"
          " larger than background image dimensions: (%d, %d)", best_width,
          best_height, background_width, background_height);
      goto out;
    }

    for (i = 0; i < gst_caps_get_size (candidate_output_caps); i++) {
      candidate_output_structure =
          gst_caps_get_structure (candidate_output_caps, i);

      gst_tiovx_mosaic_fixate_structure_fields (candidate_output_structure,
          best_width, best_height, best_fps_n, best_fps_d, format_str);

      /* When there is a background image, the fixation for width & height needs
       * to be successful (exact), check that the fixated values match the
       * background dimensions */
      ret =
          gst_structure_get_int (candidate_output_structure, "width",
          &candidate_width);
      ret =
          gst_structure_get_int (candidate_output_structure, "height",
          &candidate_height);

      if (ret) {
        GST_ERROR_OBJECT (self,
            "Width and height couldn't be fixated to : %" GST_PTR_FORMAT,
            candidate_output_structure);
      } else if ((candidate_width != best_width)
          || (candidate_height != best_height)) {
        GST_ERROR_OBJECT (self,
            "Could not fixate: (%d, %d) to current source caps: %"
            GST_PTR_FORMAT, best_width, best_height,
            candidate_output_structure);
      }

      ret_val = g_strcmp0 (format_str,
          gst_structure_get_string (candidate_output_structure, "format"));
      if (ret_val) {
        GST_ERROR_OBJECT (self, "Could not fixate output format");
        goto out;
      }
    }

  } else {
    for (i = 0; i < gst_caps_get_size (candidate_output_caps); i++) {
      candidate_output_structure =
          gst_caps_get_structure (candidate_output_caps, i);

      /* When there is no background pad, we only care that minimum width/height
       * according to input windows is smaller than the output range max value  */
      if (!gst_tiovx_mosaic_validate_candidate_dimension (self,
              candidate_output_structure, "width", best_width)
          || !gst_tiovx_mosaic_validate_candidate_dimension (self,
              candidate_output_structure, "height", best_height)) {
        GST_ERROR_OBJECT (self,
            "Minimum required width and height for windows: (%d, %d) is"
            " larger than current source caps: %" GST_PTR_FORMAT, best_width,
            best_height, candidate_output_structure);
        goto out;
      }

      gst_tiovx_mosaic_fixate_structure_fields (candidate_output_structure,
          best_width, best_height, best_fps_n, best_fps_d, format_str);

      ret_val = g_strcmp0 (format_str,
          gst_structure_get_string (candidate_output_structure, "format"));
      if (ret_val) {
        GST_ERROR_OBJECT (self, "Could not fixate output format");
        goto out;
      }
    }
  }

  output_caps = gst_caps_intersect (candidate_output_caps, src_caps);
  output_caps = gst_caps_fixate (output_caps);

  GST_DEBUG_OBJECT (self, "Fixated %" GST_PTR_FORMAT " into %" GST_PTR_FORMAT,
      src_caps, output_caps);

out:
  gst_caps_unref (candidate_output_caps);

  return output_caps;
}

static const gchar *
target_id_to_target_name (gint target_id)
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

static gboolean
gst_tiovx_mosaic_load_background_image (GstTIOVXMosaic * self,
    GstMemory ** memory, vx_image background_img)
{
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  void *addr[MODULE_MAX_NUM_PLANES] = { NULL };
  void *plane_addr[MODULE_MAX_NUM_PLANES] = { NULL };
  uint32_t plane_sizes[MODULE_MAX_NUM_PLANES] = { 0 };
  guint num_planes = 0;
  vx_size data_size = 0;
  GstTIOVXMemoryData *ti_memory = NULL;
  FILE *background_img_file = NULL;
  gint file_size = 0;
  void *img_buffer = NULL;
  guint i = 0;
  gint width = 0;
  gint height = 0;
  vx_rectangle_t rectangle = { 0 };
  vx_imagepatch_addressing_t image_addr = { 0 };
  vx_map_id map_id = 0;
  guint planes_offset = 0;
  gint plane_rows = 0;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (memory, FALSE);
  g_return_val_if_fail (background_img, FALSE);

  /* Get plane and size info */
  status = tivxReferenceExportHandle (
      (const vx_reference) background_img,
      plane_addr, (uint32_t *) plane_sizes, MODULE_MAX_NUM_PLANES, &num_planes);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Unable to retrieve plane and size info from VX image: %p",
        background_img);
    goto out;
  }
  GST_DEBUG_OBJECT (self, "Number of planes for background image: %d",
      num_planes);

  status =
      vxQueryImage (background_img, VX_IMAGE_SIZE, &data_size,
      sizeof (data_size));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Unable to retrieve image size from VX image: %p", background_img);
    goto out;
  }
  GST_DEBUG_OBJECT (self, "Data size for background image: %ld", data_size);

  status =
      vxQueryImage (background_img, VX_IMAGE_WIDTH, &width, sizeof (width));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Unable to retrieve image width from VX image: %p", background_img);
    goto out;
  }
  GST_DEBUG_OBJECT (self, "Width for background image: %d", width);

  status =
      vxQueryImage (background_img, VX_IMAGE_HEIGHT, &height, sizeof (height));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Unable to retrieve image height from VX image: %p", background_img);
    goto out;
  }
  GST_DEBUG_OBJECT (self, "Height for background image: %d", height);

  rectangle.start_x = 0;
  rectangle.start_y = 0;
  rectangle.end_x = width;
  rectangle.end_y = height;

  /* Alloc GStreamer memory */
  *memory =
      gst_allocator_alloc (GST_ALLOCATOR (self->user_data_allocator), data_size,
      NULL);
  if (NULL == *memory) {
    GST_ERROR_OBJECT (self, "Unable to allocate GStreamer memory");
    goto out;
  }

  /* Alloc TI memory */
  ti_memory = gst_tiovx_memory_get_data (*memory);
  if (!ti_memory) {
    GST_ERROR_OBJECT (self, "Unable to retrieve TI memory");
    goto out;
  }

  /* Read out the background image file */
  background_img_file = fopen (self->background, "rb");
  if (!background_img_file) {
    GST_ERROR_OBJECT (self, "Unable to open the background image file: %s",
        self->background);
    goto out;
  }

  fseek (background_img_file, 0, SEEK_END);
  file_size = ftell (background_img_file);
  fseek (background_img_file, 0, SEEK_SET);

  if (0 > file_size) {
    GST_ERROR_OBJECT (self, "File: %s has invalid size", self->background);
    fclose (background_img_file);
    goto out;
  }

  /* Organize the memory per plane pointers */
  for (i = 0; i < num_planes; i++) {
    guint j = 0;
    gint width_per_plane = 0;

    addr[i] = ((char *) ti_memory->mem_ptr.host_ptr + planes_offset);

    if (data_size != file_size) {
      GST_DEBUG_OBJECT (self,
          "Got background image with padding; width doesn't match stride");

      status =
          vxMapImagePatch (background_img, &rectangle, i, &map_id, &image_addr,
          &img_buffer, VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST, VX_NOGAP_X);
      if (VX_SUCCESS != status) {
        GST_ERROR_OBJECT (self,
            "Unable to map VX image with rectagular patch: %p", background_img);
        goto out;
      }

      width_per_plane =
          ((image_addr.dim_x * image_addr.stride_x) / image_addr.step_x);
      plane_rows = image_addr.dim_y / image_addr.step_y;

      for (j = 0; j < plane_rows; j++) {
        fread (addr[i], 1, width_per_plane, background_img_file);
        addr[i] = (char *) addr[i] + image_addr.stride_y;
      }

      /* Return pointer to plane base */
      addr[i] = ((char *) ti_memory->mem_ptr.host_ptr + planes_offset);

      vxUnmapImagePatch (background_img, map_id);
    }

    planes_offset += plane_sizes[i];
  }

  if (data_size == file_size) {
    GST_DEBUG_OBJECT (self,
        "Got background image with no padding; width matches stride");
    fread (*addr, 1, planes_offset, background_img_file);
  }

  fclose (background_img_file);

  status =
      tivxReferenceImportHandle ((vx_reference) background_img,
      (const void **) addr, (const uint32_t *) plane_sizes, num_planes);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Unable to import handles to exemplar: %p", background_img);
    goto out;
  }

  ret = TRUE;

out:
  if (!ret && *memory) {
    gst_memory_unref (*memory);
    *memory = NULL;
  }

  return ret;
}

static gboolean
gst_tiovx_mosaic_load_mosaic_module_objects (GstTIOVXMosaic * self)
{
  gboolean ret = FALSE;
  TIOVXImgMosaicModuleObj *mosaic = NULL;

  g_return_val_if_fail (self, FALSE);

  GST_DEBUG_OBJECT (self, "Load Mosaic module objects");

  mosaic = &self->obj;
  if (self->background_image_memory) {
    gst_memory_unref (self->background_image_memory);
    self->background_image_memory = NULL;
  }
  if (self->has_background_image) {
    if (mosaic->background_image[0]) {
      gst_tiovx_empty_exemplar ((vx_reference) mosaic->background_image[0]);
    }
    ret =
        gst_tiovx_mosaic_load_background_image (self,
        &self->background_image_memory, mosaic->background_image[0]);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Unable to load data for background image");
      goto out;
    }
  }

  ret = TRUE;

out:
  return ret;
}

static void
gst_tiovx_mosaic_finalize (GObject * object)
{
  GstTIOVXMosaic *self = GST_TIOVX_MOSAIC (object);

  GST_LOG_OBJECT (self, "mosaic_finalize");

  g_free (self->background);
  self->background = NULL;

  if (self->user_data_allocator) {
    gst_object_unref (self->user_data_allocator);
    self->user_data_allocator = NULL;
  }
  G_OBJECT_CLASS (gst_tiovx_mosaic_parent_class)->finalize (object);
}
