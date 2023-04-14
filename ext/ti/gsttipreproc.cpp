/*
 * Copyright (c) [2023] Texas Instruments Incorporated
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

extern "C"
{
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttipreproc.h"
}

#include <edgeai_dl_pre_proc_armv8_utils.h>

#include <ti_pre_process_config.h>

#define SCALE_DIM 3
#define MEAN_DIM 3

#define MIN_SCALE 0.0
#define MAX_SCALE 1.0

#define MIN_MEAN 0.0
#define MAX_MEAN 255.0

#define DEFAULT_SCALE 1.0
#define DEFAULT_MEAN 0.0

/* Channel order definition */
#define GST_TYPE_TI_PRE_PROC_CHANNEL_ORDER (gst_ti_pre_proc_channel_order_get_type())
#define DEFAULT_TI_PRE_PROC_CHANNEL_ORDER DL_PRE_PROC_ARMV8_CHANNEL_ORDER_NCHW

/* Data type definition */
#define GST_TYPE_TI_PRE_PROC_DATA_TYPE (gst_ti_pre_proc_data_type_get_type())
#define DEFAULT_TI_PRE_PROC_DATA_TYPE 0x00A

/* Tensor format definition */
#define GST_TYPE_TI_PRE_PROC_TENSOR_FORMAT (gst_ti_pre_proc_tensor_format_get_type())
#define DEFAULT_TI_PRE_PROC_TENSOR_FORMAT DL_PRE_PROC_ARMV8_TENSOR_FORMAT_RGB

/* Formats definition */
#define TI_PRE_PROC_SUPPORTED_FORMATS_SINK "{RGB, NV12}"
#define TI_PRE_PROC_SUPPORTED_WIDTH "[1 , 8192]"
#define TI_PRE_PROC_SUPPORTED_HEIGHT "[1 , 8192]"
#define TI_PRE_PROC_SUPPORTED_DIMENSIONS "3"
#define TI_PRE_PROC_SUPPORTED_DATA_TYPES "[2 , 10]"
#define TI_PRE_PROC_SUPPORTED_CHANNEL_ORDER "{NCHW, NHWC}"
#define TI_PRE_PROC_SUPPORTED_TENSOR_FORMAT "{RGB, BGR}"
#define TI_PRE_PROC_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TI_PRE_PROC_STATIC_CAPS_SRC                             \
  "application/x-tensor-tiovx, "                                \
  "num-dims = " TI_PRE_PROC_SUPPORTED_DIMENSIONS ", "           \
  "data-type = " TI_PRE_PROC_SUPPORTED_DATA_TYPES ", "          \
  "channel-order = " TI_PRE_PROC_SUPPORTED_CHANNEL_ORDER ", "   \
  "tensor-format = " TI_PRE_PROC_SUPPORTED_TENSOR_FORMAT ", "   \
  "tensor-width = " TI_PRE_PROC_SUPPORTED_WIDTH ", "            \
  "tensor-height = " TI_PRE_PROC_SUPPORTED_HEIGHT
/* Sink caps */
#define TI_PRE_PROC_STATIC_CAPS_SINK                           \
  "video/x-raw, "                                              \
  "format = (string) " TI_PRE_PROC_SUPPORTED_FORMATS_SINK ", " \
  "width = " TI_PRE_PROC_SUPPORTED_WIDTH ", "                  \
  "height = " TI_PRE_PROC_SUPPORTED_HEIGHT ", "                \
  "framerate = " GST_VIDEO_FPS_RANGE                                 

using namespace
    ti::pre_process;

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
  PROP_MODEL,
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
gst_ti_pre_proc_channel_order_get_type (void)
{
  static GType order_type = 0;

  static const GEnumValue channel_orders[] = {
    {DL_PRE_PROC_ARMV8_CHANNEL_ORDER_NCHW, "NCHW channel order", "nchw"},
    {DL_PRE_PROC_ARMV8_CHANNEL_ORDER_NHWC, "NHWC channel order", "nhwc"},
    {0, NULL, NULL},
  };

  if (!order_type) {
    order_type =
        g_enum_register_static ("GstTIPreProcChannelOrder",
        channel_orders);
  }
  return order_type;
}

