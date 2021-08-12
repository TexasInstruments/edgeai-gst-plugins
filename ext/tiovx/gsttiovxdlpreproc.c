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

#include "gsttiovxdlpreproc.h"

struct _GstTIOVXDLPreProc
{
  GstTIOVXSiso element;
};

/* Formats definition */
#define TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SRC "{RGB, BGR, NV12}"
#define TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SINK "{RGB, BGR, NV12}"
#define TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TIOVX_DL_PRE_PROC_STATIC_CAPS_SRC \
  "video/x-raw, "                           \
  "format = (string) " TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SRC ", "                    \
  "width = " TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT ", "                  \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Sink caps */
#define TIOVX_DL_PRE_PROC_STATIC_CAPS_SINK \
  "video/x-raw, "                           \
  "format = (string) " TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SINK ", "                   \
  "width = " TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT ", "                  \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Pads definitions */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DL_PRE_PROC_STATIC_CAPS_SRC)
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DL_PRE_PROC_STATIC_CAPS_SINK)
    );

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_dl_pre_proc_debug);
#define GST_CAT_DEFAULT gst_tiovx_dl_pre_proc_debug

#define gst_tiovx_dl_pre_proc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXDLPreProc, gst_tiovx_dl_pre_proc,
    GST_TIOVX_SISO_TYPE, GST_DEBUG_CATEGORY_INIT (gst_tiovx_dl_pre_proc_debug,
        "tiovxdlpreproc", 0, "debug category for the tiovxdlpreproc element"););

static void gst_tiovx_dl_pre_proc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void gst_tiovx_dl_pre_proc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_tiovx_dl_pre_proc_transform_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * filter);

static gboolean gst_tiovx_dl_pre_proc_init_module (GstTIOVXSiso * trans,
    vx_context context, GstVideoInfo * in_info, GstVideoInfo * out_info,
    guint num_channels);

static gboolean gst_tiovx_dl_pre_proc_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph);

static gboolean gst_tiovx_dl_pre_proc_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node);

static gboolean gst_tiovx_dl_pre_proc_release_buffer (GstTIOVXSiso * trans);

static gboolean gst_tiovx_dl_pre_proc_deinit_module (GstTIOVXSiso * trans);

/* Initialize the plugin's class */
static void
gst_tiovx_dl_pre_proc_class_init (GstTIOVXDLPreProcClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstBaseTransformClass *gstbasetransform_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSisoClass *gsttiovxsiso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasetransform_class = GST_BASE_TRANSFORM_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsiso_class = GST_TIOVX_SISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX DL PreProc",
      "Filter/Converter/Video",
      "Preprocesses a video for conventional deep learning algorithms using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  gobject_class->set_property = gst_tiovx_dl_pre_proc_set_property;
  gobject_class->get_property = gst_tiovx_dl_pre_proc_get_property;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_pre_proc_transform_caps);

  gsttiovxsiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_pre_proc_init_module);
  gsttiovxsiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_pre_proc_create_graph);
  gsttiovxsiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_pre_proc_get_node_info);
  gsttiovxsiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_pre_proc_release_buffer);
  gsttiovxsiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_pre_proc_deinit_module);
}

/* Initialize the new element */
static void
gst_tiovx_dl_pre_proc_init (GstTIOVXDLPreProc * self)
{
}

static void
gst_tiovx_dl_pre_proc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
}

static void
gst_tiovx_dl_pre_proc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
}

static GstCaps *
gst_tiovx_dl_pre_proc_transform_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  return NULL;
}

static gboolean
gst_tiovx_dl_pre_proc_init_module (GstTIOVXSiso * trans,
    vx_context context, GstVideoInfo * in_info, GstVideoInfo * out_info,
    guint num_channels)
{
  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);
  g_return_val_if_fail (num_channels >= MIN_NUM_CHANNELS, FALSE);
  g_return_val_if_fail (num_channels <= MAX_NUM_CHANNELS, FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_pre_proc_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph)
{
  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_pre_proc_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node)
{
  g_return_val_if_fail (trans, FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_pre_proc_release_buffer (GstTIOVXSiso * trans)
{
  g_return_val_if_fail (trans, FALSE);

  return FALSE;
}

static gboolean
gst_tiovx_dl_pre_proc_deinit_module (GstTIOVXSiso * trans)
{
  g_return_val_if_fail (trans, FALSE);

  return FALSE;
}
