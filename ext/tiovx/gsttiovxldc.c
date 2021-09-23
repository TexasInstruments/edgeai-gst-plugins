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

#include "gsttiovxldc.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_ldc_module.h"

/* Formats definition */
#define TIOVX_LDC_SUPPORTED_FORMATS_SRC "{ GRAY8, GRAY16_LE, NV12, YUYV, UYVY }"
#define TIOVX_LDC_SUPPORTED_FORMATS_SINK "{ GRAY8, GRAY16_LE, NV12, YUYV, UYVY }"
#define TIOVX_LDC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_LDC_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TIOVX_LDC_STATIC_CAPS_SRC \
  "video/x-raw, "							\
  "format = (string) " TIOVX_LDC_SUPPORTED_FORMATS_SRC ", "					\
  "width = " TIOVX_LDC_SUPPORTED_WIDTH ", "					\
  "height = " TIOVX_LDC_SUPPORTED_HEIGHT ", "					\
  "framerate = " GST_VIDEO_FPS_RANGE

/* Sink caps */
#define TIOVX_LDC_STATIC_CAPS_SINK \
  "video/x-raw, "							\
  "format = (string) " TIOVX_LDC_SUPPORTED_FORMATS_SINK ", "					\
  "width = " TIOVX_LDC_SUPPORTED_WIDTH ", "					\
  "height = " TIOVX_LDC_SUPPORTED_HEIGHT ", "					\
  "framerate = " GST_VIDEO_FPS_RANGE

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_LDC_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_LDC_STATIC_CAPS_SRC)
    );

struct _GstTIOVXLDC
{
  GstTIOVXSimo element;
  gint target_id;
  TIOVXLDCModuleObj obj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_ldc_debug);
#define GST_CAT_DEFAULT gst_tiovx_ldc_debug

#define gst_tiovx_ldc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXLDC, gst_tiovx_ldc,
    GST_TIOVX_SIMO_TYPE, GST_DEBUG_CATEGORY_INIT (gst_tiovx_ldc_debug,
        "tiovxldc", 0, "debug category for the tiovxldc element"););

static gboolean gst_tiovx_ldc_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean gst_tiovx_ldc_configure_module (GstTIOVXSimo * simo);

static gboolean gst_tiovx_ldc_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    vx_reference * input_ref, vx_reference ** output_refs);

static gboolean gst_tiovx_ldc_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static GstCaps *gst_tiovx_ldc_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list);

static GstCaps *gst_tiovx_ldc_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps);

static GList *gst_tiovx_ldc_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean
gst_tiovx_ldc_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction);

static gboolean gst_tiovx_ldc_deinit_module (GstTIOVXSimo * simo);

/* Initialize the plugin's class */
static void
gst_tiovx_ldc_class_init (GstTIOVXLDCClass * klass)
{
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSimoClass *gsttiovxsimo_class = NULL;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsimo_class = GST_TIOVX_SIMO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX LDC",
      "Filter",
      "Lens Distortion Correction using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gsttiovxsimo_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_init_module);

  gsttiovxsimo_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_configure_module);

  gsttiovxsimo_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_node_info);

  gsttiovxsimo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_create_graph);

  gsttiovxsimo_class->get_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_sink_caps);

  gsttiovxsimo_class->get_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_get_src_caps);

  gsttiovxsimo_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_fixate_caps);

  gsttiovxsimo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_deinit_module);

  gsttiovxsimo_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_ldc_compare_caps);
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_ldc_init (GstTIOVXLDC * self)
{

}

static gboolean
gst_tiovx_ldc_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list)
{
  return FALSE;
}

static gboolean
gst_tiovx_ldc_configure_module (GstTIOVXSimo * simo)
{
  return FALSE;
}

static gboolean
gst_tiovx_ldc_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    vx_reference * input_ref, vx_reference ** output_refs)
{
  return FALSE;
}

static gboolean
gst_tiovx_ldc_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph)
{
  return FALSE;
}

static GstCaps *
gst_tiovx_ldc_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list)
{
  return NULL;
}

static GstCaps *
gst_tiovx_ldc_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps)
{
  return NULL;
}

static GList *
gst_tiovx_ldc_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list)
{
  return NULL;
}

static gboolean
gst_tiovx_ldc_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  return FALSE;
}

static gboolean
gst_tiovx_ldc_deinit_module (GstTIOVXSimo * simo)
{
  return FALSE;
}
