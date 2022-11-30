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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttiovxdofviz.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_dof_viz_module.h"

#define MIN_THRESHOLD 0
#define MAX_THRESHOLD 15
#define DEFAULT_THRESHOLD 9

#define DOFVIZ_INPUT_PARAM_INDEX 0
#define DOFVIZ_OUTPUT_PARAM_INDEX 2

/* Target definition */
#define GST_TYPE_TIOVX_DOF_VIZ_TARGET (gst_tiovx_dof_viz_target_get_type())
static GType
gst_tiovx_dof_viz_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
#ifdef SOC_J721E
    {TIVX_CPU_ID_DSP1, "DSP instance 1, assigned to C66_0 core",
        TIVX_TARGET_DSP1},
    {TIVX_CPU_ID_DSP2, "DSP instance 2, assigned to C66_1 core",
        TIVX_TARGET_DSP2},
#elif defined(SOC_J721S2) || defined(SOC_J784S4)
    {TIVX_CPU_ID_DSP1, "DSP instance 1, assigned to C7_2 core",
        TIVX_TARGET_DSP1},
#endif
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXDofVizTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_DOF_VIZ_TARGET TIVX_CPU_ID_DSP1

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
  CONFIDENCE_THRESHOLD,
};

/* Formats definition */
#define TIOVX_DOF_VIZ_SUPPORTED_FORMATS_SRC "{RGB}"
#define TIOVX_DOF_VIZ_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_DOF_VIZ_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_DOF_VIZ_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_DOF_VIZ_STATIC_CAPS_SRC                              \
  "video/x-raw, "                                                  \
  "format = (string) " TIOVX_DOF_VIZ_SUPPORTED_FORMATS_SRC ", "    \
  "width = " TIOVX_DOF_VIZ_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DOF_VIZ_SUPPORTED_HEIGHT                       \
  "; "                                                             \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "             \
  "format = (string) " TIOVX_DOF_VIZ_SUPPORTED_FORMATS_SRC ", "    \
  "width = " TIOVX_DOF_VIZ_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DOF_VIZ_SUPPORTED_HEIGHT ", "                  \
  "num-channels = " TIOVX_DOF_VIZ_SUPPORTED_CHANNELS

/* Sink caps */
#define TIOVX_DOF_VIZ_STATIC_CAPS_SINK                             \
  "application/x-dof-tiovx, "                                      \
  "width = " TIOVX_DOF_VIZ_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DOF_VIZ_SUPPORTED_HEIGHT                       \
  "; "                                                             \
  "application/x-dof-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), " \
  "width = " TIOVX_DOF_VIZ_SUPPORTED_WIDTH ", "                    \
  "height = " TIOVX_DOF_VIZ_SUPPORTED_HEIGHT ", "                  \
  "num-channels = " TIOVX_DOF_VIZ_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DOF_VIZ_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_DOF_VIZ_STATIC_CAPS_SRC)
    );

struct _GstTIOVXDofViz
{
  GstTIOVXSiso element;
  gint target_id;
  guint confidence_threshold;
  TIOVXDofVizModuleObj obj;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_dof_viz_debug);
#define GST_CAT_DEFAULT gst_tiovx_dof_viz_debug

#define gst_tiovx_dof_viz_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXDofViz, gst_tiovx_dof_viz, GST_TYPE_TIOVX_SISO);

static void gst_tiovx_dof_viz_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_dof_viz_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_tiovx_dof_viz_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_tiovx_dof_viz_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels);
static gboolean gst_tiovx_dof_viz_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_dof_viz_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node, guint * input_param_index,
    guint * output_param_index);
static gboolean gst_tiovx_dof_viz_release_buffer (GstTIOVXSiso * trans);
static gboolean gst_tiovx_dof_viz_deinit_module (GstTIOVXSiso * trans,
    vx_context context);
static gboolean gst_tiovx_dof_viz_compare_caps (GstTIOVXSiso * trans,
    GstCaps * caps1, GstCaps * caps2, GstPadDirection direction);
