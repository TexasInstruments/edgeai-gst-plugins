/*
 * Copyright (c) [2025] Texas Instruments Incorporated
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

#include "gsttiovxfc.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxallocator.h"
#include "gst-libs/gst/tiovx/gsttiovxsimo.h"
#include "gst-libs/gst/tiovx/gsttiovxpad.h"
#include "gst-libs/gst/tiovx/gsttiovxqueueableobject.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "gsttiovxmultiscalerpad.h"

#include "tiovx_fc_module.h"
#include "ti_2a_wrapper.h"

static const char default_tiovx_sensor_name[] = "SENSOR_SONY_IMX219_RPI";
#define GST_TYPE_TIOVX_FC_TARGET (gst_tiovx_fc_target_get_type())

GST_DEBUG_CATEGORY_STATIC (gst_tiovxfc_debug);


static const gint min_num_exposures = 1;
static const gint default_num_exposures = 1;
static const gint max_num_exposures = 4;

static const gint min_format_msb = 1;
static const gint default_format_msb = 7;
static const gint max_format_msb = 16;

static const gboolean default_lines_interleaved = FALSE;
static const gboolean default_wdr_enabled = FALSE;
static const gboolean default_bypass_cac = TRUE;
static const gboolean default_bypass_dwb = TRUE;
static const gboolean default_bypass_nsf4 = FALSE;
static const guint default_ee_mode = TIVX_VPAC_VISS_EE_MODE_OFF;

static const int input_param_id = 3;
static const int output0_param_id = 12;


static const int default_ae_mode = ALGORITHMS_ISS_AE_AUTO;
static const int default_awb_mode = ALGORITHMS_ISS_AWB_AUTO;
static const guint default_ae_num_skip_frames = 0;
static const guint default_awb_num_skip_frames = 0;

static const guint exposure_ctrl_id = V4L2_CID_EXPOSURE;
static const guint analog_gain_ctrl_id = V4L2_CID_ANALOGUE_GAIN;


#define MIN_ROI_VALUE 0
#define MAX_ROI_VALUE G_MAXUINT32
#define DEFAULT_ROI_VALUE 0

/* Parameters for VPAV MSC Operations */
/* Target definition */
enum
{
  TIVX_TARGET_VPAC_FC_ID = 0,
};

static GType
gst_tiovx_fc_target_get_type (void)
{
  static GType target_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_TARGET_VPAC_FC_ID, "VPAC FC", TIVX_TARGET_VPAC_FC},
  };

  if (!target_type) {
    target_type = g_enum_register_static ("GstTIOVXFCTarget", targets);
  }
  return target_type;
}

#define DEFAULT_TIOVX_FC_TARGET TIVX_TARGET_VPAC_FC_ID

/* Interpolation Method definition */
#define GST_TYPE_TIOVX_FC_INTERPOLATION_METHOD (gst_tiovx_fc_interpolation_method_get_type())
static GType
gst_tiovx_fc_interpolation_method_get_type (void)
{
  static GType interpolation_method_type = 0;

  static const GEnumValue interpolation_methods[] = {
    {VX_INTERPOLATION_BILINEAR, "Bilinear", "bilinear"},
    {VX_INTERPOLATION_NEAREST_NEIGHBOR, "Nearest Neighbor", "nearest-neighbor"},
    {TIVX_VPAC_MSC_INTERPOLATION_GAUSSIAN_32_PHASE, "Gaussian 32 Phase",
        "gaussian-32-phase"},
    {0, NULL, NULL},
  };

  if (!interpolation_method_type) {
    interpolation_method_type =
        g_enum_register_static ("GstTIOVXFlexConnectInterpolationMethod",
        interpolation_methods);
  }
  return interpolation_method_type;
}

#define DEFAULT_TIOVX_FC_INTERPOLATION_METHOD VX_INTERPOLATION_BILINEAR


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

#define ISS_IMX728_GAIN_TBL_SIZE           (421U)

static const uint32_t gIMX728GainsTable[ISS_IMX728_GAIN_TBL_SIZE][2U] =
{
    {1024, 0x0},
    {1036, 0x1},
    {1048, 0x2},
    {1060, 0x3},
    {1072, 0x4},
    {1085, 0x5},
    {1097, 0x6},
    {1110, 0x7},
    {1123, 0x8},
    {1136, 0x9},
    {1149, 0xA},
    {1162, 0xB},
    {1176, 0xC},
    {1189, 0xD},
    {1203, 0xE},
    {1217, 0xF},
    {1231, 0x10},
    {1245, 0x11},
    {1260, 0x12},
    {1274, 0x13},
    {1289, 0x14},
    {1304, 0x15},
    {1319, 0x16},
    {1334, 0x17},
    {1350, 0x18},
    {1366, 0x19},
    {1381, 0x1A},
    {1397, 0x1B},
    {1414, 0x1C},
    {1430, 0x1D},
    {1446, 0x1E},
    {1463, 0x1F},
    {1480, 0x20},
    {1497, 0x21},
    {1515, 0x22},
    {1532, 0x23},
    {1550, 0x24},
    {1568, 0x25},
    {1586, 0x26},
    {1604, 0x27},
    {1623, 0x28},
    {1642, 0x29},
    {1661, 0x2A},
    {1680, 0x2B},
    {1699, 0x2C},
    {1719, 0x2D},
    {1739, 0x2E},
    {1759, 0x2F},
    {1780, 0x30},
    {1800, 0x31},
    {1821, 0x32},
    {1842, 0x33},
    {1863, 0x34},
    {1885, 0x35},
    {1907, 0x36},
    {1929, 0x37},
    {1951, 0x38},
    {1974, 0x39},
    {1997, 0x3A},
    {2020, 0x3B},
    {2043, 0x3C},
    {2067, 0x3D},
    {2091, 0x3E},
    {2115, 0x3F},
    {2139, 0x40},
    {2164, 0x41},
    {2189, 0x42},
    {2215, 0x43},
    {2240, 0x44},
    {2266, 0x45},
    {2292, 0x46},
    {2319, 0x47},
    {2346, 0x48},
    {2373, 0x49},
    {2400, 0x4A},
    {2428, 0x4B},
    {2456, 0x4C},
    {2485, 0x4D},
    {2514, 0x4E},
    {2543, 0x4F},
    {2572, 0x50},
    {2602, 0x51},
    {2632, 0x52},
    {2663, 0x53},
    {2693, 0x54},
    {2725, 0x55},
    {2756, 0x56},
    {2788, 0x57},
    {2820, 0x58},
    {2853, 0x59},
    {2886, 0x5A},
    {2919, 0x5B},
    {2953, 0x5C},
    {2987, 0x5D},
    {3022, 0x5E},
    {3057, 0x5F},
    {3092, 0x60},
    {3128, 0x61},
    {3164, 0x62},
    {3201, 0x63},
    {3238, 0x64},
    {3276, 0x65},
    {3314, 0x66},
    {3352, 0x67},
    {3391, 0x68},
    {3430, 0x69},
    {3470, 0x6A},
    {3510, 0x6B},
    {3551, 0x6C},
    {3592, 0x6D},
    {3633, 0x6E},
    {3675, 0x6F},
    {3718, 0x70},
    {3761, 0x71},
    {3805, 0x72},
    {3849, 0x73},
    {3893, 0x74},
    {3938, 0x75},
    {3984, 0x76},
    {4030, 0x77},
    {4077, 0x78},
    {4124, 0x79},
    {4172, 0x7A},
    {4220, 0x7B},
    {4269, 0x7C},
    {4318, 0x7D},
    {4368, 0x7E},
    {4419, 0x7F},
    {4470, 0x80},
    {4522, 0x81},
    {4574, 0x82},
    {4627, 0x83},
    {4681, 0x84},
    {4735, 0x85},
    {4790, 0x86},
    {4845, 0x87},
    {4901, 0x88},
    {4958, 0x89},
    {5015, 0x8A},
    {5073, 0x8B},
    {5132, 0x8C},
    {5192, 0x8D},
    {5252, 0x8E},
    {5313, 0x8F},
    {5374, 0x90},
    {5436, 0x91},
    {5499, 0x92},
    {5563, 0x93},
    {5627, 0x94},
    {5692, 0x95},
    {5758, 0x96},
    {5825, 0x97},
    {5893, 0x98},
    {5961, 0x99},
    {6030, 0x9A},
    {6100, 0x9B},
    {6170, 0x9C},
    {6242, 0x9D},
    {6314, 0x9E},
    {6387, 0x9F},
    {6461, 0xA0},
    {6536, 0xA1},
    {6611, 0xA2},
    {6688, 0xA3},
    {6766, 0xA4},
    {6844, 0xA5},
    {6923, 0xA6},
    {7003, 0xA7},
    {7084, 0xA8},
    {7166, 0xA9},
    {7249, 0xAA},
    {7333, 0xAB},
    {7418, 0xAC},
    {7504, 0xAD},
    {7591, 0xAE},
    {7679, 0xAF},
    {7768, 0xB0},
    {7858, 0xB1},
    {7949, 0xB2},
    {8041, 0xB3},
    {8134, 0xB4},
    {8228, 0xB5},
    {8323, 0xB6},
    {8420, 0xB7},
    {8517, 0xB8},
    {8616, 0xB9},
    {8716, 0xBA},
    {8817, 0xBB},
    {8919, 0xBC},
    {9022, 0xBD},
    {9126, 0xBE},
    {9232, 0xBF},
    {9339, 0xC0},
    {9447, 0xC1},
    {9557, 0xC2},
    {9667, 0xC3},
    {9779, 0xC4},
    {9892, 0xC5},
    {10007, 0xC6},
    {10123, 0xC7},
    {10240, 0xC8},
    {10359, 0xC9},
    {10479, 0xCA},
    {10600, 0xCB},
    {10723, 0xCC},
    {10847, 0xCD},
    {10972, 0xCE},
    {11099, 0xCF},
    {11228, 0xD0},
    {11358, 0xD1},
    {11489, 0xD2},
    {11623, 0xD3},
    {11757, 0xD4},
    {11893, 0xD5},
    {12031, 0xD6},
    {12170, 0xD7},
    {12311, 0xD8},
    {12454, 0xD9},
    {12598, 0xDA},
    {12744, 0xDB},
    {12891, 0xDC},
    {13041, 0xDD},
    {13192, 0xDE},
    {13344, 0xDF},
    {13499, 0xE0},
    {13655, 0xE1},
    {13813, 0xE2},
    {13973, 0xE3},
    {14135, 0xE4},
    {14299, 0xE5},
    {14464, 0xE6},
    {14632, 0xE7},
    {14801, 0xE8},
    {14973, 0xE9},
    {15146, 0xEA},
    {15321, 0xEB},
    {15499, 0xEC},
    {15678, 0xED},
    {15860, 0xEE},
    {16044, 0xEF},
    {16229, 0xF0},
    {16417, 0xF1},
    {16607, 0xF2},
    {16800, 0xF3},
    {16994, 0xF4},
    {17191, 0xF5},
    {17390, 0xF6},
    {17591, 0xF7},
    {17795, 0xF8},
    {18001, 0xF9},
    {18210, 0xFA},
    {18420, 0xFB},
    {18634, 0xFC},
    {18850, 0xFD},
    {19068, 0xFE},
    {19289, 0xFF},
    {19512, 0x100},
    {19738, 0x101},
    {19966, 0x102},
    {20198, 0x103},
    {20431, 0x104},
    {20668, 0x105},
    {20907, 0x106},
    {21149, 0x107},
    {21394, 0x108},
    {21642, 0x109},
    {21893, 0x10A},
    {22146, 0x10B},
    {22403, 0x10C},
    {22662, 0x10D},
    {22925, 0x10E},
    {23190, 0x10F},
    {23458, 0x110},
    {23730, 0x111},
    {24005, 0x112},
    {24283, 0x113},
    {24564, 0x114},
    {24848, 0x115},
    {25136, 0x116},
    {25427, 0x117},
    {25722, 0x118},
    {26020, 0x119},
    {26321, 0x11A},
    {26626, 0x11B},
    {26934, 0x11C},
    {27246, 0x11D},
    {27561, 0x11E},
    {27880, 0x11F},
    {28203, 0x120},
    {28530, 0x121},
    {28860, 0x122},
    {29194, 0x123},
    {29532, 0x124},
    {29874, 0x125},
    {30220, 0x126},
    {30570, 0x127},
    {30924, 0x128},
    {31282, 0x129},
    {31645, 0x12A},
    {32011, 0x12B},
    {32382, 0x12C},
    {32757, 0x12D},
    {33136, 0x12E},
    {33520, 0x12F},
    {33908, 0x130},
    {34300, 0x131},
    {34698, 0x132},
    {35099, 0x133},
    {35506, 0x134},
    {35917, 0x135},
    {36333, 0x136},
    {36754, 0x137},
    {37179, 0x138},
    {37610, 0x139},
    {38045, 0x13A},
    {38486, 0x13B},
    {38931, 0x13C},
    {39382, 0x13D},
    {39838, 0x13E},
    {40300, 0x13F},
    {40766, 0x140},
    {41238, 0x141},
    {41716, 0x142},
    {42199, 0x143},
    {42687, 0x144},
    {43182, 0x145},
    {43682, 0x146},
    {44188, 0x147},
    {44699, 0x148},
    {45217, 0x149},
    {45740, 0x14A},
    {46270, 0x14B},
    {46806, 0x14C},
    {47348, 0x14D},
    {47896, 0x14E},
    {48451, 0x14F},
    {49012, 0x150},
    {49579, 0x151},
    {50153, 0x152},
    {50734, 0x153},
    {51322, 0x154},
    {51916, 0x155},
    {52517, 0x156},
    {53125, 0x157},
    {53740, 0x158},
    {54363, 0x159},
    {54992, 0x15A},
    {55629, 0x15B},
    {56273, 0x15C},
    {56925, 0x15D},
    {57584, 0x15E},
    {58251, 0x15F},
    {58925, 0x160},
    {59607, 0x161},
    {60298, 0x162},
    {60996, 0x163},
    {61702, 0x164},
    {62417, 0x165},
    {63139, 0x166},
    {63870, 0x167},
    {64610, 0x168},
    {65358, 0x169},
    {66115, 0x16A},
    {66881, 0x16B},
    {67655, 0x16C},
    {68438, 0x16D},
    {69231, 0x16E},
    {70033, 0x16F},
    {70843, 0x170},
    {71664, 0x171},
    {72494, 0x172},
    {73333, 0x173},
    {74182, 0x174},
    {75041, 0x175},
    {75910, 0x176},
    {76789, 0x177},
    {77678, 0x178},
    {78578, 0x179},
    {79488, 0x17A},
    {80408, 0x17B},
    {81339, 0x17C},
    {82281, 0x17D},
    {83234, 0x17E},
    {84198, 0x17F},
    {85173, 0x180},
    {86159, 0x181},
    {87157, 0x182},
    {88166, 0x183},
    {89187, 0x184},
    {90219, 0x185},
    {91264, 0x186},
    {92321, 0x187},
    {93390, 0x188},
    {94471, 0x189},
    {95565, 0x18A},
    {96672, 0x18B},
    {97791, 0x18C},
    {98924, 0x18D},
    {100069, 0x18E},
    {101228, 0x18F},
    {102400, 0x190},
    {103586, 0x191},
    {104785, 0x192},
    {105999, 0x193},
    {107226, 0x194},
    {108468, 0x195},
    {109724, 0x196},
    {110994, 0x197},
    {112279, 0x198},
    {113580, 0x199},
    {114895, 0x19A},
    {116225, 0x19B},
    {117571, 0x19C},
    {118932, 0x19D},
    {120310, 0x19E},
    {121703, 0x19F},
    {123112, 0x1A0},
    {124537, 0x1A1},
    {125980, 0x1A2},
    {127438, 0x1A3},
    {128914, 0x1A4}
};

