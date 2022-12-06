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

extern "C"
{

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <semaphore.h>
#include "gsttidlpostproc.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxcontext.h"
#include "gst-libs/gst/tiovx/gsttidloutmeta.h"

}

#include <ti_dl_inferer.h>
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
};

/* Formats definition */
#define TI_DL_POST_PROC_SUPPORTED_FORMATS_SRC "{NV12}"
#define TI_DL_POST_PROC_SUPPORTED_FORMATS_SINK "{NV12}"
#define TI_DL_POST_PROC_SUPPORTED_WIDTH "[1 , 8192]"
#define TI_DL_POST_PROC_SUPPORTED_HEIGHT "[1 , 8192]"
#define TI_DL_POST_PROC_SUPPORTED_DATA_TYPES "[2, 10]"

/* Src caps */
#define TI_DL_POST_PROC_STATIC_CAPS_IMAGE                         \
  "video/x-raw, "                                                 \
  "format = (string) " TI_DL_POST_PROC_SUPPORTED_FORMATS_SRC ", " \
  "width = " TI_DL_POST_PROC_SUPPORTED_WIDTH ", "                 \
  "height = " TI_DL_POST_PROC_SUPPORTED_HEIGHT

/* Sink caps */
#define TI_DL_POST_PROC_STATIC_CAPS_TENSOR_SINK                  \
  "application/x-tensor-tiovx "

/* Pads definitions */
static
    GstStaticPadTemplate
    src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_DL_POST_PROC_STATIC_CAPS_IMAGE)
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
  GstTIOVXPad *
      image_pad;
  GstTIOVXPad *
      tensor_pad;
  GstTIOVXPad *
      src_pad;
  gchar *
      model;
  GstBuffer *
      tensor_buf;
  GstBuffer *
      image_buf;
  vx_context
      context;
  vx_reference
      tensor_ref;
  vx_reference
      input_ref;
  vx_reference
      output_ref;
  GstTIOVXContext *
      tiovx_context;
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
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_dl_post_proc_debug);
#define GST_CAT_DEFAULT gst_ti_dl_post_proc_debug

#define gst_ti_dl_post_proc_parent_class parent_class
G_DEFINE_TYPE (GstTIDLPostProc, gst_ti_dl_post_proc, GST_TYPE_ELEMENT);

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

static
    GstFlowReturn
gst_ti_dl_post_proc_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static
    gboolean
gst_ti_dl_post_proc_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static
    GstStateChangeReturn
gst_ti_dl_post_proc_change_state
    (GstElement * element, GstStateChange transition);