static const gchar *target_id_to_target_name (gint target_id);

/* Initialize the plugin's class */
static void
gst_tiovx_dof_viz_class_init (GstTIOVXDofVizClass * klass)
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
      "TIOVX DofViz",
      "Filter/Converter/Video",
      "Converts Dof Flow Vector(VX_DF_IMAGE_U32) to RGB format to visualize",
      "Rahul T R <r-ravikumar@ti.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_tiovx_dof_viz_set_property;
  gobject_class->get_property = gst_tiovx_dof_viz_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_DOF_VIZ_TARGET,
          DEFAULT_TIOVX_DOF_VIZ_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE |
          G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, CONFIDENCE_THRESHOLD,
      g_param_spec_uint ("confidence-threshold", "Confidence",
          "Confidence threshold for dof visualization",
          MIN_THRESHOLD, MAX_THRESHOLD, DEFAULT_THRESHOLD,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dof_viz_transform_caps);
  gsttiovxsiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dof_viz_init_module);
  gsttiovxsiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_dof_viz_create_graph);
  gsttiovxsiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_dof_viz_get_node_info);
  gsttiovxsiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_dof_viz_release_buffer);
  gsttiovxsiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_dof_viz_deinit_module);
  gsttiovxsiso_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_dof_viz_compare_caps);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_dof_viz_debug,
      "tiovxdofviz", 0, "TIOVX DofViz element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_dof_viz_init (GstTIOVXDofViz * self)
{
  self->target_id = DEFAULT_TIOVX_DOF_VIZ_TARGET;
  self->confidence_threshold = DEFAULT_THRESHOLD;
  memset (&self->obj, 0, sizeof self->obj);
}

