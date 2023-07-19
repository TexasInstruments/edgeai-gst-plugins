/*
 * Copyright (c) [2023] Texas Instruments Incorporated
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

#include "gstticolorconvert.h"

#include <edgeai_dl_color_convert_armv8_utils.h>

#define MAX_PLANE 4

/* Properties definition */
enum
{
  PROP_0,
};

/* Formats definition */
#define TI_COLOR_CONVERT_SUPPORTED_FORMATS_SRC "{RGB, NV12, I420}"
#define TI_COLOR_CONVERT_SUPPORTED_FORMATS_SINK "{RGB, NV12, NV21, I420, UYVY, YUY2, GRAY8}"
#define TI_COLOR_CONVERT_SUPPORTED_WIDTH "[1 , 8192]"
#define TI_COLOR_CONVERT_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TI_COLOR_CONVERT_STATIC_CAPS_SRC                           \
  "video/x-raw, "                                                     \
  "format = (string) " TI_COLOR_CONVERT_SUPPORTED_FORMATS_SRC ", " \
  "width = " TI_COLOR_CONVERT_SUPPORTED_WIDTH ", "                 \
  "height = " TI_COLOR_CONVERT_SUPPORTED_HEIGHT                    \

/* Sink caps */
#define TI_COLOR_CONVERT_STATIC_CAPS_SINK                           \
  "video/x-raw, "                                                      \
  "format = (string) " TI_COLOR_CONVERT_SUPPORTED_FORMATS_SINK ", " \
  "width = " TI_COLOR_CONVERT_SUPPORTED_WIDTH ", "                  \
  "height = " TI_COLOR_CONVERT_SUPPORTED_HEIGHT                     \

/* Pads definitions */
static
    GstStaticPadTemplate
    src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_COLOR_CONVERT_STATIC_CAPS_SRC)
    );

static
    GstStaticPadTemplate
    sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_COLOR_CONVERT_STATIC_CAPS_SINK)
    );

struct _GstTIColorConvert
{
  GstVideoFilter
      element;
  guint
      input_image_width;
  guint
      input_image_height;
  guint
      output_image_width;
  guint
      output_image_height;
  guint
      num_input_plane;
  guint
      num_output_plane;
  gboolean
    parse_in_video_meta;
  gboolean
    parse_out_video_meta;
  GstVideoFormat
      input_format;
  GstVideoFormat
      output_format;
  bufParams2D_t *
      in_buf_param;
  bufParams2D_t *
      out_buf_param;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_color_convert_debug);
#define GST_CAT_DEFAULT gst_ti_color_convert_debug

#define gst_ti_color_convert_parent_class parent_class
G_DEFINE_TYPE (GstTIColorConvert, gst_ti_color_convert, GST_TYPE_VIDEO_FILTER);

static void
gst_ti_color_convert_finalize (GObject * obj);
static 
    GstCaps *
gst_ti_color_convert_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static
    GstFlowReturn
gst_ti_color_convert_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);
static
    gboolean
gst_ti_color_convert_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);

/* Initialize the plugin's class */
static void
gst_ti_color_convert_class_init (GstTIColorConvertClass * klass)
{
  GObjectClass *
      gobject_class = NULL;
  GstBaseTransformClass *
      gstbasetransform_class = NULL;
  GstElementClass *
      gstelement_class = NULL;
  GstVideoFilterClass *
      gstvideofilter_class = NULL;
  
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;
  gstvideofilter_class = (GstVideoFilterClass *) klass;

  gst_element_class_set_details_simple (gstelement_class,
      "TI Color Convert",
      "Filter/Converter/Video",
      "Converts video from one colorspace to another using the ARM Neon Kernels",
      "Abhay Chirania <a-chirania@ti.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_ti_color_convert_transform_caps);
  
  gstvideofilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_ti_color_convert_transform_frame);
  gstvideofilter_class->set_info =
      GST_DEBUG_FUNCPTR (gst_ti_color_convert_set_info);
  
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_color_convert_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_color_convert_debug,
      "ticolorconvert", 0, "TI Color Convert");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_ti_color_convert_init (GstTIColorConvert * self)
{
  GST_LOG_OBJECT (self, "init");
  self->input_image_width = 0;
  self->input_image_height = 0;
  self->output_image_width = 0;
  self->output_image_height = 0;
  self->num_input_plane = 0;
  self->num_output_plane = 0;
  self->parse_in_video_meta = TRUE;
  self->parse_out_video_meta = TRUE;
  self->in_buf_param = (bufParams2D_t *) malloc(MAX_PLANE * sizeof(bufParams2D_t));
  self->out_buf_param = (bufParams2D_t *) malloc(MAX_PLANE * sizeof(bufParams2D_t));
  return;
}

