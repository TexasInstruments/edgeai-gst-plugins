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
extern "C"
{

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <TI/tivx.h>

#include "gsttiperfoverlay.h"
#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "gst-libs/gst/tiovx/gsttiovxcontext.h"
#include "utils/app_init/include/app_init.h"
#include "utils/perf_stats/include/app_perf_stats.h"
#include "utils/perf_stats/src/app_perf_stats_priv.h"
#include "utils/ipc/include/app_ipc.h"
#include "utils/remote_service/include/app_remote_service.h"
}
#include <ti_post_process.h>

using namespace
    ti::post_process;

/* Properties definition */
enum
{
  PROP_0,
  PROP_OVERLAY_GRAPHICS,
  PROP_OVERLAY_TEXT,
  PROP_UPDATE_STATS_INTERVAL,
  PROP_UPDATE_FPS_INTERVAL,
  PROP_TITLE,
};

#define DEFAULT_UPDATE_STATS_INTERVAL 500 // Update fps after ? millisecond
#define DEFAULT_UPDATE_FPS_INTERVAL 2000 // Update fps after ? millisecond

#define TEXT_COLOR 122,13,255

#define OVERLAY_COLOR 0,0,0
#define OVERLAY_TEXT_COLOR 255,255,255
#define GRAPH_BG_COLOR 45,45,45

#define MAX_OVERLAY_HEIGHT 250  //Max allowed overlay height
#define MIN_OVERLAY_HEIGHT 50   //MIn allowed overlay height
#define TOTAL_OVERLAY_HEIGHT_PCT 0.2    // Percentage of total height to make as ovarlay
#define MAX_GRAPH 15            //Max allowed graphs

/* Formats definition */
#define TI_PERF_OVERLAY_SUPPORTED_FORMATS_SRC "{NV12}"
#define TI_PERF_OVERLAY_SUPPORTED_FORMATS_SINK "{NV12}"
#define TI_PERF_OVERLAY_SUPPORTED_WIDTH "[1 , 8192]"
#define TI_PERF_OVERLAY_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src/Sink caps */
#define TI_PERF_OVERLAY_STATIC_CAPS                               \
  "video/x-raw, "                                                 \
  "format = (string) " TI_PERF_OVERLAY_SUPPORTED_FORMATS_SRC ", " \
  "width = " TI_PERF_OVERLAY_SUPPORTED_WIDTH ", "                 \
  "height = " TI_PERF_OVERLAY_SUPPORTED_HEIGHT

/* Pads definitions */
static
    GstStaticPadTemplate
    src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_PERF_OVERLAY_STATIC_CAPS)
    );

static
    GstStaticPadTemplate
    sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_PERF_OVERLAY_STATIC_CAPS)
    );

struct _GstTIPerfOverlay
{
  GstBaseTransform
      element;
  gboolean
      overlay_graphics;
  gboolean
      overlay_text;
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
  app_perf_stats_cpu_load_t
      cpu_loads[APP_IPC_CPU_MAX];
  app_perf_stats_hwa_stats_t
      hwa_loads[4];
  app_perf_stats_ddr_stats_t
      ddr_load;
  GstTIOVXContext *
      tiovx_context;
  BarGraph *
      bar_graphs[MAX_GRAPH];
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
  YUVColor *
      fps_text_color;
  YUVColor *
      fps_bg_color;
  gchar *
      title;
  FontProperty *
      main_title_font_property;
  FontProperty *
      title_font_property;
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
static
    guint
update_perf_stats (GstTIPerfOverlay * self);
static void
display_perf_stats_text (GstTIPerfOverlay * self);
static void
display_perf_stats_graphics (GstTIPerfOverlay * self);

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