static void
gst_tiovx_dof_viz_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXDofViz *self = GST_TIOVX_DOF_VIZ (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    case CONFIDENCE_THRESHOLD:
      self->confidence_threshold = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_tiovx_dof_viz_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXDofViz *self = GST_TIOVX_DOF_VIZ (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    case CONFIDENCE_THRESHOLD:
      g_value_set_uint (value, self->confidence_threshold);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static GstCaps *
gst_tiovx_dof_viz_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIOVXDofViz *self = GST_TIOVX_DOF_VIZ (base);
  GstCaps *result_caps = NULL;
  GstStructure *result_structure = NULL;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  if (GST_PAD_SINK == direction) {
    gint i = 0;

    result_caps = gst_caps_from_string (TIOVX_DOF_VIZ_STATIC_CAPS_SRC);

    for (i = 0; i < gst_caps_get_size (result_caps); i++) {
      result_structure = gst_caps_get_structure (result_caps, i);

      if (gst_caps_is_fixed (caps)) {
        /* Transfer input flow vector width and height to output Image */

        gint width = 0;
        gint height = 0;

        gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "width", &width);
        gst_structure_get_int (gst_caps_get_structure (caps, 0),
            "height", &height);

        gst_structure_fixate_field_nearest_int (result_structure,
            "width", width);

        gst_structure_fixate_field_nearest_int (result_structure,
            "height", height);
      }
    }
  } else {
    result_caps = gst_caps_from_string (TIOVX_DOF_VIZ_STATIC_CAPS_SINK);
  }

  if (filter) {
    GstCaps *tmp = result_caps;
    result_caps = gst_caps_intersect (result_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_DEBUG_OBJECT (self, "Resulting caps are %" GST_PTR_FORMAT, result_caps);

  return result_caps;

}

/* GstTIOVXSiso Functions */

static gboolean
gst_tiovx_dof_viz_init_module (GstTIOVXSiso * trans, vx_context context,
    GstCaps * in_caps, GstCaps * out_caps, guint num_channels)
{
  GstTIOVXDofViz *self = NULL;
  vx_status status = VX_SUCCESS;
  TIOVXDofVizModuleObj *dofviz = NULL;
  GstVideoInfo out_info;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (in_caps, FALSE);
  g_return_val_if_fail (out_caps, FALSE);
  g_return_val_if_fail (num_channels >= MIN_NUM_CHANNELS, FALSE);
  g_return_val_if_fail (num_channels <= MAX_NUM_CHANNELS, FALSE);

  self = GST_TIOVX_DOF_VIZ (trans);

  GST_INFO_OBJECT (self, "Init module");

  if (!gst_video_info_from_caps (&out_info, out_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get video info from output caps");
    goto exit;
  }

  /* Configure TIOVXDofVizModuleObj */
  dofviz = &self->obj;
  dofviz->num_channels = num_channels;
  dofviz->input.bufq_depth = 1;
  dofviz->output.bufq_depth = 1;

  dofviz->input.graph_parameter_index = INPUT_PARAMETER_INDEX;
  dofviz->output.graph_parameter_index = OUTPUT_PARAMETER_INDEX;

  dofviz->input.color_format = VX_DF_IMAGE_U32;
  dofviz->output.color_format =
      gst_format_to_vx_format (out_info.finfo->format);

  dofviz->width = GST_VIDEO_INFO_WIDTH (&out_info);
  dofviz->height = GST_VIDEO_INFO_HEIGHT (&out_info);

  dofviz->confidence_threshold_value = self->confidence_threshold;

  status = tiovx_dof_viz_module_init (context, dofviz);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto exit;
  }
  ret = TRUE;

exit:

  return ret;
}

static gboolean
gst_tiovx_dof_viz_create_graph (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph)
{
  GstTIOVXDofViz *self = NULL;
  vx_status status = VX_SUCCESS;
  const char *target = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  self = GST_TIOVX_DOF_VIZ (trans);

  GST_INFO_OBJECT (self, "Create graph");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (NULL == target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    goto out;
  }

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  status = tiovx_dof_viz_module_create (graph, &self->obj, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_dof_viz_release_buffer (GstTIOVXSiso * trans)
{
  GstTIOVXDofViz *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_DOF_VIZ (trans);

  GST_INFO_OBJECT (self, "Release buffer");

  status = tiovx_dof_viz_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dof_viz_deinit_module (GstTIOVXSiso * trans, vx_context context)
{
  GstTIOVXDofViz *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (context, FALSE);

  self = GST_TIOVX_DOF_VIZ (trans);

  GST_INFO_OBJECT (self, "Deinit module");

  status = tiovx_dof_viz_module_delete (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    return FALSE;
  }

  status = tiovx_dof_viz_module_deinit (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_dof_viz_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node,
    guint * input_param_index, guint * output_param_index)
{
  GstTIOVXDofViz *self = NULL;

  g_return_val_if_fail (trans, FALSE);
  self = GST_TIOVX_DOF_VIZ (trans);

  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.node), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.input.image_handle[0]), FALSE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) self->obj.output.image_handle[0]), FALSE);

  GST_INFO_OBJECT (self, "Get node info from module");

  *node = self->obj.node;
  *input = self->obj.input.arr[0];
  *output = self->obj.output.arr[0];
  *input_ref = (vx_reference) self->obj.input.image_handle[0];
  *output_ref = (vx_reference) self->obj.output.image_handle[0];

  *input_param_index = DOFVIZ_INPUT_PARAM_INDEX;
  *output_param_index = DOFVIZ_OUTPUT_PARAM_INDEX;

  return TRUE;
}

static const gchar *
target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_dof_viz_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static gboolean
gst_tiovx_dof_viz_compare_caps (GstTIOVXSiso * trans, GstCaps * caps1,
    GstCaps * caps2, GstPadDirection direction)
{
  GstVideoInfo video_info1;
  GstVideoInfo video_info2;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (caps1, FALSE);
  g_return_val_if_fail (caps2, FALSE);
  g_return_val_if_fail (GST_PAD_UNKNOWN != direction, FALSE);

  if (GST_PAD_SRC == direction) {
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

  if (GST_PAD_SINK == direction) {
    if (gst_caps_is_equal (caps1, caps2)) {
      ret = TRUE;
    }
  }

out:
  return ret;
}
