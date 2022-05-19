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

#ifndef _GST_TIOVX_SISO_H_
#define _GST_TIOVX_SISO_H_

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <TI/tivx.h>

G_BEGIN_DECLS

#define GST_TYPE_TIOVX_SISO   (gst_tiovx_siso_get_type())
G_DECLARE_DERIVABLE_TYPE (GstTIOVXSiso, gst_tiovx_siso, GST,
	TIOVX_SISO, GstBaseTransform)

/* Parameters settings to install in the vx_graph */
#define INPUT_PARAMETER_INDEX  0
#define OUTPUT_PARAMETER_INDEX 1
#define NUM_PARAMETERS         2

/* Number of channels constants */
#define MIN_NUM_CHANNELS 1
#define MAX_NUM_CHANNELS 16

/* BufferPool constants */
#define MIN_POOL_SIZE 2
#define MAX_POOL_SIZE 16

#define DEFAULT_NUM_CHANNELS MIN_NUM_CHANNELS

/**
 * GstTIOVXSisoClass:
 * @parent_class:   Element parent class
 * @init_module:        Required. Subclasses must override to init
 *                      the element-specific module.
 * @create_graph:       Required. Subclasses must override to init
 *                      the element-specific graph.
 * @get_node_info:      Required. Subclasses must override to return
                        node information
 *                      on the element-specific node parameters.
 * @release_buffer:     Required. Subclasses must override to release
 *                      vx_reference memory allocated.
 * @deinit_module:      Required. Subclasses must override to deinit
 *                      the element-specific module.
 * @compare_caps:       Optional. Subclasses must override to compare
 *                      caps based on pad direction. Returns TRUE if
 *                      caps are equal and FALSE otherwise
 *
 * Subclasses can override any of the available virtual methods.
 */
struct _GstTIOVXSisoClass
{
  GstBaseTransformClass parent_class;

  /*< public >*/
  /* virtual methods for subclasses */
  gboolean      (*init_module)              (GstTIOVXSiso *trans, vx_context context, GstCaps * in_caps,
					     GstCaps * out_caps, guint num_channels);

  gboolean      (*create_graph)             (GstTIOVXSiso *trans, vx_context context, vx_graph graph);

  gboolean      (*get_node_info)            (GstTIOVXSiso *trans, vx_object_array * input, vx_object_array * output, vx_reference * input_ref, vx_reference * output_ref,
					     vx_node * node, guint * input_param_index, guint * output_param_index);

  gboolean      (*release_buffer)           (GstTIOVXSiso *trans);

  gboolean      (*deinit_module)            (GstTIOVXSiso *trans, vx_context context);

  gboolean      (*compare_caps)             (GstTIOVXSiso *trans, GstCaps *caps1, GstCaps *caps2, GstPadDirection direction);
};

G_END_DECLS

#endif /* _GST_TIOVX_SISO_H_ */
