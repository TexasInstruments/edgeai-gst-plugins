/*
 * Copyright (c) [2021-2022] Texas Instruments Incorporated
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

#include <fcntl.h>
#include <linux/videodev2.h>
#include <math.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "gsttiovxisp.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxallocator.h"
#include "gst-libs/gst/tiovx/gsttiovxmiso.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxqueueableobject.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "gst-libs/gst/tiovx/gsttiovxsensormeta.h"

#include "tiovx_viss_module.h"
#include "ti_2a_wrapper.h"

static const char default_tiovx_sensor_name[] = "SENSOR_SONY_IMX219_RPI";
#define GST_TYPE_TIOVX_ISP_TARGET (gst_tiovx_isp_target_get_type())
#define DEFAULT_TIOVX_ISP_TARGET TIVX_TARGET_VPAC_VISS1_ID
#define MODULE_MAX_NUM_USER_DATA_PLANES 8

static const gint min_num_exposures = 1;
static const gint default_num_exposures = 1;
static const gint max_num_exposures = 4;

static const gint min_format_msb = 1;
static const gint default_format_msb = 7;
static const gint max_format_msb = 16;

static const gboolean default_lines_interleaved = FALSE;

static const int input_param_id = 3;
static const int output2_param_id = 6;
#if defined(SOC_AM62A)
static const int output0_param_id = 4;
#endif
static const int ae_awb_result_param_id = 1;
static const int h3a_stats_param_id = 9;

static const int default_ae_mode = ALGORITHMS_ISS_AE_AUTO;
static const int default_awb_mode = ALGORITHMS_ISS_AWB_AUTO;
static const guint default_ae_num_skip_frames = 0;
static const guint default_awb_num_skip_frames = 0;
static const guint default_sensor_img_format = 0;       /* BAYER = 0x0, Rest unsupported */

static const guint exposure_ctrl_id = V4L2_CID_EXPOSURE;
static const guint analog_gain_ctrl_id = V4L2_CID_ANALOGUE_GAIN;

static const guint postprocess_skip_frames = 1;

#define ISS_IMX390_GAIN_TBL_SIZE                (71U)

static const uint16_t gIMX390GainsTable[ISS_IMX390_GAIN_TBL_SIZE][2U] = {
  {1024, 0x20},
  {1121, 0x23},
  {1223, 0x26},
  {1325, 0x28},
  {1427, 0x2A},
  {1529, 0x2C},
  {1631, 0x2E},
  {1733, 0x30},
  {1835, 0x32},
  {1937, 0x33},
  {2039, 0x35},
  {2141, 0x36},
  {2243, 0x37},
  {2345, 0x39},
  {2447, 0x3A},
  {2549, 0x3B},
  {2651, 0x3C},
  {2753, 0x3D},
  {2855, 0x3E},
  {2957, 0x3F},
  {3059, 0x40},
  {3160, 0x41},
  {3262, 0x42},
  {3364, 0x43},
  {3466, 0x44},
  {3568, 0x45},
  {3670, 0x46},
  {3772, 0x46},
  {3874, 0x47},
  {3976, 0x48},
  {4078, 0x49},
  {4180, 0x49},
  {4282, 0x4A},
  {4384, 0x4B},
  {4486, 0x4B},
  {4588, 0x4C},
  {4690, 0x4D},
  {4792, 0x4D},
  {4894, 0x4E},
  {4996, 0x4F},
  {5098, 0x4F},
  {5200, 0x50},
  {5301, 0x50},
  {5403, 0x51},
  {5505, 0x51},
  {5607, 0x52},
  {5709, 0x52},
  {5811, 0x53},
  {5913, 0x53},
  {6015, 0x54},
  {6117, 0x54},
  {6219, 0x55},
  {6321, 0x55},
  {6423, 0x56},
  {6525, 0x56},
  {6627, 0x57},
  {6729, 0x57},
  {6831, 0x58},
  {6933, 0x58},
  {7035, 0x58},
  {7137, 0x59},
  {7239, 0x59},
  {7341, 0x5A},
  {7442, 0x5A},
  {7544, 0x5A},
  {7646, 0x5B},
  {7748, 0x5B},
  {7850, 0x5C},
  {7952, 0x5C},
  {8054, 0x5C},
  {8192, 0x5D}
};

/* TIOVX ISP Pad */

#define GST_TYPE_TIOVX_ISP_PAD (gst_tiovx_isp_pad_get_type())
G_DECLARE_FINAL_TYPE (GstTIOVXIspPad, gst_tiovx_isp_pad,
    GST_TIOVX, ISP_PAD, GstTIOVXMisoPad);

struct _GstTIOVXIspPadClass
{
  GstTIOVXMisoPadClass parent_class;
};

enum
{
  PROP_DEVICE = 1,
  PROP_DCC_2A_CONFIG_FILE,
  PROP_AE_MODE,
  PROP_AWB_MODE,
  PROP_AE_NUM_SKIP_FRAMES,
  PROP_AWB_NUM_SKIP_FRAMES,
};

static GType
gst_tiovx_isp_awb_mode_get_type (void)
{
  static GType awb_mode_type = 0;

  static const GEnumValue awb_modes[] = {
    {ALGORITHMS_ISS_AWB_AUTO, "AWB mode auto", "AWB_MODE_AUTO"},
    {ALGORITHMS_ISS_AWB_MANUAL, "AWB mode manual", "AWB_MODE_MANUAL"},
    {ALGORITHMS_ISS_AWB_DISABLED, "AWB mode disabled", "AWB_MODE_DISABLED"},
    {0, NULL, NULL},
  };

  if (!awb_mode_type) {
    awb_mode_type = g_enum_register_static ("GstTIOVXISPAWBModes", awb_modes);
  }
  return awb_mode_type;
}

static GType
gst_tiovx_isp_ae_mode_get_type (void)
{
  static GType ae_mode_type = 0;

  static const GEnumValue targets[] = {
    {ALGORITHMS_ISS_AE_AUTO, "AE mode auto", "AE_MODE_AUTO"},
    {ALGORITHMS_ISS_AE_MANUAL, "AE mode manual", "AE_MODE_MANUAL"},
    {ALGORITHMS_ISS_AE_DISABLED, "AE mode disabled", "AE_MODE_DISABLED"},
    {0, NULL, NULL},
  };

  if (!ae_mode_type) {
    ae_mode_type = g_enum_register_static ("GstTIOVXISPAEModes", targets);
  }
  return ae_mode_type;
}


struct _GstTIOVXIspPad
{
  GstTIOVXMisoPad base;

  gchar *videodev;

  sensor_config_get sensor_in_data;
  sensor_config_set sensor_out_data;

  TI_2A_wrapper ti_2a_wrapper;

  /* TI_2A_wrapper settings */
  gchar *dcc_2a_config_file;
  gboolean ae_mode;
  gboolean awb_mode;
  guint ae_num_skip_frames;
  guint awb_num_skip_frames;

  tivx_aewb_config_t aewb_config;
  uint8_t *dcc_2a_buf;
  uint32_t dcc_2a_buf_size;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_isp_pad_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXIspPad, gst_tiovx_isp_pad,
    GST_TYPE_TIOVX_MISO_PAD,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_isp_pad_debug_category,
        "tiovxisppad", 0, "debug category for TIOVX ISP pad class"));

static void
gst_tiovx_isp_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_tiovx_isp_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_tiovx_isp_pad_finalize (GObject * obj);

