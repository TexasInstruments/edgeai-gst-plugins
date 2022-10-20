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
#include "gst-libs/gst/tiovx/gsttiovxpyramidmeta.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_pyramid_module.h"

#define PYRAMID_INPUT_PARAM_INDEX 0
#define PYRAMID_OUTPUT_PARAM_INDEX 1
#define PYRAMID_MIN_RESOLUTION 64

/* Target definition */
enum
{
  TIVX_TARGET_VPAC_MSC1_ID = 0,
  TIVX_TARGET_VPAC_MSC2_ID,
};

#define GST_TYPE_TIOVX_PYRAMID_TARGET (gst_tiovx_pyramid_target_get_type())
static GType
gst_tiovx_pyramid_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_MSC1_ID, "VPAC MSC1", TIVX_TARGET_VPAC_MSC1},
    {TIVX_TARGET_VPAC_MSC2_ID, "VPAC MSC2", TIVX_TARGET_VPAC_MSC2},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXPyramidTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_PYRAMID_TARGET TIVX_TARGET_VPAC_MSC1_ID

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
};

/* Formats definition */
#define TIOVX_PYRAMID_SUPPORTED_FORMATS "{NV12, GRAY8, GRAY16_LE}"
#define TIOVX_PYRAMID_SUPPORTED_WIDTH "[1 , 2592]"
#define TIOVX_PYRAMID_SUPPORTED_HEIGHT "[1 , 1952]"
#define TIOVX_PYRAMID_SUPPORTED_LEVELS "[1 , 8]"
#define TIOVX_PYRAMID_SUPPORTED_SCALE "[0.25 , 1.0]"
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
  "num-channels = " TIOVX_PYRAMID_SUPPORTED_CHANNELS

/* Sink caps */
#define TIOVX_PYRAMID_STATIC_CAPS_SINK                       \
  "video/x-raw, "                                            \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "  \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "              \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT                 \
  "; "                                                       \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "       \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "  \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "              \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "            \
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
  gint target_id;
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
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node,
    guint * input_param_index, guint * output_param_index);
static gboolean gst_tiovx_pyramid_release_buffer (GstTIOVXSiso * trans);
static gboolean gst_tiovx_pyramid_deinit_module (GstTIOVXSiso * trans,
    vx_context context);
static gboolean gst_tiovx_pyramid_compare_caps (GstTIOVXSiso * trans,
    GstCaps * caps1, GstCaps * caps2, GstPadDirection direction);
static GstCaps *gst_tiovx_pyramid_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_tiovx_pyramid_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static void gst_tiovx_pyramid_set_max_levels (GstTIOVXPyramid * self,
    const GValue * vwidth, const GValue * vheight, const GValue * vscale,
    const GValue * vout_formats, GValue * vlevels);
static const gchar *target_id_to_target_name (gint target_id);
static void gst_tiovx_pyramid_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_pyramid_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* Initialize the plugin's class */
static void
gst_tiovx_pyramid_class_init (GstTIOVXPyramidClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSisoClass *gsttiovxsiso_class = NULL;
  GstBaseTransformClass *gstbasetransform_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsiso_class = GST_TIOVX_SISO_CLASS (klass);
  gstbasetransform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Pyramid",
      "Filter/Converter/Video",
      "Converts video frames to a pyramid representation using the TIOVX Modules API",
      "RidgeRun support@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_tiovx_pyramid_set_property;
  gobject_class->get_property = gst_tiovx_pyramid_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_PYRAMID_TARGET,
          DEFAULT_TIOVX_PYRAMID_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_transform_caps);
  gstbasetransform_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_fixate_caps);

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
  self->target_id = DEFAULT_TIOVX_PYRAMID_TARGET;
}

