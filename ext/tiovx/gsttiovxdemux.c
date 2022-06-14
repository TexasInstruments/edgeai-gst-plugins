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

#include "gsttiovxdemux.h"

#include <stdio.h>

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferutils.h"
#include "gst-libs/gst/tiovx/gsttiovxcontext.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "gst-libs/gst/tiovx/gsttiovxmuxmeta.h"

#define GST_TIOVX_DEMUX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TIOVX_DEMUX, GstTIOVXDemuxClass))

#define TIOVX_MAX_CHANNELS 16

/* Formats definition */
#define TIOVX_DEMUX_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_DEMUX_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_DEMUX_SUPPORTED_CHANNELS "[1 , 16]"

#define TIOVX_DEMUX_SUPPORTED_VIDEO_FORMATS "{ RGB, RGBx, NV12, NV21, UYVY, YUY2, I420, GRAY8, GRAY16_LE }"

#define TIOVX_DEMUX_SUPPORTED_TENSOR_FORMATS "{RGB, NV12, NV21}"
#define TIOVX_DEMUX_SUPPORTED_TENSOR_DIMENSIONS "3"
#define TIOVX_DEMUX_SUPPORTED_TENSOR_DATA_TYPES "[2 , 10]"
#define TIOVX_DEMUX_SUPPORTED_TENSOR_CHANNEL_ORDER "{NCHW, NHWC}"
#define TIOVX_DEMUX_SUPPORTED_TENSOR_FORMAT "{RGB, BGR}"

#define TIOVX_DEMUX_SUPPORTED_PYRAMID_FORMAT "{GRAY8, GRAY16_LE}"
#define TIOVX_DEMUX_SUPPORTED_PYRAMID_WIDTH "[1 , 1920]"
#define TIOVX_DEMUX_SUPPORTED_PYRAMID_HEIGHT "[1 , 1088]"
#define TIOVX_DEMUX_SUPPORTED_PYRAMID_LEVELS "[1 , 8]"
#define TIOVX_DEMUX_SUPPORTED_PYRAMID_SCALE "[0.25 , 1.0]"

/* Src caps */
#define TIOVX_DEMUX_STATIC_CAPS_SRC                                  \
  "video/x-raw, "                                                    \
  "format = (string) " TIOVX_DEMUX_SUPPORTED_VIDEO_FORMATS ", "      \
  "width = " TIOVX_DEMUX_SUPPORTED_WIDTH ", "                        \
  "height = " TIOVX_DEMUX_SUPPORTED_HEIGHT                           \
  "; "                                                               \
  "application/x-tensor-tiovx, "                                     \
  "num-dims = " TIOVX_DEMUX_SUPPORTED_TENSOR_DIMENSIONS ", "         \
  "data-type = " TIOVX_DEMUX_SUPPORTED_TENSOR_DATA_TYPES ", "        \
  "channel-order = " TIOVX_DEMUX_SUPPORTED_TENSOR_CHANNEL_ORDER ", " \
  "tensor-format = " TIOVX_DEMUX_SUPPORTED_TENSOR_FORMAT ", "        \
  "tensor-width = " TIOVX_DEMUX_SUPPORTED_WIDTH ", "                 \
  "tensor-height = " TIOVX_DEMUX_SUPPORTED_HEIGHT                    \
  "; "                                                               \
  "application/x-pyramid-tiovx, "                                    \
  "format = (string) " TIOVX_DEMUX_SUPPORTED_PYRAMID_FORMAT ", "     \
  "width = " TIOVX_DEMUX_SUPPORTED_PYRAMID_WIDTH ", "                \
  "height = " TIOVX_DEMUX_SUPPORTED_PYRAMID_HEIGHT ", "              \
  "levels = " TIOVX_DEMUX_SUPPORTED_PYRAMID_LEVELS ", "              \
  "scale = " TIOVX_DEMUX_SUPPORTED_PYRAMID_SCALE                     \
  "; "                                                               \
  "application/x-dof-tiovx, "                                        \
  "width = " TIOVX_DEMUX_SUPPORTED_WIDTH ", "                        \
  "height = " TIOVX_DEMUX_SUPPORTED_HEIGHT

