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

#include "gsttiovxoptflow.h"

#include "unistd.h"

#include <tiovx_dof_module.h>

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxallocator.h"
#include "gst-libs/gst/tiovx/gsttiovxmiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

static const int input_param_id = 3;
static const int input_ref_param_id = 4;
static const int output_flow_param_id = 8;

#define MODULE_MAX_NUM_PLANES 4

/* TIOVX Optflow */

/* Target definition */
enum
{
  TIVX_TARGET_DMPAC_DOF_ID = 0,
};

#define GST_TYPE_TIOVX_OPTFLOW_TARGET (gst_tiovx_optflow_target_get_type())
static GType
gst_tiovx_optflow_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_DMPAC_DOF_ID, "DMPAC DOF", TIVX_TARGET_DMPAC_DOF},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXOptflowTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_OPTFLOW_TARGET TIVX_TARGET_DMPAC_DOF_ID

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
};

/* Formats definition */

#define TIOVX_OPTFLOW_SUPPORTED_FORMATS_PYRAMID "{GRAY8, GRAY16_LE}"
#define TIOVX_OPTFLOW_SUPPORTED_WIDTH "[1 , 2048]"
#define TIOVX_OPTFLOW_SUPPORTED_HEIGHT "[1 , 1024]"
#define TIOVX_OPTFLOW_SUPPORTED_LEVELS_PYRAMID "[1 , 6]"
#define TIOVX_OPTFLOW_SUPPORTED_SCALE_PYRAMID "0.5"
#define TIOVX_OPTFLOW_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_OPTFLOW_STATIC_CAPS_SINK                                 \
  "application/x-pyramid-tiovx, "                                      \
  "format = (string) " TIOVX_OPTFLOW_SUPPORTED_FORMATS_PYRAMID ", "    \
  "width = " TIOVX_OPTFLOW_SUPPORTED_WIDTH ", "                \
  "height = " TIOVX_OPTFLOW_SUPPORTED_HEIGHT ", "              \
  "levels = " TIOVX_OPTFLOW_SUPPORTED_LEVELS_PYRAMID ", "              \
  "scale = " TIOVX_OPTFLOW_SUPPORTED_SCALE_PYRAMID                     \
  "; "                                                                 \
  "application/x-pyramid-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), " \
  "format = (string) " TIOVX_OPTFLOW_SUPPORTED_FORMATS_PYRAMID ", "    \
  "width = " TIOVX_OPTFLOW_SUPPORTED_WIDTH ", "                \
  "height = " TIOVX_OPTFLOW_SUPPORTED_HEIGHT ", "              \
  "levels = " TIOVX_OPTFLOW_SUPPORTED_LEVELS_PYRAMID ", "              \
  "scale = " TIOVX_OPTFLOW_SUPPORTED_SCALE_PYRAMID ", "                \
  "num-channels = " TIOVX_OPTFLOW_SUPPORTED_CHANNELS


#define TIOVX_OPTFLOW_STATIC_CAPS_SRC                                  \
  "application/x-optflow-tiovx, "                                      \
  "width = " TIOVX_OPTFLOW_SUPPORTED_WIDTH ", "                        \
  "height = " TIOVX_OPTFLOW_SUPPORTED_HEIGHT                      \
  "; "                                                                 \
  "application/x-optflow-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), " \
  "width = " TIOVX_OPTFLOW_SUPPORTED_WIDTH ", "                        \
  "height = " TIOVX_OPTFLOW_SUPPORTED_HEIGHT ", "                      \
  "num-channels = " TIOVX_OPTFLOW_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_OPTFLOW_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate delayed_sink_template =
GST_STATIC_PAD_TEMPLATE ("delayed_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_OPTFLOW_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_OPTFLOW_STATIC_CAPS_SRC)
    );


struct _GstTIOVXOptflow
{
  GstTIOVXMiso parent;

  TIOVXDofModuleObj obj;

