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

#include <glib.h>

extern "C"
{

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_TIDL

#include <TI/tivx.h>
#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferpool.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferutils.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferpoolutils.h"
#include "gst-libs/gst/tiovx/gsttiovxtensorbufferpool.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#endif //ENABLE_TIDL

#include "gst-libs/gst/ti/gsttidloutmeta.h"
#include "gsttidlinferer.h"

}

#include <ti_dl_inferer_config.h>
#include <ti_dl_inferer.h>

#define MIN_POOL_SIZE 2
#define MAX_POOL_SIZE 16
#define DEFAULT_POOL_SIZE MIN_POOL_SIZE

#define MIN_DEBUG_LEVEL 0
#define MAX_DEBUG_LEVEL 4
#define DEFAULT_DEBUG_LEVEL MIN_DEBUG_LEVEL

#define MEMORY_ALIGNMENT 128

#define NUM_TENSOR_DIMS 3
#define TENSOR_ALIGNMENT_BYTES 128

#ifndef ENABLE_TIDL
#define ARM_MAX_OUTPUT_WIDTH  1001
#define ARM_MAX_OUTPUT_HEIGHT 1001
#endif // NOT ENABLE_TIDL

#ifdef ENABLE_TIDL

/* Target definition */
#define GST_TYPE_TI_DL_INFERER_TARGET (gst_ti_dl_inferer_target_get_type())
static GType
gst_ti_dl_inferer_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {1, "C7x instance 1", "C7x1"},
#if defined(SOC_J784S4) || defined(SOC_J722S)
    {2, "C7x instance 2", "C7x2"},
#if defined(SOC_J784S4)
    {3, "C7x instance 3", "C7x3"},
    {4, "C7x instance 4", "C7x4"},
#endif
#endif
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type =
        g_enum_register_static ("GstTIDLInfererTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TI_DL_INFERER_TARGET 1

#endif //ENABLE_TIDL

using namespace
    ti::dl_inferer;

/* Properties definition */
enum
{
  PROP_0,
  PROP_MODEL,
  PROP_IN_POOL_SIZE,
  PROP_OUT_POOL_SIZE,
  PROP_DEBUG_LEVEL,
#ifdef ENABLE_TIDL
  PROP_TARGET,
#endif //ENABLE_TIDL
};

/* Formats definition */
#define TI_DL_INFERER_SUPPORTED_WIDTH "[1 , 8192]"
#define TI_DL_INFERER_SUPPORTED_HEIGHT "[1 , 8192]"
#define TI_DL_INFERER_SUPPORTED_DIMENSIONS "3"
#define TI_DL_INFERER_SUPPORTED_DATA_TYPES "[2 , 10]"
#define TI_DL_INFERER_SUPPORTED_CHANNEL_ORDER "{NCHW, NHWC}"
#define TI_DL_INFERER_SUPPORTED_TENSOR_FORMAT "{RGB, BGR}"

/* caps */
#define TI_DL_INFERER_STATIC_CAPS                                 \
  "application/x-tensor-tiovx, "                                      \
  "num-dims = " TI_DL_INFERER_SUPPORTED_DIMENSIONS ", "           \
  "data-type = " TI_DL_INFERER_SUPPORTED_DATA_TYPES ", "          \
  "channel-order = " TI_DL_INFERER_SUPPORTED_CHANNEL_ORDER ", "   \
  "tensor-format = " TI_DL_INFERER_SUPPORTED_TENSOR_FORMAT ", "   \
  "tensor-width = " TI_DL_INFERER_SUPPORTED_WIDTH ", "            \
  "tensor-height = " TI_DL_INFERER_SUPPORTED_HEIGHT

/* Pads definitions */
static
    GstStaticPadTemplate
    sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_DL_INFERER_STATIC_CAPS)
    );

static
    GstStaticPadTemplate
    src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_DL_INFERER_STATIC_CAPS)
    );

struct _GstTIDLInferer
{
  GstBaseTransform
      element;
  gchar *
      model;
  guint
      in_pool_size;
  guint
      out_pool_size;
  InfererConfig *
      inferer_config;
  DLInferer *
      inferer;
  VecDlTensorPtr
      input_buffs;
  VecDlTensorPtr
      output_buffs;
  GstTiDlOutMeta
      out_meta;
  guint
      debug_level;
  guint
      input_height;
  guint
      input_width;
  guint
      output_height;
  guint
      output_width;
  GstBufferPool *
      sink_buffer_pool;

#ifdef ENABLE_TIDL
  gint
      target;
  vx_context
      context;
  vx_tensor
      input_ref;
  vx_tensor
      output_ref;
#else
  gsize
      input_size;
  gsize
      output_size;
#endif //ENABLE_TIDL
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_dl_inferer_debug);
#define GST_CAT_DEFAULT gst_ti_dl_inferer_debug

