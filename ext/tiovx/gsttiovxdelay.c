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

#include "gsttiovxdelay.h"

#define DEFAULT_DELAY_SIZE 0

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,
  PROP_DELAY_SIZE,
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_delay_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_delay_debug_category

typedef struct _GstTIOVXDelay
{
  GstBaseTransform parent;

  GQueue *held_buffers;
  guint delay_size;

} GstTIOVXDelay;

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXDelay, gst_tiovx_delay,
    GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_delay_debug_category, "tiovxdelay", 0,
        "debug category for tiovxdelay base class"));

static gboolean gst_tiovx_delay_stop (GstBaseTransform * trans);
static GstFlowReturn gst_tiovx_delay_prepare_output_buffer (GstBaseTransform *
    trans, GstBuffer * input, GstBuffer ** outbuf);
static GstFlowReturn gst_tiovx_delay_transform (GstBaseTransform * trans,
    GstBuffer * input, GstBuffer * outbuf);
static void gst_tiovx_delay_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_delay_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);

static void
gst_tiovx_delay_class_init (GstTIOVXDelayClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Delay",
      "Generic",
      "Delays buffers by a given amount. The first buffer will be replicated while the delay is reached.",
      "RidgeRun support@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_tiovx_delay_set_property;
  gobject_class->get_property = gst_tiovx_delay_get_property;

  g_object_class_install_property (gobject_class, PROP_DELAY_SIZE,
      g_param_spec_uint ("delay-size", "Delay Size",
          "Size of the delay between input and output buffers", 0, G_MAXUINT,
          DEFAULT_DELAY_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  base_transform_class->transform =
      GST_DEBUG_FUNCPTR (gst_tiovx_delay_transform);
  base_transform_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_delay_prepare_output_buffer);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_tiovx_delay_stop);
}

static void
gst_tiovx_delay_init (GstTIOVXDelay * self)
{
  self->delay_size = DEFAULT_DELAY_SIZE;
  self->held_buffers = g_queue_new ();

  return;
}

static void
gst_tiovx_delay_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXDelay *self = GST_TIOVX_DELAY (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_DELAY_SIZE:
      self->delay_size = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_delay_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXDelay *self = GST_TIOVX_DELAY (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_DELAY_SIZE:
      g_value_set_uint (value, self->delay_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_delay_stop (GstBaseTransform * trans)
{
  GstTIOVXDelay *self = GST_TIOVX_DELAY (trans);
  GST_LOG_OBJECT (self, "stop");

  g_queue_clear_full (self->held_buffers, (GDestroyNotify) gst_buffer_unref);
  self->held_buffers = NULL;

  return TRUE;
}

static GstFlowReturn
gst_tiovx_delay_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * input, GstBuffer ** outbuf)
{
  GstTIOVXDelay *self = GST_TIOVX_DELAY (trans);
  gint current_held_buffers = 0;

  current_held_buffers = g_queue_get_length (self->held_buffers);

  if (0 == current_held_buffers) {
    g_queue_push_tail (self->held_buffers, input);
    current_held_buffers++;
  }

  /* We'll fill the queue with the first buffer */
  while (current_held_buffers <= self->delay_size) {
    gst_buffer_ref (input);
    g_queue_push_tail (self->held_buffers, input);

    current_held_buffers++;
  }

  *outbuf = g_queue_pop_head (self->held_buffers);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_tiovx_delay_transform (GstBaseTransform * trans, GstBuffer * input,
    GstBuffer * outbuf)
{
  GST_LOG_OBJECT (trans, "Transform");

  return GST_FLOW_OK;
}
