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
 * *	Any redistribution and use are licensed by TI for use only with TI Devices.
 * 
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 * 
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
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

#include "gsttiovxpyramid.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_pyramid_module.h"

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
};

/* Formats definition */
#define TIOVX_PYRAMID_SUPPORTED_FORMATS "{GRAY8, GRAY16_LE}"
#define TIOVX_PYRAMID_SUPPORTED_WIDTH "[1 , 1088]"
#define TIOVX_PYRAMID_SUPPORTED_HEIGHT "[1 , 1920]"
#define TIOVX_PYRAMID_SUPPORTED_LEVELS "[1 , 32]"
#define TIOVX_PYRAMID_SUPPORTED_SCALE "[1/4 , 1/1]"
#define TIOVX_PYRAMID_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_PYRAMID_STATIC_CAPS_SRC                                   \
  "application/x-pyramid-tiovx, "                                       \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "             \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "                         \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "                       \
  "levels = " TIOVX_PYRAMID_SUPPORTED_LEVELS ", "                       \
  "scale = " TIOVX_PYRAMID_SUPPORTED_SCALE                              \
  "; "                                                                  \
  "application/x-pyramid-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "  \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "             \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "                         \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "                       \
  "levels = " TIOVX_PYRAMID_SUPPORTED_LEVELS ", "                       \
  "scale = " TIOVX_PYRAMID_SUPPORTED_SCALE ", "                         \
  "num-channels = " TIOVX_PYRAMID_SUPPORTED_CHANNELS \

/* Sink caps */
#define TIOVX_PYRAMID_STATIC_CAPS_SINK                       \
  "video/x-raw, "                                            \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "  \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "              \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "            \
  "framerate = " GST_VIDEO_FPS_RANGE                         \
  "; "                                                       \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "       \
  "format = (string) " TIOVX_PYRAMID_SUPPORTED_FORMATS ", "  \
  "width = " TIOVX_PYRAMID_SUPPORTED_WIDTH ", "              \
  "height = " TIOVX_PYRAMID_SUPPORTED_HEIGHT ", "            \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                    \
  "num-channels = " TIOVX_PYRAMID_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_PYRAMID_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_PYRAMID_STATIC_CAPS_SRC)
    );

struct _GstTIOVXPyramid
{
  GstTIOVXSiso element;
  TIOVXPyramidModuleObj obj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_pyramid_debug);
#define GST_CAT_DEFAULT gst_tiovx_pyramid_debug

#define gst_tiovx_pyramid_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXPyramid, gst_tiovx_pyramid, GST_TYPE_TIOVX_SISO);

static gboolean gst_tiovx_pyramid_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels);
static gboolean gst_tiovx_pyramid_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_pyramid_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node,
    guint * input_param_index, guint * output_param_index);
static gboolean gst_tiovx_pyramid_release_buffer (GstTIOVXSiso * trans);
static gboolean gst_tiovx_pyramid_deinit_module (GstTIOVXSiso * trans,
    vx_context context);
static gboolean gst_tiovx_pyramid_compare_caps (GstTIOVXSiso * trans,
    GstCaps * caps1, GstCaps * caps2, GstPadDirection direction);

/* Initialize the plugin's class */
static void
gst_tiovx_pyramid_class_init (GstTIOVXPyramidClass * klass)
{
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSisoClass *gsttiovxsiso_class = NULL;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsiso_class = GST_TIOVX_SISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Pyramid",
      "Filter/Converter/Video",
      "Converts video frames to a vx_pyramid representation using the TIOVX Modules API",
      "RidgeRun support@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gsttiovxsiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_init_module);
  gsttiovxsiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_create_graph);
  gsttiovxsiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_get_node_info);
  gsttiovxsiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_release_buffer);
  gsttiovxsiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_deinit_module);
  gsttiovxsiso_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_pyramid_compare_caps);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_pyramid_debug,
      "tiovxpyramid", 0, "TIOVX Pyramid element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_pyramid_init (GstTIOVXPyramid * self)
{
  memset (&self->obj, 0, sizeof self->obj);
}


/* GstTIOVXSiso Functions */

static gboolean
gst_tiovx_pyramid_init_module (GstTIOVXSiso * trans, vx_context context,
    GstCaps * in_caps, GstCaps * out_caps, guint num_channels)
{
  return TRUE;
}

static gboolean
gst_tiovx_pyramid_create_graph (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph)
{
  return TRUE;
}

static gboolean
gst_tiovx_pyramid_release_buffer (GstTIOVXSiso * trans)
{
  return TRUE;
}

static gboolean
gst_tiovx_pyramid_deinit_module (GstTIOVXSiso * trans, vx_context context)
{
  return TRUE;
}

static gboolean
gst_tiovx_pyramid_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node,
    guint * input_param_index, guint * output_param_index)
{
  return TRUE;
}

static gboolean
gst_tiovx_pyramid_compare_caps (GstTIOVXSiso * trans, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  return TRUE;
}
