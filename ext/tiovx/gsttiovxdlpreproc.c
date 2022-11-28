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

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_dl_pre_proc_module.h"

#define DLPREPROC_INPUT_PARAM_INDEX 1
#define DLPREPROC_OUTPUT_PARAM_INDEX 2

#define SCALE_DIM 3
#define MEAN_DIM 3

#define MIN_SCALE 0.0
#define MAX_SCALE 1.0

#define MIN_MEAN 0.0
#define MAX_MEAN 255.0

#define DEFAULT_SCALE 1.0
#define DEFAULT_MEAN 0.0

#define NUM_DIMS_SUPPORTED 3
#define NUM_CHANNELS_SUPPORTED 3

/* Target definition */
#define GST_TYPE_TIOVX_DL_PRE_PROC_TARGET (gst_tiovx_dl_pre_proc_target_get_type())
#define DEFAULT_TIOVX_DL_PRE_PROC_TARGET TIVX_CPU_ID_A72_0

/* Channel order definition */
#define GST_TYPE_TIOVX_DL_PRE_PROC_CHANNEL_ORDER (gst_tiovx_dl_pre_proc_channel_order_get_type())
#define DEFAULT_TIOVX_DL_PRE_PROC_CHANNEL_ORDER TIVX_DL_PRE_PROC_CHANNEL_ORDER_NCHW

/* Data type definition */
#define GST_TYPE_TIOVX_DL_PRE_PROC_DATA_TYPE (gst_tiovx_dl_pre_proc_data_type_get_type())
#define DEFAULT_TIOVX_DL_PRE_PROC_DATA_TYPE VX_TYPE_FLOAT32

/* Tensor format definition */
#define GST_TYPE_TIOVX_DL_PRE_PROC_TENSOR_FORMAT (gst_tiovx_dl_pre_proc_tensor_format_get_type())
#define DEFAULT_TIOVX_DL_PRE_PROC_TENSOR_FORMAT TIVX_DL_PRE_PROC_TENSOR_FORMAT_RGB

/* Formats definition */
#define TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SINK "{RGB, NV12, NV21}"
#define TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_DL_PRE_PROC_SUPPORTED_DIMENSIONS "3"
#define TIOVX_DL_PRE_PROC_SUPPORTED_DATA_TYPES "[2 , 10]"
#define TIOVX_DL_PRE_PROC_SUPPORTED_CHANNEL_ORDER "{NCHW, NHWC}"
#define TIOVX_DL_PRE_PROC_SUPPORTED_TENSOR_FORMAT "{RGB, BGR}"
#define TIOVX_DL_PRE_PROC_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_DL_PRE_PROC_STATIC_CAPS_SRC                             \
  "application/x-tensor-tiovx, "                                      \
  "num-dims = " TIOVX_DL_PRE_PROC_SUPPORTED_DIMENSIONS ", "           \
  "data-type = " TIOVX_DL_PRE_PROC_SUPPORTED_DATA_TYPES ", "          \
  "channel-order = " TIOVX_DL_PRE_PROC_SUPPORTED_CHANNEL_ORDER ", "   \
  "tensor-format = " TIOVX_DL_PRE_PROC_SUPPORTED_TENSOR_FORMAT ", "   \
  "tensor-width = " TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH ", "            \
  "tensor-height = " TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT               \
  "; "                                                                \
  "application/x-tensor-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), " \
  "num-dims = " TIOVX_DL_PRE_PROC_SUPPORTED_DIMENSIONS ", "           \
  "data-type = " TIOVX_DL_PRE_PROC_SUPPORTED_DATA_TYPES ", "          \
  "channel-order = " TIOVX_DL_PRE_PROC_SUPPORTED_CHANNEL_ORDER ", "   \
  "tensor-format = " TIOVX_DL_PRE_PROC_SUPPORTED_TENSOR_FORMAT ", "   \
  "tensor-width = " TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH ", "            \
  "tensor-height = " TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT ", "          \
  "num-channels = " TIOVX_DL_PRE_PROC_SUPPORTED_CHANNELS

