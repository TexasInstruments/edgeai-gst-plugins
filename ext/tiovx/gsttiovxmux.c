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
#  include "config.h"
#endif

#include "gsttiovxmux.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferpoolutils.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferutils.h"
#include "gst-libs/gst/tiovx/gsttiovxcontext.h"
#include "gst-libs/gst/tiovx/gsttiovxmuxmeta.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include <gst/video/video.h>

/* TIOVX Mux pad */

/* BufferPool constants */
static const gint min_pool_size = 2;
static const gint max_pool_size = 16;
static const gint default_pool_size = min_pool_size;

enum
{
  PROP_PAD_0,
  PROP_PAD_POOL_SIZE,
};

#define GST_TYPE_TIOVX_MUX_PAD (gst_tiovx_mux_pad_get_type())
G_DECLARE_FINAL_TYPE (GstTIOVXMuxPad, gst_tiovx_mux_pad,
    GST_TIOVX, MUX_PAD, GstAggregatorPad);

static void
gst_tiovx_mux_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void
gst_tiovx_mux_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_mux_pad_finalize (GObject * object);

typedef struct _GstTIOVXMuxPad
{
  GstAggregatorPad parent;

  guint pool_size;
  vx_reference exemplar;

  GstBufferPool *buffer_pool;
} GstTIOVXMuxPadPrivate;

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_mux_pad_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXMuxPad, gst_tiovx_mux_pad,
    GST_TYPE_AGGREGATOR_PAD,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_mux_pad_debug_category,
        "tiovxmuxpad", 0, "debug category for TIOVX mux pad class"));


static void
gst_tiovx_mux_pad_class_init (GstTIOVXMuxPadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_tiovx_mux_pad_set_property;
  gobject_class->get_property = gst_tiovx_mux_pad_get_property;
  gobject_class->finalize = gst_tiovx_mux_pad_finalize;

  g_object_class_install_property (gobject_class, PROP_PAD_POOL_SIZE,
      g_param_spec_int ("pool-size", "Pool size",
          "Pool size of the internal buffer pool", min_pool_size, max_pool_size,
          default_pool_size,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_tiovx_mux_pad_init (GstTIOVXMuxPad * self)
{
  GST_DEBUG_OBJECT (self, "gst_tiovx_mux_pad_init");

  self->pool_size = default_pool_size;
  self->exemplar = NULL;
  self->buffer_pool = NULL;
}

static void
gst_tiovx_mux_pad_finalize (GObject * object)
{
  GstTIOVXMuxPad *pad = GST_TIOVX_MUX_PAD (object);

  vxReleaseReference (&pad->exemplar);

  if (pad->buffer_pool) {
    gst_object_unref (pad->buffer_pool);
    pad->buffer_pool = NULL;
  }
}

static void
gst_tiovx_mux_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMuxPad *pad = GST_TIOVX_MUX_PAD (object);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_POOL_SIZE:
      g_value_set_int (value, pad->pool_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (pad);
}

static void
gst_tiovx_mux_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMuxPad *pad = GST_TIOVX_MUX_PAD (object);

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_POOL_SIZE:
      pad->pool_size = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (pad);
}

/* TIOVX Mux */

#define TIOVX_MUX_SUPPORTED_FORMATS_SRC "{RGB, RGBx, NV12, NV21, UYVY, YUY2, I420}"
#define TIOVX_MUX_SUPPORTED_FORMATS_SINK "{RGB, RGBx, NV12, NV21, UYVY, YUY2, I420}"
#define TIOVX_MUX_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_MUX_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_MUX_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_MUX_STATIC_CAPS_SRC                            \
  "video/x-raw, "                                            \
  "format = (string) " TIOVX_MUX_SUPPORTED_FORMATS_SRC ", "  \
  "width = " TIOVX_MUX_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_MUX_SUPPORTED_HEIGHT ", "                \
  "num-channels = " TIOVX_MUX_SUPPORTED_CHANNELS

/* Sink caps */
#define TIOVX_MUX_STATIC_CAPS_SINK                           \
  "video/x-raw, "                                            \
  "format = (string) " TIOVX_MUX_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_MUX_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_MUX_SUPPORTED_HEIGHT ", "                \
  "num-channels = 1"

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_MUX_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_MUX_STATIC_CAPS_SRC)
    );

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_mux_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_mux_debug_category

