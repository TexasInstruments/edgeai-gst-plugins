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

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxmiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_dl_color_blend_module.h"


#define NUM_INPUT_IMAGES 1
#define MIN_INPUT_TENSORS 1

#define NUM_DIMS_SUPPORTED 3
#define TENSOR_CHANNELS_SUPPORTED 1

#define DEFAULT_USE_COLOR_MAP 0

/* Target definition */
#define GST_TIOVX_TYPE_DL_COLOR_BLEND_TARGET (gst_tiovx_dl_color_blend_target_get_type())
#define DEFAULT_TIOVX_DL_COLOR_BLEND_TARGET TIVX_CPU_ID_DSP1

/* Data type definition */
#define GST_TIOVX_TYPE_DL_COLOR_BLEND_DATA_TYPE (gst_tiovx_dl_color_blend_data_type_get_type())
#define DEFAULT_TIOVX_DL_COLOR_BLEND_DATA_TYPE VX_TYPE_FLOAT32

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
  "data-type = " TIOVX_DL_COLOR_BLEND_SUPPORTED_DATA_TYPES

#define TIOVX_DL_COLOR_BLEND_STATIC_CAPS_IMAGE_SINK \
  "video/x-raw, "                           \
  "format = " TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SINK ", "                   \
  "width = " TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT ", "                  \
  "framerate = " GST_VIDEO_FPS_RANGE

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
  PROP_DATA_TYPE,
};

static GType
gst_tiovx_dl_color_blend_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_CPU_ID_DSP1, "DSP instance 1, assigned to C66_0 core",
        TIVX_TARGET_DSP1},
    {TIVX_CPU_ID_DSP2, "DSP instance 2, assigned to C66_1 core",
        TIVX_TARGET_DSP2},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type =
        g_enum_register_static ("GstTIOVXDLColorBlendTarget", targets);
  }
  return target_type;
}

static GType
gst_tiovx_dl_color_blend_data_type_get_type (void)
{
  static GType data_type_type = 0;

  static const GEnumValue data_types[] = {
    {VX_TYPE_INT8, "VX_TYPE_INT8", "int8"},
    {VX_TYPE_UINT8, "VX_TYPE_UINT8", "uint8"},
    {VX_TYPE_INT16, "VX_TYPE_INT16", "int16"},
    {VX_TYPE_UINT16, "VX_TYPE_UINT16", "uint16"},
    {VX_TYPE_INT32, "VX_TYPE_INT32", "int32"},
    {VX_TYPE_UINT32, "VX_TYPE_UINT32", "uint32"},
    {VX_TYPE_FLOAT32, "VX_TYPE_FLOAT32", "float32"},
    {0, NULL, NULL},
  };

  if (!data_type_type) {
    data_type_type =
        g_enum_register_static ("GstTIOVXDLColorBlendDataType", data_types);
  }
  return data_type_type;
}

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

struct _GstTIOVXDLColorBlend
{
  GstTIOVXMiso element;
  gint target_id;
  vx_enum data_type;
  TIOVXDLColorBlendModuleObj *obj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_dl_color_blend_debug);
#define GST_CAT_DEFAULT gst_tiovx_dl_color_blend_debug

#define gst_tiovx_dl_color_blend_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXDLColorBlend, gst_tiovx_dl_color_blend,
    GST_TIOVX_MISO_TYPE,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_dl_color_blend_debug,
        "tiovxdlcolorblend", 0,
        "debug category for the tiovxdlcolorblend element");
    );

static void gst_tiovx_dl_color_blend_finalize (GObject * obj);

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

static gboolean gst_tiovx_dl_color_blend_deinit_module (GstTIOVXMiso * miso,
    vx_context context);

static GstCaps *gst_tiovx_dl_color_blend_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps);

static const gchar *target_id_to_target_name (gint target_id);

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

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TIOVX_TYPE_DL_COLOR_BLEND_TARGET,
          DEFAULT_TIOVX_DL_COLOR_BLEND_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DATA_TYPE,
      g_param_spec_enum ("data-type", "Data Type",
          "Data Type of tensor at the output",
          GST_TIOVX_TYPE_DL_COLOR_BLEND_DATA_TYPE,
          DEFAULT_TIOVX_DL_COLOR_BLEND_DATA_TYPE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

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

  gobject_class->finalize =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_finalize);
}

