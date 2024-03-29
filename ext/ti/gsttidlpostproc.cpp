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
 * *	Any redistribution and use are licensed by TI for use only with TI
 * Devices.
 *
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are
 * met:
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

#include <glib.h>

extern "C"
{

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <semaphore.h>
#include <gst/video/video.h>

#include "gsttidlpostproc.h"

#include "gst-libs/gst/ti/gsttidloutmeta.h"

}

#include <yaml-cpp/yaml.h>

#include <ti_post_process_config.h>
#include <ti_post_process.h>

#define GST_TI_DL_POST_PROC_GET_CLASS(obj)  \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TI_DL_POST_PROC, \
                              GstTIDLPostProcClass))

#define TOP_N_MIN 1
#define TOP_N_MAX 10
#define TOP_N_DEFAULT 5

#define ALPHA_MIN 0
#define ALPHA_MAX 1
#define ALPHA_DEFAULT 0.5

#define VIZ_THRESHOLD_MIN 0.0
#define VIZ_THRESHOLD_MAX 1
#define VIZ_THRESHOLD_DEFAULT 0.6

using namespace
    ti::dl_inferer;

using namespace
    ti::post_process;

/* Properties definition */
enum
{
  PROP_0,
  PROP_MODEL,
  PROP_TOPN,
  PROP_ALPHA,
  PROP_VIZ_THRESHOLD,
  PROP_DISPLAY_MODEL,
};

/* Formats definition */
#define TI_DL_POST_PROC_SUPPORTED_FORMATS_SRC "{NV12}"
#define TI_DL_POST_PROC_SUPPORTED_FORMATS_SINK "{NV12}"
#define TI_DL_POST_PROC_SUPPORTED_WIDTH "[1 , 8192]"
#define TI_DL_POST_PROC_SUPPORTED_HEIGHT "[1 , 8192]"
#define TI_DL_POST_PROC_SUPPORTED_DATA_TYPES "[2, 10]"

/* Image src/sink caps */
#define TI_DL_POST_PROC_STATIC_CAPS_IMAGE                         \
  "video/x-raw, "                                                 \
  "format = (string) " TI_DL_POST_PROC_SUPPORTED_FORMATS_SRC ", " \
  "width = " TI_DL_POST_PROC_SUPPORTED_WIDTH ", "                 \
  "height = " TI_DL_POST_PROC_SUPPORTED_HEIGHT

/* Tensor sink caps */
#define TI_DL_POST_PROC_STATIC_CAPS_TENSOR_SINK                  \
  "application/x-tensor-tiovx "

/* Text src caps */
#define TI_DL_POST_PROC_STATIC_CAPS_TEXT_SRC                      \
  "text/x-raw, "                                                  \
  "format = (string) utf8"

/* Pads definitions */
static
    GstStaticPadTemplate
    image_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TI_DL_POST_PROC_STATIC_CAPS_IMAGE)
    );

static
    GstStaticPadTemplate
    text_src_template = GST_STATIC_PAD_TEMPLATE ("text",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TI_DL_POST_PROC_STATIC_CAPS_TEXT_SRC)
    );

static
    GstStaticPadTemplate
    tensor_sink_template = GST_STATIC_PAD_TEMPLATE ("tensor",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_DL_POST_PROC_STATIC_CAPS_TENSOR_SINK)
    );

static
    GstStaticPadTemplate
    image_sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_DL_POST_PROC_STATIC_CAPS_IMAGE)
    );