  gint target_id;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_optflow_debug);
#define GST_CAT_DEFAULT gst_tiovx_optflow_debug

#define gst_tiovx_optflow_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXOptflow, gst_tiovx_optflow,
    GST_TYPE_TIOVX_MISO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_optflow_debug,
        "tiovxoptflow", 0, "debug category for the tiovxoptflow element"));

static const gchar *target_id_to_target_name (gint target_id);
static void
gst_tiovx_optflow_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_optflow_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_tiovx_optflow_init_module (GstTIOVXMiso * agg,
    vx_context context, GList * sink_pads_list, GstPad * src_pad,
    guint num_channels);
static gboolean gst_tiovx_optflow_create_graph (GstTIOVXMiso * agg,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_optflow_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects);
static gboolean gst_tiovx_optflow_configure_module (GstTIOVXMiso * agg);
static gboolean gst_tiovx_optflow_release_buffer (GstTIOVXMiso * agg);
static gboolean gst_tiovx_optflow_deinit_module (GstTIOVXMiso * agg);
static GstCaps *gst_tiovx_optflow_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps);
static void gst_tiovx_optflow_finalize (GObject * object);

static void
gst_tiovx_optflow_class_init (GstTIOVXOptflowClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXMisoClass *gsttiovxmiso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxmiso_class = GST_TIOVX_MISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Optflow",
      "Filter",
      "Optflow using the TIOVX Modules API", "RidgeRun <support@ridgerun.com>");

  gobject_class->set_property = gst_tiovx_optflow_set_property;
  gobject_class->get_property = gst_tiovx_optflow_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_OPTFLOW_TARGET,
          DEFAULT_TIOVX_OPTFLOW_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));


  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &src_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sink_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &delayed_sink_template, GST_TYPE_TIOVX_MISO_PAD);

  gsttiovxmiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_optflow_init_module);

  gsttiovxmiso_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_optflow_configure_module);

  gsttiovxmiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_optflow_get_node_info);

  gsttiovxmiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_optflow_create_graph);

  gsttiovxmiso_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_optflow_fixate_caps);

  gsttiovxmiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_optflow_release_buffer);

  gsttiovxmiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_optflow_deinit_module);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_optflow_finalize);
}

static void
gst_tiovx_optflow_init (GstTIOVXOptflow * self)
{
  memset (&self->obj, 0, sizeof (self->obj));

  self->target_id = DEFAULT_TIOVX_OPTFLOW_TARGET;
}