static void
gst_tiovx_isp_pad_class_init (GstTIOVXIspPadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_pad_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_pad_get_property);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_isp_pad_finalize);

  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "Device location, e.g, /dev/v4l-subdev1."
          "Required by the user to use the sensor IOCTL support",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_DCC_2A_CONFIG_FILE,
      g_param_spec_string ("dcc-2a-file", "DCC AE/AWB File",
          "TIOVX DCC tuning binary file for the given image sensor.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AE_MODE,
      g_param_spec_enum ("ae-mode", "Auto exposure mode",
          "Flag to set if the auto exposure algorithm mode.",
          gst_tiovx_isp_ae_mode_get_type (),
          default_ae_mode,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AWB_MODE,
      g_param_spec_enum ("awb-mode", "Auto white balance mode",
          "Flag to set if the auto white balance algorithm mode.",
          gst_tiovx_isp_awb_mode_get_type (),
          default_awb_mode,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AE_NUM_SKIP_FRAMES,
      g_param_spec_uint ("ae-num-skip-frames", "AE number of skipped frames",
          "To indicate the AE algorithm how often to process frames, 0 means every frame.",
          0, G_MAXUINT,
          default_ae_num_skip_frames,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AWB_NUM_SKIP_FRAMES,
      g_param_spec_uint ("awb-num-skip-frames", "AWB number of skipped frames",
          "To indicate the AWB algorithm how often to process frames, 0 means every frame.",
          0, G_MAXUINT,
          default_awb_num_skip_frames,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
}

static void
gst_tiovx_isp_pad_init (GstTIOVXIspPad * self)
{
  self->videodev = NULL;

  memset (&self->ti_2a_wrapper, 0, sizeof (self->ti_2a_wrapper));

  self->dcc_2a_config_file = NULL;
  self->ae_mode = default_ae_mode;
  self->awb_mode = default_awb_mode;
  self->ae_num_skip_frames = default_ae_num_skip_frames;
  self->awb_num_skip_frames = default_awb_num_skip_frames;

  self->dcc_2a_buf = NULL;
  self->dcc_2a_buf_size = 0;
}

static void
gst_tiovx_isp_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXIspPad *self = GST_TIOVX_ISP_PAD (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DEVICE:
      g_free (self->videodev);
      self->videodev = g_value_dup_string (value);
      break;
    case PROP_DCC_2A_CONFIG_FILE:
      g_free (self->dcc_2a_config_file);
      self->dcc_2a_config_file = g_value_dup_string (value);
      break;
    case PROP_AE_MODE:
      self->ae_mode = g_value_get_enum (value);
      break;
    case PROP_AWB_MODE:
      self->awb_mode = g_value_get_enum (value);
      break;
    case PROP_AE_NUM_SKIP_FRAMES:
      self->ae_num_skip_frames = g_value_get_uint (value);
      break;
    case PROP_AWB_NUM_SKIP_FRAMES:
      self->awb_num_skip_frames = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_isp_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXIspPad *self = GST_TIOVX_ISP_PAD (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, self->videodev);
      break;
    case PROP_DCC_2A_CONFIG_FILE:
      g_value_set_string (value, self->dcc_2a_config_file);
      break;
    case PROP_AE_MODE:
      g_value_set_enum (value, self->ae_mode);
      break;
    case PROP_AWB_MODE:
      g_value_set_enum (value, self->awb_mode);
      break;
    case PROP_AE_NUM_SKIP_FRAMES:
      g_value_set_uint (value, self->ae_num_skip_frames);
      break;
    case PROP_AWB_NUM_SKIP_FRAMES:
      g_value_set_uint (value, self->awb_num_skip_frames);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_isp_pad_finalize (GObject * obj)
{
  GstTIOVXIspPad *self = GST_TIOVX_ISP_PAD (obj);

  g_free (self->videodev);
  self->videodev = NULL;

  g_free (self->dcc_2a_config_file);
  self->dcc_2a_config_file = NULL;


  G_OBJECT_CLASS (gst_tiovx_isp_pad_parent_class)->finalize (obj);
}

enum
{
  TI_2A_WRAPPER_SENSOR_IMG_PHASE_BGGR = 0,
  TI_2A_WRAPPER_SENSOR_IMG_PHASE_GBRG = 1,
  TI_2A_WRAPPER_SENSOR_IMG_PHASE_GRBG = 2,
  TI_2A_WRAPPER_SENSOR_IMG_PHASE_RGGB = 3,
};

/* Properties definition */
enum
{
  PROP_0,
  PROP_DCC_ISP_CONFIG_FILE,
  PROP_SENSOR_NAME,
  PROP_TARGET,
  PROP_NUM_EXPOSURES,
  PROP_LINE_INTERLEAVED,
  PROP_FORMAT_MSB,
};

/* Target definition */
enum
{
  TIVX_TARGET_VPAC_VISS1_ID = 0,
#if defined(SOC_J784S4)
  TIVX_TARGET_VPAC2_VISS1_ID,
#endif
};

static GType
gst_tiovx_isp_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_VISS1_ID, "VPAC VISS1", TIVX_TARGET_VPAC_VISS1},
#if defined(SOC_J784S4)
    {TIVX_TARGET_VPAC2_VISS1_ID, "VPAC2 VISS1", TIVX_TARGET_VPAC2_VISS1},
#endif
    {0, NULL, NULL},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXISPTarget", targets);
  }
  return target_type;
}

/* Formats definition */
#if defined(SOC_AM62A)
#define TIOVX_ISP_SUPPORTED_FORMATS_SRC "{NV12, GRAY8}"
#else
#define TIOVX_ISP_SUPPORTED_FORMATS_SRC "{NV12}"
#endif
#define TIOVX_ISP_SUPPORTED_FORMATS_SINK "{ bggr, gbrg, grbg, rggb, bggr10, gbrg10, grbg10, rggb10, rggi10, grig10, bggi10, gbig10, girg10, iggr10, gibg10, iggb10, bggr12, gbrg12, grbg12, rggb12, bggr16, gbrg16, grbg16, rggb16 }"
#define TIOVX_ISP_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_ISP_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_ISP_SUPPORTED_CHANNELS "[2 , 16]"

/* Src caps */
#define TIOVX_ISP_STATIC_CAPS_SRC                           \
  "video/x-raw, "                                           \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT                    \
  "; "                                                      \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "      \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT ", "               \
  "num-channels = " TIOVX_ISP_SUPPORTED_CHANNELS

/* Sink caps */
#define TIOVX_ISP_STATIC_CAPS_SINK                           \
  "video/x-bayer, "                                          \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                  \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_ISP_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_ISP_STATIC_CAPS_SRC)
    );

struct _GstTIOVXISP
{
  GstTIOVXMiso element;
  gchar *dcc_isp_config_file;
  gchar *sensor_name;
  gint target_id;
  SensorObj sensor_obj;

  gint num_exposures;
  gboolean line_interleaved;
  gint format_msb;
  gint meta_height_before;
  gint meta_height_after;
  gint meta_before_size;
  gint meta_after_size;

  GstTIOVXAllocator *user_data_allocator;

  GstMemory *aewb_memory;
  GstMemory *h3a_stats_memory;

  TIOVXVISSModuleObj viss_obj;

  /*
   * Since the ISP has an asymmetrical input and output, there is only a single array on the
   * module's side for all input pads. Therefore the array can't be passed as an argument for the
   * miso_pad_set_params, this array holds references to members for access in the MISO parent class.
   */
  vx_reference input_references[MAX_NUM_CHANNELS];

  gint num_channels;
  guint postprocess_iter;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_isp_debug);
#define GST_CAT_DEFAULT gst_tiovx_isp_debug

#define gst_tiovx_isp_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXISP, gst_tiovx_isp,
    GST_TYPE_TIOVX_MISO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_isp_debug,
        "tiovxisp", 0, "debug category for the tiovxisp element"));

static void gst_tiovx_isp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void gst_tiovx_isp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_tiovx_isp_finalize (GObject * obj);

static gboolean gst_tiovx_isp_init_module (GstTIOVXMiso * agg,
    vx_context context, GList * sink_pads_list, GstPad * src_pad,
    guint num_channels);

static gboolean gst_tiovx_isp_configure_module (GstTIOVXMiso * miso);

static gboolean gst_tiovx_isp_release_buffer (GstTIOVXMiso * agg);

static gboolean gst_tiovx_isp_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects);

static gboolean gst_tiovx_isp_create_graph (GstTIOVXMiso * miso,
    vx_context context, vx_graph graph);

static GstCaps *gst_tiovx_isp_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels);

static gboolean gst_tiovx_isp_deinit_module (GstTIOVXMiso * miso);

static gboolean gst_tiovx_isp_postprocess (GstTIOVXMiso * self);

static gboolean gst_tiovx_isp_add_sensor_meta (GstTIOVXMiso * self,
    GstBuffer * buf);

