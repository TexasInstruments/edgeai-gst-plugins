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

#include "gsttimosaic.h"
#include <edgeai_dl_scaler_armv8_utils.h>
#include <edgeai_arm_neon_utils.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_MOSAIC_INPUTS 8
#define MOSAIC_MAX_WIDTH 8192
#define MOSAIC_MAX_HEIGHT 8192
#define MOSAIC_DEFAULT_WINDOW_WIDTH 1280
#define MOSAIC_DEFAULT_WINDOW_HEIGHT 720
#define MAX_UNIQUE_BUFFERS 32
#define GST_BUFFER_OFFSET_FIXED_VALUE -1
#define GST_BUFFER_OFFSET_END_FIXED_VALUE -1
#define MIN_POOL_SIZE 2
#define MAX_POOL_SIZE 16
#define DEFAULT_POOL_SIZE 2
#define MEMORY_ALIGNMENT 128


/* Formats definition */
#define TI_MOSAIC_SUPPORTED_FORMATS_SRC "{NV12}"
#define TI_MOSAIC_SUPPORTED_FORMATS_SINK "{NV12}"
#define TI_MOSAIC_SUPPORTED_WIDTH "[1 , 8192]"
#define TI_MOSAIC_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TI_MOSAIC_STATIC_CAPS_SRC                           \
  "video/x-raw, "                                           \
  "format = (string) " TI_MOSAIC_SUPPORTED_FORMATS_SRC ", " \
  "width = " TI_MOSAIC_SUPPORTED_WIDTH ", "                 \
  "height = " TI_MOSAIC_SUPPORTED_HEIGHT ", "               \
  "framerate = " GST_VIDEO_FPS_RANGE                        \

/* Sink caps */
#define TI_MOSAIC_STATIC_CAPS_SINK                           \
  "video/x-raw, "                                            \
  "format = (string) " TI_MOSAIC_SUPPORTED_FORMATS_SINK ", " \
  "width = " TI_MOSAIC_SUPPORTED_WIDTH ", "                  \
  "height = " TI_MOSAIC_SUPPORTED_HEIGHT ", "                \
  "framerate = " GST_VIDEO_FPS_RANGE                         \

enum
{
  PROP_STARTX = 1,
  PROP_STARTY,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_POOL_SIZE,
};

struct _GstTIMosaicPad
{
  GstAggregatorPad base;
  gint startx;
  gint starty;
  gint width;
  gint height;
  gint pool_size;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_mosaic_pad_debug);

G_DEFINE_TYPE_WITH_CODE (GstTIMosaicPad, gst_ti_mosaic_pad,
    GST_TYPE_AGGREGATOR_PAD,
    GST_DEBUG_CATEGORY_INIT (gst_ti_mosaic_pad_debug,
        "timosaicpad", 0, "debug category for TI mosaic pad class"));

static void gst_ti_mosaic_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ti_mosaic_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_ti_mosaic_pad_class_init (GstTIMosaicPadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_ti_mosaic_pad_set_property;
  gobject_class->get_property = gst_ti_mosaic_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_STARTX,
      g_param_spec_uint ("startx", "Window start X in output",
          "Starting X coordinate of the window in output for given sink pad",
          0, MOSAIC_MAX_WIDTH - 2, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_STARTY,
      g_param_spec_uint ("starty", "Window start Y in output",
          "Starting Y coordinate of the window in output for given sink pad",
          0, MOSAIC_MAX_HEIGHT - 2, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_uint ("width", "Window width in output",
          "Width of the window in output for given sink pad",
          2, MOSAIC_MAX_WIDTH, MOSAIC_DEFAULT_WINDOW_WIDTH,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_uint ("height", "Window height in output",
          "Height of the window in output for given sink pad",
          2, MOSAIC_MAX_HEIGHT, MOSAIC_DEFAULT_WINDOW_HEIGHT,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_POOL_SIZE,
      g_param_spec_uint ("pool-size", "Pool Size",
          "Number of buffers in the buffer pool",
          MIN_POOL_SIZE, MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
          G_PARAM_READWRITE));
}

static void
gst_ti_mosaic_pad_init (GstTIMosaicPad * self)
{
  GST_DEBUG_OBJECT (self, "gst_ti_mosaic_pad_init");

  self->startx = 0;
  self->starty = 0;
  self->width = 0;
  self->height = 0;
  self->pool_size = DEFAULT_POOL_SIZE;
}

static void
gst_ti_mosaic_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIMosaicPad *self = GST_TI_MOSAIC_PAD (object);

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
    case PROP_POOL_SIZE:
      self->pool_size = g_value_get_uint (value);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_ti_mosaic_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIMosaicPad *self = GST_TI_MOSAIC_PAD (object);

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
      g_value_set_uint (value, self->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, self->height);
      break;
    case PROP_POOL_SIZE:
      g_value_set_uint (value, self->pool_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}
/* Pads definitions */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_MOSAIC_STATIC_CAPS_SRC)
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TI_MOSAIC_STATIC_CAPS_SINK)
    );