struct _GstTIDLPostProc
{
  GstElement
      element;
  GstPad *
      image_pad;
  GstPad *
      tensor_pad;
  GstPad *
      src_pad;
  GstPad *
      text_pad;
  gchar *
      model;
  GstBuffer *
      tensor_buf;
  GstBuffer *
      image_buf;
  GstBuffer *
      text_buf;
  PostprocessImageConfig *
      post_proc_config;
  guint
      top_n;
  gfloat
      alpha;
  gfloat
      viz_threshold;
  PostprocessImage *
      post_proc_obj;
  PostProcessResult *
      post_proc_result;
  guint
      image_height;
  guint
      image_width;
  sem_t
      sem_tensor;
  sem_t
      sem_image;
  GMutex
      mutex;
  gboolean
      stop;
  VecDlTensorPtr
      dl_tensor;
  GstTiDlOutMeta
      meta;
  gboolean
    display_model;
  Image *
    image_handler;
  guint
    uv_offset;
  YUVColor *
    model_bg_color;
  YUVColor *
    model_fg_color;
  FontProperty *
    model_font_property;
  guint64
    frame_count;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_dl_post_proc_debug);
#define GST_CAT_DEFAULT gst_ti_dl_post_proc_debug

static void
gst_ti_dl_post_proc_child_proxy_init (gpointer g_iface, gpointer iface_data);

#define gst_ti_dl_post_proc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIDLPostProc, gst_ti_dl_post_proc,
    GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_ti_dl_post_proc_child_proxy_init););

static void
gst_ti_dl_post_proc_finalize (GObject * obj);

static void
gst_ti_dl_post_proc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

static void
gst_ti_dl_post_proc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void
gst_ti_dl_post_proc_parse_model (GstTIDLPostProc * self);

static void
gst_ti_dl_post_proc_update_meta_info (GstTIDLPostProc * self,
  GstTiDlOutMeta * meta);

static
    GstFlowReturn
gst_ti_dl_post_proc_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static gboolean
gst_ti_dl_post_proc_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static gboolean
gst_ti_dl_post_proc_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static
    gboolean
gst_ti_dl_post_proc_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static
    GstStateChangeReturn
gst_ti_dl_post_proc_change_state
(GstElement * element, GstStateChange transition);

static
  GstPad *
gst_ti_dl_post_proc_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * name_templ, const GstCaps * caps);

static void
gst_ti_dl_post_proc_release_pad (GstElement * element, GstPad * pad);

static void
gst_ti_dl_make_post_proc (GstTIDLPostProc * self, GstBuffer * buffer);

static void
gst_ti_dl_post_proc_get_yaml (GstTIDLPostProc * self, YAML::Emitter * emitter);