/* TIOVX FC Pad */
#define GST_TYPE_TIOVX_FC_PAD (gst_tiovx_fc_pad_get_type())
G_DECLARE_FINAL_TYPE (GstTIOVXFCPad, gst_tiovx_fc_pad,
    GST_TIOVX, FC_PAD, GstTIOVXPad);


struct _GstTIOVXFCPad
{
  GstTIOVXPad base;

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

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_fc_pad_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXFCPad, gst_tiovx_fc_pad,
    GST_TYPE_TIOVX_PAD,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_fc_pad_debug_category,
        "tiovxfcpad", 0, "debug category for TIOVX FC pad class"));

struct _GstTIOVXFCPadClass
{
  GstTIOVXPadClass parent_class;
};

enum
{
  PROP_DEVICE = 1,
  PROP_DCC_2A_CONFIG_FILE,
  PROP_AE_MODE,
  PROP_AWB_MODE,
  PROP_AE_NUM_SKIP_FRAMES,
  PROP_AWB_NUM_SKIP_FRAMES,
  PROP_INTERPOLATION_METHOD,
  PROP_0,
  PROP_DCC_ISP_CONFIG_FILE,
  PROP_SENSOR_NAME,
  PROP_TARGET,
  PROP_NUM_EXPOSURES,
  PROP_LINE_INTERLEAVED,
  PROP_FORMAT_MSB,
  PROP_WDR_ENABLED,
  PROP_BYPASS_CAC,
  PROP_BYPASS_DWB,
  PROP_BYPASS_NSF4,
  PROP_EE_MODE,
};


/* Formats definition */
#if defined(SOC_AM62A)
/* AM62A/VPAC3L with VISS_OUT2: Supports U8, NV12 only */
#define TIOVX_FC_SUPPORTED_FORMATS_SRC "{NV12, GRAY8}"
#else
/* Full VPAC support: All standard formats */
#define TIOVX_FC_SUPPORTED_FORMATS_SRC "{NV12, GRAY8, GRAY16_LE, UYVY, YUYV}"
#endif
// #define TIOVX_FC_SUPPORTED_FORMATS_SINK "{ NV12, GRAY8, GRAY16_LE }"
#define TIOVX_FC_SUPPORTED_FORMATS_SINK "{ bggr, gbrg, grbg, rggb}"
#define TIOVX_FC_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_FC_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_FC_SUPPORTED_CHANNELS "[1 , 10]"

/* Src caps */
#define TIOVX_FC_STATIC_CAPS_SRC                           \
  "video/x-raw, "                                           \
  "format = (string) " TIOVX_FC_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_FC_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_FC_SUPPORTED_HEIGHT                    \
  "; "                                                      \
  "video/x-raw(" GST_CAPS_FEATURE_BATCHED_MEMORY "), "      \
  "format = (string) " TIOVX_FC_SUPPORTED_FORMATS_SRC ", " \
  "width = " TIOVX_FC_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_FC_SUPPORTED_HEIGHT ", "                \
  "num-channels = " TIOVX_FC_SUPPORTED_CHANNELS

#define TIOVX_FC_STATIC_CAPS_SINK                         \
  "video/x-bayer, "                                                    \
  "format = (string) " TIOVX_FC_SUPPORTED_FORMATS_SINK ", " \
  "width = " TIOVX_FC_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_FC_SUPPORTED_HEIGHT

/* Pads definitions */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TIOVX_FC_STATIC_CAPS_SINK)
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (TIOVX_FC_STATIC_CAPS_SRC)
    );  


struct _GstTIOVXFC
{
  GstTIOVXSimo element;
  gchar *dcc_fc_config_file;
  gchar *sensor_name;
  gint target_id;
  SensorObj sensor_obj;

  gint num_exposures;
  gboolean line_interleaved;
  gint format_msb;
  gint meta_height_before;
  gint meta_height_after;
  guint wdr_enabled;
  guint bypass_cac;
  guint bypass_dwb;
  guint bypass_nsf4;
  guint ee_mode;
  gint total_height;
  gint image_height;

  GstTIOVXAllocator *user_data_allocator;

  GstMemory *aewb_memory;
  GstMemory *h3a_stats_memory;

  TIOVXFCModuleObj fc_obj; 

  vx_reference input_references[MAX_NUM_CHANNELS];
  
  gint num_channels;
  guint postprocess_iter;

  gint interpolation_method;

};

static GType
gst_tiovx_fc_awb_mode_get_type (void)
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
gst_tiovx_fc_ae_mode_get_type (void)
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

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_fc_debug);
#define GST_CAT_DEFAULT gst_tiovx_fc_debug