/* Sink caps */
#define TIOVX_DEMUX_STATIC_CAPS_SINK                                   \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "                 \
  "format = (string) " TIOVX_DEMUX_SUPPORTED_VIDEO_FORMATS ", "        \
  "width = " TIOVX_DEMUX_SUPPORTED_WIDTH ", "                          \
  "height = " TIOVX_DEMUX_SUPPORTED_HEIGHT ", "                        \
  "num-channels = " TIOVX_DEMUX_SUPPORTED_CHANNELS                     \
  "; "                                                                 \
  "application/x-tensor-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "  \
  "num-dims = " TIOVX_DEMUX_SUPPORTED_TENSOR_DIMENSIONS ", "           \
  "data-type = " TIOVX_DEMUX_SUPPORTED_TENSOR_DATA_TYPES ", "          \
  "channel-order = " TIOVX_DEMUX_SUPPORTED_TENSOR_CHANNEL_ORDER ", "   \
  "tensor-format = " TIOVX_DEMUX_SUPPORTED_TENSOR_FORMAT ", "          \
  "tensor-width = " TIOVX_DEMUX_SUPPORTED_WIDTH ", "                   \
  "tensor-height = " TIOVX_DEMUX_SUPPORTED_HEIGHT ", "                 \
  "num-channels = " TIOVX_DEMUX_SUPPORTED_CHANNELS                     \
  "; "                                                                 \
  "application/x-pyramid-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), " \
  "format = (string) " TIOVX_DEMUX_SUPPORTED_PYRAMID_FORMAT ", "       \
  "width = " TIOVX_DEMUX_SUPPORTED_PYRAMID_WIDTH ", "                  \
  "height = " TIOVX_DEMUX_SUPPORTED_PYRAMID_HEIGHT ", "                \
  "levels = " TIOVX_DEMUX_SUPPORTED_PYRAMID_LEVELS ", "                \
  "scale = " TIOVX_DEMUX_SUPPORTED_PYRAMID_SCALE ", "                  \
  "num-channels = " TIOVX_DEMUX_SUPPORTED_CHANNELS                     \
  "; "                                                                 \
  "application/x-dof-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "     \
  "width = " TIOVX_DEMUX_SUPPORTED_WIDTH ", "                          \
  "height = " TIOVX_DEMUX_SUPPORTED_HEIGHT ", "                        \
  "num-channels = " TIOVX_DEMUX_SUPPORTED_CHANNELS

#define TENSOR_NUM_DIMS_SUPPORTED 3
#define TENSOR_CHANNELS_SUPPORTED 3

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DEMUX_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_DEMUX_STATIC_CAPS_SRC)
    );

typedef struct _GstTIOVXDemux
{
  GstElement parent;

  GstTIOVXContext *tiovx_context;
  vx_context context;
  vx_reference input_reference;

  GstTIOVXPad *sinkpad;
  GList *srcpads;
} GstTIOVXDemux;

static GstElementClass *parent_class = NULL;

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_demux_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_demux_debug_category

static void gst_tiovx_demux_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

static void gst_tiovx_demux_finalize (GObject * object);

static gboolean gst_tiovx_demux_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_tiovx_demux_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstFlowReturn gst_tiovx_demux_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static GstPad *gst_tiovx_demux_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * name_templ, const GstCaps * caps);
static void gst_tiovx_demux_release_pad (GstElement * element, GstPad * pad);
static GstCaps *gst_tiovx_demux_get_sink_caps (GstTIOVXDemux * self,
    GstCaps * filter, GList * src_caps_list);
static GstCaps *gst_tiovx_demux_get_src_caps (GstTIOVXDemux * self,
    GstCaps * filter, GstCaps * sink_caps);
static GList *gst_tiovx_demux_fixate_caps (GstTIOVXDemux * self,
    GstCaps * sink_caps, GList * src_caps_list);
static gboolean gst_tiovx_demux_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn
gst_tiovx_demux_push_buffers (GstTIOVXDemux * demux, GList * pads,
    GstBuffer ** buffer_list);

#define GST_TIOVX_DEMUX_DEFINE_CUSTOM_CODE \
  GST_DEBUG_CATEGORY_INIT (gst_tiovx_demux_debug_category, "tiovxdemux", 0, "debug category for the tiovxdemux element"); \
  G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,  gst_tiovx_demux_child_proxy_init);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXDemux, gst_tiovx_demux,
    GST_TYPE_ELEMENT, GST_TIOVX_DEMUX_DEFINE_CUSTOM_CODE);

