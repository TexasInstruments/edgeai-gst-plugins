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
#include <gst/allocators/gstdmabuf.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <cstdio>

extern "C"
{
#include <rproc_id.h>
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

typedef struct _GstDspKernelAllocator GstDspKernelAllocator;
typedef struct _GstDspKernelAllocatorClass GstDspKernelAllocatorClass;

struct _GstDspKernelAllocator
{
  GstDmaBufAllocator base;
  struct dma_buf_params *target;
};

struct _GstDspKernelAllocatorClass
{
  GstDmaBufAllocatorClass parent_class;
};

#define GST_TYPE_DSP_KERNEL_ALLOCATOR (gst_dsp_kernel_allocator_get_type ())
#define GST_DSP_KERNEL_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DSP_KERNEL_ALLOCATOR, GstDspKernelAllocator))
#define GST_IS_DSP_KERNEL_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DSP_KERNEL_ALLOCATOR))

static GType gst_dsp_kernel_allocator_get_type (void);
G_DEFINE_TYPE (GstDspKernelAllocator, gst_dsp_kernel_allocator,
    GST_TYPE_DMABUF_ALLOCATOR);

static GstMemory *
gst_dsp_kernel_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstDspKernelAllocator *self = GST_DSP_KERNEL_ALLOCATOR (allocator);

  if (!self->target) {
    GST_ERROR_OBJECT (allocator, "GstDspKernelAllocator has no target bound");
    return NULL;
  }

  return gst_dmabuf_allocator_alloc_with_flags (allocator,
      self->target->dma_buf_fd, size, GST_FD_MEMORY_FLAG_DONT_CLOSE);
}

static void
gst_dsp_kernel_allocator_class_init (GstDspKernelAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = GST_DEBUG_FUNCPTR (gst_dsp_kernel_allocator_alloc);
}

static void
gst_dsp_kernel_allocator_init (GstDspKernelAllocator * self)
{
  GST_OBJECT_FLAG_SET (GST_ALLOCATOR_CAST (self),
      GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
  self->target = NULL;
}

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
  PROP_MAX_STREAM_SAMPLES,
};

/* RPMsg/DMA infrastructure defaults */
#define DEFAULT_RPROC_DEVICE    "/dev/remoteproc0"
#define DEFAULT_RPROC_ID        DSP_C71_0       /* from <rproc_id.h> (ti-rpmsg-char) */
#define DEFAULT_REMOTE_EP       13      /* C7X_SERVICE_ENDPOINT_GENERIC */
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
#define DEFAULT_MAX_STREAM_SAMPLES 0    /* 0 = must be set explicitly when chunking is active */

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
static GstFlowReturn gst_dsp_kernel_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_dsp_kernel_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize);
static GstCaps *gst_dsp_kernel_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_dsp_kernel_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_dsp_kernel_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean gst_dsp_kernel_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static gboolean gst_dsp_kernel_input_is_dma_input (GstDspKernel * kernel,
    GstBuffer * buf);
static gboolean gst_dsp_kernel_acquire_dma_output (GstDspKernel * kernel,
    gsize size, GstBuffer ** out_buf, GstMapInfo * out_map);
static gboolean gst_dsp_kernel_acquire_output_buffer (GstDspKernel * kernel,
    gsize size, GstBuffer ** out_buf, GstMapInfo * out_map,
    struct dma_buf_params **out_target);

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
  /* Real transform, not transform_ip: always_in_place elements never get
   * decide_allocation called on them. */
  bt->transform = GST_DEBUG_FUNCPTR (gst_dsp_kernel_transform);
  bt->transform_size = GST_DEBUG_FUNCPTR (gst_dsp_kernel_transform_size);
  bt->transform_caps = GST_DEBUG_FUNCPTR (gst_dsp_kernel_transform_caps);
  bt->set_caps = GST_DEBUG_FUNCPTR (gst_dsp_kernel_set_caps);
  bt->sink_event = GST_DEBUG_FUNCPTR (gst_dsp_kernel_sink_event);
  bt->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_dsp_kernel_propose_allocation);
  bt->decide_allocation = GST_DEBUG_FUNCPTR (gst_dsp_kernel_decide_allocation);
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

  g_object_class_install_property (gobject_class, PROP_MAX_STREAM_SAMPLES,
      g_param_spec_uint ("max-stream-samples", "Max Stream Samples",
          "Upper bound on audio samples for one overlap-save chunked STFT "
          "stream (start to EOS) - REQUIRED (> 0) when chunking is active. ",
          0, G_MAXUINT, DEFAULT_MAX_STREAM_SAMPLES,
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
    } else if (kernel->model_elems == 0) {
      GST_ERROR_OBJECT (kernel,
          "model-path '%s' does not match any known model ",
          kernel->model_path);
      return FALSE;
    } else {
      GST_INFO_OBJECT (kernel,
          "model-path '%s' does not match any known model - using "
          "explicitly-set model-elems=%u", kernel->model_path,
          kernel->model_elems);
    }
  }

  if (kernel->model_elems == 0 && kernel->fft_size == 0) {
    GST_WARNING_OBJECT (kernel, "fft_size is 0, cannot calculate buffer sizes");
    return TRUE;
  }

  guint model_elems = gst_dsp_kernel_get_model_elems (kernel);

  switch (kernel->msg_type) {
    case DSP_OP_STFT:
      kernel->input_buf_size =
          kernel->window_frames * kernel->hop_size * sizeof (int16_t);
      kernel->output_buf_size =
          kernel->window_frames * model_elems * sizeof (float);
      break;

    case DSP_OP_ISTFT:
      kernel->input_buf_size =
          kernel->window_frames * model_elems * sizeof (float);
      kernel->output_buf_size =
          kernel->window_frames * kernel->hop_size * sizeof (int16_t);
      break;

    case DSP_OP_DEINT_INTERLEAVE:
      kernel->input_buf_size =
          kernel->window_frames * model_elems * sizeof (float);
      kernel->output_buf_size =
          kernel->window_frames * model_elems * sizeof (float);
      break;

    default:
      GST_WARNING_OBJECT (kernel,
          "Unknown operation type 0x%04x, buffer sizes must be provided",
          kernel->msg_type);
      break;
  }

  GST_INFO_OBJECT (kernel, "Buffer sizes: input=%u bytes output=%u bytes "
      "(model_elems=%u)", kernel->input_buf_size, kernel->output_buf_size,
      model_elems);

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

  if (g_str_has_suffix (name, "istft") || g_strcmp0 (name, "istft") == 0) {
    kernel->msg_type = DSP_OP_ISTFT;
  } else if (g_str_has_suffix (name, "stft") || g_strcmp0 (name, "stft") == 0) {
    kernel->msg_type = DSP_OP_STFT;
  } else if (g_str_has_suffix (name, "deinterleave")
      || g_strcmp0 (name, "deinterleave") == 0) {
    kernel->msg_type = DSP_OP_DEINT_INTERLEAVE;
    kernel->interleave_direction = 0;   /* 0 = deinterleave */
  } else if (g_str_has_suffix (name, "interleave")
      || g_strcmp0 (name, "interleave") == 0) {
    kernel->msg_type = DSP_OP_DEINT_INTERLEAVE;
    kernel->interleave_direction = 1;   /* 1 = interleave */
  } else {
    GST_WARNING_OBJECT (kernel,
        "Could not auto-detect operation from name '%s'", name);
    return;
  }

  GST_INFO_OBJECT (kernel, "Auto-detected msg-type=0x%04x from name '%s'",
      kernel->msg_type, name);

  if (kernel->msg_resp_type == 0) {
    kernel->msg_resp_type = (kernel->msg_type & 0x0FFF) | 0x2000;
  }
}