/* Initialize the new element */
static void
gst_tiovx_dl_color_blend_init (GstTIOVXDLColorBlend * self)
{
  self->obj = g_malloc0 (sizeof (*self->obj));
  self->target_id = DEFAULT_TIOVX_DL_COLOR_BLEND_TARGET;
  self->data_type = DEFAULT_TIOVX_DL_COLOR_BLEND_DATA_TYPE;
}

static void
gst_tiovx_dl_color_blend_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXDLColorBlend *self = GST_TIOVX_DL_COLOR_BLEND (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    case PROP_DATA_TYPE:
      self->data_type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_tiovx_dl_color_blend_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXDLColorBlend *self = GST_TIOVX_DL_COLOR_BLEND (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    case PROP_DATA_TYPE:
      g_value_set_enum (value, self->data_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static gboolean
gst_tiovx_dl_color_blend_init_module (GstTIOVXMiso * miso,
    vx_context context, GList * sink_pads_list, GstPad * src_pad)
{
  GstTIOVXDLColorBlend *self = NULL;
  TIOVXDLColorBlendModuleObj *colorblend = NULL;
  GstStructure *structure = NULL;
  GstCaps *caps = NULL;
  GList *l = NULL;
  GstVideoInfo video_info = { };
  vx_status status = VX_SUCCESS;
  gboolean video_caps_found = FALSE;
  gboolean ret = FALSE;
  gint num_tensors = 0;
  gint i = 0;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (NUM_INPUT_IMAGES + MIN_INPUT_TENSORS >
      g_list_length (sink_pads_list), FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);

  GST_INFO_OBJECT (self, "Init module");

  num_tensors = g_list_length (sink_pads_list) - NUM_INPUT_IMAGES;

  /* Configure TIOVXColorBlendModuleObj */
  colorblend = self->obj;
  colorblend->num_channels = DEFAULT_NUM_CHANNELS;
  colorblend->params.num_outputs = num_tensors;
  colorblend->params.use_color_map = DEFAULT_USE_COLOR_MAP;

  for (l = sink_pads_list; l != NULL; l = g_list_next (l)) {
    caps = gst_pad_get_current_caps (GST_PAD (l->data));

    if (gst_video_info_from_caps (&video_info, caps)) {
      colorblend->img_input.bufq_depth = DEFAULT_NUM_CHANNELS;
      colorblend->img_input.color_format =
          gst_format_to_vx_format (video_info.finfo->format);
      colorblend->img_input.width = GST_VIDEO_INFO_WIDTH (&video_info);
      colorblend->img_input.height = GST_VIDEO_INFO_HEIGHT (&video_info);
      colorblend->img_input.graph_parameter_index = INPUT_PARAMETER_INDEX;
      video_caps_found = TRUE;
      gst_caps_unref (caps);
      break;
    }
    gst_caps_unref (caps);
  }

  if (!video_caps_found) {
    GST_ERROR_OBJECT (self, "Failed to get video compatible caps");
    goto out;
  }

  for (l = sink_pads_list; l != NULL; l = g_list_next (l)) {
    caps = gst_pad_get_current_caps (GST_PAD (l->data));
    structure = gst_caps_get_structure (caps, 0);

    if (gst_structure_has_name (structure, "application/x-tensor-tiovx")) {
      colorblend->tensor_input.bufq_depth = DEFAULT_NUM_CHANNELS;
      colorblend->tensor_input.datatype = self->data_type;
      colorblend->tensor_input.num_dims = NUM_DIMS_SUPPORTED;
      colorblend->tensor_input.dim_sizes[0] =
          GST_VIDEO_INFO_WIDTH (&video_info);
      colorblend->tensor_input.dim_sizes[1] =
          GST_VIDEO_INFO_HEIGHT (&video_info);
      colorblend->tensor_input.dim_sizes[2] = TENSOR_CHANNELS_SUPPORTED;
      colorblend->tensor_input.graph_parameter_index = i;

      colorblend->img_outputs[i].bufq_depth = DEFAULT_NUM_CHANNELS;
      colorblend->img_outputs[i].color_format =
          gst_format_to_vx_format (video_info.finfo->format);
      colorblend->img_outputs[i].width = GST_VIDEO_INFO_WIDTH (&video_info);
      colorblend->img_outputs[i].height = GST_VIDEO_INFO_HEIGHT (&video_info);
      colorblend->img_outputs[i].graph_parameter_index = i + num_tensors;
      i++;

    } else if (!gst_video_info_from_caps (&video_info, caps)) {
      /* TODO This allows for multiple video pads but only uses the first one */
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps);
      gst_caps_unref (caps);
      goto out;
    }
    gst_caps_unref (caps);
  }

  /* Initialize modules */
  status = tiovx_dl_color_blend_module_init (context, colorblend);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_dl_color_blend_create_graph (GstTIOVXMiso * miso,
    vx_context context, vx_graph graph)
{
  GstTIOVXDLColorBlend *self = NULL;
  vx_status status = VX_SUCCESS;
  const char *target = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);
  GST_INFO_OBJECT (self, "Create graph");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (!target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    g_return_val_if_reached (FALSE);
  }

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  status =
      tiovx_dl_color_blend_module_create (graph, self->obj, NULL, NULL, target);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_dl_color_blend_get_node_info (GstTIOVXMiso * miso,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node)
{
  GstTIOVXDLColorBlend *self = NULL;
  GList *l = NULL;
  GstCaps *caps = NULL;
  GstStructure *structure = NULL;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);

  for (l = sink_pads_list; l; l = l->next) {
    GstAggregatorPad *pad = l->data;
    caps = gst_pad_get_current_caps (GST_PAD (pad));
    structure = gst_caps_get_structure (caps, 0);

    if (gst_structure_has_name (structure, "application/x-tensor-tiovx")) {
      gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (pad),
          (vx_reference *) & self->obj->tensor_input.tensor_handle[0],
          self->obj->tensor_input.graph_parameter_index);
    } else {
      gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (pad),
          (vx_reference *) & self->obj->img_input.image_handle[0],
          self->obj->img_input.graph_parameter_index);
    }
    gst_caps_unref (caps);
  }
  *node = self->obj->node;

  return TRUE;
}