/* Properties definition */
enum
{
  PROP_0,
  PROP_BACKGROUND,
};

struct _GstTIMosaic
{
  GstAggregator element;
  bufParams2D_t in_y_buf_param[MAX_MOSAIC_INPUTS];
  bufParams2D_t in_uv_buf_param[MAX_MOSAIC_INPUTS];
  bufParams2D_t out_y_buf_param[MAX_MOSAIC_INPUTS];
  bufParams2D_t out_uv_buf_param[MAX_MOSAIC_INPUTS];
  gint startx[MAX_MOSAIC_INPUTS];
  gint starty[MAX_MOSAIC_INPUTS];
  gint out_width;
  gint out_height;
  gint out_buffer_offsets_y[MAX_MOSAIC_INPUTS];
  gint out_buffer_offsets_uv[MAX_MOSAIC_INPUTS];
  gint in_buffer_uv_plane_offset[MAX_MOSAIC_INPUTS];
  gint out_buffer_uv_plane_offset;
  gint out_buffer_uv_plane_size;
  GstBuffer *unique_buffers[MAX_UNIQUE_BUFFERS];
  gint unique_buffers_last_index;
  gchar *background;
  gboolean parsed_video_meta;
};

static void gst_ti_mosaic_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

GST_DEBUG_CATEGORY_STATIC (gst_ti_mosaic_debug);
#define GST_CAT_DEFAULT gst_ti_mosaic_debug

#define GST_TI_MOSAIC_DEFINE_CUSTOM_CODE \
  G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY, gst_ti_mosaic_child_proxy_init);

G_DEFINE_TYPE_WITH_CODE (GstTIMosaic, gst_ti_mosaic, GST_TYPE_AGGREGATOR,
  GST_TI_MOSAIC_DEFINE_CUSTOM_CODE);

static void gst_ti_mosaic_finalize (GObject * obj);
static void gst_ti_mosaic_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ti_mosaic_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstPad * gst_ti_mosaic_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps);
static void gst_ti_mosaic_release_pad (GstElement * element, GstPad * pad);
GstCaps *gst_ti_mosaic_fixate_src_caps (GstAggregator * self, GstCaps * caps);
static gboolean
gst_ti_mosaic_negotiated_src_caps (GstAggregator * self, GstCaps * caps);
static gboolean
gst_ti_mosaic_decide_allocation (GstAggregator * agg, GstQuery * query);
static GstFlowReturn gst_ti_mosaic_aggregate (GstAggregator * aggregator,
    gboolean timeout);