static void
gst_dsp_kernel_init (GstDspKernel * kernel)
{
  kernel->rproc_device = g_strdup (DEFAULT_RPROC_DEVICE);
  kernel->rproc_id = DEFAULT_RPROC_ID;
  kernel->remote_ep = DEFAULT_REMOTE_EP;
  kernel->selected_model = DEFAULT_SELECTED_MODEL;
  kernel->model_path = g_strdup (DEFAULT_MODEL_PATH);
  kernel->overlap_frames_prop = DEFAULT_OVERLAP_FRAMES;
  kernel->chunking_mode = DEFAULT_CHUNKING_MODE;
  kernel->sequence_number = 1;
  kernel->sample_rate = 0;
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
    case PROP_MAX_STREAM_SAMPLES:
      kernel->max_stream_samples = g_value_get_uint (value);
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
    case PROP_MAX_STREAM_SAMPLES:
      g_value_set_uint (value, kernel->max_stream_samples);
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

  gst_dsp_kernel_stop (GST_BASE_TRANSFORM (kernel));

  g_free (kernel->rproc_device);
  g_free (kernel->model_path);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dsp_kernel_setup_dma_output_pool (GstDspKernel * kernel)
{
  GstBufferPool *pool;
  GstStructure *config;
  GstAllocationParams params;

  kernel->dma_output_allocator =
      GST_ALLOCATOR (g_object_new (GST_TYPE_DSP_KERNEL_ALLOCATOR, NULL));
  GST_DSP_KERNEL_ALLOCATOR (kernel->dma_output_allocator)->target =
      &kernel->dma_output;

  pool = gst_buffer_pool_new ();
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, NULL, kernel->output_buf_size,
      1, 1);
  gst_allocation_params_init (&params);
  gst_buffer_pool_config_set_allocator (config, kernel->dma_output_allocator,
      &params);

  if (!gst_buffer_pool_set_config (pool, config) ||
      !gst_buffer_pool_set_active (pool, TRUE)) {
    GST_WARNING_OBJECT (kernel,
        "Failed to set up internal dma_output reuse pool (size=%u)",
        kernel->output_buf_size);
    gst_object_unref (pool);
    gst_clear_object (&kernel->dma_output_allocator);
    kernel->dma_output_pool = NULL;
    kernel->dma_output_pool_ok = FALSE;
  } else {
    kernel->dma_output_pool = pool;
    kernel->dma_output_pool_ok = TRUE;
  }
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
  } else if (kernel->chunking_mode == 1) {
    kernel->enable_chunking = TRUE;
  } else {
    kernel->enable_chunking = kernel->window_frames > CHUNKING_THRESHOLD;
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

        kernel->overlap_frames = kernel->overlap_frames_prop;
        kernel->t_frames = kernel->overlap_frames / 2;
        kernel->hop_frames = kernel->window_frames - kernel->overlap_frames;
      } else {
        kernel->overlap_frames = 0;
        kernel->t_frames = 0;
        kernel->hop_frames = kernel->window_frames;
      }
      kernel->hop_samples = kernel->hop_frames * kernel->hop_size;
      kernel->chunk_samples = kernel->window_frames * kernel->hop_size;

      /* Samples accumulate directly in dma_input instead of a heap buffer */
      if (kernel->enable_chunking) {
        if (kernel->max_stream_samples == 0) {
          GST_ERROR_OBJECT (kernel,
              "max-stream-samples must be explicitly set (> 0) when "
              "overlap-save chunking is active -- bounds dma_input's "
              "whole-stream size");
          return FALSE;
        }
        kernel->input_buf_size =
            (guint) (kernel->max_stream_samples * sizeof (gint16));
      } else {
        kernel->input_buf_size =
            (guint) (2 * kernel->chunk_samples * sizeof (gint16));
      }
      break;
    case DSP_OP_ISTFT:
    case DSP_OP_DEINT_INTERLEAVE:
      break;
    default:
      GST_WARNING_OBJECT (kernel, "Unknown msg-type: 0x%04x", kernel->msg_type);
      break;
  }

  kernel->rpmsg_chan =
      gst_ti_rpmsg_chan_acquire (kernel->rproc_id, kernel->remote_ep);
  if (!kernel->rpmsg_chan) {
    GST_ERROR_OBJECT (kernel, "Failed to acquire rpmsg channel");
    return FALSE;
  }

  /* Allocate DMA buffers */
  int r1 = dmabuf_heap_init ((char *) "linux,cma", kernel->input_buf_size,
      kernel->rproc_device, &kernel->dma_input);
  int r2 = dmabuf_heap_init ((char *) "linux,cma", kernel->output_buf_size,
      kernel->rproc_device, &kernel->dma_output);

  if (r1 != 0 || r2 != 0) {
    GST_ERROR_OBJECT (kernel, "DMA alloc failed (r1=%d r2=%d)", r1, r2);
    if (r1 == 0)
      dmabuf_heap_destroy (&kernel->dma_input);
    if (r2 == 0)
      dmabuf_heap_destroy (&kernel->dma_output);
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

  GST_INFO_OBJECT (kernel,
      "msg-type=0x%04x window-frames=%u dma_input=%u@0x%08lx "
      "dma_output=%u@0x%08lx", kernel->msg_type, kernel->window_frames,
      kernel->input_buf_size, (unsigned long) kernel->dma_input.phys_addr,
      kernel->output_buf_size, (unsigned long) kernel->dma_output.phys_addr);

  gst_dsp_kernel_setup_dma_output_pool (kernel);

  return TRUE;
}

static gboolean
gst_dsp_kernel_stop (GstBaseTransform * trans)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);

  gst_clear_object (&kernel->dma_input_allocator);

  if (kernel->dma_output_pool) {
    gst_buffer_pool_set_active (kernel->dma_output_pool, FALSE);
    gst_object_unref (kernel->dma_output_pool);
    kernel->dma_output_pool = NULL;
  }
  kernel->dma_output_pool_ok = FALSE;
  gst_clear_object (&kernel->dma_output_allocator);

  if (kernel->adopted_output_pool) {
    gst_buffer_pool_set_active (kernel->adopted_output_pool, FALSE);
    gst_object_unref (kernel->adopted_output_pool);
    kernel->adopted_output_pool = NULL;
  }
  kernel->adopted_output_target = NULL;

  if (kernel->dma_allocated) {
    dmabuf_heap_destroy (&kernel->dma_input);
    dmabuf_heap_destroy (&kernel->dma_output);
    kernel->dma_allocated = FALSE;
  }

  kernel->input_buffer_size = 0;

  if (kernel->collected_dma_allocated) {
    dmabuf_heap_destroy (&kernel->collected_dma);
    kernel->collected_dma_allocated = FALSE;
  }
  memset (&kernel->collected_dma, 0, sizeof (kernel->collected_dma));
  kernel->collected_final_samples = 0;
  kernel->collected_dma_written = 0;
  kernel->chunks_received = 0;
  kernel->chunk_buffer_counter = 0;
  kernel->expected_n_chunks = 0;

  gst_ti_rpmsg_chan_release (kernel->rpmsg_chan);
  kernel->rpmsg_chan = NULL;

  return TRUE;
}