typedef struct _GstTIOVXMux
{
  GstAggregator aggregator;

  GstTIOVXContext *tiovx_context;
  vx_context context;
} GstTIOVXMux;

static GstFlowReturn gst_tiovx_mux_aggregate (GstAggregator * aggregator,
    gboolean timeout);
static gboolean gst_tiovx_mux_propose_allocation (GstAggregator * self,
    GstAggregatorPad * pad, GstQuery * decide_query, GstQuery * query);
GstCaps *gst_tiovx_mux_fixate_src_caps (GstAggregator * self, GstCaps * caps);
static void gst_tiovx_mux_child_proxy_init (gpointer g_iface,
    gpointer iface_data);
static GstPad *gst_tiovx_mux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps);
static void gst_tiovx_mux_release_pad (GstElement * element, GstPad * pad);
static int gst_tiovx_mux_sink_query (GstAggregator * agg,
    GstAggregatorPad * bpad, GstQuery * query);
static int gst_tiovx_mux_src_query (GstAggregator * agg, GstQuery * query);
static GstCaps *intersect_with_template_caps (GstCaps * caps, GstPad * pad);
static GstCaps *gst_tiovx_mux_get_current_src_caps (GstTIOVXMux * self);
static GstCaps *gst_tiovx_mux_get_src_caps (GstTIOVXMux * self,
    GstCaps * filter);
static void gst_tiovx_mux_finalize (GObject * obj);

#define GST_TIOVX_MUX_DEFINE_CUSTOM_CODE \
  GST_DEBUG_CATEGORY_INIT (gst_tiovx_mux_debug_category, "tiovxmux", 0, "debug category for the tiovxmux element"); \
  G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,  gst_tiovx_mux_child_proxy_init);

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXMux, gst_tiovx_mux,
    GST_TYPE_AGGREGATOR, GST_TIOVX_MUX_DEFINE_CUSTOM_CODE);

static void
gst_tiovx_mux_class_init (GstTIOVXMuxClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstAggregatorClass *aggregator_class = GST_AGGREGATOR_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Mux",
      "Filter",
      "Compounds multiple streams into a single one",
      "RidgeRun support@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sink_template, GST_TYPE_TIOVX_MUX_PAD);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_mux_finalize);

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_mux_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (gst_tiovx_mux_release_pad);

  aggregator_class->aggregate = GST_DEBUG_FUNCPTR (gst_tiovx_mux_aggregate);
  aggregator_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_tiovx_mux_propose_allocation);
  aggregator_class->fixate_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_mux_fixate_src_caps);
  aggregator_class->sink_query = gst_tiovx_mux_sink_query;
  aggregator_class->src_query = gst_tiovx_mux_src_query;
}

static void
gst_tiovx_mux_init (GstTIOVXMux * self)
{
  /* App common init */
  GST_DEBUG_OBJECT (self, "Running TIOVX common init");
  self->tiovx_context = gst_tiovx_context_new ();

  /* Create OpenVX Context */
  GST_DEBUG_OBJECT (self, "Creating context");
  self->context = vxCreateContext ();

  return;
}

static void
gst_tiovx_mux_finalize (GObject * obj)
{
  GstTIOVXMux *self = GST_TIOVX_MUX (obj);

  /* Release context */
  if (VX_SUCCESS == vxGetStatus ((vx_reference) self->context)) {
    vxReleaseContext (&self->context);
  }
}