/* Initialize the plugin's class */
static void
gst_ti_mosaic_class_init (GstTIMosaicClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAggregatorClass *aggregator_class = GST_AGGREGATOR_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TI Mosaic",
      "Aggregator/Videomixer",
      "Stitch the input video streams to produce a single Mosaic output",
      "Rahul T R <r-ravikumar@ti.com>");

  gobject_class->set_property = gst_ti_mosaic_set_property;
  gobject_class->get_property = gst_ti_mosaic_get_property;

  g_object_class_install_property (gobject_class, PROP_BACKGROUND,
      g_param_spec_string ("background", "Background",
          "Background image of the Mosaic to be used by this element",
          "", G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &src_template, GST_TYPE_TI_MOSAIC_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sink_template, GST_TYPE_TI_MOSAIC_PAD);

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_ti_mosaic_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_ti_mosaic_release_pad);


  aggregator_class->fixate_src_caps =
      GST_DEBUG_FUNCPTR (gst_ti_mosaic_fixate_src_caps);
  aggregator_class->negotiated_src_caps =
      GST_DEBUG_FUNCPTR (gst_ti_mosaic_negotiated_src_caps);
  aggregator_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_ti_mosaic_decide_allocation);
  aggregator_class->aggregate = GST_DEBUG_FUNCPTR (gst_ti_mosaic_aggregate);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_mosaic_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_mosaic_debug,
      "timosaic", 0, "TI Mosaic");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_ti_mosaic_init (GstTIMosaic * self)
{
  GST_LOG_OBJECT (self, "init");
  self->parsed_video_meta = FALSE;
  self->background = g_strdup("");

  for (gint i = 0; i < MAX_UNIQUE_BUFFERS; ++i){
    self->unique_buffers[i] = NULL;
  }

  self->unique_buffers_last_index = 0;

  return;
}