#define gst_ti_dl_inferer_parent_class parent_class
G_DEFINE_TYPE (GstTIDLInferer, gst_ti_dl_inferer, GST_TYPE_BASE_TRANSFORM);

static void
gst_ti_dl_inferer_finalize (GObject * obj);
static void
gst_ti_dl_inferer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_ti_dl_inferer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static guint
gst_ti_dl_inferer_set_output_params(GstTIDLInferer * self);

static GstCaps *
gst_ti_dl_inferer_transform_caps (GstBaseTransform *
    base, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static
    GstFlowReturn
gst_ti_dl_inferer_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static
    gboolean
gst_ti_dl_inferer_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static
    gboolean
gst_ti_dl_inferer_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);

/* Initialize the plugin's class */
static void
gst_ti_dl_inferer_class_init (GstTIDLInfererClass * klass)
{
  GObjectClass *
      gobject_class = NULL;
  GstBaseTransformClass *
      gstbasetransform_class = NULL;
  GstElementClass *
      gstelement_class = NULL;

  gobject_class = (GObjectClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gst_element_class_set_details_simple (gstelement_class,
      "TI DL Inferer",
      "Filter/Converter/Tensor",
      "Compute the DL inference of input tensor for model specified",
      "Rahul T R <r-ravikumar@ti.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_ti_dl_inferer_set_property;
  gobject_class->get_property = gst_ti_dl_inferer_get_property;

#ifdef ENABLE_TIDL
  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "C7x target to offload the inference",
          GST_TYPE_TI_DL_INFERER_TARGET,
          DEFAULT_TI_DL_INFERER_TARGET,
          (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
              G_PARAM_STATIC_STRINGS)));
#endif //ENABLE_TIDL

  g_object_class_install_property (gobject_class, PROP_MODEL,
      g_param_spec_string ("model", "Model Directory",
          "TIDL Model directory with params, model and artifacts",
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property (gobject_class, PROP_IN_POOL_SIZE,
      g_param_spec_uint ("in-pool-size", "Input Pool Size",
          "Number of buffers to allocate in input pool", MIN_POOL_SIZE,
          MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_OUT_POOL_SIZE,
      g_param_spec_uint ("out-pool-size", "Output Pool Size",
          "Number of buffers to allocate in output pool", MIN_POOL_SIZE,
          MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DEBUG_LEVEL,
      g_param_spec_uint ("debug-level", "Debug Level",
          "Debug Level Log to set", MIN_DEBUG_LEVEL,
          MAX_DEBUG_LEVEL, DEFAULT_DEBUG_LEVEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_ti_dl_inferer_transform_caps);
  gstbasetransform_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_ti_dl_inferer_decide_allocation);
  gstbasetransform_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_ti_dl_inferer_propose_allocation);
  gstbasetransform_class->transform =
      GST_DEBUG_FUNCPTR (gst_ti_dl_inferer_transform);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_dl_inferer_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_dl_inferer_debug,
      "tidlinferer", 0, "TI DL Inferer");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_ti_dl_inferer_init (GstTIDLInferer * self)
{
  GST_LOG_OBJECT (self, "init");

  self->model = NULL;
  self->in_pool_size = DEFAULT_POOL_SIZE;
  self->out_pool_size = DEFAULT_POOL_SIZE;
  self->inferer_config = NULL;
  self->inferer = NULL;
  self->sink_buffer_pool = NULL;
  self->out_meta.num_outputs = 0;
  self->debug_level = DEFAULT_DEBUG_LEVEL;
  self->input_height = 0;
  self->input_width = 0;
  self->output_height = 0;
  self->output_width = 0;
#ifdef ENABLE_TIDL
  self->target = DEFAULT_TI_DL_INFERER_TARGET;
  self->input_ref = NULL;
  self->output_ref = NULL;
  self->context = NULL;
#else
  self->input_size = 0;
  self->output_size = 0;
#endif //ENABLE_TIDL

  return;
}

static void
gst_ti_dl_inferer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
#ifdef ENABLE_TIDL
    case PROP_TARGET:
      self->target = g_value_get_enum (value);
      break;
#endif //ENABLE_TIDL
    case PROP_MODEL:
      self->model = g_value_dup_string (value);
      break;
    case PROP_IN_POOL_SIZE:
      self->in_pool_size = g_value_get_uint (value);
      break;
    case PROP_OUT_POOL_SIZE:
      self->out_pool_size = g_value_get_uint (value);
      break;
    case PROP_DEBUG_LEVEL:
      self->debug_level = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_ti_dl_inferer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
#ifdef ENABLE_TIDL
    case PROP_TARGET:
      g_value_set_enum (value, self->target);
      break;
#endif //ENABLE_TIDL
    case PROP_MODEL:
      g_value_set_string (value, self->model);
      break;
    case PROP_IN_POOL_SIZE:
      g_value_set_uint (value, self->in_pool_size);
      break;
    case PROP_OUT_POOL_SIZE:
      g_value_set_uint (value, self->out_pool_size);
      break;
    case PROP_DEBUG_LEVEL:
      g_value_set_uint (value, self->debug_level);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static guint
gst_ti_dl_inferer_set_output_params(GstTIDLInferer * self)
{
    /* Setting output tensor params and update meta info*/
    GST_LOG_OBJECT (self, "set output params and update meta info");

    guint offset = 0;
    for (guint i = 0; i < self->output_buffs.size (); i++) {
      guint current_height = 0, current_width = 0;
      for (gint j = 0; j < self->output_buffs[i]->dim; j++) {
        if (self->output_buffs[i]->shape[j] > 1) {
          if (!current_height) {
            current_height = self->output_buffs[i]->shape[j];
          } else {
            current_width = self->output_buffs[i]->shape[j];
            break;
          }
        }
      }

      if (!current_height) {
        current_height = self->output_height;
      }

      if (!current_width) {
        current_width = 1;
      }

#ifdef ENABLE_TIDL
      if (self->output_height == 0) {
        self->output_height = current_height;
      } else if (self->output_height != current_height) {
        GST_ERROR_OBJECT (self,
            "Number of entries in output tensors differ %u %u",
            self->output_height, current_height);
        return -1;
      }
#else
      self->output_height = current_height;
#endif //ENABLE_TIDL

      self->out_meta.widths[i] = current_width;
      self->out_meta.types[i] = self->output_buffs[i]->type;
      self->out_meta.offsets[i] = offset;
      offset += current_width * current_height *
          getTypeSize (self->output_buffs[i]->type);
      /* Aligne the offset */
      offset = (offset & ~(TENSOR_ALIGNMENT_BYTES - 1)) +
          TENSOR_ALIGNMENT_BYTES;
      self->out_meta.num_outputs++;

      current_width *= getTypeSize (self->output_buffs[i]->type);
      self->output_width += current_width;
    }

    self->out_meta.height = self->output_height;

    /* Compensate for aligned bytes */
    self->output_height +=
        self->output_buffs.size () * TENSOR_ALIGNMENT_BYTES /
        self->output_width + 1;
    return 0;
}

static GstCaps *
gst_ti_dl_inferer_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (base);
  GstCaps *
      result_caps = NULL;
  GstStructure *
      result_structure = NULL;
  guint status = -1;
#ifdef ENABLE_TIDL
  vx_size dim_sizes[NUM_TENSOR_DIMS];
#endif //ENABLE_TIDL

  GST_LOG_OBJECT (self, "transform_caps");

  if (!self->inferer_config) {
    if (self->model == NULL) {
      GST_ERROR_OBJECT (self, "model property is not set");
      goto exit;
    }

    self->inferer_config = new InfererConfig;
#ifdef ENABLE_TIDL
    status = self->inferer_config->getConfig (self->model, TRUE, self->target);
#else
    status = self->inferer_config->getConfig (self->model, FALSE, 1);
    /*  Don't allocate outbuffer (ex: in case of onr-od) even if the output
        tensor changes for each run. This is beacause we will use buffer
        allocated from gstreamer.
    */
    self->inferer_config->allocateOutBuf = FALSE;
#endif //ENABLE_TIDL

    /*  Setting debug level
    */
    self->inferer_config->debug_level = self->debug_level;

    if (status < 0) {
      GST_ERROR_OBJECT (self, "Failed to get inferer config");
      goto exit;
    }

    self->inferer = DLInferer::makeInferer (*(self->inferer_config));
    if (self->inferer == NULL) {
      GST_ERROR_OBJECT (self, "Failed to create inferer");
      goto exit;
    }

    status = self->inferer->createBuffers (self->inferer->getOutputInfo (),
        self->output_buffs, TRUE);
    if (status < 0) {
      GST_ERROR_OBJECT (self, "Failed to create output buffers");
      goto exit;
    }

    status = self->inferer->createBuffers (self->inferer->getInputInfo (),
        self->input_buffs, TRUE);
    if (status < 0) {
      GST_ERROR_OBJECT (self, "Failed to create input buffers");
      goto exit;
    }

    /* HACK: For some models shapes were getting set after one run */
    self->inferer->run (self->input_buffs, self->output_buffs);

    /* Setting Input tensor params */
    if (self->inferer_config->dataLayout == "NHWC") {
      self->input_width = self->input_buffs[0]->shape[2];
      self->input_height = self->input_buffs[0]->shape[1];
    } else {
      self->input_width = self->input_buffs[0]->shape[3];
      self->input_height = self->input_buffs[0]->shape[2];
    }

    self->out_meta.input_width = self->input_width;
    self->out_meta.input_height = self->input_height;

#ifdef ENABLE_TIDL
    self->context = vxCreateContext ();

    dim_sizes[0] = self->input_buffs[0]->shape[1];
    dim_sizes[1] = self->input_buffs[0]->shape[2];
    dim_sizes[2] = self->input_buffs[0]->shape[3];
    self->input_ref = vxCreateTensor (self->context, NUM_TENSOR_DIMS, dim_sizes,
        self->input_buffs[0]->type, 0);
#else
    self->input_size = getTypeSize (self->input_buffs[0]->type);
    for (gint i=0; i < self->input_buffs[0]->dim; i++) {
      self->input_size *= self->input_buffs[0]->shape[i];
    }
#endif //ENABLE_TIDL

    status = gst_ti_dl_inferer_set_output_params(self);
    if (status < 0) {
      GST_ERROR_OBJECT (self, "Failed to set output params");
      goto exit;
    }

#ifdef ENABLE_TIDL
    dim_sizes[0] = self->output_width;
    dim_sizes[1] = self->output_height;
    dim_sizes[2] = 1;

    self->output_ref = vxCreateTensor (self->context, NUM_TENSOR_DIMS,
        dim_sizes, DlInferType_UInt8, 0);
#else
    self->output_width = ARM_MAX_OUTPUT_WIDTH;
    self->output_height = ARM_MAX_OUTPUT_HEIGHT;
    self->output_size = self->output_width * self->output_height;
#endif //ENABLE_TIDL

    GST_LOG_OBJECT (self, "output width = %u", self->output_width);
    GST_LOG_OBJECT (self, "output height = %u", self->output_height);
  }

  result_caps = gst_caps_from_string (TI_DL_INFERER_STATIC_CAPS);
  if (GST_PAD_SINK == direction) {
    for (guint i = 0; i < gst_caps_get_size (result_caps); i++) {
      result_structure = gst_caps_get_structure (result_caps, i);
      gst_structure_fixate_field_nearest_int (result_structure,
          "tensor-width", self->output_width);

      gst_structure_fixate_field_nearest_int (result_structure,
          "tensor-height", self->output_height);

      gst_structure_fixate_field_nearest_int (result_structure,
          "data-type", DlInferType_UInt8);
    }
  } else {
    for (guint i = 0; i < gst_caps_get_size (result_caps); i++) {
      result_structure = gst_caps_get_structure (result_caps, i);
      gst_structure_fixate_field_nearest_int (result_structure,
          "tensor-width", self->input_width);

      gst_structure_fixate_field_nearest_int (result_structure,
          "tensor-height", self->input_height);

      gst_structure_fixate_field_nearest_int (result_structure,
          "data-type", self->input_buffs[0]->type);
    }
  }

  if (filter) {
    GstCaps *
        tmp = result_caps;
    result_caps = gst_caps_intersect (result_caps, filter);
    gst_caps_unref (tmp);
  }

  if (gst_caps_is_empty (result_caps)) {
    GST_ERROR_OBJECT (self, "Filter Caps is not supported %" GST_PTR_FORMAT,
        filter);
    goto exit;
  }

  GST_DEBUG_OBJECT (self, "Resulting caps are %" GST_PTR_FORMAT, result_caps);

exit:
  return result_caps;
}

static void
gst_ti_dl_inferer_finalize (GObject * obj)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (obj);

  GST_LOG_OBJECT (self, "finalize");

  if (NULL != self->sink_buffer_pool) {
    gst_object_unref (self->sink_buffer_pool);
  }

#ifdef ENABLE_TIDL
  if (NULL != self->input_ref) {
    vxReleaseTensor (&(self->input_ref));
  }

  if (NULL != self->output_ref) {
    vxReleaseTensor (&(self->output_ref));
  }

  vxReleaseContext (&self->context);
#endif //ENABLE_TIDL
  delete self->inferer;
  delete self->inferer_config;

  G_OBJECT_CLASS (gst_ti_dl_inferer_parent_class)->finalize (obj);
}

static
    GstFlowReturn
gst_ti_dl_inferer_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (trans);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstMapInfo input_mapinfo, output_mapinfo;
  GstClockTime pts = 0, dts = 0, duration = 0;
  #ifndef ENABLE_TIDL
  guint status = -1;
  #endif // NOT ENABLE_TIDL

  GST_LOG_OBJECT (self, "transform");

  pts = GST_BUFFER_PTS (inbuf);
  dts = GST_BUFFER_DTS (inbuf);
  duration = GST_BUFFER_DURATION (inbuf);

  if (!gst_buffer_map (inbuf, &input_mapinfo, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "failed to map input buffer");
    goto exit;
  }

  if (!gst_buffer_map (outbuf, &output_mapinfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (self, "failed to map input buffer");
    goto exit;
  }

  self->input_buffs[0]->data = input_mapinfo.data;

#ifndef ENABLE_TIDL
  /* For ARM mode, the output tensors is not fixed for some models (ex: ONR-OD).
  Hence meta information keeps changing after each run.
  */
  self->out_meta.num_outputs = 0;
  self->output_width = 0;
  status = gst_ti_dl_inferer_set_output_params(self);
  if (status < 0) {
    GST_ERROR_OBJECT (self, "Failed to set output params");
    goto exit;
  }
  if (self->out_meta.height > ARM_MAX_OUTPUT_HEIGHT) {
    GST_WARNING_OBJECT (self,
      "Output Tensor Height [%d] is greater than max allowed height [%d]\n",
      self->out_meta.height,ARM_MAX_OUTPUT_HEIGHT);
    goto skip;
  }
  if (self->output_width > ARM_MAX_OUTPUT_WIDTH) {
    GST_WARNING_OBJECT (self,
      "Output Tensor Width [%d] is greater than max allowed width [%d]\n",
      self->output_width,ARM_MAX_OUTPUT_WIDTH);
    goto skip;
  }
#endif // NOT ENABLE_TIDL

  for (guint i = 0; i < self->output_buffs.size (); i++) {
    self->output_buffs[i]->data = output_mapinfo.data +
        self->out_meta.offsets[i];
  }

  self->inferer->run (self->input_buffs, self->output_buffs);
  goto skip;

skip:
  gst_buffer_unmap (inbuf, &input_mapinfo);
  gst_buffer_unmap (outbuf, &output_mapinfo);

  GST_BUFFER_PTS (outbuf) = pts;
  GST_BUFFER_DTS (outbuf) = dts;
  GST_BUFFER_DURATION (outbuf) = duration;

  gst_buffer_add_tidl_out_meta (outbuf, &(self->out_meta));

  ret = GST_FLOW_OK;

exit:
  return ret;
}

#ifdef ENABLE_TIDL
static
    gboolean
gst_ti_dl_inferer_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (trans);
  GstBufferPool *
      pool = NULL;
  gboolean ret = TRUE;
  guint npool = 0;
  gboolean pool_needed = TRUE;

  GST_LOG_OBJECT (self, "Decide allocation");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *
        pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (NULL == pool) {
      GST_DEBUG_OBJECT (self, "No pool in query position: %d, ignoring", npool);
      gst_query_remove_nth_allocation_pool (query, npool);
      continue;
    }

    /* Use TIOVX pool if found */
    if (GST_TIOVX_IS_BUFFER_POOL (pool)) {
      GST_INFO_OBJECT (self, "TIOVX pool found, using this one: \"%s\"",
          GST_OBJECT_NAME (pool));

      pool_needed = FALSE;
    } else {
      GST_INFO_OBJECT (self, "No TIOVX pool, discarding: \"%s\"",
          GST_OBJECT_NAME (pool));

      gst_query_remove_nth_allocation_pool (query, npool);
    }

    gst_object_unref (pool);
  }

  if (pool_needed) {
    /* Create our own pool if a TIOVX was not found.
       We use output vx_reference to decide a pool to use downstream. */
    gsize size = 0;

    size = gst_tiovx_get_size_from_exemplar ((vx_reference) self->output_ref);
    if (0 >= size) {
      GST_ERROR_OBJECT (self, "Failed to get size from exemplar");
      ret = FALSE;
      goto exit;
    }

    ret =
        gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, self->out_pool_size,
        (vx_reference) self->output_ref, size, 1, &pool);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to add new pool in decide allocation");
      return ret;
    }

    ret = gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), TRUE);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
      goto exit;
    }
    gst_object_unref (pool);
    pool = NULL;
  }