static GstFlowReturn
gst_tiovx_mux_aggregate (GstAggregator * agg, gboolean timeout)
{
  GstTIOVXMux *self = GST_TIOVX_MUX (agg);
  GstBuffer *outbuf = NULL;
  GstFlowReturn ret = GST_FLOW_ERROR;
  GList *l = NULL;
  vx_object_array output_array = NULL;
  vx_reference exemplar = NULL;

  GST_DEBUG_OBJECT (self, "TIOVX Mux aggregate");

  /* Create object_array with input's vx_references */

  /* All inputs and the output should have the same type, dimensions, etc.
   * Any of the examplars should work, we are using the one from the first sinkpad
   */
  if (GST_ELEMENT (agg)->sinkpads) {
    GstAggregatorPad *agg_pad = NULL;
    GstTIOVXMuxPad *pad = NULL;

    agg_pad = GST_ELEMENT (agg)->sinkpads->data;

    pad = GST_TIOVX_MUX_PAD (agg_pad);

    exemplar = pad->exemplar;
  }

  output_array =
      vxCreateObjectArray (self->context, exemplar,
      g_list_length (GST_ELEMENT (agg)->sinkpads));

  /* Extract vx_references from inputs */
  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *agg_pad = l->data;
    GstBuffer *in_buffer = NULL;
    GstTIOVXMuxPad *pad = NULL;
    GstCaps *caps = NULL;
    vx_reference buffer_reference = NULL;
    vx_reference output_arr_reference = NULL;
    vx_object_array input_array = NULL;
    gboolean pad_is_eos = FALSE;
    gint position = 0;

    pad = GST_TIOVX_MUX_PAD (agg_pad);

    pad_is_eos = gst_aggregator_pad_is_eos (agg_pad);

    if (pad_is_eos) {
      ret = GST_FLOW_EOS;
      goto exit;
    }

    in_buffer = gst_aggregator_pad_peek_buffer (agg_pad);

    caps = gst_pad_get_current_caps (GST_PAD (agg_pad));
    in_buffer =
        gst_tiovx_validate_tiovx_buffer (GST_CAT_DEFAULT,
        &pad->buffer_pool, in_buffer, &pad->exemplar, caps, pad->pool_size);
    gst_caps_unref (caps);
    if (NULL == in_buffer) {
      GST_ERROR_OBJECT (agg_pad, "Unable to validate buffer");
      goto exit;
    }

    input_array =
        gst_tiovx_get_vx_array_from_buffer (GST_CAT_DEFAULT, &pad->exemplar,
        in_buffer);

    if (NULL == input_array) {
      GST_ERROR_OBJECT (agg_pad, "Failed getting array from buffer");
      goto exit;
    }

    /* Transfer handle for all inputs to output array 
     * We currently only support a single-channel inputs to the Mux
     */

    position = g_list_position (GST_ELEMENT (agg)->sinkpads, l);

    buffer_reference = vxGetObjectArrayItem (input_array, 0);

    output_arr_reference = vxGetObjectArrayItem (output_array, position);

    gst_tiovx_transfer_handle (GST_CAT_DEFAULT, buffer_reference,
        output_arr_reference);

    vxReleaseReference (&buffer_reference);
    vxReleaseReference (&output_arr_reference);

    gst_buffer_unref (in_buffer);
  }

  /* Create corresponding meta, and assing object_array to it */

  /* Declare the other buffer's as outbuf's parent */
  outbuf = gst_buffer_new ();

  gst_buffer_add_tiovx_mux_meta (outbuf, (vx_reference) output_array);

  for (l = GST_ELEMENT (agg)->sinkpads; l; l = g_list_next (l)) {
    GstAggregatorPad *agg_pad = l->data;
    GstBuffer *in_buffer = NULL;

    in_buffer = gst_aggregator_pad_peek_buffer (agg_pad);

    /* Make outbuf hold a reference to in_buffer until it itself is freed */
    gst_buffer_add_parent_buffer_meta (outbuf, in_buffer);

    gst_buffer_unref (in_buffer);
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

  ret = GST_FLOW_OK;

exit:
  if (output_array) {
    vxReleaseObjectArray (&output_array);
  }

  return ret;
}

