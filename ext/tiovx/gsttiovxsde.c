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

#include "gsttiovxsde.h"

#include "unistd.h"

#include <tiovx_sde_module.h>

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxallocator.h"
#include "gst-libs/gst/tiovx/gsttiovxmiso.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"

static const int input_left_param_id = 1;
static const int input_right_param_id = 2;
static const int output_param_id = 3;

static const gboolean median_filter_enable_default = TRUE;
static const gboolean reduced_range_search_enable_default = FALSE;
static const gboolean texture_filter_enable_default = FALSE;
static const int threshold_left_right_min = 0;
static const int threshold_left_right_max = 255;
static const int threshold_left_right_default = 3;
static const int threshold_texture_min = 0;
static const int threshold_texture_max = 255;
static const int threshold_texture_default = 100;
static const int aggregation_penalty_p1_min = 0;
static const int aggregation_penalty_p1_max = 127;
static const int aggregation_penalty_p1_default = 32;
static const int aggregation_penalty_p2_min = 0;
static const int aggregation_penalty_p2_max = 255;
static const int aggregation_penalty_p2_default = 192;
static const int confidence_score_map_min = 0;
static const int confidence_score_map_max = 127;
static const int confidence_score_map_0_default = 0;
static const int confidence_score_map_1_default = 1;
static const int confidence_score_map_2_default = 2;
static const int confidence_score_map_3_default = 3;
static const int confidence_score_map_4_default = 5;
static const int confidence_score_map_5_default = 9;
static const int confidence_score_map_6_default = 14;
static const int confidence_score_map_7_default = 127;

/* TIOVX Sde */

/* Target definition */
enum
{
  TIVX_TARGET_DMPAC_SDE_ID = 0,
};

#define GST_TYPE_TIOVX_SDE_TARGET (gst_tiovx_sde_target_get_type())
static GType
gst_tiovx_sde_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_DMPAC_SDE_ID, "DMPAC SDE", TIVX_TARGET_DMPAC_SDE},
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXSdeTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_SDE_TARGET TIVX_TARGET_DMPAC_SDE_ID

/* Disparity Min definition */
enum
{
  TIVX_MIN_DISPARITY_ZERO = 0,
  TIVX_MIN_DISPARITY_MINUS_THREE = 1,
};

#define GST_TYPE_TIOVX_SDE_MIN_DISPARITY (gst_tiovx_sde_min_disparity_get_type())
static GType
gst_tiovx_sde_min_disparity_get_type (void)
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
        g_enum_register_static ("GstTIOVXSdeMinDisparity", min_disparity);
  }
  return min_disparity_type;
}

#define DEFAULT_TIOVX_SDE_MIN_DISPARITY TIVX_MIN_DISPARITY_ZERO

/* Disparity Max definition */
enum
{
  TIVX_MAX_DISPARITY_63 = 0,
  TIVX_MAX_DISPARITY_127 = 1,
  TIVX_MAX_DISPARITY_191 = 2,
};

#define GST_TYPE_TIOVX_SDE_MAX_DISPARITY (gst_tiovx_sde_max_disparity_get_type())
static GType
gst_tiovx_sde_max_disparity_get_type (void)
{
  static GType max_disparity_type = 0;

  static const GEnumValue max_disparity[] = {
    {TIVX_MAX_DISPARITY_63, "disparity_min +63", "disparity_max_63"},
    {TIVX_MAX_DISPARITY_127, "disparity_min +127", "disparity_max_127"},
    {TIVX_MAX_DISPARITY_191, "disparity_min +191", "disparity_max_191"},
    {0, NULL, NULL},
  };

  if (!max_disparity_type) {
    max_disparity_type =
        g_enum_register_static ("GstTIOVXSdeMaxDisparity", max_disparity);
  }
  return max_disparity_type;
}

#define DEFAULT_TIOVX_SDE_MAX_DISPARITY TIVX_MAX_DISPARITY_63