exit:
  return ret;
}

static
    gboolean
gst_ti_dl_inferer_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (trans);
  GstBufferPool *
      pool = NULL;
  gsize size = 0;
  gboolean ret = TRUE;

  GST_LOG_OBJECT (self, "Propose allocation");

  /* We use input vx_reference to propose a pool upstream */
  size = gst_tiovx_get_size_from_exemplar ((vx_reference) self->input_ref);
  if (0 >= size) {
    GST_ERROR_OBJECT (self, "Failed to get size from input");
    ret = FALSE;
    goto exit;
  }

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, self->in_pool_size,
      (vx_reference) self->input_ref, size, 1, &pool);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to add new pool in propose allocation");
    return ret;
  }
  /* Unref the old stored pool */
  if (NULL != self->sink_buffer_pool) {
    gst_object_unref (self->sink_buffer_pool);
  }
  /* Assign the new pool to the internal value */
  self->sink_buffer_pool = GST_BUFFER_POOL (pool);

exit:
  return ret;
}
#else
static
    gboolean
gst_ti_dl_inferer_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (trans);
  GstBufferPool *
      pool = NULL;
  gboolean ret = TRUE;
  guint npool = 0;
  gboolean pool_needed = TRUE;

  GST_LOG_OBJECT (self, "Decide allocation");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *
        pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (NULL == pool) {
      GST_DEBUG_OBJECT (self, "No pool in query position: %d, ignoring", npool);
      gst_query_remove_nth_allocation_pool (query, npool);
      pool_needed = FALSE;
      gst_object_unref (pool);
      break;
    }

    gst_object_unref (pool);
  }

  if (pool_needed) {
    GstStructure *config;
    GstCaps *caps;
    GstAllocationParams alloc_params;

    gst_query_parse_allocation (query, &caps, NULL);
    pool = gst_buffer_pool_new ();
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config,
                                       caps,
                                       self->output_size,
                                       self->out_pool_size,
                                       self->out_pool_size);

    gst_allocation_params_init(&alloc_params);
    alloc_params.align = MEMORY_ALIGNMENT - 1;

    gst_buffer_pool_config_set_allocator (config,
                                          NULL,
                                          &alloc_params);
    gst_buffer_pool_set_config(pool, config);
    gst_query_add_allocation_pool (query, pool, self->output_size,
            self->out_pool_size, self->out_pool_size);

    ret = gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), TRUE);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
      goto exit;
    }
    gst_object_unref (pool);
    pool = NULL;
  }

