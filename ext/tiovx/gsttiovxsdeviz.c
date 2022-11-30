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

#include "gsttiovxsdeviz.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxsiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

#include "tiovx_sde_viz_module.h"

#define MIN_VIZ_CONFIDENCE     0
#define MAX_VIZ_CONFIDENCE     255
#define DEFAULT_VIZ_CONFIDENCE 1

#define DEFAULT_DISPARITY_ONLY FALSE

#define SDEVIZ_INPUT_PARAM_INDEX 1
#define SDEVIZ_OUTPUT_PARAM_INDEX 2

/* Target definition */
#define GST_TYPE_TIOVX_SDE_VIZ_TARGET (gst_tiovx_sde_viz_target_get_type())
static GType
gst_tiovx_sde_viz_target_get_type (void)
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
    target_type = g_enum_register_static ("GstTIOVXSdeVizTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_SDE_VIZ_TARGET TIVX_CPU_ID_DSP1

/* Disparity Min definition */
enum
{
  TIVX_MIN_DISPARITY_ZERO = 0,
  TIVX_MIN_DISPARITY_MINUS_THREE = 1,
};

#define GST_TYPE_TIOVX_SDE_VIZ_MIN_DISPARITY (gst_tiovx_sde_viz_min_disparity_get_type())
static GType
gst_tiovx_sde_viz_min_disparity_get_type (void)
{
  static GType min_disparity_type = 0;

  static const GEnumValue min_disparity[] = {
    {TIVX_MIN_DISPARITY_ZERO, "minimum disparity == 0", "minimum_disparity_0"},
    {TIVX_MIN_DISPARITY_MINUS_THREE, "minimum disparity == -3",
        "minimum_disparity_minus_3"},
    {0, NULL, NULL},
  };

  if (!min_disparity_type) {
    min_disparity_type =
        g_enum_register_static ("GstTIOVXSdeVizMinDisparity", min_disparity);
  }
  return min_disparity_type;
}

#define DEFAULT_TIOVX_SDE_VIZ_MIN_DISPARITY TIVX_MIN_DISPARITY_ZERO

/* Disparity Max definition */
enum
{
  TIVX_MAX_DISPARITY_63 = 0,
  TIVX_MAX_DISPARITY_127 = 1,
  TIVX_MAX_DISPARITY_191 = 2,
};

#define GST_TYPE_TIOVX_SDE_VIZ_MAX_DISPARITY (gst_tiovx_sde_viz_max_disparity_get_type())
static GType
gst_tiovx_sde_viz_max_disparity_get_type (void)
{
  static GType max_disparity_type = 0;

  static const GEnumValue max_disparity[] = {
    {TIVX_MAX_DISPARITY_63, "disparity_min + 63", "disparity_max_63"},
    {TIVX_MAX_DISPARITY_127, "disparity_min +127", "disparity_max_127"},
    {TIVX_MAX_DISPARITY_191, "disparity_min +191", "disparity_max_191"},
    {0, NULL, NULL},
  };

  if (!max_disparity_type) {
    max_disparity_type =
        g_enum_register_static ("GstTIOVXSdeVizMaxDisparity", max_disparity);
  }
  return max_disparity_type;
}

#define DEFAULT_TIOVX_SDE_VIZ_MAX_DISPARITY TIVX_MAX_DISPARITY_63

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
  PROP_DISPARITY_MIN,
  PROP_DISPARITY_MAX,
  PROP_DISPARITY_ONLY,
  PROP_VIZ_CONFIDENCE,
};

/* Formats definition */
#define TIOVX_SDE_VIZ_SUPPORTED_FORMATS_SRC "{RGB}"
#define TIOVX_SDE_VIZ_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_SDE_VIZ_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_SDE_VIZ_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_SDE_VIZ_STATIC_CAPS_SRC                           \
  "video/x-raw, "                                               \
  "format = (string) " TIOVX_SDE_VIZ_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_SDE_VIZ_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_SDE_VIZ_SUPPORTED_HEIGHT                    \
  "; "                                                          \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "          \
  "format = (string) " TIOVX_SDE_VIZ_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_SDE_VIZ_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_SDE_VIZ_SUPPORTED_HEIGHT ", "               \
  "num-channels = " TIOVX_SDE_VIZ_SUPPORTED_CHANNELS            \

/* Sink caps */
#define TIOVX_SDE_VIZ_STATIC_CAPS_SINK                              \
  "application/x-sde-tiovx, "                                       \
  "width = " TIOVX_SDE_VIZ_SUPPORTED_WIDTH ", "                     \
  "height = " TIOVX_SDE_VIZ_SUPPORTED_HEIGHT                        \
  "; "                                                              \
  "application/x-sde-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "  \
  "width = " TIOVX_SDE_VIZ_SUPPORTED_WIDTH ", "                     \
  "height = " TIOVX_SDE_VIZ_SUPPORTED_HEIGHT ", "                   \
  "num-channels = " TIOVX_SDE_VIZ_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_SDE_VIZ_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_SDE_VIZ_STATIC_CAPS_SRC)
    );

struct _GstTIOVXSdeViz
{
  GstTIOVXSiso element;