static void
gst_ti_mosaic_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIMosaic *self = GST_TI_MOSAIC (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
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
gst_ti_mosaic_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIMosaic *self = GST_TI_MOSAIC (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
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
gst_ti_mosaic_finalize (GObject * obj)
{
  GstTIMosaic *
      self = GST_TI_MOSAIC (obj);
  GST_LOG_OBJECT (self, "finalize");
  G_OBJECT_CLASS (gst_ti_mosaic_parent_class)->finalize (obj);
}

static GstCaps *
intersect_with_template_caps (GstCaps * caps, GstPad * pad)
{
  GstCaps *template_caps = NULL;
  GstCaps *filtered_caps = NULL;

  g_return_val_if_fail (pad, NULL);

  if (caps) {
    template_caps = gst_pad_get_pad_template_caps (pad);

    filtered_caps = gst_caps_intersect (caps, template_caps);
    gst_caps_unref (template_caps);
  }

  return filtered_caps;
}

static GList *
gst_ti_mosaic_get_sink_caps_list (GstTIMosaic * self)
{
  GstAggregator *agg = NULL;
  GList *sink_caps_list = NULL;
  GList *l = NULL;

  g_return_val_if_fail (self, sink_caps_list);

  agg = GST_AGGREGATOR (self);

  GST_DEBUG_OBJECT (self, "Generating sink caps list");

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstPad *sink_pad = GST_PAD (l->data);
    GstCaps *pad_caps = NULL;
    GstCaps *peer_caps = NULL;

    pad_caps = gst_pad_get_current_caps (sink_pad);
    if (NULL == pad_caps) {
      peer_caps = gst_pad_peer_query_caps (sink_pad, NULL);
      pad_caps = intersect_with_template_caps (peer_caps, sink_pad);
      gst_caps_unref (peer_caps);
    }

    GST_DEBUG_OBJECT (self, "Caps from %s:%s peer: %" GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (sink_pad), pad_caps);
    /* Insert at the end of the src caps list */
    sink_caps_list = g_list_insert (sink_caps_list, pad_caps, -1);
  }

  return sink_caps_list;
}

static gboolean
gst_ti_mosaic_validate_candidate_dimension (GstTIMosaic * self,
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
gst_ti_mosaic_fixate_structure_fields (GstStructure * structure, gint width,
    gint height, gint fps_n, gint fps_d, const gchar * format)
{
  gst_structure_fixate_field_nearest_int (structure, "width", width);
  gst_structure_fixate_field_nearest_int (structure, "height", height);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", fps_n,
      fps_d);
  gst_structure_fixate_field_string (structure, "format", format);
}

static GstCaps *
gst_ti_mosaic_fixate_caps (GstTIMosaic * self, GList * sink_caps_list,
        GstCaps * src_caps)
{
  GstCaps *output_caps = NULL, *candidate_output_caps = NULL;
  GList *l = NULL;
  gint best_width = -1, best_height = -1;
  gint best_fps_n = -1, best_fps_d = -1;
  const gchar *format_str = NULL;
  gdouble best_fps = 0.;
  GstStructure *candidate_output_structure = NULL;
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
    GstTIMosaicPad *mosaic_pad = NULL;
    GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
    GstVideoInfo video_info = {
    };
    GstCaps *caps = NULL;
    gint width = 0, height = 0;
    gint fps_n = 0, fps_d = 0;
    gdouble cur_fps = 0;

    mosaic_pad = GST_TI_MOSAIC_PAD (sink_pad);
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

    if (format == GST_VIDEO_FORMAT_UNKNOWN) {
      format = video_info.finfo->format;
      format_str = video_info.finfo->name;
    } else if (format != video_info.finfo->format) {
      GST_ERROR_OBJECT (self, "All sink format should have same format");
      goto out;
    }

    fps_n = GST_VIDEO_INFO_FPS_N (&video_info);
    fps_d = GST_VIDEO_INFO_FPS_D (&video_info);

    if (mosaic_pad->width == 0)
        width = GST_VIDEO_INFO_WIDTH (&video_info);
    else
        width = mosaic_pad->width;

    if (mosaic_pad->height == 0)
        height = GST_VIDEO_INFO_HEIGHT (&video_info);
    else
        height = mosaic_pad->height;

    width += MAX (mosaic_pad->startx, 0);
    height += MAX (mosaic_pad->starty, 0);

    if (best_width < width) {
      best_width = width;
    }
    if (best_height < height) {
      best_height = height;
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

  for (i = 0; i < gst_caps_get_size (candidate_output_caps); i++) {
    candidate_output_structure =
        gst_caps_get_structure (candidate_output_caps, i);

    if (!gst_ti_mosaic_validate_candidate_dimension (self,
            candidate_output_structure, "width", best_width)
        || !gst_ti_mosaic_validate_candidate_dimension (self,
            candidate_output_structure, "height", best_height)) {
      GST_ERROR_OBJECT (self,
          "Minimum required width and height for windows: (%d, %d) is"
          " larger than current source caps: %" GST_PTR_FORMAT, best_width,
          best_height, candidate_output_structure);
      goto out;
    }

    gst_ti_mosaic_fixate_structure_fields (candidate_output_structure,
        best_width, best_height, best_fps_n, best_fps_d, format_str);

    ret_val = g_strcmp0 (format_str,
        gst_structure_get_string (candidate_output_structure, "format"));
    if (ret_val) {
      GST_ERROR_OBJECT (self, "Could not fixate output format");
      goto out;
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

GstCaps *
gst_ti_mosaic_fixate_src_caps (GstAggregator * agg, GstCaps * src_caps)
{
  GstTIMosaic *self = GST_TI_MOSAIC (agg);
  GList *sink_caps_list = NULL;
  GstCaps *fixated_caps = NULL;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (src_caps, NULL);

  sink_caps_list = gst_ti_mosaic_get_sink_caps_list (self);

  /* Should return the fixated caps the element will use on the src pads */
  fixated_caps = gst_ti_mosaic_fixate_caps (self, sink_caps_list, src_caps);
  if (NULL == fixated_caps) {
    GST_ERROR_OBJECT (self, "Subclass did not fixate caps");
    goto exit;
  }

exit:
  return fixated_caps;
}

gboolean
gst_ti_mosaic_negotiated_src_caps (GstAggregator * agg, GstCaps * caps)
{
  GstTIMosaic *self = GST_TI_MOSAIC (agg);
  gboolean ret = FALSE;
  GList *l = NULL, *p = NULL, *sink_caps_list = NULL;
  GstVideoInfo in_info = {
  };
  GstVideoInfo out_info = {
  };
  gint i=0;

  GST_DEBUG_OBJECT (self, "Negotiated src caps");

  /* We are calling this manually to ensure that during module initialization
   * the src pad has all the information. Normally this would be called by
   * GstAggregator right after the negotiated_src_caps
   */
  if (!gst_caps_is_fixed (caps)) {
    GST_ERROR_OBJECT (self,
        "Unable to set Aggregator source caps. Negotiated source caps aren't fixed");
    goto exit;
  }
  gst_aggregator_set_src_caps (agg, caps);

  ret = gst_video_info_from_caps (&out_info, caps);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
        GST_PTR_FORMAT, caps);
    goto exit;
  }

  self->out_width = GST_VIDEO_INFO_WIDTH (&out_info);
  self->out_height = GST_VIDEO_INFO_HEIGHT (&out_info);
  self->out_buffer_uv_plane_offset = self->out_width * self->out_height;
  self->out_buffer_uv_plane_size = self->out_width * self->out_height / 2;

  sink_caps_list = gst_ti_mosaic_get_sink_caps_list (self);

  for (l = sink_caps_list, p = GST_ELEMENT (agg)->sinkpads, i=0;
          l && p; l = g_list_next (l), p = g_list_next (p), i++) {
    GstCaps *sink_caps = l->data;
    GstTIMosaicPad *pad = GST_TI_MOSAIC_PAD(p->data);

    ret = gst_video_info_from_caps (&in_info, sink_caps);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT, sink_caps);
      goto exit;
    }

    self->in_y_buf_param[i].dim_x = GST_VIDEO_INFO_WIDTH (&in_info);
    self->in_y_buf_param[i].dim_y = GST_VIDEO_INFO_HEIGHT (&in_info);
    self->in_y_buf_param[i].stride_y = GST_VIDEO_INFO_PLANE_STRIDE (&in_info,0);

    self->in_uv_buf_param[i].dim_x = GST_VIDEO_INFO_WIDTH (&in_info);
    self->in_uv_buf_param[i].dim_y = GST_VIDEO_INFO_HEIGHT (&in_info) / 2;
    self->in_uv_buf_param[i].stride_y = GST_VIDEO_INFO_PLANE_STRIDE (
            &in_info,1);

    self->startx[i] = (pad->startx >> 1) << 1;
    self->starty[i] = (pad->starty >> 1) << 1;

    if (!pad->width) {
      self->out_y_buf_param[i].dim_x = GST_VIDEO_INFO_WIDTH (&in_info);
      self->out_uv_buf_param[i].dim_x = GST_VIDEO_INFO_WIDTH (&in_info);
    } else {
      self->out_y_buf_param[i].dim_x = (pad->width >> 1) << 1;
      self->out_uv_buf_param[i].dim_x = (pad->width >> 1) << 1;
    }

    if (!pad->height) {
      self->out_y_buf_param[i].dim_y = GST_VIDEO_INFO_HEIGHT (&in_info);
      self->out_uv_buf_param[i].dim_y = GST_VIDEO_INFO_HEIGHT (&in_info) / 2;
    } else {
      self->out_y_buf_param[i].dim_y = (pad->height >> 1) << 1;
      self->out_uv_buf_param[i].dim_y = pad->height / 2;
    }

    self->out_y_buf_param[i].stride_y = GST_VIDEO_INFO_PLANE_STRIDE (&out_info,0);
    self->out_uv_buf_param[i].stride_y = GST_VIDEO_INFO_PLANE_STRIDE (&out_info,1);
  }
  /*
   * We'll call reconfiguration on all upstream pads. This will force a propose
   * allocation which should now be using the subclass correct reference.
   */
  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstPad *pad = l->data;
    GstEvent *event = NULL;

    event = gst_event_new_reconfigure ();
    gst_pad_push_event (pad, event);
  }

  ret = TRUE;

exit:
  return ret;
}