/* Initialize the plugin's class */
static void
gst_ti_dl_post_proc_class_init (GstTIDLPostProcClass * klass)
{
  GObjectClass *
      gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *
      gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *
      pad_template = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TI DL PostProc",
      "Filter/Converter/Video",
      "Applies post processing on input image by parsing the input tensor",
      "Rahul T R <r-ravikumar@ti.com>");

  gobject_class->set_property = gst_ti_dl_post_proc_set_property;
  gobject_class->get_property = gst_ti_dl_post_proc_get_property;

  g_object_class_install_property (gobject_class, PROP_MODEL,
      g_param_spec_string ("model", "Model Directory",
          "TIDL Model directory with params, model and artifacts",
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property (gobject_class, PROP_TOPN,
      g_param_spec_int ("top-N", "top N",
          "Number of class to overlay for classification results",
          TOP_N_MIN, TOP_N_MAX, TOP_N_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_ALPHA,
      g_param_spec_float ("alpha", "Alpha",
          "Alpha value for sematic segmentation blending",
          ALPHA_MIN, ALPHA_MAX, ALPHA_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_VIZ_THRESHOLD,
      g_param_spec_float ("viz-threshold", "Vizualization threashold",
          "Confidence Threashold for drawing bounding boxes for object "
          "detection and human pose estimation results",
          VIZ_THRESHOLD_MIN, VIZ_THRESHOLD_MAX, VIZ_THRESHOLD_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DISPLAY_MODEL,
        g_param_spec_boolean ("display-model", "Display Model",
          "Display model name on top of the frame",
          FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype
      (&image_src_template, GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype
      (&text_src_template, GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype
      (&tensor_sink_template, GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype
      (&image_sink_template, GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_change_state);
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_release_pad);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_dl_post_proc_debug,
      "tidlpostproc", 0, "debug category for the tidlpostproc element");
}

/* Initialize the new element */
static void
gst_ti_dl_post_proc_init (GstTIDLPostProc * self)
{
  GstTIDLPostProcClass *
      klass = GST_TI_DL_POST_PROC_GET_CLASS (self);
  GstElementClass *
      gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *
      pad_template = NULL;

  GST_LOG_OBJECT (self, "init");

  self->model = NULL;
  self->image_pad = NULL;
  self->tensor_pad = NULL;
  self->src_pad = NULL;
  self->text_pad = NULL;
  self->tensor_buf = NULL;
  self->image_buf = NULL;
  self->text_buf = NULL;
  self->frame_count = 0;
  self->post_proc_config = NULL;
  self->top_n = TOP_N_DEFAULT;
  self->alpha = ALPHA_DEFAULT;
  self->viz_threshold = VIZ_THRESHOLD_DEFAULT;
  self->post_proc_obj = NULL;
  self->post_proc_result = NULL;
  sem_init (&self->sem_tensor, 0, 0);
  sem_init (&self->sem_image, 0, 0);
  self->mutex = {
  0};
  self->stop = FALSE;
  self->display_model = FALSE;
  self->image_handler = new Image;
  self->uv_offset = 0;
  self->model_font_property = new FontProperty;
  self->model_bg_color = new YUVColor;
  self->model_fg_color = new YUVColor;
  getColor (self->model_bg_color, 0, 0, 0);
  getColor (self->model_fg_color, 255, 255, 255);

  pad_template = gst_element_class_get_pad_template (gstelement_class, "sink");
  self->image_pad = gst_pad_new_from_template (pad_template, "sink");

  pad_template =
      gst_element_class_get_pad_template (gstelement_class, "tensor");
  self->tensor_pad = gst_pad_new_from_template (pad_template, "tensor");

  gst_pad_set_event_function (self->image_pad,
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_sink_event));

  gst_pad_set_chain_function (self->image_pad,
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_chain));
  gst_pad_set_chain_function (self->tensor_pad,
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_chain));

  gst_pad_set_query_function (self->image_pad,
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_sink_query));

  gst_element_add_pad (GST_ELEMENT (self), self->tensor_pad);
  gst_pad_set_active (self->tensor_pad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), self->image_pad);
  gst_pad_set_active (self->image_pad, TRUE);
}

static void
gst_ti_dl_post_proc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_MODEL:
      self->model = g_value_dup_string (value);
      gst_ti_dl_post_proc_parse_model (self);
      break;
    case PROP_TOPN:
      self->top_n = g_value_get_int (value);
      if (self->post_proc_config)
        self->post_proc_config->topN = self->top_n;
      break;
    case PROP_ALPHA:
      self->alpha = g_value_get_float (value);
      if (self->post_proc_config)
        self->post_proc_config->alpha = self->alpha;
      break;
    case PROP_VIZ_THRESHOLD:
      self->viz_threshold = g_value_get_float (value);
      if (self->post_proc_config)
        self->post_proc_config->vizThreshold = self->viz_threshold;
      break;
    case PROP_DISPLAY_MODEL:
      self->display_model = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_ti_dl_post_proc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_MODEL:
      g_value_set_string (value, self->model);
      break;
    case PROP_TOPN:
      g_value_set_int (value, self->top_n);
      break;
    case PROP_ALPHA:
      g_value_set_float (value, self->alpha);
      break;
    case PROP_VIZ_THRESHOLD:
      g_value_set_float (value, self->viz_threshold);
      break;
    case PROP_DISPLAY_MODEL:
      g_value_set_boolean(value,self->display_model);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_ti_dl_post_proc_finalize (GObject * obj)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (obj);

  GST_LOG_OBJECT (self, "finalize");

  if (self->post_proc_result) {
    delete self->post_proc_result;
  }

  if (self->post_proc_obj) {
    delete self->post_proc_obj;
  }

  if (self->post_proc_config) {
    delete self->post_proc_config;
  }

  G_OBJECT_CLASS (gst_ti_dl_post_proc_parent_class)->finalize (obj);
}

static void
gst_ti_dl_post_proc_parse_model (GstTIDLPostProc * self)
{
  guint status = -1;

  GST_LOG_OBJECT (self, "parse_model");

  if (self->post_proc_config == NULL)
    self->post_proc_config = new PostprocessImageConfig;

  status = self->post_proc_config->getConfig (self->model);
  if (status < 0) {
    GST_ERROR_OBJECT (self, "Getting Post Proc config failed");
  }

  self->post_proc_config->topN = self->top_n;
  self->post_proc_config->alpha = self->alpha;
  self->post_proc_config->vizThreshold = self->viz_threshold;
}

static void
gst_ti_dl_post_proc_update_meta_info (GstTIDLPostProc * self,
  GstTiDlOutMeta * meta)
{
  GST_DEBUG_OBJECT (self, "update_meta_info");

  /* Delete previous dl_tensor*/
  for (guint i = 0; i < self->dl_tensor.size(); i++) {
    delete self->dl_tensor[i];
  }
  self->dl_tensor.clear();

  for (guint i = 0; i < meta->num_outputs; i++) {
    self->dl_tensor.push_back (new DlTensor);
    self->dl_tensor[i]->type = (DlInferType) (meta->types[i]);
    self->dl_tensor[i]->numElem = meta->widths[i] * meta->height;
    self->dl_tensor[i]->size = getTypeSize ((DlInferType) (meta->types[i])) *
        self->dl_tensor[i]->numElem;
    self->dl_tensor[i]->dim = 2;
    self->dl_tensor[i]->shape.push_back (meta->height);
    self->dl_tensor[i]->shape.push_back (meta->widths[i]);
  }
  self->meta = *meta;
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

static gboolean
gst_ti_dl_post_proc_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (parent);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "src_query");

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter = NULL;
      GstCaps *sink_caps = NULL;
      GstCaps *src_caps = NULL;

      gst_query_parse_caps (query, &filter);
      filter = intersect_with_template_caps (filter, pad);

      sink_caps = gst_pad_peer_query_caps (self->image_pad, filter);
      src_caps = intersect_with_template_caps (sink_caps, self->image_pad);

      gst_caps_unref (sink_caps);

      ret = TRUE;

      gst_query_set_caps_result (query, src_caps);
      gst_caps_unref (src_caps);
      break;
    }
    default:
    {
      ret = gst_pad_peer_query(self->image_pad, query);
      break;
    }
  }

  return ret;
}