static void
gst_tiovx_pyramid_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXPyramid *self = GST_TIOVX_PYRAMID (object);

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
gst_tiovx_pyramid_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXPyramid *self = GST_TIOVX_PYRAMID (object);

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
  gint levels = 0;
  gdouble scale = 0;
  gint width = 0;
  gint height = 0;
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

  ret =
      gst_tioxv_get_pyramid_caps_info ((GObject *) self, GST_CAT_DEFAULT,
      out_caps, &levels, &scale, &width, &height, &format);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to get pyramid info from output caps");
    goto exit;
  }

  /* Configure pyramid */
  pyramid = &self->obj;
  pyramid->num_channels = num_channels;
  pyramid->width = GST_VIDEO_INFO_WIDTH (&in_info);
  pyramid->height = GST_VIDEO_INFO_HEIGHT (&in_info);

  /* Configure input */
  pyramid->input.color_format = gst_format_to_vx_format (in_info.finfo->format);
  pyramid->input.bufq_depth = 1;

  /* Configure output */
  pyramid->output.levels = levels;
  pyramid->output.scale = scale;
  pyramid->output.color_format = gst_format_to_vx_format (format);
  pyramid->output.bufq_depth = 1;

  status = tiovx_pyramid_module_init (context, pyramid);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto exit;
  }

  ret = TRUE;
exit:
  return ret;
}

static GstCaps *
gst_tiovx_pyramid_transform_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIOVXPyramid *self = GST_TIOVX_PYRAMID (base);
  GstCaps *result_caps = NULL, *current_caps = NULL, *tmp = NULL;
  GstStructure *result_structure = NULL, *structure = NULL;
  const GValue *vwidth = NULL, *vheight = NULL, *vformat = NULL, *vscale = NULL;
  GValue vlevels = G_VALUE_INIT;
  guint i = 0;
  const gchar *format_name = NULL;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  GValue output_formats = G_VALUE_INIT;
  gint size = 0;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  /* Get shared values from caps */
  structure = gst_caps_get_structure (caps, 0);
  vwidth = gst_structure_get_value (structure, "width");
  vheight = gst_structure_get_value (structure, "height");
  vformat = gst_structure_get_value (structure, "format");

  g_value_init (&output_formats, GST_TYPE_LIST);

  size = GST_VALUE_HOLDS_LIST (vformat) ? gst_value_list_get_size (vformat) : 1;

  for (i = 0; i < size; i++) {
    const GValue *value = NULL;

    if (GST_VALUE_HOLDS_LIST (vformat)) {
      value = gst_value_list_get_value (vformat, i);
    } else {
      value = vformat;
    }
    format_name = g_value_get_string (value);
    format = gst_video_format_from_string (format_name);
    if ((GST_VIDEO_FORMAT_NV12 == format && GST_PAD_SINK == direction) ||
        (GST_VIDEO_FORMAT_GRAY8 == format && GST_PAD_SRC == direction) ||
        (GST_VIDEO_FORMAT_GRAY16_LE == format && GST_PAD_SRC == direction)) {
      GValue out_value = G_VALUE_INIT;
      g_value_init (&out_value, G_TYPE_STRING);
      g_value_set_string (&out_value, "NV12");
      gst_value_list_append_value (&output_formats, &out_value);
      g_value_set_string (&out_value, "GRAY8");
      gst_value_list_append_value (&output_formats, &out_value);
      g_value_set_string (&out_value, "GRAY16_LE");
      gst_value_list_append_value (&output_formats, &out_value);
      g_value_unset (&out_value);
    } else if ((GST_VIDEO_FORMAT_GRAY8 == format && GST_PAD_SINK == direction)
        || (GST_VIDEO_FORMAT_GRAY16_LE == format && GST_PAD_SINK == direction)) {
      GValue out_value = G_VALUE_INIT;
      g_value_init (&out_value, G_TYPE_STRING);
      g_value_set_string (&out_value, "GRAY8");
      gst_value_list_append_value (&output_formats, &out_value);
      g_value_set_string (&out_value, "GRAY16_LE");
      gst_value_list_append_value (&output_formats, &out_value);
      g_value_unset (&out_value);
    } else {
      gst_value_list_append_value (&output_formats, value);
    }
  }

  if (GST_PAD_SINK == direction) {
    result_caps = gst_caps_from_string (TIOVX_PYRAMID_STATIC_CAPS_SRC);
    current_caps = gst_pad_get_current_caps (base->srcpad);
    if (!current_caps) {
      current_caps = gst_pad_peer_query_caps (base->srcpad, NULL);
    }
    tmp = current_caps;
    current_caps = gst_caps_intersect (result_caps, current_caps);
    gst_caps_unref (tmp);
    tmp = NULL;

    structure = gst_caps_get_structure (current_caps, 0);
    vscale = gst_structure_get_value (structure, "scale");
    gst_tiovx_pyramid_set_max_levels (self, vwidth, vheight, vscale,
        &output_formats, &vlevels);
    gst_caps_unref (current_caps);
    current_caps = NULL;

    if (!G_IS_VALUE (&vlevels)) {
      gst_caps_unref (result_caps);
      return NULL;
    }
  } else {
    result_caps = gst_caps_from_string (TIOVX_PYRAMID_STATIC_CAPS_SINK);
  }

  /* Set shared values in transformed caps */
  result_caps = gst_caps_make_writable (result_caps);
  for (i = 0; i < gst_caps_get_size (result_caps); i++) {
    result_structure = gst_caps_get_structure (result_caps, i);
    gst_structure_set_value (result_structure, "width", vwidth);
    gst_structure_set_value (result_structure, "height", vheight);
    gst_structure_set_value (result_structure, "format", &output_formats);
    if (G_IS_VALUE (&vlevels) && GST_PAD_SINK == direction) {
      gst_structure_set_value (result_structure, "levels", &vlevels);
    }
  }

  if (filter) {
    tmp = result_caps;
    result_caps = gst_caps_intersect (result_caps, filter);
    gst_caps_unref (tmp);
    tmp = NULL;
  }

  GST_DEBUG_OBJECT (self, "Resulting caps are %" GST_PTR_FORMAT, result_caps);

  return result_caps;
}