static GstFlowReturn
gst_ti_mosaic_create_output_buffer (GstTIMosaic * self, GstBuffer ** outbuf)
{
  GstAggregator *aggregator = NULL;
  GstBufferPool *pool;
  GstFlowReturn ret = GST_FLOW_ERROR;

  g_return_val_if_fail (self, ret);

  aggregator = GST_AGGREGATOR (self);

  pool = gst_aggregator_get_buffer_pool (aggregator);

  if (pool) {
    if (!gst_buffer_pool_is_active (pool)) {
      if (!gst_buffer_pool_set_active (pool, TRUE)) {
        GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
        goto exit;
      }
    }

    ret = gst_buffer_pool_acquire_buffer (pool, outbuf, NULL);
    gst_object_unref (pool);
    pool = NULL;
    ret = GST_FLOW_OK;
  } else {
    GST_ERROR_OBJECT (self,
        "An output buffer can only be created from a buffer pool");
  }

exit:
  return ret;
}

static GstFlowReturn
gst_ti_mosaic_aggregate (GstAggregator * agg, gboolean timeout)
{
  GstTIMosaic *self = GST_TI_MOSAIC (agg);
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GList *l = NULL;
  GstClockTime pts = GST_CLOCK_TIME_NONE;
  GstClockTime dts = GST_CLOCK_TIME_NONE;
  GstClockTime duration = 0;
  GstVideoMeta *out_video_meta;
  GstMapInfo out_buffer_mapinfo;
  gint i=0;
  gboolean unique_buffer = TRUE;

  GST_LOG_OBJECT (self, "TI Mosaic aggregate");

  ret = gst_ti_mosaic_create_output_buffer (self, &outbuf);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to acquire output buffer");
    goto exit;
  }

  if (!self->parsed_video_meta) {
    out_video_meta = gst_buffer_get_video_meta (outbuf);
    if (out_video_meta) {
      self->out_buffer_uv_plane_offset = out_video_meta->stride[0]
          * self->out_height;
      self->out_buffer_uv_plane_size = out_video_meta->stride[0]
          * self->out_height / 2;
    }
  }

  /* Map output Buffer */
  if (!gst_buffer_map (outbuf, &out_buffer_mapinfo, GST_MAP_READWRITE)) {
      GST_ERROR_OBJECT (self, "failed to map output buffer");
      goto unref_output;
  }

  /* Check if this output buffer is alread encountered previously */
  for (i = 0; i <= self->unique_buffers_last_index; i++){
    if (outbuf == self->unique_buffers[i]) {
      unique_buffer = FALSE;
      break;
    }
  }

  if (unique_buffer) {
    self->unique_buffers[self->unique_buffers_last_index] = outbuf;
    self->unique_buffers_last_index++;
    if (self->unique_buffers_last_index == MAX_UNIQUE_BUFFERS) {
      self->unique_buffers_last_index = MAX_UNIQUE_BUFFERS-1;
    }
    if (0 != g_strcmp0 ("", self->background)) {
      if (F_OK != access (self->background, F_OK)) {
        GST_ERROR_OBJECT (self, "Invalid background property file path: %s",
            self->background);
        goto unref_output;
      } else {
        FILE *background_img_file = fopen (self->background, "rb");
        if (!background_img_file) {
          GST_ERROR_OBJECT (self, "Unable to open the background image file: %s",
              self->background);
          goto unref_output;
        }
        fread (out_buffer_mapinfo.data, 1, self->out_buffer_uv_plane_size * 3,
                background_img_file);
        fclose(background_img_file);
      }
    } else {
      memset_neon(out_buffer_mapinfo.data + self->out_buffer_uv_plane_offset, 128,
              self->out_buffer_uv_plane_size);
    }
  }

  /* Ensure valid references in the inputs */
  for (l = GST_ELEMENT (agg)->sinkpads, i=0; l; l = g_list_next (l), i++) {
    GstAggregatorPad *pad = l->data;
    GstBuffer *in_buffer = NULL;
    GstClockTime tmp_pts = 0;
    GstClockTime tmp_dts = 0;
    GstClockTime tmp_duration = 0;
    GstMapInfo in_buffer_mapinfo;

    in_buffer = gst_aggregator_pad_peek_buffer (pad);
    if (in_buffer) {
      tmp_pts = GST_BUFFER_PTS (in_buffer);
      tmp_dts = GST_BUFFER_DTS (in_buffer);
      tmp_duration = GST_BUFFER_DURATION (in_buffer);

      /* Find the smallest timestamp and the largest duration */
      if (tmp_pts < pts) {
        pts = tmp_pts;
      }
      if (tmp_dts < dts) {
        dts = tmp_dts;
      }
      if (tmp_duration > duration) {
        duration = tmp_duration;
      }

      /* Parse video meta to get stride information */
      if (!self->parsed_video_meta) {
        GstVideoMeta *video_meta;
        video_meta = gst_buffer_get_video_meta (in_buffer);
        if (video_meta) {
          self->in_y_buf_param[i].stride_y = video_meta->stride[0];
          self->in_uv_buf_param[i].stride_y = video_meta->stride[1];
        }
        if (out_video_meta) {
          self->out_y_buf_param[i].stride_y = out_video_meta->stride[0];
          self->out_uv_buf_param[i].stride_y = out_video_meta->stride[1];
        }

        self->out_buffer_offsets_y[i] = self->startx[i] +
            self->starty[i] * self->out_y_buf_param[i].stride_y;
        self->out_buffer_offsets_uv[i] = self->startx[i] +
            self->starty[i]/2 * self->out_uv_buf_param[i].stride_y;

        self->in_buffer_uv_plane_offset[i] = self->in_y_buf_param[i].stride_y
            * self->in_y_buf_param[i].dim_y;
      }

      /* Map Input Buffer */
      if (!gst_buffer_map (in_buffer, &in_buffer_mapinfo, GST_MAP_READ)) {
          GST_ERROR_OBJECT (self, "failed to map input buffer");
          goto unref_output;
      }

      /* processing */
      scaleNNNV12 (in_buffer_mapinfo.data,
              &self->in_y_buf_param[i],
              in_buffer_mapinfo.data + self->in_buffer_uv_plane_offset[i],
              &self->in_uv_buf_param[i],
              out_buffer_mapinfo.data + self->out_buffer_offsets_y[i],
              &self->out_y_buf_param[i],
              out_buffer_mapinfo.data + self->out_buffer_uv_plane_offset +
              self->out_buffer_offsets_uv[i],
              &self->out_y_buf_param[i]);

      gst_buffer_unmap (in_buffer, &in_buffer_mapinfo);
      gst_buffer_unref (in_buffer);

    } else {
      GST_LOG_OBJECT (pad, "pad: %" GST_PTR_FORMAT " has no buffers", pad);
    }

  }

  gst_buffer_unmap (outbuf, &out_buffer_mapinfo);
  self->parsed_video_meta = TRUE;

  /* Assign the smallest timestamp and the largest duration */
  GST_BUFFER_PTS (outbuf) = pts;
  GST_BUFFER_DTS (outbuf) = dts;
  GST_BUFFER_DURATION (outbuf) = duration;
  /* The offset and offset end is used to indicate a "buffer number", should be
   * monotically increasing. For now we are not messing with this and it is
   * assigned to -1 */
  GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET_FIXED_VALUE;
  GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_END_FIXED_VALUE;

  if (GST_BUFFER_PTS_IS_VALID (outbuf)) {
    GST_AGGREGATOR_PAD (agg->srcpad)->segment.position =
        GST_BUFFER_PTS (outbuf);
  }

  gst_aggregator_finish_buffer (agg, outbuf);

  /* Mark all input buffers as read  */
  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *pad = l->data;
    GstBuffer *in_buffer = NULL;

    in_buffer = gst_aggregator_pad_pop_buffer (pad);
    if (in_buffer) {
      gst_buffer_unref (in_buffer);
    }
  }