static void
gst_tiovx_demux_class_init (GstTIOVXDemuxClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Demux",
      "Generic",
      "Decompounds a multi-stream into multiple single streams",
      "RidgeRun <support@ridgerun.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sink_template, GST_TYPE_TIOVX_PAD);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_demux_finalize);

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_demux_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_demux_release_pad);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_demux_debug_category, "tiovxdemux", 0,
      "tiovxdemux element");

  parent_class = g_type_class_peek_parent (klass);
}

static void
gst_tiovx_demux_init (GstTIOVXDemux * self)
{
  GstElementClass *gstelement_class =
      (GstElementClass *) GST_TIOVX_DEMUX_GET_CLASS (self);
  GstPadTemplate *pad_template = NULL;
  vx_status status = VX_FAILURE;

  GST_DEBUG_OBJECT (self, "gst_tiovx_demux_init");

  pad_template = gst_element_class_get_pad_template (gstelement_class, "sink");
  g_return_if_fail (pad_template != NULL);

  self->sinkpad =
      GST_TIOVX_PAD (gst_pad_new_from_template (pad_template, "sink"));

  gst_pad_set_event_function (GST_PAD (self->sinkpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_demux_sink_event));
  gst_pad_set_query_function (GST_PAD (self->sinkpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_demux_sink_query));
  gst_pad_set_chain_function (GST_PAD (self->sinkpad),
      GST_DEBUG_FUNCPTR (gst_tiovx_demux_chain));
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD (self->sinkpad));
  gst_pad_set_active (GST_PAD (self->sinkpad), FALSE);

  /* Initialize all private member data */
  self->context = NULL;

  self->tiovx_context = NULL;

  self->tiovx_context = gst_tiovx_context_new ();
  if (NULL == self->tiovx_context) {
    GST_ERROR_OBJECT (self, "Failed to do common initialization");
    return;
  }

  self->context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) self->context);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Context creation failed, vx_status %" G_GINT32_FORMAT, status);
    return;
  }

  return;
}

static void
gst_tiovx_demux_finalize (GObject * gobject)
{
  GstTIOVXDemux *self = GST_TIOVX_DEMUX (gobject);

  GST_LOG_OBJECT (self, "finalize");

  if (self->input_reference) {
    vxReleaseReference (&self->input_reference);
    self->input_reference = NULL;
  }

  if (self->context) {
    vxReleaseContext (&self->context);
    self->context = NULL;
  }

  if (self->tiovx_context) {
    g_object_unref (self->tiovx_context);
    self->tiovx_context = NULL;
  }

  if (self->srcpads) {
    g_list_free_full (self->srcpads, (GDestroyNotify) gst_object_unref);
    self->srcpads = NULL;
  }


  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static GstPad *
gst_tiovx_demux_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name_templ, const GstCaps * caps)
{
  GstTIOVXDemux *self = GST_TIOVX_DEMUX (element);
  guint name_index = 0;
  GstPad *src_pad = NULL;
  gchar *name = NULL;

  GST_DEBUG_OBJECT (self, "requesting pad");

  g_return_val_if_fail (templ, NULL);

  GST_OBJECT_LOCK (self);

  if (NULL == name_templ) {
    name_templ = "src_%u";
  }

  /* The user may request the pad in two ways:
   * - src_%u: We need to assign a free index
   * - src_N: We need to create a pad with the given name
   */

  /* src_%u: No index provided, need to compute one */
  if (0 == sscanf (name_templ, "src_%u", &name_index)) {
    GList *iter = self->srcpads;

    name_index = 0;
    for (; iter; iter = g_list_next (iter)) {
      GstPad *pad = GST_PAD (iter->data);
      const gchar *pad_name = GST_OBJECT_NAME (pad);
      guint used_index = 0;

      sscanf (pad_name, "src_%u", &used_index);
      if (used_index >= name_index) {
        name_index = used_index + 1;
      }
    }

    GST_INFO_OBJECT (self, "Requested new pad, assigned an index of %u",
        name_index);
    name = g_strdup_printf ("src_%u", name_index);
  } else {
    GList *iter = self->srcpads;

    for (; iter; iter = g_list_next (iter)) {
      GstPad *pad = GST_PAD (iter->data);
      const gchar *pad_name = GST_OBJECT_NAME (pad);
      guint used_index = 0;

      sscanf (pad_name, "src_%u", &used_index);
      if (used_index == name_index) {
        GST_ERROR_OBJECT (self, "A pad with index %u is already in use",
            name_index);
        goto unlock;
      }
    }

    GST_INFO_OBJECT (self, "Requested pad index %u is free", name_index);
    name = g_strdup (name_templ);
  }

  src_pad = gst_pad_new_from_template (templ, name);
  if (NULL == src_pad) {
    GST_ERROR_OBJECT (self, "Failed to create source pad");
    goto free_name;
  }

  GST_OBJECT_UNLOCK (self);

  gst_element_add_pad (GST_ELEMENT_CAST (self), src_pad);
  gst_pad_set_active (src_pad, TRUE);

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (src_pad),
      GST_OBJECT_NAME (src_pad));

  GST_OBJECT_LOCK (self);
  self->srcpads = g_list_append (self->srcpads, gst_object_ref (src_pad));

  gst_pad_set_query_function (GST_PAD (src_pad),
      GST_DEBUG_FUNCPTR (gst_tiovx_demux_src_query));