static gboolean
gst_tiovx_mux_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * agg_pad, GstQuery * decide_query, GstQuery * query)
{
  GstTIOVXMux *self = GST_TIOVX_MUX (agg);
  GstTIOVXMuxPad *mux_pad = GST_TIOVX_MUX_PAD (agg_pad);
  GstBufferPool *pool = NULL;
  GstCaps *caps = NULL;
  vx_reference reference = NULL;
  gsize size = 0;
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "Propose allocation");

  gst_query_parse_allocation (query, &caps, NULL);

  /* Ignore propose allocation if caps
     are not defined in query */
  if (!caps) {
    GST_WARNING_OBJECT (self,
        "Caps empty in query, ignoring propose allocation for pad %s",
        GST_PAD_NAME (GST_PAD (agg_pad)));
    return FALSE;
  }

  if (mux_pad->exemplar) {
    reference = mux_pad->exemplar;
  } else {
    /* Image */
    if (gst_structure_has_name (gst_caps_get_structure (caps, 0),
                "video/x-raw")) {
      GstVideoInfo info;

      if (!gst_video_info_from_caps (&info, caps)) {
        GST_ERROR_OBJECT (agg, "Unable to get video info from caps");
        return FALSE;
      }

      GST_INFO_OBJECT (self,
          "creating image with width: %d\t height: %d\t format: 0x%x",
          info.width, info.height,
          gst_format_to_vx_format (info.finfo->format));

      reference = (vx_reference) vxCreateImage (self->context, info.width,
          info.height, gst_format_to_vx_format (info.finfo->format));

      mux_pad->exemplar = reference;

    }
  }

  size = gst_tiovx_get_size_from_exemplar (reference);

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, mux_pad->pool_size,
      &reference, size, &pool);

  if (mux_pad->buffer_pool) {
    gst_object_unref (mux_pad->buffer_pool);
    mux_pad->buffer_pool = NULL;
  }

  mux_pad->buffer_pool = pool;

  return ret;
}

GstCaps *
gst_tiovx_mux_fixate_src_caps (GstAggregator * agg, GstCaps * src_caps)
{
  GstTIOVXMux *self = GST_TIOVX_MUX (agg);
  GstCaps *fixated_caps = NULL;
  GstCaps *current_src_caps = NULL;
  GstCaps *src_caps_from_sinks = NULL;

  current_src_caps = gst_tiovx_mux_get_current_src_caps (self);
  src_caps_from_sinks = gst_tiovx_mux_get_src_caps (self, NULL);

  src_caps = gst_caps_intersect (current_src_caps, src_caps_from_sinks);
  fixated_caps = gst_caps_fixate (src_caps);

  gst_caps_unref (current_src_caps);
  gst_caps_unref (src_caps_from_sinks);

  GST_DEBUG_OBJECT (self, "Fixated src caps: %" GST_PTR_FORMAT, fixated_caps);

  return fixated_caps;
}

static GstPad *
gst_tiovx_mux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstPad *newpad;

  newpad = (GstPad *)
      GST_ELEMENT_CLASS (gst_tiovx_mux_parent_class)->request_new_pad (element,
      templ, req_name, caps);

  if (newpad == NULL) {
    GST_DEBUG_OBJECT (element, "could not create/add pad");
    goto exit;
  }

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));

exit:
  return newpad;
}

static void
gst_tiovx_mux_release_pad (GstElement * element, GstPad * pad)
{
  GstTIOVXMux *mux = GST_TIOVX_MUX (element);

  GST_DEBUG_OBJECT (mux, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));
  gst_child_proxy_child_removed (GST_CHILD_PROXY (mux), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));
  GST_ELEMENT_CLASS (gst_tiovx_mux_parent_class)->release_pad (element, pad);
}