static gboolean
gst_dsp_kernel_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps = NULL;
  GstAllocationParams params;

  if (!kernel->dma_allocated || kernel->input_buf_size == 0) {
    return GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation
        (trans, decide_query, query);
  }

  if (!kernel->dma_input_allocator) {
    kernel->dma_input_allocator =
        GST_ALLOCATOR (g_object_new (GST_TYPE_DSP_KERNEL_ALLOCATOR, NULL));
    GST_DSP_KERNEL_ALLOCATOR (kernel->dma_input_allocator)->target =
        &kernel->dma_input;
  }

  gst_query_parse_allocation (query, &caps, NULL);

  pool = gst_buffer_pool_new ();
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, kernel->input_buf_size, 1,
      1);
  gst_allocation_params_init (&params);
  gst_buffer_pool_config_set_allocator (config, kernel->dma_input_allocator,
      &params);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_WARNING_OBJECT (kernel,
        "Failed to configure DMA-BUF input pool (size=%u)",
        kernel->input_buf_size);
    gst_object_unref (pool);
    return GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation
        (trans, decide_query, query);
  }

  gst_query_add_allocation_pool (query, pool, kernel->input_buf_size, 1, 1);
  gst_query_add_allocation_param (query, kernel->dma_input_allocator, &params);
  gst_object_unref (pool);

  GST_INFO_OBJECT (kernel, "Proposed DMA-BUF input pool (fd=%d, size=%u)",
      kernel->dma_input.dma_buf_fd, kernel->input_buf_size);

  return TRUE;
}

static gboolean
gst_dsp_kernel_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);
  gint n_pools = gst_query_get_n_allocation_pools (query);
  gint i;

  if (kernel->adopted_output_pool) {
    gst_buffer_pool_set_active (kernel->adopted_output_pool, FALSE);
    gst_object_unref (kernel->adopted_output_pool);
    kernel->adopted_output_pool = NULL;
  }
  kernel->adopted_output_target = NULL;

  for (i = 0; i < n_pools; i++) {
    GstBufferPool *candidate = NULL;
    guint size = 0;
    guint pool_min = 0;
    guint pool_max = 0;
    GstStructure *config;
    GstAllocator *allocator = NULL;
    GstAllocationParams params;

    gst_query_parse_nth_allocation_pool (query, i, &candidate, &size,
        &pool_min, &pool_max);
    if (!candidate) {
      continue;
    }

    if (i != 0) {
      gst_object_unref (candidate);
      continue;
    }

    config = gst_buffer_pool_get_config (candidate);
    gst_buffer_pool_config_get_allocator (config, &allocator, &params);
    gst_structure_free (config);

    if (allocator && GST_IS_DSP_KERNEL_ALLOCATOR (allocator) &&
        GST_DSP_KERNEL_ALLOCATOR (allocator)->target &&
        size == kernel->output_buf_size) {
      struct dma_buf_params *target =
          GST_DSP_KERNEL_ALLOCATOR (allocator)->target;

      if (!gst_buffer_pool_is_active (candidate) &&
          !gst_buffer_pool_set_active (candidate, TRUE)) {
        GST_WARNING_OBJECT (kernel,
            "Failed to activate downstream-proposed pool \"%s\", not adopting",
            GST_OBJECT_NAME (candidate));
        gst_object_unref (candidate);
        continue;
      }

      kernel->adopted_output_pool = candidate;
      kernel->adopted_output_target = target;
      GST_INFO_OBJECT (kernel,
          "Adopted downstream dma_input pool \"%s\" (phys=0x%08lx, fd=%d)",
          GST_OBJECT_NAME (candidate), (unsigned long) target->phys_addr,
          target->dma_buf_fd);

      gst_query_set_nth_allocation_pool (query, 0, NULL, size, pool_min,
          pool_max);
    } else {
      gst_object_unref (candidate);
    }
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);
}

/* True if buf's payload is already the dma_input region (acquired from
 * the pool proposed in gst_dsp_kernel_propose_allocation()). */
static gboolean
gst_dsp_kernel_input_is_dma_input (GstDspKernel * kernel, GstBuffer * buf)
{
  GstMemory *mem;

  if (!kernel->dma_allocated || gst_buffer_n_memory (buf) != 1) {
    return FALSE;
  }

  mem = gst_buffer_peek_memory (buf, 0);

  return gst_is_dmabuf_memory (mem) &&
      gst_dmabuf_memory_get_fd (mem) == kernel->dma_input.dma_buf_fd;
}

static gboolean
gst_dsp_kernel_acquire_dma_output (GstDspKernel * kernel, gsize size,
    GstBuffer ** out_buf, GstMapInfo * out_map)
{
  GstBuffer *buf;

  if (!kernel->dma_output_pool_ok || size != kernel->output_buf_size) {
    return FALSE;
  }

  if (gst_buffer_pool_acquire_buffer (kernel->dma_output_pool, &buf,
          NULL) != GST_FLOW_OK) {
    GST_WARNING_OBJECT (kernel, "Failed to acquire from dma_output pool");
    return FALSE;
  }

  if (!gst_buffer_map (buf, out_map, GST_MAP_WRITE)) {
    GST_WARNING_OBJECT (kernel, "Failed to map buffer from dma_output pool");
    gst_buffer_unref (buf);
    return FALSE;
  }

  *out_buf = buf;
  return TRUE;
}

static gboolean
gst_dsp_kernel_acquire_output_buffer (GstDspKernel * kernel, gsize size,
    GstBuffer ** out_buf, GstMapInfo * out_map,
    struct dma_buf_params **out_target)
{
  if (kernel->adopted_output_pool && kernel->adopted_output_target &&
      size == kernel->output_buf_size) {
    GstBuffer *buf = NULL;

    if (gst_buffer_pool_acquire_buffer (kernel->adopted_output_pool, &buf,
            NULL) != GST_FLOW_OK) {
      GST_WARNING_OBJECT (kernel,
          "Failed to acquire from adopted downstream pool");
    } else if (!gst_buffer_map (buf, out_map, GST_MAP_WRITE)) {
      GST_WARNING_OBJECT (kernel,
          "Failed to map buffer from adopted downstream pool");
      gst_buffer_unref (buf);
    } else {
      *out_buf = buf;
      *out_target = kernel->adopted_output_target;
      return TRUE;
    }
  }

  if (gst_dsp_kernel_acquire_dma_output (kernel, size, out_buf, out_map)) {
    *out_target = &kernel->dma_output;
    return TRUE;
  }

