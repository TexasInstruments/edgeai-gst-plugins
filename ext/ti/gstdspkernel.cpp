/*
 * Copyright (c) [2026] Texas Instruments Incorporated
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

#include "gstdspkernel.h"
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <cstdio>

extern "C"
{
#include "rpmsg.h"
#include "dmabuf.h"
}

GST_DEBUG_CATEGORY_STATIC (gst_dsp_kernel_debug_category);
#define GST_CAT_DEFAULT gst_dsp_kernel_debug_category

/* Auto-detection threshold: models with window_frames > this use overlap-save chunking */
#define CHUNKING_THRESHOLD 256

/* DSP message structure */
struct c7x_msg_hdr
{
  uint32_t type;
  uint32_t seq;
  uint32_t len;
  int32_t status;
} __attribute__((packed));

/* Wire format for STFT/ISTFT requests: 5 fixed params after the header. */
struct stft_istft_msg
{
  struct c7x_msg_hdr hdr;
  uint32_t selected_model;      /* param0: firmware ModelId */
  uint32_t input_buffer;        /* param1: Physical address of input DMA buffer */
  uint32_t output_buffer;       /* param2: Physical address of output DMA buffer */
  uint32_t input_frame;         /* param3: Number of frames to process */
  uint32_t output_frame;        /* param4: Always equals input_frame - STFT/ISTFT
                                 * are frame-synchronous regardless of model */
} __attribute__((packed));

struct deint_interleave_msg
{
  struct c7x_msg_hdr hdr;
  uint32_t input_buffer;        /* Physical address of input DMA buffer */
  uint32_t output_buffer;       /* Physical address of output DMA buffer */
  uint32_t input_frame;         /* Number of time frames */
  uint32_t fft_size;            /* FFT size */
  uint32_t flag;                /* 0=deinterleave, 1=interleave */
} __attribute__((packed));

#define C7X_STATUS_SUCCESS 0

enum
{
  PROP_0,
  PROP_MSG_TYPE,
  PROP_INTERLEAVE_DIRECTION,
  PROP_HOP_SIZE,
  PROP_FFT_SIZE,
  PROP_WINDOW_FRAMES,
  PROP_BATCH_SIZE,
  PROP_SELECTED_MODEL,
  PROP_MODEL_ELEMS,
  PROP_MODEL_PATH,
  PROP_OVERLAP_FRAMES,
  PROP_CHUNKING_MODE,
};

/* RPMsg/DMA infrastructure defaults */
#define DEFAULT_RPROC_DEVICE    "/dev/remoteproc0"
#define DEFAULT_RPROC_ID        8
#define DEFAULT_REMOTE_EP       13
#define DEFAULT_MSG_TYPE        0
#define DEFAULT_INTERLEAVE_DIRECTION          0

/* STFT/ISTFT/Deinterleave parameter defaults (0 = must be set explicitly) */
#define DEFAULT_HOP_SIZE        0
#define DEFAULT_FFT_SIZE        0
#define DEFAULT_WINDOW_FRAMES   0
#define DEFAULT_BATCH_SIZE      0

/* Model identification defaults */
#define DEFAULT_SELECTED_MODEL  2
#define DEFAULT_MODEL_ELEMS     0       /* 0 = derive from fft-size */
#define DEFAULT_MODEL_PATH      ""

/* Overlap-save chunking defaults */
#define DEFAULT_OVERLAP_FRAMES  100     /* GCRN's overlap-save overlap amount */
#define DEFAULT_CHUNKING_MODE   (-1)    /* auto: derive from CHUNKING_THRESHOLD */

/* Known model names (case-insensitive substring of model-path) mapped to
 * firmware ModelId + spectral elements per frame. */
struct DspKernelModelInfo
{
  const gchar *name;
  guint selected_model;
  guint model_elems;
};

static const struct DspKernelModelInfo dsp_kernel_known_models[] = {
  {"dccrn", 0, 514},
  {"gtcrn", 1, 514},
  {"gcrn", 2, 322},
  {"vggish", 3, 64},
  {"yamnet", 4, 64},
};

static gboolean
gst_dsp_kernel_detect_model_from_path (const gchar * model_path,
    guint * selected_model, guint * model_elems)
{
  gchar *path_lower;
  guint i;
  gboolean found = FALSE;

  if (!model_path || model_path[0] == '\0')
    return FALSE;

  path_lower = g_ascii_strdown (model_path, -1);

  for (i = 0; i < G_N_ELEMENTS (dsp_kernel_known_models); i++) {
    if (g_strrstr (path_lower, dsp_kernel_known_models[i].name)) {
      *selected_model = dsp_kernel_known_models[i].selected_model;
      *model_elems = dsp_kernel_known_models[i].model_elems;
      found = TRUE;
      break;
    }
  }

  g_free (path_lower);
  return found;
}

static guint
gst_dsp_kernel_get_model_elems (GstDspKernel * kernel)
{
  if (kernel->model_elems > 0)
    return kernel->model_elems;
  return (kernel->fft_size / 2 + 1) * 2;
}

/* Function prototypes */
static void gst_dsp_kernel_auto_detect_operation (GstDspKernel * kernel);
static gboolean gst_dsp_kernel_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static void gst_dsp_kernel_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dsp_kernel_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_dsp_kernel_finalize (GObject * object);
static gboolean gst_dsp_kernel_start (GstBaseTransform * trans);
static gboolean gst_dsp_kernel_stop (GstBaseTransform * trans);
static GstFlowReturn gst_dsp_kernel_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static GstCaps *gst_dsp_kernel_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

/* STFT: audio/x-raw,S16LE → application/octet-stream
 * ISTFT: application/octet-stream → audio/x-raw,F32LE
 * Deint/Interleave: application/octet-stream ↔ application/octet-stream */
static GstStaticPadTemplate dsp_kernel_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format=(string){S16LE,F32LE}, "
        "rate=(int)[1,2147483647], channels=(int)[1,2147483647]; "
        "application/octet-stream"));
static GstStaticPadTemplate dsp_kernel_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, format=(string){S16LE,F32LE}, "
        "rate=(int)[1,2147483647], channels=(int)[1,2147483647]; "
        "application/octet-stream"));

#define gst_dsp_kernel_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDspKernel, gst_dsp_kernel, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_dsp_kernel_debug_category, "tidspkernel", 0,
        "TI DSP Kernel Transform"));