static void
gst_tiovx_optflow_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXOptflow *self = GST_TIOVX_OPTFLOW (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_optflow_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXOptflow *self = GST_TIOVX_OPTFLOW (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_optflow_init_module (GstTIOVXMiso * agg, vx_context context,
    GList * sink_pads_list, GstPad * src_pad, guint num_channels)
{
  GstTIOVXOptflow *self = NULL;
  TIOVXDofModuleObj *optflow = NULL;
  GstCaps *caps = NULL;
  GList *l = NULL;
  gboolean ret = FALSE;

  gint i = 0;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_OPTFLOW (agg);
  optflow = &self->obj;

  /* Initialize the output parameters */
  for (l = sink_pads_list; l != NULL; l = g_list_next (l)) {
    GstTIOVXMisoPad *sink_pad = GST_TIOVX_MISO_PAD (l->data);
    gint levels = 0;
    gdouble scale = 0;
    gint width = 0;
    gint height = 0;
    GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

    caps = gst_pad_get_current_caps (GST_PAD (sink_pad));
    if (!caps) {
      caps = gst_pad_peer_query_caps (GST_PAD (sink_pad), NULL);
    }

    ret =
        gst_tioxv_get_pyramid_caps_info ((GObject *) self, GST_CAT_DEFAULT,
        caps, &levels, &scale, &width, &height, &format);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to get pyramid info from output caps");
      goto exit;
    }

    optflow->width = width;
    optflow->height = height;

    /* We do both inputs at once, since they should be equal */
    optflow->input.bufq_depth = num_channels;
    optflow->input.color_format = gst_format_to_vx_format (format);
    optflow->input.levels = levels;
    optflow->input.scale = scale;
    optflow->input.graph_parameter_index = i;
    i++;


    optflow->input_ref.bufq_depth = num_channels;
    optflow->input_ref.color_format = gst_format_to_vx_format (format);
    optflow->input_ref.levels = levels;
    optflow->input_ref.scale = scale;
    optflow->input_ref.graph_parameter_index = i;
    i++;

    GST_INFO_OBJECT (self,
        "Input & Input ref parameters: \n\tWidth: %d \n\tHeight: %d \n\tNum channels: %d\n\tLevels: %d\n\tScale: %f",
        optflow->width, optflow->height, optflow->input.bufq_depth,
        optflow->input.levels, optflow->input.scale);
    break;
  }

  optflow->output_flow_vector.bufq_depth = num_channels;
  optflow->output_flow_vector.color_format = VX_DF_IMAGE_U32;
  optflow->output_flow_vector.graph_parameter_index = i;
  i++;

  optflow->num_channels = num_channels;
  optflow->enable_temporal_predicton_flow_vector = 0;
  optflow->enable_output_distribution = 0;


  tivx_dmpac_dof_params_init (&optflow->params);

  if (optflow->enable_temporal_predicton_flow_vector == 0) {
    optflow->params.base_predictor[0] = TIVX_DMPAC_DOF_PREDICTOR_DELAY_LEFT;
    optflow->params.base_predictor[1] = TIVX_DMPAC_DOF_PREDICTOR_PYR_COLOCATED;

    optflow->params.inter_predictor[0] = TIVX_DMPAC_DOF_PREDICTOR_DELAY_LEFT;
    optflow->params.inter_predictor[1] = TIVX_DMPAC_DOF_PREDICTOR_PYR_COLOCATED;
  }

  optflow->params.vertical_search_range[0] = 48;
  optflow->params.vertical_search_range[1] = 48;
  optflow->params.horizontal_search_range = 191;
  optflow->params.median_filter_enable = 1;
  optflow->params.motion_smoothness_factor = 12;
  optflow->params.motion_direction = 1; /* 0: for side camera */

  /* Initialize modules */
  tiovx_dof_module_init (context, optflow);

  ret = TRUE;

exit:
  return ret;
}

static gboolean
gst_tiovx_optflow_create_graph (GstTIOVXMiso * agg, vx_context context,
    vx_graph graph)
{
  GstTIOVXOptflow *self = NULL;
  TIOVXDofModuleObj *optflow = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  const gchar *target = NULL;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_OPTFLOW (agg);
  optflow = &self->obj;

  /* We read this value here, but the actual target will be done as part of the object params */
  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  GST_DEBUG_OBJECT (self, "Creating optflow graph");
  status = tiovx_dof_module_create (graph, optflow, NULL, NULL, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto exit;
  }

  ret = TRUE;

exit:
  return ret;
}

static gboolean
gst_tiovx_optflow_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects)
{
  GstTIOVXOptflow *optflow = NULL;
  GList *l = NULL;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);
  g_return_val_if_fail (node, FALSE);

  optflow = GST_TIOVX_OPTFLOW (agg);

  for (l = sink_pads_list; l; l = l->next) {
    GstTIOVXMisoPad *pad = l->data;

    if (g_strcmp0 (GST_PAD_TEMPLATE_NAME_TEMPLATE (GST_PAD_PAD_TEMPLATE (pad)),
            "sink") == 0) {
      gst_tiovx_miso_pad_set_params (pad,
          (vx_reference *) & optflow->obj.input.pyramid_handle[0],
          optflow->obj.input.graph_parameter_index, input_param_id);
    } else
        if (g_strcmp0 (GST_PAD_TEMPLATE_NAME_TEMPLATE (GST_PAD_PAD_TEMPLATE
                (pad)), "delayed_sink") == 0) {
      gst_tiovx_miso_pad_set_params (pad,
          (vx_reference *) & optflow->obj.input_ref.pyramid_handle[0],
          optflow->obj.input_ref.graph_parameter_index, input_ref_param_id);
    }
  }

  gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (src_pad),
      (vx_reference *) & optflow->obj.output_flow_vector.image_handle[0],
      optflow->obj.output_flow_vector.graph_parameter_index,
      output_flow_param_id);

  *node = optflow->obj.node;
  return TRUE;
}