  return FALSE;
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

/* Process one complete window immediately and push it downstream, for
 * live/streaming sources (e.g. a microphone) that never send EOS. */
static GstFlowReturn
dsp_kernel_process_stream_window (GstDspKernel * kernel,
    GstBaseTransform * trans)
{
  guint model_elems = gst_dsp_kernel_get_model_elems (kernel);
  gsize bytes_per_frame = model_elems * sizeof (float);
  gsize bytes_per_chunk = kernel->window_frames * bytes_per_frame;
  gsize offset_bytes = 0;
  guint num_batches =
      (kernel->window_frames + kernel->batch_size - 1) / kernel->batch_size;
  guint batch_idx;
  GstPad *srcpad;
  GstBuffer *out_buf = NULL;
  GstMapInfo out_map;
  GstFlowReturn push_ret;
  struct dma_buf_params *output_target = &kernel->dma_output;

  if (!gst_dsp_kernel_acquire_output_buffer (kernel, bytes_per_chunk,
          &out_buf, &out_map, &output_target)) {
    GST_ERROR_OBJECT (kernel,
        "STFT: stream window: failed to acquire pool-backed output buffer ");
    return GST_FLOW_ERROR;
  }

  for (batch_idx = 0; batch_idx < num_batches; batch_idx++) {
    guint frame_start = batch_idx * kernel->batch_size;
    guint frames_in_batch = MIN (kernel->batch_size,
        kernel->window_frames - frame_start);
    gsize sample_offset = frame_start * kernel->hop_size;
    struct stft_istft_msg req = { }, resp = { };
    gsize batch_spectral_bytes;

    req.hdr.type = kernel->msg_type;
    req.hdr.seq = kernel->sequence_number++;
    req.hdr.len = sizeof (req);
    req.selected_model = kernel->selected_model;
    req.input_buffer = (uint32_t) (kernel->dma_input.phys_addr +
        sample_offset * sizeof (gint16));
    req.output_buffer = (uint32_t) (output_target->phys_addr + offset_bytes);
    req.input_frame = frames_in_batch;
    req.output_frame = frames_in_batch;

    if (dsp_kernel_send_recv_stft (kernel, &req, &resp) != GST_FLOW_OK) {
      GST_ERROR_OBJECT (kernel, "STFT: stream window batch %u failed",
          batch_idx + 1);
      gst_buffer_unmap (out_buf, &out_map);
      gst_buffer_unref (out_buf);
      return GST_FLOW_ERROR;
    }

    if (resp.output_frame != frames_in_batch) {
      GST_ERROR_OBJECT (kernel,
          "STFT: stream window batch %u: firmware returned output_frame=%u, "
          "expected %u - refusing to trust it for a buffer copy size",
          batch_idx + 1, resp.output_frame, frames_in_batch);
      gst_buffer_unmap (out_buf, &out_map);
      gst_buffer_unref (out_buf);
      return GST_FLOW_ERROR;
    }

    batch_spectral_bytes = resp.output_frame * model_elems * sizeof (float);
    dmabuf_sync (output_target->dma_buf_fd, DMA_BUF_SYNC_START);
    dmabuf_sync (output_target->dma_buf_fd, DMA_BUF_SYNC_END);
    offset_bytes += batch_spectral_bytes;
  }

  gst_buffer_unmap (out_buf, &out_map);
  gst_buffer_set_size (out_buf, bytes_per_chunk);

  srcpad = gst_element_get_static_pad (GST_ELEMENT (trans), "src");
  push_ret = gst_pad_push (srcpad, out_buf);
  gst_object_unref (srcpad);

  if (push_ret != GST_FLOW_OK && push_ret != GST_FLOW_NOT_LINKED) {
    GST_WARNING_OBJECT (kernel, "STFT: stream window push failed: %s",
        gst_flow_get_name (push_ret));
  }

  return push_ret;
}

/* STFT transform: accumulate audio. Overlap-save models process at EOS via
 * dsp_kernel_process_chunks(); plain-windowing models push each complete
 * window immediately via dsp_kernel_process_stream_window(). outbuf stays empty. */
static GstFlowReturn
dsp_kernel_transform_stft (GstDspKernel * kernel, GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstMapInfo map_info;
  if (!gst_buffer_map (inbuf, &map_info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (kernel, "Failed to map buffer");
    return GST_FLOW_ERROR;
  }

  const gint16 *audio_in = (const gint16 *) map_info.data;
  gsize incoming_samples = map_info.size / sizeof (gint16);
  gsize capacity_samples = kernel->input_buf_size / sizeof (gint16);

  if (kernel->input_buffer_size + incoming_samples > capacity_samples) {
    GST_ERROR_OBJECT (kernel, "Invalid incoming buffer");
    gst_buffer_unmap (inbuf, &map_info);
    gst_buffer_set_size (outbuf, 0);
    return GST_FLOW_ERROR;
  }

  dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
  memcpy ((gint16 *) kernel->dma_input.kern_addr + kernel->input_buffer_size,
      audio_in, incoming_samples * sizeof (gint16));
  dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);
  kernel->input_buffer_size += incoming_samples;

  gst_buffer_unmap (inbuf, &map_info);

  if (kernel->overlap_frames == 0) {
    while (kernel->input_buffer_size >= kernel->chunk_samples) {
      GstFlowReturn ret = dsp_kernel_process_stream_window (kernel, trans);

      if (ret != GST_FLOW_OK && ret != GST_FLOW_NOT_LINKED) {
        gst_buffer_set_size (outbuf, 0);
        return ret;
      }

      /* Shift remaining buffered samples down to the front of dma_input */
      kernel->input_buffer_size -= kernel->chunk_samples;
      if (kernel->input_buffer_size > 0) {
        dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
        memmove ((gint16 *) kernel->dma_input.kern_addr,
            (gint16 *) kernel->dma_input.kern_addr + kernel->chunk_samples,
            kernel->input_buffer_size * sizeof (gint16));
        dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);
      }
    }
  }

  gst_buffer_set_size (outbuf, 0);

  return GST_FLOW_OK;
}

static void
dsp_kernel_istft_chunk_keep_range (GstDspKernel * kernel, gsize chunk_idx,
    gsize n_chunks, gsize * keep_frame_start, gsize * keep_frame_end)
{
  if (n_chunks <= 1) {
    *keep_frame_start = 0;
    *keep_frame_end = kernel->window_frames;
  } else if (chunk_idx == 0) {
    *keep_frame_start = 0;
    *keep_frame_end = kernel->window_frames - kernel->t_frames;
  } else if (chunk_idx == n_chunks - 1) {
    *keep_frame_start = kernel->t_frames;
    *keep_frame_end = kernel->window_frames;
  } else {
    *keep_frame_start = kernel->t_frames;
    *keep_frame_end = kernel->window_frames - kernel->t_frames;
  }
}

/* Total samples collected_dma needs (before the final padding trim). Sums
 * each chunk's keep-window width so this can't disagree with the dispatch logic. */