static gboolean gst_tiovx_isp_allocate_user_data_objects (GstTIOVXISP * src);

static const gchar *target_id_to_target_name (gint target_id);

static int32_t get_imx219_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms);

static int32_t get_imx390_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms);

static int32_t get_ov2312_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms);

static void gst_tiovx_isp_map_2A_values (GstTIOVXISP * self, int exposure_time,
    int analog_gain, gint32 * exposure_time_mapped,
    gint32 * analog_gain_mapped);

/* Initialize the plugin's class */
static void
gst_tiovx_isp_class_init (GstTIOVXISPClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstTIOVXMisoClass *gsttiovxmiso_class = GST_TIOVX_MISO_CLASS (klass);
  GstPadTemplate *src_temp = NULL;
  GstPadTemplate *sink_temp = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX ISP",
      "Filter",
      "Image Signal Processing using the TIOVX Modules API",
      "RidgeRun <support@ridgerun.com>");

  src_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_TIOVX_MISO_PAD);
  gst_element_class_add_pad_template (gstelement_class, src_temp);

  sink_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TYPE_TIOVX_ISP_PAD);
  gst_element_class_add_pad_template (gstelement_class, sink_temp);

  gobject_class->set_property = gst_tiovx_isp_set_property;
  gobject_class->get_property = gst_tiovx_isp_get_property;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_isp_finalize);

  g_object_class_install_property (gobject_class, PROP_DCC_ISP_CONFIG_FILE,
      g_param_spec_string ("dcc-isp-file", "DCC ISP File",
          "TIOVX DCC tuning binary file for the given image sensor.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SENSOR_NAME,
      g_param_spec_string ("sensor-name", "Sensor name",
          "TIOVX camera sensor string ID. Below are the supported sensors\n"
          "                                   SENSOR_SONY_IMX390_UB953_D3\n"
          "                                   SENSOR_ONSEMI_AR0820_UB953_LI\n"
          "                                   SENSOR_ONSEMI_AR0233_UB953_MARS\n"
          "                                   SENSOR_SONY_IMX219_RPI\n"
          "                                   SENSOR_OV2312_UB953_LI",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element.",
          GST_TYPE_TIOVX_ISP_TARGET,
          DEFAULT_TIOVX_ISP_TARGET,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_NUM_EXPOSURES,
      g_param_spec_int ("num-exposures", "Number of exposures",
          "Number of exposures for the incoming raw image.",
          min_num_exposures, max_num_exposures, default_num_exposures,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_LINE_INTERLEAVED,
      g_param_spec_boolean ("lines-interleaved", "Interleaved lines",
          "Flag to indicate if lines are interleaved.",
          default_lines_interleaved,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_FORMAT_MSB,
      g_param_spec_int ("format-msb", "Format MSB",
          "Flag indicating which is the most significant bit that still has data.",
          min_format_msb, max_format_msb, default_format_msb,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  gsttiovxmiso_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_init_module);

  gsttiovxmiso_class->configure_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_configure_module);

  gsttiovxmiso_class->release_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_release_buffer);

  gsttiovxmiso_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_get_node_info);

  gsttiovxmiso_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_create_graph);

  gsttiovxmiso_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_fixate_caps);

  gsttiovxmiso_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_deinit_module);

  gsttiovxmiso_class->postprocess =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_postprocess);

  gsttiovxmiso_class->add_outbuf_meta =
      GST_DEBUG_FUNCPTR (gst_tiovx_isp_add_sensor_meta);
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_tiovx_isp_init (GstTIOVXISP * self)
{
  gint i = 0;

  self->dcc_isp_config_file = NULL;
  self->sensor_name = g_strdup (default_tiovx_sensor_name);

  self->num_exposures = default_num_exposures;
  self->line_interleaved = default_lines_interleaved;
  self->format_msb = default_format_msb;
  self->meta_height_before = 0;
  self->meta_height_after = 0;
  self->meta_before_size = 0;
  self->meta_after_size = 0;

  self->aewb_memory = NULL;
  self->h3a_stats_memory = NULL;

  self->user_data_allocator = g_object_new (GST_TYPE_TIOVX_ALLOCATOR, NULL);

  self->num_channels = 0;
  self->postprocess_iter = 0;

  for (i = 0; i < MAX_NUM_CHANNELS; i++) {
    self->input_references[i] = NULL;
  }
}

static void
gst_tiovx_isp_finalize (GObject * obj)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (obj);
  gint i = 0;

  GST_LOG_OBJECT (self, "finalize");

  /* Free internal strings */
  g_free (self->dcc_isp_config_file);
  self->dcc_isp_config_file = NULL;
  g_free (self->sensor_name);
  self->sensor_name = NULL;

  if (NULL != self->aewb_memory) {
    gst_memory_unref (self->aewb_memory);
  }
  if (NULL != self->h3a_stats_memory) {
    gst_memory_unref (self->h3a_stats_memory);
  }
  if (self->user_data_allocator) {
    g_object_unref (self->user_data_allocator);
  }

  for (i = 0; i < MAX_NUM_CHANNELS; i++) {
    if (self->input_references[i]) {
      vxReleaseReference (&self->input_references[i]);
      self->input_references[i] = NULL;
    }
  }

  G_OBJECT_CLASS (gst_tiovx_isp_parent_class)->finalize (obj);
}

