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

/* Node Params indexes */
#define DLCOLORBLEND_INPUT_IMAGE_NODE_PARAM_INDEX 1
#define DLCOLORBLEND_INPUT_TENSOR_NODE_PARAM_INDEX 2
#define DLCOLORBLEND_OUTPUT_IMAGE_NODE_PARAM_INDEX 3
/* Graph Params indexes */
#define DLCOLORBLEND_INPUT_IMAGE_GRAPH_PARAM_INDEX 0
#define DLCOLORBLEND_INPUT_TENSOR_GRAPH_PARAM_INDEX 1
#define DLCOLORBLEND_OUTPUT_IMAGE_GRAPH_PARAM_INDEX 2

#define MAX_SINK_PADS 2

#define NUM_DIMS_SUPPORTED 3
#define TENSOR_CHANNELS_SUPPORTED 1

#define DEFAULT_USE_COLOR_MAP 0
#define DEFAULT_NUM_CLASSES 8

/* Target definition */
#define GST_TYPE_TIOVX_DL_COLOR_BLEND_TARGET (gst_tiovx_dl_color_blend_target_get_type())
#define DEFAULT_TIOVX_DL_COLOR_BLEND_TARGET TIVX_CPU_ID_A72_0

/* Data type definition */
#define GST_TYPE_TIOVX_DL_COLOR_BLEND_DATA_TYPE (gst_tiovx_dl_color_blend_data_type_get_type())
#define DEFAULT_TIOVX_DL_COLOR_BLEND_DATA_TYPE VX_TYPE_FLOAT32

/* Formats definition */
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SRC "{RGB, NV12}"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SINK "{RGB, NV12}"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_DIMENSIONS "3"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_DATA_TYPES "[2, 10]"
#define TIOVX_DL_COLOR_BLEND_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_DL_COLOR_BLEND_STATIC_CAPS_IMAGE                         \
  "video/x-raw, "                                                      \
  "format = (string) " TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT ", "               \
  "framerate = " GST_VIDEO_FPS_RANGE                                   \
  "; "                                                                 \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "                 \
  "format = (string) " TIOVX_DL_COLOR_BLEND_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT ", "               \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                              \
  "num-channels = " TIOVX_DL_COLOR_BLEND_SUPPORTED_CHANNELS

/* Sink caps */
#define TIOVX_DL_COLOR_BLEND_STATIC_CAPS_TENSOR_SINK                  \
  "application/x-tensor-tiovx, "                                      \
  "num-dims = " TIOVX_DL_COLOR_BLEND_SUPPORTED_DIMENSIONS ", "        \
  "tensor-width = " TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH ", "         \
  "tensor-height = " TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT ", "       \
  "data-type = " TIOVX_DL_COLOR_BLEND_SUPPORTED_DATA_TYPES            \
  "; "                                                                \
  "application/x-tensor-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), " \
  "num-dims = " TIOVX_DL_COLOR_BLEND_SUPPORTED_DIMENSIONS ", "        \
  "tensor-width = " TIOVX_DL_COLOR_BLEND_SUPPORTED_WIDTH ", "         \
  "tensor-height = " TIOVX_DL_COLOR_BLEND_SUPPORTED_HEIGHT ", "       \
  "data-type = " TIOVX_DL_COLOR_BLEND_SUPPORTED_DATA_TYPES ", "       \
  "num-channels = " TIOVX_DL_COLOR_BLEND_SUPPORTED_CHANNELS

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
  PROP_DATA_TYPE,
  PROP_NUM_CLASSES,
};

static GType
gst_tiovx_dl_color_blend_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_CPU_ID_A72_0, "A72 instance 1, assigned to A72_0 core",
        TIVX_TARGET_A72_0},
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
    GST_STATIC_CAPS (TIOVX_DL_COLOR_BLEND_STATIC_CAPS_IMAGE)
    );

static GstStaticPadTemplate tensor_sink_template =
GST_STATIC_PAD_TEMPLATE ("tensor",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_DL_COLOR_BLEND_STATIC_CAPS_TENSOR_SINK)
    );
static GstStaticPadTemplate image_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_DL_COLOR_BLEND_STATIC_CAPS_IMAGE)
    );

struct _GstTIOVXDLColorBlend
{
  GstTIOVXMiso element;
  gint target_id;
  vx_enum data_type;
  guint num_classes;
  TIOVXDLColorBlendModuleObj *obj;
  GstPad *image_pad;
  GstPad *tensor_pad;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_dl_color_blend_debug);