static gsize
dsp_kernel_istft_total_final_samples (GstDspKernel * kernel, gsize n_chunks)
{
  gsize total = 0;
  gsize c;

  for (c = 0; c < n_chunks; c++) {
    gsize keep_frame_start, keep_frame_end;

    dsp_kernel_istft_chunk_keep_range (kernel, c, n_chunks, &keep_frame_start,
        &keep_frame_end);
    if (keep_frame_end > keep_frame_start) {
      total += (keep_frame_end - keep_frame_start) * kernel->hop_size;
    }
  }

  return total;
}

/* GstMemory free-func for the final chunk's outbuf; owns a heap copy of
 * collected_dma since kernel->collected_dma is reset before this fires. */
static void
dsp_kernel_collected_dma_free (gpointer data)
{
  struct dma_buf_params *owned = (struct dma_buf_params *) data;

  dmabuf_heap_destroy (owned);
  g_free (owned);
}

/* Frees collected_dma and resets chunk-collection state on a mid-stream
 * abort, so the next stream starts clean. Not used on the success path --
 * there ownership transfers to outbuf's GstMemory instead. */
static void
dsp_kernel_istft_abort_collected_dma (GstDspKernel * kernel)
{
  if (kernel->collected_dma_allocated) {
    dmabuf_heap_destroy (&kernel->collected_dma);
    kernel->collected_dma_allocated = FALSE;
  }
  memset (&kernel->collected_dma, 0, sizeof (kernel->collected_dma));
  kernel->collected_final_samples = 0;
  kernel->collected_dma_written = 0;
  kernel->chunks_received = 0;
  kernel->chunk_buffer_counter = 0;
  kernel->expected_n_chunks = 0;
}

static GstFlowReturn
dsp_kernel_transform_istft_chunked (GstDspKernel * kernel, GstBuffer * inbuf,
    GstBuffer * outbuf, GstMapInfo * map_info, gsize total_spectral_bytes,
    gsize chunk_idx, gsize n_chunks)
{
  if (chunk_idx == 0) {
    gsize total_final_samples =
        dsp_kernel_istft_total_final_samples (kernel, n_chunks);
    guint32 collected_bytes = (guint32) (total_final_samples * sizeof (gint16));

    if (dmabuf_heap_init ((char *) "linux,cma", collected_bytes,
            kernel->rproc_device, &kernel->collected_dma) != 0) {
      GST_ERROR_OBJECT (kernel,
          "ISTFT chunked: failed to allocate %u-byte collected_dma for this "
          "stream's final output (n_chunks=%zu, total_final_samples=%zu) -- "
          "no fallback, failing this stream", collected_bytes, n_chunks,
          total_final_samples);
      gst_buffer_unmap (inbuf, map_info);
      return GST_FLOW_ERROR;
    }
    if (kernel->collected_dma.phys_addr > G_MAXUINT32) {
      GST_ERROR_OBJECT (kernel,
          "ISTFT chunked: collected_dma physical address exceeds 32 bits -- "
          "firmware wire protocol only carries 32-bit addresses");
      dmabuf_heap_destroy (&kernel->collected_dma);
      gst_buffer_unmap (inbuf, map_info);
      return GST_FLOW_ERROR;
    }

    kernel->collected_dma_allocated = TRUE;
    kernel->collected_final_samples = total_final_samples;
    kernel->collected_dma_written = 0;
    kernel->chunks_received = 0;
    GST_INFO_OBJECT (kernel,
        "ISTFT chunked: allocated collected_dma: %u bytes (%zu samples) @ "
        "phys=0x%08lx fd=%d for this stream (n_chunks=%zu)", collected_bytes,
        total_final_samples, (unsigned long) kernel->collected_dma.phys_addr,
        kernel->collected_dma.dma_buf_fd, n_chunks);
  }

  if (!kernel->collected_dma_allocated) {
    /* Guards against an earlier chunk in this stream having already failed. */
    GST_ERROR_OBJECT (kernel,
        "ISTFT chunked: collected_dma not allocated for chunk %zu/%zu "
        "(stream already failed?)", chunk_idx + 1, n_chunks);
    gst_buffer_unmap (inbuf, map_info);
    return GST_FLOW_ERROR;
  }

  gsize keep_frame_start, keep_frame_end;
  dsp_kernel_istft_chunk_keep_range (kernel, chunk_idx, n_chunks,
      &keep_frame_start, &keep_frame_end);

  gsize cut_points[4];
  guint n_cuts = 0;
  cut_points[n_cuts++] = 0;
  if (keep_frame_start > 0 && keep_frame_start < kernel->window_frames) {
    cut_points[n_cuts++] = keep_frame_start;
  }
  if (keep_frame_end > keep_frame_start &&
      keep_frame_end < kernel->window_frames) {
    cut_points[n_cuts++] = keep_frame_end;
  }
  cut_points[n_cuts++] = kernel->window_frames;

  guint bins_per_frame = gst_dsp_kernel_get_model_elems (kernel);
  gsize spectral_offset = 0;
  guint seg;

  gsize chunk_base = kernel->collected_dma_written;

  GST_INFO_OBJECT (kernel,
      "ISTFT chunked: chunk %zu/%zu keep=[%zu:%zu) window_frames=%u "
      "batch_size=%u chunk_base=%zu", chunk_idx + 1, n_chunks,
      keep_frame_start, keep_frame_end, kernel->window_frames,
      kernel->batch_size, chunk_base);

  for (seg = 0; seg + 1 < n_cuts; seg++) {
    gsize seg_start = cut_points[seg];
    gsize seg_end = cut_points[seg + 1];
    gboolean seg_kept = keep_frame_end > keep_frame_start &&
        seg_start >= keep_frame_start && seg_end <= keep_frame_end;
    gsize frame_start;

    for (frame_start = seg_start; frame_start < seg_end;
        frame_start += kernel->batch_size) {
      guint frames_in_batch =
          (guint) MIN (kernel->batch_size, seg_end - frame_start);
      gsize batch_spectral_bytes =
          frames_in_batch * bins_per_frame * sizeof (float);

      if (spectral_offset + batch_spectral_bytes > total_spectral_bytes) {
        batch_spectral_bytes = (spectral_offset < total_spectral_bytes) ?
            (total_spectral_bytes - spectral_offset) : 0;
      }

      guint64 batch_input_phys_addr =
          (guint64) kernel->dma_input.phys_addr + spectral_offset;
      guint64 batch_output_phys_addr;
      gsize final_sample_offset = 0;

      if (seg_kept) {
        gsize local_keep_offset =
            (frame_start - keep_frame_start) * kernel->hop_size;
        final_sample_offset = chunk_base + local_keep_offset;
        batch_output_phys_addr = kernel->collected_dma.phys_addr +
            final_sample_offset * sizeof (gint16);
      } else {
        batch_output_phys_addr = kernel->dma_output.phys_addr;
      }

      dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
      dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);

      if (seg_kept) {
        dmabuf_sync (kernel->collected_dma.dma_buf_fd, DMA_BUF_SYNC_START);
      }

      struct stft_istft_msg req = { }, resp = { };
      req.hdr.type = kernel->msg_type;
      req.hdr.seq = kernel->sequence_number++;
      req.hdr.len = sizeof (req);
      req.selected_model = kernel->selected_model;
      req.input_buffer = (uint32_t) batch_input_phys_addr;
      req.output_buffer = (uint32_t) batch_output_phys_addr;
      req.input_frame = frames_in_batch;
      req.output_frame = frames_in_batch;

      GST_INFO_OBJECT (kernel,
          "[ISTFT chunked] chunk %zu/%zu seg[%zu:%zu) frames=%u %s -> "
          "out_phys=0x%08x", chunk_idx + 1, n_chunks, frame_start,
          frame_start + frames_in_batch, frames_in_batch,
          seg_kept ? "KEEP" : "discard", (uint32_t) batch_output_phys_addr);

      if (dsp_kernel_send_recv_stft (kernel, &req, &resp) != GST_FLOW_OK) {
        dsp_kernel_istft_abort_collected_dma (kernel);
        gst_buffer_unmap (inbuf, map_info);
        return GST_FLOW_ERROR;
      }

      if (resp.output_frame != frames_in_batch) {
        GST_ERROR_OBJECT (kernel,
            "ISTFT chunked: chunk %zu/%zu seg[%zu:%zu): firmware returned "
            "output_frame=%u, expected %u", chunk_idx + 1, n_chunks,
            frame_start, frame_start + frames_in_batch, resp.output_frame,
            frames_in_batch);
        dsp_kernel_istft_abort_collected_dma (kernel);
        gst_buffer_unmap (inbuf, map_info);
        return GST_FLOW_ERROR;
      }

      if (seg_kept) {
        dmabuf_sync (kernel->collected_dma.dma_buf_fd, DMA_BUF_SYNC_END);
      }
      spectral_offset += batch_spectral_bytes;
    }
  }

  if (keep_frame_end > keep_frame_start) {
    kernel->collected_dma_written =
        chunk_base + (keep_frame_end - keep_frame_start) * kernel->hop_size;
  }

  kernel->chunks_received++;
  kernel->chunk_buffer_counter++;

  GST_INFO_OBJECT (kernel,
      "ISTFT chunked: chunk %zu/%zu collected (received %zu/%zu, "
      "collected_dma_written=%zu/%zu samples)", chunk_idx + 1, n_chunks,
      kernel->chunks_received, n_chunks, kernel->collected_dma_written,
      kernel->collected_final_samples);

  gst_buffer_unmap (inbuf, map_info);

  if (kernel->chunks_received != n_chunks) {
    gst_buffer_set_size (outbuf, 0);
    return GST_FLOW_OK;
  }

  if (kernel->collected_dma_written != kernel->collected_final_samples) {
    GST_ERROR_OBJECT (kernel,
        "ISTFT chunked: collected_dma_written=%zu != collected_final_samples="
        "%zu -- accumulation bug, refusing to hand downstream a buffer sized "
        "from a possibly-wrong written count", kernel->collected_dma_written,
        kernel->collected_final_samples);
    dsp_kernel_istft_abort_collected_dma (kernel);
    return GST_FLOW_ERROR;
  }

  gsize final_samples = kernel->collected_dma_written;
  if (kernel->padded_samples_added > 0 &&
      final_samples >= kernel->padded_samples_added) {
    final_samples -= kernel->padded_samples_added;
  }
  gsize output_bytes = final_samples * sizeof (gint16);

  GST_INFO_OBJECT (kernel,
      "ISTFT chunked: all %zu chunks collected, %zu samples (%zu bytes)",
      n_chunks, final_samples, output_bytes);

  struct dma_buf_params *owned = g_new (struct dma_buf_params, 1);
  *owned = kernel->collected_dma;

  GstMemory *new_mem = gst_memory_new_wrapped ((GstMemoryFlags) 0,
      owned->kern_addr, kernel->collected_final_samples * sizeof (gint16), 0,
      output_bytes, owned, dsp_kernel_collected_dma_free);

  gst_buffer_remove_all_memory (outbuf);
  gst_buffer_append_memory (outbuf, new_mem);
  gst_buffer_set_size (outbuf, output_bytes);

  kernel->collected_dma_allocated = FALSE;
  memset (&kernel->collected_dma, 0, sizeof (kernel->collected_dma));
  kernel->collected_final_samples = 0;
  kernel->collected_dma_written = 0;
  kernel->chunks_received = 0;
  kernel->chunk_buffer_counter = 0;
  kernel->expected_n_chunks = 0;

  return GST_FLOW_OK;
}

