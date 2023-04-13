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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/video.h>

#include "gsttilog.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#define GST_TI_LOG_GET_CLASS(obj)  \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TI_LOG, \
                              GstTILogClass))

/* Pads definitions */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

struct _GstTILog
{
  GstElement element;
  GstPad *sink_pad;
  GstPad *src_pad;
  guint count;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_log_debug);
#define GST_CAT_DEFAULT gst_ti_log_debug

#define gst_ti_log_parent_class parent_class
G_DEFINE_TYPE (GstTILog, gst_ti_log,
    GST_TYPE_ELEMENT);

static void gst_ti_log_finalize (GObject * obj);
static GstFlowReturn gst_ti_log_chain (GstPad * pad, GstObject * parent,
        GstBuffer * buffer);
static gboolean gst_ti_log_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_ti_log_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_ti_log_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

/* Initialize the plugin's class */
static void
gst_ti_log_class_init (GstTILogClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *pad_template = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TI Log",
      "Filter/Converter/Video",
      "Log frames",
      "Rahul T R <r-ravikumar@ti.com>");

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_log_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_log_debug,
      "tilog", 0, "debug category for the tilog element");
}

/* Initialize the new element */
static void
gst_ti_log_init (GstTILog * self)
{
  GstTILogClass *klass = GST_TI_LOG_GET_CLASS (self);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *pad_template = NULL;

  GST_LOG_OBJECT (self, "init");

  self->count = 0;

  pad_template = gst_element_class_get_pad_template (gstelement_class, "sink");
  self->sink_pad = gst_pad_new_from_template (pad_template, "sink");

  pad_template = gst_element_class_get_pad_template (gstelement_class, "src");
  self->src_pad = gst_pad_new_from_template (pad_template, "src");

  gst_pad_set_event_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_ti_log_sink_event));
  gst_pad_set_chain_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_ti_log_chain));
  gst_pad_set_query_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_ti_log_sink_query));
  gst_pad_set_query_function (self->src_pad,
      GST_DEBUG_FUNCPTR (gst_ti_log_src_query));

  gst_element_add_pad (GST_ELEMENT (self), self->src_pad);
  gst_pad_set_active (self->src_pad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), self->sink_pad);
  gst_pad_set_active (self->sink_pad, TRUE);
}

static void
gst_ti_log_finalize (GObject * obj)
{
  GstTILog *self = GST_TI_LOG (obj);

  GST_LOG_OBJECT (self, "finalize");

  G_OBJECT_CLASS (gst_ti_log_parent_class)->finalize (obj);
}

static gboolean
gst_ti_log_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstTILog *self = GST_TI_LOG (parent);

  GST_LOG_OBJECT (self, "src_query");

  return gst_pad_peer_query(self->sink_pad, query);
}

static gboolean
gst_ti_log_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstTILog *self = GST_TI_LOG (parent);

  GST_LOG_OBJECT (self, "src_query");

  return gst_pad_peer_query(self->src_pad, query);
}

static
    gboolean
gst_ti_log_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstTILog *self = GST_TI_LOG (parent);

  GST_LOG_OBJECT (self, "sink_event");

  return gst_pad_push_event (GST_PAD (self->src_pad), event);
}

static
    GstFlowReturn
gst_ti_log_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstTILog *self = GST_TI_LOG (parent);

  GST_LOG_OBJECT (self, "chain");

  log_time("capture", "start");

  ret = gst_pad_push (self->src_pad, buffer);

  return ret;
}