static void
gst_tiovx_isp_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_ISP_CONFIG_FILE:
      g_free (self->dcc_isp_config_file);
      self->dcc_isp_config_file = g_value_dup_string (value);
      break;
    case PROP_SENSOR_NAME:
      g_free (self->sensor_name);
      self->sensor_name = g_value_dup_string (value);
      break;
    case PROP_TARGET:
      self->target_id = g_value_get_enum (value);
      break;
    case PROP_NUM_EXPOSURES:
      self->num_exposures = g_value_get_int (value);
      break;
    case PROP_LINE_INTERLEAVED:
      self->line_interleaved = g_value_get_boolean (value);
      break;
    case PROP_FORMAT_MSB:
      self->format_msb = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_isp_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_ISP_CONFIG_FILE:
      g_value_set_string (value, self->dcc_isp_config_file);
      break;
    case PROP_SENSOR_NAME:
      g_value_set_string (value, self->sensor_name);
      break;
    case PROP_TARGET:
      g_value_set_enum (value, self->target_id);
      break;
    case PROP_NUM_EXPOSURES:
      g_value_set_int (value, self->num_exposures);
      break;
    case PROP_LINE_INTERLEAVED:
      g_value_set_boolean (value, self->line_interleaved);
      break;
    case PROP_FORMAT_MSB:
      g_value_set_int (value, self->format_msb);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_tiovx_isp_read_2a_config_file (GstTIOVXIspPad * self)
{
  FILE *dcc_2a_file = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);

  dcc_2a_file = fopen (self->dcc_2a_config_file, "rb");

  if (NULL == dcc_2a_file) {
    GST_ERROR_OBJECT (self, "Unable to open 2A config file: %s",
        self->dcc_2a_config_file);
    goto out;
  }

  fseek (dcc_2a_file, 0, SEEK_END);
  self->dcc_2a_buf_size = ftell (dcc_2a_file);
  fseek (dcc_2a_file, 0, SEEK_SET);     /* same as rewind(f); */

  if (0 == self->dcc_2a_buf_size) {
    GST_ERROR_OBJECT (self, "File: %s has size of 0", self->dcc_2a_config_file);
    fclose (dcc_2a_file);
    goto out;
  }

  self->dcc_2a_buf =
      (uint8_t *) tivxMemAlloc (self->dcc_2a_buf_size, TIVX_MEM_EXTERNAL);
  fread (self->dcc_2a_buf, 1, self->dcc_2a_buf_size, dcc_2a_file);
  fclose (dcc_2a_file);

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_isp_init_module (GstTIOVXMiso * miso,
    vx_context context, GList * sink_pads_list, GstPad * src_pad,
    guint num_channels)
{
  GstTIOVXISP *self = NULL;
  GstVideoInfo in_info = { };
  GstVideoInfo out_info = { };
  gboolean ret = FALSE;
  guint32 ti_2a_wrapper_ret = 0;
  vx_status status = VX_FAILURE;
  GstCaps *sink_caps = NULL;
  GstCaps *src_caps = NULL;
  GstStructure *sink_caps_st = NULL;
  GList *l = NULL;
  const gchar *format_str = NULL;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_ISP (miso);

  tiovx_querry_sensor (&self->sensor_obj);
  tiovx_init_sensor (&self->sensor_obj, self->sensor_name);
  self->sensor_obj.num_cameras_enabled = num_channels;

  if (NULL == self->dcc_isp_config_file) {
    GST_ERROR_OBJECT (self, "DCC ISP config file not specified");
    goto out;
  }

  snprintf (self->viss_obj.dcc_config_file_path, TIVX_FILEIO_FILE_PATH_LENGTH,
      "%s", self->dcc_isp_config_file);

  sink_caps = gst_pad_get_current_caps (GST_PAD (sink_pads_list->data));
  if (NULL == sink_caps) {
    sink_caps = gst_pad_peer_query_caps (GST_PAD (sink_pads_list->data), NULL);
  }

  sink_caps_st = gst_caps_get_structure (sink_caps, 0);
  /* Initialize the input parameters */
  if (!gst_video_info_from_caps (&in_info, sink_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from input pad: %"
        GST_PTR_FORMAT, GST_PAD (sink_pads_list->data));
    goto out;
  }

  gst_structure_get_int (sink_caps_st, "meta-height-before",
      &self->meta_height_before);
  gst_structure_get_int (sink_caps_st, "meta-height-after",
      &self->meta_height_after);
  self->num_channels = num_channels;

  self->viss_obj.input.bufq_depth = 1;
  /* TODO: currently the user has the responsability of setting this parameters
   * through properties. This should ideally be obtained through a sensor query or
   * through the caps
   */
  self->viss_obj.input.params.num_exposures = self->num_exposures;
  self->viss_obj.input.params.line_interleaved = self->line_interleaved;
  self->viss_obj.input.params.format[0].msb = self->format_msb;
  self->viss_obj.input.params.meta_height_before = self->meta_height_before;
  self->viss_obj.input.params.meta_height_after = self->meta_height_after;
  self->viss_obj.input.params.width = GST_VIDEO_INFO_WIDTH (&in_info);
  self->viss_obj.input.params.height = GST_VIDEO_INFO_HEIGHT (&in_info)
                                       - self->meta_height_before
                                       - self->meta_height_after;

  format_str = gst_structure_get_string (sink_caps_st, "format");

  if ((g_strcmp0 (format_str, "bggr16") == 0)
      || (g_strcmp0 (format_str, "gbrg16") == 0)
      || (g_strcmp0 (format_str, "grbg16") == 0)
      || (g_strcmp0 (format_str, "rggb16") == 0)
      || (g_strcmp0 (format_str, "bggr10") == 0)
      || (g_strcmp0 (format_str, "gbrg10") == 0)
      || (g_strcmp0 (format_str, "grbg10") == 0)
      || (g_strcmp0 (format_str, "rggb10") == 0)
      || (g_strcmp0 (format_str, "rggi10") == 0)
      || (g_strcmp0 (format_str, "grig10") == 0)
      || (g_strcmp0 (format_str, "bggi10") == 0)
      || (g_strcmp0 (format_str, "gbig10") == 0)
      || (g_strcmp0 (format_str, "girg10") == 0)
      || (g_strcmp0 (format_str, "iggr10") == 0)
      || (g_strcmp0 (format_str, "gibg10") == 0)
      || (g_strcmp0 (format_str, "iggb10") == 0)
      || (g_strcmp0 (format_str, "bggr12") == 0)
      || (g_strcmp0 (format_str, "gbrg12") == 0)
      || (g_strcmp0 (format_str, "grbg12") == 0)
      || (g_strcmp0 (format_str, "rggb12") == 0)
      ) {
    self->viss_obj.input.params.format[0].pixel_container =
        TIVX_RAW_IMAGE_16_BIT;
  } else if ((g_strcmp0 (format_str, "bggr") == 0)
      || (g_strcmp0 (format_str, "gbrg") == 0)
      || (g_strcmp0 (format_str, "grbg") == 0)
      || (g_strcmp0 (format_str, "rggb") == 0)
      ) {
    self->viss_obj.input.params.format[0].pixel_container =
        TIVX_RAW_IMAGE_8_BIT;
  } else {
    GST_ERROR_OBJECT (self, "Couldn't determine pixel container form caps");
    goto out;
  }

  self->meta_before_size = self->viss_obj.input.params.width *
      self->viss_obj.input.params.meta_height_before;
  self->meta_after_size = self->viss_obj.input.params.width *
      self->viss_obj.input.params.meta_height_after;

  if (self->viss_obj.input.params.format[0].pixel_container
      == TIVX_RAW_IMAGE_16_BIT) {
    self->meta_before_size *= 2;
    self->meta_after_size *= 2;
  }

  self->viss_obj.ae_awb_result_bufq_depth = 1;

  GST_INFO_OBJECT (self,
      "Input parameters:\n"
      "\tWidth: %d\n"
      "\tHeight: %d\n"
      "\tNum exposures: %d\n"
      "\tLines interleaved: %d\n"
      "\tFormat pixel container: 0x%x\n"
      "\tFormat MSB: %d\n"
      "\tMeta height before: %d\n"
      "\tMeta height after: %d",
      self->viss_obj.input.params.width,
      self->viss_obj.input.params.height,
      self->viss_obj.input.params.num_exposures,
      self->viss_obj.input.params.line_interleaved,
      self->viss_obj.input.params.format[0].pixel_container,
      self->viss_obj.input.params.format[0].msb,
      self->viss_obj.input.params.meta_height_before,
      self->viss_obj.input.params.meta_height_after);



  /* Initialize tiovx params */
  tivx_vpac_viss_params_init (&self->viss_obj.params);

  src_caps = gst_pad_get_current_caps (GST_PAD (src_pad));
  if (!gst_video_info_from_caps (&out_info, src_caps)) {
    GST_ERROR_OBJECT (self, "Failed to get info from output caps: %"
        GST_PTR_FORMAT, src_caps);
    goto out;
  }
#if defined(SOC_AM62A)
  if (NULL == g_strrstr (format_str, "i"))
    self->viss_obj.params.bypass_pcid = 1;
  else
    self->viss_obj.params.bypass_pcid = 0;

  if (out_info.finfo->format == GST_VIDEO_FORMAT_NV12) {
    self->viss_obj.params.enable_ir_op = TIVX_VPAC_VISS_IR_DISABLE;
    self->viss_obj.params.enable_bayer_op = TIVX_VPAC_VISS_BAYER_ENABLE;
  } else if (out_info.finfo->format == GST_VIDEO_FORMAT_GRAY8) {
    self->viss_obj.params.enable_ir_op = TIVX_VPAC_VISS_IR_ENABLE;
    self->viss_obj.params.enable_bayer_op = TIVX_VPAC_VISS_BAYER_DISABLE;
  } else {
    GST_ERROR_OBJECT (self, "Unsupported Src format %s",
        gst_video_format_to_string (out_info.finfo->format));
    goto out;
  }

  if (self->viss_obj.params.enable_ir_op) {
    self->viss_obj.output_select[0] = TIOVX_VISS_MODULE_OUTPUT_EN;
    self->viss_obj.output_select[1] = TIOVX_VISS_MODULE_OUTPUT_NA;
    self->viss_obj.output_select[2] = TIOVX_VISS_MODULE_OUTPUT_NA;
    self->viss_obj.output_select[3] = TIOVX_VISS_MODULE_OUTPUT_NA;
    self->viss_obj.output_select[4] = TIOVX_VISS_MODULE_OUTPUT_NA;

    self->viss_obj.output0.bufq_depth = 1;
    self->viss_obj.output0.color_format =
        gst_format_to_vx_format (out_info.finfo->format);
    self->viss_obj.output0.width = GST_VIDEO_INFO_WIDTH (&out_info);
    self->viss_obj.output0.height = GST_VIDEO_INFO_HEIGHT (&out_info);

    GST_INFO_OBJECT (self,
        "Output parameters:\n"
        "\tWidth: %d\n"
        "\tHeight: %d\n",
        self->viss_obj.output0.width, self->viss_obj.output0.height);
  }

  if (self->viss_obj.params.enable_bayer_op)
#endif
  {
    /* Initialize the output parameters.
     * TODO: Only output for 12 or 8 bit is enabled, so only output2
     * parameters are specified.
     */
    self->viss_obj.output_select[0] = TIOVX_VISS_MODULE_OUTPUT_NA;
    self->viss_obj.output_select[1] = TIOVX_VISS_MODULE_OUTPUT_NA;
    self->viss_obj.output_select[2] = TIOVX_VISS_MODULE_OUTPUT_EN;
    self->viss_obj.output_select[3] = TIOVX_VISS_MODULE_OUTPUT_NA;
    self->viss_obj.output_select[4] = TIOVX_VISS_MODULE_OUTPUT_NA;

    self->viss_obj.output2.bufq_depth = 1;
    self->viss_obj.output2.color_format =
        gst_format_to_vx_format (out_info.finfo->format);
    self->viss_obj.output2.width = GST_VIDEO_INFO_WIDTH (&out_info);
    self->viss_obj.output2.height = GST_VIDEO_INFO_HEIGHT (&out_info);

    GST_INFO_OBJECT (self,
        "Output parameters:\n"
        "\tWidth: %d\n"
        "\tHeight: %d\n",
        self->viss_obj.output2.width, self->viss_obj.output2.height);
  }

  self->viss_obj.h3a_stats_bufq_depth = 1;

  GST_INFO_OBJECT (self, "Initializing ISP object");
  status = tiovx_viss_module_init (context, &self->viss_obj, &self->sensor_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
    goto out;
  }

  for (l = sink_pads_list; l != NULL; l = g_list_next (l)) {
    GstTIOVXIspPad *sink_pad = (GstTIOVXIspPad *) l->data;

    /* TI_2A_wrapper configuration */
    ret = gst_tiovx_isp_read_2a_config_file (sink_pad);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Unable to read 2a config file");
      goto out;
    }

    if (NULL == sink_pad->dcc_2a_config_file) {
      GST_ERROR_OBJECT (self, "DCC AE/AWB config file not specified");
      goto out;
    }

    if (NULL != g_strrstr (format_str, "bggr")) {
      sink_pad->aewb_config.sensor_img_phase =
          TI_2A_WRAPPER_SENSOR_IMG_PHASE_BGGR;
    } else if (NULL != g_strrstr (format_str, "gbrg")) {
      sink_pad->aewb_config.sensor_img_phase =
          TI_2A_WRAPPER_SENSOR_IMG_PHASE_GBRG;
    } else if (NULL != g_strrstr (format_str, "grbg")) {
      sink_pad->aewb_config.sensor_img_phase =
          TI_2A_WRAPPER_SENSOR_IMG_PHASE_GRBG;
    } else if (NULL != g_strrstr (format_str, "rggb")) {
      sink_pad->aewb_config.sensor_img_phase =
          TI_2A_WRAPPER_SENSOR_IMG_PHASE_RGGB;
    } else if (NULL != g_strrstr (format_str, "i")) {
      //RGGB is used for all IR formats
      sink_pad->aewb_config.sensor_img_phase =
          TI_2A_WRAPPER_SENSOR_IMG_PHASE_RGGB;
    } else {
      GST_ERROR_OBJECT (self, "Couldn't determine sensor img phase from caps");
      ret = FALSE;
      goto out;
    }

    sink_pad->aewb_config.sensor_dcc_id = self->sensor_obj.sensorParams.dccId;
    sink_pad->aewb_config.sensor_img_format = default_sensor_img_format;
    sink_pad->aewb_config.awb_mode = sink_pad->awb_mode;
    sink_pad->aewb_config.ae_mode = sink_pad->ae_mode;
    sink_pad->aewb_config.awb_num_skip_frames = sink_pad->awb_num_skip_frames;
    sink_pad->aewb_config.ae_num_skip_frames = sink_pad->ae_num_skip_frames;
    sink_pad->aewb_config.channel_id = 0;

    ti_2a_wrapper_ret =
        TI_2A_wrapper_create (&sink_pad->ti_2a_wrapper, &sink_pad->aewb_config,
        sink_pad->dcc_2a_buf, sink_pad->dcc_2a_buf_size);
    if (ti_2a_wrapper_ret) {
      GST_ERROR_OBJECT (self, "Unable to create TI 2A wrapper: %d",
          ti_2a_wrapper_ret);
      ret = FALSE;
      goto out;
    }

    GST_INFO_OBJECT (sink_pad,
        "TI 2A parameters:\n"
        "\tSensor DCC ID: %d\n"
        "\tSensor Image Format: %d\n"
        "\tSensor Image Phase: %d\n"
        "\tSensor AWB Mode: %d\n"
        "\tSensor AE Mode: %d\n"
        "\tSensor AWB number of skipped frames: %d\n"
        "\tSensor AE number of skipped frames: %d\n",
        sink_pad->aewb_config.sensor_dcc_id,
        sink_pad->aewb_config.sensor_img_format,
        sink_pad->aewb_config.sensor_img_phase,
        sink_pad->aewb_config.awb_mode,
        sink_pad->aewb_config.ae_mode,
        sink_pad->aewb_config.awb_num_skip_frames,
        sink_pad->aewb_config.ae_num_skip_frames);
  }

  ret = TRUE;