static gboolean
gst_ti_dl_post_proc_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (parent);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "sink_query");

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter = NULL;
      GstCaps *sink_caps = NULL;
      GstCaps *src_caps = NULL;

      if (NULL == self->src_pad) {
        break;
      }

      gst_query_parse_caps (query, &filter);
      filter = intersect_with_template_caps (filter, pad);

      src_caps = gst_pad_peer_query_caps (self->src_pad, filter);
      sink_caps = intersect_with_template_caps (src_caps, self->src_pad);

      gst_caps_unref (src_caps);

      ret = TRUE;

      gst_query_set_caps_result (query, sink_caps);
      gst_caps_unref (sink_caps);
      break;
    }
    default:
    {
      if (NULL == self->src_pad) {
        break;
      }
      ret = gst_pad_peer_query(self->src_pad, query);
      break;
    }
  }

  return ret;
}

static
    gboolean
gst_ti_dl_post_proc_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (parent);
  gboolean ret = FALSE;
  GstVideoInfo video_info = {
  };

  GST_LOG_OBJECT (self, "sink_event");

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *
          sink_caps = NULL;
      GstCaps *
          peer_caps = NULL;

      if (NULL == self->src_pad) {
        ret = TRUE;
        break;
      }

      gst_event_parse_caps (event, &sink_caps);

      gst_pad_push_event (self->src_pad, gst_event_new_caps (sink_caps));

      peer_caps = gst_pad_get_current_caps (self->src_pad);

      if (!gst_video_info_from_caps (&video_info, peer_caps)) {
        GST_ERROR_OBJECT (self, "failed to get caps from image sink pad");
        return ret;
      }

      self->image_width = GST_VIDEO_INFO_WIDTH (&video_info);
      self->image_height = GST_VIDEO_INFO_HEIGHT (&video_info);

      gst_caps_unref (peer_caps);
      gst_event_unref (event);

      ret = TRUE;
      break;
    }
    default:
    {
      if (NULL == self->src_pad) {
        break;
      }
      ret = gst_pad_push_event (GST_PAD (self->src_pad), event);
      break;
    }
  }

  return ret;
}