exit:
  return ret;

unref_output:
  if (outbuf) {
    gst_buffer_unref (outbuf);
  }
  return ret;
}

static GstPad *
gst_ti_mosaic_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstPad *newpad;
  newpad = (GstPad *)
      GST_ELEMENT_CLASS (gst_ti_mosaic_parent_class)->request_new_pad (element,
      templ, req_name, caps);
  if (newpad == NULL)
    goto could_not_create;
  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));
  return newpad;
could_not_create:
  {
    GST_DEBUG_OBJECT (element, "could not create/add pad");
    return NULL;
  }
}

static void
gst_ti_mosaic_release_pad (GstElement * element, GstPad * pad)
{
  GstTIMosaic *self = GST_TI_MOSAIC (element);
  GST_DEBUG_OBJECT (self, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));
  gst_child_proxy_child_removed (GST_CHILD_PROXY (self), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));
  GST_ELEMENT_CLASS (gst_ti_mosaic_parent_class)->release_pad (element, pad);
}

static gboolean
gst_ti_mosaic_decide_allocation (GstAggregator * agg, GstQuery * query)
{
  GstTIMosaic *self = GST_TI_MOSAIC (agg);
  gboolean ret = TRUE;
  gint npool = 0;
  gboolean pool_needed = TRUE;

  GST_LOG_OBJECT (self, "Decide allocation");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *pool = NULL;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (NULL == pool) {
      GST_DEBUG_OBJECT (self, "No pool in query position: %d, ignoring", npool);
      gst_query_remove_nth_allocation_pool (query, npool);
      continue;
    }

    /* Use pool if found */
    if (pool) {
      pool_needed = FALSE;
    }

    gst_object_unref (pool);
    pool = NULL;
  }

  if (pool_needed) {
    GstBufferPool * pool = NULL;
    GstStructure *config;
    GstCaps *caps;
    GstAllocationParams alloc_params;

    gst_query_parse_allocation (query, &caps, NULL);
    pool = gst_buffer_pool_new ();
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config,
                                     caps,
                                     self->out_width * self->out_height * 3/2,
                                     GST_TI_MOSAIC_PAD(agg->srcpad)->pool_size,
                                     GST_TI_MOSAIC_PAD(agg->srcpad)->pool_size);

    gst_allocation_params_init(&alloc_params);
    alloc_params.align = MEMORY_ALIGNMENT - 1;

    gst_buffer_pool_config_set_allocator (config,
                                          NULL,
                                          &alloc_params);
    gst_buffer_pool_set_config(pool, config);
    gst_query_add_allocation_pool (query, pool,
                                   self->out_width * self->out_height * 3/2,
                                   GST_TI_MOSAIC_PAD(agg->srcpad)->pool_size,
                                   GST_TI_MOSAIC_PAD(agg->srcpad)->pool_size);

    ret = gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), TRUE);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
      goto exit;
    }
    gst_object_unref (pool);
    pool = NULL;
  }