out:

  if (!ret) {
    tiovx_deinit_sensor (&self->sensor_obj);
  }

  return ret;
}

static gboolean
gst_tiovx_isp_release_buffer (GstTIOVXMiso * miso)
{
  GstTIOVXISP *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;

  g_return_val_if_fail (miso, FALSE);

  self = GST_TIOVX_ISP (miso);

  GST_DEBUG_OBJECT (self, "Release buffer ISP");
  status = tiovx_viss_module_release_buffers (&self->viss_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure release buffer failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_isp_configure_module (GstTIOVXMiso * miso)
{
  GstTIOVXISP *self = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (miso, FALSE);

  self = GST_TIOVX_ISP (miso);

  ret = gst_tiovx_isp_allocate_user_data_objects (self);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to allocate user data objects");
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_isp_get_node_info (GstTIOVXMiso * agg,
    GList * sink_pads_list, GstPad * src_pad, vx_node * node,
    GList ** queueable_objects)
{
  GstTIOVXISP *self = NULL;
  GList *l = NULL;
  GstTIOVXQueueable *queueable_object = NULL;
  gint graph_parameter_index = 0;
  int i = 0;

  g_return_val_if_fail (agg, FALSE);
  g_return_val_if_fail (sink_pads_list, FALSE);
  g_return_val_if_fail (src_pad, FALSE);

  self = GST_TIOVX_ISP (agg);

  *node = self->viss_obj.node;

  /* Set input parameters */
  for (l = sink_pads_list, i = 0; l != NULL; l = g_list_next (l), i++) {
    GstTIOVXMisoPad *sink_pad = (GstTIOVXMisoPad *) l->data;

    if (0 == i) {
      gst_tiovx_miso_pad_set_params (sink_pad,
          NULL, (vx_reference *) & self->viss_obj.input.image_handle[i],
          graph_parameter_index, input_param_id);
      graph_parameter_index++;

    } else {
      self->input_references[i] =
          vxGetObjectArrayItem (self->viss_obj.input.arr[0], i);

      gst_tiovx_miso_pad_set_params (sink_pad,
          NULL, (vx_reference *) & self->input_references[i], -1, -1);
    }
  }

#if defined(SOC_AM62A)
  if (self->viss_obj.params.enable_ir_op) {
    gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (src_pad),
        self->viss_obj.output0.arr[0],
        (vx_reference *) & self->viss_obj.output0.image_handle[0],
        graph_parameter_index, output0_param_id);
  }

  if (self->viss_obj.params.enable_bayer_op)
#endif
    /* Set output parameters, currently only output2 is supported */
  {
    gst_tiovx_miso_pad_set_params (GST_TIOVX_MISO_PAD (src_pad),
        self->viss_obj.output2.arr[0],
        (vx_reference *) & self->viss_obj.output2.image_handle[0],
        graph_parameter_index, output2_param_id);
  }
  graph_parameter_index++;

  /* ae_awb results & h3a stats aren't input or outputs, these are added as queueable_objects */
  queueable_object =
      GST_TIOVX_QUEUEABLE (g_object_new (GST_TYPE_TIOVX_QUEUEABLE, NULL));
  gst_tiovx_queueable_set_params (queueable_object,
      NULL, (vx_reference) self->viss_obj.ae_awb_result_handle[0],
      graph_parameter_index, ae_awb_result_param_id);
  graph_parameter_index++;
  *queueable_objects = g_list_append (*queueable_objects, queueable_object);

  queueable_object =
      GST_TIOVX_QUEUEABLE (g_object_new (GST_TYPE_TIOVX_QUEUEABLE, NULL));
  gst_tiovx_queueable_set_params (queueable_object,
      NULL, (vx_reference) self->viss_obj.h3a_stats_handle[0],
      graph_parameter_index, h3a_stats_param_id);
  graph_parameter_index++;
  *queueable_objects = g_list_append (*queueable_objects, queueable_object);

  return TRUE;
}

static gboolean
gst_tiovx_isp_create_graph (GstTIOVXMiso * miso,
    vx_context context, vx_graph graph)
{
  GstTIOVXISP *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  const gchar *target = NULL;

  g_return_val_if_fail (miso, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_ISP (miso);

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  GST_DEBUG_OBJECT (self, "Creating ISP graph");
  status =
      tiovx_viss_module_create (graph, &self->viss_obj, NULL, NULL, target);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Finished creating viss graph");

  ret = TRUE;

out:
  return ret;
}

static GstCaps *
gst_tiovx_isp_fixate_caps (GstTIOVXMiso * self,
    GList * sink_caps_list, GstCaps * src_caps, gint * num_channels)
{
  GList *l = NULL;
  gint width = 0, src_width = 0;
  gint height = 0, src_height = 0;
  gint meta_height_before = -1;
  gint meta_height_after = -1;
  const gchar *format_str = NULL;
  GstCaps *output_caps = NULL, *candidate_output_caps = NULL;
  GstStructure *candidate_output_structure = NULL;
#if defined(SOC_AM62A)
  GValue output_formats = G_VALUE_INIT;
  GValue format = G_VALUE_INIT;
#endif

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (sink_caps_list, NULL);
  g_return_val_if_fail (src_caps, NULL);
  g_return_val_if_fail (num_channels, NULL);

  GST_DEBUG_OBJECT (self, "Fixating caps");

  for (l = sink_caps_list; l != NULL; l = l->next) {
    GstCaps *sink_caps = (GstCaps *) l->data;
    gint this_sink_width = 0, this_sink_height = 0;
    gint this_meta_height_before = 0, this_meta_height_after = 0;
    const gchar *this_format_str = NULL;
    GstStructure *sink_structure = NULL;

    sink_structure = gst_caps_get_structure (sink_caps, 0);

    if (!gst_structure_get_int (sink_structure, "width", &this_sink_width)) {
      GST_ERROR_OBJECT (self, "Width is missing in sink caps");
      return NULL;
    }

    if (!gst_structure_get_int (sink_structure, "height", &this_sink_height)) {
      GST_ERROR_OBJECT (self, "Height is missing in sink caps");
      return NULL;
    }

    gst_structure_get_int (sink_structure, "meta-height-before",
        &this_meta_height_before);
    gst_structure_get_int (sink_structure, "meta-height-after",
        &this_meta_height_after);

    this_format_str = gst_structure_get_string (sink_structure, "format");
    if (NULL == this_format_str) {
      GST_ERROR_OBJECT (self, "Format is missing in sink caps");
      return NULL;
    }

    if ((0 != width) && (this_sink_width != width)) {
      GST_ERROR_OBJECT (self, "Width doesn't match in all sink caps");
      return NULL;
    }
    if ((0 != height) && (this_sink_height != height)) {
      GST_ERROR_OBJECT (self, "Height doesn't match in all sink caps");
      return NULL;
    }

    if ((-1 != meta_height_before) &&
        (this_meta_height_before != meta_height_before)) {
      GST_ERROR_OBJECT (self, "Meta height before doesn't match in all sink caps");
      return NULL;
    }
    if ((-1 != meta_height_after) &&
        (this_meta_height_after != meta_height_after)) {
      GST_ERROR_OBJECT (self, "Meta height after doesn't match in all sink caps");
      return NULL;
    }

    if ((NULL != format_str) && (g_strcmp0 (format_str, this_format_str) != 0)) {
      GST_ERROR_OBJECT (self, "Format doesn't match in all sink caps");
      return NULL;
    }

    format_str = this_format_str;
    width = this_sink_width;
    height = this_sink_height;
    meta_height_before = this_meta_height_before;
    meta_height_after = this_meta_height_after;
  }

  if ((0 == width) || (0 == height)) {
    return NULL;
  }

  height = height - meta_height_before - meta_height_after;

  candidate_output_caps = gst_caps_make_writable (gst_caps_copy (src_caps));
  candidate_output_structure =
      gst_caps_get_structure (candidate_output_caps, 0);

  gst_structure_get_int (candidate_output_structure, "width", &src_width);
  gst_structure_get_int (candidate_output_structure, "height", &src_height);

  if (src_width != width) {
    if (!gst_structure_fixate_field_nearest_int (candidate_output_structure,
          "width", width)) {
        GST_ERROR_OBJECT (self, "Could not Fixate Width to %d", width);
        return NULL;
    }
  }

  if (src_height != height) {
    if (!gst_structure_fixate_field_nearest_int (candidate_output_structure,
          "height", height)) {
        GST_ERROR_OBJECT (self, "Could not Fixate Height to %d", height);
        return NULL;
    }
  }

  if (g_list_length (sink_caps_list) > 1) {
    gst_structure_fixate_field_nearest_int (candidate_output_structure,
        "num-channels", g_list_length (sink_caps_list));
    gst_caps_set_features_simple (candidate_output_caps,
        gst_tiovx_get_batched_memory_feature ());
  }
#if defined(SOC_AM62A)
  if (NULL == g_strrstr (format_str, "i")) {
    g_value_init (&output_formats, GST_TYPE_LIST);
    g_value_init (&format, G_TYPE_STRING);
    g_value_set_string (&format, "NV12");
    gst_value_list_append_value (&output_formats, &format);
    gst_structure_set_value (candidate_output_structure, "format",
        &output_formats);
    g_value_unset (&format);
    g_value_unset (&output_formats);
  }
#endif

  output_caps = gst_caps_intersect (candidate_output_caps, src_caps);
  output_caps = gst_caps_fixate (output_caps);

  GST_DEBUG_OBJECT (self, "Fixated %" GST_PTR_FORMAT " into %" GST_PTR_FORMAT,
      src_caps, output_caps);

  gst_caps_unref (candidate_output_caps);

  return output_caps;
}

static gboolean
gst_tiovx_isp_deinit_module (GstTIOVXMiso * miso)
{
  GstTIOVXISP *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  guint32 ti_2a_wrapper_ret = 0;
  GList *sink_pads_list = NULL;
  GList *l = NULL;

  g_return_val_if_fail (miso, FALSE);

  self = GST_TIOVX_ISP (miso);
  sink_pads_list = GST_ELEMENT (miso)->sinkpads;

  for (l = sink_pads_list; l != NULL; l = g_list_next (l)) {
    GstTIOVXIspPad *sink_pad = (GstTIOVXIspPad *) l->data;

    ti_2a_wrapper_ret = TI_2A_wrapper_delete (&sink_pad->ti_2a_wrapper);
    if (ti_2a_wrapper_ret) {
      GST_ERROR_OBJECT (self, "Unable to delete TI 2A wrapper: %d",
          ti_2a_wrapper_ret);
    }
  }

  gst_tiovx_empty_exemplar ((vx_reference) self->viss_obj.
      ae_awb_result_handle[0]);
  gst_tiovx_empty_exemplar ((vx_reference) self->viss_obj.h3a_stats_handle[0]);

  tiovx_deinit_sensor (&self->sensor_obj);

  /* Delete graph */
  status = tiovx_viss_module_delete (&self->viss_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d",
        status);
    goto out;
  }

  status = tiovx_viss_module_deinit (&self->viss_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_tiovx_isp_allocate_single_user_data_object (GstTIOVXISP * self,
    GstMemory ** memory, vx_object_array user_data_arr, gint num_channels)
{
  vx_size data_size = 0;
  vx_status status = VX_FAILURE;
  GstTIOVXMemoryData *ti_memory = NULL;
  vx_reference user_data = NULL;
  void *addr[MODULE_MAX_NUM_USER_DATA_PLANES] = { NULL };
  void *user_addr[MODULE_MAX_NUM_USER_DATA_PLANES] = { NULL };
  vx_uint32 user_size[MODULE_MAX_NUM_USER_DATA_PLANES];
  vx_uint32 num_user = 0;
  gint prev_size = 0;
  gint i = 0;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (memory, FALSE);
  g_return_val_if_fail (user_data_arr, FALSE);

  if (NULL != *memory) {
    gst_memory_unref (*memory);
  }

  user_data = vxGetObjectArrayItem (user_data_arr, 0);

  status =
      vxQueryUserDataObject ((vx_user_data_object) user_data,
      VX_USER_DATA_OBJECT_SIZE, &data_size, sizeof (data_size));
  vxReleaseReference (&user_data);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Unable to query user data object size from exemplar: %p", user_data);
    goto out;
  }
  *memory =
      gst_allocator_alloc (GST_ALLOCATOR (self->user_data_allocator),
      data_size * num_channels, NULL);
  if (!*memory) {
    GST_ERROR_OBJECT (self, "Unable to allocate memory");
    goto out;
  }

  ti_memory = gst_tiovx_memory_get_data (*memory);
  if (NULL == ti_memory) {
    GST_ERROR_OBJECT (self, "Unable retrieve TI memory");
    goto out;
  }

  user_data = vxGetObjectArrayItem (user_data_arr, 0);
  tivxReferenceExportHandle (user_data,
      user_addr, user_size, MODULE_MAX_NUM_USER_DATA_PLANES, &num_user);
  vxReleaseReference (&user_data);

  for (i = 0; i < num_channels; i++) {
    addr[0] = (void *) (ti_memory->mem_ptr.host_ptr + prev_size);

    user_data = vxGetObjectArrayItem (user_data_arr, i);
    /* User data objects have a single "plane" */
    status = tivxReferenceImportHandle ((vx_reference) user_data,
        (const void **) addr, user_size, 1);
    vxReleaseReference (&user_data);
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (self, "Unable to import handles to exemplar: %p",
          user_data);
      goto out;
    }

    prev_size += user_size[0];
  }


  ret = TRUE;

out:
  if (!ret && *memory) {
    gst_memory_unref (*memory);
  }

  return ret;
}

/**
 * gst_tiovx_isp_allocate_user_data_objects:
 *
 * This subclass has 2 inputs/outputs for the node that aren't inputs/outputs
 * for the overall plugin. This class allocates the memory for these elements.
 *
 * After calling this functions gst_memory_unref should be called on
 * self->aewb_memory and self->h3a_stats_memory
 */
static gboolean
gst_tiovx_isp_allocate_user_data_objects (GstTIOVXISP * self)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);

  GST_DEBUG_OBJECT (self, "Allocating user data objects");

  ret =
      gst_tiovx_isp_allocate_single_user_data_object (self, &self->aewb_memory,
      self->viss_obj.ae_awb_result_arr[0], self->num_channels);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to allocate data for AEWB user data");
    goto out;
  }

  ret =
      gst_tiovx_isp_allocate_single_user_data_object (self,
      &self->h3a_stats_memory, self->viss_obj.h3a_stats_arr[0],
      self->num_channels);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to allocate data for H3A stats user data");
  }