exit:
  return ret;
}

static
    gboolean
gst_ti_dl_inferer_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstTIDLInferer *
      self = GST_TI_DL_INFERER (trans);
  GstBufferPool *
      pool = NULL;
  gboolean ret = TRUE;
  GstStructure *config;
  GstCaps *caps;
  GstAllocationParams alloc_params;

  gst_query_parse_allocation (query, &caps, NULL);
  pool = gst_buffer_pool_new ();
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config,
                                     caps,
                                     self->input_size,
                                     self->in_pool_size,
                                     self->in_pool_size);

  gst_allocation_params_init(&alloc_params);
  alloc_params.align = MEMORY_ALIGNMENT - 1;

  gst_buffer_pool_config_set_allocator (config,
                                        NULL,
                                        &alloc_params);
  gst_buffer_pool_set_config(pool, config);
  gst_query_add_allocation_pool (query, pool, self->output_size,
          self->out_pool_size, self->out_pool_size);

  ret = gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), TRUE);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
    goto exit;
  }
  gst_object_unref (pool);
  pool = NULL;

  GST_LOG_OBJECT (self, "Propose allocation");

  /* Unref the old stored pool */
  if (NULL != self->sink_buffer_pool) {
    gst_object_unref (self->sink_buffer_pool);
  }
  /* Assign the new pool to the internal value */
  self->sink_buffer_pool = GST_BUFFER_POOL (pool);

exit:
  return ret;
}
#endif //ENABLE_TIDL