free_name:
  g_free (name);

unlock:
  GST_OBJECT_UNLOCK (self);
  return src_pad;
}

static void
gst_tiovx_demux_release_pad (GstElement * element, GstPad * pad)
{
  GstTIOVXDemux *self = GST_TIOVX_DEMUX (element);
  GList *node = NULL;

  GST_OBJECT_LOCK (self);

  node = g_list_find (self->srcpads, pad);
  g_return_if_fail (node);

  self->srcpads = g_list_remove (self->srcpads, pad);
  gst_object_unref (pad);

  GST_OBJECT_UNLOCK (self);

  gst_child_proxy_child_removed (GST_CHILD_PROXY (self), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT_CAST (self), pad);
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
gst_tiovx_demux_get_src_caps_list (GstTIOVXDemux * self)
{
  GList *src_caps_list = NULL;
  GList *node = NULL;

  g_return_val_if_fail (self, NULL);

  GST_DEBUG_OBJECT (self, "Generating src caps list");

  for (node = self->srcpads; node; node = g_list_next (node)) {
    GstPad *src_pad = GST_PAD (node->data);
    GstCaps *peer_caps = gst_pad_peer_query_caps (src_pad, NULL);
    GstCaps *pad_caps = NULL;

    pad_caps = intersect_with_template_caps (peer_caps, src_pad);

    gst_caps_unref (peer_caps);

    GST_DEBUG_OBJECT (self, "Caps from %s:%s peer: %" GST_PTR_FORMAT,
        GST_DEBUG_PAD_NAME (src_pad), pad_caps);
    /* Insert at the end of the src caps list */
    src_caps_list = g_list_insert (src_caps_list, pad_caps, -1);
  }

  return src_caps_list;
}

static GstCaps *
gst_tiovx_demux_get_sink_caps (GstTIOVXDemux * self,
    GstCaps * filter, GList * src_caps_list)
{
  GstCaps *sink_caps = NULL;
  GstCaps *template_caps = NULL;
  GList *l = NULL;
  guint num_channels = 0;
  guint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_DEBUG_OBJECT (self,
      "Computing sink caps based on src caps and filter %"
      GST_PTR_FORMAT, filter);

  template_caps = gst_static_pad_template_get_caps (&sink_template);
  if (filter) {
    sink_caps = gst_caps_intersect (template_caps, filter);
  } else {
    sink_caps = gst_caps_copy (template_caps);
  }
  gst_caps_unref (template_caps);

  /* Input and output dimensions should match, remove num-channels from src caps to intersect */
  for (l = src_caps_list; l != NULL; l = g_list_next (l)) {
    GstCaps *src_caps = gst_caps_copy ((GstCaps *) l->data);
    GstCaps *tmp = NULL;

    /* Upstream might have more than 1 channel, downstream only accepts 1.
     * We'll remove the here, it will be readded as 1 when intersecting
     * against the src_template
     */
    gst_caps_set_features_simple (src_caps,
        gst_tiovx_get_batched_memory_feature ());
    for (i = 0; i < gst_caps_get_size (src_caps); i++) {
      GstStructure *structure = structure =
          gst_caps_get_structure (src_caps, i);

      gst_structure_remove_field (structure, "num-channels");
    }

    tmp = gst_caps_intersect (sink_caps, src_caps);
    gst_caps_unref (sink_caps);
    gst_caps_unref (src_caps);
    sink_caps = tmp;
  }

  /* Set the correct num-channels to the caps */
  num_channels = g_list_length (GST_ELEMENT (self)->srcpads);
  for (i = 0; i < gst_caps_get_size (sink_caps); i++) {
    GstStructure *structure = structure = gst_caps_get_structure (sink_caps, i);
    GValue channels_value = G_VALUE_INIT;

    g_value_init (&channels_value, G_TYPE_INT);
    g_value_set_int (&channels_value, num_channels);

    gst_structure_set_value (structure, "num-channels", &channels_value);
    g_value_unset (&channels_value);
  }

  GST_DEBUG_OBJECT (self, "result: %" GST_PTR_FORMAT, sink_caps);

  return sink_caps;
}

static GstCaps *
gst_tiovx_demux_get_src_caps (GstTIOVXDemux * self,
    GstCaps * filter, GstCaps * sink_caps)
{
  GstCaps *src_caps = NULL;
  GstCaps *template_caps = NULL;
  guint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (sink_caps, NULL);

  GST_DEBUG_OBJECT (self,
      "Computing src caps based on sink caps %" GST_PTR_FORMAT " and filter %"
      GST_PTR_FORMAT, sink_caps, filter);

  /* Upstream might have more than 1 channel, downstream only accepts 1.
   * We'll remove the here, it will be readded as 1 when intersecting
   * against the src_template
   */
  sink_caps = gst_caps_copy (sink_caps);
  for (i = 0; i < gst_caps_get_size (sink_caps); i++) {
    GstStructure *structure = structure = gst_caps_get_structure (sink_caps, i);
    GstCapsFeatures *caps_feature = gst_caps_get_features (sink_caps, i);

    gst_caps_features_remove (caps_feature, GST_CAPS_FEATURE_BATCHED_MEMORY);
    gst_structure_remove_field (structure, "num-channels");
  }

  template_caps = gst_static_pad_template_get_caps (&src_template);

  src_caps = gst_caps_intersect (template_caps, sink_caps);

  gst_caps_unref (template_caps);

  if (filter) {
    GstCaps *tmp = src_caps;
    src_caps = gst_caps_intersect (src_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_INFO_OBJECT (self,
      "Resulting supported src caps by TIOVX demux: %"
      GST_PTR_FORMAT, src_caps);

  gst_caps_unref (sink_caps);

  return src_caps;
}

static gboolean
gst_tiovx_demux_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXDemux *self = GST_TIOVX_DEMUX (parent);
  GstCaps *sink_caps = NULL;
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter;
      GList *src_caps_list = NULL;

      if (NULL == self->srcpads) {
        break;
      }

      gst_query_parse_caps (query, &filter);
      filter = intersect_with_template_caps (filter, pad);

      src_caps_list = gst_tiovx_demux_get_src_caps_list (self);
      if (NULL == src_caps_list) {
        GST_ERROR_OBJECT (self, "Get src caps list method failed");
        break;
      }

      /* Should return the caps the element supports on the sink pad */
      sink_caps = gst_tiovx_demux_get_sink_caps (self, filter, src_caps_list);
      if (NULL == sink_caps) {
        GST_ERROR_OBJECT (self, "Get caps method failed");
        break;
      }

      if (gst_caps_is_fixed (sink_caps)) {
        if (self->input_reference) {
          vxReleaseReference (&self->input_reference);
          self->input_reference = NULL;
        }

        self->input_reference =
            gst_tiovx_get_exemplar_from_caps ((GObject *) self, GST_CAT_DEFAULT,
            self->context, sink_caps);

        gst_tiovx_pad_set_exemplar (self->sinkpad, self->input_reference);
      }

      ret = TRUE;

      /* The query response should be the supported caps in the sink
       * from get_caps */
      gst_query_set_caps_result (query, sink_caps);
      gst_caps_unref (sink_caps);
      if (filter) {
        gst_caps_unref (filter);
      }
      g_list_free_full (src_caps_list, (GDestroyNotify) gst_caps_unref);

      break;
    }
    default:
      ret = gst_tiovx_pad_query (GST_PAD (self->sinkpad), parent, query);
      break;
  }

  return ret;
}