/* Properties definition */
enum
{
  PROP_0,
  PROP_TARGET,
  PROP_MEDIAN_FILTER_ENABLE,
  PROP_REDUCED_RANGE_SEARCH_ENABLE,
  PROP_TEXTURE_FILTER_ENABLE,
  PROP_THRESHOLD_LEFT_RIGHT,
  PROP_THRESHOLD_TEXTURE,
  PROP_AGGREGATION_PENALTY_P1,
  PROP_AGGREGATION_PENALTY_P2,
  PROP_CONFIDENCE_SCORE_MAP_0,
  PROP_CONFIDENCE_SCORE_MAP_1,
  PROP_CONFIDENCE_SCORE_MAP_2,
  PROP_CONFIDENCE_SCORE_MAP_3,
  PROP_CONFIDENCE_SCORE_MAP_4,
  PROP_CONFIDENCE_SCORE_MAP_5,
  PROP_CONFIDENCE_SCORE_MAP_6,
  PROP_CONFIDENCE_SCORE_MAP_7,
  PROP_DISPARITY_MIN,
  PROP_DISPARITY_MAX,
};

/* Formats definition */

#define TIOVX_SDE_SUPPORTED_FORMATS "{GRAY8, GRAY16_LE, NV12}"
#define TIOVX_SDE_SUPPORTED_WIDTH "[1 , 2048]"
#define TIOVX_SDE_SUPPORTED_HEIGHT "[1 , 1024]"
#define TIOVX_SDE_SUPPORTED_CHANNELS "[1 , 16]"

/* Src caps */
#define TIOVX_SDE_STATIC_CAPS_SINK                         \
  "video/x-raw, "                                          \
  "format = (string) " TIOVX_SDE_SUPPORTED_FORMATS ", "    \
  "width = " TIOVX_SDE_SUPPORTED_WIDTH ", "                \
  "height = " TIOVX_SDE_SUPPORTED_HEIGHT                   \
  "; "                                                     \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "     \
  "format = (string) " TIOVX_SDE_SUPPORTED_FORMATS ", "    \
  "width = " TIOVX_SDE_SUPPORTED_WIDTH ", "                \
  "height = " TIOVX_SDE_SUPPORTED_HEIGHT ", "              \
  "num-channels = " TIOVX_SDE_SUPPORTED_CHANNELS


#define TIOVX_SDE_STATIC_CAPS_SRC                                  \
  "application/x-sde-tiovx, "                                      \
  "width = " TIOVX_SDE_SUPPORTED_WIDTH ", "                        \
  "height = " TIOVX_SDE_SUPPORTED_HEIGHT                           \
  "; "                                                             \
  "application/x-sde-tiovx(" GST_CAPS_FEATURE_BATCHED_MEMORY "), " \
  "width = " TIOVX_SDE_SUPPORTED_WIDTH ", "                        \
  "height = " TIOVX_SDE_SUPPORTED_HEIGHT ", "                      \
  "num-channels = " TIOVX_SDE_SUPPORTED_CHANNELS

/* Pads definitions */
static GstStaticPadTemplate left_sink_template =
GST_STATIC_PAD_TEMPLATE ("left_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_SDE_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate right_sink_template =
GST_STATIC_PAD_TEMPLATE ("right_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_SDE_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_SDE_STATIC_CAPS_SRC)
    );


struct _GstTIOVXSde
{
  GstTIOVXMiso parent;

  TIOVXSdeModuleObj obj;

  gint target_id;

  gboolean median_filter_enable;
  gboolean reduced_range_search_enable;
  gboolean texture_filter_enable;
  gint threshold_left_right;
  gint threshold_texture;
  gint aggregation_penalty_p1;
  gint aggregation_penalty_p2;
  gint confidence_score_map[8];
  gint disparity_max;
  gint disparity_min;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_sde_debug);
#define GST_CAT_DEFAULT gst_tiovx_sde_debug

#define gst_tiovx_sde_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXSde, gst_tiovx_sde,
    GST_TYPE_TIOVX_MISO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_sde_debug,
        "tiovxsde", 0, "debug category for the tiovxsde element"));

