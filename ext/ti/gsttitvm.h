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

#ifndef __GST_TI_TVM_H__
#define __GST_TI_TVM_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#ifdef __cplusplus
extern "C" {
#endif

G_BEGIN_DECLS

/* Element type macros */
#define GST_TYPE_TI_TVM \
  (gst_ti_tvm_get_type())
#define GST_TI_TVM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TI_TVM,GstTiTvm))
#define GST_TI_TVM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TI_TVM,GstTiTvmClass))
#define GST_IS_TI_TVM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TI_TVM))
#define GST_IS_TI_TVM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TI_TVM))

typedef struct _GstTiTvm      GstTiTvm;
typedef struct _GstTiTvmClass GstTiTvmClass;

/* TVM inference performance data */
struct TiTvmPerformanceData {
    gint64 first_run_time;        /* First run latency (includes init) */
    gint64 *inference_times;      /* Array of inference times (excluding first run) */
};

/* GStreamer TI TVM element structure */
struct _GstTiTvm
{
    GstBaseTransform element;

    /* Properties */
    gchar *model_path;            /* Path to TVM artifacts directory */
    gchar *class_map_path;        /* Optional: YAML file of ordered class names for
                                    * live top-k printing (e.g. yamnet_class_map.yml).
                                    * Empty = printing disabled (default, no-op for
                                    * non-classification models like GCRN). */
    guint top_k;                  /* Number of top predictions to print per window */

    /* TVM runtime state */
    void *graph_executor;         /* TVM graph executor handle */
    void *set_input_func;         /* TVM set_input function */
    void *run_func;               /* TVM run function */
    void *get_output_func;        /* TVM get_output function */

    /* Auto-detected from deploy_graph.json */
    void *auto_input_shape;
    void *auto_output_shape;

    /* Input/output data */
    void *final_output;           /* Final inference output buffer */
    gsize output_num_floats;      /* Dynamic output size determined at inference time */

    /* tvm-model-daemon client state (preferred path: the DSP compute
     * channel only supports one client, and tvm-model-daemon already
     * owns it on boards where it is running). */
    gint daemon_fd;                /* fd to /var/run/tvm-inference.sock, -1 if not connected */
    gfloat *daemon_output_buf;     /* latest inference output received from the daemon */
    gsize daemon_output_buf_size;  /* allocated size (in floats) of daemon_output_buf */

    /* Performance tracking */
    struct TiTvmPerformanceData perf_data;

    /* Live top-k prediction printing (class-map-path) */
    gchar **class_names;          /* Ordered class names parsed from class-map-path */
    guint num_class_names;
    guint window_counter;         /* Windows processed so far, for "N/?" style logging */
};

struct _GstTiTvmClass
{
    GstBaseTransformClass parent_class;
};

/* Function declarations */
GType gst_ti_tvm_get_type (void);

/* Element property IDs */
enum
{
    PROP_0,
    PROP_MODEL_PATH,
    PROP_CLASS_MAP_PATH,
    PROP_TOP_K
};

/* Default values */
#define DEFAULT_MODEL_PATH ""
#define DEFAULT_CLASS_MAP_PATH ""
#define DEFAULT_TOP_K 3

G_END_DECLS

#ifdef __cplusplus
}
#endif

#endif /* __GST_TI_TVM_H__ */
