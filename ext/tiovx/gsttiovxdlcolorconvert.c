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

#include "gsttiovxdlcolorconvert.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_dl_color_convert_module.h"

#define DL_COLOR_CONVERT_INPUT_PARAM_INDEX 0
#define DL_COLOR_CONVERT_OUTPUT_PARAM_INDEX 1

/* Target definition */
#define GST_TYPE_TIOVX_DL_COLOR_CONVERT_TARGET (gst_tiovx_dl_color_convert_target_get_type())
static GType
gst_tiovx_dl_color_convert_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_CPU_ID_A72_0, "A72 instance 1, assigned to A72_0 core",
        TIVX_TARGET_A72_0},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type =
        g_enum_register_static ("GstTIOVXDLColorConvertTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_DL_COLOR_CONVERT_TARGET TIVX_CPU_ID_A72_0

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
};

/* Formats definition */
#define TIOVX_DL_COLOR_CONVERT_SUPPORTED_FORMATS_SRC "{RGB, NV12, I420}"
#define TIOVX_DL_COLOR_CONVERT_SUPPORTED_FORMATS_SINK "{RGB, NV12, NV21, I420}"
#define TIOVX_DL_COLOR_CONVERT_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_DL_COLOR_CONVERT_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_DL_COLOR_CONVERT_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_DL_COLOR_CONVERT_STATIC_CAPS_SRC                           \
  "video/x-raw, "                                                     \
  "format = (string) " TIOVX_DL_COLOR_CONVERT_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_HEIGHT                    \
  "; "                                                                \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "                \
  "format = (string) " TIOVX_DL_COLOR_CONVERT_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_HEIGHT ", "               \
  "num-channels = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_CHANNELS            \

/* Sink caps */
#define TIOVX_DL_COLOR_CONVERT_STATIC_CAPS_SINK                           \
  "video/x-raw, "                                                      \
  "format = (string) " TIOVX_DL_COLOR_CONVERT_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_HEIGHT                     \
  "; "                                                                 \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "                 \
  "format = (string) " TIOVX_DL_COLOR_CONVERT_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_HEIGHT ", "                \
  "num-channels = " TIOVX_DL_COLOR_CONVERT_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DL_COLOR_CONVERT_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DL_COLOR_CONVERT_STATIC_CAPS_SRC)
    );

struct _GstTIOVXDLColorconvert
{
  GstTIOVXSiso element;
  gint target_id;
  TIOVXDLColorConvertModuleObj obj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_dl_color_convert_debug);
#define GST_CAT_DEFAULT gst_tiovx_dl_color_convert_debug

#define gst_tiovx_dl_color_convert_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXDLColorconvert, gst_tiovx_dl_color_convert,
    GST_TYPE_TIOVX_SISO);

static void
gst_tiovx_dl_color_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_dl_color_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_tiovx_dl_color_convert_transform_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * filter);

static gboolean gst_tiovx_dl_color_convert_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels);
static gboolean gst_tiovx_dl_color_convert_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_dl_color_convert_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node,
    guint * input_param_index, guint * output_param_index);
static gboolean gst_tiovx_dl_color_convert_release_buffer (GstTIOVXSiso *
    trans);
static gboolean gst_tiovx_dl_color_convert_deinit_module (GstTIOVXSiso * trans,
    vx_context context);
static gboolean gst_tiovx_dl_color_convert_compare_caps (GstTIOVXSiso * trans,
    GstCaps * caps1, GstCaps * caps2, GstPadDirection direction);

static const gchar *target_id_to_target_name (gint target_id);

/* Initialize the plugin's class */
static void
gst_tiovx_dl_color_convert_class_init (GstTIOVXDLColorconvertClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstBaseTransformClass *gstbasetransform_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSisoClass *gsttiovxsiso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasetransform_class = GST_BASE_TRANSFORM_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsiso_class = GST_TIOVX_SISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX DL ColorConvert",
      "Filter/Converter/Video",
      "Converts video from one colorspace to another using the TIOVX Modules API",
      "RidgeRun support@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_tiovx_dl_color_convert_set_property;
  gobject_class->get_property = gst_tiovx_dl_color_convert_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_DL_COLOR_CONVERT_TARGET,
          DEFAULT_TIOVX_DL_COLOR_CONVERT_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_convert_transform_caps);
  /* Disable processing if input & output caps are equal, i.e., no format convertion */
  gstbasetransform_class->passthrough_on_same_caps = TRUE;
  gstbasetransform_class->transform_ip_on_passthrough = FALSE;

  gsttiovxsiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_convert_init_module);
  gsttiovxsiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_convert_create_graph);
  gsttiovxsiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_convert_get_node_info);
  gsttiovxsiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_convert_release_buffer);
  gsttiovxsiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_convert_deinit_module);
  gsttiovxsiso_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_convert_compare_caps);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_dl_color_convert_debug,
      "tiovxdlcolorconvert", 0, "TIOVX DL ColorConvert element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_dl_color_convert_init (GstTIOVXDLColorconvert * self)
{
  self->target_id = DEFAULT_TIOVX_DL_COLOR_CONVERT_TARGET;
  memset (&self->obj, 0, sizeof self->obj);
}

