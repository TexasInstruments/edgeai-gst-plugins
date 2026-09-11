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
#ifndef __GST_DSP_KERNEL_H__
#define __GST_DSP_KERNEL_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <stdint.h>
#include "gsttirpmsgctx.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "dmabuf.h"
#ifdef __cplusplus
}
#endif

G_BEGIN_DECLS

#define GST_TYPE_DSP_KERNEL            (gst_dsp_kernel_get_type())
#define GST_DSP_KERNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),  GST_TYPE_DSP_KERNEL, GstDspKernel))
#define GST_DSP_KERNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),   GST_TYPE_DSP_KERNEL, GstDspKernelClass))
#define GST_IS_DSP_KERNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),  GST_TYPE_DSP_KERNEL))
#define GST_IS_DSP_KERNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),   GST_TYPE_DSP_KERNEL))

typedef struct _GstDspKernel      GstDspKernel;
typedef struct _GstDspKernelClass GstDspKernelClass;

/* DSP operation types */
typedef enum {
    DSP_OP_STFT           = 0x1020,  /* STFT analysis (requires accumulation) */
    DSP_OP_ISTFT          = 0x1030,  /* ISTFT synthesis (requires overlap-add) */
    DSP_OP_DEINT_INTERLEAVE = 0x1040,  /* Deinterleave/Interleave (interleave-direction: 0=deinterleave, 1=interleave) */
} DspOpType;

struct _GstDspKernel {
    GstBaseTransform parent;

    /* Configuration properties */
    gchar   *rproc_device;
    guint    rproc_id;
    guint    remote_ep;
    guint    msg_type;
    guint    msg_resp_type;
    guint    input_buf_size;
    guint    output_buf_size;
    guint    interleave_direction; /* For DSP_OP_DEINT_INTERLEAVE: 0=deinterleave, 1=interleave */
    guint    hop_size;          /* For STFT/ISTFT */
    guint    fft_size;          /* For STFT/ISTFT */
    guint    window_frames;     /* For STFT/ISTFT */
    guint    batch_size;        /* For STFT/ISTFT */
    guint    selected_model;    /* Firmware ModelId sent in STFT/ISTFT requests (2=GCRN default) */
    guint    model_elems;       /* Spectral elements/frame; 0 = derive from fft_size (GCRN formula) */
    gchar   *model_path;        /* Optional: artifacts dir, e.g. ".../artifacts_yamnet". When set,
                                  * selected-model/model-elems are derived from a known model name
                                  * in the path, overriding the properties above. */
    guint    overlap_frames_prop; /* User-configurable overlap-save overlap amount (frames).
                                    * Default 100 matches GCRN; other models needing overlap-save
                                    * chunking with a different overlap must set this explicitly. */
    gint     chunking_mode;     /* -1 = auto (CHUNKING_THRESHOLD heuristic on window_frames),
                                  * 0 = force plain windowing (no overlap-save),
                                  * 1 = force overlap-save chunking. */

    /* RPMsg and DMA */
    GstTiRpmsgChan *rpmsg_chan;
    guint32         sequence_number;
    struct dma_buf_params dma_input;
    struct dma_buf_params dma_output;
    gboolean        dma_allocated;


    /* Overlap-save chunking state */
    gint16  *input_buffer;               /* Buffer all audio until EOS */
    gsize    input_buffer_size;          /* Total input samples buffered */
    gsize    input_buffer_capacity;      /* Allocated capacity */

    /* Overlap-save parameters (calculated from window_frames, hop_size, batch_size) */
    gsize    overlap_frames;             /* OVERLAP_FRAMES = 100 */
    gsize    t_frames;                   /* T_FRAMES = overlap_frames / 2 = 50 */
    gsize    hop_frames;                 /* HOP_FRAMES = window_frames - overlap_frames */
    gsize    hop_samples;                /* HOP_SAMPLES = hop_frames * hop_size */
    gsize    chunk_samples;              /* CHUNK_SAMPLES = window_frames * hop_size */

    /* Chunking state */
    gsize    total_padded_len;           /* Total padded length needed */
    gsize    padded_samples_added;       /* Padding added at end */

    /* Chunk collection for overlap-save reconstruction */
    gint16  *collected_audio;            /* Collected trimmed audio from all chunks */
    gsize    collected_audio_size;       /* Total samples collected */
    gsize    collected_audio_capacity;   /* Allocated capacity */
    gsize    chunks_received;            /* Number of chunks received in ISTFT */

    /* Chunk count from upstream (received via event) */
    gsize    expected_n_chunks;          /* Number of chunks expected from STFT */
    gsize    chunk_buffer_counter;       /* Sequential counter for each buffer in ISTFT */

    /* Chunking control - auto-detected based on window_frames */
    gboolean enable_chunking;            /* TRUE if window_frames > CHUNKING_THRESHOLD */

};

struct _GstDspKernelClass {
    GstBaseTransformClass parent_class;
};

GType gst_dsp_kernel_get_type(void);

G_END_DECLS

#endif /* __GST_DSP_KERNEL_H__ */