#define gst_tiovx_fc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXFC, gst_tiovx_fc,
    GST_TYPE_TIOVX_SIMO, GST_DEBUG_CATEGORY_INIT (gst_tiovx_fc_debug,
        "tiovxfc", 0, "debug category for the tiovxfc element"));

/* Function Prototypes */

static const gchar *
target_id_to_target_name (gint target_id);

static gboolean
gst_tiovx_fc_deinit_module (GstTIOVXSimo * simo);

static gboolean
gst_tiovx_fc_postprocess (GstTIOVXSimo * simo);

static gboolean 
gst_tiovx_fc_create_graph ( GstTIOVXSimo * simo, vx_context context, 
    vx_graph graph);

static GList *
gst_tiovx_fc_fixate_caps (GstTIOVXSimo * simo,
GstCaps * sink_caps, GList * src_caps_list);

static gboolean 
gst_tiovx_fc_get_node_info (GstTIOVXSimo * simo, vx_node * node,
    GstTIOVXPad * sink_pad, GList * src_pads, GList ** queueable_objects);

static gboolean 
gst_tiovx_fc_configure_module (GstTIOVXSimo * simo);

static void
gst_tivox_fc_compute_src_dimension (GstTIOVXSimo * simo,
    const GValue * dimension, GValue * out_value, guint roi_len);

static void
gst_tivox_fc_compute_sink_dimension (GstTIOVXSimo * simo,
    const GValue * dimension, GValue * out_value, guint roi_len);

static GstCaps *
gst_tiovx_fc_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list, GList *src_pads);

static GstCaps *
gst_tiovx_fc_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps, GstTIOVXPad *src_pad_tiovx);

static gboolean 
gst_tiovx_fc_init_module (GstTIOVXSimo * simo,
    vx_context context, GstTIOVXPad * sink_pad, GList * src_pads, GstCaps * sink_caps,
    GList * src_caps_list, guint num_channels);

static void
gst_tiovx_fc_set_property(GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void
gst_tiovx_fc_get_property(GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_tiovx_fc_finalize (GObject * obj);

static int32_t
get_imx219_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms);

static int32_t get_imx390_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms);

static int32_t get_imx728_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms);

static int32_t get_ov2312_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms);

static int32_t get_ox05b1s_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms);


static void
gst_tiovx_fc_map_2A_values (GstTIOVXFC * self, int exposure_time,
    int analog_gain, gint32 * exposure_time_mapped, gint32 * analog_gain_mapped);

static void
gst_tiovx_fc_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void
gst_tiovx_fc_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_tiovx_fc_pad_finalize (GObject * obj);