static void
gst_dsp_kernel_class_init (GstDspKernelClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *bt = GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&dsp_kernel_sink_template));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&dsp_kernel_src_template));
  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "TI DSP Kernel Transform", "Transform/DSP",
      "Generic DSP kernel offload (STFT/ISTFT/Deinterleave/Interleave) via RPMsg-DMA",
      "Pratham Deshmukh <p-deshmukh@ti.com>");

  gobject_class->set_property = gst_dsp_kernel_set_property;
  gobject_class->get_property = gst_dsp_kernel_get_property;
  gobject_class->finalize = gst_dsp_kernel_finalize;

  bt->start = GST_DEBUG_FUNCPTR (gst_dsp_kernel_start);
  bt->stop = GST_DEBUG_FUNCPTR (gst_dsp_kernel_stop);
  bt->transform_ip = GST_DEBUG_FUNCPTR (gst_dsp_kernel_transform_ip);
  bt->transform_caps = GST_DEBUG_FUNCPTR (gst_dsp_kernel_transform_caps);
  bt->sink_event = GST_DEBUG_FUNCPTR (gst_dsp_kernel_sink_event);
  bt->passthrough_on_same_caps = FALSE;

  /* Install user-facing properties */
  g_object_class_install_property (gobject_class, PROP_MSG_TYPE,
      g_param_spec_uint ("msg-type", "DSP Message Type",
          "IPC message type (0x1020=STFT, 0x1030=ISTFT, 0x1040=De/Interleave)",
          0, G_MAXUINT, DEFAULT_MSG_TYPE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INTERLEAVE_DIRECTION,
      g_param_spec_uint ("interleave-direction", "Interleave Direction",
          "For msg-type=0x1040 (De/Interleave): 0=deinterleave, 1=interleave",
          0, G_MAXUINT, DEFAULT_INTERLEAVE_DIRECTION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_HOP_SIZE,
      g_param_spec_uint ("hop-size", "Hop Size",
          "Hop size between frames (for STFT/ISTFT) - REQUIRED, must be > 0",
          0, 8192, DEFAULT_HOP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FFT_SIZE,
      g_param_spec_uint ("fft-size", "FFT Size",
          "FFT size in samples (for STFT/ISTFT) - REQUIRED, must be > 0",
          0, 8192, DEFAULT_FFT_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_WINDOW_FRAMES,
      g_param_spec_uint ("window-frames", "Window Frames",
          "Total frames to accumulate (for STFT/ISTFT) - REQUIRED, must be > 0",
          0, 8192, DEFAULT_WINDOW_FRAMES,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BATCH_SIZE,
      g_param_spec_uint ("batch-size", "Batch Size",
          "Frames per batch (for STFT/ISTFT) - optional, 0 = process all frames",
          0, 8192, DEFAULT_BATCH_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SELECTED_MODEL,
      g_param_spec_uint ("selected-model", "Selected Model",
          "Firmware ModelId sent in STFT/ISTFT requests (0=DCCRN, 1=GTCRN, "
          "2=GCRN, 3=VGGISH, 4=YAMNET).",
          0, G_MAXUINT, DEFAULT_SELECTED_MODEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MODEL_ELEMS,
      g_param_spec_uint ("model-elems", "Model Elements",
          "Spectral elements per frame; 0 = derive from fft-size using the "
          "GCRN formula (fft-size/2+1)*2, non-zero = used as-is (e.g. 64 for YAMNet)",
          0, G_MAXUINT, DEFAULT_MODEL_ELEMS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MODEL_PATH,
      g_param_spec_string ("model-path", "Model Path",
          "TVM artifacts directory. ",
          DEFAULT_MODEL_PATH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_OVERLAP_FRAMES,
      g_param_spec_uint ("overlap-frames", "Overlap Frames",
          "Overlap-save overlap amount in frames, used only when overlap-save "
          "chunking is active. Default (100) matches GCRN.",
          0, 8192, DEFAULT_OVERLAP_FRAMES,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_CHUNKING_MODE,
      g_param_spec_int ("chunking-mode", "Chunking Mode",
          "-1 = auto-detect from window-frames vs internal threshold "
          "(default), 0 = force plain windowing, 1 = force overlap-save.",
          -1, 1, DEFAULT_CHUNKING_MODE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static gboolean
gst_dsp_kernel_calculate_buffer_sizes (GstDspKernel * kernel)
{
  if (kernel->model_path && kernel->model_path[0] != '\0') {
    if (gst_dsp_kernel_detect_model_from_path (kernel->model_path,
            &kernel->selected_model, &kernel->model_elems)) {
      GST_INFO_OBJECT (kernel,
          "Detected model from model-path '%s': selected-model=%u, "
          "model-elems=%u", kernel->model_path, kernel->selected_model,
          kernel->model_elems);
    } else {
      GST_WARNING_OBJECT (kernel,
          "model-path '%s' does not match any known model "
          "(dccrn/gtcrn/gcrn/vggish/yamnet) - using selected-model=%u "
          "model-elems=%u from properties instead of model-path "
          "auto-detection", kernel->model_path, kernel->selected_model,
          kernel->model_elems);
    }
  }

  if (kernel->model_elems == 0 && kernel->fft_size == 0) {
    GST_WARNING_OBJECT (kernel, "fft_size is 0, cannot calculate buffer sizes");
    return TRUE;
  }

  guint model_elems = gst_dsp_kernel_get_model_elems (kernel);

  GST_INFO_OBJECT (kernel, "Auto-calculating buffer sizes: model_elems=%u (%s)",
      model_elems, kernel->model_elems > 0 ? "explicit" : "from fft-size");

  switch (kernel->msg_type) {
    case DSP_OP_STFT:
      kernel->input_buf_size =
          kernel->window_frames * kernel->hop_size * sizeof (int16_t);
      kernel->output_buf_size =
          kernel->window_frames * model_elems * sizeof (float);
      GST_INFO_OBJECT (kernel,
          "Auto-calculated STFT buffer sizes: input=%u bytes "
          "(window_frames=%u * hop_size=%u * 2), output=%u bytes "
          "(window_frames=%u * model_elems=%u * 4)", kernel->input_buf_size,
          kernel->window_frames, kernel->hop_size, kernel->output_buf_size,
          kernel->window_frames, model_elems);
      break;

    case DSP_OP_ISTFT:
      kernel->input_buf_size =
          kernel->window_frames * model_elems * sizeof (float);
      kernel->output_buf_size =
          kernel->window_frames * kernel->hop_size * sizeof (int16_t);
      GST_INFO_OBJECT (kernel,
          "Auto-calculated ISTFT buffer sizes: input=%u bytes "
          "(window_frames=%u * model_elems=%u * 4), output=%u bytes "
          "(window_frames=%u * hop_size=%u * 2)", kernel->input_buf_size,
          kernel->window_frames, model_elems, kernel->output_buf_size,
          kernel->window_frames, kernel->hop_size);
      break;

    case DSP_OP_DEINT_INTERLEAVE:
      kernel->input_buf_size =
          kernel->window_frames * model_elems * sizeof (float);
      kernel->output_buf_size =
          kernel->window_frames * model_elems * sizeof (float);
      GST_INFO_OBJECT (kernel,
          "Auto-calculated Deinterleave/Interleave buffer sizes: "
          "input=output=%u bytes (window_frames=%u * model_elems=%u * 4)",
          kernel->input_buf_size, kernel->window_frames, model_elems);
      break;

    default:
      GST_WARNING_OBJECT (kernel,
          "Unknown operation type 0x%04x, buffer sizes must be provided",
          kernel->msg_type);
      break;
  }

  return TRUE;
}

static void
gst_dsp_kernel_auto_detect_operation (GstDspKernel * kernel)
{
  /* Only auto-detect if msg_type is not already set */
  if (kernel->msg_type != 0) {
    return;
  }

  const gchar *name = GST_ELEMENT_NAME (kernel);
  GST_INFO_OBJECT (kernel, "Auto-detecting operation from element name: %s",
      name);

  if (g_str_has_suffix (name, "istft") || g_strcmp0 (name, "istft") == 0) {
    kernel->msg_type = DSP_OP_ISTFT;
    GST_INFO_OBJECT (kernel, "Auto-detected: ISTFT (0x%04x)", kernel->msg_type);
  } else if (g_str_has_suffix (name, "stft") || g_strcmp0 (name, "stft") == 0) {
    kernel->msg_type = DSP_OP_STFT;
    GST_INFO_OBJECT (kernel, "Auto-detected: STFT (0x%04x)", kernel->msg_type);
  } else if (g_str_has_suffix (name, "deinterleave")
      || g_strcmp0 (name, "deinterleave") == 0) {
    kernel->msg_type = DSP_OP_DEINT_INTERLEAVE;
    kernel->interleave_direction = 0;   /* 0 = deinterleave */
    GST_INFO_OBJECT (kernel, "Auto-detected: Deinterleave (0x%04x, flag=0)",
        kernel->msg_type);
  } else if (g_str_has_suffix (name, "interleave")
      || g_strcmp0 (name, "interleave") == 0) {
    kernel->msg_type = DSP_OP_DEINT_INTERLEAVE;
    kernel->interleave_direction = 1;   /* 1 = interleave */
    GST_INFO_OBJECT (kernel, "Auto-detected: Interleave (0x%04x, flag=1)",
        kernel->msg_type);
  } else {
    GST_WARNING_OBJECT (kernel,
        "Could not auto-detect operation from name '%s'", name);
  }

  /* Auto-derive response type if detected */
  if (kernel->msg_resp_type == 0 && kernel->msg_type != 0) {
    kernel->msg_resp_type = (kernel->msg_type & 0x0FFF) | 0x2000;
    GST_INFO_OBJECT (kernel, "Auto-derived msg-resp-type: 0x%04x",
        kernel->msg_resp_type);
  }
}

static void
gst_dsp_kernel_init (GstDspKernel * kernel)
{
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (kernel), TRUE);

  kernel->rproc_device = g_strdup (DEFAULT_RPROC_DEVICE);
  kernel->rproc_id = DEFAULT_RPROC_ID;
  kernel->remote_ep = DEFAULT_REMOTE_EP;
  kernel->selected_model = DEFAULT_SELECTED_MODEL;
  kernel->model_path = g_strdup (DEFAULT_MODEL_PATH);
  kernel->overlap_frames_prop = DEFAULT_OVERLAP_FRAMES;
  kernel->chunking_mode = DEFAULT_CHUNKING_MODE;
  kernel->sequence_number = 1;
}

static void
gst_dsp_kernel_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (object);
  GST_OBJECT_LOCK (kernel);
  switch (prop_id) {
    case PROP_MSG_TYPE:
      kernel->msg_type = g_value_get_uint (value);
      break;
    case PROP_INTERLEAVE_DIRECTION:
      kernel->interleave_direction = g_value_get_uint (value);
      break;
    case PROP_HOP_SIZE:
      kernel->hop_size = g_value_get_uint (value);
      break;
    case PROP_FFT_SIZE:
      kernel->fft_size = g_value_get_uint (value);
      break;
    case PROP_WINDOW_FRAMES:
      kernel->window_frames = g_value_get_uint (value);
      break;
    case PROP_BATCH_SIZE:
      kernel->batch_size = g_value_get_uint (value);
      break;
    case PROP_SELECTED_MODEL:
      kernel->selected_model = g_value_get_uint (value);
      break;
    case PROP_MODEL_ELEMS:
      kernel->model_elems = g_value_get_uint (value);
      break;
    case PROP_MODEL_PATH:
      g_free (kernel->model_path);
      kernel->model_path = g_value_dup_string (value);
      break;
    case PROP_OVERLAP_FRAMES:
      kernel->overlap_frames_prop = g_value_get_uint (value);
      break;
    case PROP_CHUNKING_MODE:
      kernel->chunking_mode = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
  GST_OBJECT_UNLOCK (kernel);
}

static void
gst_dsp_kernel_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (object);
  GST_OBJECT_LOCK (kernel);
  switch (prop_id) {
    case PROP_MSG_TYPE:
      g_value_set_uint (value, kernel->msg_type);
      break;
    case PROP_INTERLEAVE_DIRECTION:
      g_value_set_uint (value, kernel->interleave_direction);
      break;
    case PROP_HOP_SIZE:
      g_value_set_uint (value, kernel->hop_size);
      break;
    case PROP_FFT_SIZE:
      g_value_set_uint (value, kernel->fft_size);
      break;
    case PROP_WINDOW_FRAMES:
      g_value_set_uint (value, kernel->window_frames);
      break;
    case PROP_BATCH_SIZE:
      g_value_set_uint (value, kernel->batch_size);
      break;
    case PROP_SELECTED_MODEL:
      g_value_set_uint (value, kernel->selected_model);
      break;
    case PROP_MODEL_ELEMS:
      g_value_set_uint (value, kernel->model_elems);
      break;
    case PROP_MODEL_PATH:
      g_value_set_string (value, kernel->model_path);
      break;
    case PROP_OVERLAP_FRAMES:
      g_value_set_uint (value, kernel->overlap_frames_prop);
      break;
    case PROP_CHUNKING_MODE:
      g_value_set_int (value, kernel->chunking_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
  GST_OBJECT_UNLOCK (kernel);
}

static void
gst_dsp_kernel_finalize (GObject * object)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (object);
  g_free (kernel->rproc_device);
  g_free (kernel->model_path);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_dsp_kernel_start (GstBaseTransform * trans)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);

  gst_dsp_kernel_auto_detect_operation (kernel);

  if (!gst_dsp_kernel_calculate_buffer_sizes (kernel)) {
    return FALSE;
  }

  if (kernel->model_elems == 0 && kernel->fft_size == 0) {
    GST_ERROR_OBJECT (kernel,
        "fft-size must be explicitly set in pipeline (> 0), or provide "
        "model-elems/model-path instead");
    return FALSE;
  }
  if (kernel->window_frames == 0) {
    GST_ERROR_OBJECT (kernel,
        "window-frames must be explicitly set in pipeline (> 0)");
    return FALSE;
  }

  if ((kernel->msg_type == DSP_OP_STFT || kernel->msg_type == DSP_OP_ISTFT) &&
      kernel->hop_size == 0) {
    GST_ERROR_OBJECT (kernel,
        "hop-size must be explicitly set in pipeline for STFT/ISTFT (> 0)");
    return FALSE;
  }

  if (kernel->batch_size == 0) {
    kernel->batch_size = kernel->window_frames;
  }

  if (kernel->chunking_mode == 0) {
    kernel->enable_chunking = FALSE;
    GST_INFO_OBJECT (kernel, "chunking-mode=0 (forced): plain windowing");
  } else if (kernel->chunking_mode == 1) {
    kernel->enable_chunking = TRUE;
    GST_INFO_OBJECT (kernel, "chunking-mode=1 (forced): overlap-save chunking");
  } else if (kernel->window_frames > CHUNKING_THRESHOLD) {
    kernel->enable_chunking = TRUE;
    GST_INFO_OBJECT (kernel,
        "window_frames=%u > %u threshold, enabling overlap-save chunking",
        kernel->window_frames, CHUNKING_THRESHOLD);
  } else {
    kernel->enable_chunking = FALSE;
    GST_INFO_OBJECT (kernel,
        "window_frames=%u <= %u threshold, chunking disabled",
        kernel->window_frames, CHUNKING_THRESHOLD);
  }

  switch (kernel->msg_type) {
    case DSP_OP_STFT:
      if (kernel->enable_chunking) {
        if (kernel->overlap_frames_prop >= kernel->window_frames) {
          GST_ERROR_OBJECT (kernel,
              "overlap-frames (%u) must be less than window-frames (%u)",
              kernel->overlap_frames_prop, kernel->window_frames);
          return FALSE;
        }

        /* Initialize overlap-save parameters */
        kernel->overlap_frames = kernel->overlap_frames_prop;
        kernel->t_frames = kernel->overlap_frames / 2;
        kernel->hop_frames = kernel->window_frames - kernel->overlap_frames;
        kernel->hop_samples = kernel->hop_frames * kernel->hop_size;
        kernel->chunk_samples = kernel->window_frames * kernel->hop_size;

        GST_INFO_OBJECT (kernel,
            "STFT: overlap-save chunking, OVERLAP=%zu T_FRAMES=%zu "
            "HOP_FRAMES=%zu HOP_SAMPLES=%zu", kernel->overlap_frames,
            kernel->t_frames, kernel->hop_frames, kernel->hop_samples);
      } else {
        kernel->overlap_frames = 0;
        kernel->t_frames = 0;
        kernel->hop_frames = kernel->window_frames;
        kernel->hop_samples = kernel->hop_frames * kernel->hop_size;
        kernel->chunk_samples = kernel->window_frames * kernel->hop_size;

        GST_INFO_OBJECT (kernel,
            "STFT: plain windowing, WINDOW_FRAMES=%u HOP_SAMPLES=%zu",
            kernel->window_frames, kernel->hop_samples);
      }
      break;
    case DSP_OP_ISTFT:
      GST_INFO_OBJECT (kernel, "ISTFT: %s",
          kernel->enable_chunking ?
          "initializing for chunk collection and reconstruction" :
          "simple pass-through mode (no chunking)");
      break;
    case DSP_OP_DEINT_INTERLEAVE:
      GST_INFO_OBJECT (kernel, "Deinterleave/Interleave, flag=%u",
          kernel->interleave_direction);
      break;
    default:
      GST_WARNING_OBJECT (kernel, "Unknown msg-type: 0x%04x", kernel->msg_type);
      break;
  }

  /* Acquire shared RPMsg channel */
  kernel->rpmsg_chan =
      gst_ti_rpmsg_chan_acquire (kernel->rproc_id, kernel->remote_ep);
  if (!kernel->rpmsg_chan) {
    GST_ERROR_OBJECT (kernel, "Failed to acquire rpmsg channel");
    return FALSE;
  }
  GST_INFO_OBJECT (kernel, "RPMsg channel acquired (fd=%d)",
      kernel->rpmsg_chan->fd);

  /* Allocate DMA buffers */
  int r1 = dmabuf_heap_init ((char *) "linux,cma", kernel->input_buf_size,
      kernel->rproc_device, &kernel->dma_input);
  int r2 = dmabuf_heap_init ((char *) "linux,cma", kernel->output_buf_size,
      kernel->rproc_device, &kernel->dma_output);

  if (r1 != 0 || r2 != 0) {
    GST_ERROR_OBJECT (kernel, "DMA alloc failed (r1=%d r2=%d)", r1, r2);
    if (r1 == 0)
      dmabuf_heap_destroy (&kernel->dma_input);
    gst_ti_rpmsg_chan_release (kernel->rpmsg_chan);
    kernel->rpmsg_chan = NULL;
    return FALSE;
  }

  kernel->dma_allocated = TRUE;

  if (kernel->dma_input.phys_addr > G_MAXUINT32
      || kernel->dma_output.phys_addr > G_MAXUINT32) {
    GST_ERROR_OBJECT (kernel,
        "DMA buffer physical address exceeds 32 bits (input=0x%"
        G_GINT64_MODIFIER "x output=0x%" G_GINT64_MODIFIER
        "x) - firmware wire protocol only " "carries 32-bit addresses",
        kernel->dma_input.phys_addr, kernel->dma_output.phys_addr);
    dmabuf_heap_destroy (&kernel->dma_input);
    dmabuf_heap_destroy (&kernel->dma_output);
    kernel->dma_allocated = FALSE;
    gst_ti_rpmsg_chan_release (kernel->rpmsg_chan);
    kernel->rpmsg_chan = NULL;
    return FALSE;
  }

  {
    const gchar *model_elems_source;

    if (kernel->model_path && kernel->model_path[0]) {
      model_elems_source = "from model-path";
    } else if (kernel->model_elems > 0) {
      model_elems_source = "explicit";
    } else {
      model_elems_source = "auto, from fft-size";
    }

    GST_INFO_OBJECT (kernel,
        "Initialization summary: msg-type=0x%04x hop-size=%u fft-size=%u "
        "window-frames=%u batch-size=%u selected-model=%u model-elems=%u "
        "(%s)%s%s", kernel->msg_type, kernel->hop_size, kernel->fft_size,
        kernel->window_frames, kernel->batch_size, kernel->selected_model,
        kernel->model_elems, model_elems_source,
        (kernel->model_path && kernel->model_path[0]) ? " model-path=" : "",
        (kernel->model_path && kernel->model_path[0]) ?
        kernel->model_path : "");
  }
  GST_INFO_OBJECT (kernel,
      "DMA buffers: input=%u bytes @ phys=0x%08lx, output=%u bytes @ "
      "phys=0x%08lx", kernel->input_buf_size,
      (unsigned long) kernel->dma_input.phys_addr, kernel->output_buf_size,
      (unsigned long) kernel->dma_output.phys_addr);
  if (kernel->enable_chunking && kernel->msg_type == DSP_OP_STFT) {
    GST_INFO_OBJECT (kernel,
        "Overlap-save chunking: overlap_frames=%zu hop_frames=%zu "
        "chunk_samples=%zu", kernel->overlap_frames, kernel->hop_frames,
        kernel->chunk_samples);
  }

  return TRUE;
}

static gboolean
gst_dsp_kernel_stop (GstBaseTransform * trans)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);

  if (kernel->dma_allocated) {
    dmabuf_heap_destroy (&kernel->dma_input);
    dmabuf_heap_destroy (&kernel->dma_output);
    kernel->dma_allocated = FALSE;
  }

  if (kernel->input_buffer) {
    g_free (kernel->input_buffer);
    kernel->input_buffer = NULL;
  }
  kernel->input_buffer_size = 0;
  kernel->input_buffer_capacity = 0;

  /* If torn down mid-chunk-collection (ISTFT hasn't seen the last chunk
   * yet, or a mid-batch send/recv failure left previously-collected
   * chunks accumulated here), this buffer would otherwise leak. */
  if (kernel->collected_audio) {
    g_free (kernel->collected_audio);
    kernel->collected_audio = NULL;
  }
  kernel->collected_audio_size = 0;
  kernel->collected_audio_capacity = 0;
  kernel->chunks_received = 0;
  kernel->chunk_buffer_counter = 0;
  kernel->expected_n_chunks = 0;

  gst_ti_rpmsg_chan_release (kernel->rpmsg_chan);
  kernel->rpmsg_chan = NULL;

  return TRUE;
}

/* Helper: Send STFT/ISTFT message and receive response */
static GstFlowReturn
dsp_kernel_send_recv_stft (GstDspKernel * kernel, struct stft_istft_msg *req,
    struct stft_istft_msg *resp)
{
  uint32_t expected_resp = kernel->msg_resp_type ?
      kernel->msg_resp_type : ((kernel->msg_type & 0x0FFF) | 0x2000);


  gst_ti_rpmsg_chan_lock (kernel->rpmsg_chan);

  if (send_msg (kernel->rpmsg_chan->fd, (char *) req, sizeof (*req)) < 0) {
    GST_ERROR_OBJECT (kernel, "send_msg failed");
    gst_ti_rpmsg_chan_unlock (kernel->rpmsg_chan);
    return GST_FLOW_ERROR;
  }

  int resp_len = sizeof (*resp);
  if (recv_msg (kernel->rpmsg_chan->fd, sizeof (*resp), (char *) resp,
          &resp_len) < 0) {
    GST_ERROR_OBJECT (kernel, "recv_msg failed");
    gst_ti_rpmsg_chan_unlock (kernel->rpmsg_chan);
    return GST_FLOW_ERROR;
  }

  gst_ti_rpmsg_chan_unlock (kernel->rpmsg_chan);


  if (resp->hdr.type != expected_resp || resp->hdr.status != C7X_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (kernel, "DSP error: type=0x%x (expected 0x%x) status=%d",
        resp->hdr.type, expected_resp, resp->hdr.status);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

/* Helper: Send deinterleave/interleave message */
static GstFlowReturn
dsp_kernel_send_recv_deint (GstDspKernel * kernel,
    struct deint_interleave_msg *req, struct deint_interleave_msg *resp)
{
  uint32_t expected_resp = kernel->msg_resp_type ?
      kernel->msg_resp_type : ((kernel->msg_type & 0x0FFF) | 0x2000);

  GST_DEBUG_OBJECT (kernel,
      "Sending Deint/Interleave msg: type=0x%04x seq=%u len=%u", req->hdr.type,
      req->hdr.seq, req->hdr.len);
  GST_DEBUG_OBJECT (kernel, "  in_buf=0x%08x out_buf=0x%08x", req->input_buffer,
      req->output_buffer);
  GST_DEBUG_OBJECT (kernel, "  input_frame=%u fft_size=%u flag=%u",
      req->input_frame, req->fft_size, req->flag);

  gst_ti_rpmsg_chan_lock (kernel->rpmsg_chan);

  if (send_msg (kernel->rpmsg_chan->fd, (char *) req, sizeof (*req)) < 0) {
    GST_ERROR_OBJECT (kernel, "send_msg failed");
    gst_ti_rpmsg_chan_unlock (kernel->rpmsg_chan);
    return GST_FLOW_ERROR;
  }

  int resp_len = sizeof (*resp);
  if (recv_msg (kernel->rpmsg_chan->fd, sizeof (*resp), (char *) resp,
          &resp_len) < 0) {
    GST_ERROR_OBJECT (kernel, "recv_msg failed");
    gst_ti_rpmsg_chan_unlock (kernel->rpmsg_chan);
    return GST_FLOW_ERROR;
  }

  gst_ti_rpmsg_chan_unlock (kernel->rpmsg_chan);

  GST_DEBUG_OBJECT (kernel,
      "Received Deint/Interleave response: type=0x%04x status=%d",
      resp->hdr.type, resp->hdr.status);
  GST_DEBUG_OBJECT (kernel, "  input_frame=%u fft_size=%u flag=%u",
      resp->input_frame, resp->fft_size, resp->flag);

  if (resp->hdr.type != expected_resp || resp->hdr.status != C7X_STATUS_SUCCESS) {
    GST_ERROR_OBJECT (kernel, "DSP error: type=0x%x (expected 0x%x) status=%d",
        resp->hdr.type, expected_resp, resp->hdr.status);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

/* Process one complete window immediately and push it downstream, for
 * live/streaming sources (e.g. a microphone) that never send EOS. */
static GstFlowReturn
dsp_kernel_process_stream_window (GstDspKernel * kernel,
    GstBaseTransform * trans, const gint16 * window_audio)
{
  guint model_elems = gst_dsp_kernel_get_model_elems (kernel);
  gsize bytes_per_frame = model_elems * sizeof (float);
  gsize bytes_per_chunk = kernel->window_frames * bytes_per_frame;
  guint8 *spectral = (guint8 *) g_malloc0 (bytes_per_chunk);
  gsize offset_bytes = 0;
  guint num_batches =
      (kernel->window_frames + kernel->batch_size - 1) / kernel->batch_size;
  guint batch_idx;
  GstPad *srcpad;
  GstBuffer *out_buf;
  GstMemory *mem;
  GstFlowReturn push_ret;

  for (batch_idx = 0; batch_idx < num_batches; batch_idx++) {
    guint frame_start = batch_idx * kernel->batch_size;
    guint frames_in_batch = MIN (kernel->batch_size,
        kernel->window_frames - frame_start);
    gsize batch_bytes = frames_in_batch * kernel->hop_size * sizeof (gint16);
    gsize sample_offset = frame_start * kernel->hop_size;
    struct stft_istft_msg req = { }, resp = { };
    gsize batch_spectral_bytes;

    dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
    memcpy (kernel->dma_input.kern_addr, window_audio + sample_offset,
        batch_bytes);
    dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);

    req.hdr.type = kernel->msg_type;
    req.hdr.seq = kernel->sequence_number++;
    req.hdr.len = sizeof (req);
    req.selected_model = kernel->selected_model;
    req.input_buffer = (uint32_t) kernel->dma_input.phys_addr;
    req.output_buffer = (uint32_t) kernel->dma_output.phys_addr;
    req.input_frame = frames_in_batch;
    req.output_frame = frames_in_batch;

    if (dsp_kernel_send_recv_stft (kernel, &req, &resp) != GST_FLOW_OK) {
      GST_ERROR_OBJECT (kernel, "STFT: stream window batch %u failed",
          batch_idx + 1);
      g_free (spectral);
      return GST_FLOW_ERROR;
    }

    if (resp.output_frame != frames_in_batch) {
      GST_ERROR_OBJECT (kernel,
          "STFT: stream window batch %u: firmware returned output_frame=%u, "
          "expected %u - refusing to trust it for a buffer copy size",
          batch_idx + 1, resp.output_frame, frames_in_batch);
      g_free (spectral);
      return GST_FLOW_ERROR;
    }

    batch_spectral_bytes = resp.output_frame * model_elems * sizeof (float);
    dmabuf_sync (kernel->dma_output.dma_buf_fd, DMA_BUF_SYNC_START);
    memcpy (spectral + offset_bytes, kernel->dma_output.kern_addr,
        batch_spectral_bytes);
    dmabuf_sync (kernel->dma_output.dma_buf_fd, DMA_BUF_SYNC_END);
    offset_bytes += batch_spectral_bytes;
  }

  mem = gst_memory_new_wrapped ((GstMemoryFlags) 0, spectral, bytes_per_chunk,
      0, bytes_per_chunk, spectral, g_free);
  out_buf = gst_buffer_new ();
  gst_buffer_append_memory (out_buf, mem);

  srcpad = gst_element_get_static_pad (GST_ELEMENT (trans), "src");
  push_ret = gst_pad_push (srcpad, out_buf);
  gst_object_unref (srcpad);

  if (push_ret != GST_FLOW_OK && push_ret != GST_FLOW_NOT_LINKED) {
    GST_WARNING_OBJECT (kernel, "STFT: stream window push failed: %s",
        gst_flow_get_name (push_ret));
  }

  return push_ret;
}

/* STFT transform: accumulate audio. Overlap-save models process at EOS
 * via dsp_kernel_process_chunks(); plain-windowing models process each
 * complete window immediately via dsp_kernel_process_stream_window(). */
static GstFlowReturn
dsp_kernel_transform_stft (GstDspKernel * kernel, GstBaseTransform * trans,
    GstBuffer * buf)
{
  GstMapInfo map_info;
  if (!gst_buffer_map (buf, &map_info, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (kernel, "Failed to map buffer");
    return GST_FLOW_ERROR;
  }

  const gint16 *audio_in = (const gint16 *) map_info.data;
  gsize incoming_samples = map_info.size / sizeof (gint16);

  /* Accumulate input buffer */
  if (kernel->input_buffer_size + incoming_samples >
      kernel->input_buffer_capacity) {
    kernel->input_buffer_capacity =
        kernel->input_buffer_size + incoming_samples + 65536;
    kernel->input_buffer =
        (gint16 *) g_realloc (kernel->input_buffer,
        kernel->input_buffer_capacity * sizeof (gint16));
    GST_DEBUG_OBJECT (kernel, "STFT: Resized input buffer to %zu samples",
        kernel->input_buffer_capacity);
  }

  /* Append input to buffer */
  memcpy (kernel->input_buffer + kernel->input_buffer_size, audio_in,
      incoming_samples * sizeof (gint16));
  kernel->input_buffer_size += incoming_samples;

  gst_buffer_unmap (buf, &map_info);

  if (kernel->overlap_frames == 0) {
    while (kernel->input_buffer_size >= kernel->chunk_samples) {
      GstFlowReturn ret = dsp_kernel_process_stream_window (kernel, trans,
          kernel->input_buffer);

      if (ret != GST_FLOW_OK && ret != GST_FLOW_NOT_LINKED) {
        gst_buffer_set_size (buf, 0);
        return ret;
      }

      /* Shift remaining buffered samples down to the front */
      kernel->input_buffer_size -= kernel->chunk_samples;
      memmove (kernel->input_buffer,
          kernel->input_buffer + kernel->chunk_samples,
          kernel->input_buffer_size * sizeof (gint16));
    }
  }

  /* Return empty buffer - overlap-save chunks (if any) are finished at
   * EOS by dsp_kernel_process_chunks(); classification windows (if any
   * were ready) were already pushed above. */
  gst_buffer_set_size (buf, 0);

  return GST_FLOW_OK;
}

/* ISTFT transform with overlap-add */
static GstFlowReturn
dsp_kernel_transform_istft (GstDspKernel * kernel, GstBuffer * buf)
{
  GstMapInfo map_info;
  if (!gst_buffer_map (buf, &map_info, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (kernel, "Failed to map buffer");
    return GST_FLOW_ERROR;
  }

  const guint8 *spectral_in = (const guint8 *) map_info.data;
  gsize total_spectral_bytes = map_info.size;

  /* Handle empty input (STFT still accumulating) - pass through empty buffer */
  if (total_spectral_bytes == 0) {
    gst_buffer_set_size (buf, 0);
    gst_buffer_unmap (buf, &map_info);
    return GST_FLOW_OK;
  }

  GST_INFO_OBJECT (kernel, "ISTFT: Processing %zu spectral bytes",
      total_spectral_bytes);

  /* Calculate batches */
  guint num_batches =
      (kernel->window_frames + kernel->batch_size - 1) / kernel->batch_size;
  gsize max_output_samples = kernel->window_frames * kernel->hop_size;
  gint16 *audio_out =
      (gint16 *) g_malloc0 (max_output_samples * sizeof (gint16));
  gsize out_written_samples = 0;
  gsize spectral_offset = 0;

  GST_INFO_OBJECT (kernel,
      "ISTFT: Processing %u batches (window_frames=%u batch_size=%u)",
      num_batches, kernel->window_frames, kernel->batch_size);

  /* Process in batches */
  for (guint batch_idx = 0; batch_idx < num_batches; batch_idx++) {
    guint frame_start = batch_idx * kernel->batch_size;
    guint frames_in_batch =
        MIN (kernel->batch_size, kernel->window_frames - frame_start);
    /* Calculate expected spectral bytes for this batch */
    guint bins_per_frame = gst_dsp_kernel_get_model_elems (kernel);
    gsize batch_spectral_bytes =
        frames_in_batch * bins_per_frame * sizeof (float);

    if (spectral_offset + batch_spectral_bytes > total_spectral_bytes) {
      batch_spectral_bytes = total_spectral_bytes - spectral_offset;
    }

    GST_DEBUG_OBJECT (kernel,
        "ISTFT: Batch %u/%u - frames=%u spectral_bytes=%zu", batch_idx + 1,
        num_batches, frames_in_batch, batch_spectral_bytes);
    GST_INFO_OBJECT (kernel, "[ISTFT] Batch %u: in=0x%08x out=0x%08x frames=%u",
        batch_idx + 1, (uint32_t) kernel->dma_input.phys_addr,
        (uint32_t) kernel->dma_output.phys_addr, frames_in_batch);

    /* Copy spectral batch to DMA input */
    dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
    memcpy (kernel->dma_input.kern_addr, spectral_in + spectral_offset,
        batch_spectral_bytes);
    dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);

    /* Send to DSP */
    struct stft_istft_msg req = { }, resp = { };
    req.hdr.type = kernel->msg_type;
    req.hdr.seq = kernel->sequence_number++;
    req.hdr.len = sizeof (req);
    req.selected_model = kernel->selected_model;        /* param0: firmware ModelId */
    req.input_buffer = (uint32_t) kernel->dma_input.phys_addr;
    req.output_buffer = (uint32_t) kernel->dma_output.phys_addr;
    req.input_frame = frames_in_batch;  /* Number of frames in this batch */
    req.output_frame = frames_in_batch; /* Must equal input_frame */

    if (dsp_kernel_send_recv_stft (kernel, &req, &resp) != GST_FLOW_OK) {
      g_free (audio_out);
      gst_buffer_unmap (buf, &map_info);
      return GST_FLOW_ERROR;
    }

    if (resp.output_frame != frames_in_batch) {
      GST_ERROR_OBJECT (kernel,
          "ISTFT: batch %u: firmware returned output_frame=%u, expected %u "
          "- refusing to trust it for a buffer copy size", batch_idx + 1,
          resp.output_frame, frames_in_batch);
      g_free (audio_out);
      gst_buffer_unmap (buf, &map_info);
      return GST_FLOW_ERROR;
    }

    gsize batch_audio_samples = resp.output_frame * kernel->hop_size;
    gsize batch_audio_bytes = batch_audio_samples * sizeof (gint16);
    dmabuf_sync (kernel->dma_output.dma_buf_fd, DMA_BUF_SYNC_START);
    memcpy (audio_out + out_written_samples, kernel->dma_output.kern_addr,
        batch_audio_bytes);
    dmabuf_sync (kernel->dma_output.dma_buf_fd, DMA_BUF_SYNC_END);

    out_written_samples += batch_audio_samples;
    spectral_offset += batch_spectral_bytes;
  }

  GST_INFO_OBJECT (kernel,
      "ISTFT: Complete - input=%zu bytes output=%zu samples (%zu bytes)",
      total_spectral_bytes, out_written_samples,
      out_written_samples * sizeof (gint16));

  /* Apply overlap-save trimming for this chunk */
  gsize final_output_samples = out_written_samples;
  gsize trim_start_samples = 0;
  gsize trim_end_samples = 0;

  /* Use sequential buffer counting from expected_n_chunks (received via event) */
  gsize chunk_idx = kernel->chunk_buffer_counter;
  gsize n_chunks = kernel->expected_n_chunks;

  GST_INFO_OBJECT (kernel,
      "ISTFT: Sequential buffer count: received_buffer=%zu expected_n_chunks=%zu",
      chunk_idx, n_chunks);
  GST_INFO_OBJECT (kernel,
      "ISTFT: Buffer timestamp: %" GST_TIME_FORMAT " PTS: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
      GST_TIME_ARGS (GST_BUFFER_PTS (buf)));

  if (n_chunks > 1) {
    GST_INFO_OBJECT (kernel,
        "ISTFT: Chunk mode detected (n_chunks=%zu). Will apply overlap-save trimming.",
        n_chunks);
    /* Apply overlap-save trimming: keep different regions for each chunk.
     *   lo_sample = (chunk_idx == 0) ? 0 : T_SAMPLES;
     *   hi_sample = (chunk_idx == n_chunks - 1) ? CHUNK_SAMPLES : CHUNK_SAMPLES - T_SAMPLES;
     *   keep [lo_sample : hi_sample]
     */
    gsize lo_sample, hi_sample;

    if (chunk_idx == 0) {
      /* First chunk: lo=0, hi=(not last) ? CHUNK_SAMPLES - T_SAMPLES : CHUNK_SAMPLES */
      lo_sample = 0;
      hi_sample = (n_chunks == 1) ? kernel->chunk_samples :
          (kernel->chunk_samples - kernel->t_frames * kernel->hop_size);
      trim_start_samples = lo_sample;
      trim_end_samples = kernel->chunk_samples - hi_sample;
      GST_INFO_OBJECT (kernel,
          "ISTFT: Chunk %zu (FIRST): Keep [%zu:%zu], trim end %zu samples",
          chunk_idx, lo_sample, hi_sample, trim_end_samples);
    } else if (chunk_idx == n_chunks - 1) {
      /* Last chunk: lo=T_SAMPLES, hi=CHUNK_SAMPLES */
      lo_sample = kernel->t_frames * kernel->hop_size;
      hi_sample = kernel->chunk_samples;
      trim_start_samples = lo_sample;
      trim_end_samples = kernel->chunk_samples - hi_sample;
      GST_INFO_OBJECT (kernel,
          "ISTFT: Chunk %zu (LAST): Keep [%zu:%zu], trim start %zu samples",
          chunk_idx, lo_sample, hi_sample, trim_start_samples);
    } else {
      /* Middle chunks: lo=T_SAMPLES, hi=CHUNK_SAMPLES - T_SAMPLES */
      lo_sample = kernel->t_frames * kernel->hop_size;
      hi_sample = kernel->chunk_samples - kernel->t_frames * kernel->hop_size;
      trim_start_samples = lo_sample;
      trim_end_samples = kernel->chunk_samples - hi_sample;
      GST_INFO_OBJECT (kernel,
          "ISTFT: Chunk %zu (MIDDLE): Keep [%zu:%zu], trim both start=%zu end=%zu",
          chunk_idx, lo_sample, hi_sample, trim_start_samples,
          trim_end_samples);
    }

    if (trim_start_samples > 0 || trim_end_samples > 0) {
      gsize trimmed_samples =
          out_written_samples - trim_start_samples - trim_end_samples;
      if (trimmed_samples > 0) {
        /* Shift the audio to remove trimmed start samples */
        if (trim_start_samples > 0) {
          memmove (audio_out, audio_out + trim_start_samples,
              trimmed_samples * sizeof (gint16));
        }
        final_output_samples = trimmed_samples;
        GST_INFO_OBJECT (kernel,
            "ISTFT: Chunk %zu/%zu trim: start=%zu end=%zu | output %zu → %zu samples",
            chunk_idx + 1, n_chunks, trim_start_samples, trim_end_samples,
            out_written_samples, final_output_samples);
      }
    }
  }

  /* Padded samples are trimmed from the FINAL total after all chunks are
   * collected, not from individual chunks. */

  /* If in chunk mode (n_chunks > 1), collect trimmed audio instead of outputting immediately */
  if (n_chunks > 1) {
    /* Accumulate this chunk's trimmed audio */
    gsize needed_size = kernel->collected_audio_size + final_output_samples;
    if (needed_size > kernel->collected_audio_capacity) {
      kernel->collected_audio_capacity = needed_size + 65536;
      kernel->collected_audio = (gint16 *) g_realloc (kernel->collected_audio,
          kernel->collected_audio_capacity * sizeof (gint16));
    }

    memcpy (kernel->collected_audio + kernel->collected_audio_size, audio_out,
        final_output_samples * sizeof (gint16));
    kernel->collected_audio_size += final_output_samples;
    kernel->chunks_received++;

    GST_INFO_OBJECT (kernel,
        "ISTFT: ===== CHUNK %zu/%zu COLLECTED =====", chunk_idx + 1, n_chunks);
    GST_INFO_OBJECT (kernel,
        "ISTFT: Chunk %zu/%zu collected: %zu samples trimmed (from %zu original)",
        chunk_idx + 1, n_chunks, final_output_samples, out_written_samples);

    /* Calculate running total duration and expected chunk timing */
    gdouble running_duration_sec =
        (gdouble) kernel->collected_audio_size / 16000.0;
    guint running_duration_ms = (guint) (running_duration_sec * 1000.0);
    guint running_minutes = running_duration_ms / 60000;
    guint running_seconds = (running_duration_ms % 60000) / 1000;
    guint running_milliseconds = running_duration_ms % 1000;

    /* Calculate expected timing for this chunk */
    gsize expected_chunk_samples = kernel->hop_samples; /* Each chunk contributes HOP_SAMPLES after trimming */
    gdouble expected_chunk_duration_sec =
        (gdouble) expected_chunk_samples / 16000.0;
    gdouble expected_chunk_end_sec =
        (chunk_idx + 1) * expected_chunk_duration_sec;

    GST_INFO_OBJECT (kernel,
        "ISTFT: Running total: %zu samples (%.6f sec / %u:%02u.%03u) collected so far",
        kernel->collected_audio_size, running_duration_sec, running_minutes,
        running_seconds, running_milliseconds);
    GST_INFO_OBJECT (kernel,
        "ISTFT: Chunk timing - Expected end: %.6f sec, Actual running: %.6f sec (diff: %.6f sec)",
        expected_chunk_end_sec, running_duration_sec,
        running_duration_sec - expected_chunk_end_sec);

    /* Increment buffer counter for next chunk */
    kernel->chunk_buffer_counter++;
    GST_INFO_OBJECT (kernel,
        "ISTFT: Incremented buffer counter to %zu (waiting for %zu total)",
        kernel->chunk_buffer_counter, n_chunks);

    /* If this is the last chunk, output the combined audio */
    if (kernel->chunks_received == n_chunks) {
      GST_INFO_OBJECT (kernel,
          "ISTFT: ===== ALL %zu CHUNKS COLLECTED =====", n_chunks);
      GST_INFO_OBJECT (kernel,
          "ISTFT: FINAL OUTPUT: Combining all chunks with total %zu samples before padding trim",
          kernel->collected_audio_size);

      /* Trim final padding from the total collected audio */
      gsize final_samples = kernel->collected_audio_size;
      if (kernel->padded_samples_added > 0
          && final_samples >= kernel->padded_samples_added) {
        final_samples -= kernel->padded_samples_added;
        GST_INFO_OBJECT (kernel,
            "ISTFT: Removing final %zu padded samples. Output: %zu → %zu",
            kernel->padded_samples_added, kernel->collected_audio_size,
            final_samples);
      }

      gsize output_bytes = final_samples * sizeof (gint16);

      /* Log buffer timing information */
      GST_INFO_OBJECT (kernel,
          "ISTFT: Output buffer timestamp: %" GST_TIME_FORMAT " duration: %"
          GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
          GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

      /* Calculate and log audio duration */
      gdouble audio_duration_sec = (gdouble) final_samples / 16000.0;   /* 16kHz sample rate */
      guint audio_duration_ms = (guint) (audio_duration_sec * 1000.0);
      guint audio_minutes = audio_duration_ms / 60000;
      guint audio_seconds = (audio_duration_ms % 60000) / 1000;
      guint audio_milliseconds = audio_duration_ms % 1000;

      GST_INFO_OBJECT (kernel, "ISTFT: Audio output details:");
      GST_INFO_OBJECT (kernel,
          "  Total samples (after padding trim): %zu", final_samples);
      GST_INFO_OBJECT (kernel, "  Total bytes: %zu", output_bytes);
      GST_INFO_OBJECT (kernel,
          "  Duration: %u:%02u.%03u (%.6f seconds)",
          audio_minutes, audio_seconds, audio_milliseconds, audio_duration_sec);


      gst_buffer_unmap (buf, &map_info);

      /* Create new memory with collected audio (trimmed to final_samples) */
      GstMemory *new_mem = gst_memory_new_wrapped ((GstMemoryFlags) 0,
          kernel->collected_audio, output_bytes, 0, output_bytes,
          kernel->collected_audio, g_free);

      gst_buffer_remove_all_memory (buf);
      gst_buffer_append_memory (buf, new_mem);
      gst_buffer_set_size (buf, output_bytes);

      /* Reset for next sequence */
      kernel->collected_audio = NULL;
      kernel->collected_audio_size = 0;
      kernel->collected_audio_capacity = 0;
      kernel->chunks_received = 0;
      kernel->chunk_buffer_counter = 0;
      kernel->expected_n_chunks = 0;

      return GST_FLOW_OK;
    } else {
      /* Not the last chunk yet - consume buffer without outputting */
      GST_INFO_OBJECT (kernel,
          "ISTFT: Chunk %zu/%zu waiting for more chunks... (received %zu/%zu)",
          chunk_idx + 1, n_chunks, kernel->chunks_received, n_chunks);

      gst_buffer_unmap (buf, &map_info);
      gst_buffer_set_size (buf, 0);
      g_free (audio_out);
      return GST_FLOW_OK;
    }
  }

  /* Non-chunk mode: output immediately */
  gsize output_bytes = final_output_samples * sizeof (gint16);

  /* Calculate and log audio duration for non-chunk mode */
  gdouble audio_duration_sec = (gdouble) final_output_samples / 16000.0;        /* 16kHz sample rate */
  guint audio_duration_ms = (guint) (audio_duration_sec * 1000.0);
  guint audio_minutes = audio_duration_ms / 60000;
  guint audio_seconds = (audio_duration_ms % 60000) / 1000;
  guint audio_milliseconds = audio_duration_ms % 1000;

  GST_INFO_OBJECT (kernel,
      "ISTFT: Non-chunk mode (or unknown): Outputting %zu samples immediately",
      final_output_samples);
  GST_INFO_OBJECT (kernel, "ISTFT: Audio output details (single buffer):");
  GST_INFO_OBJECT (kernel, "  Total samples: %zu", final_output_samples);
  GST_INFO_OBJECT (kernel, "  Total bytes: %zu", output_bytes);
  GST_INFO_OBJECT (kernel,
      "  Duration: %u:%02u.%03u (%.6f seconds)",
      audio_minutes, audio_seconds, audio_milliseconds, audio_duration_sec);


  gst_buffer_unmap (buf, &map_info);

  /* Create new memory for the audio output (only final_output_samples) */
  GstMemory *new_mem = gst_memory_new_wrapped ((GstMemoryFlags) 0,
      audio_out, final_output_samples * sizeof (gint16), 0, output_bytes,
      audio_out, g_free);

  /* Replace buffer memory */
  gst_buffer_replace_all_memory (buf, new_mem);

  return GST_FLOW_OK;
}

/* Deinterleave/Interleave transform */
static GstFlowReturn
dsp_kernel_transform_deint_interleave (GstDspKernel * kernel, GstBuffer * buf)
{
  GstMapInfo map_info;
  if (!gst_buffer_map (buf, &map_info, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (kernel, "Failed to map buffer");
    return GST_FLOW_ERROR;
  }

  gsize input_size = map_info.size;
  const char *op_name =
      (kernel->interleave_direction == 0) ? "Deinterleave" : "Interleave";

  GST_DEBUG_OBJECT (kernel, "%s: Processing %zu bytes (msg_type=0x%04x)",
      op_name, input_size, kernel->msg_type);

  /* Handle empty input - pass through empty buffer */
  if (input_size == 0) {
    gst_buffer_unmap (buf, &map_info);
    return GST_FLOW_OK;
  }

  guint expected_size =
      kernel->window_frames * gst_dsp_kernel_get_model_elems (kernel) *
      sizeof (float);
  if (input_size != expected_size) {
    GST_WARNING_OBJECT (kernel, "%s: Input size %zu != expected %u bytes "
        "(frames=%u fft_size=%u)", op_name, input_size, expected_size,
        kernel->window_frames, kernel->fft_size);
  }

  /* Copy to DMA input */
  GST_INFO_OBJECT (kernel, "[%s] DMA Input Buffer:", op_name);
  GST_INFO_OBJECT (kernel, "  Physical addr: 0x%08x",
      (uint32_t) kernel->dma_input.phys_addr);
  GST_INFO_OBJECT (kernel, "  Virtual addr:  %p", kernel->dma_input.kern_addr);
  GST_INFO_OBJECT (kernel, "  Writing %zu bytes from GStreamer buffer",
      input_size);

  dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
  memcpy (kernel->dma_input.kern_addr, map_info.data, input_size);
  dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);

  /* Print first few floats for debugging */
  float *input_floats = (float *) map_info.data;
  GST_INFO_OBJECT (kernel, "  Input data [0..3]: %.6f, %.6f, %.6f, %.6f",
      input_floats[0], input_floats[1], input_floats[2], input_floats[3]);

  /* Send to DSP using correct message structure */
  struct deint_interleave_msg req = { }, resp = { };
  req.hdr.type = kernel->msg_type;
  req.hdr.seq = kernel->sequence_number++;
  req.hdr.len = sizeof (req);
  req.input_buffer = (uint32_t) kernel->dma_input.phys_addr;
  req.output_buffer = (uint32_t) kernel->dma_output.phys_addr;
  req.input_frame = kernel->window_frames;
  req.fft_size = kernel->fft_size;
  req.flag = kernel->interleave_direction;


  if (dsp_kernel_send_recv_deint (kernel, &req, &resp) != GST_FLOW_OK) {
    gst_buffer_unmap (buf, &map_info);
    return GST_FLOW_ERROR;
  }

  /* Copy DSP output - output size should equal input size */
  gsize output_size = input_size;
  GST_INFO_OBJECT (kernel, "[%s] DMA Output Buffer:", op_name);
  GST_INFO_OBJECT (kernel, "  Physical addr: 0x%08x",
      (uint32_t) kernel->dma_output.phys_addr);
  GST_INFO_OBJECT (kernel, "  Virtual addr:  %p", kernel->dma_output.kern_addr);
  GST_INFO_OBJECT (kernel, "  Reading %zu bytes to GStreamer buffer",
      output_size);

  dmabuf_sync (kernel->dma_output.dma_buf_fd, DMA_BUF_SYNC_START);
  memcpy ((void *) map_info.data, kernel->dma_output.kern_addr, output_size);
  dmabuf_sync (kernel->dma_output.dma_buf_fd, DMA_BUF_SYNC_END);

  /* Print first few floats for debugging */
  float *output_floats = (float *) map_info.data;
  GST_INFO_OBJECT (kernel, "  Output data [0..3]: %.6f, %.6f, %.6f, %.6f",
      output_floats[0], output_floats[1], output_floats[2], output_floats[3]);

  gst_buffer_set_size (buf, (gssize) output_size);
  gst_buffer_unmap (buf, &map_info);

  GST_INFO_OBJECT (kernel, "%s: Complete - input=%zu output=%zu", op_name,
      input_size, output_size);
  return GST_FLOW_OK;
}

/* Process all chunks with overlap-save; each chunk outputs full spectral
 * data, trimming happens downstream in ISTFT. */
static GstFlowReturn
dsp_kernel_process_chunks (GstDspKernel * kernel, GstBaseTransform * trans)
{
  gsize n_samples = kernel->input_buffer_size;

  /* Calculate number of chunks */
  gsize n_chunks;
  if (kernel->overlap_frames == 0) {
    n_chunks = n_samples / kernel->chunk_samples;
    if (n_chunks == 0) {
      GST_WARNING_OBJECT (kernel,
          "STFT: %zu samples is less than one window (%zu samples) - "
          "no complete window to process, skipping", n_samples,
          kernel->chunk_samples);
      kernel->input_buffer_size = 0;
      return GST_FLOW_OK;
    }
  } else if (n_samples <= kernel->chunk_samples) {
    /* Overlap-save (GCRN-like enhancement models): pad so every input
     * sample is covered and can be reconstructed downstream by ISTFT. */
    n_chunks = 1;
  } else {
    n_chunks = 1 + (gsize) ceil ((double) (n_samples - kernel->chunk_samples) /
        (double) kernel->hop_samples);
  }

  /* Calculate padded length. In truncating mode (overlap_frames==0) this is
   * <= n_samples (the trailing partial window is dropped), so there is no
   * padding to add - guard against the unsigned subtraction underflowing. */
  kernel->total_padded_len =
      (n_chunks - 1) * kernel->hop_samples + kernel->chunk_samples;
  kernel->padded_samples_added =
      (kernel->total_padded_len > n_samples) ?
      kernel->total_padded_len - n_samples : 0;

  gint16 *padded_audio =
      (gint16 *) g_malloc0 (kernel->total_padded_len * sizeof (gint16));
  memcpy (padded_audio, kernel->input_buffer,
      MIN (kernel->total_padded_len, n_samples) * sizeof (gint16));

  /* Calculate spectral output parameters - same for all chunks */
  guint bins_per_frame = gst_dsp_kernel_get_model_elems (kernel);
  gsize bytes_per_frame = bins_per_frame * sizeof (float);
  gsize bytes_per_chunk = kernel->window_frames * bytes_per_frame;

  GstPad *srcpad = gst_element_get_static_pad (GST_ELEMENT (trans), "src");

  /* Send custom event downstream BEFORE pushing chunks with overlap-save parameters */
  GstEvent *chunk_event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
      gst_structure_new ("ti-overlap-save-chunk-count",
          "n_chunks", G_TYPE_UINT, (guint) n_chunks,
          "t_frames", G_TYPE_UINT, (guint) kernel->t_frames,
          "hop_size", G_TYPE_UINT, (guint) kernel->hop_size,
          "chunk_samples", G_TYPE_UINT64, (guint64) kernel->chunk_samples,
          "padded_samples_added", G_TYPE_UINT64,
          (guint64) kernel->padded_samples_added,
          NULL));

  gboolean event_ret = gst_pad_push_event (srcpad, chunk_event);
  GST_INFO_OBJECT (kernel,
      "STFT: Sent chunk count event BEFORE processing (n_chunks=%zu, t_frames=%zu, hop_size=%u, event_sent=%d)",
      n_chunks, kernel->t_frames, kernel->hop_size, event_ret);

  /* Process each chunk and push immediately (one chunk at a time to downstream) */
  for (gsize chunk_idx = 0; chunk_idx < n_chunks; chunk_idx++) {
    gsize chunk_offset = chunk_idx * kernel->hop_samples;
    gint16 *chunk_input = padded_audio + chunk_offset;

    GST_INFO_OBJECT (kernel,
        "STFT: ===== CHUNK %zu/%zu START =====", chunk_idx + 1, n_chunks);
    GST_INFO_OBJECT (kernel,
        "STFT: Chunk %zu/%zu: audio offset=%zu samples, input from [%zu:%zu]",
        chunk_idx + 1, n_chunks, chunk_offset, chunk_offset,
        chunk_offset + kernel->chunk_samples);
    GST_INFO_OBJECT (kernel,
        "STFT: Parameters: hop_size=%u fft_size=%u window_frames=%u batch_size=%u",
        kernel->hop_size, kernel->fft_size, kernel->window_frames,
        kernel->batch_size);
    GST_INFO_OBJECT (kernel,
        "STFT: Overlap-save params: overlap_frames=%zu t_frames=%zu hop_frames=%zu hop_samples=%zu",
        kernel->overlap_frames, kernel->t_frames, kernel->hop_frames,
        kernel->hop_samples);

    /* Allocate buffer for this chunk's spectral data */
    guint8 *chunk_spectral = (guint8 *) g_malloc0 (bytes_per_chunk);
    gsize chunk_offset_bytes = 0;

    /* Process this chunk in batches */
    guint num_batches =
        (kernel->window_frames + kernel->batch_size - 1) / kernel->batch_size;

    for (guint batch_idx = 0; batch_idx < num_batches; batch_idx++) {
      guint frame_start = batch_idx * kernel->batch_size;
      guint frames_in_batch = MIN (kernel->batch_size,
          kernel->window_frames - frame_start);
      gsize batch_samples = frames_in_batch * kernel->hop_size;
      gsize batch_bytes = batch_samples * sizeof (gint16);
      gsize sample_offset = frame_start * kernel->hop_size;


      /* Copy batch to DMA input */
      dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
      memcpy (kernel->dma_input.kern_addr, chunk_input + sample_offset,
          batch_bytes);
      dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);

      /* Send to DSP */
      struct stft_istft_msg req = { }, resp = { };
      req.hdr.type = kernel->msg_type;
      req.hdr.seq = kernel->sequence_number++;
      req.hdr.len = sizeof (req);
      req.selected_model = kernel->selected_model;      /* param0: firmware ModelId */
      req.input_buffer = (uint32_t) kernel->dma_input.phys_addr;
      req.output_buffer = (uint32_t) kernel->dma_output.phys_addr;
      req.input_frame = frames_in_batch;
      req.output_frame = frames_in_batch;

      if (dsp_kernel_send_recv_stft (kernel, &req, &resp) != GST_FLOW_OK) {
        GST_ERROR_OBJECT (kernel, "STFT: Batch %u failed for chunk %zu",
            batch_idx + 1, chunk_idx + 1);
        g_free (padded_audio);
        g_free (chunk_spectral);
        gst_object_unref (srcpad);
        return GST_FLOW_ERROR;
      }

      if (resp.output_frame != frames_in_batch) {
        GST_ERROR_OBJECT (kernel,
            "STFT: chunk %zu batch %u: firmware returned output_frame=%u, "
            "expected %u - refusing to trust it for a buffer copy size",
            chunk_idx + 1, batch_idx + 1, resp.output_frame, frames_in_batch);
        g_free (padded_audio);
        g_free (chunk_spectral);
        gst_object_unref (srcpad);
        return GST_FLOW_ERROR;
      }

      /* Copy DSP output */
      guint out_bins_per_frame = gst_dsp_kernel_get_model_elems (kernel);
      gsize batch_spectral_bytes =
          resp.output_frame * out_bins_per_frame * sizeof (float);

      dmabuf_sync (kernel->dma_output.dma_buf_fd, DMA_BUF_SYNC_START);
      memcpy (chunk_spectral + chunk_offset_bytes, kernel->dma_output.kern_addr,
          batch_spectral_bytes);
      dmabuf_sync (kernel->dma_output.dma_buf_fd, DMA_BUF_SYNC_END);
      chunk_offset_bytes += batch_spectral_bytes;
    }

    /* Push this chunk's spectral output immediately */
    GstMemory *chunk_mem = gst_memory_new_wrapped ((GstMemoryFlags) 0,
        chunk_spectral, bytes_per_chunk, 0, bytes_per_chunk,
        chunk_spectral, g_free);

    GstBuffer *chunk_buf = gst_buffer_new ();
    gst_buffer_append_memory (chunk_buf, chunk_mem);

    GST_INFO_OBJECT (kernel,
        "STFT: Pushing chunk %zu/%zu spectral data: %zu bytes",
        chunk_idx + 1, n_chunks, bytes_per_chunk);
    GST_INFO_OBJECT (kernel,
        "STFT: Chunk %zu: Expected to be trimmed by ISTFT as: [idx=%zu/%zu]",
        chunk_idx + 1, chunk_idx, n_chunks);

    GstFlowReturn push_ret = gst_pad_push (srcpad, chunk_buf);
    if (push_ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (kernel,
          "STFT: Failed to push chunk %zu spectral output: %s",
          chunk_idx + 1, gst_flow_get_name (push_ret));
      g_free (padded_audio);
      gst_object_unref (srcpad);
      return push_ret;
    }
    GST_INFO_OBJECT (kernel,
        "STFT: ===== CHUNK %zu/%zu END (pushed) =====", chunk_idx + 1,
        n_chunks);
  }

  g_free (padded_audio);
  gst_object_unref (srcpad);

  GST_INFO_OBJECT (kernel,
      "========== STFT CHUNK PROCESSING COMPLETE ==========");
  GST_INFO_OBJECT (kernel,
      "STFT: All %zu chunks processed and pushed downstream", n_chunks);
  GST_INFO_OBJECT (kernel,
      "STFT: Now waiting for ISTFT to collect and output combined audio...");

  return GST_FLOW_OK;
}

/* Sink event handler for EOS - triggers chunk processing */
static gboolean
gst_dsp_kernel_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);

  /* Handle custom chunk count event from STFT (for ISTFT to use) */
  if (GST_EVENT_TYPE (event) == GST_EVENT_CUSTOM_DOWNSTREAM) {
    const GstStructure *structure = gst_event_get_structure (event);
    if (gst_structure_has_name (structure, "ti-overlap-save-chunk-count")) {
      guint32 n_chunks = 0;
      guint32 t_frames = 0;
      guint32 hop_size = 0;
      guint64 chunk_samples = 0;
      guint64 padded_samples_added = 0;

      if (gst_structure_get_uint (structure, "n_chunks", &n_chunks)) {
        kernel->expected_n_chunks = n_chunks;
        kernel->chunk_buffer_counter = 0;

        /* Extract overlap-save parameters for trimming calculations */
        gst_structure_get_uint (structure, "t_frames", &t_frames);
        gst_structure_get_uint (structure, "hop_size", &hop_size);
        gst_structure_get_uint64 (structure, "chunk_samples", &chunk_samples);
        gst_structure_get_uint64 (structure, "padded_samples_added",
            &padded_samples_added);

        /* Set these in kernel for ISTFT trimming */
        kernel->t_frames = t_frames;
        kernel->hop_size = hop_size;
        kernel->chunk_samples = chunk_samples;
        kernel->padded_samples_added = padded_samples_added;

        GST_INFO_OBJECT (kernel, "ISTFT: Received chunk count event:");
        GST_INFO_OBJECT (kernel,
            "  n_chunks=%u t_frames=%u hop_size=%u chunk_samples=%zu padded=%zu",
            n_chunks, t_frames, hop_size, (gsize) chunk_samples,
            (gsize) padded_samples_added);
      }
    }
  }

  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    GST_INFO_OBJECT (kernel, "EOS event received");
    GST_INFO_OBJECT (kernel, "  msg_type=0x%04x (DSP_OP_STFT=0x%04x)",
        kernel->msg_type, DSP_OP_STFT);
    GST_INFO_OBJECT (kernel, "  input_buffer_size=%zu",
        kernel->input_buffer_size);

    /* Trigger overlap-save chunk processing for STFT */
    if (kernel->msg_type == DSP_OP_STFT && kernel->input_buffer_size > 0) {
      GST_INFO_OBJECT (kernel,
          "STFT: Processing %zu buffered audio samples with overlap-save chunking",
          kernel->input_buffer_size);

      GstFlowReturn ret = dsp_kernel_process_chunks (kernel, trans);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (kernel, "STFT: Chunk processing failed");
        return FALSE;           /* Let EOS propagate even on error */
      }

      GST_INFO_OBJECT (kernel, "STFT: Chunk processing complete");
    } else {
      GST_INFO_OBJECT (kernel,
          "STFT: Skipping chunk processing (msg_type match=%d, has data=%d)",
          kernel->msg_type == DSP_OP_STFT, kernel->input_buffer_size > 0);
    }
  }

  /* Chain up to parent class event handler */
  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static GstFlowReturn
gst_dsp_kernel_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);

  switch (kernel->msg_type) {
    case DSP_OP_STFT:
      return dsp_kernel_transform_stft (kernel, trans, buf);
    case DSP_OP_ISTFT:
      return dsp_kernel_transform_istft (kernel, buf);
    case DSP_OP_DEINT_INTERLEAVE:
      return dsp_kernel_transform_deint_interleave (kernel, buf);
    default:
      GST_ERROR_OBJECT (kernel, "Unknown operation type: 0x%04x",
          kernel->msg_type);
      return GST_FLOW_ERROR;
  }
}

/* Transform caps based on operation type */
static GstCaps *
gst_dsp_kernel_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);
  GstCaps *ret = NULL;

  GST_DEBUG_OBJECT (kernel,
      "transform_caps: direction=%s, msg_type=0x%04x, caps=%" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "src" : "sink", kernel->msg_type, caps);

  /* STFT: audio/x-raw (S16LE) → application/octet-stream */
  if (kernel->msg_type == DSP_OP_STFT) {
    if (direction == GST_PAD_SINK) {
      /* Sink receives audio/x-raw, src outputs application/octet-stream */
      ret = gst_caps_new_empty_simple ("application/octet-stream");
    } else {
      /* Src outputs application/octet-stream, sink receives audio/x-raw */
      ret = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S16LE",
          "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
    }
  }
  /* ISTFT: application/octet-stream → audio/x-raw (S16LE) */
  else if (kernel->msg_type == DSP_OP_ISTFT) {
    if (direction == GST_PAD_SINK) {
      /* Sink receives application/octet-stream, src outputs audio/x-raw */
      ret = gst_caps_new_simple ("audio/x-raw",
          "format", G_TYPE_STRING, "S16LE",
          "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
    } else {
      /* Src outputs audio/x-raw, sink receives application/octet-stream */
      ret = gst_caps_new_empty_simple ("application/octet-stream");
    }
  }
  /* Deinterleave/Interleave: application/octet-stream both ways */
  else {
    ret = gst_caps_new_empty_simple ("application/octet-stream");
  }

  /* Apply filter if provided */
  if (filter) {
    GstCaps *tmp =
        gst_caps_intersect_full (ret, filter, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (ret);
    ret = tmp;
  }

  GST_DEBUG_OBJECT (kernel, "transformed caps to %" GST_PTR_FORMAT, ret);
  return ret;
}
