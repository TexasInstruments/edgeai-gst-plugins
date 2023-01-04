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

#include "gsttiovxmultiscalerpad.h"

#define MIN_ROI_VALUE 0
#define MAX_ROI_VALUE G_MAXUINT32
#define DEFAULT_ROI_VALUE 0

enum
{
  PROP_ROI_STARTX = 1,
  PROP_ROI_STARTY,
  PROP_ROI_WIDTH,
  PROP_ROI_HEIGHT,
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_multiscaler_pad_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_multiscaler_pad_debug_category

G_DEFINE_TYPE_WITH_CODE (GstTIOVXMultiScalerPad, gst_tiovx_multiscaler_pad,
    GST_TYPE_TIOVX_PAD,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_multiscaler_pad_debug_category,
        "tiovxmultiscalerpad", 0,
        "debug category for TIOVX multiscaler pad class"));

static void
gst_tiovx_multiscaler_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_multiscaler_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_tiovx_multiscaler_pad_class_init (GstTIOVXMultiScalerPadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_tiovx_multiscaler_pad_set_property;
  gobject_class->get_property = gst_tiovx_multiscaler_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_ROI_STARTX,
      g_param_spec_uint ("roi-startx", "ROI Start X",
          "Starting X coordinate of the cropped region of interest",
          MIN_ROI_VALUE, MAX_ROI_VALUE, DEFAULT_ROI_VALUE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ROI_STARTY,
      g_param_spec_uint ("roi-starty", "ROI Start Y",
          "Starting Y coordinate of the cropped region of interest",
          MIN_ROI_VALUE, MAX_ROI_VALUE, DEFAULT_ROI_VALUE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ROI_WIDTH,
      g_param_spec_uint ("roi-width", "ROI Width",
          "Width of the cropped region of interest",
          MIN_ROI_VALUE, MAX_ROI_VALUE, DEFAULT_ROI_VALUE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ROI_HEIGHT,
      g_param_spec_uint ("roi-height", "ROI Height",
          "Height of the cropped region of interest",
          MIN_ROI_VALUE, MAX_ROI_VALUE, DEFAULT_ROI_VALUE, G_PARAM_READWRITE));
}

static void
gst_tiovx_multiscaler_pad_init (GstTIOVXMultiScalerPad * self)
{
  GST_DEBUG_OBJECT (self, "gst_tiovx_multiscaler_pad_init");

  self->roi_startx = 0;
  self->roi_starty = 0;
  self->roi_width = 0;
  self->roi_height = 0;
}

static void
gst_tiovx_multiscaler_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMultiScalerPad *self = GST_TIOVX_MULTISCALER_PAD (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_ROI_STARTX:
      self->roi_startx = g_value_get_uint (value);
      break;
    case PROP_ROI_STARTY:
      self->roi_starty = g_value_get_uint (value);
      break;
    case PROP_ROI_WIDTH:
      self->roi_width = g_value_get_uint (value);
      break;
    case PROP_ROI_HEIGHT:
      self->roi_height = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_multiscaler_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMultiScalerPad *self = GST_TIOVX_MULTISCALER_PAD (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_ROI_STARTX:
      g_value_set_uint (value, self->roi_startx);
      break;
    case PROP_ROI_STARTY:
      g_value_set_uint (value, self->roi_starty);
      break;
    case PROP_ROI_WIDTH:
      g_value_set_enum (value, self->roi_width);
      break;
    case PROP_ROI_HEIGHT:
      g_value_set_enum (value, self->roi_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}