static GList *
gst_tiovx_mux_get_sink_caps_list (GstTIOVXMux * self)
{
  GList *sink_caps_list = NULL;
  GList *node = NULL;

  g_return_val_if_fail (self, NULL);

  GST_DEBUG_OBJECT (self, "Generating sink caps list");

  for (node = GST_ELEMENT (self)->sinkpads; node; node = g_list_next (node)) {
    GstPad *sink_pad = GST_PAD (node->data);
    GstCaps *peer_caps = gst_pad_peer_query_caps (sink_pad, NULL);
    GstCaps *pad_caps = NULL;

    pad_caps = intersect_with_template_caps (peer_caps, sink_pad);

    gst_caps_unref (peer_caps);

    GST_DEBUG_OBJECT (self, "Caps from %s:%s peer: %" GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (sink_pad), pad_caps);
    /* Insert at the end of the sink caps list */
    sink_caps_list = g_list_insert (sink_caps_list, pad_caps, -1);
  }

  return sink_caps_list;
}

static GstCaps *
gst_tiovx_mux_get_current_src_caps (GstTIOVXMux * self)
{
  GstCaps *peer_caps = NULL;
  GstCaps *pad_caps = NULL;

  g_return_val_if_fail (self, NULL);

  GST_DEBUG_OBJECT (self, "Generating src caps");

  peer_caps =
      gst_pad_peer_query_caps (GST_PAD (GST_ELEMENT (self)->srcpads->data),
      NULL);
  pad_caps =
      intersect_with_template_caps (peer_caps,
      GST_PAD (GST_ELEMENT (self)->srcpads->data));
  gst_caps_unref (peer_caps);

  GST_DEBUG_OBJECT (self, "Caps from %s:%s peer: %" GST_PTR_FORMAT,
      GST_DEBUG_PAD_NAME (GST_PAD (GST_ELEMENT (self)->srcpads->data)),
      pad_caps);

  return pad_caps;
}

static GstCaps *
gst_tiovx_mux_get_src_caps (GstTIOVXMux * self, GstCaps * filter)
{
  GstCaps *template_caps = NULL;
  GstCaps *src_caps = NULL;
  GList *sink_caps_list = NULL;
  GList *l = NULL;
  guint num_channels = 0;
  guint i = 0;

  template_caps = gst_static_pad_template_get_caps (&src_template);
  if (filter) {
    src_caps = gst_caps_intersect (template_caps, filter);
  } else {
    src_caps = gst_caps_copy (template_caps);
  }
  gst_caps_unref (template_caps);

  /* Intersect against all sink caps */
  sink_caps_list = gst_tiovx_mux_get_sink_caps_list (self);
  for (l = sink_caps_list; l != NULL; l = g_list_next (l)) {
    GstCaps *sink_caps = gst_caps_copy ((GstCaps *) l->data);
    GstCaps *tmp = NULL;

    /* Downstream might have more than 1 channel, upstream only accepts 1.
     * We'll remove the here, it will be readded as 1 when intersecting
     * against the src_template
     */
    for (i = 0; i < gst_caps_get_size (sink_caps); i++) {
      GstStructure *structure = structure =
          gst_caps_get_structure (sink_caps, i);
      gst_structure_remove_field (structure, "num-channels");
    }

    tmp = gst_caps_intersect (src_caps, sink_caps);
    gst_caps_unref (sink_caps);
    gst_caps_unref (src_caps);
    src_caps = tmp;
  }

  num_channels = g_list_length (GST_ELEMENT (self)->sinkpads);
  for (i = 0; i < gst_caps_get_size (src_caps); i++) {
    GstStructure *structure = structure = gst_caps_get_structure (src_caps, i);
    GValue channels_value = G_VALUE_INIT;

    g_value_init (&channels_value, G_TYPE_INT);
    g_value_set_int (&channels_value, num_channels);

    gst_structure_set_value (structure, "num-channels", &channels_value);
    g_value_unset (&channels_value);
  }

  GST_DEBUG_OBJECT (self, "result: %" GST_PTR_FORMAT, src_caps);

  return src_caps;
}

