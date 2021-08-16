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

#define SCALE_DEFAULT 1.0
#define MEAN_DEFAULT 1.0
#define CROP_DEFAULT 8192
#define SCALE_DIM 3
#define MEAN_DIM 3
#define CROP_DIM 4

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
  PROP_CROP_T,
  PROP_CROP_B,
  PROP_CROP_L,
  PROP_CROP_R,
  PROP_CHANNEL_ORDER,
};

struct _GstTIOVXDLPreProc
{
  GstTIOVXSiso element;
  TIOVXDLPreProcModuleObj obj;
  vx_context context;
  gint target_id;
  gfloat scale[SCALE_DIM];
  gfloat mean[MEAN_DIM];
  gfloat crop[CROP_DIM];
  gint channel_order;
};

/* Formats definition */
#define TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SRC "{RGB, BGR, NV12}"
#define TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SINK "{RGB, BGR, NV12}"
#define TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_DL_PRE_PROC_SUPPORTED_DIMENSIONS "[1 , 2147483647]"
#define TIOVX_DL_PRE_PROC_SUPPORTED_DATA_TYPES "[VX_TYPE_CHAR,  VX_TYPE_FLOAT64]"

/* Src caps */
#define TIOVX_DL_PRE_PROC_STATIC_CAPS_SRC \
  "video/x-tensor-tiovx, "                           \
  "num-dims = " TIOVX_DL_PRE_PROC_SUPPORTED_DIMENSIONS ", "                    \
  "data_type = " TIOVX_DL_PRE_PROC_SUPPORTED_DATA_TYPES

/* Sink caps */
#define TIOVX_DL_PRE_PROC_STATIC_CAPS_SINK \
  "video/x-raw, "                           \
  "format = (string) " TIOVX_DL_PRE_PROC_SUPPORTED_FORMATS_SINK ", "                   \
  "width = " TIOVX_DL_PRE_PROC_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DL_PRE_PROC_SUPPORTED_HEIGHT ", "                  \
  "framerate = " GST_VIDEO_FPS_RANGE

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

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_dl_pre_proc_debug);
#define GST_CAT_DEFAULT gst_tiovx_dl_pre_proc_debug

#define gst_tiovx_dl_pre_proc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXDLPreProc, gst_tiovx_dl_pre_proc,
    GST_TIOVX_SISO_TYPE, GST_DEBUG_CATEGORY_INIT (gst_tiovx_dl_pre_proc_debug,
        "tiovxdlpreproc", 0, "debug category for the tiovxdlpreproc element"););

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
    vx_reference ** input, vx_reference ** output, vx_node * node);

static gboolean gst_tiovx_dl_pre_proc_release_buffer (GstTIOVXSiso * trans);

static gboolean gst_tiovx_dl_pre_proc_deinit_module (GstTIOVXSiso * trans);

/* Initialize the plugin's class */
static void
gst_tiovx_dl_pre_proc_class_init (GstTIOVXDLPreProcClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstBaseTransformClass *gstbasetransform_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXSisoClass *gsttiovxsiso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasetransform_class = GST_BASE_TRANSFORM_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxsiso_class = GST_TIOVX_SISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX DL PreProc",
      "Filter/Converter/Video",
      "Preprocesses a video for conventional deep learning algorithms using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  gobject_class->set_property = gst_tiovx_dl_pre_proc_set_property;
  gobject_class->get_property = gst_tiovx_dl_pre_proc_get_property;

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
}

/* Initialize the new element */
static void
gst_tiovx_dl_pre_proc_init (GstTIOVXDLPreProc * self)
{
  self->context = NULL;
  memset (&self->obj, 0, sizeof self->obj);

  int i;
  for (i = 0; i < SCALE_DIM; i++) {
    self->scale[i] = SCALE_DEFAULT;
  }
  for (i = 0; i < MEAN_DIM; i++) {
    self->mean[i] = MEAN_DEFAULT;
  }
  for (i = 0; i < CROP_DIM; i++) {
    self->crop[i] = CROP_DEFAULT;
  }

  gint channel_order;
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
      self->target_id = g_value_set_enum (value);
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
    case PROP_CROP_T:
      self->crop[0] = g_value_get_float (value);
      break;
    case PROP_CROP_B:
      self->crop[1] = g_value_get_float (value);
      break;
    case PROP_CROP_L:
      self->crop[2] = g_value_get_float (value);
      break;
    case PROP_CROP_R:
      self->crop[3] = g_value_get_float (value);
      break;
    case PROP_CHANNEL_ORDER:
      self->channel_order = g_value_get_enum (value);
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
    case PROP_CROP_T:
      g_value_set_float (value, self->crop[0]);
      break;
    case PROP_CROP_B:
      g_value_set_float (value, self->crop[1]);
      break;
    case PROP_CROP_L:
      g_value_set_float (value, self->crop[2]);
      break;
    case PROP_CROP_R:
      g_value_set_float (value, self->crop[3]);
      break;
    case PROP_CHANNEL_ORDER:
      g_value_set_enum (value, self->channel_order);
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
  return NULL;
}