static gboolean
gst_tiovx_demux_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstTIOVXDemux *self = GST_TIOVX_DEMUX (parent);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstPad *sink_pad = GST_PAD (self->sinkpad);
      GstCaps *filter = NULL;
      GstCaps *sink_caps = NULL;
      GstCaps *filtered_sink_caps = NULL;
      GstCaps *src_caps = NULL;

      gst_query_parse_caps (query, &filter);
      filter = intersect_with_template_caps (filter, pad);

      sink_caps = gst_pad_peer_query_caps (sink_pad, filter);
      filtered_sink_caps = intersect_with_template_caps (sink_caps, sink_pad);
      gst_caps_unref (sink_caps);

      /* Should return the caps the element supports on the src pad */
      src_caps =
          gst_tiovx_demux_get_src_caps (self, filter, filtered_sink_caps);
      gst_caps_unref (filtered_sink_caps);

      if (NULL == src_caps) {
        GST_ERROR_OBJECT (self, "Get src caps method failed");
        break;
      }

      ret = TRUE;

      /* The query response should be the supported caps in the sink
       * from get_caps */
      gst_query_set_caps_result (query, src_caps);
      gst_caps_unref (src_caps);
      break;
    }
    default:
    {
      ret = gst_pad_query_default (pad, parent, query);
      break;
    }
  }

  return ret;
}