static
    GstFlowReturn
gst_ti_dl_post_proc_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (parent);
  GstMapInfo image_mapinfo, tensor_mapinfo, text_mapinfo;
  GstClockTime pts = 0, dts = 0, duration = 0;
  YAML::Emitter result_yaml;

#ifndef ENABLE_TIDL
  GstTiDlOutMeta *meta;
#endif //NOT ENABLE_TIDL

  GST_LOG_OBJECT (self, "chain");

  if (self->stop) {
    g_mutex_lock (&self->mutex);
    gst_buffer_unref (buffer);
    goto exit;
  }

  if (0 == g_strcmp0 (pad->padtemplate->name_template,
          image_sink_template.name_template)) {
    self->image_buf = buffer;
    sem_post (&self->sem_image);
    sem_wait (&self->sem_tensor);
    g_mutex_lock (&self->mutex);
    if (self->image_buf == NULL) {
      ret = GST_FLOW_OK;
      goto exit;
    }
  } else if (0 == g_strcmp0 (pad->padtemplate->name_template,
          tensor_sink_template.name_template)) {
    if (self->post_proc_obj == NULL) {
      gst_ti_dl_make_post_proc (self, buffer);
      self->uv_offset = self->image_height * self->image_width;
      self->image_handler->width = self->image_width;
      self->image_handler->height = self->image_height;
      getFont (self->model_font_property, (int) (0.01 * self->image_width));
    }
    self->tensor_buf = buffer;
    sem_post (&self->sem_tensor);
    if (self->src_pad) {
      sem_wait (&self->sem_image);
    }
    g_mutex_lock (&self->mutex);
    if (self->tensor_buf == NULL) {
      ret = GST_FLOW_OK;
      goto exit;
    }
  } else {
    GST_ERROR_OBJECT (self, "Unknown Pad");
    goto exit;
  }

  if (self->stop) {
    if (self->tensor_buf) {
      gst_buffer_unref (self->tensor_buf);
      self->tensor_buf = NULL;
    }
    if (self->image_buf) {
      gst_buffer_unref (self->image_buf);
      self->image_buf = NULL;
    }
    if (self->text_buf) {
      gst_buffer_unref (self->text_buf);
      self->text_buf = NULL;
    }
    goto exit;
  }

  pts = GST_BUFFER_PTS (self->tensor_buf);
  dts = GST_BUFFER_DTS (self->tensor_buf);
  duration = GST_BUFFER_DURATION (self->tensor_buf);

  if (!gst_buffer_map (self->tensor_buf, &tensor_mapinfo, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "failed to map tensor buffer");
    goto exit;
  }

#ifndef ENABLE_TIDL
  /* For ARM mode, the output tensors is not fixed for some models (ex: ONR-OD).
  Hence meta information keeps changing after each run.
  */

  meta = (GstTiDlOutMeta *) gst_buffer_get_meta (buffer,GST_TYPE_TIDL_OUT_META_API);
  if (!meta) {
    GST_WARNING_OBJECT (self, "No meta received\n");
    goto skip;
  }

  /* Create new tenosr with updated info.*/
  gst_ti_dl_post_proc_update_meta_info(self,meta);

#endif // NOT ENABLE_TIDL

  for (guint i = 0; i < self->meta.num_outputs; i++) {
    self->dl_tensor[i]->data = tensor_mapinfo.data + self->meta.offsets[i];
  }

  goto skip;