/* ISTFT transform with overlap-add. outbuf gets real output when a window
 * (or the last chunk of a sequence) is ready, else stays empty. */
static GstFlowReturn
dsp_kernel_transform_istft (GstDspKernel * kernel, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstMapInfo map_info;
  if (!gst_buffer_map (inbuf, &map_info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (kernel, "Failed to map buffer");
    return GST_FLOW_ERROR;
  }

  gsize total_spectral_bytes = map_info.size;

  /* Handle empty input (STFT still accumulating) - pass through empty buffer */
  if (total_spectral_bytes == 0) {
    gst_buffer_set_size (outbuf, 0);
    gst_buffer_unmap (inbuf, &map_info);
    return GST_FLOW_OK;
  }

  GST_INFO_OBJECT (kernel, "ISTFT: Processing %zu spectral bytes",
      total_spectral_bytes);

  if (!gst_dsp_kernel_input_is_dma_input (kernel, inbuf)) {
    GST_ERROR_OBJECT (kernel, "ISTFT: input buffer is not dma_input");
    gst_buffer_unmap (inbuf, &map_info);
    return GST_FLOW_ERROR;
  }

  /* A chunked stream is handled entirely by dsp_kernel_transform_istft_chunked(). */
  gsize chunk_idx = kernel->chunk_buffer_counter;
  gsize n_chunks = kernel->expected_n_chunks;

  if (n_chunks > 1) {
    return dsp_kernel_transform_istft_chunked (kernel, inbuf, outbuf,
        &map_info, total_spectral_bytes, chunk_idx, n_chunks);
  }

  /* Non-chunk mode: single window, output immediately. */
  guint num_batches =
      (kernel->window_frames + kernel->batch_size - 1) / kernel->batch_size;
  gsize max_output_samples = kernel->window_frames * kernel->hop_size;
  gsize out_written_samples = 0;
  gsize spectral_offset = 0;

  GstBuffer *istft_out_buf = NULL;
  GstMapInfo istft_out_map;
  struct dma_buf_params *istft_output_target = &kernel->dma_output;
  if (!gst_dsp_kernel_acquire_output_buffer (kernel,
          max_output_samples * sizeof (gint16), &istft_out_buf, &istft_out_map,
          &istft_output_target)) {
    GST_ERROR_OBJECT (kernel,
        "ISTFT: failed to acquire pool-backed output buffer ");
    gst_buffer_unmap (inbuf, &map_info);
    return GST_FLOW_ERROR;
  }

  /* Process in batches */
  for (guint batch_idx = 0; batch_idx < num_batches; batch_idx++) {
    guint frame_start = batch_idx * kernel->batch_size;
    guint frames_in_batch =
        MIN (kernel->batch_size, kernel->window_frames - frame_start);
    guint bins_per_frame = gst_dsp_kernel_get_model_elems (kernel);
    gsize batch_spectral_bytes =
        frames_in_batch * bins_per_frame * sizeof (float);

    if (spectral_offset + batch_spectral_bytes > total_spectral_bytes) {
      batch_spectral_bytes = total_spectral_bytes - spectral_offset;
    }

    guint64 batch_input_phys_addr =
        (guint64) kernel->dma_input.phys_addr + spectral_offset;

    dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
    dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);

    /* Send to DSP */
    struct stft_istft_msg req = { }, resp = { };
    req.hdr.type = kernel->msg_type;
    req.hdr.seq = kernel->sequence_number++;
    req.hdr.len = sizeof (req);
    req.selected_model = kernel->selected_model;        /* param0: firmware ModelId */
    req.input_buffer = (uint32_t) batch_input_phys_addr;
    req.output_buffer = (uint32_t) (istft_output_target->phys_addr +
        out_written_samples * sizeof (gint16));
    req.input_frame = frames_in_batch;  /* Number of frames in this batch */
    req.output_frame = frames_in_batch; /* Must equal input_frame */

    if (dsp_kernel_send_recv_stft (kernel, &req, &resp) != GST_FLOW_OK) {
      gst_buffer_unmap (istft_out_buf, &istft_out_map);
      gst_buffer_unref (istft_out_buf);
      gst_buffer_unmap (inbuf, &map_info);
      return GST_FLOW_ERROR;
    }

    if (resp.output_frame != frames_in_batch) {
      GST_ERROR_OBJECT (kernel,
          "ISTFT: batch %u: firmware returned output_frame=%u, expected %u "
          "- refusing to trust it for a buffer copy size", batch_idx + 1,
          resp.output_frame, frames_in_batch);
      gst_buffer_unmap (istft_out_buf, &istft_out_map);
      gst_buffer_unref (istft_out_buf);
      gst_buffer_unmap (inbuf, &map_info);
      return GST_FLOW_ERROR;
    }

    gsize batch_audio_samples = resp.output_frame * kernel->hop_size;
    dmabuf_sync (istft_output_target->dma_buf_fd, DMA_BUF_SYNC_START);
    dmabuf_sync (istft_output_target->dma_buf_fd, DMA_BUF_SYNC_END);

    out_written_samples += batch_audio_samples;
    spectral_offset += batch_spectral_bytes;
  }

  GST_INFO_OBJECT (kernel,
      "ISTFT: Complete - input=%zu bytes output=%zu samples (%zu bytes)",
      total_spectral_bytes, out_written_samples,
      out_written_samples * sizeof (gint16));

  /* n_chunks <= 1 here, so no overlap-save trimming needed. */
  gsize final_output_samples = out_written_samples;

  /* Non-chunk mode: output immediately */
  gsize output_bytes = final_output_samples * sizeof (gint16);

  GST_INFO_OBJECT (kernel, "ISTFT: outputting %zu samples (%zu bytes, %.3fs)",
      final_output_samples, output_bytes,
      (gdouble) final_output_samples / kernel->sample_rate);

  gst_buffer_unmap (inbuf, &map_info);
  gst_buffer_unmap (istft_out_buf, &istft_out_map);
  gst_buffer_set_size (istft_out_buf, output_bytes);
  GstMemory *new_mem = gst_buffer_get_all_memory (istft_out_buf);
  gst_buffer_unref (istft_out_buf);

  /* Replace buffer memory */
  gst_buffer_replace_all_memory (outbuf, new_mem);

  return GST_FLOW_OK;
}