#define GST_CAT_DEFAULT gst_tiovx_dl_color_blend_debug

#define gst_tiovx_dl_color_blend_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXDLColorBlend, gst_tiovx_dl_color_blend,
    GST_TYPE_TIOVX_MISO,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_dl_color_blend_debug,
        "tiovxdlcolorblend", 0,
        "debug category for the tiovxdlcolorblend element"););

static void gst_tiovx_dl_color_blend_finalize (GObject * obj);

static void gst_tiovx_dl_color_blend_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

static void gst_tiovx_dl_color_blend_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_tiovx_dl_color_blend_init_module (GstTIOVXMiso * miso,
    vx_context context, GList * sink_pads_list, GstPad * src_pad,
    guint num_channels);

static gboolean gst_tiovx_dl_color_blend_create_graph (GstTIOVXMiso * miso,
    vx_context context, vx_graph graph);

static gboolean gst_tiovx_dl_color_blend_get_node_info (GstTIOVXMiso * miso,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects);

static gboolean gst_tiovx_dl_color_blend_configure_module (GstTIOVXMiso * miso);

static gboolean gst_tiovx_dl_color_blend_release_buffer (GstTIOVXMiso * miso);

static gboolean gst_tiovx_dl_color_blend_deinit_module (GstTIOVXMiso * miso);

static GstCaps *gst_tiovx_dl_color_blend_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels);

static const gchar *target_id_to_target_name (gint target_id);

static GstPad *gst_tiovx_dl_color_blend_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);