  TIOVXSdeVizModuleObj obj;

  gint target_id;

  gint disparity_max;
  gint disparity_min;
  gboolean disparity_only;
  guint viz_confidence;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_sde_viz_debug);
#define GST_CAT_DEFAULT gst_tiovx_sde_viz_debug

#define gst_tiovx_sde_viz_parent_class parent_class
G_DEFINE_TYPE (GstTIOVXSdeViz, gst_tiovx_sde_viz, GST_TYPE_TIOVX_SISO);

static void gst_tiovx_sde_viz_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tiovx_sde_viz_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_tiovx_sde_viz_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_tiovx_sde_viz_init_module (GstTIOVXSiso * trans,
    vx_context context, GstCaps * in_caps, GstCaps * out_caps,
    guint num_channels);
static gboolean gst_tiovx_sde_viz_create_graph (GstTIOVXSiso * trans,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_sde_viz_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node, guint * input_param_index,
    guint * output_param_index);
static gboolean gst_tiovx_sde_viz_release_buffer (GstTIOVXSiso * trans);
static gboolean gst_tiovx_sde_viz_deinit_module (GstTIOVXSiso * trans,
    vx_context context);
static gboolean gst_tiovx_sde_viz_compare_caps (GstTIOVXSiso * trans,
    GstCaps * caps1, GstCaps * caps2, GstPadDirection direction);
static const gchar *target_id_to_target_name (gint target_id);