static GList *
gst_tiovx_demux_fixate_caps (GstTIOVXDemux * self, GstCaps * sink_caps,
    GList * src_caps_list)
{
  GList *l = NULL;
  GList *result_src_caps_list = NULL;
  GstCaps *sink_caps_copy = NULL;
  gint i = 0;

  g_return_val_if_fail (sink_caps, NULL);
  g_return_val_if_fail (gst_caps_is_fixed (sink_caps), NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_DEBUG_OBJECT (self, "Fixating src caps from sink caps %" GST_PTR_FORMAT,
      sink_caps);

  sink_caps_copy = gst_caps_copy (sink_caps);

  for (i = 0; i < gst_caps_get_size (sink_caps); i++) {
    GstCapsFeatures *caps_feature = gst_caps_get_features (sink_caps_copy, i);
    GstStructure *structure = gst_caps_get_structure (sink_caps_copy, i);

    gst_structure_remove_field (structure, "num-channels");
    gst_caps_features_remove (caps_feature, GST_CAPS_FEATURE_BATCHED_MEMORY);
  }

  for (l = src_caps_list; l != NULL; l = g_list_next (l)) {
    GstCaps *src_caps = gst_caps_copy ((GstCaps *) l->data);
    GstCaps *fixated_src_caps = NULL;
    GstCaps *tmp = NULL;

    /* Upstream might have more than 1 channel, dowstream only accepts 1.
     * We'll remove the here, it will be readded as 1 when intersecting
     * against the src_template
     */

    tmp = gst_caps_intersect (sink_caps_copy, src_caps);
    gst_caps_unref (src_caps);
    fixated_src_caps = gst_caps_fixate (tmp);

    if (!gst_caps_is_empty (fixated_src_caps)) {
      GValue channels_value = G_VALUE_INIT;
      GstStructure *structure = gst_caps_get_structure (fixated_src_caps, 0);

      g_value_init (&channels_value, G_TYPE_INT);
      g_value_set_int (&channels_value, 1);

      gst_structure_set_value (structure, "num-channels", &channels_value);
      g_value_unset (&channels_value);
    }

    result_src_caps_list =
        g_list_append (result_src_caps_list, fixated_src_caps);
  }

  gst_caps_unref (sink_caps_copy);

  return result_src_caps_list;
}

static GstFlowReturn
gst_tiovx_demux_chain (GstPad * pad, GstObject * parent, GstBuffer * in_buffer)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstTIOVXDemux *self = NULL;
  vx_object_array in_array = NULL;
  GstBuffer *buffer_list[TIOVX_MAX_CHANNELS] = { NULL };
  vx_size in_num_channels = 0;
  GstClockTime pts = 0, dts = 0, duration = 0;
  guint64 offset = 0, offset_end = 0;
  vx_status status = VX_FAILURE;
  gint num_pads = 0;
  gint i = 0;
  vx_reference exemplar = NULL;

  self = GST_TIOVX_DEMUX (parent);

  pts = GST_BUFFER_PTS (in_buffer);
  dts = GST_BUFFER_DTS (in_buffer);
  duration = GST_BUFFER_DURATION (in_buffer);
  offset = GST_BUFFER_OFFSET (in_buffer);
  offset_end = GST_BUFFER_OFFSET_END (in_buffer);

  GST_LOG_OBJECT (self, "Chaining buffer");

  /* Chain sink pads' TIOVXPad call, this ensures valid vx_reference in the input  */
  ret = gst_tiovx_pad_chain (pad, parent, &in_buffer);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (pad, "Pad's chain function failed");
    goto exit;
  }

  exemplar = gst_tiovx_pad_get_exemplar (self->sinkpad);
  in_array =
      gst_tiovx_get_vx_array_from_buffer (GST_CAT_DEFAULT, exemplar, in_buffer);

  num_pads = g_list_length (self->srcpads);
  status =
      vxQueryObjectArray (in_array, VX_OBJECT_ARRAY_NUMITEMS, &in_num_channels,
      sizeof (vx_size));
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Get number of channels in input buffer failed %" G_GINT32_FORMAT,
        status);
    goto exit;
  }

  g_return_val_if_fail (in_num_channels == num_pads, GST_FLOW_ERROR);

  /* Transfer handles */
  GST_LOG_OBJECT (self, "Transferring handles");

  for (i = 0; i < num_pads; i++) {
    GstMemory *out_memory = NULL;
    vx_object_array output_array = NULL;
    vx_reference input_reference = NULL;
    vx_reference output_reference = NULL;
    gsize size = 0;
    void *in_data = NULL;

    input_reference = vxGetObjectArrayItem (in_array, i);

    buffer_list[i] = gst_buffer_new ();

    /* Create GstMemory with the correct memory, this allows mapping for SW objects */
    status =
        gst_tiovx_demux_get_exemplar_mem ((GObject *) self, GST_CAT_DEFAULT,
        input_reference, &in_data, &size);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self,
          "Unable to extract memory information from input buffer");
      goto exit;
    }
    out_memory = gst_memory_new_wrapped (GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS,
        in_data, size, 0, size, NULL, NULL);
    gst_buffer_append_memory (buffer_list[i], out_memory);

    /* Create output array and assign the memory from the input */
    output_array = vxCreateObjectArray (self->context, exemplar, 1);
    output_reference = vxGetObjectArrayItem (output_array, 0);
    gst_tiovx_transfer_handle (GST_CAT_DEFAULT, input_reference,
        output_reference);
    gst_buffer_add_tiovx_mux_meta (buffer_list[i], (vx_reference) output_array);

    /* Assign parent so this memory remains valid */
    gst_buffer_add_parent_buffer_meta (buffer_list[i], in_buffer);

    vxReleaseReference (&input_reference);
    vxReleaseReference (&output_reference);
    vxReleaseObjectArray (&output_array);
  }

  for (i = 0; i < num_pads; i++) {
    GST_BUFFER_PTS (buffer_list[i]) = pts;
    GST_BUFFER_DTS (buffer_list[i]) = dts;
    GST_BUFFER_DURATION (buffer_list[i]) = duration;
    GST_BUFFER_OFFSET (buffer_list[i]) = offset;
    GST_BUFFER_OFFSET_END (buffer_list[i]) = offset_end;
  }

  ret = gst_tiovx_demux_push_buffers (self, self->srcpads, buffer_list);
  if (GST_FLOW_OK != ret) {
    GST_ERROR_OBJECT (self, "Unable to push all buffers to source pads: %d",
        ret);
  }