static void
gst_tiovx_dl_color_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXDLColorconvert *self = GST_TIOVX_DL_COLOR_CONVERT (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_tiovx_dl_color_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXDLColorconvert *self = GST_TIOVX_DL_COLOR_CONVERT (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

typedef gboolean (*AppendFormatFunc) (GstVideoFormat, GValue *);

static void
append_format_to_list (GValue * list, const gchar * format)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (list);
  g_return_if_fail (format);
  g_return_if_fail (GST_VALUE_HOLDS_LIST (list));

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, format);

  gst_value_list_append_value (list, &value);
  g_value_unset (&value);
}

static gboolean
append_sink_formats (GstVideoFormat src_format, GValue * sink_formats)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (sink_formats, FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_LIST (sink_formats), FALSE);

  switch (src_format) {
    case GST_VIDEO_FORMAT_RGB:
      append_format_to_list (sink_formats, "NV12");
      append_format_to_list (sink_formats, "NV21");
      append_format_to_list (sink_formats, "RGB");
      break;
    case GST_VIDEO_FORMAT_NV12:
      append_format_to_list (sink_formats, "RGB");
      append_format_to_list (sink_formats, "I420");
      append_format_to_list (sink_formats, "NV12");
      break;
    case GST_VIDEO_FORMAT_I420:
      append_format_to_list (sink_formats, "NV12");
      append_format_to_list (sink_formats, "NV21");
      append_format_to_list (sink_formats, "I420");
      break;
    default:
      ret = FALSE;
      break;
  }

  return ret;
}

static gboolean
append_src_formats (GstVideoFormat sink_format, GValue * src_formats)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (src_formats, FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_LIST (src_formats), FALSE);

  switch (sink_format) {
    case GST_VIDEO_FORMAT_RGB:
      append_format_to_list (src_formats, "NV12");
      append_format_to_list (src_formats, "RGB");
      break;
    case GST_VIDEO_FORMAT_I420:
      append_format_to_list (src_formats, "NV12");
      append_format_to_list (src_formats, "I420");
      break;
    case GST_VIDEO_FORMAT_NV12:
      append_format_to_list (src_formats, "RGB");
      append_format_to_list (src_formats, "I420");
      append_format_to_list (src_formats, "NV12");
      break;
    case GST_VIDEO_FORMAT_NV21:
      append_format_to_list (src_formats, "RGB");
      append_format_to_list (src_formats, "I420");
      append_format_to_list (src_formats, "NV21");
      break;
    default:
      ret = FALSE;
      break;
  }

  return ret;
}

static gboolean
get_formats (const GValue * input_formats, GValue * output_formats,
    AppendFormatFunc cb)
{
  gint i = 0;
  gboolean ret = TRUE;
  gint size = 0;

  g_return_val_if_fail (input_formats, FALSE);
  g_return_val_if_fail (output_formats, FALSE);
  g_return_val_if_fail (GST_VALUE_HOLDS_LIST (output_formats), FALSE);

  size =
      GST_VALUE_HOLDS_LIST (input_formats) ?
      gst_value_list_get_size (input_formats) : 1;

  for (i = 0; i < size; i++) {
    const GValue *value = NULL;
    const gchar *format_name = NULL;
    GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

    if (GST_VALUE_HOLDS_LIST (input_formats)) {
      value = gst_value_list_get_value (input_formats, i);
    } else {
      value = input_formats;
    }

    format_name = g_value_get_string (value);
    format = gst_video_format_from_string (format_name);

    if (FALSE == cb (format, output_formats)) {
      ret = FALSE;
      break;
    }
  }

  return ret;
}