out:
  return ret;
}

static const gchar *
target_id_to_target_name (gint target_id)
{
  GType type = G_TYPE_NONE;
  GEnumClass *enum_class = NULL;
  GEnumValue *enum_value = NULL;
  const gchar *value_nick = NULL;

  type = gst_tiovx_isp_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static gboolean
gst_tiovx_isp_postprocess (GstTIOVXMiso * miso)
{
  GstTIOVXISP *self = NULL;
  GList *sink_pads_list = NULL;
  GList *l = NULL;
  gboolean ret = FALSE;
  struct v4l2_control control;
  gchar *video_dev = NULL;
  vx_map_id h3a_buf_map_id;
  vx_map_id aewb_buf_map_id;
  gint i = 0;

  g_return_val_if_fail (miso, FALSE);

  self = GST_TIOVX_ISP (miso);
  sink_pads_list = GST_ELEMENT (miso)->sinkpads;

  if (postprocess_skip_frames >= ++self->postprocess_iter) {
    GST_LOG_OBJECT (self, "Skipping postprocess iteration #%d",
        self->postprocess_iter);
    ret = TRUE;
    goto exit;
  } else {
    self->postprocess_iter = 0;
  }

  for (l = sink_pads_list, i = 0; l != NULL; l = g_list_next (l), i++) {
    GstTIOVXIspPad *sink_pad = (GstTIOVXIspPad *) l->data;
    tivx_h3a_data_t *h3a_data = NULL;
    tivx_ae_awb_params_t *ae_awb_result = NULL;
    int32_t ti_2a_wrapper_ret = 0;
    vx_user_data_object h3a_stats_ref = NULL;
    vx_user_data_object ae_awb_result_ref = NULL;

    ae_awb_result_ref =
        (vx_user_data_object) vxGetObjectArrayItem (self->
        viss_obj.ae_awb_result_arr[0], i);
    h3a_stats_ref =
        (vx_user_data_object) vxGetObjectArrayItem (self->
        viss_obj.h3a_stats_arr[0], i);

    vxMapUserDataObject (h3a_stats_ref, 0,
        sizeof (tivx_h3a_data_t), &h3a_buf_map_id, (void **) &h3a_data,
        VX_READ_ONLY, VX_MEMORY_TYPE_HOST, 0);
    vxMapUserDataObject (ae_awb_result_ref, 0,
        sizeof (tivx_ae_awb_params_t), &aewb_buf_map_id,
        (void **) &ae_awb_result, VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST, 0);

    if (g_strcmp0 (self->sensor_name, "SENSOR_SONY_IMX390_UB953_D3") == 0) {
      get_imx390_ae_dyn_params (&sink_pad->sensor_in_data.ae_dynPrms);
    } else if (g_strcmp0 (self->sensor_name, "SENSOR_OV2312_UB953_LI") == 0) {
      get_ov2312_ae_dyn_params (&sink_pad->sensor_in_data.ae_dynPrms);
    } else {
      get_imx219_ae_dyn_params (&sink_pad->sensor_in_data.ae_dynPrms);
    }

    ti_2a_wrapper_ret =
        TI_2A_wrapper_process (&sink_pad->ti_2a_wrapper, &sink_pad->aewb_config,
        h3a_data, &sink_pad->sensor_in_data, ae_awb_result,
        &sink_pad->sensor_out_data);
    if (ti_2a_wrapper_ret) {
      GST_ERROR_OBJECT (self, "Unable to process TI 2A wrapper: %d",
          ti_2a_wrapper_ret);
      goto out;
    }

    GST_LOG_OBJECT (sink_pad, "Exposure time output from TI 2A library: %d",
        sink_pad->sensor_out_data.aePrms.exposureTime[0]);
    GST_LOG_OBJECT (sink_pad, "Analog gain output from TI 2A library: %d",
        sink_pad->sensor_out_data.aePrms.analogGain[0]);

    video_dev = sink_pad->videodev;
    if (NULL == video_dev) {
      GST_LOG_OBJECT (sink_pad,
          "Device location was not provided, skipping IOCTL calls");
    } else {
      gint fd = -1;
      int ret_val = -1;
      gint32 analog_gain = 0;
      gint32 coarse_integration_time = 0;

      fd = open (video_dev, O_RDWR | O_NONBLOCK);
      if (-1 == fd) {
        GST_ERROR_OBJECT (self, "Unable to open video device: %s", video_dev);
        goto exit;
      }
      gst_tiovx_isp_map_2A_values (self,
          sink_pad->sensor_out_data.aePrms.exposureTime[0],
          sink_pad->sensor_out_data.aePrms.analogGain[0],
          &coarse_integration_time, &analog_gain);

      GST_LOG_OBJECT (sink_pad, "%s sensor specific exposure time: %d",
          self->sensor_name, coarse_integration_time);
      GST_LOG_OBJECT (sink_pad, "%s Sensor specific analog gain: %d",
          self->sensor_name, analog_gain);

      control.id = exposure_ctrl_id;
      control.value = coarse_integration_time;
      ret_val = ioctl (fd, VIDIOC_S_CTRL, &control);
      if (ret_val < 0) {
        GST_ERROR_OBJECT (self, "Unable to call exposure ioctl: %d", ret_val);
        goto close_fd;
      }

      control.id = analog_gain_ctrl_id;
      control.value = analog_gain;
      ret_val = ioctl (fd, VIDIOC_S_CTRL, &control);
      if (ret_val < 0) {
        GST_ERROR_OBJECT (self, "Unable to call analog gain ioctl: %d",
            ret_val);
      }

    close_fd:
      close (fd);
    }

  out:
    vxUnmapUserDataObject (h3a_stats_ref, h3a_buf_map_id);
    vxUnmapUserDataObject (ae_awb_result_ref, aewb_buf_map_id);

    vxReleaseReference ((vx_reference *) & ae_awb_result_ref);
    vxReleaseReference ((vx_reference *) & h3a_stats_ref);
  }

  ret = TRUE;

exit:
  return ret;
}

/* Typically this is obtained by querying the sensor */
static int32_t
get_imx219_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms)
{
  int32_t status = -1;
  uint8_t count = 0;

  g_return_val_if_fail (p_ae_dynPrms, status);

  p_ae_dynPrms->targetBrightnessRange.min = 40;
  p_ae_dynPrms->targetBrightnessRange.max = 50;
  p_ae_dynPrms->targetBrightness = 45;
  p_ae_dynPrms->threshold = 1;
  p_ae_dynPrms->enableBlc = 1;
  p_ae_dynPrms->exposureTimeStepSize = 1;

  p_ae_dynPrms->exposureTimeRange[count].min = 100;
  p_ae_dynPrms->exposureTimeRange[count].max = 33333;
  p_ae_dynPrms->analogGainRange[count].min = 1024;
  p_ae_dynPrms->analogGainRange[count].max = 8192;
  p_ae_dynPrms->digitalGainRange[count].min = 256;
  p_ae_dynPrms->digitalGainRange[count].max = 256;
  count++;

  p_ae_dynPrms->numAeDynParams = count;
  status = 0;
  return status;
}