skip:
  if (self->src_pad) {
    if (!gst_buffer_map (self->image_buf, &image_mapinfo, GST_MAP_READWRITE)) {
      GST_ERROR_OBJECT (self, "failed to map image buffer");
      goto exit;
    }
#ifndef ENABLE_TIDL
    if (meta) {
      (*(self->post_proc_obj)) (image_mapinfo.data,
        self->dl_tensor, self->post_proc_result);
    }
#else
    (*(self->post_proc_obj)) (image_mapinfo.data,
        self->dl_tensor, self->post_proc_result);
#endif

    if (self->display_model) {
      self->image_handler->yRowAddr = (uint8_t *) image_mapinfo.data;
      self->image_handler->uvRowAddr =
            self->image_handler->yRowAddr + self->uv_offset;

      fillRegion (self->image_handler,
                  0,
                  0,
                  self->image_handler->width,
                  self->model_font_property->height + 4,
                  self->model_bg_color);
      drawText (self->image_handler,
                self->post_proc_config->modelName.c_str(),
                5,
                2,
                self->model_font_property,
                self->model_fg_color);
    }

    gst_buffer_unmap (self->image_buf, &image_mapinfo);
    GST_BUFFER_PTS (self->image_buf) = pts;
    GST_BUFFER_DTS (self->image_buf) = dts;
    GST_BUFFER_DURATION (self->image_buf) = duration;
    ret = gst_pad_push (GST_PAD (self->src_pad), self->image_buf);
    self->image_buf = NULL;

  } else {
#ifndef ENABLE_TIDL
    if (meta) {
      (*(self->post_proc_obj)) (NULL, self->dl_tensor, self->post_proc_result);
    }
#else
    (*(self->post_proc_obj)) (NULL, self->dl_tensor, self->post_proc_result);
#endif
  }

  gst_buffer_unmap (self->tensor_buf, &tensor_mapinfo);
  gst_buffer_unref (self->tensor_buf);

  self->tensor_buf = NULL;

  if (self->text_pad) {

    self->frame_count++;

    // Get post-proc result as yaml
    gst_ti_dl_post_proc_get_yaml (self, &result_yaml);

    // Allocate buffer for text pad and fill
    self->text_buf = gst_buffer_new_allocate (NULL, (gsize) result_yaml.size(), NULL);
    if (G_UNLIKELY (!self->text_buf)) {
      GST_WARNING_OBJECT (self,
        "Failed to allocate buffer for text pad (%ld bytes)", result_yaml.size());
      goto exit;
    }

    if (!gst_buffer_map (self->text_buf, &text_mapinfo, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (self, "failed to map text buffer");
      goto exit;
    }

    memcpy (text_mapinfo.data, result_yaml.c_str(), result_yaml.size());

    gst_buffer_unmap (self->text_buf, &text_mapinfo);

    GST_BUFFER_PTS (self->text_buf) = pts;
    GST_BUFFER_DTS (self->text_buf) = dts;
    GST_BUFFER_DURATION (self->text_buf) = duration;

    ret = gst_pad_push (GST_PAD (self->text_pad), self->text_buf);

    self->text_buf = NULL;
  }

exit:
  g_mutex_unlock (&self->mutex);
  return ret;
}

static void
gst_ti_dl_make_post_proc (GstTIDLPostProc * self, GstBuffer * buffer)
{
  GstTiDlOutMeta *
      meta = (GstTiDlOutMeta *) gst_buffer_get_meta (buffer,
      GST_TYPE_TIDL_OUT_META_API);

  GST_LOG_OBJECT (self, "make_post_proc");

  if (self->post_proc_config->taskType == "segmentation") {
    self->post_proc_config->inDataWidth = meta->widths[0];
    self->post_proc_config->inDataHeight = meta->height;
  } else {
    self->post_proc_config->inDataWidth = meta->input_width;
    self->post_proc_config->inDataHeight = meta->input_height;
  }

  if (self->src_pad) {
    self->post_proc_config->outDataWidth = self->image_width;
    self->post_proc_config->outDataHeight = self->image_height;
  } else {
    self->post_proc_config->outDataWidth = self->post_proc_config->inDataWidth;
    self->post_proc_config->outDataHeight = self->post_proc_config->inDataHeight;
  }

  self->post_proc_obj =
      PostprocessImage::makePostprocessImageObj (*(self->post_proc_config));
  if (self->post_proc_obj == NULL) {
    GST_ERROR_OBJECT (self, "Making Post Proc Object failed");
  }

  gst_ti_dl_post_proc_update_meta_info(self,meta);

  return;
}

