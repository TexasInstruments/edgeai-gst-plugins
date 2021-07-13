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

#include "gsttiovxmultiscaler.h"

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>
#include <VX/vx.h>
#include <VX/vx_nodes.h>
#include <VX/vx_types.h>

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_multi_scaler_debug);
#define GST_CAT_DEFAULT gst_tiovx_multi_scaler_debug

#define TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC "{NV12}"
#define TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SINK "{NV12}"

/* Src caps */
#define TIOVX_MULTI_SCALER_STATIC_CAPS_SRC GST_VIDEO_CAPS_MAKE (TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC)

/* Sink caps */
#define TIOVX_MULTI_SCALER_STATIC_CAPS_SINK GST_VIDEO_CAPS_MAKE (TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SINK)

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_MULTI_SCALER_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_MULTI_SCALER_STATIC_CAPS_SRC)
    );

struct _GstTIOVXMultiScaler
{
  GstTIOVXSimo element;
};

#define gst_tiovx_multi_scaler_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXMultiScaler, gst_tiovx_multi_scaler,
    GST_TIOVX_SIMO_TYPE);

static void
gst_tiovx_multi_scaler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_multi_scaler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* Initialize the plugin's class */
static void
gst_tiovx_multi_scaler_class_init (GstTIOVXMultiScalerClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "Multi Scaler",
      "Generic/Filter",
      "Multi scales dimensions using the OVX API",
      "RidgeRun <support@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_tiovx_multi_scaler_set_property;
  gobject_class->get_property = gst_tiovx_multi_scaler_get_property;

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_multi_scaler_debug,
      "tiovxmultiscaler", 0, "debug category for the tiovxmultiscaler element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_multi_scaler_init (GstTIOVXMultiScaler * filter)
{
}

static void
gst_tiovx_multi_scaler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMultiScaler *filter = GST_TIOVX_MULTI_SCALER (object);

  GST_LOG_OBJECT (filter, "set_property");

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tiovx_multi_scaler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMultiScaler *filter = GST_TIOVX_MULTI_SCALER (object);

  GST_LOG_OBJECT (filter, "get_property");

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