static void
gst_ti_dl_make_post_proc (GstTIDLPostProc * self, GstBuffer * buffer);

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

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype
      (&tensor_sink_template, GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype
      (&image_sink_template, GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_change_state);
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
  self->tensor_buf = NULL;
  self->image_buf = NULL;
  self->context = NULL;
  self->tensor_ref = NULL;
  self->input_ref = NULL;
  self->output_ref = NULL;
  self->post_proc_config = NULL;
  self->top_n = TOP_N_DEFAULT;
  self->alpha = ALPHA_DEFAULT;
  self->viz_threshold = VIZ_THRESHOLD_DEFAULT;
  self->post_proc_obj = NULL;
  sem_init (&self->sem_tensor, 0, 0);
  sem_init (&self->sem_image, 0, 0);
  self->mutex = {
  0};
  self->stop = FALSE;

  pad_template = gst_element_class_get_pad_template (gstelement_class, "sink");
  self->image_pad =
      GST_TIOVX_PAD (gst_pad_new_from_template (pad_template, "sink"));
  if (!GST_TIOVX_IS_PAD (self->image_pad)) {
    GST_ERROR_OBJECT (self, "Requested pad from template isn't a TIOVX pad");
    return;
  }

  pad_template = gst_element_class_get_pad_template (gstelement_class, "src");
  self->src_pad =
      GST_TIOVX_PAD (gst_pad_new_from_template (pad_template, "src"));
  if (!GST_TIOVX_IS_PAD (self->src_pad)) {
    GST_ERROR_OBJECT (self, "Requested pad from template isn't a TIOVX pad");
    return;
  }

  pad_template =
      gst_element_class_get_pad_template (gstelement_class, "tensor");
  self->tensor_pad =
      GST_TIOVX_PAD (gst_pad_new_from_template (pad_template, "tensor"));
  if (!GST_TIOVX_IS_PAD (self->tensor_pad)) {
    GST_ERROR_OBJECT (self, "Requested pad from template isn't a TIOVX pad");
    return;
  }

  gst_pad_set_event_function (GST_PAD (self->image_pad),
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_sink_event));
  gst_pad_set_event_function (GST_PAD (self->tensor_pad),
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_sink_event));

  gst_pad_set_chain_function (GST_PAD (self->image_pad),
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_chain));
  gst_pad_set_chain_function (GST_PAD (self->tensor_pad),
      GST_DEBUG_FUNCPTR (gst_ti_dl_post_proc_chain));

  gst_element_add_pad (GST_ELEMENT (self), GST_PAD (self->src_pad));
  gst_pad_set_active (GST_PAD (self->src_pad), FALSE);
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD (self->tensor_pad));
  gst_pad_set_active (GST_PAD (self->tensor_pad), FALSE);
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD (self->image_pad));
  gst_pad_set_active (GST_PAD (self->image_pad), FALSE);

  self->tiovx_context = gst_tiovx_context_new ();
  if (NULL == self->tiovx_context) {
    GST_ERROR_OBJECT (self, "Failed to do common initialization");
  }

  self->context = vxCreateContext ();
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

  if (self->post_proc_obj) {
    delete self->post_proc_obj;
  }

  if (self->post_proc_config) {
    delete self->post_proc_config;
  }

  if (self->input_ref) {
    vxReleaseReference (&(self->input_ref));
  }

  if (self->output_ref) {
    vxReleaseReference (&(self->output_ref));
  }

  if (self->tensor_ref) {
    vxReleaseReference (&(self->tensor_ref));
  }

  if (self->context) {
    vxReleaseContext (&self->context);
  }

  if (self->tiovx_context) {
    g_object_unref (self->tiovx_context);
    self->tiovx_context = NULL;
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

      gst_event_parse_caps (event, &sink_caps);

      if (0 == g_strcmp0 (pad->padtemplate->name_template,
              image_sink_template.name_template)) {
        gst_pad_push_event (GST_PAD (self->src_pad),
            gst_event_new_caps (sink_caps));

        peer_caps = gst_pad_get_current_caps (GST_PAD (self->src_pad));
        if (self->output_ref == NULL) {
          self->output_ref = gst_tiovx_get_exemplar_from_caps ((GObject *) self,
              GST_CAT_DEFAULT, self->context, peer_caps);
          gst_tiovx_pad_set_exemplar (self->src_pad, self->output_ref);
        }

        if (self->output_ref == NULL) {
          self->input_ref = gst_tiovx_get_exemplar_from_caps ((GObject *) self,
              GST_CAT_DEFAULT, self->context, sink_caps);
          gst_tiovx_pad_set_exemplar (self->image_pad, self->input_ref);
        }

        gst_tiovx_pad_peer_query_allocation (self->src_pad, peer_caps);

        if (!gst_video_info_from_caps (&video_info, peer_caps)) {
          GST_ERROR_OBJECT (self, "failed to get caps from image sink pad");
          return ret;
        }

        self->image_width = GST_VIDEO_INFO_WIDTH (&video_info);
        self->image_height = GST_VIDEO_INFO_HEIGHT (&video_info);

        gst_caps_unref (peer_caps);
      } else if (0 == g_strcmp0 (pad->padtemplate->name_template,
              tensor_sink_template.name_template)) {
        if (self->tensor_ref == NULL) {
          self->tensor_ref = gst_tiovx_get_exemplar_from_caps ((GObject *) self,
              GST_CAT_DEFAULT, self->context, sink_caps);
          gst_tiovx_pad_set_exemplar (self->tensor_pad, self->tensor_ref);
        }
      } else {
        GST_ERROR_OBJECT (self, "Unknown Pad");
        return ret;
      }

      gst_event_unref (event);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_event_default (GST_PAD (pad), parent, event);
      break;
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
  GstMapInfo image_mapinfo, tensor_mapinfo;

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
    if (self->post_proc_obj == NULL)
      gst_ti_dl_make_post_proc (self, buffer);
    self->tensor_buf = buffer;
    sem_post (&self->sem_tensor);
    sem_wait (&self->sem_image);
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
    goto exit;
  }

  if (!gst_buffer_map (self->image_buf, &image_mapinfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (self, "failed to map image buffer");
    goto exit;
  }

  if (!gst_buffer_map (self->tensor_buf, &tensor_mapinfo, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "failed to map tensor buffer");
    goto exit;
  }

  for (guint i = 0; i < self->meta.num_outputs; i++) {
    self->dl_tensor[i]->data = tensor_mapinfo.data + self->meta.offsets[i];
  }

  (*(self->post_proc_obj)) (image_mapinfo.data, self->dl_tensor);

  gst_buffer_unmap (self->tensor_buf, &tensor_mapinfo);
  gst_buffer_unmap (self->image_buf, &image_mapinfo);

  ret = gst_pad_push (GST_PAD (self->src_pad), self->image_buf);

  gst_buffer_unref (self->tensor_buf);
  self->tensor_buf = NULL;
  self->image_buf = NULL;

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

  self->post_proc_config->outDataWidth = self->image_width;
  self->post_proc_config->outDataHeight = self->image_height;

  self->post_proc_obj =
      PostprocessImage::makePostprocessImageObj (*(self->post_proc_config));
  if (self->post_proc_obj == NULL) {
    GST_ERROR_OBJECT (self, "Making Post Proc Object failed");
  }

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