static GType
gst_ti_pre_proc_data_type_get_type (void)
{
  static GType data_type_type = 0;

  static const GEnumValue data_types[] = {
    {0x002, "TYPE_INT8", "int8"},
    {0x003, "TYPE_UINT8", "uint8"},
    {0x004, "TYPE_INT16", "int16"},
    {0x005, "TYPE_UINT16", "uint16"},
    {0x006, "TYPE_INT32", "int32"},
    {0x007, "TYPE_UINT32", "uint32"},
    {0x00A, "TYPE_FLOAT32", "float32"},
    {0, NULL, NULL},
  };

  if (!data_type_type) {
    data_type_type =
        g_enum_register_static ("GstTIPreProcDataType", data_types);
  }
  return data_type_type;
}

static GType
gst_ti_pre_proc_tensor_format_get_type (void)
{
  static GType tensor_format_type = 0;

  static const GEnumValue tensor_formats[] = {
    {DL_PRE_PROC_ARMV8_TENSOR_FORMAT_RGB, "RGB plane format", "rgb"},
    {DL_PRE_PROC_ARMV8_TENSOR_FORMAT_BGR, "BGR plane format", "bgr"},
    {0, NULL, NULL},
  };

  if (!tensor_format_type) {
    tensor_format_type =
        g_enum_register_static ("GstTIPreProcTensorFormat",
        tensor_formats);
  }
  return tensor_format_type;
}

/* Pads definitions */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_PRE_PROC_STATIC_CAPS_SRC)
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_PRE_PROC_STATIC_CAPS_SINK)
    );


struct _GstTIPreProc
{
  GstBaseTransform
      element;
  GstVideoInfo in_info;
  GstVideoInfo out_info;
  gsize
    out_buffer_size;
  gboolean
    parse_in_video_meta;
  gchar *
    model;
  PreprocessImageConfig *
    pre_proc_config;
  gfloat
    scale[SCALE_DIM];
  gfloat
    mean[MEAN_DIM];
  gint
    channel_order;
  gint
    tensor_format;
  gint
    data_type;
  gint
    tensor_width;
  gint
    tensor_height;
  dlPreProcessImageParams *
    pre_proc_image_params;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_pre_proc_debug);
#define GST_CAT_DEFAULT gst_ti_pre_proc_debug

#define gst_ti_pre_proc_parent_class parent_class
G_DEFINE_TYPE (GstTIPreProc, gst_ti_pre_proc, GST_TYPE_BASE_TRANSFORM);

static void
gst_ti_pre_proc_finalize (GObject * obj);
static void
gst_ti_pre_proc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_ti_pre_proc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static 
    GstCaps *
gst_ti_pre_proc_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static
    gboolean
gst_ti_pre_proc_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static
    gboolean
gst_ti_pre_proc_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize);
static
    GstFlowReturn
gst_ti_pre_proc_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static const
    gchar *
gst_ti_pre_proc_get_enum_nickname (GType type,
    gint value_id);
static void
gst_ti_pre_proc_parse_model (GstTIPreProc * self);