static gboolean
gst_tiovx_dl_color_blend_configure_module (GstTIOVXMiso * miso)
{
  return TRUE;
}

static gboolean
gst_tiovx_dl_color_blend_release_buffer (GstTIOVXMiso * miso)
{
  GstTIOVXDLColorBlend *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (miso, FALSE);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);
  GST_INFO_OBJECT (self, "Release buffer");

  status = tiovx_dl_color_blend_module_release_buffers (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dl_color_blend_deinit_module (GstTIOVXMiso * miso, vx_context context)
{
  GstTIOVXDLColorBlend *self = NULL;
  vx_status status = VX_SUCCESS;
  gboolean ret = FALSE;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);
  GST_INFO_OBJECT (self, "Deinit module");

  status = tiovx_dl_color_blend_module_delete (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    goto out;
  }

  status = tiovx_dl_color_blend_module_deinit (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    goto out;
  }

  tivxImgProcUnLoadKernels (context);
  ret = TRUE;

out:
  return ret;
}

static GstCaps *
gst_tiovx_dl_color_blend_fixate_caps (GstTIOVXMiso * miso,
    GList * sink_caps_list, GstCaps * src_caps)
{
  GstCaps *caps = NULL;
  GstCaps *output_caps = NULL;
  GList *l = NULL;
  GstVideoInfo video_info = { };
  gboolean video_caps_found = FALSE;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (sink_caps_list, FALSE);
  g_return_val_if_fail (src_caps, FALSE);

  GST_INFO_OBJECT (miso, "Fixating caps");

  for (l = sink_caps_list; l != NULL; l = g_list_next (l)) {
    caps = gst_pad_get_current_caps (GST_PAD (l->data));

    if (gst_video_info_from_caps (&video_info, caps)) {
      gst_caps_unref (caps);
      video_caps_found = TRUE;
      break;
    }
    gst_caps_unref (caps);
  }

  if (video_caps_found) {
    output_caps = gst_video_info_to_caps (&video_info);
  } else {
    GST_ERROR_OBJECT (miso, "Video info not found in sink caps");
  }

  return output_caps;
}

static const gchar *
target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_dl_color_blend_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static void
gst_tiovx_dl_color_blend_finalize (GObject * obj)
{
  GstTIOVXDLColorBlend *self = GST_TIOVX_DL_COLOR_BLEND (obj);

  GST_LOG_OBJECT (self, "finalize");

  g_free (self->obj);

  G_OBJECT_CLASS (gst_tiovx_dl_color_blend_parent_class)->finalize (obj);
}
