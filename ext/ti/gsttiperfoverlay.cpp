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

#define __STDC_FORMAT_MACROS 1
#include <glib.h>

extern "C"
{

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttiperfoverlay.h"
#include <edgeai_perf_stats_utils.h>
#include <gst/video/video.h>
#include <edgeai_nv12_drawing_utils.h>
#include <edgeai_nv12_font_utils.h>

#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)
#include <TI/tivx.h>
#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "gst-libs/gst/tiovx/gsttiovxcontext.h"
#include "utils/app_init/include/app_init.h"
#include "utils/perf_stats/include/app_perf_stats.h"
#include "utils/perf_stats/src/app_perf_stats_priv.h"
#include "utils/ipc/include/app_ipc.h"
#include "utils/remote_service/include/app_remote_service.h"
#endif

}

/* Overlay Type definition */
#define GST_TYPE_TI_PERF_OVERLAY_TYPE (gst_ti_perf_overlay_type_get_type())
static GType
gst_ti_perf_overlay_type_get_type (void)
{
  static GType overlay_type = 0;

  static const GEnumValue types[] = {
    {0, "Bar Graph Overlay", "graph"},
    {1, "Text Overlay", "text"},
    {0, NULL, NULL},
  };

  if (!overlay_type) {
    overlay_type =
        g_enum_register_static ("GstTIPerfOverlayType", types);
  }
  return overlay_type;
}

/* Properties definition */
enum
{
  PROP_0,
  PROP_OVERLAY_STATS,
  PROP_OVERLAY_TYPE,
  PROP_DUMP_STATS,
  PROP_UPDATE_STATS_INTERVAL,
  PROP_UPDATE_FPS_INTERVAL,
  PROP_MAIN_TITLE,
  PROP_TITLE,
  PROP_LOCATION,
  PROP_START_DUMPS,
  PROP_NUM_DUMPS
};

#define DEFAULT_UPDATE_STATS_INTERVAL 1000 // Update fps after ? millisecond
#define DEFAULT_UPDATE_FPS_INTERVAL 2000 // Update fps after ? millisecond
#define DEFAULT_START_DUMPS 300
#define DEFAULT_NUM_DUMPS 1
#define DEFAULT_MAIN_TITLE "Texas Instruments Edge AI"

#define TEXT_COLOR 122,13,255

#define OVERLAY_COLOR 0,0,0
#define OVERLAY_TEXT_COLOR 255,255,255
#define GRAPH_BG_COLOR 45,45,45

#define MAX_OVERLAY_HEIGHT 250  // Max allowed overlay height
#define MIN_OVERLAY_HEIGHT 50   // Min allowed overlay height
#define TOTAL_OVERLAY_HEIGHT_PCT 0.2    // Percentage of total height to make as overlay

#define DEFAULT_TI_PERF_OVERLAY_TYPE 0x00 //Bar graph

/* Pads definitions */
static
    GstStaticPadTemplate
    src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );

static
    GstStaticPadTemplate
    sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
    );

struct _GstTIPerfOverlay
{
  GstBaseTransform
      element;
  gboolean
      caps_negotiated;
  gboolean
      overlay;
  gint
      overlay_type;
  gboolean
      dump_stats;
  guint
      image_width;
  guint
      image_height;
  guint
      uv_offset;
  Image *
      image_handler;
  YUVColor *
      text_color;
  FontProperty *
      very_small_font_property;
  FontProperty *
      small_font_property;
  FontProperty *
      big_font_property;
  YUVColor *
      overlay_color;
  YUVColor *
      overlay_text_color;
  YUVColor *
      graph_bg_color;
  YUVColor *
      color_red;
  YUVColor *
      color_yellow;
  YUVColor *
      color_green;
  YUVColor *
      color_blue;
  YUVColor *
      color_purple;
  YUVColor *
      color_orange;
  YUVColor *
      color_pink;
  YUVColor *
      color_white;
  YUVColor *
      color_black;
  YUVColor *
      soc_temp_text_color;
  BarGraph *
      bar_graphs;
  guint
      overlay_pos_y;
  guint
      overlay_pos_x;
  guint
      overlay_width;
  guint
      overlay_height;
  guint
      graph_pos_y;
  guint
      graph_pos_x;
  guint
      graph_offset_x;
  guint
      graph_width;
  guint
      graph_height;
  guint
      num_graphs;
  guint
      update_stats_interval;
  gint64
      update_stats_interval_m;
  guint
      update_fps_interval;
  gint64
      update_fps_interval_m;
  guint
      frames_rendered;
  guint8
      fps;
  guint64
      frame_count;
  GstClockTime
      previous_fps_timestamp;
  GstClockTime
      previous_stats_timestamp;
  guint
      fps_x_pos;
  guint
      fps_y_pos;
  guint
      fps_width;
  guint
      fps_height;
  gchar *
      main_title;
  gchar *
      title;
  guint
      start_dumps;
  guint
      num_dumps;
  guint
      dump_count;
  FILE *
      dump_fd;
  FontProperty *
      main_title_font_property;
  FontProperty *
      title_font_property;
  gchar *
      location;
  perfStatsCpuLoad *
      cpu_load;
  perf_stats_ddr_stats_t
      *ddr_load;
  perfStatsSOCTemp *
      soc_temp;
  guint
      soc_temp_cpu_idx;
#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)
  app_perf_stats_cpu_load_t
      c7x_loads[APP_IPC_CPU_MAX];
  app_perf_stats_hwa_stats_t
      hwa_loads[4];
  GstTIOVXContext *
      tiovx_context;
#endif
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_perf_overlay_debug);
#define GST_CAT_DEFAULT gst_ti_perf_overlay_debug