static void
gst_ti_color_convert_finalize (GObject * obj)
{
  GstTIColorConvert *
      self = GST_TI_COLOR_CONVERT (obj);
  GST_LOG_OBJECT (self, "finalize");

  free(self->in_buf_param);
  free(self->out_buf_param);

  G_OBJECT_CLASS (gst_ti_color_convert_parent_class)->finalize (obj);
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
      append_format_to_list (sink_formats, "UYVY");
      append_format_to_list (sink_formats, "YUY2");
      append_format_to_list (sink_formats, "NV12");
      append_format_to_list (sink_formats, "GRAY8");
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
    case GST_VIDEO_FORMAT_UYVY:
      append_format_to_list (src_formats, "NV12");
      append_format_to_list (src_formats, "UYVY");
      break;
    case GST_VIDEO_FORMAT_YUY2:
      append_format_to_list (src_formats, "NV12");
      append_format_to_list (src_formats, "YUY2");
      break;
    case GST_VIDEO_FORMAT_GRAY8:
      append_format_to_list (src_formats, "NV12");
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
gst_ti_color_convert_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIColorConvert *self = GST_TI_COLOR_CONVERT (trans);
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

    gst_structure_set_value (st, "format", &output_formats);
    g_value_unset (&output_formats);

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

static
    gboolean
gst_ti_color_convert_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstTIColorConvert *
    self = GST_TI_COLOR_CONVERT (filter);
  guint i;

  self->input_image_width = GST_VIDEO_INFO_WIDTH (in_info);
  self->input_image_height = GST_VIDEO_INFO_HEIGHT (in_info);
  self->num_input_plane = GST_VIDEO_INFO_N_PLANES (in_info);
  self->input_format = GST_VIDEO_INFO_FORMAT (in_info);
  self->output_image_width = GST_VIDEO_INFO_WIDTH (out_info);
  self->output_image_height = GST_VIDEO_INFO_HEIGHT (out_info);
  self->output_format = GST_VIDEO_INFO_FORMAT (out_info);
  self->num_output_plane = GST_VIDEO_INFO_N_PLANES (out_info);

  if (self->input_image_width  != self->output_image_width &&
      self->input_image_height != self->output_image_height) {
    GST_ERROR_OBJECT (self, "input and output dimension does not match.");
    return FALSE;
  }

  for (i = 0; i < self->num_input_plane; i++) {
    self->in_buf_param[i].dim_x = self->input_image_width;
    self->in_buf_param[i].dim_y = self->input_image_height;
    self->in_buf_param[i].stride_y = GST_VIDEO_INFO_PLANE_STRIDE (in_info,i);
  }
  for (i = 0; i < self->num_output_plane; i++) {
    self->out_buf_param[i].dim_x = self->output_image_width;
    self->out_buf_param[i].dim_y = self->output_image_height;
    self->out_buf_param[i].stride_y = GST_VIDEO_INFO_PLANE_STRIDE (out_info,i);
  }

  // Passthrough
  if (self->input_format == self->output_format)
  {
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (filter), TRUE);
  }

  return TRUE;
}

static
    GstFlowReturn