exit:
  if (GST_FLOW_ERROR == ret) {
    for (i = 0; i < TIOVX_MAX_CHANNELS; i++) {
      if (buffer_list[i]) {
        gst_buffer_unref (buffer_list[i]);
        buffer_list[i] = NULL;
      }
    }
  }

  gst_buffer_unref (in_buffer);
  return ret;
}


static gboolean
gst_tiovx_demux_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTIOVXDemux *self = GST_TIOVX_DEMUX (parent);
  GList *caps_node = NULL;
  GList *pads_node = NULL;
  gboolean ret = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *sink_caps = NULL;
      GList *src_caps_list = NULL;
      GList *fixated_list = NULL;

      gst_event_parse_caps (event, &sink_caps);

      src_caps_list = gst_tiovx_demux_get_src_caps_list (self);

      /* Should return the fixated caps the element will use on the src pads */
      fixated_list =
          gst_tiovx_demux_fixate_caps (self, sink_caps, src_caps_list);
      if (NULL == fixated_list) {
        GST_ERROR_OBJECT (self, "Subclass did not fixate caps");
        gst_event_unref (event);
        break;
      }

      /* Discard previous list of source caps */
      g_list_free_full (src_caps_list, (GDestroyNotify) gst_caps_unref);

      for (caps_node = fixated_list, pads_node = self->srcpads;
          caps_node && pads_node;
          caps_node = g_list_next (caps_node), pads_node =
          g_list_next (pads_node)) {
        GstCaps *src_caps = (GstCaps *) caps_node->data;
        GstPad *src_pad = GST_PAD (pads_node->data);

        /* Notify peer pads downstream about fixated caps in this pad */
        gst_pad_push_event (src_pad, gst_event_new_caps (src_caps));
      }

      g_list_free_full (fixated_list, (GDestroyNotify) gst_caps_unref);

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