#define gst_ti_perf_overlay_parent_class parent_class
G_DEFINE_TYPE (GstTIPerfOverlay, gst_ti_perf_overlay, GST_TYPE_BASE_TRANSFORM);

static void
gst_ti_perf_overlay_finalize (GObject * obj);
static void
gst_ti_perf_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_ti_perf_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static
    gboolean
gst_ti_perf_overlay_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static
    GstFlowReturn
gst_ti_perf_overlay_transform_ip (GstBaseTransform * trans, GstBuffer * buffer);
static void
update_perf_stats (GstTIPerfOverlay * self);
static
    guint
get_graph_count (GstTIPerfOverlay * self);
static void
dump_perf_stats (GstTIPerfOverlay * self);
static void
trim_string(gchar *s);
static void
overlay_perf_stats_graph (GstTIPerfOverlay * self);
static void
overlay_perf_stats_text (GstTIPerfOverlay * self);

/* Initialize the plugin's class */
static void
gst_ti_perf_overlay_class_init (GstTIPerfOverlayClass * klass)
{
  GObjectClass *
      gobject_class = NULL;
  GstBaseTransformClass *
      gstbasetransform_class = NULL;
  GstElementClass *
      gstelement_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasetransform_class = GST_BASE_TRANSFORM_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TI Perf Overlay",
      "Filter/Converter/Tensor",
      "Gets the Hardware performance and reports it",
      "Abhay Chirania <a-chirania@ti.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_ti_perf_overlay_set_property;
  gobject_class->get_property = gst_ti_perf_overlay_get_property;

  g_object_class_install_property (gobject_class, PROP_OVERLAY_STATS,
      g_param_spec_boolean ("overlay", "Overlay Stats",
          "Overlay Performance Stats on frame",
          TRUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_OVERLAY_TYPE,
    g_param_spec_enum ("overlay-type", "Overlay Type",
        "Type of overlay",
        GST_TYPE_TI_PERF_OVERLAY_TYPE,
        DEFAULT_TI_PERF_OVERLAY_TYPE,
        (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_DUMP_STATS,
      g_param_spec_boolean ("dump", "Dump Stats ",
          "Dump Performance Stats on the terminal",
          FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_UPDATE_FPS_INTERVAL,
      g_param_spec_uint ("update-fps-interval", "Update FPS Interval",
          "Time in millisecond to update fps after",
          1, G_MAXINT, DEFAULT_UPDATE_FPS_INTERVAL,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_UPDATE_STATS_INTERVAL,
      g_param_spec_uint ("update-stats-interval", "Update Stats Interval",
          "Time in millisecond to update stats after",
          1, G_MAXINT, DEFAULT_UPDATE_STATS_INTERVAL,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MAIN_TITLE,
      g_param_spec_string ("main-title", "Overlay Main Title",
          "Main Title to overlay (Set to null or \"\" to disable)",
          DEFAULT_MAIN_TITLE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_TITLE,
      g_param_spec_string ("title", "Overlay Title",
          "Title to overlay",
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "File location",
          "File location to dump Stats",
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_START_DUMPS,
      g_param_spec_uint ("start-dumps", "Start Frame for dumping",
          "Start Frame for dumping stats in file", 0, G_MAXUINT,
          DEFAULT_START_DUMPS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_NUM_DUMPS,
      g_param_spec_uint ("num-dumps", "Number of perf samples",
          "Number of samples to dump in file", 0, G_MAXUINT,
          DEFAULT_NUM_DUMPS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  gstbasetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_ti_perf_overlay_set_caps);
  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_ti_perf_overlay_transform_ip);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_perf_overlay_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_perf_overlay_debug,
      "tiperfoverlay", 0, "TI Perf Overlay");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_ti_perf_overlay_init (GstTIPerfOverlay * self)
{
  GST_LOG_OBJECT (self, "init");
  self->caps_negotiated = FALSE;
  self->overlay = TRUE;
  self->overlay_type = DEFAULT_TI_PERF_OVERLAY_TYPE;
  self->dump_stats = FALSE;
  self->image_width = 0;
  self->image_height = 0;
  self->uv_offset = 0;
  self->image_handler = new Image;
  self->text_color = new YUVColor;
  self->small_font_property = new FontProperty;
  self->very_small_font_property = new FontProperty;
  self->big_font_property = new FontProperty;
  self->overlay_color = new YUVColor;
  self->overlay_text_color = new YUVColor;
  self->graph_bg_color = new YUVColor;
  self->color_red = new YUVColor;
  self->color_yellow = new YUVColor;
  self->color_green = new YUVColor;
  self->color_blue = new YUVColor;
  self->color_purple = new YUVColor;
  self->color_orange = new YUVColor;
  self->color_pink = new YUVColor;
  self->color_white = new YUVColor;
  self->color_black = new YUVColor;
  self->overlay_pos_y = 0;
  self->overlay_pos_x = 0;
  self->overlay_width = 0;
  self->overlay_height = 0;
  self->graph_pos_y = 0;
  self->graph_pos_x = 0;
  self->graph_offset_x = 0;
  self->graph_width = 0;
  self->graph_height = 0;
  self->num_graphs = 0;
  self->update_fps_interval = DEFAULT_UPDATE_FPS_INTERVAL;
  self->update_fps_interval_m = GST_MSECOND * self->update_fps_interval;
  self->update_stats_interval = DEFAULT_UPDATE_STATS_INTERVAL;
  self->update_stats_interval_m = GST_MSECOND * self->update_stats_interval;
  self->frames_rendered = 0;
  self->fps = 0;
  self->previous_fps_timestamp = GST_CLOCK_TIME_NONE;
  self->previous_stats_timestamp = GST_CLOCK_TIME_NONE;
  self->fps_x_pos = 0;
  self->fps_y_pos = 0;
  self->fps_width = 0;
  self->fps_height = 0;
  self->main_title = (gchar*) DEFAULT_MAIN_TITLE;
  self->title = NULL;
  self->location = NULL;
  self->start_dumps = DEFAULT_START_DUMPS;
  self->num_dumps = DEFAULT_NUM_DUMPS;
  self->main_title_font_property = new FontProperty;
  self->title_font_property = new FontProperty;
  getColor (self->text_color, TEXT_COLOR);
  getColor (self->overlay_color, OVERLAY_COLOR);
  getColor (self->overlay_text_color, OVERLAY_TEXT_COLOR);
  getColor (self->graph_bg_color, GRAPH_BG_COLOR);
  getColor (self->color_red, 255, 43, 43);
  getColor (self->color_yellow, 245, 227, 66);
  getColor (self->color_green, 43, 255, 43);
  getColor (self->color_blue, 0, 225, 255);
  getColor (self->color_purple, 113, 0, 199);
  getColor (self->color_orange, 255, 150, 46);
  getColor (self->color_pink, 204, 73, 131);
  getColor (self->color_white, 255, 255, 255);
  getColor (self->color_black, 0, 0, 0);
  self->frame_count = 0;
  self->dump_count = 0;
  self->cpu_load = new perfStatsCpuLoad;
  perfStatsResetCpuLoadCalc (self->cpu_load);
  self->ddr_load = new perf_stats_ddr_stats_t;
  self->soc_temp = new perfStatsSOCTemp;
  perfStatsSocTempInit (self->soc_temp);
  self->soc_temp_cpu_idx = 0;
  for (guint i = 0; i < NUM_THERMAL_ZONE; i++) {
    if(NULL != g_strrstr (self->soc_temp->thermal_zone_name[i], "CPU")) {
      self->soc_temp_cpu_idx = i;
      break;
    }
  }
#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)
  self->tiovx_context = gst_tiovx_context_new ();
  if (NULL == self->tiovx_context) {
    GST_ERROR_OBJECT (self, "Failed to do common initialization");
  }
#endif
  return;
}

static void
gst_ti_perf_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIPerfOverlay *
      self = GST_TI_PERF_OVERLAY (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_OVERLAY_STATS:
      self->overlay = g_value_get_boolean (value);
      break;
    case PROP_OVERLAY_TYPE:
      self->overlay_type = g_value_get_enum (value);
      break;
    case PROP_DUMP_STATS:
      self->dump_stats = g_value_get_boolean (value);
      break;
    case PROP_UPDATE_FPS_INTERVAL:
      self->update_fps_interval = g_value_get_uint (value);
      self->update_fps_interval_m = GST_MSECOND * self->update_fps_interval;
      break;
    case PROP_UPDATE_STATS_INTERVAL:
      self->update_stats_interval = g_value_get_uint (value);
      self->update_stats_interval_m = GST_MSECOND * self->update_stats_interval;
      break;
    case PROP_MAIN_TITLE:
      self->main_title = g_value_dup_string (value);
      break;
    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;
    case PROP_LOCATION:
      self->location = g_value_dup_string (value);
      break;
    case PROP_START_DUMPS:
      self->start_dumps = g_value_get_uint (value);
      break;
    case PROP_NUM_DUMPS:
      self->num_dumps = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_ti_perf_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIPerfOverlay *
      self = GST_TI_PERF_OVERLAY (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_OVERLAY_STATS:
      g_value_set_boolean (value, self->overlay);
      break;
    case PROP_OVERLAY_TYPE:
      g_value_set_enum (value, self->overlay_type);
      break;
    case PROP_DUMP_STATS:
      g_value_set_boolean (value, self->dump_stats);
      break;
    case PROP_UPDATE_FPS_INTERVAL:
      g_value_set_uint (value, self->update_fps_interval);
      break;
    case PROP_UPDATE_STATS_INTERVAL:
      g_value_set_uint (value, self->update_stats_interval);
      break;
    case PROP_MAIN_TITLE:
      g_value_set_string (value, self->main_title);
      break;
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;
    case PROP_LOCATION:
      g_value_set_string (value, self->location);
      break;
    case PROP_START_DUMPS:
      g_value_set_uint (value, self->start_dumps);
      break;
    case PROP_NUM_DUMPS:
      g_value_set_uint (value, self->num_dumps);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_ti_perf_overlay_finalize (GObject * obj)
{
  GstTIPerfOverlay *
      self = GST_TI_PERF_OVERLAY (obj);
  GST_LOG_OBJECT (self, "finalize");

  if (self->dump_fd) {
    fclose(self->dump_fd);
    self->dump_fd = NULL;
  }
#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)
  if (self->tiovx_context) {
    g_object_unref (self->tiovx_context);
    self->tiovx_context = NULL;
  }
#endif
  G_OBJECT_CLASS (gst_ti_perf_overlay_parent_class)->finalize (obj);
}

static
    gboolean
gst_ti_perf_overlay_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstTIPerfOverlay *
      self = GST_TI_PERF_OVERLAY (trans);
  gboolean ret = TRUE;

  if (self->caps_negotiated == FALSE) {
    GstVideoInfo video_info;
    if (!gst_video_info_from_caps (&video_info, incaps)) {
        GST_WARNING_OBJECT (self,
            "Failed to get info from caps, disabling overlay");
        goto no_overlay;
    }

    if (GST_VIDEO_FORMAT_NV12 != GST_VIDEO_INFO_FORMAT (&video_info)) {
      goto no_overlay;
    }

    self->image_width = GST_VIDEO_INFO_WIDTH (&video_info);
    self->image_height = GST_VIDEO_INFO_HEIGHT (&video_info);

    self->overlay_width = self->image_width;
    self->overlay_height = TOTAL_OVERLAY_HEIGHT_PCT * self->image_height;

    if (self->overlay_height > MAX_OVERLAY_HEIGHT) {
        self->overlay_height = MAX_OVERLAY_HEIGHT;
    }

    else if (self->overlay_height < MIN_OVERLAY_HEIGHT) {
        self->overlay_height = MIN_OVERLAY_HEIGHT;
    }

    self->overlay_pos_y = self->image_height - self->overlay_height;

    self->uv_offset = self->image_height * self->image_width;
    self->image_handler->width = self->image_width;
    self->image_handler->height = self->image_height;
    getFont (self->very_small_font_property, (int) (0.005 * self->overlay_width));
    getFont (self->small_font_property, (int) (0.01 * self->overlay_width));
    getFont (self->big_font_property, (int) (0.011 * self->overlay_width));
    getFont (self->main_title_font_property, (int) (0.02 * self->image_width));
    getFont (self->title_font_property, (int) (0.015 * self->image_width));

    self->fps_x_pos = (self->image_width - (10*self->big_font_property->width));
    self->fps_y_pos = 0;
    self->fps_width = 10*self->big_font_property->width;
    self->fps_height = self->big_font_property->height+1;
    self->graph_height = (0.5 * self->overlay_height);
    self->graph_pos_y = self->overlay_pos_y + 10;
    self->graph_width = (0.03 * self->overlay_width);
    goto exit;
  }

no_overlay:
  self->overlay = FALSE;

exit:
  self->caps_negotiated = TRUE;
  return ret;
}

static
    GstFlowReturn
gst_ti_perf_overlay_transform_ip (GstBaseTransform * trans, GstBuffer * buffer)
{
  GstTIPerfOverlay *
      self = GST_TI_PERF_OVERLAY (trans);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstMapInfo buffer_mapinfo;
  GstClockTime current_timestamp;
  gchar text_buffer[128];

  GST_LOG_OBJECT (self, "transform_ip");

  g_atomic_int_inc (&self->frames_rendered);
  current_timestamp = gst_util_get_timestamp ();

  if (self->previous_fps_timestamp == GST_CLOCK_TIME_NONE) {
      self->previous_fps_timestamp = current_timestamp;
  }
  else if (GST_CLOCK_DIFF (self->previous_fps_timestamp, current_timestamp) >
          self->update_fps_interval_m) {
      gdouble mul_factor = (double)(self->update_fps_interval)/1000;
      self->fps = round(self->frames_rendered/mul_factor);
      self->previous_fps_timestamp = current_timestamp;
      self->frame_count += self->frames_rendered;
      self->frames_rendered = 0;
  }

  if (self->previous_stats_timestamp == GST_CLOCK_TIME_NONE) {
      self->previous_stats_timestamp = current_timestamp;
      update_perf_stats (self);
  } else if (GST_CLOCK_DIFF
              (self->previous_stats_timestamp, current_timestamp) >
              self->update_stats_interval_m) {
      update_perf_stats (self);
      self->previous_stats_timestamp = current_timestamp;
      if (self->dump_stats) {
        dump_perf_stats (self);
      }
  }

  if (self->overlay) {
    if (!gst_buffer_map (buffer, &buffer_mapinfo, GST_MAP_READWRITE)) {
      GST_ERROR_OBJECT (self, "failed to map buffer");
      goto exit;
    }

    self->image_handler->yRowAddr = (uint8_t *) buffer_mapinfo.data;
    self->image_handler->uvRowAddr =
          self->image_handler->yRowAddr + self->uv_offset;

    if (g_strcmp0("null", self->main_title ) != 0 &&
        g_strcmp0("", self->main_title ) != 0) {
      drawText (self->image_handler,
          self->main_title,
          5,
          5,
          self->main_title_font_property,
          self->color_red);

      if (self->title) {
        drawText (self->image_handler,
                  self->title,
                  5,
                  5+self->main_title_font_property->height,
                  self->title_font_property,
                  self->color_green);
      }
    }

    else if (self->title) {
      drawText (self->image_handler,
                self->title,
                5,
                5,
                self->title_font_property,
                self->color_green);
    }

    if (self->overlay_type == 0x00) {
      overlay_perf_stats_graph (self);
    }
    else if (self->overlay_type == 0x01) {
      overlay_perf_stats_text (self);
    }

    fillRegion (self->image_handler,
                self->fps_x_pos,
                self->fps_y_pos,
                self->fps_width,
                self->fps_height,
                self->color_black);

    drawText (self->image_handler,
              "FPS:  ",
              self->fps_x_pos+self->big_font_property->width,
              self->fps_y_pos,
              self->big_font_property,
              self->color_white);
    sprintf (text_buffer,"%u" , self->fps);
    drawText (self->image_handler,
              text_buffer,
              self->fps_x_pos+(7*self->big_font_property->width),
              self->fps_y_pos,
              self->big_font_property,
              self->color_green);

    if(NUM_THERMAL_ZONE > 0) {
      guint cpu_temp = \
        self->soc_temp->thermal_zone_temp[self->soc_temp_cpu_idx];

      if (cpu_temp < 60) {
        self->soc_temp_text_color = self->color_blue;
      } else if (cpu_temp >= 60 && cpu_temp <= 80) {
        self->soc_temp_text_color = self->color_yellow;
      } else {
        self->soc_temp_text_color = self->color_red;
      }

      fillRegion (self->image_handler,
                  self->fps_x_pos,
                  self->fps_y_pos + self->fps_height,
                  self->fps_width,
                  self->fps_height,
                  self->color_black);

      drawText (self->image_handler,
                "TEMP: ",
                self->fps_x_pos+self->big_font_property->width,
                self->fps_y_pos+self->big_font_property->height,
                self->big_font_property,
                self->color_white);

      sprintf (text_buffer,"%u", cpu_temp);
      drawText (self->image_handler,
                text_buffer,
                self->fps_x_pos+(7*self->big_font_property->width),
                self->fps_y_pos+self->big_font_property->height,
                self->big_font_property,
                self->soc_temp_text_color);
    }

    gst_buffer_unmap (buffer, &buffer_mapinfo);
  }

  ret = GST_FLOW_OK;
exit:
  return ret;
}

static
    guint
get_graph_count (GstTIPerfOverlay * self)
{
  guint count = 0;

  // MPU Load
  count += 1;

#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)

  // C7x Load
  for (guint cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    gchar *cpuName = appIpcGetCpuName(cpu_id);
    if (appIpcIsCpuEnabled (cpu_id) && NULL != g_strrstr (cpuName, "c7x")) {
      count++;
    }
  }

  //HWA
  for (guint i = 0; i < sizeof (self->hwa_loads) / sizeof (self->hwa_loads[0]); i++) {
    guint hwa_id;
    app_perf_stats_hwa_load_t *
        hwaLoad;
    for (hwa_id = 0; hwa_id < APP_PERF_HWA_MAX; hwa_id++) {
      app_perf_hwa_id_t id = (app_perf_hwa_id_t) hwa_id;
      hwaLoad = &self->hwa_loads[i].hwa_stats[id];
      if (hwaLoad->active_time > 0 && hwaLoad->pixels_processed > 0
          && hwaLoad->total_time > 0) {
        count++;
      }
    }
  }
#endif

  //DDR READ AND WRITE AND TOTAL
  count += 3;

  return count;
}

static
  void
update_perf_stats (GstTIPerfOverlay * self)
{
  guint curr_graph_count;

  // MPU Load
  perfStatsCpuLoadCalc (self->cpu_load);

  // MPU Reset
  perfStatsResetCpuLoadCalc (self->cpu_load);

  // DDR
  *(self->ddr_load) = *(perfStatsDdrStatsGet ());

  // DDR Reset
  perfStatsResetDdrLoadCalcAll ();

  // SOC Temp Calc
  perfStatsSocTempGet (self->soc_temp);

#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)

  guint cpu_id;

  // C7x Load
  for (cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    gchar *cpuName = appIpcGetCpuName(cpu_id);
    if (appIpcIsCpuEnabled (cpu_id) && NULL != g_strrstr (cpuName, "c7x")) {
      appPerfStatsCpuLoadGet (cpu_id, &self->c7x_loads[cpu_id]);
    }
  }

  // HWA
  guint hwa_count = 0;
#if defined(SOC_AM62A)
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MCU1_0, &self->hwa_loads[hwa_count++]);
#else
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MCU2_0, &self->hwa_loads[hwa_count++]);
#if !defined(SOC_J722S)
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MCU2_1, &self->hwa_loads[hwa_count++]);
#endif
#if defined(SOC_J784S4)
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MCU4_0, &self->hwa_loads[hwa_count++]);
#endif
#endif
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MPU1_0, &self->hwa_loads[hwa_count++]);

  // Reset C7X
  for (cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    gchar *cpuName = appIpcGetCpuName(cpu_id);
    if (appIpcIsCpuEnabled (cpu_id) && NULL != g_strrstr (cpuName, "c7x")) {
      appPerfStatsCpuLoadReset (cpu_id);
    }
  }

  // Reset HWA
#if defined(SOC_AM62A)
  appRemoteServiceRun (APP_IPC_CPU_MCU1_0, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
#else
  appRemoteServiceRun (APP_IPC_CPU_MCU2_0, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
#if !defined(SOC_J722S)
  appRemoteServiceRun (APP_IPC_CPU_MCU2_1, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
#endif
#if defined(SOC_J784S4)
  appRemoteServiceRun (APP_IPC_CPU_MCU4_0, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
#endif
#endif
  appRemoteServiceRun (APP_IPC_CPU_MPU1_0, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
#endif

  curr_graph_count = get_graph_count (self);
  if (self->num_graphs == curr_graph_count)
    return;

  self->num_graphs = curr_graph_count;

  if (self->overlay && self->overlay_type == 0x00) {
    delete self->bar_graphs;
    self->bar_graphs = new BarGraph[self->num_graphs];
    self->graph_offset_x = (self->overlay_width -
        (self->graph_width * self->num_graphs)) / self->num_graphs;
    self->graph_pos_x = self->graph_offset_x - (self->graph_offset_x / 2);
  }
}

static void
overlay_perf_stats_graph (GstTIPerfOverlay * self)
{
  fillRegion (self->image_handler,
      self->overlay_pos_x,
      self->overlay_pos_y,
      self->overlay_width, self->overlay_height, self->overlay_color);

  guint graph_x = self->graph_pos_x;
  guint count = 0;
  gint value;

  initGraph (&self->bar_graphs[count],
             self->image_handler,
             graph_x,
             self->graph_pos_y,
             self->graph_width,
             self->graph_height,
             100,
             "mpu",
              "%",
             self->small_font_property,
             self->big_font_property,
             self->overlay_text_color,
             self->color_green,
             self->graph_bg_color);
  value = self->cpu_load->cpu_load / 100u;
  if (value > 50 && value <= 80) {
    self->bar_graphs[count].m_fillColor = self->color_yellow;
  } else if (value > 80) {
    self->bar_graphs[count].m_fillColor = self->color_red;
  }
  updateGraph (&self->bar_graphs[count],value);
  graph_x += self->graph_offset_x + self->graph_width;
  count++;

#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)

  app_perf_stats_cpu_load_t c7x_load;
  for (guint cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    gchar *cpuName = appIpcGetCpuName(cpu_id);
    if (appIpcIsCpuEnabled (cpu_id) && NULL != g_strrstr (cpuName, "c7x")) {
      c7x_load = self->c7x_loads[cpu_id];
      initGraph (&self->bar_graphs[count],
                 self->image_handler,
                 graph_x,
                 self->graph_pos_y,
                 self->graph_width,
                 self->graph_height,
                 100,
                 cpuName,
                 "%",
                 self->small_font_property,
                 self->big_font_property,
                 self->overlay_text_color,
                 self->color_green,
                 self->graph_bg_color);
      value = c7x_load.cpu_load / 100u;
      if (value > 50 && value <= 80) {
        self->bar_graphs[count].m_fillColor = self->color_yellow;
      } else if (value > 80) {
        self->bar_graphs[count].m_fillColor = self->color_red;
      }
      updateGraph (&self->bar_graphs[count],value);
      graph_x += self->graph_offset_x + self->graph_width;
      count++;
    }
  }

  for (guint i = 0; i < sizeof (self->hwa_loads) / sizeof (self->hwa_loads[0]); i++) {
    guint hwa_id;
    app_perf_stats_hwa_load_t *
        hwaLoad;
    uint64_t load;
    for (hwa_id = 0; hwa_id < APP_PERF_HWA_MAX; hwa_id++) {
      app_perf_hwa_id_t id = (app_perf_hwa_id_t) hwa_id;
      hwaLoad = &self->hwa_loads[i].hwa_stats[id];
      if (hwaLoad->active_time > 0 && hwaLoad->pixels_processed > 0
          && hwaLoad->total_time > 0) {
        initGraph (&self->bar_graphs[count],self->image_handler, graph_x,
            self->graph_pos_y, self->graph_width, self->graph_height, 100,
            appPerfStatsGetHwaName (id), "%", self->small_font_property,
            self->big_font_property, self->overlay_text_color,
            self->color_green, self->graph_bg_color);
        load = (hwaLoad->active_time * 10000) / hwaLoad->total_time;
        value = load / 100;
        if (value > 50 && value <= 80) {
          self->bar_graphs[count].m_fillColor = self->color_yellow;
        } else if (value > 80) {
          self->bar_graphs[count].m_fillColor = self->color_red;
        }
        updateGraph (&self->bar_graphs[count],value);
        graph_x += self->graph_offset_x + self->graph_width;
        count++;
      }
    }
  }
#endif
  initGraph (&self->bar_graphs[count],
             self->image_handler,
             graph_x,
             self->graph_pos_y,
             self->graph_width,
             self->graph_height,
             self->ddr_load->total_available_bw,
             "DDR RD",
             "MB/s",
             self->small_font_property,
             self->big_font_property,
             self->overlay_text_color,
             self->color_purple,
             self->graph_bg_color);
  updateGraph (&self->bar_graphs[count],self->ddr_load->read_bw_avg);
  graph_x += self->graph_offset_x + self->graph_width;
  count++;

  initGraph (&self->bar_graphs[count],
             self->image_handler,
             graph_x,
             self->graph_pos_y,
             self->graph_width,
             self->graph_height,
             self->ddr_load->total_available_bw,
             "DDR WR",
             "MB/s",
             self->small_font_property,
             self->big_font_property,
             self->overlay_text_color,
             self->color_orange,
             self->graph_bg_color);
  updateGraph (&self->bar_graphs[count],self->ddr_load->write_bw_avg);
  graph_x += self->graph_offset_x + self->graph_width;
  count++;

  initGraph (&self->bar_graphs[count],
             self->image_handler,
             graph_x,
             self->graph_pos_y,
             self->graph_width,
             self->graph_height,
             self->ddr_load->total_available_bw,
             "DDR Total",
             "MB/s",
             self->small_font_property,
             self->big_font_property,
             self->overlay_text_color,
             self->color_pink,
             self->graph_bg_color);
  int total_bw = self->ddr_load->write_bw_avg + self->ddr_load->read_bw_avg;
  updateGraph (&self->bar_graphs[count],total_bw);
  graph_x += self->graph_offset_x + self->graph_width;
  count++;
}

static void
overlay_perf_stats_text (GstTIPerfOverlay * self)
{
  guint height;
  guint text_y;
  guint value;
  gchar buffer[50];
  YUVColor *text_color;

  height = self->num_graphs * self->very_small_font_property->height + 10;

  text_y = self->image_height - height + 5;

  fillRegion (self->image_handler,
              0,
              self->image_height - height,
              self->very_small_font_property->width * 20,
              height,
              self->overlay_color);

  value = self->cpu_load->cpu_load / 100u;
  if (value <= 50) {
    text_color = self->color_green;
  } else if (value > 50 && value <= 80) {
    text_color = self->color_yellow;
  } else {
    text_color = self->color_red;
  }
  sprintf(buffer, "MPU: %d%%", value);
  trim_string(buffer);
  drawText (self->image_handler,
            buffer,
            5,
            text_y,
            self->very_small_font_property,
            text_color);
  text_y += self->very_small_font_property->height;

#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)

  app_perf_stats_cpu_load_t c7x_load;
  for (guint cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    gchar *cpuName = appIpcGetCpuName(cpu_id);
    if (appIpcIsCpuEnabled (cpu_id) && NULL != g_strrstr (cpuName, "c7x")) {
      c7x_load = self->c7x_loads[cpu_id];
      value = c7x_load.cpu_load / 100u;
      if (value <= 50) {
        text_color = self->color_green;
      } else if (value > 50 && value <= 80) {
        text_color = self->color_yellow;
      } else {
        text_color = self->color_red;
      }
      sprintf(buffer, "%s: %d%%", cpuName, value);
      trim_string(buffer);
      drawText (self->image_handler,
                buffer,
                5,
                text_y,
                self->very_small_font_property,
                text_color);
      text_y += self->very_small_font_property->height;
    }
  }

  for (guint i = 0; i < sizeof (self->hwa_loads) / sizeof (self->hwa_loads[0]); i++) {
    guint hwa_id;
    app_perf_stats_hwa_load_t *hwaLoad;
    uint64_t load;
    for (hwa_id = 0; hwa_id < APP_PERF_HWA_MAX; hwa_id++) {
      app_perf_hwa_id_t id = (app_perf_hwa_id_t) hwa_id;
      hwaLoad = &self->hwa_loads[i].hwa_stats[id];
      if (hwaLoad->active_time > 0 && hwaLoad->pixels_processed > 0
          && hwaLoad->total_time > 0) {
        load = (hwaLoad->active_time * 10000) / hwaLoad->total_time;
        value = load / 100;
        if (value <= 50) {
          text_color = self->color_green;
        } else if (value > 50 && value <= 80) {
          text_color = self->color_yellow;
        } else {
          text_color = self->color_red;
        }
        sprintf(buffer, "%s: %d%%", appPerfStatsGetHwaName (id), value);
        trim_string(buffer);
        drawText (self->image_handler,
                  buffer,
                  5,
                  text_y,
                  self->very_small_font_property,
                  text_color);
        text_y += self->very_small_font_property->height;
      }
    }
  }
#endif
  sprintf(buffer, "DDR RD: %d MB/s", self->ddr_load->read_bw_avg);
  trim_string(buffer);
  drawText (self->image_handler,
            buffer,
            5,
            text_y,
            self->very_small_font_property,
            self->color_purple);
  text_y += self->very_small_font_property->height;

  sprintf(buffer, "DDR WR: %d MB/s", self->ddr_load->write_bw_avg);
  trim_string(buffer);
  drawText (self->image_handler,
            buffer,
            5,
            text_y,
            self->very_small_font_property,
            self->color_orange);
  text_y += self->very_small_font_property->height;

  sprintf(buffer, "DDR Tot: %d MB/s", self->ddr_load->write_bw_avg + self->ddr_load->read_bw_avg);
  trim_string(buffer);
  drawText (self->image_handler,
            buffer,
            5,
            text_y,
            self->very_small_font_property,
            self->color_pink);
}

static void
dump_perf_stats (GstTIPerfOverlay * self)
{
  if (self->location && self->dump_count == 0 &&
      self->frame_count > self->start_dumps) {
    self->dump_fd = fopen (self->location, "a");
    if (NULL == self->dump_fd)
        return;
    fprintf (self->dump_fd, "name,title");
  }

  if (self->dump_count > self->num_dumps && self->dump_fd) {
    fclose(self->dump_fd);
    self->dump_fd = NULL;
  }

  if (self->location && self->dump_count != 0 && self->dump_fd) {
    fprintf (self->dump_fd, "%s", GST_ELEMENT_NAME(self));
    if (self->title) {
      fprintf (self->dump_fd, ",%s" , self->title);
    } else {
      fprintf (self->dump_fd, ",None");
    }
  }

  printf ("CPU: mpu: TOTAL LOAD = %.2f\n", (float)self->cpu_load->cpu_load/100);
  if (self->dump_count == 0 && self->dump_fd) {
    fprintf (self->dump_fd, ",mpu");
  } else if (self->dump_fd) {
    fprintf (self->dump_fd, ",%.2f", (float)self->cpu_load->cpu_load/100);
  }

#if defined(SOC_AM62A) || defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4) || defined(SOC_J722S)

  app_perf_stats_cpu_load_t c7x_load;
  for (guint cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    gchar *cpuName = appIpcGetCpuName(cpu_id);
    if (appIpcIsCpuEnabled (cpu_id) && NULL != g_strrstr (cpuName, "c7x")) {
      c7x_load = self->c7x_loads[cpu_id];
      printf ("CPU: %6s: TOTAL LOAD = %.2f\n",
          appIpcGetCpuName (cpu_id),
          (float)c7x_load.cpu_load/100);
      if (self->dump_count == 0 && self->dump_fd) {
        fprintf (self->dump_fd, ",%s", appIpcGetCpuName (cpu_id));
      } else if (self->dump_fd) {
        fprintf (self->dump_fd, ",%.2f", (float)c7x_load.cpu_load/100);
      }
    }
  }

  for (guint i = 0; i < sizeof (self->hwa_loads) / sizeof (self->hwa_loads[0]); i++) {
    guint hwa_id;
    app_perf_stats_hwa_load_t *
        hwaLoad;
    uint64_t load;
    for (hwa_id = 0; hwa_id < APP_PERF_HWA_MAX; hwa_id++) {
      app_perf_hwa_id_t id = (app_perf_hwa_id_t) hwa_id;
      hwaLoad = &self->hwa_loads[i].hwa_stats[id];

      if (self->dump_count == 0 && self->dump_fd) {
        fprintf (self->dump_fd, ",%s", appPerfStatsGetHwaName (id));
      }

      if (hwaLoad->active_time > 0 && hwaLoad->pixels_processed > 0
          && hwaLoad->total_time > 0) {
        load = (hwaLoad->active_time * 10000) / hwaLoad->total_time;
        printf ("HWA: %6s: LOAD = %.2f %% ( %lu MP/s )\n",
            appPerfStatsGetHwaName (id),
            (float)load/100, (hwaLoad->pixels_processed / hwaLoad->total_time)
            );
        if (self->dump_fd && self->dump_count > 0) {
          fprintf (self->dump_fd, ",%.2f", (float)load/100);
        }
      } else {
        if (self->dump_fd && self->dump_count > 0) {
          fprintf (self->dump_fd, ",%.2f", 0.0);
        }
      }
    }
  }
#endif

  if (self->dump_count == 0 && self->dump_fd) {
    fprintf (self->dump_fd, ",ddr_read_avg,ddr_read_peak");
    fprintf (self->dump_fd, ",ddr_write_avg,ddr_write_peak");
    fprintf (self->dump_fd, ",ddr_total_avg,ddr_total_peak");
    for (guint i = 0; i < NUM_THERMAL_ZONE; i++) {
      fprintf (self->dump_fd, ",temp_%s", self->soc_temp->thermal_zone_name[i]);
    }
    fprintf (self->dump_fd, ",fps\n");
    self->dump_count++;
  } else if (self->dump_fd) {
    fprintf (self->dump_fd, ",%d,%d",
        self->ddr_load->read_bw_avg, self->ddr_load->read_bw_peak);
    fprintf (self->dump_fd, ",%d,%d",
        self->ddr_load->write_bw_avg, self->ddr_load->write_bw_peak);
    fprintf (self->dump_fd, ",%d,%d",
        self->ddr_load->read_bw_avg + self->ddr_load->write_bw_avg,
        self->ddr_load->write_bw_peak + self->ddr_load->read_bw_peak);
    for (guint i = 0; i < NUM_THERMAL_ZONE; i++) {
      fprintf (self->dump_fd, ",%.2f", self->soc_temp->thermal_zone_temp[i]);
    }
    fprintf (self->dump_fd, ",%d\n",self->fps);
    self->dump_count++;
  }

  printf ("DDR: READ  BW: AVG = %6d MB/s, PEAK = %6d MB/s\n",
      self->ddr_load->read_bw_avg, self->ddr_load->read_bw_peak);
  printf ("DDR: WRITE BW: AVG = %6d MB/s, PEAK = %6d MB/s\n",
      self->ddr_load->write_bw_avg, self->ddr_load->write_bw_peak);
  printf ("DDR: TOTAL BW: AVG = %6d MB/s, PEAK = %6d MB/s\n",
      self->ddr_load->read_bw_avg + self->ddr_load->write_bw_avg,
      self->ddr_load->write_bw_peak + self->ddr_load->read_bw_peak);

  for (guint i = 0; i < NUM_THERMAL_ZONE; i++) {
    printf ("TEMP: %s = %.2f C\n",
      self->soc_temp->thermal_zone_name[i],
      self->soc_temp->thermal_zone_temp[i]);
  }

  printf ("FPS: %d\n",self->fps);
  printf("\n");
}

static void
trim_string(gchar *s)
{
	int  i,j;

	for(i=0;s[i]==' '||s[i]=='\t';i++);

	for(j=0;s[i];i++)
	{
		s[j++]=s[i];
	}
	s[j]='\0';
	for(i=0;s[i]!='\0';i++)
	{
		if(s[i]!=' '&& s[i]!='\t')
				j=i;
	}
	s[j+1]='\0';
}