static
    GstStateChangeReturn
gst_ti_dl_post_proc_change_state (GstElement * element,
    GstStateChange transition)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (element);
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  GST_DEBUG_OBJECT (self, "change_state");

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      g_mutex_lock (&self->mutex);
      self->stop = TRUE;
      sem_post (&self->sem_tensor);
      sem_post (&self->sem_image);
      g_mutex_unlock (&self->mutex);
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      sem_init (&self->sem_tensor, 0, 0);
      sem_init (&self->sem_image, 0, 0);
      self->stop = FALSE;
      break;
    default:
      break;
  }

  return result;
}

static
  GstPad *
gst_ti_dl_post_proc_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name_templ, const GstCaps * caps)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (element);
  GstPad *pad = NULL;
  gchar *name = NULL;

  GST_DEBUG_OBJECT (self, "requesting pad");

  g_return_val_if_fail (templ, NULL);

  GST_OBJECT_LOCK (self);

  if (NULL == name_templ) {
    name_templ = "src";
  }

  name = g_strdup (name_templ);

  pad = gst_pad_new_from_template (templ, name);

  GST_OBJECT_UNLOCK (self);

  if (NULL == pad) {
    GST_ERROR_OBJECT (self, "Failed to create text pad");
    goto exit;
  }

  gst_element_add_pad (GST_ELEMENT_CAST (self), pad);
  gst_pad_set_active (pad, TRUE);

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_OBJECT_LOCK (self);

  if (0 == strcmp(name,"src")) {
    self->src_pad = GST_PAD (gst_object_ref (pad));
    gst_pad_set_query_function (self->src_pad,
        GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_src_query));
  } else if (0 == strcmp(name,"text")) {
      self->text_pad = GST_PAD (gst_object_ref (pad));
      self->post_proc_result = new PostProcessResult;
  }

  GST_OBJECT_UNLOCK (self);

exit:
  return pad;
}

static void
gst_ti_dl_post_proc_release_pad (GstElement * element, GstPad * pad)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (element);

  GST_DEBUG_OBJECT (self, "release request pad");

  GST_OBJECT_LOCK (self);

  gst_object_unref (pad);

  GST_OBJECT_UNLOCK (self);

  gst_child_proxy_child_removed (GST_CHILD_PROXY (self), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT_CAST (self), pad);
}

/* GstChildProxy implementation */
static GObject *
gst_ti_dl_post_proc_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  return NULL;
}

static GObject *
gst_ti_dl_post_proc_child_proxy_get_child_by_name (GstChildProxy *
    child_proxy, const gchar * name)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (child_proxy);
  GObject *
      obj = NULL;

  GST_OBJECT_LOCK (self);

  if (0 == strcmp (name, "src")) {
    obj = G_OBJECT (self->src_pad);
  } else if (0 == strcmp (name, "sink")) {
    obj = G_OBJECT (self->image_pad);
  } else if (0 == strcmp (name, "tensor")) {
    obj = G_OBJECT (self->tensor_pad);
  } else if (0 == strcmp (name, "text")) {
    obj = G_OBJECT (self->text_pad);
  }

  if (obj) {
    gst_object_ref (obj);
  }

  GST_OBJECT_UNLOCK (self);

  return obj;
}

static
    guint
