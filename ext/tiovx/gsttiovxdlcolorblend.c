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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttiovxdlcolorblend.h"

struct _GstTIOVXDLColorBlend
{
  GstTIOVXMiso element;
};

/* Formats definition */
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SRC "ANY"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SINK "ANY"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_DIMENSIONS "3"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_DATA_TYPES "[2, 10]"
/* Src caps */
#define TIOVX_DL_COLOR_BLEND_STATIC_CAPS_SRC \
  "video/x-raw, "                           \
  "format = " TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SRC ", "                    \
  "width = " TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT ", "                  \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Sink caps */
#define TIOVX_DL_COLOR_BLEND_STATIC_CAPS_TENSOR_SINK \
  "application/x-tensor-tiovx, "                           \
  "num-dims = " TIOVX_DL_COLOR_BLEND_SUPPORTED_DIMENSIONS ", "                    \
  "data-type = " TIOVX_DL_COLOR_BLEND_SUPPORTED_DATA_TYPES

#define TIOVX_DL_COLOR_BLEND_STATIC_CAPS_IMAGE_SINK \
  "video/x-raw, "                           \
  "format = " TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SINK ", "                   \
  "width = " TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT ", "                  \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Pads definitions */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DL_COLOR_BLEND_STATIC_CAPS_SRC)
    );

static GstStaticPadTemplate tensor_sink_template =
GST_STATIC_PAD_TEMPLATE ("tensor_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DL_COLOR_BLEND_STATIC_CAPS_TENSOR_SINK)
    );
static GstStaticPadTemplate image_sink_template =
GST_STATIC_PAD_TEMPLATE ("image_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DL_COLOR_BLEND_STATIC_CAPS_IMAGE_SINK)
    );

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_dl_color_blend_debug);
#define GST_CAT_DEFAULT gst_tiovx_dl_color_blend_debug

#define gst_tiovx_dl_color_blend_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXDLColorBlend, gst_tiovx_dl_color_blend,
    GST_TIOVX_MISO_TYPE,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_dl_color_blend_debug,
        "tiovxdlcolorblend", 0,
        "debug category for the tiovxdlcolorblend element");
    );

static void gst_tiovx_dl_color_blend_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

static void gst_tiovx_dl_color_blend_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_tiovx_dl_color_blend_init_module (GstTIOVXMiso * miso,
    vx_context context, GList * sink_pads_list, GstPad * src_pad);

static gboolean gst_tiovx_dl_color_blend_create_graph (GstTIOVXMiso * miso,
    vx_context context, vx_graph graph);

static gboolean gst_tiovx_dl_color_blend_get_node_info (GstTIOVXMiso * miso,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node);

static gboolean gst_tiovx_dl_color_blend_configure_module (GstTIOVXMiso * miso);

static gboolean gst_tiovx_dl_color_blend_release_buffer (GstTIOVXMiso * miso);

static gboolean gst_tiovx_dl_color_blend_deinit_module (GstTIOVXMiso * miso);

static GstCaps *gst_tiovx_dl_color_blend_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps);

static gsize gst_tiovx_dl_color_blend_get_size_from_caps (GstTIOVXMiso * miso,
    GstCaps * caps);

static vx_reference
gst_tiovx_dl_color_blend_get_reference_from_caps (GstTIOVXMiso * miso,
    GstCaps * caps);

/* Initialize the plugin's class */
static void
gst_tiovx_dl_color_blend_class_init (GstTIOVXDLColorBlendClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXMisoClass *gsttiovxmiso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxmiso_class = GST_TIOVX_MISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX DL ColorBlend",
      "Filter/Converter/Video",
      "Applies a mask defined by an input tensor over an input image using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  gobject_class->set_property = gst_tiovx_dl_color_blend_set_property;
  gobject_class->get_property = gst_tiovx_dl_color_blend_get_property;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&tensor_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&image_sink_template));

  gsttiovxmiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_init_module);
  gsttiovxmiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_create_graph);
  gsttiovxmiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_get_node_info);
  gsttiovxmiso_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_configure_module);
  gsttiovxmiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_release_buffer);
  gsttiovxmiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_deinit_module);
  gsttiovxmiso_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_fixate_caps);
  gsttiovxmiso_class->get_size_from_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_get_size_from_caps);
  gsttiovxmiso_class->get_reference_from_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_get_reference_from_caps);
}

/* Initialize the new element */
static void
gst_tiovx_dl_color_blend_init (GstTIOVXDLColorBlend * self)
{
}

static void
gst_tiovx_dl_color_blend_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
}

static void
gst_tiovx_dl_color_blend_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
}

static gboolean
gst_tiovx_dl_color_blend_init_module (GstTIOVXMiso * miso,
    vx_context context, GList * sink_pads_list, GstPad * src_pad)
{
  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_color_blend_create_graph (GstTIOVXMiso * miso,
    vx_context context, vx_graph graph)
{
  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_color_blend_get_node_info (GstTIOVXMiso * miso,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node)
{
  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_color_blend_configure_module (GstTIOVXMiso * miso)
{
  g_return_val_if_fail (miso, FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_color_blend_release_buffer (GstTIOVXMiso * miso)
{
  g_return_val_if_fail (miso, FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_color_blend_deinit_module (GstTIOVXMiso * miso)
{
  g_return_val_if_fail (miso, FALSE);

  return FALSE;
}

static GstCaps *
gst_tiovx_dl_color_blend_fixate_caps (GstTIOVXMiso * miso,
    GList * sink_caps_list, GstCaps * src_caps)
{
  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (sink_caps_list, FALSE);
  g_return_val_if_fail (src_caps, FALSE);
  return NULL;
}

static gsize
gst_tiovx_dl_color_blend_get_size_from_caps (GstTIOVXMiso * miso,
    GstCaps * caps)
{
  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (caps, FALSE);

  return 0;
}

static vx_reference
gst_tiovx_dl_color_blend_get_reference_from_caps (GstTIOVXMiso * miso,
    GstCaps * caps)
{
  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (caps, FALSE);

  return NULL;
}