/* Deinterleave/Interleave transform */
static GstFlowReturn
dsp_kernel_transform_deint_interleave (GstDspKernel * kernel,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstMapInfo map_info;
  if (!gst_buffer_map (inbuf, &map_info, GST_MAP_READ)) {
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
    gst_buffer_unmap (inbuf, &map_info);
    gst_buffer_set_size (outbuf, 0);
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

  if (!gst_dsp_kernel_input_is_dma_input (kernel, inbuf)) {
    GST_ERROR_OBJECT (kernel, "%s: input buffer is not dma_input", op_name);
    gst_buffer_unmap (inbuf, &map_info);
    return GST_FLOW_ERROR;
  }
  dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
  dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);

  /* Output size equals input size here, known up front, so the output
   * buffer/target can be acquired before the DSP round trip. */
  gsize output_size = input_size;
  GstBuffer *out_pool_buf = NULL;
  GstMapInfo out_pool_map;
  struct dma_buf_params *output_target = &kernel->dma_output;
  if (!gst_dsp_kernel_acquire_output_buffer (kernel, output_size,
          &out_pool_buf, &out_pool_map, &output_target)) {
    GST_ERROR_OBJECT (kernel,
        "%s: failed to acquire pool-backed output buffer ", op_name);
    gst_buffer_unmap (inbuf, &map_info);
    return GST_FLOW_ERROR;
  }

  /* Send to DSP using correct message structure */
  struct deint_interleave_msg req = { }, resp = { };
  req.hdr.type = kernel->msg_type;
  req.hdr.seq = kernel->sequence_number++;
  req.hdr.len = sizeof (req);
  req.input_buffer = (uint32_t) kernel->dma_input.phys_addr;
  req.output_buffer = (uint32_t) output_target->phys_addr;
  req.input_frame = kernel->window_frames;
  req.fft_size = kernel->fft_size;
  req.flag = kernel->interleave_direction;

  gst_buffer_unmap (inbuf, &map_info);

  if (dsp_kernel_send_recv_deint (kernel, &req, &resp) != GST_FLOW_OK) {
    gst_buffer_unmap (out_pool_buf, &out_pool_map);
    gst_buffer_unref (out_pool_buf);
    return GST_FLOW_ERROR;
  }

  dmabuf_sync (output_target->dma_buf_fd, DMA_BUF_SYNC_START);
  dmabuf_sync (output_target->dma_buf_fd, DMA_BUF_SYNC_END);

  gst_buffer_unmap (out_pool_buf, &out_pool_map);
  gst_buffer_set_size (out_pool_buf, (gssize) output_size);

  GstMemory *out_mem = gst_buffer_get_all_memory (out_pool_buf);
  gst_buffer_unref (out_pool_buf);
  gst_buffer_replace_all_memory (outbuf, out_mem);

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

  {
    gsize capacity_samples = kernel->input_buf_size / sizeof (gint16);

    if (kernel->total_padded_len > capacity_samples) {
      GST_ERROR_OBJECT (kernel,
          "STFT chunking: padded length %zu exceeds dma_input capacity "
          "%zu samples -- increase max-stream-samples",
          kernel->total_padded_len, capacity_samples);
      return GST_FLOW_ERROR;
    }

    if (kernel->total_padded_len > n_samples) {
      dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_START);
      memset ((gint16 *) kernel->dma_input.kern_addr + n_samples, 0,
          (kernel->total_padded_len - n_samples) * sizeof (gint16));
      dmabuf_sync (kernel->dma_input.dma_buf_fd, DMA_BUF_SYNC_END);
    }
  }

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

  gst_pad_push_event (srcpad, chunk_event);

  /* Process each chunk and push immediately (one chunk at a time to downstream) */
  for (gsize chunk_idx = 0; chunk_idx < n_chunks; chunk_idx++) {
    gsize chunk_offset = chunk_idx * kernel->hop_samples;

    GST_INFO_OBJECT (kernel, "STFT: chunk %zu/%zu, offset=%zu",
        chunk_idx + 1, n_chunks, chunk_offset);

    guint num_batches =
        (kernel->window_frames + kernel->batch_size - 1) / kernel->batch_size;
    GstBuffer *chunk_out_buf = NULL;
    GstMapInfo chunk_out_map;
    struct dma_buf_params *chunk_output_target = &kernel->dma_output;
    if (!gst_dsp_kernel_acquire_output_buffer (kernel, bytes_per_chunk,
            &chunk_out_buf, &chunk_out_map, &chunk_output_target)) {
      GST_ERROR_OBJECT (kernel,
          "STFT: chunk %zu: failed to acquire pool-backed output buffer ",
          chunk_idx + 1);
      gst_object_unref (srcpad);
      return GST_FLOW_ERROR;
    }
    gsize chunk_offset_bytes = 0;

    for (guint batch_idx = 0; batch_idx < num_batches; batch_idx++) {
      guint frame_start = batch_idx * kernel->batch_size;
      guint frames_in_batch = MIN (kernel->batch_size,
          kernel->window_frames - frame_start);
      gsize sample_offset = frame_start * kernel->hop_size;
      gsize global_sample_offset = chunk_offset + sample_offset;

      /* Send to DSP */
      struct stft_istft_msg req = { }, resp = { };
      req.hdr.type = kernel->msg_type;
      req.hdr.seq = kernel->sequence_number++;
      req.hdr.len = sizeof (req);
      req.selected_model = kernel->selected_model;      /* param0: firmware ModelId */
      req.input_buffer = (uint32_t) (kernel->dma_input.phys_addr +
          global_sample_offset * sizeof (gint16));
      req.output_buffer = (uint32_t) (chunk_output_target->phys_addr +
          chunk_offset_bytes);
      req.input_frame = frames_in_batch;
      req.output_frame = frames_in_batch;

      if (dsp_kernel_send_recv_stft (kernel, &req, &resp) != GST_FLOW_OK) {
        GST_ERROR_OBJECT (kernel, "STFT: Batch %u failed for chunk %zu",
            batch_idx + 1, chunk_idx + 1);
        gst_buffer_unmap (chunk_out_buf, &chunk_out_map);
        gst_buffer_unref (chunk_out_buf);
        gst_object_unref (srcpad);
        return GST_FLOW_ERROR;
      }

      if (resp.output_frame != frames_in_batch) {
        GST_ERROR_OBJECT (kernel,
            "STFT: chunk %zu batch %u: firmware returned output_frame=%u, "
            "expected %u - refusing to trust it for a buffer copy size",
            chunk_idx + 1, batch_idx + 1, resp.output_frame, frames_in_batch);
        gst_buffer_unmap (chunk_out_buf, &chunk_out_map);
        gst_buffer_unref (chunk_out_buf);
        gst_object_unref (srcpad);
        return GST_FLOW_ERROR;
      }

      guint out_bins_per_frame = gst_dsp_kernel_get_model_elems (kernel);
      gsize batch_spectral_bytes =
          resp.output_frame * out_bins_per_frame * sizeof (float);

      dmabuf_sync (chunk_output_target->dma_buf_fd, DMA_BUF_SYNC_START);
      dmabuf_sync (chunk_output_target->dma_buf_fd, DMA_BUF_SYNC_END);
      chunk_offset_bytes += batch_spectral_bytes;
    }

    gst_buffer_unmap (chunk_out_buf, &chunk_out_map);
    gst_buffer_set_size (chunk_out_buf, bytes_per_chunk);

    GstFlowReturn push_ret = gst_pad_push (srcpad, chunk_out_buf);
    if (push_ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (kernel,
          "STFT: Failed to push chunk %zu spectral output: %s",
          chunk_idx + 1, gst_flow_get_name (push_ret));
      gst_object_unref (srcpad);
      return push_ret;
    }
  }

  gst_object_unref (srcpad);

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

        GST_INFO_OBJECT (kernel,
            "ISTFT: chunk count event: n_chunks=%u t_frames=%u hop_size=%u",
            n_chunks, t_frames, hop_size);
      }
    }
  }

  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    if (kernel->msg_type == DSP_OP_STFT && kernel->input_buffer_size > 0) {
      GstFlowReturn ret = dsp_kernel_process_chunks (kernel, trans);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (kernel, "STFT: Chunk processing failed");
        return FALSE;           /* Let EOS propagate even on error */
      }
    }
  }

  if (GST_EVENT_TYPE (event) == GST_EVENT_FLUSH_STOP) {
    GST_INFO_OBJECT (kernel,
        "FLUSH_STOP: resetting STFT/ISTFT accumulation state");
    kernel->input_buffer_size = 0;
    kernel->chunks_received = 0;
    kernel->chunk_buffer_counter = 0;
    kernel->expected_n_chunks = 0;
    if (kernel->collected_dma_allocated) {
      dsp_kernel_istft_abort_collected_dma (kernel);
    }
  }

  /* Chain up to parent class event handler */
  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static GstFlowReturn