exit:
  return ret;
}

/* GstChildProxy implementation */
static GObject *
gst_ti_mosaic_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  return NULL;
}

static GObject *
gst_ti_mosaic_child_proxy_get_child_by_name (GstChildProxy *
    child_proxy, const gchar * name)
{
  GstTIMosaic *self = NULL;
  GObject *obj = NULL;

  self = GST_TI_MOSAIC (child_proxy);

  GST_OBJECT_LOCK (self);

  if (0 == strcmp (name, "src")) {
    /* Only one src pad for MISO class */
    obj = G_OBJECT (GST_AGGREGATOR (self)->srcpad);
    if (obj) {
      gst_object_ref (obj);
    }
  } else {                      /* sink pad case */
    GList *node = NULL;
    node = GST_ELEMENT_CAST (self)->sinkpads;
    for (; node; node = g_list_next (node)) {
      if (0 == strcmp (name, GST_OBJECT_NAME (node->data))) {
        obj = G_OBJECT (node->data);
        gst_object_ref (obj);
        break;
      }
    }
  }

  GST_OBJECT_UNLOCK (self);

  return obj;
}

static guint
gst_ti_mosaic_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  GstTIMosaic *self = NULL;
  guint count = 0;

  self = GST_TI_MOSAIC (child_proxy);

  GST_OBJECT_LOCK (self);
  /* Number of source pads (always 1) + number of sink pads */
  count = GST_ELEMENT_CAST (self)->numsinkpads + 1;
  GST_OBJECT_UNLOCK (self);
  GST_INFO_OBJECT (self, "Children Count: %d", count);

  return count;
}

static void
gst_ti_mosaic_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  iface->get_child_by_index = gst_ti_mosaic_child_proxy_get_child_by_index;
  iface->get_child_by_name = gst_ti_mosaic_child_proxy_get_child_by_name;
  iface->get_children_count = gst_ti_mosaic_child_proxy_get_children_count;
}