/* Sink caps */
#define TIOVX_DL_PRE_PROC_STATIC_CAPS_SINK                           \
  "video/x-raw, "                                                    \
  "format = (string) " TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT ", "                \
  "framerate = " GST_VIDEO_FPS_RANGE                                 \
  "; "                                                               \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "               \
  "format = (string) " TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT ", "                \
  "framerate = " GST_VIDEO_FPS_RANGE ", "                            \
  "num-channels = " TIOVX_DL_PRE_PROC_SUPPORTED_CHANNELS

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
  PROP_SCALE_0,
  PROP_SCALE_1,
  PROP_SCALE_2,
  PROP_MEAN_0,
  PROP_MEAN_1,
  PROP_MEAN_2,
  PROP_CHANNEL_ORDER,
  PROP_DATA_TYPE,
  PROP_TENSOR_FORMAT,
};

static GType
gst_tiovx_dl_pre_proc_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_CPU_ID_A72_0, "A72 instance 1, assigned to A72_0 core",
        TIVX_TARGET_A72_0},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXDLPreProcTarget", targets);
  }
  return target_type;
}

static GType
gst_tiovx_dl_pre_proc_channel_order_get_type (void)
{
  static GType order_type = 0;

  static const GEnumValue channel_orders[] = {
    {TIVX_DL_PRE_PROC_CHANNEL_ORDER_NCHW, "NCHW channel order", "nchw"},
    {TIVX_DL_PRE_PROC_CHANNEL_ORDER_NHWC, "NHWC channel order", "nhwc"},
    {0, NULL, NULL},
  };

  if (!order_type) {
    order_type =
        g_enum_register_static ("GstTIOVXDLPreProcChannelOrder",
        channel_orders);
  }
  return order_type;
}

static GType
gst_tiovx_dl_pre_proc_data_type_get_type (void)
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
        g_enum_register_static ("GstTIOVXDLPreProcDataType", data_types);
  }
  return data_type_type;
}

static GType
gst_tiovx_dl_pre_proc_tensor_format_get_type (void)
{
  static GType tensor_format_type = 0;

  static const GEnumValue tensor_formats[] = {
    {TIVX_DL_PRE_PROC_TENSOR_FORMAT_RGB, "RGB plane format", "rgb"},
    {TIVX_DL_PRE_PROC_TENSOR_FORMAT_BGR, "BGR plane format", "bgr"},
    {0, NULL, NULL},
  };

  if (!tensor_format_type) {
    tensor_format_type =
        g_enum_register_static ("GstTIOVXDLPreProcTensorFormat",
        tensor_formats);
  }
  return tensor_format_type;
}

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

struct _GstTIOVXDLPreProc
{
  GstTIOVXSiso element;
  gint target_id;
  gfloat scale[SCALE_DIM];
  gfloat mean[MEAN_DIM];
  gint channel_order;
  gint tensor_format;
  vx_enum data_type;
  TIOVXDLPreProcModuleObj *obj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_dl_pre_proc_debug);
#define GST_CAT_DEFAULT gst_tiovx_dl_pre_proc_debug

#define gst_tiovx_dl_pre_proc_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXDLPreProc, gst_tiovx_dl_pre_proc, GST_TYPE_TIOVX_SISO);

static void gst_tiovx_dl_pre_proc_finalize (GObject * obj);

static void gst_tiovx_dl_pre_proc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void gst_tiovx_dl_pre_proc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_tiovx_dl_pre_proc_transform_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * filter);

static gboolean gst_tiovx_dl_pre_proc_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels);

static gboolean gst_tiovx_dl_pre_proc_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph);

static gboolean gst_tiovx_dl_pre_proc_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node, guint * input_param_index,
    guint * output_param_index);

static gboolean gst_tiovx_dl_pre_proc_release_buffer (GstTIOVXSiso * trans);

static gboolean gst_tiovx_dl_pre_proc_deinit_module (GstTIOVXSiso * trans,
    vx_context context);