gst_dsp_kernel_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);

  switch (kernel->msg_type) {
    case DSP_OP_STFT:
      return dsp_kernel_transform_stft (kernel, trans, inbuf, outbuf);
    case DSP_OP_ISTFT:
      return dsp_kernel_transform_istft (kernel, inbuf, outbuf);
    case DSP_OP_DEINT_INTERLEAVE:
      return dsp_kernel_transform_deint_interleave (kernel, inbuf, outbuf);
    default:
      GST_ERROR_OBJECT (kernel, "Unknown operation type: 0x%04x",
          kernel->msg_type);
      return GST_FLOW_ERROR;
  }
}

/* Only used when decide_allocation doesn't negotiate a pool. Must be exact
 * for Deinterleave/Interleave; STFT/ISTFT outbuf gets resized/replaced later. */
static gboolean
gst_dsp_kernel_transform_size (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, gsize size, GstCaps * othercaps,
    gsize * othersize)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);

  if (kernel->msg_type == DSP_OP_DEINT_INTERLEAVE) {
    *othersize = size;
  } else {
    *othersize = kernel->output_buf_size > 0 ? kernel->output_buf_size : 1;
  }
  return TRUE;
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

static gboolean
gst_dsp_kernel_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstDspKernel *kernel = GST_DSP_KERNEL (trans);
  GstCaps *audio_caps[2] = { incaps, outcaps };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (audio_caps); i++) {
    GstStructure *s = gst_caps_get_structure (audio_caps[i], 0);
    gint rate;

    if (gst_structure_has_name (s, "audio/x-raw") &&
        gst_structure_get_int (s, "rate", &rate) && rate > 0) {
      kernel->sample_rate = (guint) rate;
      GST_INFO_OBJECT (kernel, "Negotiated sample rate: %u Hz",
          kernel->sample_rate);
      break;
    }
  }

  return TRUE;
}