static int32_t
get_imx390_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms)
{
  int32_t status = -1;
  uint8_t count = 0;

  g_return_val_if_fail (p_ae_dynPrms, status);

  p_ae_dynPrms->targetBrightnessRange.min = 40;
  p_ae_dynPrms->targetBrightnessRange.max = 50;
  p_ae_dynPrms->targetBrightness = 45;
  p_ae_dynPrms->threshold = 1;
  p_ae_dynPrms->enableBlc = 1;
  p_ae_dynPrms->exposureTimeStepSize = 1;

  p_ae_dynPrms->exposureTimeRange[count].min = 100;
  p_ae_dynPrms->exposureTimeRange[count].max = 33333;
  p_ae_dynPrms->analogGainRange[count].min = 1024;
  p_ae_dynPrms->analogGainRange[count].max = 8192;
  p_ae_dynPrms->digitalGainRange[count].min = 256;
  p_ae_dynPrms->digitalGainRange[count].max = 256;
  count++;

  p_ae_dynPrms->numAeDynParams = count;
  status = 0;
  return status;
}

static int32_t
get_ov2312_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms)
{
  int32_t status = -1;
  uint8_t count = 0;

  g_return_val_if_fail (p_ae_dynPrms, status);

  p_ae_dynPrms->targetBrightnessRange.min = 40;
  p_ae_dynPrms->targetBrightnessRange.max = 50;
  p_ae_dynPrms->targetBrightness = 45;
  p_ae_dynPrms->threshold = 5;
  p_ae_dynPrms->enableBlc = 0;
  p_ae_dynPrms->exposureTimeStepSize = 1;

  p_ae_dynPrms->exposureTimeRange[count].min = 1000;
  p_ae_dynPrms->exposureTimeRange[count].max = 14450;
  p_ae_dynPrms->analogGainRange[count].min = 1;
  p_ae_dynPrms->analogGainRange[count].max = 1;
  p_ae_dynPrms->digitalGainRange[count].min = 1024;
  p_ae_dynPrms->digitalGainRange[count].max = 1024;
  count++;

  p_ae_dynPrms->exposureTimeRange[count].min = 14450;
  p_ae_dynPrms->exposureTimeRange[count].max = 14450;
  p_ae_dynPrms->analogGainRange[count].min = 1;
  p_ae_dynPrms->analogGainRange[count].max = 512;
  p_ae_dynPrms->digitalGainRange[count].min = 1024;
  p_ae_dynPrms->digitalGainRange[count].max = 1024;
  count++;

  p_ae_dynPrms->numAeDynParams = count;
  status = 0;
  return status;
}