/* Initialize the plugin's class */
static void
gst_tiovx_sde_viz_class_init (GstTIOVXSdeVizClass * klass)
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
      "TIOVX SdeViz",
      "Filter/Converter/Video",
      "Converts Sde Output(VX_DF_IMAGE_S16) to RGB format to visualize",
      "Rahul T R <r-ravikumar@ti.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_tiovx_sde_viz_set_property;
  gobject_class->get_property = gst_tiovx_sde_viz_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_SDE_VIZ_TARGET,
          DEFAULT_TIOVX_SDE_VIZ_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISPARITY_MIN,
      g_param_spec_enum ("disparity-min", "Disparity Min",
          "Minimum Disparity",
          GST_TYPE_TIOVX_SDE_VIZ_MIN_DISPARITY,
          DEFAULT_TIOVX_SDE_VIZ_MIN_DISPARITY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISPARITY_MAX,
      g_param_spec_enum ("disparity-max", "Disparity Max",
          "Maximum Disparity",
          GST_TYPE_TIOVX_SDE_VIZ_MAX_DISPARITY,
          DEFAULT_TIOVX_SDE_VIZ_MAX_DISPARITY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_DISPARITY_ONLY,
      g_param_spec_boolean ("disparity-only",
          "Disparity Only",
          "Disparity Only",
          DEFAULT_DISPARITY_ONLY,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VIZ_CONFIDENCE,
      g_param_spec_uint ("viz-confidence", "Confidence",
          "Confidence threshold for sde visualization",
          MIN_VIZ_CONFIDENCE, MAX_VIZ_CONFIDENCE,
          DEFAULT_VIZ_CONFIDENCE, G_PARAM_READWRITE));

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_viz_transform_caps);
  gsttiovxsiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_viz_init_module);
  gsttiovxsiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_viz_create_graph);
  gsttiovxsiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_viz_get_node_info);
  gsttiovxsiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_viz_release_buffer);
  gsttiovxsiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_viz_deinit_module);
  gsttiovxsiso_class->compare_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_viz_compare_caps);

  GST_DEBUG_CATEGORY_INIT (gst_tiovx_sde_viz_debug,
      "tiovxsdeviz", 0, "TIOVX SdeViz element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_sde_viz_init (GstTIOVXSdeViz * self)
{
  memset (&self->obj, 0, sizeof self->obj);

  self->target_id = DEFAULT_TIOVX_SDE_VIZ_TARGET;
  self->disparity_max = DEFAULT_TIOVX_SDE_VIZ_MAX_DISPARITY;
  self->disparity_min = DEFAULT_TIOVX_SDE_VIZ_MIN_DISPARITY;
  self->disparity_only = DEFAULT_DISPARITY_ONLY;
  self->viz_confidence = DEFAULT_VIZ_CONFIDENCE;
}

static void
gst_tiovx_sde_viz_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXSdeViz *self = GST_TIOVX_SDE_VIZ (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    case PROP_DISPARITY_MIN:
      self->disparity_min = g_value_get_enum (value);
      break;
    case PROP_DISPARITY_MAX:
      self->disparity_max = g_value_get_enum (value);
      break;
    case PROP_DISPARITY_ONLY:
      self->disparity_only = g_value_get_boolean (value);
      break;
    case PROP_VIZ_CONFIDENCE:
      self->viz_confidence = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_tiovx_sde_viz_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXSdeViz *self = GST_TIOVX_SDE_VIZ (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    case PROP_DISPARITY_MIN:
      g_value_set_enum (value, self->disparity_min);
      break;
    case PROP_DISPARITY_MAX:
      g_value_set_enum (value, self->disparity_max);
      break;
    case PROP_DISPARITY_ONLY:
      g_value_set_boolean (value, self->disparity_only);
      break;
    case PROP_VIZ_CONFIDENCE:
      g_value_set_uint (value, self->viz_confidence);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static GstCaps *
gst_tiovx_sde_viz_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIOVXSdeViz *self = GST_TIOVX_SDE_VIZ (base);
  GstCaps *result_caps = NULL;
  GstStructure *result_structure = NULL;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  if (GST_PAD_SINK == direction) {
    gint i = 0;

    result_caps = gst_caps_from_string (TIOVX_SDE_VIZ_STATIC_CAPS_SRC);

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
    result_caps = gst_caps_from_string (TIOVX_SDE_VIZ_STATIC_CAPS_SINK);
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
gst_tiovx_sde_viz_init_module (GstTIOVXSiso * trans, vx_context context,
    GstCaps * in_caps, GstCaps * out_caps, guint num_channels)
{
  GstTIOVXSdeViz *self = NULL;
  vx_status status = VX_SUCCESS;
  TIOVXSdeVizModuleObj *sdeviz = NULL;
  GstVideoInfo out_info;
  gboolean ret = FALSE;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (in_caps, FALSE);
  g_return_val_if_fail (out_caps, FALSE);
  g_return_val_if_fail (num_channels >= MIN_NUM_CHANNELS, FALSE);
  g_return_val_if_fail (num_channels <= MAX_NUM_CHANNELS, FALSE);

  self = GST_TIOVX_SDE_VIZ (trans);

  GST_INFO_OBJECT (self, "Init module");

  if (!gst_video_info_from_caps (&out_info, out_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get video info from output caps");
    goto exit;
  }

  /* Configure TIOVXSdeVizModuleObj */
  sdeviz = &self->obj;
  sdeviz->num_channels = num_channels;
  sdeviz->input.bufq_depth = 1;
  sdeviz->output.bufq_depth = 1;

  sdeviz->input.graph_parameter_index = INPUT_PARAMETER_INDEX;
  sdeviz->output.graph_parameter_index = OUTPUT_PARAMETER_INDEX;

  sdeviz->input.color_format = VX_DF_IMAGE_S16;
  sdeviz->output.color_format =
      gst_format_to_vx_format (out_info.finfo->format);

  sdeviz->width = GST_VIDEO_INFO_WIDTH (&out_info);
  sdeviz->height = GST_VIDEO_INFO_HEIGHT (&out_info);

  sdeviz->params.disparity_min = self->disparity_min;
  sdeviz->params.disparity_max = self->disparity_max;
  sdeviz->params.disparity_only = self->disparity_only;
  sdeviz->params.vis_confidence = self->viz_confidence;

  status = tiovx_sde_viz_module_init (context, sdeviz);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto exit;
  }
  ret = TRUE;

exit:

  return ret;
}

static gboolean
gst_tiovx_sde_viz_create_graph (GstTIOVXSiso * trans, vx_context context,
    vx_graph graph)
{
  GstTIOVXSdeViz *self = NULL;
  vx_status status = VX_SUCCESS;
  const char *target = NULL;
  gboolean ret = FALSE;
  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) context),
      FALSE);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) graph),
      FALSE);

  self = GST_TIOVX_SDE_VIZ (trans);

  GST_INFO_OBJECT (self, "Create graph");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (NULL == target) {
    GST_ERROR_OBJECT (self, "TIOVX target selection failed");
    goto out;
  }

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  tivxStereoLoadKernels (context);

  status = tiovx_sde_viz_module_create (graph, &self->obj, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_sde_viz_release_buffer (GstTIOVXSiso * trans)
{
  GstTIOVXSdeViz *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);

  self = GST_TIOVX_SDE_VIZ (trans);

  GST_INFO_OBJECT (self, "Release buffer");

  status = tiovx_sde_viz_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_sde_viz_deinit_module (GstTIOVXSiso * trans, vx_context context)
{
  GstTIOVXSdeViz *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (trans, FALSE);
  g_return_val_if_fail (context, FALSE);

  self = GST_TIOVX_SDE_VIZ (trans);

  GST_INFO_OBJECT (self, "Deinit module");

  tivxStereoUnLoadKernels (context);

  status = tiovx_sde_viz_module_delete (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module delete failed with error: %d", status);
    return FALSE;
  }

  status = tiovx_sde_viz_module_deinit (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_sde_viz_get_node_info (GstTIOVXSiso * trans,
    vx_object_array * input, vx_object_array * output, vx_reference * input_ref,
    vx_reference * output_ref, vx_node * node,
    guint * input_param_index, guint * output_param_index)
{
  GstTIOVXSdeViz *self = NULL;

  g_return_val_if_fail (trans, FALSE);
  self = GST_TIOVX_SDE_VIZ (trans);

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

  *input_param_index = SDEVIZ_INPUT_PARAM_INDEX;
  *output_param_index = SDEVIZ_OUTPUT_PARAM_INDEX;

  return TRUE;
}

static const gchar *
target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_sde_viz_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static gboolean
gst_tiovx_sde_viz_compare_caps (GstTIOVXSiso * trans, GstCaps * caps1,
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