  g_object_class_install_property (gobject_class, PROP_OVERLAY_GRAPHICS,
      g_param_spec_boolean ("overlay-graphics", "Overlay Graphics",
          "Overlay Preformance graphics on frame",
          TRUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_OVERLAY_TEXT,
      g_param_spec_boolean ("overlay-text", "Overlay Text",
          "Overlay Preformance text on frame",
          TRUE,
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
  g_object_class_install_property (gobject_class, PROP_TITLE,
      g_param_spec_string ("title", "Overlay Title",
          "Title to overlay",
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
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
  self->overlay_graphics = TRUE;
  self->overlay_text = FALSE;
  self->image_width = 0;
  self->image_height = 0;
  self->uv_offset = 0;
  self->image_handler = new Image;
  self->text_color = new YUVColor;
  self->small_font_property = new FontProperty;
  self->big_font_property = new FontProperty;
  self->overlay_color = new YUVColor;
  self->overlay_text_color = new YUVColor;
  self->graph_bg_color = new YUVColor;
  self->color_red = new YUVColor;
  self->color_yellow = new YUVColor;
  self->color_green = new YUVColor;
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
  self->fps_text_color = new YUVColor;
  self->fps_bg_color = new YUVColor;
  self->title = NULL;
  self->main_title_font_property = new FontProperty;
  self->title_font_property = new FontProperty;
  self->tiovx_context = gst_tiovx_context_new ();
  if (NULL == self->tiovx_context) {
    GST_ERROR_OBJECT (self, "Failed to do common initialization");
  }
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
    case PROP_OVERLAY_GRAPHICS:
      self->overlay_graphics = g_value_get_boolean (value);
      break;
    case PROP_OVERLAY_TEXT:
      self->overlay_text = g_value_get_boolean (value);
      break;
    case PROP_UPDATE_FPS_INTERVAL:
      self->update_fps_interval = g_value_get_uint (value);
      self->update_fps_interval_m = GST_MSECOND * self->update_fps_interval;
      break;
    case PROP_UPDATE_STATS_INTERVAL:
      self->update_stats_interval = g_value_get_uint (value);
      self->update_stats_interval_m = GST_MSECOND * self->update_stats_interval;
      break;
    case PROP_TITLE:
      self->title = g_value_dup_string (value);
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
    case PROP_OVERLAY_GRAPHICS:
      g_value_set_boolean (value, self->overlay_graphics);
      break;
    case PROP_OVERLAY_TEXT:
      g_value_set_boolean (value, self->overlay_text);
      break;
    case PROP_UPDATE_FPS_INTERVAL:
      g_value_set_uint (value, self->update_fps_interval);
      break;
    case PROP_UPDATE_STATS_INTERVAL:
      g_value_set_uint (value, self->update_stats_interval);
      break;
    case PROP_TITLE:
      g_value_set_string (value, self->title);
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

  if (self->tiovx_context) {
    g_object_unref (self->tiovx_context);
    self->tiovx_context = NULL;
  }
  G_OBJECT_CLASS (gst_ti_perf_overlay_parent_class)->finalize (obj);
}

static
    gboolean
gst_ti_perf_overlay_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstTIPerfOverlay *
      self = GST_TI_PERF_OVERLAY (trans);
  GstVideoInfo video_info;
  gboolean ret = TRUE;

  if (!gst_video_info_from_caps (&video_info, incaps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from caps");
    ret = FALSE;
    goto exit;
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
  getFont (self->small_font_property, (int) (0.002 * self->overlay_width));
  getFont (self->big_font_property, (int) (0.01 * self->overlay_width));
  getColor (self->overlay_color, OVERLAY_COLOR);
  getColor (self->text_color, TEXT_COLOR);
  getColor (self->overlay_text_color, OVERLAY_TEXT_COLOR);
  getColor (self->graph_bg_color, GRAPH_BG_COLOR);
  getColor (self->color_red, 255, 43, 43);
  getColor (self->color_yellow, 245, 227, 66);
  getColor (self->color_green, 43, 255, 43);
  self->num_graphs = update_perf_stats (self);
  for (guint i = 0; i < self->num_graphs; i++) {
    self->bar_graphs[i] = new BarGraph;
  }

  self->graph_height = (0.5 * self->overlay_height);
  self->graph_pos_y = self->overlay_pos_y + 10;
  self->graph_width = (0.03 * self->overlay_width);
  self->graph_offset_x = (self->overlay_width -
      (self->graph_width * self->num_graphs)) / self->num_graphs;
  self->graph_pos_x = self->graph_offset_x - (self->graph_offset_x / 2);
  self->fps_x_pos = (self->image_width - (9*self->big_font_property->width));
  self->fps_y_pos = 0;
  self->fps_width = 9*self->big_font_property->width;
  self->fps_height = self->big_font_property->height;
  getColor (self->fps_text_color, 255, 255, 255);
  getColor (self->fps_bg_color, 0, 0, 0);

  getFont (self->main_title_font_property, (int) (0.02 * self->image_width));
  getFont (self->title_font_property, (int) (0.01 * self->image_width));

exit:
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
  gchar fps_buffer[20];

  GST_LOG_OBJECT (self, "transform_ip");
  if (!gst_buffer_map (buffer, &buffer_mapinfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (self, "failed to map buffer");
    goto exit;
  }

  self->image_handler->yRowAddr = (uint8_t *) buffer_mapinfo.data;
  self->image_handler->uvRowAddr =
        self->image_handler->yRowAddr + self->uv_offset;

  drawText (self->image_handler,
            "Texas Instruments EdgeAI Analytics",
            0,
            0,
            self->main_title_font_property,
            self->color_red);
  if (self->title) {
    drawText (self->image_handler,
              self->title,
              0,
              self->main_title_font_property->height,
              self->title_font_property,
              self->color_green);
  }


  if (self->overlay_graphics || self->overlay_text) {
    g_atomic_int_inc (&self->frames_rendered);
    current_timestamp = gst_util_get_timestamp ();

    if (self->previous_fps_timestamp == GST_CLOCK_TIME_NONE) {
        self->previous_fps_timestamp = current_timestamp;
    }
    else if (GST_CLOCK_DIFF (self->previous_fps_timestamp, current_timestamp) >
            self->update_fps_interval_m) {
            gdouble mul_factor = (double)(self->update_fps_interval)/1000;
            self->fps = self->frames_rendered/mul_factor;
            self->previous_fps_timestamp = current_timestamp;
            self->frames_rendered = 0;
    }

    if (self->previous_stats_timestamp == GST_CLOCK_TIME_NONE) {
        self->previous_stats_timestamp = current_timestamp;
    } else if (GST_CLOCK_DIFF
                (self->previous_stats_timestamp, current_timestamp) >
                self->update_stats_interval_m) {
        update_perf_stats (self);
        self->previous_stats_timestamp = current_timestamp;
    }

    if (self->overlay_graphics)
      display_perf_stats_graphics (self);
    if (self->overlay_text)
      display_perf_stats_text (self);

    sprintf (fps_buffer,"FPS: %u",self->fps);
    fillRegion (self->image_handler,
                self->fps_x_pos,
                self->fps_y_pos,
                self->fps_width,
                self->fps_height,
                self->fps_bg_color);
    drawText (self->image_handler,
              fps_buffer,
              self->fps_x_pos+self->big_font_property->width,
              self->fps_y_pos,
              self->big_font_property,
              self->fps_text_color);
  }
  gst_buffer_unmap (buffer, &buffer_mapinfo);
  ret = GST_FLOW_OK;
exit:
  return ret;

}

static void
display_perf_stats_text (GstTIPerfOverlay * self)
{
  gchar buffer[100];
  guint i;
  guint cpu_id;
  app_perf_stats_cpu_load_t cpu_load;

  guint vertical_padding = 0;
  guint horizontal_padding = 5;

  guint x_pos = horizontal_padding;
  guint y_pos = self->main_title_font_property->height + vertical_padding + 5;

  if (self->title) {
    y_pos += self->title_font_property->height;
  }

  for (cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    if (appIpcIsCpuEnabled (cpu_id)) {
      cpu_load = self->cpu_loads[cpu_id];
      sprintf (buffer,
          "CPU: %6s: TOTAL LOAD = %3d.%2d%%",
          appIpcGetCpuName (cpu_id),
          cpu_load.cpu_load / 100u, cpu_load.cpu_load % 100u);

      drawText (self->image_handler, buffer, x_pos, y_pos,
          self->big_font_property, self->text_color);
      y_pos += self->big_font_property->height + vertical_padding;
    }
  }

  for (i = 0; i < sizeof (self->hwa_loads) / sizeof (self->hwa_loads[0]); i++) {
    guint hwa_id;
    app_perf_stats_hwa_load_t *
        hwaLoad;
    uint64_t load;
    for (hwa_id = 0; hwa_id < APP_PERF_HWA_MAX; hwa_id++) {
      app_perf_hwa_id_t id = (app_perf_hwa_id_t) hwa_id;
      hwaLoad = &self->hwa_loads[i].hwa_stats[id];

      if (hwaLoad->active_time > 0 && hwaLoad->pixels_processed > 0
          && hwaLoad->total_time > 0) {
        load = (hwaLoad->active_time * 10000) / hwaLoad->total_time;
        sprintf (buffer,
            "HWA: %6s: LOAD = %3lu.%2lu %% ( %lu MP/s )\n",
            appPerfStatsGetHwaName (id),
            load / 100,
            load % 100, (hwaLoad->pixels_processed / hwaLoad->total_time)
            );
        drawText (self->image_handler, buffer, x_pos, y_pos,
            self->big_font_property, self->text_color);
        y_pos += self->big_font_property->height + vertical_padding;
      }
    }
  }
  sprintf (buffer,
      "DDR: READ  BW: AVG = %6d MB/s, PEAK = %6d MB/s\n",
      self->ddr_load.read_bw_avg, self->ddr_load.read_bw_peak);
  drawText (self->image_handler, buffer, x_pos, y_pos,
      self->big_font_property, self->text_color);
  y_pos += self->big_font_property->height + vertical_padding;
  sprintf (buffer,
      "DDR: WRITE BW: AVG = %6d MB/s, PEAK = %6d MB/s\n",
      self->ddr_load.write_bw_avg, self->ddr_load.write_bw_peak);
  drawText (self->image_handler, buffer, x_pos, y_pos,
      self->big_font_property, self->text_color);
  y_pos += self->big_font_property->height + vertical_padding;
  sprintf (buffer,
      "DDR: TOTAL BW: AVG = %6d MB/s, PEAK = %6d MB/s\n",
      self->ddr_load.read_bw_avg + self->ddr_load.write_bw_avg,
      self->ddr_load.write_bw_peak + self->ddr_load.read_bw_peak);
  drawText (self->image_handler, buffer, x_pos, y_pos,
      self->big_font_property, self->text_color);
  y_pos += self->big_font_property->height + vertical_padding;

}

static void
display_perf_stats_graphics (GstTIPerfOverlay * self)
{
  fillRegion (self->image_handler,
      self->overlay_pos_x,
      self->overlay_pos_y,
      self->overlay_width, self->overlay_height, self->overlay_color);

  guint cpu_id;
  guint i;
  app_perf_stats_cpu_load_t cpu_load;
  guint graph_x = self->graph_pos_x;
  guint count = 0;
  gint value;
  for (cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    if (appIpcIsCpuEnabled (cpu_id)) {
      cpu_load = self->cpu_loads[cpu_id];
      //sprintf(buffer,"CPU: %s",appIpcGetCpuName(cpu_id));
      self->bar_graphs[count]->initGraph (self->image_handler,
          graph_x,
          self->graph_pos_y,
          self->graph_width,
          self->graph_height,
          100,
          appIpcGetCpuName (cpu_id),
          "%",
          self->small_font_property,
          self->big_font_property,
          self->overlay_text_color, self->color_green, self->graph_bg_color);
      value = cpu_load.cpu_load / 100u;
      if (value > 33 && value <= 66) {
        self->bar_graphs[count]->m_fillColor = self->color_yellow;
      } else if (value > 66) {
        self->bar_graphs[count]->m_fillColor = self->color_red;
      }
      self->bar_graphs[count]->update (value);
      graph_x += self->graph_offset_x + self->graph_width;
      count++;
    }
  }

  for (i = 0; i < sizeof (self->hwa_loads) / sizeof (self->hwa_loads[0]); i++) {
    guint hwa_id;
    app_perf_stats_hwa_load_t *
        hwaLoad;
    uint64_t load;
    for (hwa_id = 0; hwa_id < APP_PERF_HWA_MAX; hwa_id++) {
      app_perf_hwa_id_t id = (app_perf_hwa_id_t) hwa_id;
      hwaLoad = &self->hwa_loads[i].hwa_stats[id];
      if (hwaLoad->active_time > 0 && hwaLoad->pixels_processed > 0
          && hwaLoad->total_time > 0) {
        self->bar_graphs[count]->initGraph (self->image_handler, graph_x,
            self->graph_pos_y, self->graph_width, self->graph_height, 100,
            appPerfStatsGetHwaName (id), "%", self->small_font_property,
            self->big_font_property, self->overlay_text_color,
            self->color_green, self->graph_bg_color);
        load = (hwaLoad->active_time * 10000) / hwaLoad->total_time;
        value = load / 100;
        if (value > 33 && value <= 66) {
          self->bar_graphs[count]->m_fillColor = self->color_yellow;
        } else if (value > 66) {
          self->bar_graphs[count]->m_fillColor = self->color_red;
        }
        self->bar_graphs[count]->update (value);
        graph_x += self->graph_offset_x + self->graph_width;
        count++;
      }
    }
  }
  self->bar_graphs[count]->initGraph (self->image_handler,
      graph_x,
      self->graph_pos_y,
      self->graph_width,
      self->graph_height,
      self->ddr_load.total_available_bw,
      "DDR Read BW",
      "MB/s",
      self->small_font_property,
      self->big_font_property,
      self->overlay_text_color, self->color_green, self->graph_bg_color);
  self->bar_graphs[count]->update (self->ddr_load.read_bw_avg);
  graph_x += self->graph_offset_x + self->graph_width;
  count++;

  self->bar_graphs[count]->initGraph (self->image_handler,
      graph_x,
      self->graph_pos_y,
      self->graph_width,
      self->graph_height,
      self->ddr_load.total_available_bw,
      "DDR Write BW",
      "MB/s",
      self->small_font_property,
      self->big_font_property,
      self->overlay_text_color, self->color_green, self->graph_bg_color);
  self->bar_graphs[count]->update (self->ddr_load.write_bw_avg);
  graph_x += self->graph_offset_x + self->graph_width;
  count++;
}


static
    guint
update_perf_stats (GstTIPerfOverlay * self)
{
  //CPU
  guint count = 0;
  guint cpu_id;
  for (cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    if (appIpcIsCpuEnabled (cpu_id)) {
      appPerfStatsCpuLoadGet (cpu_id, &self->cpu_loads[cpu_id]);
      count++;
    }
  }

  // HWA
  guint hwa_count = 0;
#if defined(SOC_AM62A)
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MCU1_0, &self->hwa_loads[hwa_count++]);
#else
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MCU2_0, &self->hwa_loads[hwa_count++]);
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MCU2_1, &self->hwa_loads[hwa_count++]);
#if defined(SOC_J784S4)
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MCU4_0, &self->hwa_loads[hwa_count++]);
#endif
#endif
  appPerfStatsHwaStatsGet (APP_IPC_CPU_MPU1_0, &self->hwa_loads[hwa_count++]);
  count += hwa_count - 1;

  // DDR
  appPerfStatsDdrStatsGet (&self->ddr_load);
  count += 2;

  // Reset
  for (cpu_id = 0; cpu_id < APP_IPC_CPU_MAX; cpu_id++) {
    if (appIpcIsCpuEnabled (cpu_id)) {
      appPerfStatsCpuLoadReset (cpu_id);
    }
  }

#if defined(SOC_AM62A)
  appRemoteServiceRun (APP_IPC_CPU_MCU1_0, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
#else
  appRemoteServiceRun (APP_IPC_CPU_MCU2_0, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
  appRemoteServiceRun (APP_IPC_CPU_MCU2_1, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
#if defined(SOC_J784S4)
  appRemoteServiceRun (APP_IPC_CPU_MCU4_0, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);
#endif
#endif
  appRemoteServiceRun (APP_IPC_CPU_MPU1_0, APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_HWA_LOAD_CALC, NULL, 0, 0);

  appRemoteServiceRun (APP_PERF_STATS_GET_DDR_STATS_CORE,
      APP_PERF_STATS_SERVICE_NAME,
      APP_PERF_STATS_CMD_RESET_DDR_STATS, NULL, 0, 0);
  return count;
}
