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

#include "app_scaler_module.h"
#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_multi_scaler_debug);
#define GST_CAT_DEFAULT gst_tiovx_multi_scaler_debug

#define TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC "{NV12}"
#define TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SINK "{NV12}"

/* Src caps */
#define TIOVX_MULTI_SCALER_STATIC_CAPS_SRC GST_VIDEO_CAPS_MAKE (TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SRC)

/* Sink caps */
#define TIOVX_MULTI_SCALER_STATIC_CAPS_SINK GST_VIDEO_CAPS_MAKE (TIOVX_MULTI_SCALER_SUPPORTED_FORMATS_SINK)

#define DEFAULT_PROP_TARGET "TIVX_TARGET_VPAC_MSC1"
#define DEFAULT_PROP_MIN_NUM_CHANNELS 1
#define DEFAULT_PROP_MAX_NUM_CHANNELS 5
#define DEFAULT_PROP_NUM_CHANNELS 2

#define DEFAULT_PROP_MIN_NUM_OUTPUTS 1
#define DEFAULT_PROP_MAX_NUM_OUTPUTS 5
#define DEFAULT_PROP_NUM_OUTPUTS 2

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DEFAULT_TARGET,
  PROP_NUM_CHANNELS,
  PROP_NUM_OUTPUTS,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

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
  ScalerObj *scaler_obj;
  vx_graph graph;

  const gchar *default_target;
  gint num_channels;
  gint num_outputs;
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

static gboolean gst_tiovx_multi_scaler_init_module (GstTIOVXSimo * simo,
    vx_context context, GstCaps * sink_caps, GList * src_caps_list,
    guint in_pool_size, GHashTable * out_pool_sizes);

static gboolean gst_tiovx_multi_scaler_configure_module (GstTIOVXSimo * simo);

static gboolean gst_tiovx_multi_scaler_create_graph (GstTIOVXSimo * simo,
    vx_context context, vx_graph graph);

static gboolean gst_tiovx_multi_scaler_deinit_module (GstTIOVXSimo * simo);