static const gchar *target_id_to_target_name (gint target_id);
static void
gst_tiovx_sde_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_sde_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_tiovx_sde_init_module (GstTIOVXMiso * agg,
    vx_context context, GList * sink_pads_list, GstPad * src_pad,
    guint num_channels);
static gboolean gst_tiovx_sde_create_graph (GstTIOVXMiso * agg,
    vx_context context, vx_graph graph);
static gboolean gst_tiovx_sde_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects);
static gboolean gst_tiovx_sde_configure_module (GstTIOVXMiso * agg);
static gboolean gst_tiovx_sde_release_buffer (GstTIOVXMiso * agg);
static gboolean gst_tiovx_sde_deinit_module (GstTIOVXMiso * agg);
static GstCaps *gst_tiovx_sde_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels);
static void gst_tiovx_sde_finalize (GObject * object);

static void
gst_tiovx_sde_class_init (GstTIOVXSdeClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstElementClass *gstelement_class = NULL;
  GstTIOVXMisoClass *gsttiovxmiso_class = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gsttiovxmiso_class = GST_TIOVX_MISO_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Sde",
      "Filter",
      "Sde using the TIOVX Modules API", "Rahul T R <r-ravikumar@ti.com>");

  gobject_class->set_property = gst_tiovx_sde_set_property;
  gobject_class->get_property = gst_tiovx_sde_get_property;

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element",
          GST_TYPE_TIOVX_SDE_TARGET,
          DEFAULT_TIOVX_SDE_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MEDIAN_FILTER_ENABLE,
      g_param_spec_boolean ("median-filter-enable", "Median filter enable",
          "Enable post-processing median filter", median_filter_enable_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_REDUCED_RANGE_SEARCH_ENABLE,
      g_param_spec_boolean ("reduced-range-search-enable",
          "Reduced Range Search Enable",
          "Enable reduced range search on pixels near right margin",
          reduced_range_search_enable_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_TEXTURE_FILTER_ENABLE,
      g_param_spec_boolean ("texture-filter-enable",
          "Texture Filter Enable",
          "Enable texture based filtering",
          texture_filter_enable_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_THRESHOLD_LEFT_RIGHT,
      g_param_spec_int ("threshold-left-right", "Threshold Left Right",
          "Left-right consistency check threshold in pixels",
          threshold_left_right_min, threshold_left_right_max,
          threshold_left_right_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_THRESHOLD_TEXTURE,
      g_param_spec_int ("threshold-texture", "Threshold Texture",
          "If texture filter enable, Scaled texture threshold",
          threshold_texture_min, threshold_texture_max,
          threshold_texture_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_AGGREGATION_PENALTY_P1,
      g_param_spec_int ("aggregation-penalty-p1", "Aggregation Penalty P1",
          "SDE aggragation penalty P1. Optimization penalty constant for "
          "small disparity change.",
          aggregation_penalty_p1_min, aggregation_penalty_p1_max,
          aggregation_penalty_p1_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_AGGREGATION_PENALTY_P2,
      g_param_spec_int ("aggregation-penalty-p2", "Aggregation Penalty P2",
          "SDE aggragation penalty P2. Optimization penalty constant for "
          "large disparity change.",
          aggregation_penalty_p2_min, aggregation_penalty_p2_max,
          aggregation_penalty_p2_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONFIDENCE_SCORE_MAP_0,
      g_param_spec_int ("confidence-score-map-0", "Confidence Score Map 0",
          "index 0 of array that defines custom ranges for mapping 7-bit"
          "confidence score to one of 8 levels",
          confidence_score_map_min, confidence_score_map_max,
          confidence_score_map_0_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONFIDENCE_SCORE_MAP_1,
      g_param_spec_int ("confidence-score-map-1", "Confidence Score Map 1",
          "index 1 of array that defines custom ranges for mapping 7-bit"
          "confidence score to one of 8 levels",
          confidence_score_map_min, confidence_score_map_max,
          confidence_score_map_1_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONFIDENCE_SCORE_MAP_2,
      g_param_spec_int ("confidence-score-map-2", "Confidence Score Map 2",
          "index 2 of array that defines custom ranges for mapping 7-bit"
          "confidence score to one of 8 levels",
          confidence_score_map_min, confidence_score_map_max,
          confidence_score_map_2_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONFIDENCE_SCORE_MAP_3,
      g_param_spec_int ("confidence-score-map-3", "Confidence Score Map 3",
          "index 3 of array that defines custom ranges for mapping 7-bit"
          "confidence score to one of 8 levels",
          confidence_score_map_min, confidence_score_map_max,
          confidence_score_map_3_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONFIDENCE_SCORE_MAP_4,
      g_param_spec_int ("confidence-score-map-4", "Confidence Score Map 4",
          "index 4 of array that defines custom ranges for mapping 7-bit"
          "confidence score to one of 8 levels",
          confidence_score_map_min, confidence_score_map_max,
          confidence_score_map_4_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONFIDENCE_SCORE_MAP_5,
      g_param_spec_int ("confidence-score-map-5", "Confidence Score Map 5",
          "index 5 of array that defines custom ranges for mapping 7-bit"
          "confidence score to one of 8 levels",
          confidence_score_map_min, confidence_score_map_max,
          confidence_score_map_5_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONFIDENCE_SCORE_MAP_6,
      g_param_spec_int ("confidence-score-map-6", "Confidence Score Map 6",
          "index 6 of array that defines custom ranges for mapping 7-bit"
          "confidence score to one of 8 levels",
          confidence_score_map_min, confidence_score_map_max,
          confidence_score_map_6_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CONFIDENCE_SCORE_MAP_7,
      g_param_spec_int ("confidence-score-map-7", "Confidence Score Map 7",
          "index 7 of array that defines custom ranges for mapping 7-bit"
          "confidence score to one of 8 levels",
          confidence_score_map_min, confidence_score_map_max,
          confidence_score_map_7_default,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISPARITY_MIN,
      g_param_spec_enum ("disparity-min", "Disparity Min",
          "Minimum Disparity",
          GST_TYPE_TIOVX_SDE_MIN_DISPARITY,
          DEFAULT_TIOVX_SDE_MIN_DISPARITY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISPARITY_MAX,
      g_param_spec_enum ("disparity-max", "Disparity Max",
          "Maximum Disparity",
          GST_TYPE_TIOVX_SDE_MAX_DISPARITY,
          DEFAULT_TIOVX_SDE_MAX_DISPARITY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &src_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &left_sink_template, GST_TYPE_TIOVX_MISO_PAD);

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &right_sink_template, GST_TYPE_TIOVX_MISO_PAD);

  gsttiovxmiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_init_module);

  gsttiovxmiso_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_configure_module);

  gsttiovxmiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_get_node_info);

  gsttiovxmiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_create_graph);

  gsttiovxmiso_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_fixate_caps);

  gsttiovxmiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_release_buffer);

  gsttiovxmiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_sde_deinit_module);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_sde_finalize);
}

static void
gst_tiovx_sde_init (GstTIOVXSde * self)
{
  memset (&self->obj, 0, sizeof (self->obj));

  self->target_id = DEFAULT_TIOVX_SDE_TARGET;
  self->median_filter_enable = median_filter_enable_default;
  self->reduced_range_search_enable = reduced_range_search_enable_default;
  self->texture_filter_enable = texture_filter_enable_default;
  self->threshold_left_right = threshold_left_right_default;
  self->threshold_texture = threshold_texture_default;
  self->aggregation_penalty_p1 = aggregation_penalty_p1_default;
  self->aggregation_penalty_p2 = aggregation_penalty_p2_default;
  self->confidence_score_map[0] = confidence_score_map_0_default;
  self->confidence_score_map[1] = confidence_score_map_1_default;
  self->confidence_score_map[2] = confidence_score_map_2_default;
  self->confidence_score_map[3] = confidence_score_map_3_default;
  self->confidence_score_map[4] = confidence_score_map_4_default;
  self->confidence_score_map[5] = confidence_score_map_5_default;
  self->confidence_score_map[6] = confidence_score_map_6_default;
  self->confidence_score_map[7] = confidence_score_map_7_default;
  self->disparity_min = DEFAULT_TIOVX_SDE_MIN_DISPARITY;
  self->disparity_max = DEFAULT_TIOVX_SDE_MAX_DISPARITY;
}

static void
gst_tiovx_sde_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXSde *self = GST_TIOVX_SDE (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    case PROP_MEDIAN_FILTER_ENABLE:
      self->median_filter_enable = g_value_get_boolean (value);
      break;
    case PROP_REDUCED_RANGE_SEARCH_ENABLE:
      self->reduced_range_search_enable = g_value_get_boolean (value);
      break;
    case PROP_TEXTURE_FILTER_ENABLE:
      self->texture_filter_enable = g_value_get_boolean (value);
      break;
    case PROP_THRESHOLD_LEFT_RIGHT:
      self->threshold_left_right = g_value_get_int (value);
      break;
    case PROP_THRESHOLD_TEXTURE:
      self->threshold_texture = g_value_get_int (value);
      break;
    case PROP_AGGREGATION_PENALTY_P1:
      self->aggregation_penalty_p1 = g_value_get_int (value);
      break;
    case PROP_AGGREGATION_PENALTY_P2:
      self->aggregation_penalty_p2 = g_value_get_int (value);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_0:
      self->confidence_score_map[0] = g_value_get_int (value);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_1:
      self->confidence_score_map[1] = g_value_get_int (value);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_2:
      self->confidence_score_map[2] = g_value_get_int (value);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_3:
      self->confidence_score_map[3] = g_value_get_int (value);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_4:
      self->confidence_score_map[4] = g_value_get_int (value);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_5:
      self->confidence_score_map[5] = g_value_get_int (value);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_6:
      self->confidence_score_map[6] = g_value_get_int (value);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_7:
      self->confidence_score_map[7] = g_value_get_int (value);
      break;
    case PROP_DISPARITY_MIN:
      self->disparity_min = g_value_get_enum (value);
      break;
    case PROP_DISPARITY_MAX:
      self->disparity_max = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_sde_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXSde *self = GST_TIOVX_SDE (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    case PROP_MEDIAN_FILTER_ENABLE:
      g_value_set_boolean (value, self->median_filter_enable);
      break;
    case PROP_REDUCED_RANGE_SEARCH_ENABLE:
      g_value_set_boolean (value, self->reduced_range_search_enable);
      break;
    case PROP_TEXTURE_FILTER_ENABLE:
      g_value_set_boolean (value, self->texture_filter_enable);
      break;
    case PROP_THRESHOLD_LEFT_RIGHT:
      g_value_set_int (value, self->threshold_left_right);
      break;
    case PROP_THRESHOLD_TEXTURE:
      g_value_set_int (value, self->threshold_texture);
      break;
    case PROP_AGGREGATION_PENALTY_P1:
      g_value_set_int (value, self->aggregation_penalty_p1);
      break;
    case PROP_AGGREGATION_PENALTY_P2:
      g_value_set_int (value, self->aggregation_penalty_p2);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_0:
      g_value_set_int (value, self->confidence_score_map[0]);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_1:
      g_value_set_int (value, self->confidence_score_map[1]);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_2:
      g_value_set_int (value, self->confidence_score_map[2]);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_3:
      g_value_set_int (value, self->confidence_score_map[3]);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_4:
      g_value_set_int (value, self->confidence_score_map[4]);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_5:
      g_value_set_int (value, self->confidence_score_map[5]);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_6:
      g_value_set_int (value, self->confidence_score_map[6]);
      break;
    case PROP_CONFIDENCE_SCORE_MAP_7:
      g_value_set_int (value, self->confidence_score_map[7]);
      break;
    case PROP_DISPARITY_MIN:
      g_value_set_enum (value, self->disparity_min);
      break;
    case PROP_DISPARITY_MAX:
      g_value_set_enum (value, self->disparity_max);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_sde_init_module (GstTIOVXMiso * agg, vx_context context,
    GList * sink_pads_list, GstPad * src_pad, guint num_channels)
{
  GstTIOVXSde *self = NULL;
  TIOVXSdeModuleObj *sde = NULL;
  GstCaps *caps = NULL;
  GList *l = NULL;
  gboolean ret = FALSE;
  GstVideoInfo video_info = {
  };

  gint i = 0;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);
  g_return_val_if_fail (num_channels, FALSE);

  self = GST_TIOVX_SDE (agg);
  sde = &self->obj;

  GST_INFO_OBJECT (self, "Init module");

  /* Initialize the output parameters */
  for (l = sink_pads_list; l != NULL; l = g_list_next (l)) {
    GstTIOVXMisoPad *sink_pad = GST_TIOVX_MISO_PAD (l->data);
    GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

    caps = gst_pad_get_current_caps (GST_PAD (sink_pad));
    if (!caps) {
      caps = gst_pad_peer_query_caps (GST_PAD (sink_pad), NULL);
    }

    ret = gst_video_info_from_caps (&video_info, caps);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
          GST_PTR_FORMAT, caps);
      gst_caps_unref (caps);
      goto exit;
    }
    gst_caps_unref (caps);

    sde->width = GST_VIDEO_INFO_WIDTH (&video_info);
    sde->height = GST_VIDEO_INFO_HEIGHT (&video_info);
    format = video_info.finfo->format;

    /* We do both inputs at once, since they should be equal */
    sde->input_left.bufq_depth = 1;
    sde->input_left.color_format = gst_format_to_vx_format (format);
    sde->input_left.graph_parameter_index = i;
    i++;
    sde->input_right.bufq_depth = 1;
    sde->input_right.color_format = gst_format_to_vx_format (format);
    sde->input_right.graph_parameter_index = i;
    i++;

    GST_INFO_OBJECT (self,
        "Input Left & Input Right parameters: \n\tWidth: %d \n\tHeight: %d",
        sde->width, sde->height);
    break;
  }

  sde->output.bufq_depth = 1;
  sde->output.color_format = VX_DF_IMAGE_S16;
  sde->output.graph_parameter_index = i;
  i++;

  sde->num_channels = num_channels;

  sde->params.median_filter_enable = self->median_filter_enable;
  sde->params.reduced_range_search_enable = self->reduced_range_search_enable;
  sde->params.disparity_min = self->disparity_min;
  sde->params.disparity_max = self->disparity_max;
  sde->params.threshold_left_right = self->threshold_left_right;
  sde->params.texture_filter_enable = self->texture_filter_enable;
  sde->params.threshold_texture = self->threshold_texture;
  sde->params.aggregation_penalty_p1 = self->aggregation_penalty_p1;
  sde->params.aggregation_penalty_p2 = self->aggregation_penalty_p2;
  sde->params.confidence_score_map[0] = self->confidence_score_map[0];
  sde->params.confidence_score_map[1] = self->confidence_score_map[1];
  sde->params.confidence_score_map[2] = self->confidence_score_map[2];
  sde->params.confidence_score_map[3] = self->confidence_score_map[3];
  sde->params.confidence_score_map[4] = self->confidence_score_map[4];
  sde->params.confidence_score_map[5] = self->confidence_score_map[5];
  sde->params.confidence_score_map[6] = self->confidence_score_map[6];
  sde->params.confidence_score_map[7] = self->confidence_score_map[7];

  /* Initialize modules */
  tiovx_sde_module_init (context, sde);

  ret = TRUE;

exit:
  return ret;
}

static gboolean
gst_tiovx_sde_create_graph (GstTIOVXMiso * agg, vx_context context,
    vx_graph graph)
{
  GstTIOVXSde *self = NULL;
  TIOVXSdeModuleObj *sde = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  const gchar *target = NULL;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_SDE (agg);
  sde = &self->obj;

  /* We read this value here, but the actual target will be done as part of the object params */
  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  GST_DEBUG_OBJECT (self, "Creating sde graph");
  status = tiovx_sde_module_create (graph, sde, NULL, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto exit;
  }

  ret = TRUE;

exit:
  return ret;
}

static gboolean
gst_tiovx_sde_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects)
{
  GstTIOVXSde *sde = NULL;
  GList *l = NULL;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);
  g_return_val_if_fail (node, FALSE);

  sde = GST_TIOVX_SDE (agg);

  for (l = sink_pads_list; l; l = l->next) {
    GstTIOVXMisoPad *pad = l->data;

    if (g_strcmp0 (GST_PAD_TEMPLATE_NAME_TEMPLATE (GST_PAD_PAD_TEMPLATE (pad)),
            "left_sink") == 0) {
      gst_tiovx_miso_pad_set_params (pad, sde->obj.input_left.arr[0],
          (vx_reference *) & sde->obj.input_left.image_handle[0],
          sde->obj.input_left.graph_parameter_index, input_left_param_id);
    } else
        if (g_strcmp0 (GST_PAD_TEMPLATE_NAME_TEMPLATE (GST_PAD_PAD_TEMPLATE
                (pad)), "right_sink") == 0) {
      gst_tiovx_miso_pad_set_params (pad, sde->obj.input_right.arr[0],
          (vx_reference *) & sde->obj.input_right.image_handle[0],
          sde->obj.input_right.graph_parameter_index, input_right_param_id);
    }
  }

  gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (src_pad),
      sde->obj.output.arr[0],
      (vx_reference *) & sde->obj.output.image_handle[0],
      sde->obj.output.graph_parameter_index, output_param_id);

  *node = sde->obj.node;
  return TRUE;
}

static gboolean
gst_tiovx_sde_configure_module (GstTIOVXMiso * agg)
{
  return TRUE;
}

static gboolean
gst_tiovx_sde_release_buffer (GstTIOVXMiso * agg)
{
  GstTIOVXSde *self = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (agg, FALSE);

  self = GST_TIOVX_SDE (agg);

  GST_INFO_OBJECT (self, "Release buffer");
  status = tiovx_sde_module_release_buffers (&self->obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Release buffer failed with error: %d", status);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_tiovx_sde_deinit_module (GstTIOVXMiso * agg)
{
  GstTIOVXSde *self = NULL;
  TIOVXSdeModuleObj *sde = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;

  g_return_val_if_fail (agg, FALSE);

  self = GST_TIOVX_SDE (agg);
  sde = &self->obj;

  GST_INFO_OBJECT (self, "Deinit module");

  /* Delete graph */
  status = tiovx_sde_module_delete (sde);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    goto out;
  }

  status = tiovx_sde_module_deinit (sde);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    goto out;
  }

  ret = TRUE;
out:
  return ret;
}

static GstCaps *
gst_tiovx_sde_fixate_caps (GstTIOVXMiso * miso,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels)
{
  GstTIOVXSde *self = NULL;
  GstCaps *output_caps = NULL;
  GList *l = NULL;
  gint i = 0;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (sink_caps_list, FALSE);
  g_return_val_if_fail (src_caps, FALSE);
  g_return_val_if_fail (num_channels, FALSE);

  self = GST_TIOVX_SDE (miso);
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

    gst_caps_unref (sink_caps);

    for (i = 0; i < gst_caps_get_size (sink_caps_copy); i++) {
      GstStructure *structure = gst_caps_get_structure (sink_caps_copy, i);

      /* Remove everything except for number of channels */
      gst_structure_remove_fields (structure, "format", NULL);

      /* Set the name to application/x-sde-tiovx in order to intersect */
      gst_structure_set_name (structure, "application/x-sde-tiovx");
    }

    output_caps_tmp = gst_caps_intersect (output_caps, sink_caps_copy);
    gst_caps_unref (output_caps);
    gst_caps_unref (sink_caps_copy);

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

  type = gst_tiovx_sde_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static void
gst_tiovx_sde_finalize (GObject * object)
{
  GstTIOVXSde *self = GST_TIOVX_SDE (object);

  GST_LOG_OBJECT (self, "SDE finalize");

  G_OBJECT_CLASS (gst_tiovx_sde_parent_class)->finalize (object);
}