/* Initialize the plugin's class */
static void
gst_ti_pre_proc_class_init (GstTIPreProcClass * klass)
{
  GObjectClass *
      gobject_class = NULL;
  GstBaseTransformClass *
      gstbasetransform_class = NULL;
  GstElementClass *
      gstelement_class = NULL;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  gst_element_class_set_details_simple (gstelement_class,
      "TI Pre Proc",
      "Filter/Converter/Video",
      "Preprocesses a video for conventional deep learning algorithms using the using the ARM Neon Kernels",
      "Abhay Chirania <a-chirania@ti.com>");
  
  gobject_class->set_property = gst_ti_pre_proc_set_property;
  gobject_class->get_property = gst_ti_pre_proc_get_property;

  g_object_class_install_property (gobject_class, PROP_MODEL,
      g_param_spec_string ("model", "Model Directory",
          "TIDL Model directory with params, model and artifacts",
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_SCALE_0,
      g_param_spec_float ("scale-0", "Scale 0",
          "Scaling value for the first plane",
          MIN_SCALE, MAX_SCALE, DEFAULT_SCALE, (GParamFlags) (G_PARAM_READWRITE)));
  g_object_class_install_property (gobject_class, PROP_SCALE_1,
      g_param_spec_float ("scale-1", "Scale 1",
          "Scaling value for the second plane",
          MIN_SCALE, MAX_SCALE, DEFAULT_SCALE, (GParamFlags) (G_PARAM_READWRITE)));
  g_object_class_install_property (gobject_class, PROP_SCALE_2,
      g_param_spec_float ("scale-2", "Scale 2",
          "Scaling value for the third plane",
          MIN_SCALE, MAX_SCALE, DEFAULT_SCALE, (GParamFlags) (G_PARAM_READWRITE)));

  g_object_class_install_property (gobject_class, PROP_MEAN_0,
      g_param_spec_float ("mean-0", "Mean 0",
          "Mean pixel to be subtracted for the first plane",
          MIN_MEAN, MAX_MEAN, DEFAULT_MEAN, (GParamFlags) (G_PARAM_READWRITE)));
  g_object_class_install_property (gobject_class, PROP_MEAN_1,
      g_param_spec_float ("mean-1", "Mean 1",
          "Mean pixel to be subtracted for the second plane",
          MIN_MEAN, MAX_MEAN, DEFAULT_MEAN, (GParamFlags) (G_PARAM_READWRITE)));
  g_object_class_install_property (gobject_class, PROP_MEAN_2,
      g_param_spec_float ("mean-2", "Mean 2",
          "Mean pixel to be subtracted for the third plane",
          MIN_MEAN, MAX_MEAN, DEFAULT_MEAN, (GParamFlags) (G_PARAM_READWRITE)));

  g_object_class_install_property (gobject_class, PROP_CHANNEL_ORDER,
      g_param_spec_enum ("channel-order", "Channel Order",
          "Channel order for the tensor dimensions",
          GST_TYPE_TI_PRE_PROC_CHANNEL_ORDER,
          DEFAULT_TI_PRE_PROC_CHANNEL_ORDER,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DATA_TYPE,
      g_param_spec_enum ("data-type", "Data Type",
          "Data Type of tensor at the output",
          GST_TYPE_TI_PRE_PROC_DATA_TYPE,
          DEFAULT_TI_PRE_PROC_DATA_TYPE,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_TENSOR_FORMAT,
      g_param_spec_enum ("tensor-format", "Tensor Format",
          "Tensor format at the output",
          GST_TYPE_TI_PRE_PROC_TENSOR_FORMAT,
          DEFAULT_TI_PRE_PROC_TENSOR_FORMAT,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS)));


  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));
  
  gstbasetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_ti_pre_proc_set_caps);
  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_ti_pre_proc_transform_caps);
  gstbasetransform_class->transform_size =
      GST_DEBUG_FUNCPTR (gst_ti_pre_proc_transform_size);
  gstbasetransform_class->transform =
      GST_DEBUG_FUNCPTR (gst_ti_pre_proc_transform);
  
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_pre_proc_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_pre_proc_debug,
      "tipreproc", 0, "TI Pre Proc");

}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_ti_pre_proc_init (GstTIPreProc * self)
{
  guint i;

  GST_LOG_OBJECT (self, "init");
  
  self->out_buffer_size = 0;
  self->parse_in_video_meta = TRUE;
  self->model = NULL;
  self->pre_proc_config = NULL;
  self->pre_proc_image_params = NULL;

  for (i = 0; i < SCALE_DIM; i++) {
    self->scale[i] = DEFAULT_SCALE;
  }
  for (i = 0; i < MEAN_DIM; i++) {
    self->mean[i] = DEFAULT_MEAN;
  }

  self->channel_order = DEFAULT_TI_PRE_PROC_CHANNEL_ORDER;
  self->data_type = DEFAULT_TI_PRE_PROC_DATA_TYPE;
  self->tensor_format = DEFAULT_TI_PRE_PROC_TENSOR_FORMAT;
  self->tensor_width = -1;
  self->tensor_height = -1;
  return;
}