static GstCaps *
gst_tiovx_pyramid_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{

  GstTIOVXPyramid *self = GST_TIOVX_PYRAMID (base);
  GstCaps *output_caps = NULL;
  GstStructure *structure = NULL;
  const GValue *vwidth = NULL, *vheight = NULL, *vscale = NULL, *vformat =
      NULL, *vlevels = NULL;
  GValue allowed_levels = G_VALUE_INIT;
  gint max_levels = 0;

  /* Fixate caps */
  output_caps = gst_caps_fixate (othercaps);

  /* Validate fixated values */
  structure = gst_caps_get_structure (output_caps, 0);
  vwidth = gst_structure_get_value (structure, "width");
  vheight = gst_structure_get_value (structure, "height");
  vscale = gst_structure_get_value (structure, "scale");
  vlevels = gst_structure_get_value (structure, "levels");
  vformat = gst_structure_get_value (structure, "format");

  gst_tiovx_pyramid_set_max_levels (self, vwidth, vheight, vscale, vformat,
      &allowed_levels);
  if (GST_VALUE_HOLDS_INT_RANGE (&allowed_levels)) {
    max_levels = gst_value_get_int_range_max (&allowed_levels);
  } else {
    max_levels = g_value_get_int (&allowed_levels);
  }

  if (max_levels >= g_value_get_int (vlevels)) {
    GST_DEBUG_OBJECT (base, "Fixated to %" GST_PTR_FORMAT, output_caps);
  } else {
    GST_ERROR_OBJECT (base,
        "Invalid levels, couldn't fixate to %" GST_PTR_FORMAT, output_caps);
    gst_caps_unref (output_caps);
    output_caps = gst_caps_new_empty ();
  }

  return output_caps;
}