static GstFlowReturn
gst_tiovx_demux_push_buffers (GstTIOVXDemux * demux, GList * pads,
    GstBuffer ** buffer_list)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GList *pad_node = NULL;
  gint i = 0;

  g_return_val_if_fail (demux, GST_FLOW_ERROR);
  g_return_val_if_fail (pads, GST_FLOW_ERROR);
  g_return_val_if_fail (buffer_list, GST_FLOW_ERROR);

  for (pad_node = pads; pad_node; pad_node = g_list_next (pad_node)) {
    GstFlowReturn push_return = GST_FLOW_ERROR;
    GstPad *pad = NULL;

    pad = GST_PAD (pad_node->data);
    g_return_val_if_fail (pad, FALSE);

    push_return = gst_pad_push (pad, buffer_list[i]);
    if (GST_FLOW_OK != push_return) {
      /* If one pad fails, don't exit immediately, attempt to push to all pads
       * but return an error
       */
      GST_ERROR_OBJECT (demux, "Error pushing to pad: %" GST_PTR_FORMAT, pad);
      ret = GST_FLOW_ERROR;
    }
    buffer_list[i] = NULL;

    i++;
  }

  return ret;
}

/* GstChildProxy implementation */
static GObject *
gst_tiovx_demux_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  return NULL;
}

static GObject *
gst_tiovx_demux_child_proxy_get_child_by_name (GstChildProxy *
    child_proxy, const gchar * name)
{
  GstTIOVXDemux *self = NULL;
  GObject *obj = NULL;

  self = GST_TIOVX_DEMUX (child_proxy);

  GST_OBJECT_LOCK (self);

  if (0 == strcmp (name, "sink")) {
    /* Only one sink pad for DEMUX class */
    obj = g_list_nth_data (GST_ELEMENT_CAST (self)->sinkpads, 0);
    if (obj) {
      gst_object_ref (obj);
    }
  } else {                      /* src pad case */
    GList *node = NULL;
    node = GST_ELEMENT_CAST (self)->srcpads;
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
gst_tiovx_demux_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  GstTIOVXDemux *self = NULL;
  guint count = 0;

  self = GST_TIOVX_DEMUX (child_proxy);

  GST_OBJECT_LOCK (self);
  /* Number of source pads + number of sink pads (always 1) */
  count = GST_ELEMENT_CAST (self)->numsrcpads + 1;
  GST_OBJECT_UNLOCK (self);
  GST_INFO_OBJECT (self, "Children Count: %d", count);

  return count;
}

static void
gst_tiovx_demux_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  iface->get_child_by_index = gst_tiovx_demux_child_proxy_get_child_by_index;
  iface->get_child_by_name = gst_tiovx_demux_child_proxy_get_child_by_name;
  iface->get_children_count = gst_tiovx_demux_child_proxy_get_children_count;
}