static void
gst_ti_pre_proc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIPreProc *self = GST_TI_PRE_PROC (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_MODEL:
      self->model = g_value_dup_string (value);
      gst_ti_pre_proc_parse_model (self);
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
gst_ti_pre_proc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIPreProc *self = GST_TI_PRE_PROC (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_MODEL:
      g_value_set_string (value, self->model);
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
gst_ti_pre_proc_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIPreProc *self = GST_TI_PRE_PROC (trans);
  GstCaps *result_caps = NULL;
  GstStructure *result_structure = NULL;
  gchar *channel_order = NULL;
  gchar *tensor_format = NULL;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  if (GST_PAD_SINK == direction) {
    guint i = 0;

    result_caps = gst_caps_from_string (TI_PRE_PROC_STATIC_CAPS_SRC);

    for (i = 0; i < gst_caps_get_size (result_caps); i++) {
      result_structure = gst_caps_get_structure (result_caps, i);

      /* Fixate data type based on property */
      gst_structure_fixate_field_nearest_int (result_structure, "data-type",
          self->data_type);

      /* Fixate channel order based on property */
      channel_order = g_ascii_strup (gst_ti_pre_proc_get_enum_nickname
          (gst_ti_pre_proc_channel_order_get_type (),
              self->channel_order), -1);
      gst_structure_fixate_field_string (result_structure, "channel-order",
          channel_order);
      g_free (channel_order);

      /* Fixate tensor format based on property */
      tensor_format = g_ascii_strup (gst_ti_pre_proc_get_enum_nickname
          (gst_ti_pre_proc_tensor_format_get_type (),
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
    result_caps = gst_caps_from_string (TI_PRE_PROC_STATIC_CAPS_SINK);
  }

  if (filter) {
    GstCaps *tmp = result_caps;
    result_caps = gst_caps_intersect (result_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_DEBUG_OBJECT (self, "Resulting caps are %" GST_PTR_FORMAT, result_caps);

  return result_caps;

}

static
    gboolean
gst_ti_pre_proc_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize)
{
  GstTIPreProc *
      self = GST_TI_PRE_PROC (trans);
  gboolean ret;

  GST_LOG_OBJECT (self, "transform_size");

  if (GST_PAD_SINK == direction) {
    *othersize = self->out_buffer_size;
    ret = TRUE;

  } else {
    ret = GST_BASE_TRANSFORM_CLASS (gst_ti_pre_proc_parent_class)->transform_size (trans, direction, caps, size, othercaps, othersize);
  }
  return ret;
}

static void
gst_ti_pre_proc_finalize (GObject * obj)
{
  GstTIPreProc *
      self = GST_TI_PRE_PROC (obj);
  GST_LOG_OBJECT (self, "finalize");

  if (self->pre_proc_config) {
    delete self->pre_proc_config;
  }

  G_OBJECT_CLASS (gst_ti_pre_proc_parent_class)->finalize (obj);
}


static const gchar *
gst_ti_pre_proc_get_enum_nickname (GType type, gint value_id)
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
gst_ti_pre_proc_parse_model (GstTIPreProc * self)
{
  guint status = -1;
  guint i;

  GST_LOG_OBJECT (self, "parse_model");

  if (self->pre_proc_config == NULL) {
    self->pre_proc_config = new PreprocessImageConfig;

    // Get Pre Process Config
    status = self->pre_proc_config->getConfig (self->model);
    if (status < 0) {
      GST_ERROR_OBJECT (self, "Getting Pre Proc config failed");
      return;
    }

    for (i = 0;i < SCALE_DIM && i <  self->pre_proc_config->scale.size(); i++ )
    {
      self->scale[i] = self->pre_proc_config->scale[i];
    }
    for (i = 0;i < MEAN_DIM && i <  self->pre_proc_config->mean.size(); i++ )
    {
      self->mean[i] = self->pre_proc_config->mean[i];
    }

    if (self->pre_proc_config->dataLayout == "NCHW") {
      self->channel_order = 0;
      if (self->pre_proc_config->inputTensorShapes.size() > 0
          &&
          self->pre_proc_config->inputTensorShapes[0].size() >= 3) {
        self->tensor_height = self->pre_proc_config->inputTensorShapes[0][2];
        self->tensor_width = self->pre_proc_config->inputTensorShapes[0][3];
      }
    } else if (self->pre_proc_config->dataLayout == "NHWC") {
      self->channel_order = 1;
      if (self->pre_proc_config->inputTensorShapes.size() > 0
          &&
          self->pre_proc_config->inputTensorShapes[0].size() >= 2) {
        self->tensor_height = self->pre_proc_config->inputTensorShapes[0][1];
        self->tensor_width = self->pre_proc_config->inputTensorShapes[0][2];
      }
    }

    if (self->pre_proc_config->reverseChannel) {
      self->tensor_format = 1;
    } else {
      self->tensor_format= 0;
    }

    self->data_type = self->pre_proc_config->inputTensorTypes[0];

  }
}

static
    gboolean
gst_ti_pre_proc_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstTIPreProc *
      self = GST_TI_PRE_PROC (trans);
  GstVideoInfo in_video_info;
  GstStructure *output_structure;
  gint out_caps_width = 0;
  gint out_caps_height = 0;
  gint out_caps_num_dims = 0;
  gint out_caps_data_type = 0;
  guint i;

  if (self->pre_proc_image_params == NULL) {
    self->pre_proc_image_params = new dlPreProcessImageParams;
  }

  if (!gst_video_info_from_caps (&in_video_info, incaps)) {
    GST_WARNING_OBJECT (self,
        "Failed to get info from input cap");
    return FALSE;
  }

  self->in_info = in_video_info;

  output_structure = gst_caps_get_structure (outcaps, 0);
  if (!output_structure) {
    GST_WARNING_OBJECT (self,
        "Failed to get structure from output cap");
    return FALSE;
  }

  if (!gst_structure_get_int (output_structure, "tensor-width", &out_caps_width)) {
    GST_ERROR_OBJECT (self, "tensor-width not found in tensor caps");
    return FALSE;
  }
  if (!gst_structure_get_int (output_structure, "tensor-height", &out_caps_height)) {
    GST_ERROR_OBJECT (self, "tensor-height not found in tensor caps");
    return FALSE;
  }
  if (!gst_structure_get_int (output_structure, "num-dims", &out_caps_num_dims)) {
    GST_ERROR_OBJECT (self, "num-dims not found in tensor caps");
    return FALSE;
  }
  if (!gst_structure_get_int (output_structure, "data-type", &out_caps_data_type)) {
    GST_ERROR_OBJECT (self, "num-dims not found in tensor caps");
    return FALSE;
  }

  self->out_buffer_size = out_caps_width * out_caps_height * out_caps_num_dims;
  self->out_buffer_size *= getTypeSize ((DlInferType) out_caps_data_type);

  // Populate pre_proc params from caps
  self->pre_proc_image_params->input_width = GST_VIDEO_INFO_WIDTH (&in_video_info);
  self->pre_proc_image_params->input_height = GST_VIDEO_INFO_HEIGHT (&in_video_info);
  self->pre_proc_image_params->in_stride_y = GST_VIDEO_INFO_PLANE_STRIDE (&in_video_info,0);
  self->pre_proc_image_params->channel_order = self->channel_order;
  self->pre_proc_image_params->tensor_format = self->tensor_format;
  self->pre_proc_image_params->tensor_data_type = self->data_type;
  for (i = 0; i < SCALE_DIM; i++) {
    self->pre_proc_image_params->scale[i] = self->scale[i];
  }
  for (i = 0; i < MEAN_DIM; i++) {
    self->pre_proc_image_params->mean[i] = self->mean[i];
  }

  if (self->channel_order == DL_PRE_PROC_ARMV8_CHANNEL_ORDER_NCHW) {
    self->pre_proc_image_params->output_dimensions[3] = 1;
    self->pre_proc_image_params->output_dimensions[2] = out_caps_num_dims;
    self->pre_proc_image_params->output_dimensions[1] = out_caps_height;
    self->pre_proc_image_params->output_dimensions[0] = out_caps_width;
  } else {
    self->pre_proc_image_params->output_dimensions[3] = 1;
    self->pre_proc_image_params->output_dimensions[2] = out_caps_height;
    self->pre_proc_image_params->output_dimensions[1] = out_caps_width;
    self->pre_proc_image_params->output_dimensions[0] = out_caps_num_dims;
  }
  return TRUE;
}

static GstFlowReturn
gst_ti_pre_proc_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstTIPreProc *self = GST_TI_PRE_PROC (trans);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstVideoFrame in_frame;
  GstMapInfo out_buffer_mapinfo;

  GST_LOG_OBJECT (self, "transform");

  // Change stride if meta is available
  if (self->parse_in_video_meta) {
   GstVideoMeta *in_video_meta;
   in_video_meta = gst_buffer_get_video_meta (inbuf);
   if (in_video_meta) {
     self->pre_proc_image_params->in_stride_y = in_video_meta->stride[0];
   }
   self->parse_in_video_meta = FALSE;
  }

  if (!gst_video_frame_map (&in_frame, &self->in_info, inbuf, GST_MAP_READ)) {
      GST_ERROR_OBJECT (self, "failed to map input video frame");
      goto exit;
    }

  if (!gst_buffer_map (outbuf, &out_buffer_mapinfo, GST_MAP_READWRITE)) {
      GST_ERROR_OBJECT (self, "failed to map output buffer");
      goto exit;
    }

  self->pre_proc_image_params->in_img_target_ptr[0] = \
                                       GST_VIDEO_FRAME_PLANE_DATA (&in_frame,0);

  self->pre_proc_image_params->in_img_target_ptr[1] = NULL;
  if (GST_VIDEO_FORMAT_NV12 == GST_VIDEO_FRAME_FORMAT (&in_frame)) {
    self->pre_proc_image_params->in_img_target_ptr[1] = \
                                       GST_VIDEO_FRAME_PLANE_DATA (&in_frame,1);
  }

  self->pre_proc_image_params->out_tensor_target_ptr = \
                                               (void *) out_buffer_mapinfo.data;
  
  if (GST_VIDEO_FORMAT_NV12 == GST_VIDEO_FRAME_FORMAT (&in_frame)) {
    dlPreProcess_NV12_image (self->pre_proc_image_params);
  } else if (GST_VIDEO_FORMAT_RGB == GST_VIDEO_FRAME_FORMAT (&in_frame)) {
    dlPreProcess_RGB_image (self->pre_proc_image_params);
  } else {
    GST_ERROR_OBJECT (self, "invalid input and output conversion formats.");
    goto unmap;
  }
  ret = GST_FLOW_OK;
unmap:
  gst_video_frame_unmap (&in_frame);
  gst_buffer_unmap (outbuf, &out_buffer_mapinfo);
exit:
  return ret;
}

