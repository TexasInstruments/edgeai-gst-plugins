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

#include "gsttidrop.h"

#define GST_TI_DROP_GET_CLASS(obj)  \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TI_DROP, \
                              GstTIDropClass))

/* Pads definitions */
static GstStaticPadTemplate oms_template = GST_STATIC_PAD_TEMPLATE ("oms",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate dms_template = GST_STATIC_PAD_TEMPLATE ("dms",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

struct _GstTIDrop
{
  GstElement element;
  GstPad *sink_pad;
  GstPad *oms_pad;
  GstPad *dms_pad;
  guint count;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_drop_debug);
#define GST_CAT_DEFAULT gst_ti_drop_debug

static void
gst_ti_drop_child_proxy_init (gpointer g_iface, gpointer iface_data);

#define gst_ti_drop_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIDrop, gst_ti_drop,
    GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_ti_drop_child_proxy_init););

static void gst_ti_drop_finalize (GObject * obj);
static GstFlowReturn gst_ti_drop_chain (GstPad * pad, GstObject * parent,
        GstBuffer * buffer);

/* Initialize the plugin's class */
static void
gst_ti_drop_class_init (GstTIDropClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *pad_template = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TI Drop",
      "Filter/Converter/Video",
      "Drop frames",
      "Rahul T R <r-ravikumar@ti.com>");

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype (&dms_template,
      GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype (&oms_template,
      GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_drop_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_drop_debug,
      "tidrop", 0, "debug category for the tidrop element");
}

/* Initialize the new element */
static void
gst_ti_drop_init (GstTIDrop * self)
{
  GstTIDropClass *klass = GST_TI_DROP_GET_CLASS (self);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *pad_template = NULL;

  GST_LOG_OBJECT (self, "init");

  self->count = 0;

  pad_template = gst_element_class_get_pad_template (gstelement_class, "sink");
  self->sink_pad = gst_pad_new_from_template (pad_template, "sink");

  pad_template = gst_element_class_get_pad_template (gstelement_class, "oms");
  self->oms_pad = gst_pad_new_from_template (pad_template, "oms");

  pad_template = gst_element_class_get_pad_template (gstelement_class, "dms");
  self->dms_pad = gst_pad_new_from_template (pad_template, "dms");

  gst_pad_set_chain_function (self->sink_pad,
      GST_DEBUG_FUNCPTR (gst_ti_drop_chain));

  gst_element_add_pad (GST_ELEMENT (self), self->oms_pad);
  gst_pad_set_active (self->oms_pad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), self->dms_pad);
  gst_pad_set_active (self->dms_pad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), self->sink_pad);
  gst_pad_set_active (self->sink_pad, TRUE);
}

static void
gst_ti_drop_finalize (GObject * obj)
{
  GstTIDrop *self = GST_TI_DROP (obj);

  GST_LOG_OBJECT (self, "finalize");

  G_OBJECT_CLASS (gst_ti_drop_parent_class)->finalize (obj);
}

static
    GstFlowReturn
gst_ti_drop_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstTIDrop *self = GST_TI_DROP (parent);

  GST_LOG_OBJECT (self, "chain");

  if ((self->count % 2) == 0) {
    ret = gst_pad_push (self->dms_pad, buffer);
  } else if (((self->count + 1) % 12) == 0) {
    ret = gst_pad_push (self->oms_pad, buffer);
  } else {
    gst_buffer_unref(buffer);
  }

  self->count = (self->count + 1) % 60;

  return ret;
}

/* GstChildProxy implementation */
static GObject *
gst_ti_drop_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  return NULL;
}

static GObject *
gst_ti_drop_child_proxy_get_child_by_name (GstChildProxy *
    child_proxy, const gchar * name)
{
  GstTIDrop *
      self = GST_TI_DROP (child_proxy);
  GObject *
      obj = NULL;

  GST_OBJECT_LOCK (self);

  if (0 == strcmp (name, "sink")) {
    obj = G_OBJECT (self->sink_pad);
  } else if (0 == strcmp (name, "oms")) {
    obj = G_OBJECT (self->oms_pad);
  } else if (0 == strcmp (name, "dms")) {
    obj = G_OBJECT (self->dms_pad);
  }

  if (obj) {
    gst_object_ref (obj);
  }

  GST_OBJECT_UNLOCK (self);

  return obj;
}

static
    guint
gst_ti_drop_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  return 3;
}

static void
gst_ti_drop_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *
      iface = (GstChildProxyInterface *) g_iface;

  iface->get_child_by_index =
      gst_ti_drop_child_proxy_get_child_by_index;
  iface->get_child_by_name = gst_ti_drop_child_proxy_get_child_by_name;
  iface->get_children_count =
      gst_ti_drop_child_proxy_get_children_count;
}