/* Initialize the plugin's class */
static void
gst_tiovx_dl_color_blend_class_init (GstTIOVXDLColorBlendClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstTIOVXMisoClass *gsttiovxmiso_class = GST_TIOVX_MISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX DL ColorBlend",
      "Filter/Converter/Video",
      "Applies a mask defined by an input tensor over an input image using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  gobject_class->set_property = gst_tiovx_dl_color_blend_set_property;
  gobject_class->get_property = gst_tiovx_dl_color_blend_get_property;

  g_object_class_install_property (gobject_class, PROP_NUM_CLASSES,
      g_param_spec_uint ("num-classes", "Number of classes",
          "Number of classes in mask", 0,
          G_MAXUINT, DEFAULT_NUM_CLASSES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_DL_COLOR_BLEND_TARGET,
          DEFAULT_TIOVX_DL_COLOR_BLEND_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DATA_TYPE,
      g_param_spec_enum ("data-type", "Data Type",
          "Data Type of tensor at the output",
          GST_TYPE_TIOVX_DL_COLOR_BLEND_DATA_TYPE,
          DEFAULT_TIOVX_DL_COLOR_BLEND_DATA_TYPE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &src_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &tensor_sink_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &image_sink_template, GST_TYPE_TIOVX_MISO_PAD);

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

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_color_blend_request_new_pad);

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
  self->num_classes = DEFAULT_NUM_CLASSES;
  self->image_pad = NULL;
  self->tensor_pad = NULL;
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
    case PROP_NUM_CLASSES:
      self->num_classes = g_value_get_uint (value);
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
    case PROP_NUM_CLASSES:
      g_value_set_uint (value, self->num_classes);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static gboolean
gst_tiovx_dl_color_blend_init_module (GstTIOVXMiso * miso,
    vx_context context, GList * sink_pads_list, GstPad * src_pad,
    guint num_channels)
{
  GstTIOVXDLColorBlend *self = NULL;
  TIOVXDLColorBlendModuleObj *colorblend = NULL;
  GstStructure *tensor_structure = NULL;
  GstCaps *tensor_caps = NULL;
  GstCaps *image_caps = NULL;
  GstCaps *src_caps = NULL;
  GstVideoInfo video_info = { };
  vx_status status = VX_SUCCESS;
  gboolean ret = FALSE;
  gint tensor_width = 0;
  gint tensor_height = 0;
  gint tensor_data_type = 0;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (MAX_SINK_PADS >= g_list_length (sink_pads_list), FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);

  GST_INFO_OBJECT (self, "Init module");

  /* Configure TIOVXColorBlendModuleObj */
  colorblend = self->obj;
  colorblend->num_channels = num_channels;
  colorblend->en_out_image_write = 0;
  colorblend->params.use_color_map = DEFAULT_USE_COLOR_MAP;
  colorblend->params.num_classes = self->num_classes;

  /* Configure module's tensor input */
  tensor_caps = gst_pad_get_current_caps (self->tensor_pad);
  tensor_structure = gst_caps_get_structure (tensor_caps, 0);
  if (!gst_structure_get_int (tensor_structure, "tensor-width", &tensor_width)) {
    GST_ERROR_OBJECT (self, "tensor-width not found in tensor caps");
    goto out;
  }

  if (!gst_structure_get_int (tensor_structure, "tensor-height",
          &tensor_height)) {
    GST_ERROR_OBJECT (self, "tensor-height not found in tensor caps");
    goto out;
  }

  if (!gst_structure_get_int (tensor_structure, "data-type", &tensor_data_type)) {
    GST_ERROR_OBJECT (self, "data-type not found in tensor caps");
    goto out;
  }

  if (tensor_data_type != self->data_type) {
    GST_ERROR_OBJECT (self,
        "Caps data type (%d) different than property data type (%d), aborting initialization",
        tensor_data_type, self->data_type);
    goto out;
  }

  colorblend->tensor_input.bufq_depth = 1;
  colorblend->tensor_input.datatype = self->data_type;
  colorblend->tensor_input.num_dims = NUM_DIMS_SUPPORTED;
  colorblend->tensor_input.dim_sizes[0] = tensor_width;
  colorblend->tensor_input.dim_sizes[1] = tensor_height;
  colorblend->tensor_input.dim_sizes[2] = TENSOR_CHANNELS_SUPPORTED;
  colorblend->tensor_input.graph_parameter_index =
      DLCOLORBLEND_INPUT_TENSOR_GRAPH_PARAM_INDEX;

  GST_INFO_OBJECT (self,
      "Configure input tensor with: \n  Data Type: %d\n  Width: %d\n  Height: %d\n  Graph Index: %d\n  Channels: %d",
      colorblend->tensor_input.datatype,
      colorblend->tensor_input.dim_sizes[0],
      colorblend->tensor_input.dim_sizes[1],
      colorblend->tensor_input.graph_parameter_index,
      colorblend->tensor_input.bufq_depth);

  /* Configure module's image input */
  image_caps = gst_pad_get_current_caps (self->image_pad);
  if (!gst_video_info_from_caps (&video_info, image_caps)) {
    GST_ERROR_OBJECT (self, "failed to get caps from image sink pad");
    goto out;
  }
  colorblend->img_input.bufq_depth = 1;
  colorblend->img_input.color_format =
      gst_format_to_vx_format (video_info.finfo->format);
  colorblend->img_input.width = GST_VIDEO_INFO_WIDTH (&video_info);
  colorblend->img_input.height = GST_VIDEO_INFO_HEIGHT (&video_info);
  colorblend->img_input.graph_parameter_index =
      DLCOLORBLEND_INPUT_IMAGE_GRAPH_PARAM_INDEX;

  GST_INFO_OBJECT (self,
      "Configure input image with: \n  Color Format: %d\n  Width: %d\n  Height: %d\n  Graph Index: %d\n  Channels: %d",
      colorblend->img_input.color_format, colorblend->img_input.width,
      colorblend->img_input.height,
      colorblend->img_input.graph_parameter_index,
      colorblend->img_input.bufq_depth);

  /* Configure module's output */
  src_caps = gst_pad_get_current_caps (src_pad);
  if (!src_caps) {
    GST_ERROR_OBJECT (self, "Failed to get caps from src pad");
    goto out;
  }

  if (!gst_video_info_from_caps (&video_info, src_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get video info from src pad caps");
    goto out;
  }

  colorblend->img_output.bufq_depth = 1;
  colorblend->img_output.color_format =
      gst_format_to_vx_format (video_info.finfo->format);
  colorblend->img_output.width = GST_VIDEO_INFO_WIDTH (&video_info);
  colorblend->img_output.height = GST_VIDEO_INFO_HEIGHT (&video_info);
  colorblend->img_output.graph_parameter_index =
      DLCOLORBLEND_OUTPUT_IMAGE_GRAPH_PARAM_INDEX;

  GST_INFO_OBJECT (self,
      "Configure output image with: \n  Color Format: %d\n  Width: %d\n  Height: %d\n  Graph Index: %d\n  Channels: %d",
      colorblend->img_output.color_format, colorblend->img_output.width,
      colorblend->img_output.height,
      colorblend->img_output.graph_parameter_index,
      colorblend->img_output.bufq_depth);

  /* Initialize module */
  status = tiovx_dl_color_blend_module_init (context, colorblend);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  if (src_caps) {
    gst_caps_unref (src_caps);
  }
  if (tensor_caps) {
    gst_caps_unref (tensor_caps);
  }
  if (image_caps) {
    gst_caps_unref (image_caps);
  }

  return ret;
}

static gboolean
gst_tiovx_dl_color_blend_create_graph (GstTIOVXMiso * miso,
    vx_context context, vx_graph graph)
{
  GstTIOVXDLColorBlend *self = NULL;
  vx_status status = VX_SUCCESS;
  const gchar *target = NULL;
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

  if (NULL == target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    goto out;
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
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects)
{
  GstTIOVXDLColorBlend *self = NULL;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);

  /* Configure GstTIOVXPad for inputs */
  gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (self->tensor_pad),
      self->obj->tensor_input.arr[0],
      (vx_reference *) & self->obj->tensor_input.tensor_handle[0],
      self->obj->tensor_input.graph_parameter_index,
      DLCOLORBLEND_INPUT_TENSOR_NODE_PARAM_INDEX);

  gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (self->image_pad),
      self->obj->img_input.arr[0],
      (vx_reference *) & self->obj->img_input.image_handle[0],
      self->obj->img_input.graph_parameter_index,
      DLCOLORBLEND_INPUT_IMAGE_NODE_PARAM_INDEX);

  /* Configure GstTIOVXPad for output */
  gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (src_pad),
      self->obj->img_output.arr[0],
      (vx_reference *) & self->obj->img_output.image_handle[0],
      self->obj->img_output.graph_parameter_index,
      DLCOLORBLEND_OUTPUT_IMAGE_NODE_PARAM_INDEX);

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
gst_tiovx_dl_color_blend_deinit_module (GstTIOVXMiso * miso)
{
  GstTIOVXDLColorBlend *self = NULL;
  vx_status status = VX_SUCCESS;
  gboolean ret = TRUE;

  g_return_val_if_fail (miso, FALSE);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);
  GST_INFO_OBJECT (self, "Deinit module");

  status = tiovx_dl_color_blend_module_delete (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    ret = FALSE;
  }

  status = tiovx_dl_color_blend_module_deinit (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    ret = FALSE;
  }

  return ret;
}