static void
gst_tiovx_fc_pad_class_init (GstTIOVXFCPadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_pad_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_pad_get_property);
  gobject_class->finalize = 
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_pad_finalize);

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
          gst_tiovx_fc_ae_mode_get_type (),
          default_ae_mode,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_AWB_MODE,
      g_param_spec_enum ("awb-mode", "Auto white balance mode",
          "Flag to set if the auto white balance algorithm mode.",
          gst_tiovx_fc_awb_mode_get_type (),
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
gst_tiovx_fc_pad_init (GstTIOVXFCPad * self)
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
gst_tiovx_fc_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXFCPad *self = GST_TIOVX_FC_PAD (object);

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
gst_tiovx_fc_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXFCPad *self = GST_TIOVX_FC_PAD (object);

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
gst_tiovx_fc_pad_finalize (GObject * obj)
{
  GstTIOVXFCPad *self = GST_TIOVX_FC_PAD (obj);

  g_free (self->videodev);
  self->videodev = NULL;

  g_free (self->dcc_2a_config_file);
  self->dcc_2a_config_file = NULL;


  G_OBJECT_CLASS (gst_tiovx_fc_pad_parent_class)->finalize (obj);
}

static GType
gst_tiovx_fc_ee_mode_get_type (void)
{
  static GType ee_mode_type = 0;

  static const GEnumValue targets[] = {
    {TIVX_VPAC_VISS_EE_MODE_OFF, "EE mode off", "EE_MODE_OFF"},
    {TIVX_VPAC_VISS_EE_MODE_Y12, "Edge Enhancer is enabled on Y12 output (output0)", "EE_MODE_Y12"},
    {TIVX_VPAC_VISS_EE_MODE_Y8, "Edge Enhancer is enabled on Y8 output (output2)", "EE_MODE_Y8"},
    {0, NULL, NULL},
  };

  if (!ee_mode_type) {
    ee_mode_type = g_enum_register_static ("GstTIOVXFCEEModes", targets);
  }
  return ee_mode_type;
}

static void
gst_tiovx_fc_class_init(GstTIOVXFCClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstTIOVXSimoClass *gsttiovxsimo_class = GST_TIOVX_SIMO_CLASS (klass);
  GstPadTemplate *src_temp = NULL;
  GstPadTemplate *sink_temp = NULL;

  GST_DEBUG_CATEGORY_INIT (gst_tiovxfc_debug, "tiovxfc", 0, "TIOVX FC PLUGIN");
  GST_DEBUG("class_init reached");

  gst_element_class_set_details_simple (gstelement_class,
        "TIOVX VISS->MSC FC",
        "Filter",
        "VPAC (VISS->MSC) Flexconnect using the TIOVX Modules API",
        "Pratham Deshmukh <p-deshmukh@ti.com>");

    src_temp =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_TIOVX_MULTISCALER_PAD);
      gst_element_class_add_pad_template (gstelement_class, src_temp);

    sink_temp = 
      gst_pad_template_new_from_static_pad_template_with_gtype (&sink_template,
      GST_TYPE_TIOVX_FC_PAD);
      gst_element_class_add_pad_template (gstelement_class, sink_temp);

    gobject_class->set_property = gst_tiovx_fc_set_property;
    gobject_class->get_property = gst_tiovx_fc_get_property;                
    
    gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_fc_finalize);    
  
  g_object_class_install_property (gobject_class, PROP_DCC_ISP_CONFIG_FILE,
      g_param_spec_string ("dcc-fc-isp-file", "DCC FC ISP File",
          "TIOVX DCC tuning binary file for the given image sensor.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SENSOR_NAME,
      g_param_spec_string ("sensor-name", "Sensor name",
          "TIOVX camera sensor string ID. Below are the supported sensors\n"
          "                                      SENSOR_SONY_IMX219_RPI",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_TARGET,
      g_param_spec_enum ("target", "Target",
          "TIOVX target to use by this element.",
          GST_TYPE_TIOVX_FC_TARGET,
          DEFAULT_TIOVX_FC_TARGET,
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

  g_object_class_install_property (gobject_class, PROP_WDR_ENABLED,
      g_param_spec_boolean ("wdr-enabled", "Wdr Enabled",
          "Set if Camera wdr mode is enabled", default_wdr_enabled,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BYPASS_CAC,
      g_param_spec_boolean ("bypass-cac", "Bypass CAC",
          "Set to bypass chromatic aberation correction (CAC)", default_bypass_cac,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BYPASS_DWB,
      g_param_spec_boolean ("bypass-dwb", "Bypass DWB",
          "Set to bypass dynamic white balance (DWB)", default_bypass_dwb,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BYPASS_NSF4,
      g_param_spec_boolean ("bypass-nsf4", "Bypass NSF4",
          "Set to bypass Noise Filtering", default_bypass_nsf4,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_EE_MODE,
      g_param_spec_enum ("ee-mode", "Edge Enhancement mode",
          "Flag to set Edge Enhancement mode.",
          gst_tiovx_fc_ee_mode_get_type (),
          default_ee_mode,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_INTERPOLATION_METHOD,
      g_param_spec_enum ("interpolation-method", "Interpolation Method",
          "Interpolation method to use by the scaler",
          GST_TYPE_TIOVX_FC_INTERPOLATION_METHOD,
          DEFAULT_TIOVX_FC_INTERPOLATION_METHOD,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gsttiovxsimo_class->init_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_init_module);

  gsttiovxsimo_class->configure_module = 
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_configure_module);
  
  gsttiovxsimo_class->get_node_info =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_get_node_info);

  gsttiovxsimo_class->create_graph =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_create_graph);

  gsttiovxsimo_class->get_sink_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_get_sink_caps);

  gsttiovxsimo_class->get_src_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_get_src_caps);

  gsttiovxsimo_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_fixate_caps);

  gsttiovxsimo_class->deinit_module =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_deinit_module);

  gsttiovxsimo_class->postprocess =
      GST_DEBUG_FUNCPTR (gst_tiovx_fc_postprocess);

}



static void
gst_tiovx_fc_init (GstTIOVXFC * self)
{
  GST_DEBUG_OBJECT (self, "Entering gst_tiovx_fc_init\n");

  self->dcc_fc_config_file = NULL;
  self->sensor_name = g_strdup (default_tiovx_sensor_name);

  self->num_exposures = default_num_exposures;
  self->line_interleaved = default_lines_interleaved;
  self->format_msb = default_format_msb;
  self->meta_height_before = 0;
  self->meta_height_after = 0;
  self->wdr_enabled = default_wdr_enabled;
  self->bypass_cac = default_bypass_cac;
  self->bypass_dwb = default_bypass_dwb;
  self->bypass_nsf4 = default_bypass_nsf4;
  self->ee_mode = default_ee_mode;

  self->aewb_memory = NULL;
  self->h3a_stats_memory = NULL;

  self->user_data_allocator = g_object_new (GST_TYPE_TIOVX_ALLOCATOR, NULL);

  self->num_channels = 0;
  self->postprocess_iter = 0;

  for (gint i = 0; i < MAX_NUM_CHANNELS; i++) {
    self->input_references[i] = NULL;
  }
  
  self->target_id = DEFAULT_TIOVX_FC_TARGET;
  self->interpolation_method = DEFAULT_TIOVX_FC_INTERPOLATION_METHOD;

}

static gboolean
gst_tiovx_fc_configure_module (GstTIOVXSimo * simo)
{
  GstTIOVXFC *self = NULL;
  gboolean ret = TRUE;
  vx_status status = VX_FAILURE;
  vx_status check_status = VX_FAILURE;

  g_return_val_if_fail (simo, FALSE);
  self = GST_TIOVX_FC (simo);

  GST_DEBUG_OBJECT (self, "Update filter coeffs");
  status = tiovx_fc_module_update_filter_coeffs (&self->fc_obj);
  if (VX_SUCCESS != status){
    GST_ERROR_OBJECT (self,
      "Module configure filter coefficients failed with error: %d", status);
    ret = FALSE;
    goto out;
  }
  
  GST_DEBUG_OBJECT (self, "Release buffer scaler");
  status = tiovx_fc_module_release_buffers (&self->fc_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Module configure release buffer failed with error: %d", status);
    ret = FALSE;
    goto out;
  }

out: 
  GST_ERROR_OBJECT (self,
          "Configure module ret is: %d\n", ret);
  return ret;
}




static gboolean 
gst_tiovx_fc_create_graph ( GstTIOVXSimo * simo, vx_context context, 
    vx_graph graph)
{
  GstTIOVXFC *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
  const gchar *target = NULL;

  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (context, FALSE);
  g_return_val_if_fail (graph, FALSE);

  self = GST_TIOVX_FC (simo);

  GST_OBJECT_LOCK (GST_OBJECT (self));
  target = target_id_to_target_name (self->target_id);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  GST_INFO_OBJECT (self, "TIOVX Target to use: %s", target);

  GST_DEBUG_OBJECT (self, "Creating FC graph");
  status = tiovx_fc_module_create (graph, &self->fc_obj, NULL, NULL, target);

  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Create graph failed with error: %d", status);
    goto out;
  }

  GST_DEBUG_OBJECT (self, "Finished creating flexconnect graph");

  ret = TRUE;

out:
  return ret;
}

static void
gst_tivox_fc_compute_src_dimension (GstTIOVXSimo * simo,
    const GValue * dimension, GValue * out_value, guint roi_len)
{
  static const gint scale = 4;
  gint out_max = -1;
  gint out_min = -1;
  gint dim_max = -1;
  gint dim_min = -1;
 

  g_return_if_fail (simo);
  g_return_if_fail (dimension);
  g_return_if_fail (out_value);

  if (roi_len) {
    dim_max = dim_min = roi_len;
  } else if (GST_VALUE_HOLDS_INT_RANGE (dimension)) {
    dim_max = gst_value_get_int_range_max (dimension);
    dim_min = gst_value_get_int_range_min (dimension);
  } else {
    dim_max = dim_min = g_value_get_int (dimension);
  }

  out_max = dim_max;
  out_min = 1.0 * dim_min / scale + 0.5;

  if (0 == out_min) {
    out_min = 1;
  }

  GST_DEBUG_OBJECT (simo,
      "computed an output of [%d, %d] from an input of [%d, %d]", out_min,
      out_max, dim_min, dim_max);

  g_value_init (out_value, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (out_value, out_min, out_max);
}

static void
gst_tivox_fc_compute_sink_dimension (GstTIOVXSimo * simo,
    const GValue * dimension, GValue * out_value, guint roi_len)
{
  static const gint scale = 4;
  gint out_max = -1;
  gint out_min = -1;
  gint dim_max = -1;
  gint dim_min = -1;

  g_return_if_fail (simo);
  g_return_if_fail (dimension);
  g_return_if_fail (out_value);


  if (GST_VALUE_HOLDS_INT_RANGE (dimension)) {
    dim_max = gst_value_get_int_range_max (dimension);
    dim_min = gst_value_get_int_range_min (dimension);
  } else {
    dim_max = dim_min = g_value_get_int (dimension);
  }

  out_min = dim_min;

  if (G_MAXINT == dim_max || (G_MAXINT / scale) < dim_max || roi_len) {
    out_max = G_MAXINT;
  } else {
    out_max = dim_max * scale;
  }

  GST_DEBUG_OBJECT (simo,
      "computed an input of [%d, %d] from an output of [%d, %d]", out_min,
      out_max, dim_min, dim_max);

  g_value_init (out_value, GST_TYPE_INT_RANGE);
  gst_value_set_int_range (out_value, out_min, out_max);
}

typedef void (*GstTIOVXDimFunc) (GstTIOVXSimo * simo,
    const GValue * dimension, GValue * out_value, guint roi_len);

static void
gst_tivox_fc_compute_named (GstTIOVXSimo * simo,
    GstStructure * structure, const gchar * name, guint roi_len,
    GstTIOVXDimFunc func)
{
  const GValue *input = NULL;
  GValue output = G_VALUE_INIT;
 
  g_return_if_fail (simo);
  g_return_if_fail (structure);
  g_return_if_fail (name);
  g_return_if_fail (func);

  input = gst_structure_get_value (structure, name);
  func (simo, input, &output, roi_len);
  gst_structure_set_value (structure, name, &output);

  g_value_unset (&output);
}

static GstCaps *
gst_tiovx_fc_get_sink_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GList * src_caps_list, GList *src_pads)
{
  GstCaps *sink_caps = NULL;
  GstCaps *template_caps = NULL;
  GList *l = NULL;
  GList *p = NULL;
  gint i = 0;

  GstTIOVXDimFunc func;
  GstCaps *src_caps;
  GstTIOVXMultiScalerPad *src_pad;

  g_return_val_if_fail (simo, NULL);
  g_return_val_if_fail (src_caps_list, NULL);

  GST_DEBUG_OBJECT (simo,
      "Computing sink caps based on src caps and filter %"
      GST_PTR_FORMAT, filter);

  template_caps = gst_static_pad_template_get_caps (&sink_template);

  if (template_caps != NULL) {
    GST_DEBUG_OBJECT (simo, "Retrieved sink template caps: %s", 
        gst_caps_to_string(template_caps));
  } else {
    GST_ERROR_OBJECT (simo, "Failed to retrieve capabilities from the sink template");
  }

  sink_caps = gst_caps_copy (template_caps);
  gst_caps_unref (template_caps);

  for (l = src_caps_list, p = src_pads; l != NULL;
      l = l->next, p = p->next) {
     func = gst_tivox_fc_compute_sink_dimension;
     src_caps = gst_caps_copy ((GstCaps *) l->data);
     src_pad = (GstTIOVXMultiScalerPad *) p->data;

    for (i = 0; i < gst_caps_get_size (src_caps); i++) {
      GstStructure *st = gst_caps_get_structure (src_caps, i);
      gst_tivox_fc_compute_named (simo, st, "width",
          src_pad->roi_width, func);
      gst_tivox_fc_compute_named (simo, st, "height",
          src_pad->roi_height, func);
    }

    /* Free the copied caps */
    gst_caps_unref (src_caps);
  }
    // Apply filter if provided
  if (filter) {
    GstCaps *tmp = sink_caps;
    sink_caps = gst_caps_intersect (sink_caps, filter);
    gst_caps_unref (tmp);
  }
  GST_DEBUG_OBJECT (simo, "Final Sink caps: %" GST_PTR_FORMAT, sink_caps);

  return sink_caps;

}

static GstCaps *
gst_tiovx_fc_get_src_caps (GstTIOVXSimo * simo,
    GstCaps * filter, GstCaps * sink_caps, GstTIOVXPad *src_pad_tiovx)
{
  GstCaps *src_caps = NULL;
  GstCaps *template_caps = NULL;
  gint i = 0;
  GstTIOVXMultiScalerPad *src_pad;
  GstTIOVXDimFunc func;


  g_return_val_if_fail (simo, NULL);
  g_return_val_if_fail (sink_caps, NULL);

  GST_DEBUG_OBJECT (simo,
      "Computing src caps based on sink caps %" GST_PTR_FORMAT " and filter %"
      GST_PTR_FORMAT, sink_caps, filter);

  template_caps = gst_static_pad_template_get_caps (&src_template);

  if (template_caps != NULL) {
    GST_DEBUG_OBJECT (simo, "Successfully retrieved src template capabilities: %s",
        gst_caps_to_string(template_caps));
  } else {
    GST_ERROR_OBJECT (simo, "Failed to retrieve capabilities from the static pad template");
    return NULL;
  }

  src_caps = gst_caps_copy (template_caps);
  gst_caps_unref (template_caps);
  src_pad = (GstTIOVXMultiScalerPad *) src_pad_tiovx;

  GST_INFO_OBJECT (simo,
      "The gst_caps_get_size is: %d", gst_caps_get_size (src_caps));

  for (i = 0; i < gst_caps_get_size (src_caps); i++) {
    GstStructure *st = gst_caps_get_structure (src_caps, i);

    func = gst_tivox_fc_compute_src_dimension;

    gst_tivox_fc_compute_named (simo, st, "width",
        src_pad->roi_width, func);
    gst_tivox_fc_compute_named (simo, st, "height",
        src_pad->roi_height, func);
  }

  if (filter) {
    GstCaps *tmp = src_caps;
    src_caps = gst_caps_intersect (src_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_INFO_OBJECT (simo,
      "Resulting supported src caps by TIOVX flexconnect node: %"
      GST_PTR_FORMAT, src_caps);

  return src_caps;
}

static GList *
gst_tiovx_fc_fixate_caps (GstTIOVXSimo *simo,
    GstCaps *sink_caps, GList *src_caps_list)
{
    GstStructure *sink_structure = NULL;
    GList *result_caps_list = NULL;
    GList *l = NULL;

    gint width = 0, height = 0;
    gint meta_height_before = 0, meta_height_after = 0;

    const gchar *input_format = NULL;
    const GValue *vframerate = NULL;

    g_return_val_if_fail (simo, NULL);
    g_return_val_if_fail (sink_caps, NULL);
    g_return_val_if_fail (gst_caps_is_fixed (sink_caps), NULL);
    g_return_val_if_fail (src_caps_list, NULL);

    sink_structure = gst_caps_get_structure (sink_caps, 0);

    if (!gst_structure_get_int (sink_structure, "width", &width)) {
      GST_ERROR_OBJECT (simo, "Width is missing in sink caps");
      return NULL;
    }
    if (!gst_structure_get_int (sink_structure, "height", &height)) {
      GST_ERROR_OBJECT (simo, "Height is missing in sink caps");
      return NULL;
    }

    gst_structure_get_int (sink_structure, "meta-height-before", &meta_height_before);
    gst_structure_get_int (sink_structure, "meta-height-after", &meta_height_after);

    height = height - meta_height_before - meta_height_after;
    if (height <= 0) {
    GST_ERROR_OBJECT (simo,
    "Invalid effective height after meta cropping: %d (before=%d after=%d)",
    height, meta_height_before, meta_height_after);
    return NULL;
    }

    input_format = gst_structure_get_string (sink_structure, "format");
    if (NULL == input_format) {
    GST_ERROR_OBJECT (simo, "Format is missing in sink caps");
    return NULL;
    }

    vframerate = gst_structure_get_value (sink_structure, "framerate");
    if (NULL == vframerate) {
    GST_ERROR_OBJECT (simo, "Framerate is missing in sink caps");
    return NULL;
    }

    GST_DEBUG_OBJECT (simo, "Fixating src caps from sink caps %" GST_PTR_FORMAT, sink_caps);

    for (l = src_caps_list; l != NULL; l = l->next) {
        GstCaps *src_caps = (GstCaps *) l->data;
        GstStructure *src_st = NULL;
        GstCaps *out_caps = NULL;
        GstStructure *out_st = NULL;
        const GValue *vwidth = NULL, *vheight = NULL;
        const gchar *chosen_out_fmt = NULL;

        #if defined(SOC_AM62A) || defined(SOC_J722S)
        GValue output_formats = G_VALUE_INIT;
        GValue fmt = G_VALUE_INIT;
        #endif

        if (src_caps == NULL) {
          GST_ERROR_OBJECT (simo, "NULL src_caps in src_caps_list");
          goto error;
        }

        GST_DEBUG_OBJECT (simo, "Processing src_caps: %" GST_PTR_FORMAT, src_caps);

        out_caps = gst_caps_make_writable (gst_caps_copy (src_caps));

        src_st = gst_caps_get_structure (src_caps, 0);
        vwidth = gst_structure_get_value (src_st, "width");
        vheight = gst_structure_get_value (src_st, "height");

        out_st = gst_caps_get_structure (out_caps, 0);

        gst_structure_set_value (out_st, "framerate", vframerate);

        if (vwidth) {
          gst_structure_fixate_field_nearest_int (out_st, "width", width);
        }
        if (vheight) {
          gst_structure_fixate_field_nearest_int (out_st, "height", height);
        }

        if (g_list_length (src_caps_list) > 1) {
          gst_structure_fixate_field_nearest_int (out_st,
              "num-channels", g_list_length (src_caps_list));
          gst_caps_set_features_simple (out_caps,
              gst_tiovx_get_batched_memory_feature ());
        }

        #if defined(SOC_AM62A) || defined(SOC_J722S)

        if (NULL == g_strrstr (input_format, "i")) {
          const GValue *current_format = gst_structure_get_value (out_st, "format");

          if (current_format && GST_VALUE_HOLDS_LIST (current_format)) {
            g_value_init (&output_formats, GST_TYPE_LIST);
            g_value_init (&fmt, G_TYPE_STRING);

            g_value_set_string (&fmt, "NV12");
            gst_value_list_append_value (&output_formats, &fmt);
            g_value_reset (&fmt);

            g_value_set_string (&fmt, "GRAY8");
            gst_value_list_append_value (&output_formats, &fmt);

            gst_structure_set_value (out_st, "format", &output_formats);
            g_value_unset (&fmt);
            g_value_unset (&output_formats);

            GST_DEBUG_OBJECT (simo, "AM62A: Restricted format list to {NV12, GRAY8}");
          } else {
            GST_DEBUG_OBJECT (simo, "AM62A: Format already fixed, not overriding");
          }
        }
        #endif

        /* Final fixation */
        out_caps = gst_caps_fixate (out_caps);

        if (!out_caps || gst_caps_is_empty (out_caps)) {
          GST_ERROR_OBJECT (simo, "Failed to fixate caps");
          if (out_caps) {
            gst_caps_unref (out_caps);
          }
          goto error;
        }

        out_st = gst_caps_get_structure (out_caps, 0);
        chosen_out_fmt = gst_structure_get_string (out_st, "format");

        GST_INFO_OBJECT (simo,
            "FlexConnect fixation: input=%s (%dx%d) -> output=%s caps=%" GST_PTR_FORMAT,
            input_format, width, height,
            chosen_out_fmt ? chosen_out_fmt : "(null)",
            out_caps);

        result_caps_list = g_list_append (result_caps_list, out_caps);

    }

    return result_caps_list;

    error:
      for (l = result_caps_list; l != NULL; l = l->next) {
      gst_caps_unref ((GstCaps *) l->data);
      }
      g_list_free (result_caps_list);
    return NULL;
    }

static gboolean
gst_tiovx_fc_deinit_module (GstTIOVXSimo * simo)
{
  GstTIOVXFC *self = NULL;
  vx_status status = VX_FAILURE;
  gboolean ret = FALSE;
     
  g_return_val_if_fail (simo, FALSE);

  self = GST_TIOVX_FC (simo);
  
  /* Delete graph */
  status = tiovx_fc_module_delete (&self->fc_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module graph delete failed with error: %d", status);
    goto out;
  }
  
  /* Deinitialize module */
  status = tiovx_fc_module_deinit (&self->fc_obj);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Module deinit failed with error: %d", status);
    goto out;
  }

  ret = TRUE;
  
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

  type = gst_tiovx_fc_target_get_type ();
  enum_class = G_ENUM_CLASS (g_type_class_ref (type));
  enum_value = g_enum_get_value (enum_class, target_id);
  value_nick = enum_value->value_nick;
  g_type_class_unref (enum_class);

  return value_nick;
}

static gboolean
gst_tiovx_fc_postprocess (GstTIOVXSimo * simo)
{
    GstTIOVXFC *self = NULL;
    GList *sink_pad_p = NULL;
    GList *l = NULL;
    gboolean ret = FALSE;
    struct v4l2_control control;
    gchar *video_dev = NULL;
    gint i = 0;
    g_return_val_if_fail (simo, FALSE);
    self = GST_TIOVX_FC (simo);

    GST_LOG_OBJECT (self, "Entering postprocess");
    sink_pad_p = GST_ELEMENT (simo)->sinkpads;
    
    for (l = sink_pad_p, i = 0; l != NULL; l = g_list_next (l), i++)
    {
        GstTIOVXFCPad *sink_pad = (GstTIOVXFCPad *) l->data;

        if (g_strcmp0 (self->sensor_name, "SENSOR_SONY_IMX390_UB953_D3") == 0) {
            get_imx390_ae_dyn_params (&sink_pad->sensor_in_data.ae_dynPrms);
        } else if (g_strcmp0 (self->sensor_name, "SENSOR_OV2312_UB953_LI") == 0) {
            get_ov2312_ae_dyn_params (&sink_pad->sensor_in_data.ae_dynPrms);
        } else if (g_strcmp0 (self->sensor_name, "SENSOR_OX05B1S") == 0) {
            get_ox05b1s_ae_dyn_params (&sink_pad->sensor_in_data.ae_dynPrms);
        } else if (g_strcmp0 (self->sensor_name, "SENSOR_SONY_IMX728_UB971_D3") == 0) {
            get_imx728_ae_dyn_params (&sink_pad->sensor_in_data.ae_dynPrms);
        } else {
            get_imx219_ae_dyn_params (&sink_pad->sensor_in_data.ae_dynPrms);
        }

          video_dev = sink_pad->videodev;
          if (NULL == video_dev) {
            GST_LOG_OBJECT (sink_pad,
                "Device location was not provided, skipping IOCTL calls");
          }
          else
          {
         
          gint fd = -1;
          int ret_val = -1;
          gint32 analog_gain = 0;
          gint32 coarse_integration_time = 0;

          fd = open (video_dev, O_RDWR | O_NONBLOCK);
          if (-1 == fd)
            {
              GST_ERROR_OBJECT (self, "Unable to open video device: %s", video_dev);
              goto exit;
            }
          gst_tiovx_fc_map_2A_values (self,
              sink_pad->sensor_out_data.aePrms.exposureTime[0],
              sink_pad->sensor_out_data.aePrms.analogGain[0],
              &coarse_integration_time, &analog_gain);
         
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
  
    ret = TRUE;
    GST_ERROR_OBJECT (self, "Return status is: %d\n", ret);
 
    exit:
        return ret;

    }

    return TRUE;
}

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

  p_ae_dynPrms->exposureTimeRange[count].min = 10;
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


static gboolean
gst_tiovx_fc_init_module (GstTIOVXSimo * simo,
vx_context context, GstTIOVXPad * sink_pad, GList * src_pads, GstCaps * sink_caps,
GList * src_caps_list, guint num_channels)
{
    GstTIOVXFC *self = NULL;
    GList *l = NULL;
    GstVideoInfo in_info = { };
    GstVideoInfo out_info = { };
    gboolean ret = FALSE;
    vx_status status = VX_FAILURE;
    const gchar *format_str = NULL;
    GstStructure *sink_caps_st = NULL;
    TIOVXFCModuleObj * flexconnect = NULL;
    gboolean is_bayer_input = FALSE;

   
    g_return_val_if_fail (simo, FALSE);
    g_return_val_if_fail (context, FALSE);
    g_return_val_if_fail (sink_pad, FALSE);
    g_return_val_if_fail (src_pads, FALSE);
    g_return_val_if_fail (sink_caps, FALSE);
    g_return_val_if_fail (src_caps_list, FALSE);

    self = GST_TIOVX_FC (simo);
    flexconnect = &self->fc_obj;
    tiovx_querry_sensor (&self->sensor_obj);
    tiovx_init_sensor (&self->sensor_obj, self->sensor_name);
    
    self->sensor_obj.num_cameras_enabled = num_channels;
    self->sensor_obj.sensor_wdr_enabled = self->wdr_enabled;
    

    GST_DEBUG_OBJECT (self, "Getting sink caps");
    if (NULL == sink_caps) {
        GST_ERROR_OBJECT (self, "Failed to get sink caps");
        goto out;
    }
    
    GST_DEBUG_OBJECT (self, "sink_caps: %" GST_PTR_FORMAT, sink_caps);
    sink_caps_st = gst_caps_get_structure (sink_caps, 0);
    GST_DEBUG_OBJECT (self, "sink_caps_st: %p", sink_caps_st);
    
    /* Initialize the input parameters */
    GST_DEBUG_OBJECT (self, "Extracting video info from caps");
    if (!gst_video_info_from_caps (&in_info, sink_caps)) {
        GST_ERROR_OBJECT (self, "Failed to get info from input pad: %" GST_PTR_FORMAT,
            sink_caps);
        goto out;
    }
    
    /* Extract metadata heights from sink caps */
    GST_DEBUG_OBJECT (self, "Extracting metadata heights");
    gst_structure_get_int (sink_caps_st, "meta-height-before", &self->meta_height_before);
    gst_structure_get_int (sink_caps_st, "meta-height-after", &self->meta_height_after);
    
    /* Store total and image heights */
    GST_DEBUG_OBJECT (self, "Computing image dimensions");
    
    self->total_height = GST_VIDEO_INFO_HEIGHT (&in_info);
    GST_DEBUG_OBJECT (self, "total_height is: %d\n", self->total_height);
    
    self->image_height = self->total_height - self->meta_height_before - self->meta_height_after;
    GST_DEBUG_OBJECT (self, "image_height is: %d\n", self->image_height);
    
    self->num_channels = num_channels;
    GST_DEBUG_OBJECT (self, "num_channels are: %d\n", self->num_channels);
    
    GST_DEBUG_OBJECT (self, "Setting up VISS input parameters");
    flexconnect->viss_input.bufq_depth = 1;
    flexconnect->viss_input.params.num_exposures = self->num_exposures;
    flexconnect->viss_input.params.line_interleaved = self->line_interleaved;
    flexconnect->viss_input.params.format[0].msb = self->format_msb;
    flexconnect->viss_input.params.meta_height_before = self->meta_height_before;
    flexconnect->viss_input.params.meta_height_after = self->meta_height_after;
    flexconnect->viss_input.params.width = GST_VIDEO_INFO_WIDTH (&in_info);
    flexconnect->viss_input.params.height = GST_VIDEO_INFO_HEIGHT (&in_info)
        - self->meta_height_before
        - self->meta_height_after;
    
    GST_DEBUG_OBJECT (self, "Getting format string");
    format_str = gst_structure_get_string (sink_caps_st, "format");
    if (NULL == format_str) {
        GST_ERROR_OBJECT (self, "Format is missing in sink caps");
        goto out;
    }
    GST_DEBUG_OBJECT (self, "Format string: %s", format_str);
    
    GST_DEBUG_OBJECT (self, "Determining pixel container format");
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
        flexconnect->viss_input.params.format[0].pixel_container =
            TIVX_RAW_IMAGE_16_BIT;
        GST_DEBUG_OBJECT (self, "Setting 16-bit pixel container");
    } else if ((g_strcmp0 (format_str, "bggr") == 0)
            || (g_strcmp0 (format_str, "gbrg") == 0)
            || (g_strcmp0 (format_str, "grbg") == 0)
            || (g_strcmp0 (format_str, "rggb") == 0)
    ) {
        flexconnect->viss_input.params.format[0].pixel_container =
            TIVX_RAW_IMAGE_8_BIT;
        GST_DEBUG_OBJECT (self, "Setting 8-bit Bayer pixel container");
    } else if ((g_strcmp0 (format_str, "NV12") == 0) ||
            (g_strcmp0 (format_str, "NV12_P12") == 0)) {
        flexconnect->viss_input.params.format[0].pixel_container = TIVX_RAW_IMAGE_8_BIT;
        GST_DEBUG_OBJECT (self, "Setting 8-bit NV12/NV12_P12 pixel container");
    } else {
        GST_ERROR_OBJECT (self, "Couldn't determine pixel container from caps");
        goto out;
    }
    
    GST_DEBUG_OBJECT(self, "Setting up VISS-MSC input mapping");
    flexconnect->fc_params.msc_in_thread_viss_out_map[0] = TIVX_VPAC_FC_VISS_OUT2;
    flexconnect->fc_params.msc_in_thread_viss_out_map[1] = TIVX_VPAC_FC_MSC_CH_INVALID;
    flexconnect->fc_params.msc_in_thread_viss_out_map[2] = TIVX_VPAC_FC_MSC_CH_INVALID;
    flexconnect->fc_params.msc_in_thread_viss_out_map[3] = TIVX_VPAC_FC_MSC_CH_INVALID;
    
    GST_INFO_OBJECT (self,
        "Input parameters:\n"
        "\tWidth: %d\n"
        "\tHeight: %d\n"
        "\tPool size: %d\n"
        "\tNum exposures: %d\n"
        "\tLines interleaved: %d\n"
        "\tFormat pixel container: 0x%x\n"
        "\tFormat MSB: %d\n"
        "\tMeta height before: %d\n"
        "\tMeta height after: %d",
        flexconnect->viss_input.params.width,
        flexconnect->viss_input.params.height,
        flexconnect->viss_input.bufq_depth,
        flexconnect->viss_input.params.num_exposures,
        flexconnect->viss_input.params.line_interleaved,
        flexconnect->viss_input.params.format[0].pixel_container,
        flexconnect->viss_input.params.format[0].msb,
        flexconnect->viss_input.params.meta_height_before,
        flexconnect->viss_input.params.meta_height_after);
    
    /* Initialize tiovx params */
    GST_DEBUG_OBJECT (self, "Initializing tiovx params");
    tivx_vpac_fc_params_init(&flexconnect->fc_params);

    /* Initialize the output parameters */
    GST_DEBUG_OBJECT (self, "Initializing output parameters");
    for (l = src_caps_list; l != NULL; l = l->next) {
        GstCaps *src_caps = (GstCaps *) l->data;
        
        gint i = g_list_position (src_caps_list, l);
        
        GST_DEBUG_OBJECT (self, "Processing output %d, src_caps: %" GST_PTR_FORMAT, i, src_caps);
        
        if (!gst_video_info_from_caps (&out_info, src_caps)) {
            GST_ERROR_OBJECT (self, "Failed to get info from caps: %" GST_PTR_FORMAT, src_caps);
            ret = FALSE;
            goto out;
        }
        

        GST_DEBUG_OBJECT (self, "Setting up MSC output %d parameters", i);
        flexconnect->msc_output[i].width = GST_VIDEO_INFO_WIDTH (&out_info);
        flexconnect->msc_output[i].height = GST_VIDEO_INFO_HEIGHT (&out_info);
        flexconnect->msc_output[i].color_format = gst_format_to_vx_format (out_info.finfo->format);
        flexconnect->msc_output[i].bufq_depth = 1;
        flexconnect->msc_output[i].graph_parameter_index = i + 1;
        GST_DEBUG_OBJECT(self, "[FC-MODULE] MSC output %d reference: %p\n",i, (void *)&flexconnect->msc_output[i]);

        
        GST_INFO_OBJECT (self,
            "Output %d parameters: \n  Width: %d \n  Height: %d \n  Pool size: %d \n  Color format: 0x%x",
            i, 
            flexconnect->msc_output[i].width,
            flexconnect->msc_output[i].height,
            flexconnect->msc_output[i].bufq_depth,
            flexconnect->msc_output[i].color_format);
        GST_INFO_OBJECT (self, "[FC-GST] MSC output %d item: %p\n",i, (void *)&flexconnect->msc_output[i]);

    }
    
      
#if defined(SOC_AM62A) 
    GST_DEBUG_OBJECT (self, "Setting up AM62A/J722S specific parameters");
    
    flexconnect->msc_num_outputs = g_list_length(src_caps_list); 
    if (NULL == g_strrstr (format_str, "i")) {
        flexconnect->fc_params.tivxVissPrms.bypass_pcid = 1;
        GST_DEBUG_OBJECT (self, "Setting bypass_pcid=1 (non-i format)");
    } else {
        flexconnect->fc_params.tivxVissPrms.bypass_pcid = 0;
        GST_DEBUG_OBJECT (self, "Setting bypass_pcid=0 (i format)");
    }
    
    GST_DEBUG_OBJECT (self, "Configuring IR/Bayer operation based on output format");

    /* Detect if input is Bayer format for GRAY8 output decision */
    is_bayer_input = FALSE;
    if ((g_strrstr (format_str, "bggr") != NULL) ||
        (g_strrstr (format_str, "gbrg") != NULL) ||
        (g_strrstr (format_str, "grbg") != NULL) ||
        (g_strrstr (format_str, "rggb") != NULL)) {
        is_bayer_input = TRUE;
    }

    /* Map GStreamer video format to OpenVX format and configure VISS mode */
    switch (out_info.finfo->format) {
    case GST_VIDEO_FORMAT_NV12:
        GST_DEBUG_OBJECT (self, "Output format: NV12 (YUV 4:2:0) - Bayer mode");
        flexconnect->color_format = VX_DF_IMAGE_NV12;
        flexconnect->fc_params.tivxVissPrms.enable_ir_op = TIVX_VPAC_VISS_IR_DISABLE;
        flexconnect->fc_params.tivxVissPrms.enable_bayer_op = TIVX_VPAC_VISS_BAYER_ENABLE;
        break;

    case GST_VIDEO_FORMAT_GRAY8:
        flexconnect->color_format = VX_DF_IMAGE_U8;
        if (is_bayer_input) {
            /* Bayer sensor → GRAY8: Extract Y channel, keep Bayer processing */
            GST_DEBUG_OBJECT (self, "Output format: GRAY8 (U8) from Bayer input - Bayer mode (Y extract)");
            flexconnect->fc_params.tivxVissPrms.enable_ir_op = TIVX_VPAC_VISS_IR_DISABLE;
            flexconnect->fc_params.tivxVissPrms.enable_bayer_op = TIVX_VPAC_VISS_BAYER_ENABLE;
        } else {
            /* IR/Mono sensor → GRAY8: Direct passthrough */
            GST_DEBUG_OBJECT (self, "Output format: GRAY8 (U8) from IR/Mono input - IR mode");
            flexconnect->fc_params.tivxVissPrms.enable_ir_op = TIVX_VPAC_VISS_IR_ENABLE;
            flexconnect->fc_params.tivxVissPrms.enable_bayer_op = TIVX_VPAC_VISS_BAYER_DISABLE;
        }
        break;

    case GST_VIDEO_FORMAT_GRAY16_LE:
        GST_DEBUG_OBJECT (self, "Output format: GRAY16_LE (U16) - Bayer mode");
        flexconnect->color_format = VX_DF_IMAGE_U16;
        flexconnect->fc_params.tivxVissPrms.enable_ir_op = TIVX_VPAC_VISS_IR_DISABLE;
        flexconnect->fc_params.tivxVissPrms.enable_bayer_op = TIVX_VPAC_VISS_BAYER_ENABLE;
        break;

    case GST_VIDEO_FORMAT_UYVY:
        GST_DEBUG_OBJECT (self, "Output format: UYVY (YUV 4:2:2 packed) - Bayer mode");
        flexconnect->color_format = VX_DF_IMAGE_UYVY;
        flexconnect->fc_params.tivxVissPrms.enable_ir_op = TIVX_VPAC_VISS_IR_DISABLE;
        flexconnect->fc_params.tivxVissPrms.enable_bayer_op = TIVX_VPAC_VISS_BAYER_ENABLE;
        break;

    case GST_VIDEO_FORMAT_YUY2:
        GST_DEBUG_OBJECT (self, "Output format: YUYV (YUV 4:2:2 packed) - Bayer mode");
        flexconnect->color_format = VX_DF_IMAGE_YUYV;
        flexconnect->fc_params.tivxVissPrms.enable_ir_op = TIVX_VPAC_VISS_IR_DISABLE;
        flexconnect->fc_params.tivxVissPrms.enable_bayer_op = TIVX_VPAC_VISS_BAYER_ENABLE;
        break;

    default:
        GST_ERROR_OBJECT (self, "Unsupported output format: %s (0x%x)",
            gst_video_format_to_string (out_info.finfo->format),
            out_info.finfo->format);
        GST_ERROR_OBJECT (self, "Supported: NV12, GRAY8, GRAY16_LE, UYVY, YUYV");
        goto out;
    }

    GST_INFO_OBJECT (self, "Configured color_format=0x%x, IR=%d, Bayer=%d",
        flexconnect->color_format,
        flexconnect->fc_params.tivxVissPrms.enable_ir_op,
        flexconnect->fc_params.tivxVissPrms.enable_bayer_op);
    
    /* Apply processing settings */
    GST_DEBUG_OBJECT (self, "Setting processing parameters");
    GST_DEBUG_OBJECT (self, "bypass_cac: %d", self->bypass_cac);
    flexconnect->fc_params.tivxVissPrms.bypass_cac = self->bypass_cac;
    
    GST_DEBUG_OBJECT (self, "bypass_dwb: %d", self->bypass_dwb);
    flexconnect->fc_params.tivxVissPrms.bypass_dwb = self->bypass_dwb;
    
    GST_DEBUG_OBJECT (self, "bypass_nsf4: %d", self->bypass_nsf4);
    flexconnect->fc_params.tivxVissPrms.bypass_nsf4 = self->bypass_nsf4;
    
    GST_DEBUG_OBJECT (self, "ee_mode: %d", self->ee_mode);
    flexconnect->fc_params.tivxVissPrms.fcp[0].ee_mode = self->ee_mode;
    
    GST_DEBUG_OBJECT (self, "interpolation_method: %d", self->interpolation_method);
    flexconnect->interpolation_method = self->interpolation_method;
    
    flexconnect->msc_num_outputs = g_list_length (src_caps_list);
    GST_DEBUG_OBJECT (self, "Setting msc_num_outputs to %d", flexconnect->msc_num_outputs);
    
    GST_DEBUG_OBJECT (self, "enable_ir_op: %d", flexconnect->fc_params.tivxVissPrms.enable_ir_op);
    if (flexconnect->fc_params.tivxVissPrms.enable_ir_op) {
        /* For IR operation */
        GST_DEBUG_OBJECT (self, "Configuring for IR operation");
        
        /* Configure MSC outputs - route IR output to MSC output 0 */
        GST_DEBUG_OBJECT (self, "Configuring MSC outputs for IR");
        for (int i = 6; i <= 10; i++) {
            flexconnect->msc_output_select[i] = TIOVX_FC_MODULE_OUTPUT_EN;
            GST_DEBUG_OBJECT (self, "Number of outputs enabled for IR operation: %d", i);

            /* Configure MSC output 0 */
        GST_DEBUG_OBJECT (self, "Setting MSC output parameters for IR");
        flexconnect->msc_output[i].width = GST_VIDEO_INFO_WIDTH(&out_info);
        flexconnect->msc_output[i].height = GST_VIDEO_INFO_HEIGHT(&out_info);
        flexconnect->msc_output[i].color_format = gst_format_to_vx_format(out_info.finfo->format);
        flexconnect->msc_output[i].bufq_depth = 1;
        
        GST_DEBUG_OBJECT (self, "MSC output 0 configured: width=%d, height=%d, format=0x%x, enabled=%d",
                 flexconnect->msc_output[i].width,
                 flexconnect->msc_output[i].height,
                 flexconnect->msc_output[i].color_format,
                 flexconnect->msc_output_select[i]);
                 
        GST_INFO_OBJECT(self, 
            "FlexConnect IR output parameters:\n"
            "\tWidth: %d\n"
            "\tHeight: %d\n", 
            flexconnect->msc_output[i].width, 
            flexconnect->msc_output[i].height);

        }
        
        
    } else if (flexconnect->fc_params.tivxVissPrms.enable_bayer_op) 
#endif
    {
        GST_DEBUG_OBJECT (self, "Configuring for Bayer/Color operation");
        
        GST_DEBUG_OBJECT (self, "Setting MSC output selections");
        flexconnect->msc_output_select[0] = TIOVX_FC_MODULE_OUTPUT_EN;
        GST_DEBUG_OBJECT (self, "msc_output_select 0 is %d\n", flexconnect->msc_output_select[0]);
       
        for (l = src_caps_list; l != NULL; l = l->next) {
          GstCaps *src_caps = (GstCaps *) l->data;
          GstVideoInfo out_info = { };
          gint i = g_list_position (src_caps_list, l);
        
          if (!gst_video_info_from_caps (&out_info, src_caps)) {
          GST_ERROR_OBJECT (self, "Failed to get info from caps: %"
              GST_PTR_FORMAT, src_caps);
          ret = FALSE;
          goto out;
          }

          /* Configure MSC output */
          GST_DEBUG_OBJECT (self, "Setting MSC output parameters for Bayer");
            flexconnect->msc_output[i].width = GST_VIDEO_INFO_WIDTH(&out_info);
            flexconnect->msc_output[i].height = GST_VIDEO_INFO_HEIGHT(&out_info);
            flexconnect->msc_output[i].color_format = gst_format_to_vx_format(out_info.finfo->format);
            flexconnect->msc_output[i].bufq_depth = 1;
            
            GST_INFO_OBJECT(self, 
                "FlexConnect Bayer output parameters:\n"
                "\tWidth: %d\n"
                "\tHeight: %d\n"
                "\tColor format: 0x%x", 
                flexconnect->msc_output[i].width, 
                flexconnect->msc_output[i].height,
                flexconnect->msc_output[i].color_format);
        }
    }  
    
    /* Set interpolation method */
    GST_DEBUG_OBJECT (self, "Setting interpolation method");
    GST_OBJECT_LOCK (GST_OBJECT (self));
    flexconnect->interpolation_method = self->interpolation_method;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    GST_DEBUG_OBJECT (self, "interpolation_method set to %d", flexconnect->interpolation_method);
    
    /* Initialize the FlexConnect module */
    GST_INFO_OBJECT (self, "Initializing FlexConnect module");
    GST_DEBUG_OBJECT (self, "context: %p, flexconnect: %p, sensor_obj: %p", 
                     context, flexconnect, &self->sensor_obj);
                     
    GST_DEBUG_OBJECT (self, "Checking if sensor_obj is properly initialized");
    GST_DEBUG_OBJECT (self, "sensor_name: %s", self->sensor_obj.sensor_name);
    GST_DEBUG_OBJECT (self, "num_cameras_enabled: %d", self->sensor_obj.num_cameras_enabled);
    GST_DEBUG_OBJECT (self, "sensor_wdr_enabled: %d", self->sensor_obj.sensor_wdr_enabled);
    
    /* Check raw_params is initialized */
    GST_DEBUG_OBJECT (self, "Checking if raw_params is initialized");
    GST_DEBUG_OBJECT (self, "raw_params width: %d, height: %d", 
                     flexconnect->raw_params.width, 
                     flexconnect->raw_params.height);
    
    /* About to call the module init function */
    GST_DEBUG_OBJECT (self, "Calling tiovx_fc_module_init()");
    for (int i = 1; i < 6; i++){
    GST_DEBUG_OBJECT (self, "msc_output_select before tiovx_fc_module_init is %d", flexconnect->msc_output_select[i]);
    }
    status = tiovx_fc_module_init (context, flexconnect, &self->sensor_obj);
    
    GST_DEBUG_OBJECT (self, "tiovx_fc_module_init() returned status: %d", status);
    if (VX_SUCCESS != status) {
        GST_ERROR_OBJECT (self, "Module init failed with error: %d", status);
        goto out;
    }
    
    GST_DEBUG_OBJECT (self, "Module init succeeded");
    ret = TRUE;
    
out:    
    return ret;
}

static gboolean
gst_tiovx_fc_get_node_info (GstTIOVXSimo * simo, vx_node * node,
    GstTIOVXPad * sink_pad, GList * src_pads, GList ** queueable_objects)
{
  GstTIOVXFC *self = NULL;
  GList *l = NULL;
 
  g_return_val_if_fail (simo, FALSE);
  g_return_val_if_fail (sink_pad, FALSE);
  g_return_val_if_fail (src_pads, FALSE);
  
  self = GST_TIOVX_FC (simo);
  GST_DEBUG_OBJECT (simo, "Cast to GstTIOVXFC successful");
  
  *node = self->fc_obj.node;
  GST_DEBUG_OBJECT (simo, "Node assigned: %p", *node);
  
  /* Set input parameters */
  GST_DEBUG_OBJECT (simo, "Setting sink pad parameters");
  gst_tiovx_pad_set_params (sink_pad,
      self->fc_obj.viss_input.arr[0], 
      (vx_reference) self->fc_obj.viss_input.image_handle[0],
      self->fc_obj.viss_input.graph_parameter_index, input_param_id);
  GST_DEBUG_OBJECT (simo, "Sink pad parameters set with graph_parameter_index: %d", 
      self->fc_obj.viss_input.graph_parameter_index);
  
  GST_DEBUG_OBJECT (simo, "Processing %d output pads", g_list_length(src_pads));
  for (l = src_pads; l != NULL; l = l->next) {
    GstTIOVXPad *src_pad = (GstTIOVXPad *) l->data;
    gint i = g_list_position (src_pads, l);
    
    /* Set output parameters */
    GST_DEBUG_OBJECT (simo, "Setting src pad %d parameters", i);
    gst_tiovx_pad_set_params (src_pad,
        self->fc_obj.msc_output[i].arr[0],
        (vx_reference) self->fc_obj.msc_output[i].image_handle[0],
        self->fc_obj.msc_output[i].graph_parameter_index, output0_param_id + i);
    GST_DEBUG_OBJECT (simo, "Src pad %d parameters set with graph_parameter_index: %d, param_id: %d", 
        i, self->fc_obj.msc_output[i].graph_parameter_index, output0_param_id + i);
  }
  
 
  return TRUE;
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
get_imx728_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms)
{
  int32_t status = -1;
  uint8_t count = 0;

  g_return_val_if_fail (p_ae_dynPrms, status);

  p_ae_dynPrms->targetBrightnessRange.min = 40;
  p_ae_dynPrms->targetBrightnessRange.max = 50;
  p_ae_dynPrms->targetBrightness = 45;
  p_ae_dynPrms->threshold = 5;
  p_ae_dynPrms->enableBlc = 0;

  p_ae_dynPrms->exposureTimeStepSize         = 1;
  p_ae_dynPrms->exposureTimeRange[count].min = 10000;  // us
  p_ae_dynPrms->exposureTimeRange[count].max = 10000;  // us
  p_ae_dynPrms->analogGainRange[count].min   = 1024; 
  p_ae_dynPrms->analogGainRange[count].max   = 16229;
  p_ae_dynPrms->digitalGainRange[count].min  = 256;
  p_ae_dynPrms->digitalGainRange[count].max  = 256;
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
  p_ae_dynPrms->analogGainRange[count].min = 16;
  p_ae_dynPrms->analogGainRange[count].max = 16;
  p_ae_dynPrms->digitalGainRange[count].min = 1024;
  p_ae_dynPrms->digitalGainRange[count].max = 1024;
  count++;

  p_ae_dynPrms->exposureTimeRange[count].min = 14450;
  p_ae_dynPrms->exposureTimeRange[count].max = 14450;
  p_ae_dynPrms->analogGainRange[count].min = 16;
  p_ae_dynPrms->analogGainRange[count].max = 42;
  p_ae_dynPrms->digitalGainRange[count].min = 1024;
  p_ae_dynPrms->digitalGainRange[count].max = 1024;
  count++;

  p_ae_dynPrms->numAeDynParams = count;
  status = 0;
  return status;
}

static int32_t
get_ox05b1s_ae_dyn_params (IssAeDynamicParams * p_ae_dynPrms)
{
  int32_t status = -1;
  uint8_t count = 0;

  g_return_val_if_fail (p_ae_dynPrms, status);

  p_ae_dynPrms->targetBrightnessRange.min = 40; /* lower bound of the target brightness range */
  p_ae_dynPrms->targetBrightnessRange.max = 50; /* upper bound of the target brightness range */
  p_ae_dynPrms->targetBrightness = 45;          /* target brightness */
  p_ae_dynPrms->threshold = 5;                  /* maximum change above or below the target brightness */
  p_ae_dynPrms->enableBlc = 0;                  /* not used */

  /* setting exposure and gains */
  p_ae_dynPrms->exposureTimeStepSize = 1;       /* step size of automatic adjustment for exposure time */
  p_ae_dynPrms->exposureTimeRange[count].min = 47;     /* 6*16.67/2128*1000 micro sec */
  p_ae_dynPrms->exposureTimeRange[count].max = 16435;  /* (2128-30)*16.67/2128*1000 micro sec */
  p_ae_dynPrms->analogGainRange[count].min = 1024;     /* 1x gain - 16*64 */
  p_ae_dynPrms->analogGainRange[count].max = 15872;    /* 15.5x gain - 16*15.5*64 = 328*64 = 15872 */
  p_ae_dynPrms->digitalGainRange[count].min = 256;     /* digital gain not used */
  p_ae_dynPrms->digitalGainRange[count].max = 256;     /* digital gain not used */

  count++;

  p_ae_dynPrms->numAeDynParams = count;
  status = 0;
  return status;
}



static void
gst_tiovx_fc_map_2A_values (GstTIOVXFC * self, int exposure_time,
    int analog_gain, gint32 * exposure_time_mapped, gint32 * analog_gain_mapped)
{
  double multiplier = 0;

  g_return_if_fail (self);
  g_return_if_fail (exposure_time_mapped);
  g_return_if_fail (analog_gain_mapped);

  if (g_strcmp0 (self->sensor_name, "SENSOR_SONY_IMX390_UB953_D3") == 0) {
    gint i = 0;
    for (i = 0; i < ISS_IMX390_GAIN_TBL_SIZE - 1; i++) {
      if (gIMX390GainsTable[i][0] >= analog_gain) {
        break;
      }
    }
    *exposure_time_mapped = exposure_time;
    *analog_gain_mapped = gIMX390GainsTable[i][1];

  } else if (g_strcmp0 (self->sensor_name, "SENSOR_SONY_IMX728_UB971_D3") == 0) {
    gint i = 0;
    for (i = 0; i < ISS_IMX728_GAIN_TBL_SIZE - 1; i++) {
      if (gIMX728GainsTable[i][0] >= analog_gain) {
        break;
      }
    }
    *exposure_time_mapped = exposure_time;
    *analog_gain_mapped = gIMX728GainsTable[i][1];
  } else if (g_strcmp0 (self->sensor_name, "SENSOR_SONY_IMX219_RPI") == 0) {
    *exposure_time_mapped = (1080 * exposure_time / 33333);  /* for 1920x1080 at 30fps */
    //*exposure_time_mapped = (2464 * exposure_time / 66666);  /* for 3280x2464 at 15fps */

    /* convert gain to the format assumed by the sensor - refer to sensor data sheet */ 
    multiplier = analog_gain / 1024.0;  // 1024 is 1x gain */
    *analog_gain_mapped = 256.0 - 256.0 / multiplier;
  } else if (g_strcmp0 (self->sensor_name, "SENSOR_OV2312_UB953_LI") == 0) {
    *exposure_time_mapped = (60 * 1300 * exposure_time / 1000000);
    // ms to row_time conversion - row_time(us) = 1000000/fps/height
    *analog_gain_mapped = analog_gain;
} else if (g_strcmp0 (self->sensor_name, "SENSOR_OX05B1S") == 0) {
    *exposure_time_mapped = (int) ((double)exposure_time * 2128 * 60 / 1000000 + 0.5);
    *analog_gain_mapped = analog_gain / 64;
  } else {
    GST_ERROR_OBJECT (self, "Unknown sensor: %s", self->sensor_name);
  }
}



static void
gst_tiovx_fc_finalize (GObject * obj)
{
  GstTIOVXFC *self = GST_TIOVX_FC (obj);
  gint i = 0;
 

  g_free (self->dcc_fc_config_file);
  self->dcc_fc_config_file = NULL;
  g_free (self->sensor_name);
  self->sensor_name = NULL;
 
  if (NULL != self->aewb_memory) {
    gst_memory_unref (self->aewb_memory);
    GST_DEBUG_OBJECT (self, "aewb memory status: %p ", self->aewb_memory);
  }
  if (NULL != self->h3a_stats_memory) {
    gst_memory_unref (self->h3a_stats_memory);
  }
  
  if (self->user_data_allocator) {
    g_object_unref (self->user_data_allocator);
    GST_DEBUG_OBJECT (self, "user data allocator is : %p ", self->user_data_allocator);

  }
 
  for (i = 0; i < MAX_NUM_CHANNELS; i++) {
    if (self->input_references[i]) {
      vxReleaseReference (&self->input_references[i]);
      self->input_references[i] = NULL;
    }
  }
 
  G_OBJECT_CLASS (gst_tiovx_fc_parent_class)->finalize (obj);
 
}

static void
gst_tiovx_fc_set_property(GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIOVXFC *self = GST_TIOVX_FC (object);


  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_ISP_CONFIG_FILE:
      g_free (self->dcc_fc_config_file);
      self->dcc_fc_config_file = g_value_dup_string (value);
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
    case PROP_WDR_ENABLED:
      self->wdr_enabled = g_value_get_boolean (value);
      break;
    case PROP_BYPASS_CAC:
      self->bypass_cac = g_value_get_boolean (value);
      break;
    case PROP_BYPASS_DWB:
      self->bypass_dwb = g_value_get_boolean (value);
      break;
    case PROP_BYPASS_NSF4:
      self->bypass_nsf4 = g_value_get_boolean (value);
      break;
    case PROP_EE_MODE:
      self->ee_mode = g_value_get_enum (value);
      break;
    case PROP_INTERPOLATION_METHOD:
      self->interpolation_method = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_tiovx_fc_get_property(GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIOVXFC *self = GST_TIOVX_FC (object);


  GST_OBJECT_LOCK (self);
  switch (prop_id) {
    case PROP_DCC_ISP_CONFIG_FILE:
      g_value_set_string (value, self->dcc_fc_config_file);
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
    case PROP_WDR_ENABLED:
      g_value_set_boolean (value, self->wdr_enabled);
      break;
    case PROP_BYPASS_CAC:
      g_value_set_boolean (value, self->bypass_cac);
      break;
    case PROP_BYPASS_DWB:
      g_value_set_boolean (value, self->bypass_dwb);
      break;
    case PROP_BYPASS_NSF4:
      g_value_set_boolean (value, self->bypass_nsf4);
      break;
    case PROP_EE_MODE:
      g_value_set_enum (value, self->ee_mode);
      break;
    case PROP_INTERPOLATION_METHOD:
      g_value_set_enum (value, self->interpolation_method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}