static GstCaps *
gst_tiovx_dl_color_convert_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIOVXDLColorconvert *self = GST_TIOVX_DL_COLOR_CONVERT (base);
  GstCaps *result_caps = NULL;
  gint i = 0;
  AppendFormatFunc func;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  result_caps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (result_caps); i++) {
    GstStructure *st = gst_caps_get_structure (result_caps, i);
    GstStructure *st_copy = gst_structure_copy (st);
    const GValue *input_formats = gst_structure_get_value (st, "format");
    GValue output_formats = G_VALUE_INIT;
    const GValue *tmp_val = NULL;

    g_value_init (&output_formats, GST_TYPE_LIST);

    if (GST_PAD_SINK == direction) {
      func = append_src_formats;
    } else {
      func = append_sink_formats;
    }

    get_formats (input_formats, &output_formats, func);
    gst_structure_remove_all_fields (st);

    /* Set tranformed format */
    gst_structure_set_value (st, "format", &output_formats);
    g_value_unset (&output_formats);

    /* Copy other fields as it is */
    tmp_val = gst_structure_get_value (st_copy, "width");
    if (tmp_val != NULL) {
      gst_structure_set_value (st, "width", tmp_val);
    }

    tmp_val = gst_structure_get_value (st_copy, "height");
    if (tmp_val != NULL) {
      gst_structure_set_value (st, "height", tmp_val);
    }

    tmp_val = gst_structure_get_value (st_copy, "framerate");
    if (tmp_val != NULL) {
      gst_structure_set_value (st, "framerate", tmp_val);
    }

    gst_structure_free (st_copy);
  }

  if (filter) {
    GstCaps *tmp = result_caps;
    result_caps = gst_caps_intersect (result_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_DEBUG_OBJECT (self, "Resulting caps are %" GST_PTR_FORMAT, result_caps);

  return result_caps;
}

/* GstTIOVXSiso Functions */

static gboolean
gst_tiovx_dl_color_convert_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels)
{
  GstTIOVXDLColorconvert *self = NULL;
  vx_status status = VX_SUCCESS;
  TIOVXDLColorConvertModuleObj *colorconvert = NULL;
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (in_caps, FALSE);
  g_return_val_if_fail (out_caps, FALSE);
  g_return_val_if_fail (num_channels >= MIN_NUM_CHANNELS, FALSE);
  g_return_val_if_fail (num_channels <= MAX_NUM_CHANNELS, FALSE);

  self = GST_TIOVX_DL_COLOR_CONVERT (trans);

  GST_INFO_OBJECT (self, "Init module");

  if (!gst_video_info_from_caps (&in_info, in_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get video info from input caps");
    goto exit;
  }
  if (!gst_video_info_from_caps (&out_info, out_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get video info from output caps");
    goto exit;
  }

  /* Configure TIOVXDLColorConvertModuleObj */
  colorconvert = &self->obj;
  colorconvert->num_channels = num_channels;
  colorconvert->input.bufq_depth = 1;
  colorconvert->output.bufq_depth = 1;

  colorconvert->input.graph_parameter_index = INPUT_PARAMETER_INDEX;
  colorconvert->output.graph_parameter_index = OUTPUT_PARAMETER_INDEX;

  colorconvert->input.color_format =
      gst_format_to_vx_format (in_info.finfo->format);
  colorconvert->output.color_format =
      gst_format_to_vx_format (out_info.finfo->format);

  colorconvert->width = GST_VIDEO_INFO_WIDTH (&in_info);
  colorconvert->height = GST_VIDEO_INFO_HEIGHT (&in_info);

  colorconvert->en_out_image_write = 0;

  status = tiovx_dl_color_convert_module_init (context, colorconvert);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto exit;
  }
  ret = TRUE;

exit:

  return ret;
}

static gboolean
gst_tiovx_dl_color_convert_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph)
{
  GstTIOVXDLColorconvert *self = NULL;
  vx_status status = VX_SUCCESS;
  const char *target = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  self = GST_TIOVX_DL_COLOR_CONVERT (trans);

  GST_INFO_OBJECT (self, "Create graph");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (NULL == target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    goto out;
  }

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  status =
      tiovx_dl_color_convert_module_create (graph, &self->obj, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_dl_color_convert_release_buffer (GstTIOVXSiso * trans)
{
  GstTIOVXDLColorconvert *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_DL_COLOR_CONVERT (trans);

  GST_INFO_OBJECT (self, "Release buffer");

  status = tiovx_dl_color_convert_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dl_color_convert_deinit_module (GstTIOVXSiso * trans,
    vx_context context)
{
  GstTIOVXDLColorconvert *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_DL_COLOR_CONVERT (trans);

  GST_INFO_OBJECT (self, "Deinit module");

  status = tiovx_dl_color_convert_module_delete (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    return FALSE;
  }

  status = tiovx_dl_color_convert_module_deinit (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dl_color_convert_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node,
    guint * input_param_index, guint * output_param_index)
{
  GstTIOVXDLColorconvert *self = NULL;

  g_return_val_if_fail (trans, FALSE);
  self = GST_TIOVX_DL_COLOR_CONVERT (trans);

  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.node), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.input.image_handle[0]), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.output.image_handle[0]), FALSE);

  GST_INFO_OBJECT (self, "Get node info from module");

  *node = self->obj.node;
  *input = self->obj.input.arr[0];
  *output = self->obj.output.arr[0];
  *input_ref = (vx_reference) self->obj.input.image_handle[0];
  *output_ref = (vx_reference) self->obj.output.image_handle[0];

  *input_param_index = DL_COLOR_CONVERT_INPUT_PARAM_INDEX;
  *output_param_index = DL_COLOR_CONVERT_OUTPUT_PARAM_INDEX;

  return TRUE;
}

static const gchar *
target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_dl_color_convert_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static gboolean
gst_tiovx_dl_color_convert_compare_caps (GstTIOVXSiso * trans, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  GstVideoInfo video_info1;
  GstVideoInfo video_info2;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (caps1, FALSE);
  g_return_val_if_fail (caps2, FALSE);
  g_return_val_if_fail (GST_PAD_UNKNOWN != direction, FALSE);

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

out:
  return ret;
}