static GstCaps *
gst_tiovx_dl_color_blend_fixate_caps (GstTIOVXMiso * miso,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels)
{
  GstTIOVXDLColorBlend *self = NULL;
  GstCaps *image_caps = NULL;
  GstCaps *tensor_caps = NULL;
  GstCaps *tensor_caps_copy = NULL;
  GstCaps *tensor_intersected_caps = NULL;
  GstCaps *output_caps = NULL;
  gint i = 0;

  g_return_val_if_fail (miso, NULL);
  g_return_val_if_fail (sink_caps_list, NULL);
  g_return_val_if_fail (src_caps, NULL);
  g_return_val_if_fail (num_channels, NULL);

  self = GST_TIOVX_DL_COLOR_BLEND (miso);
  GST_INFO_OBJECT (miso, "Fixating caps");

  tensor_caps = gst_pad_get_current_caps (self->tensor_pad);
  tensor_caps_copy = gst_caps_copy (tensor_caps);

  gst_caps_unref (tensor_caps);
  for (i = 0; i < gst_caps_get_size (tensor_caps_copy); i++) {
    GstStructure *structure = gst_caps_get_structure (tensor_caps_copy, i);

    /* Remove everything except for number of channels */
    gst_structure_remove_fields (structure, "num-dims", "data-type",
        "channel-order", "tensor-format", "tensor-width", "tensor-height",
        NULL);

    /* Set the name to video/x-raw in order to intersect */
    gst_structure_set_name (structure, "video/x-raw");
  }

  tensor_intersected_caps = gst_caps_intersect (tensor_caps_copy, src_caps);
  gst_caps_unref (tensor_caps_copy);

  image_caps = gst_pad_get_current_caps (self->image_pad);
  output_caps = gst_caps_intersect (image_caps, tensor_intersected_caps);
  gst_caps_unref (image_caps);

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

static GstPad *
gst_tiovx_dl_color_blend_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstPad *pad = NULL;
  GstTIOVXDLColorBlend *self = GST_TIOVX_DL_COLOR_BLEND (element);

  pad =
      GST_ELEMENT_CLASS (gst_tiovx_dl_color_blend_parent_class)->request_new_pad
      (element, templ, name, caps);

  if (0 == g_strcmp0 (templ->name_template, image_sink_template.name_template)) {
    self->image_pad = pad;
  } else if (0 == g_strcmp0 (templ->name_template,
          tensor_sink_template.name_template)) {
    self->tensor_pad = pad;
  }

  return pad;
}