static void
gst_tiovx_pyramid_set_max_levels (GstTIOVXPyramid * self,
    const GValue * vwidth, const GValue * vheight, const GValue * vscale,
    const GValue * vout_formats, GValue * vlevels)
{
  guint i = 0;
  gdouble max_scale = 0;
  gint max_width = 0, max_height = 0;
  gint max_levels = MODULE_MAX_NUM_PYRAMIDS, min_levels = 1;
  gint size = 0;
  gboolean requires_even_resolution = TRUE;

  g_return_if_fail (self);
  g_return_if_fail (vwidth);
  g_return_if_fail (vheight);
  g_return_if_fail (vscale);
  g_return_if_fail (vout_formats);
  g_return_if_fail (vlevels);

  GST_DEBUG_OBJECT (self, "Calculating pyramid maximum levels");

  /* Get max width */
  if (GST_VALUE_HOLDS_INT_RANGE (vwidth)) {
    max_width = gst_value_get_int_range_max (vwidth);
  } else {
    max_width = g_value_get_int (vwidth);
  }
  /* Get max height */
  if (GST_VALUE_HOLDS_INT_RANGE (vheight)) {
    max_height = gst_value_get_int_range_max (vheight);
  } else {
    max_height = g_value_get_int (vheight);
  }
  /* Get max scale */
  if (GST_VALUE_HOLDS_DOUBLE_RANGE (vscale)) {
    max_scale = gst_value_get_double_range_max (vscale);
  } else {
    max_scale = g_value_get_double (vscale);
  }

  size =
      GST_VALUE_HOLDS_LIST (vout_formats) ?
      gst_value_list_get_size (vout_formats) : 1;
  for (i = 0; i < size; i++) {
    const GValue *value = NULL;
    const gchar *format_name = NULL;
    GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

    if (GST_VALUE_HOLDS_LIST (vout_formats)) {
      value = gst_value_list_get_value (vout_formats, i);
    } else {
      value = vout_formats;
    }
    format_name = g_value_get_string (value);
    format = gst_video_format_from_string (format_name);
    if (GST_VIDEO_FORMAT_NV12 != format) {
      requires_even_resolution = FALSE;
      break;
    }
  }

  /* Calculate max levels */
  for (i = 0; i <= MODULE_MAX_NUM_PYRAMIDS; i++) {
    if ((PYRAMID_MIN_RESOLUTION > max_width)
        || (PYRAMID_MIN_RESOLUTION > max_height) || (requires_even_resolution
            && ((0 != max_width % 2) || (0 != max_height % 2)))) {
      max_levels = i;
      GST_DEBUG_OBJECT (self, "Maximum levels allowed: %d", max_levels);
      break;
    }
    max_width = max_width * max_scale;
    max_height = max_height * max_scale;
  }
  if (max_levels > min_levels) {
    g_value_init (vlevels, GST_TYPE_INT_RANGE);
    gst_value_set_int_range (vlevels, min_levels, max_levels);
  } else {
    g_value_init (vlevels, G_TYPE_INT);
    g_value_set_int (vlevels, min_levels);
  }
}

static gboolean
gst_tiovx_pyramid_create_graph (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph)
{
  GstTIOVXPyramid *self = NULL;
  vx_status status = VX_SUCCESS;
  const char *target = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  self = GST_TIOVX_PYRAMID (trans);

  GST_INFO_OBJECT (self, "Create graph");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

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
    vx_object_array * input, vx_object_array * output,
    vx_reference * input_ref, vx_reference * output_ref, vx_node * node,
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
  *input = self->obj.input.arr[0];
  *output = self->obj.output.arr[0];
  *input_ref = (vx_reference) self->obj.input.image_handle[0];
  *output_ref = (vx_reference) self->obj.output.pyramid_handle[0];

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

static const gchar *
target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_pyramid_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}