static gboolean
gst_tiovx_dl_pre_proc_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels)
{

  GstTIOVXDLPreProc *self = NULL;
  vx_status status = VX_SUCCESS;
  TIOVXDLPreProcModuleObj *preproc = NULL;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (in_caps, FALSE);
  g_return_val_if_fail (out_caps, FALSE);
  g_return_val_if_fail (num_channels >= MIN_NUM_CHANNELS, FALSE);
  g_return_val_if_fail (num_channels <= MAX_NUM_CHANNELS, FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);

  tivxImgProcLoadKernels (context);

  GST_INFO_OBJECT (self, "Init module");

  self->context = context;

/* Configure PreProcObj */
  preproc = &self->obj;
  preproc->params.channel_order = self.channel_order;
  preproc->params.tensor_bit_depth = sizeof (gfloat);

  memcpy (preproc->params.scale, self->scale, sizeof (preproc->params.scale));
  memcpy (preproc->params.mean, self->mean, sizeof (preproc->params.mean));
  memcpy (preproc->params.crop, self->crop, sizeof (preproc->params.crop));

/* Configure input */
  preproc->num_channels = DEFAULT_NUM_CHANNELS;
  preproc->input.bufq_depth = num_channels;
  preproc->input.color_format =
      gst_format_to_vx_format (in_info->finfo->format);
  preproc->input.width = GST_VIDEO_INFO_WIDTH (in_info);
  preproc->input.height = GST_VIDEO_INFO_HEIGHT (in_info);

  preproc->input.graph_parameter_index = INPUT_PARAMETER_INDEX;

/* TODO update preproc output params */
  preproc->output.bufq_depth = num_channels;
  preproc->output.datatype = VX_TYPE_FLOAT32;
  preproc->output.num_dims = 3;
  preproc->output.dim_sizes[0] = GST_VIDEO_INFO_WIDTH (in_info);
  preproc->output.dim_sizes[1] = GST_VIDEO_INFO_HEIGHT (in_info);
  preproc->output.dim_sizes[2] = 3;
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

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);
  GST_INFO_OBJECT (self, "Create graph");

  /* TODO add target selection */
  status =
      tiovx_dl_pre_proc_module_create (graph, &self->obj, NULL,
      TIVX_TARGET_DSP1);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dl_pre_proc_get_node_info (GstTIOVXSiso * trans,
    vx_reference ** input, vx_reference ** output, vx_node * node)
{
  GstTIOVXDLPreProc *self = NULL;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);

  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.node), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.input.image_handle[0]), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.output.tensor_handle[0]), FALSE);

  GST_INFO_OBJECT (self, "Get node info from module");

  *node = self->obj.node;
  *input = (vx_reference *) & self->obj.input.image_handle[0];
  *output = (vx_reference *) & self->obj.output.tensor_handle[0];

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

  status = tiovx_dl_pre_proc_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dl_pre_proc_deinit_module (GstTIOVXSiso * trans)
{
  GstTIOVXDLPreProc *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_DL_PRE_PROC (trans);
  GST_INFO_OBJECT (self, "Deinit module");

  status = tiovx_dl_pre_proc_module_delete (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    return FALSE;
  }

  status = tiovx_dl_pre_proc_module_deinit (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    return FALSE;
  }

  if (VX_SUCCESS == vxGetStatus ((vx_reference) self->context)) {
    /*Happends before tivxHwaLoadKernels */
    tivxImgProcUnLoadKernels (self->context);
  }

  /*Context is released by the base class */
  self->context = NULL;

  return TRUE;
}