static gboolean
gst_tiovx_optflow_configure_module (GstTIOVXMiso * agg)
{
  return TRUE;
}

static gboolean
gst_tiovx_optflow_release_buffer (GstTIOVXMiso * agg)
{
  GstTIOVXOptflow *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (agg, FALSE);

  self = GST_TIOVX_OPTFLOW (agg);

  GST_INFO_OBJECT (self, "Release buffer");
  status = tiovx_dof_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_optflow_deinit_module (GstTIOVXMiso * agg)
{
  GstTIOVXOptflow *self = NULL;
  TIOVXDofModuleObj *optflow = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;

  g_return_val_if_fail (agg, FALSE);

  self = GST_TIOVX_OPTFLOW (agg);
  optflow = &self->obj;

  /* Delete graph */
  status = tiovx_dof_module_delete (optflow);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    goto out;
  }

  status = tiovx_dof_module_deinit (optflow);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    goto out;
  }

  ret = TRUE;
out:
  return ret;
}

static GstCaps *
gst_tiovx_optflow_fixate_caps (GstTIOVXMiso * miso,
    GList * sink_caps_list, GstCaps * src_caps)
{
  GstTIOVXOptflow *self = NULL;
  GstCaps *output_caps = NULL;
  GList *l = NULL;
  gint i = 0;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (sink_caps_list, FALSE);
  g_return_val_if_fail (src_caps, FALSE);

  self = GST_TIOVX_OPTFLOW (miso);
  GST_INFO_OBJECT (miso, "Fixating caps");

  output_caps = gst_caps_copy (src_caps);

  for (l = GST_ELEMENT (self)->sinkpads; l; l = g_list_next (l)) {
    GstPad *sink_pad = l->data;
    GstCaps *sink_caps = gst_pad_get_current_caps (sink_pad);
    GstCaps *sink_caps_copy = NULL;
    GstCaps *output_caps_tmp = NULL;

    if (!sink_caps) {
      sink_caps = gst_pad_peer_query_caps (sink_pad, NULL);
    }
    sink_caps_copy = gst_caps_copy (sink_caps);

    sink_caps_copy = gst_caps_copy (sink_caps);
    gst_caps_unref (sink_caps);

    for (i = 0; i < gst_caps_get_size (sink_caps_copy); i++) {
      GstStructure *structure = gst_caps_get_structure (sink_caps_copy, i);

      /* Remove everything except for number of channels */
      gst_structure_remove_fields (structure, "format", "levels", "scale",
          NULL);

      /* Set the name to application/x-optflow-tiovx in order to intersect */
      gst_structure_set_name (structure, "application/x-optflow-tiovx");
    }

    output_caps_tmp = gst_caps_intersect (output_caps, sink_caps_copy);
    gst_caps_unref (output_caps);

    output_caps = output_caps_tmp;
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

  type = gst_tiovx_optflow_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static void
gst_tiovx_optflow_finalize (GObject * object)
{
  GstTIOVXOptflow *self = GST_TIOVX_OPTFLOW (object);

  GST_LOG_OBJECT (self, "OptFlow finalize");

  G_OBJECT_CLASS (gst_tiovx_optflow_parent_class)->finalize (object);
}