gst_ti_color_convert_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
    GstTIColorConvert *
      self = GST_TI_COLOR_CONVERT (filter);
    guint i;

    GST_LOG_OBJECT (self, "transform_frame");

    // Parse Stride from meta
    if (self->parse_in_video_meta) {
      GstVideoMeta *in_video_meta;
      in_video_meta = gst_buffer_get_video_meta (in_frame->buffer);
      if (in_video_meta) {
        for (i = 0; i < self->num_input_plane; i++) {
          self->in_buf_param[i].stride_y = in_video_meta->stride[i];
        }
      }
      self->parse_in_video_meta = FALSE;
    }

    if (self->parse_out_video_meta) {
      GstVideoMeta *out_video_meta;
      out_video_meta = gst_buffer_get_video_meta (out_frame->buffer);
      if (out_video_meta) {
        for (i = 0; i < self->num_output_plane; i++) {
          self->out_buf_param[i].stride_y = out_video_meta->stride[i];
        }
      }
      self->parse_out_video_meta = FALSE;
    }

    if (GST_VIDEO_FORMAT_RGB == self->input_format && GST_VIDEO_FORMAT_NV12 == self->output_format ) {
        colorConvert_RGBtoNV12_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                             &self->in_buf_param[0],
                                             GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                             &self->out_buf_param[0],
                                             GST_VIDEO_FRAME_PLANE_DATA (out_frame, 1),
                                             &self->out_buf_param[1]);
    } else if (GST_VIDEO_FORMAT_NV12 == self->input_format && GST_VIDEO_FORMAT_RGB == self->output_format ) {
        colorConvert_NVXXtoRGB_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                             &self->in_buf_param[0],
                                             GST_VIDEO_FRAME_PLANE_DATA (in_frame, 1),
                                             &self->in_buf_param[1],
                                             GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                             &self->out_buf_param[0],
                                             0,
                                             0); // BT_709
    } else if (GST_VIDEO_FORMAT_NV12 == self->input_format && GST_VIDEO_FORMAT_I420 == self->output_format ) {
        colorConvert_NVXXtoIYUV_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                              &self->in_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (in_frame, 1),
                                              &self->in_buf_param[1],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                              &self->out_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 1),
                                              &self->out_buf_param[1],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 2),
                                              &self->out_buf_param[2],
                                              0);
    } else if (GST_VIDEO_FORMAT_NV21 == self->input_format && GST_VIDEO_FORMAT_RGB == self->output_format ) {
        colorConvert_NVXXtoRGB_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                             &self->in_buf_param[0],
                                             GST_VIDEO_FRAME_PLANE_DATA (in_frame, 1),
                                             &self->in_buf_param[1],
                                             GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                             &self->out_buf_param[0],
                                             1,
                                             0); // BT_709
    } else if (GST_VIDEO_FORMAT_NV21 == self->input_format && GST_VIDEO_FORMAT_I420 == self->output_format ) {
        colorConvert_NVXXtoIYUV_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                              &self->in_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (in_frame, 1),
                                              &self->in_buf_param[1],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                              &self->out_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 1),
                                              &self->out_buf_param[1],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 2),
                                              &self->out_buf_param[2],
                                              1);
    } else if (GST_VIDEO_FORMAT_I420 == self->input_format && GST_VIDEO_FORMAT_NV12 == self->output_format ) {
        colorConvert_IYUVtoNV12_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                              &self->in_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (in_frame, 1),
                                              &self->in_buf_param[1],
                                              GST_VIDEO_FRAME_PLANE_DATA (in_frame, 2),
                                              &self->in_buf_param[2],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                              &self->out_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 1),
                                              &self->out_buf_param[1]);
    } else if (GST_VIDEO_FORMAT_YUY2 == self->input_format && GST_VIDEO_FORMAT_NV12 == self->output_format ) {
        colorConvert_YUVXtoNV12_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                              &self->in_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                              &self->out_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 1),
                                              &self->out_buf_param[1],
                                              0);
    } else if (GST_VIDEO_FORMAT_UYVY == self->input_format && GST_VIDEO_FORMAT_NV12 == self->output_format ) {
        colorConvert_YUVXtoNV12_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                              &self->in_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                              &self->out_buf_param[0],
                                              GST_VIDEO_FRAME_PLANE_DATA (out_frame, 1),
                                              &self->out_buf_param[1],
                                              1);
    } else if (GST_VIDEO_FORMAT_GRAY8 == self->input_format && GST_VIDEO_FORMAT_NV12 == self->output_format ) {
        colorConvert_U8toNV12_i8u_o8u_armv8(GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0),
                                             &self->in_buf_param[0],
                                             GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0),
                                             &self->out_buf_param[0],
                                             GST_VIDEO_FRAME_PLANE_DATA (out_frame, 1),
                                             &self->out_buf_param[1]);
    } else {
        GST_ERROR_OBJECT (self, "invalid input and output conversion formats.");
        return GST_FLOW_ERROR;
    }
    return GST_FLOW_OK;
}