static gboolean gst_tiovx_dl_pre_proc_compare_caps (GstTIOVXSiso * trans,
    GstCaps * caps1, GstCaps * caps2, GstPadDirection direction);

static const gchar *gst_tiovx_dl_pre_proc_get_enum_nickname (GType type,
    gint value_id);

/* Initialize the plugin's class */
static void
gst_tiovx_dl_pre_proc_class_init (GstTIOVXDLPreProcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *gstbasetransform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstTIOVXSisoClass *gsttiovxsiso_class = GST_TIOVX_SISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX DL PreProc",
      "Filter/Converter/Video",
      "Preprocesses a video for conventional deep learning algorithms using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  gobject_class->set_property = gst_tiovx_dl_pre_proc_set_property;
  gobject_class->get_property = gst_tiovx_dl_pre_proc_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_DL_PRE_PROC_TARGET,
          DEFAULT_TIOVX_DL_PRE_PROC_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SCALE_0,
      g_param_spec_float ("scale-0", "Scale 0",
          "Scaling value for the first plane",
          MIN_SCALE, MAX_SCALE, DEFAULT_SCALE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SCALE_1,
      g_param_spec_float ("scale-1", "Scale 1",
          "Scaling value for the second plane",
          MIN_SCALE, MAX_SCALE, DEFAULT_SCALE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SCALE_2,
      g_param_spec_float ("scale-2", "Scale 2",
          "Scaling value for the third plane",
          MIN_SCALE, MAX_SCALE, DEFAULT_SCALE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MEAN_0,
      g_param_spec_float ("mean-0", "Mean 0",
          "Mean pixel to be subtracted for the first plane",
          MIN_MEAN, MAX_MEAN, DEFAULT_MEAN, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_MEAN_1,
      g_param_spec_float ("mean-1", "Mean 1",
          "Mean pixel to be subtracted for the second plane",
          MIN_MEAN, MAX_MEAN, DEFAULT_MEAN, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_MEAN_2,
      g_param_spec_float ("mean-2", "Mean 2",
          "Mean pixel to be subtracted for the third plane",
          MIN_MEAN, MAX_MEAN, DEFAULT_MEAN, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CHANNEL_ORDER,
      g_param_spec_enum ("channel-order", "Channel Order",
          "Channel order for the tensor dimensions",
          GST_TYPE_TIOVX_DL_PRE_PROC_CHANNEL_ORDER,
          DEFAULT_TIOVX_DL_PRE_PROC_CHANNEL_ORDER,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DATA_TYPE,
      g_param_spec_enum ("data-type", "Data Type",
          "Data Type of tensor at the output",
          GST_TYPE_TIOVX_DL_PRE_PROC_DATA_TYPE,
          DEFAULT_TIOVX_DL_PRE_PROC_DATA_TYPE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TENSOR_FORMAT,
      g_param_spec_enum ("tensor-format", "Tensor Format",
          "Tensor format at the output",
          GST_TYPE_TIOVX_DL_PRE_PROC_TENSOR_FORMAT,
          DEFAULT_TIOVX_DL_PRE_PROC_TENSOR_FORMAT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

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
  gsttiovxsiso_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dl_pre_proc_compare_caps);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_dl_pre_proc_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_dl_pre_proc_debug,
      "tiovxdlpreproc", 0, "TIOVX DL Pre Proc element");
}

/* Initialize the new element */
static void
gst_tiovx_dl_pre_proc_init (GstTIOVXDLPreProc * self)
{
  gint i;

  self->obj = g_malloc0 (sizeof (*self->obj));
  self->target_id = DEFAULT_TIOVX_DL_PRE_PROC_TARGET;

  for (i = 0; i < SCALE_DIM; i++) {
    self->scale[i] = DEFAULT_SCALE;
  }
  for (i = 0; i < MEAN_DIM; i++) {
    self->mean[i] = DEFAULT_MEAN;
  }

  self->channel_order = DEFAULT_TIOVX_DL_PRE_PROC_CHANNEL_ORDER;
  self->data_type = DEFAULT_TIOVX_DL_PRE_PROC_DATA_TYPE;
  self->tensor_format = DEFAULT_TIOVX_DL_PRE_PROC_TENSOR_FORMAT;
}

static void
gst_tiovx_dl_pre_proc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXDLPreProc *self = GST_TIOVX_DL_PRE_PROC (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    case PROP_SCALE_0:
      self->scale[0] = g_value_get_float (value);
      break;
    case PROP_SCALE_1:
      self->scale[1] = g_value_get_float (value);
      break;
    case PROP_SCALE_2:
      self->scale[2] = g_value_get_float (value);
      break;
    case PROP_MEAN_0:
      self->mean[0] = g_value_get_float (value);
      break;
    case PROP_MEAN_1:
      self->mean[1] = g_value_get_float (value);
      break;
    case PROP_MEAN_2:
      self->mean[2] = g_value_get_float (value);
      break;
    case PROP_CHANNEL_ORDER:
      self->channel_order = g_value_get_enum (value);
      break;
    case PROP_DATA_TYPE:
      self->data_type = g_value_get_enum (value);
      break;
    case PROP_TENSOR_FORMAT:
      self->tensor_format = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_tiovx_dl_pre_proc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXDLPreProc *self = GST_TIOVX_DL_PRE_PROC (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    case PROP_SCALE_0:
      g_value_set_float (value, self->scale[0]);
      break;
    case PROP_SCALE_1:
      g_value_set_float (value, self->scale[1]);
      break;
    case PROP_SCALE_2:
      g_value_set_float (value, self->scale[2]);
      break;
    case PROP_MEAN_0:
      g_value_set_float (value, self->mean[0]);
      break;
    case PROP_MEAN_1:
      g_value_set_float (value, self->mean[1]);
      break;
    case PROP_MEAN_2:
      g_value_set_float (value, self->mean[2]);
      break;
    case PROP_CHANNEL_ORDER:
      g_value_set_enum (value, self->channel_order);
      break;
    case PROP_DATA_TYPE:
      g_value_set_enum (value, self->data_type);
      break;
    case PROP_TENSOR_FORMAT:
      g_value_set_enum (value, self->tensor_format);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static GstCaps *
gst_tiovx_dl_pre_proc_transform_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIOVXDLPreProc *self = GST_TIOVX_DL_PRE_PROC (base);
  GstCaps *result_caps = NULL;
  GstStructure *result_structure = NULL;
  gchar *channel_order = NULL;
  gchar *tensor_format = NULL;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  if (GST_PAD_SINK == direction) {
    gint i = 0;

    result_caps = gst_caps_from_string (TIOVX_DL_PRE_PROC_STATIC_CAPS_SRC);

    for (i = 0; i < gst_caps_get_size (result_caps); i++) {
      result_structure = gst_caps_get_structure (result_caps, i);

      /* Fixate data type based on property */
      gst_structure_fixate_field_nearest_int (result_structure, "data-type",
          self->data_type);

      /* Fixate channel order based on property */
      channel_order = g_ascii_strup (gst_tiovx_dl_pre_proc_get_enum_nickname
          (gst_tiovx_dl_pre_proc_channel_order_get_type (),
              self->channel_order), -1);
      gst_structure_fixate_field_string (result_structure, "channel-order",
          channel_order);
      g_free (channel_order);

      /* Fixate tensor format based on property */
      tensor_format = g_ascii_strup (gst_tiovx_dl_pre_proc_get_enum_nickname
          (gst_tiovx_dl_pre_proc_tensor_format_get_type (),
              self->tensor_format), -1);
      gst_structure_fixate_field_string (result_structure, "tensor-format",
          tensor_format);
      g_free (tensor_format);

      if (gst_caps_is_fixed (caps)) {
        /* Transfer input image width and height to tensor */
        GstVideoInfo video_info;

        if (!gst_video_info_from_caps (&video_info, caps)) {
          GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
              GST_PTR_FORMAT, caps);
          gst_caps_unref (result_caps);
          return NULL;
        }

        gst_structure_fixate_field_nearest_int (result_structure,
            "tensor-width", GST_VIDEO_INFO_WIDTH (&video_info));

        gst_structure_fixate_field_nearest_int (result_structure,
            "tensor-height", GST_VIDEO_INFO_HEIGHT (&video_info));
      }
    }
  } else {
    result_caps = gst_caps_from_string (TIOVX_DL_PRE_PROC_STATIC_CAPS_SINK);
  }

  if (filter) {
    GstCaps *tmp = result_caps;
    result_caps = gst_caps_intersect (result_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_DEBUG_OBJECT (self, "Resulting caps are %" GST_PTR_FORMAT, result_caps);

  return result_caps;

}

static gboolean
gst_tiovx_dl_pre_proc_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels)
{

  GstTIOVXDLPreProc *self = NULL;
  vx_status status = VX_SUCCESS;
  TIOVXDLPreProcModuleObj *preproc = NULL;
  GstVideoInfo in_info;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (in_caps, FALSE);
  g_return_val_if_fail (out_caps, FALSE);
  g_return_val_if_fail (num_channels >= MIN_NUM_CHANNELS, FALSE);
  g_return_val_if_fail (num_channels <= MAX_NUM_CHANNELS, FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);

  GST_INFO_OBJECT (self, "Init module");

  if (!gst_video_info_from_caps (&in_info, in_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get video info from input caps");
    return FALSE;
  }

  /* Configure PreProcObj */
  preproc = self->obj;
  preproc->params.channel_order = self->channel_order;
  preproc->params.tensor_format = self->tensor_format;

  memcpy (preproc->params.scale, self->scale, sizeof (preproc->params.scale));
  memcpy (preproc->params.mean, self->mean, sizeof (preproc->params.mean));

  GST_DEBUG_OBJECT (self, "Preproc Scale parameters: %f, %f, %f",
      preproc->params.scale[0], preproc->params.scale[1],
      preproc->params.scale[2]);
  GST_DEBUG_OBJECT (self, "Preproc Mean parameters: %f, %f, %f",
      preproc->params.mean[0], preproc->params.mean[1],
      preproc->params.mean[2]);

  /* Configure input */
  preproc->num_channels = num_channels;
  preproc->input.bufq_depth = 1;
  preproc->input.color_format = gst_format_to_vx_format (in_info.finfo->format);
  preproc->input.width = GST_VIDEO_INFO_WIDTH (&in_info);
  preproc->input.height = GST_VIDEO_INFO_HEIGHT (&in_info);

  preproc->input.graph_parameter_index = INPUT_PARAMETER_INDEX;
  preproc->output.graph_parameter_index = OUTPUT_PARAMETER_INDEX;

  /* Configure output */
  preproc->output.bufq_depth = 1;
  preproc->output.datatype = self->data_type;
  preproc->output.num_dims = NUM_DIMS_SUPPORTED;

  GST_DEBUG_OBJECT (self,
      "Configure DLPreproc with \n Width: %d\n Height: %d\n Data type: %d\n Channel order: %d\n Tensor format: %d",
      preproc->input.width, preproc->input.height, preproc->output.datatype,
      preproc->params.channel_order, preproc->params.tensor_format);

  if (TIVX_DL_PRE_PROC_CHANNEL_ORDER_NCHW == self->channel_order) {
    preproc->output.dim_sizes[0] = GST_VIDEO_INFO_WIDTH (&in_info);
    preproc->output.dim_sizes[1] = GST_VIDEO_INFO_HEIGHT (&in_info);
    preproc->output.dim_sizes[2] = NUM_CHANNELS_SUPPORTED;
  } else if (TIVX_DL_PRE_PROC_CHANNEL_ORDER_NHWC == self->channel_order) {
    preproc->output.dim_sizes[0] = NUM_CHANNELS_SUPPORTED;
    preproc->output.dim_sizes[1] = GST_VIDEO_INFO_WIDTH (&in_info);
    preproc->output.dim_sizes[2] = GST_VIDEO_INFO_HEIGHT (&in_info);
  } else {
    GST_ERROR_OBJECT (self, "Invalid channel order selected: %d",
        self->channel_order);
    return FALSE;
  }
  preproc->en_out_tensor_write = 0;

  status = tiovx_dl_pre_proc_module_init (context, preproc);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dl_pre_proc_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph)
{
  GstTIOVXDLPreProc *self = NULL;
  vx_status status = VX_SUCCESS;
  const char *target = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);
  GST_INFO_OBJECT (self, "Create graph");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target =
      gst_tiovx_dl_pre_proc_get_enum_nickname
      (gst_tiovx_dl_pre_proc_target_get_type (), self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (NULL == target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    goto out;
  }

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  status = tiovx_dl_pre_proc_module_create (graph, self->obj, NULL, target);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_dl_pre_proc_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node, guint * input_param_index,
    guint * output_param_index)
{
  GstTIOVXDLPreProc *self = NULL;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);

  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj->node), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj->input.image_handle[0]), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj->output.tensor_handle[0]), FALSE);

  GST_INFO_OBJECT (self, "Get node info from module");

  *node = self->obj->node;
  *input = self->obj->input.arr[0];
  *output = self->obj->output.arr[0];
  *input_ref = (vx_reference) self->obj->input.image_handle[0];
  *output_ref = (vx_reference) self->obj->output.tensor_handle[0];

  *input_param_index = DLPREPROC_INPUT_PARAM_INDEX;
  *output_param_index = DLPREPROC_OUTPUT_PARAM_INDEX;

  return TRUE;
}

static gboolean
gst_tiovx_dl_pre_proc_release_buffer (GstTIOVXSiso * trans)
{
  GstTIOVXDLPreProc *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);
  GST_INFO_OBJECT (self, "Release buffer");

  status = tiovx_dl_pre_proc_module_release_buffers (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dl_pre_proc_deinit_module (GstTIOVXSiso * trans, vx_context context)
{
  GstTIOVXDLPreProc *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);
  GST_INFO_OBJECT (self, "Deinit module");

  status = tiovx_dl_pre_proc_module_delete (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    return FALSE;
  }

  status = tiovx_dl_pre_proc_module_deinit (self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static const gchar *
gst_tiovx_dl_pre_proc_get_enum_nickname (GType type, gint value_id)
{
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, value_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static void
gst_tiovx_dl_pre_proc_finalize (GObject * obj)
{
  GstTIOVXDLPreProc *self = GST_TIOVX_DL_PRE_PROC (obj);

  GST_LOG_OBJECT (self, "finalize");

  g_free (self->obj);

  G_OBJECT_CLASS (gst_tiovx_dl_pre_proc_parent_class)->finalize (obj);
}

static gboolean
gst_tiovx_dl_pre_proc_compare_caps (GstTIOVXSiso * trans, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  GstVideoInfo video_info1;
  GstVideoInfo video_info2;
  gboolean ret = FALSE;

  g_return_val_if_fail (caps1, FALSE);
  g_return_val_if_fail (caps2, FALSE);
  g_return_val_if_fail (GST_PAD_UNKNOWN != direction, FALSE);

  /* Compare image fields is sink pad */
  if (GST_PAD_SINK == direction) {
    if (!gst_video_info_from_caps (&video_info1, caps1)) {
      GST_ERROR_OBJECT (trans, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps1);
      goto out;
    }

    if (!gst_video_info_from_caps (&video_info2, caps2)) {
      GST_ERROR_OBJECT (trans, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps2);
      goto out;
    }

    if ((video_info1.width == video_info2.width) &&
        (video_info1.height == video_info2.height) &&
        (video_info1.finfo->format == video_info2.finfo->format)
        ) {
      ret = TRUE;
    }
  }

  /* Compare tensor fields if src pad */
  if (GST_PAD_SRC == direction) {
    if (gst_caps_is_equal (caps1, caps2)) {
      ret = TRUE;
    }
  }

out:
  return ret;
}
