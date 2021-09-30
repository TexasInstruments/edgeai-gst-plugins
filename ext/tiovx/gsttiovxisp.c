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
#include "config.h"
#endif

#include "gsttiovxisp.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_viss_module.h"

/* Properties definition */
enum
{
  PROP_0,
  PROP_DCC_CONFIG_FILE,
};

/* Formats definition */
#define TIOVX_ISP_SUPPORTED_FORMATS_SRC "{ GRAY8, GRAY16_LE, NV12, I420 }"
#define TIOVX_ISP_SUPPORTED_FORMATS_SINK "{ bggr, gbrg, grbg, rggb }"
#define TIOVX_ISP_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_ISP_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TIOVX_ISP_STATIC_CAPS_SRC                            \
  "video/x-raw, "                                            \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SRC ", "  \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT ", "                \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Sink caps */
#define TIOVX_ISP_STATIC_CAPS_SINK                           \
  "video/x-bayer, "                                          \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT ", "                \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_ISP_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_ISP_STATIC_CAPS_SRC)
    );

struct _GstTIOVXISP
{
  GstTIOVXSimo element;
  gint target_id;
  gchar *dcc_config_file;
  SensorObj sensorObj;
  TIOVXVISSModuleObj vissObj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_isp_debug);
#define GST_CAT_DEFAULT gst_tiovx_isp_debug

#define gst_tiovx_isp_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXISP, gst_tiovx_isp,
    GST_TYPE_TIOVX_SIMO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_isp_debug,
        "tiovxisp", 0, "debug category for the tiovxisp element"));

static void gst_tiovx_isp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void gst_tiovx_isp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tiovx_isp_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean gst_tiovx_isp_configure_module (GstTIOVXSimo * simo);

static gboolean gst_tiovx_isp_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    vx_reference * input_ref, vx_reference ** output_refs);

static gboolean gst_tiovx_isp_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static GstCaps *gst_tiovx_isp_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list);

static GstCaps *gst_tiovx_isp_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps);

static GList *gst_tiovx_isp_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list);

static gboolean
gst_tiovx_isp_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction);

static gboolean gst_tiovx_isp_deinit_module (GstTIOVXSimo * simo);

static gboolean
gst_tiovx_isp_set_dcc_file (GstTIOVXISP * src, const gchar * location);

/* Initialize the plugin's class */
static void
gst_tiovx_isp_class_init (GstTIOVXISPClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstTIOVXSimoClass *gsttiovxsimo_class = GST_TIOVX_SIMO_CLASS (klass);
  GstPadTemplate *src_temp = NULL;
  GstPadTemplate *sink_temp = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX ISP",
      "Filter",
      "Image Signal Processing using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  src_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, src_temp);

  sink_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TYPE_TIOVX_PAD);
  gst_element_class_add_pad_template (gstelement_class, sink_temp);

  gobject_class->set_property = gst_tiovx_isp_set_property;
  gobject_class->get_property = gst_tiovx_isp_get_property;

  g_object_class_install_property (gobject_class, PROP_DCC_CONFIG_FILE,
      g_param_spec_string ("dcc-file", "DCC File",
          "TIOVX DCC tuning binary file for the given image sensor",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  gsttiovxsimo_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_init_module);

  gsttiovxsimo_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_configure_module);

  gsttiovxsimo_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_get_node_info);

  gsttiovxsimo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_create_graph);

  gsttiovxsimo_class->get_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_get_sink_caps);

  gsttiovxsimo_class->get_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_get_src_caps);

  gsttiovxsimo_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_fixate_caps);

  gsttiovxsimo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_deinit_module);

  gsttiovxsimo_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_compare_caps);
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_isp_init (GstTIOVXISP * self)
{
  self->dcc_config_file = NULL;
}

static gboolean
gst_tiovx_isp_set_dcc_file (GstTIOVXISP * self, const gchar * location)
{
  GstState state;

  /* the element must be stopped in order to do this */
  state = GST_STATE (self);
  if (state != GST_STATE_READY && state != GST_STATE_NULL) {
    goto wrong_state;
  }

  g_free (self->dcc_config_file);

  /* clear the filename if we get a NULL */
  if (location == NULL) {
    self->dcc_config_file = NULL;
  } else {
    self->dcc_config_file = g_strdup (location);
  }

  return TRUE;

  /* ERROR */
wrong_state:
  GST_WARNING_OBJECT (self,
      "Changing the dcc-file path of the tiovxisp 'on the fly' is not supported");
  return FALSE;
}

static void
gst_tiovx_isp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_CONFIG_FILE:
      gst_tiovx_isp_set_dcc_file (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_isp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_CONFIG_FILE:
      g_value_set_string (value, self->dcc_config_file);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_isp_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads,
    GstCaps * sink_caps, GList * src_caps_list)
{
  return FALSE;
}

static gboolean
gst_tiovx_isp_configure_module (GstTIOVXSimo * simo)
{
  return FALSE;
}

static gboolean
gst_tiovx_isp_get_node_info (GstTIOVXSimo * simo,
    vx_node * node, GstTIOVXPad * sink_pad, GList * src_pads,
    vx_reference * input_ref, vx_reference ** output_refs)
{
  return FALSE;
}

static gboolean
gst_tiovx_isp_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph)
{
  return FALSE;
}

static GstCaps *
gst_tiovx_isp_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list)
{
  return NULL;
}

static GstCaps *
gst_tiovx_isp_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps)
{
  return NULL;
}

static GList *
gst_tiovx_isp_fixate_caps (GstTIOVXSimo * simo,
    GstCaps * sink_caps, GList * src_caps_list)
{
  return NULL;
}

static gboolean
gst_tiovx_isp_compare_caps (GstTIOVXSimo * simo, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  return FALSE;
}

static gboolean
gst_tiovx_isp_deinit_module (GstTIOVXSimo * simo)
{
  return FALSE;
}