static void
gst_tiovx_isp_map_2A_values (GstTIOVXISP * self, int exposure_time,
    int analog_gain, gint32 * exposure_time_mapped, gint32 * analog_gain_mapped)
{
  g_return_if_fail (self);
  g_return_if_fail (exposure_time_mapped);
  g_return_if_fail (analog_gain_mapped);

  if (g_strcmp0 (self->sensor_name, "SENSOR_SONY_IMX390_UB953_D3") == 0) {
    gint i = 0;
    for (i = 0; i <= ISS_IMX390_GAIN_TBL_SIZE; i++) {
      if (gIMX390GainsTable[i][0] >= analog_gain) {
        break;
      }
    }
    *exposure_time_mapped = exposure_time;
    *analog_gain_mapped = gIMX390GainsTable[i][1];
  } else if (g_strcmp0 (self->sensor_name, "SENSOR_SONY_IMX219_RPI") == 0) {
    double multiplier = 0;
    /* Theoretically time per line should be computed as:
     * line_lenght_pck/2*pix_clock_mhz,
     * here it is roughly estimated as 33 ms/1080 lines.
     */

    /* FIXME: This only works for 1080p@30fps mode */
    /* Assuming self->sensor_out_data.aePrms.exposureTime[0] is in miliseconds,
     * then:
     */
    *exposure_time_mapped = (1080 * exposure_time / 33);

    multiplier = analog_gain / 1024.0;
    *analog_gain_mapped = 256.0 - 256.0 / multiplier;
  } else if (g_strcmp0 (self->sensor_name, "SENSOR_OV2312_UB953_LI") == 0) {
    *exposure_time_mapped = (60 * 1300 * exposure_time / 1000000);
    // ms to row_time conversion - row_time(us) = 1000000/fps/height
    *analog_gain_mapped = analog_gain;
  } else {
    GST_ERROR_OBJECT (self, "Unknown sensor: %s", self->sensor_name);
  }
}

static gboolean
gst_tiovx_isp_add_sensor_meta (GstTIOVXMiso * miso, GstBuffer * buf)
{
  GstTIOVXISP *self = GST_TIOVX_ISP (miso);
  GstAggregatorPad *pad = GST_ELEMENT (miso)->sinkpads->data;
  GstMapInfo info;
  GstBuffer *in_buffer = gst_aggregator_pad_peek_buffer (pad);

  /* TODO: 1. add support for meta after 2. add support for batched */
  gst_buffer_map (in_buffer, &info, GST_MAP_READ);
  gst_buffer_add_tiovx_sensor_meta (buf, self->meta_before_size,
      info.data, 0, NULL);
  gst_buffer_unmap (in_buffer, &info);
  gst_buffer_unref (in_buffer);

  return TRUE;
}