gst_ti_dl_post_proc_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  GstTIDLPostProc *
      self = GST_TI_DL_POST_PROC (child_proxy);
  guint count = 0;

  GST_OBJECT_LOCK (self);

  count =  2;
  if (self->src_pad) {
    count ++;
  }

  if (self->text_pad) {
    count++;
  }

  GST_OBJECT_UNLOCK (self);

  GST_INFO_OBJECT (self, "Children Count: %d", count);

  return count;
}

static void
gst_ti_dl_post_proc_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *
      iface = (GstChildProxyInterface *) g_iface;

  iface->get_child_by_index =
      gst_ti_dl_post_proc_child_proxy_get_child_by_index;
  iface->get_child_by_name = gst_ti_dl_post_proc_child_proxy_get_child_by_name;
  iface->get_children_count =
      gst_ti_dl_post_proc_child_proxy_get_children_count;
}

static void
gst_ti_dl_post_proc_get_yaml (GstTIDLPostProc * self, YAML::Emitter * emitter)
{

  if (NULL == self->post_proc_result) {
    GST_WARNING_OBJECT (self, "PostProcessResult not found, skipping yaml dump.");
    return;
  }

  emitter->SetIndent(4);

  *emitter << YAML::BeginMap;
  *emitter << YAML::Key << "frame_no" << YAML::Value << self->frame_count;
  *emitter << YAML::Key << "input_width";
  *emitter << YAML::Value << self->post_proc_result->m_inputWidth;
  *emitter << YAML::Key << "input_height";
  *emitter << YAML::Value << self->post_proc_result->m_inputHeight;
  *emitter << YAML::Key << "output_width";
  *emitter << YAML::Value << self->post_proc_result->m_outputWidth;
  *emitter << YAML::Key << "output_height";
  *emitter << YAML::Value << self->post_proc_result->m_outputHeight;

  if (self->post_proc_config->taskType == "classification") {

    *emitter << YAML::Key << "task_type" << YAML::Value << "classification";
    *emitter << YAML::Key << "label_ids";
    *emitter << self->post_proc_result->m_imgClResult.m_labelId;
    *emitter << YAML::Key << "labels";
    *emitter << self->post_proc_result->m_imgClResult.m_label;
    *emitter << YAML::EndMap;

  } else if (self->post_proc_config->taskType == "detection") {

    *emitter << YAML::Key << "task_type" << YAML::Value << "detection";
    *emitter << YAML::Key << "label_ids";
    *emitter << self->post_proc_result->m_objDetResult.m_labelId;
    *emitter << YAML::Key << "labels";
    *emitter << self->post_proc_result->m_objDetResult.m_label;
    *emitter << YAML::Key << "scores";
    *emitter << self->post_proc_result->m_objDetResult.m_score;
    *emitter << YAML::Key << "bounding_boxes";
    *emitter << self->post_proc_result->m_objDetResult.m_box;
    *emitter << YAML::EndMap;

  } else if (self->post_proc_config->taskType == "segmentation") {

    *emitter << YAML::Key << "task_type" << YAML::Value << "segmentation";
    *emitter << YAML::Key << "class_ids";
    *emitter << self->post_proc_result->m_semSegResult.m_classId;
    *emitter << YAML::EndMap;

  } else if (self->post_proc_config->taskType == "keypoint_detection") {

    *emitter << YAML::Key << "task_type" << YAML::Value << "keypoint_detection";
    *emitter << YAML::Key << "label_ids";
    *emitter << self->post_proc_result->m_keyPointDetResult.m_labelId;
    *emitter << YAML::Key << "labels";
    *emitter << self->post_proc_result->m_keyPointDetResult.m_label;
    *emitter << YAML::Key << "scores";
    *emitter << self->post_proc_result->m_keyPointDetResult.m_score;
    *emitter << YAML::Key << "bounding_boxes";
    *emitter << self->post_proc_result->m_keyPointDetResult.m_box;
    *emitter << YAML::Key << "keypoints";
    *emitter << self->post_proc_result->m_keyPointDetResult.m_keypoints;
    *emitter << YAML::EndMap;

  } else {

    *emitter << "No Data found";

  }

  *emitter << YAML::EndMap;

}