/* Initialize the plugin's class */
static void
gst_tiovx_multi_scaler_class_init (GstTIOVXMultiScalerClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSimoClass *tiovx_simo_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  tiovx_simo_class = GST_TIOVX_SIMO_CLASS (klass);

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

  tiovx_simo_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_init_module);

  tiovx_simo_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_configure_module);

  tiovx_simo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_create_graph);

  tiovx_simo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_multi_scaler_deinit_module);

  obj_properties[PROP_DEFAULT_TARGET] =
      g_param_spec_string ("default-target", "TIVX default target",
      "TIVX default target", "TIVX_TARGET_VPAC_MSC1",
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  obj_properties[PROP_NUM_CHANNELS] =
      g_param_spec_int ("num-channels", "Multi Scaler number of channels",
      "Multi Scaler number of channels", DEFAULT_PROP_MIN_NUM_CHANNELS,
      DEFAULT_PROP_MAX_NUM_CHANNELS, DEFAULT_PROP_NUM_CHANNELS,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  obj_properties[PROP_NUM_OUTPUTS] =
      g_param_spec_int ("num-outputs", "Multi Scaler number of outputs",
      "Multi Scaler number of outputs", DEFAULT_PROP_MIN_NUM_OUTPUTS,
      DEFAULT_PROP_MAX_NUM_OUTPUTS, DEFAULT_PROP_NUM_OUTPUTS,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (gobject_class, N_PROPERTIES,
      obj_properties);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_multi_scaler_debug,
      "tiovxmultiscaler", 0, "debug category for the tiovxmultiscaler element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_multi_scaler_init (GstTIOVXMultiScaler * self)
{
  self->scaler_obj = g_malloc0 (sizeof (ScalerObj));

  self->default_target = DEFAULT_PROP_TARGET;
  self->num_channels = DEFAULT_PROP_NUM_CHANNELS;
  self->num_outputs = DEFAULT_PROP_NUM_OUTPUTS;
}

static void
gst_tiovx_multi_scaler_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXMultiScaler *self = GST_TIOVX_MULTI_SCALER (object);

  GST_LOG_OBJECT (self, "set_property");

  switch (prop_id) {
    case PROP_DEFAULT_TARGET:
      self->default_target = g_value_get_string (value);
      break;
    case PROP_NUM_CHANNELS:
      self->num_channels = g_value_get_int (value);
      break;
    case PROP_NUM_OUTPUTS:
      self->num_outputs = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tiovx_multi_scaler_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXMultiScaler *self = GST_TIOVX_MULTI_SCALER (object);

  GST_LOG_OBJECT (self, "get_property");

  switch (prop_id) {
    case PROP_DEFAULT_TARGET:
      g_value_set_string (value, self->default_target);
      break;
    case PROP_NUM_CHANNELS:
      g_value_set_int (value, self->num_channels);
      break;
    case PROP_NUM_OUTPUTS:
      g_value_set_int (value, self->num_outputs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_tiovx_multi_scaler_init_module (GstTIOVXSimo * simo, vx_context context,
    GstCaps * sink_caps, GList * src_caps_list, guint in_pool_size,
    GHashTable * out_pool_sizes)
{
  GstTIOVXMultiScaler *self = NULL;
  ScalerObj *multiscaler = NULL;
  vx_status status = VX_SUCCESS;
  gpointer g_size = 0;
  guint out_pool_size_ = 0;
  guint i = 0;
  gboolean ret = TRUE;
  GstCaps *src_caps = NULL;
  GstVideoInfo in_info = { };
  GstVideoInfo out_info = { };

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_caps, FALSE);
  g_return_val_if_fail (src_caps_list, FALSE);
  g_return_val_if_fail ((in_pool_size >= MIN_POOL_SIZE), FALSE);
  g_return_val_if_fail ((in_pool_size <= MAX_POOL_SIZE), FALSE);
  g_return_val_if_fail (out_pool_sizes, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);

  /* Initialize the input parameters */
  multiscaler = self->scaler_obj;
  multiscaler->num_ch = (vx_int32) self->num_channels;
  multiscaler->num_outputs = (vx_int32) self->num_outputs;

  gst_video_info_from_caps (&in_info, sink_caps);
  multiscaler->input.width = (vx_int32) GST_VIDEO_INFO_WIDTH ((&in_info));

  multiscaler->input.height = (vx_int32) GST_VIDEO_INFO_HEIGHT ((&in_info));
  multiscaler->input.color_format =
      gst_tiovx_utils_map_gst_video_format_to_vx_format (in_info.finfo->format);

  multiscaler->method = VX_INTERPOLATION_BILINEAR;
  multiscaler->input.bufq_depth = (vx_int32) in_pool_size;

  /* Initialize the output parameters */
  for (i = 0; i < multiscaler->num_outputs; i++) {
    src_caps = g_list_next (src_caps_list)->data;
    gst_video_info_from_caps (&out_info, src_caps);


    multiscaler->output[i].width =
        (vx_int32) GST_VIDEO_INFO_WIDTH ((&out_info));
    multiscaler->output[i].height =
        (vx_int32) GST_VIDEO_INFO_HEIGHT ((&out_info));
    multiscaler->output[i].color_format =
        gst_tiovx_utils_map_gst_video_format_to_vx_format (out_info.
        finfo->format);


    if (g_hash_table_contains (out_pool_sizes, GUINT_TO_POINTER (i))) {
      g_size = g_hash_table_lookup (out_pool_sizes, GUINT_TO_POINTER (i));
      out_pool_size_ = GPOINTER_TO_UINT (g_size);
      if (0 >= out_pool_size_) {
        GST_ERROR_OBJECT (self,
            ("Module init failed to get hashed out pool size"));
        ret = FALSE;
        goto out;
      }
    }

    multiscaler->output[i].bufq_depth = out_pool_size_;
  }

  status = app_init_scaler (context, multiscaler);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

out:
  return ret;
}

static gboolean
gst_tiovx_multi_scaler_configure_module (GstTIOVXSimo * simo)
{
  GstTIOVXMultiScaler *self = NULL;

  vx_status status = VX_SUCCESS;
  gboolean ret = TRUE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);

  status = vxVerifyGraph (self->graph);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure verify graph failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  status = app_update_scaler_filter_coeffs (self->scaler_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure filter coefficients failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  status = app_release_buffer_scaler (self->scaler_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure release buffer failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

out:
  return ret;
}

static gboolean
gst_tiovx_multi_scaler_create_graph (GstTIOVXSimo * simo, vx_context context,
    vx_graph graph)
{
  GstTIOVXMultiScaler *self = NULL;
  ScalerObj *multiscaler = NULL;
  vx_status status = VX_SUCCESS;
  gboolean ret = TRUE;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);

  status =
      app_create_graph_scaler (context, graph, multiscaler, NULL,
      self->default_target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

  self->graph = graph;

out:
  return ret;
}

static gboolean
gst_tiovx_multi_scaler_deinit_module (GstTIOVXSimo * simo)
{
  GstTIOVXMultiScaler *self = NULL;
  ScalerObj *multiscaler = NULL;
  vx_status status = VX_SUCCESS;
  gboolean ret = TRUE;

  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_MULTI_SCALER (simo);
  multiscaler = self->scaler_obj;

  /* Delete graph */
  status = app_delete_scaler (multiscaler);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    ret = FALSE;
    goto out;
  }

  status = app_deinit_scaler (multiscaler);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

out:
  g_free (self->scaler_obj);

  return ret;
}