static GstCaps *
gst_tiovx_mux_get_sink_caps (GstPad * pad, GstTIOVXMux * self, GstCaps * filter)
{
  GstCaps *sink_caps = NULL;
  GstCaps *src_caps = NULL;
  GstCaps *template_caps = NULL;
  GList *sink_caps_list = NULL;
  GstCaps *tmp = NULL;
  GList *l = NULL;
  guint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (pad, NULL);

  GST_DEBUG_OBJECT (pad,
      "Computing sink caps based on src caps and filter %"
      GST_PTR_FORMAT, filter);

  template_caps = gst_static_pad_template_get_caps (&sink_template);
  if (filter) {
    sink_caps = gst_caps_intersect (template_caps, filter);
  } else {
    sink_caps = gst_caps_copy (template_caps);
  }
  gst_caps_unref (template_caps);

  /* All caps on input and output should match, we'll interesect against both */

  /* Intersect against src caps */
  src_caps = gst_tiovx_mux_get_current_src_caps (self);
  tmp = src_caps;
  src_caps = gst_caps_make_writable (gst_caps_copy (src_caps));
  gst_caps_unref (tmp);

  /* Downstream might have more than 1 channel, upstream only accepts 1.
   * We'll remove the here, it will be readded as 1 when intersecting
   * against the src_template
   */
  for (i = 0; i < gst_caps_get_size (src_caps); i++) {
    GstStructure *structure = structure = gst_caps_get_structure (src_caps, i);
    gst_structure_remove_field (structure, "num-channels");
  }

  tmp = gst_caps_intersect (sink_caps, src_caps);
  gst_caps_unref (sink_caps);
  gst_caps_unref (src_caps);
  sink_caps = tmp;


  /* Intersect against all sink caps */
  sink_caps_list = gst_tiovx_mux_get_sink_caps_list (self);
  for (l = sink_caps_list; l != NULL; l = g_list_next (l)) {
    GstCaps *other_sink_caps = gst_caps_copy ((GstCaps *) l->data);
    GstCaps *tmp = NULL;

    tmp = gst_caps_intersect (sink_caps, other_sink_caps);
    gst_caps_unref (sink_caps);
    gst_caps_unref (other_sink_caps);
    sink_caps = tmp;
  }

  GST_DEBUG_OBJECT (pad, "result: %" GST_PTR_FORMAT, sink_caps);

  return sink_caps;
}

static int
gst_tiovx_mux_sink_query (GstAggregator * agg, GstAggregatorPad * pad,
    GstQuery * query)
{
  GstTIOVXMux *self = GST_TIOVX_MUX (agg);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_tiovx_mux_get_sink_caps (GST_PAD (pad), self, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret =
          GST_AGGREGATOR_CLASS (gst_tiovx_mux_parent_class)->sink_query
          (agg, pad, query);
      break;
  }
  return ret;
}

static int
gst_tiovx_mux_src_query (GstAggregator * agg, GstQuery * query)
{
  GstTIOVXMux *self = GST_TIOVX_MUX (agg);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_tiovx_mux_get_src_caps (self, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret =
          GST_AGGREGATOR_CLASS (gst_tiovx_mux_parent_class)->src_query
          (agg, query);
      break;
  }
  return ret;
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


/* GstChildProxy implementation */
static GObject *
gst_tiovx_mux_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  GstTIOVXMux *mux = GST_TIOVX_MUX (child_proxy);
  GObject *obj = NULL;
  GST_OBJECT_LOCK (mux);
  obj = g_list_nth_data (GST_ELEMENT_CAST (mux)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (mux);
  return obj;
}

static guint
gst_tiovx_mux_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstTIOVXMux *mux = GST_TIOVX_MUX (child_proxy);
  GST_OBJECT_LOCK (mux);
  count = GST_ELEMENT_CAST (mux)->numsinkpads;
  GST_OBJECT_UNLOCK (mux);
  return count;
}

static void
gst_tiovx_mux_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;
  iface->get_child_by_index = gst_tiovx_mux_child_proxy_get_child_by_index;
  iface->get_children_count = gst_tiovx_mux_child_proxy_get_children_count;
}
