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

#ifndef _GST_TIOVX_MISO_H_
#define _GST_TIOVX_MISO_H_

#include <gst/gst.h>
#include <gst/base/gstaggregator.h>
#include <gst/video/video.h>
#include <TI/tivx.h>

G_BEGIN_DECLS

#define GST_TYPE_TIOVX_MISO   (gst_tiovx_miso_get_type())
G_DECLARE_DERIVABLE_TYPE (GstTIOVXMiso, gst_tiovx_miso, GST,
	TIOVX_MISO, GstAggregator)

/* Parameters settings to install in the vx_graph */
#define INPUT_PARAMETER_INDEX  0
#define OUTPUT_PARAMETER_INDEX 1
#define NUM_PARAMETERS         2

/* BufferPool constants */
#define MIN_POOL_SIZE 2
#define MAX_POOL_SIZE 16

/**
 * GstTIOVXMisoClass:
 * @parent_class:   Element parent class
 * 
 * @init_module:             Required. Subclasses must override to init
 *                           the element-specific module.
 * @create_graph:            Required. Subclasses must override to init
 *                           the element-specific graph.
 * @get_node_info:           Required. Subclasses must override to return
                             node information
 *                           on the element-specific node parameters.
 * @configure_module:        Optional. Subclasses may override to release
 *                           vx_image memory allocated and do module configuration
 *                           prior to starting the process graph if needed.
 * @release_buffer:          Required. Subclasses must override to release
 *                           vx_image memory allocated.
 * @deinit_module:           Required. Subclasses must override to deinit
 *                           the element-specific module.
 * @fixate_caps:             Optional. Subclasses may override to manage custom
 *                           implementation of caps events. Default
 *                           implementation is to use gst_caps_fixate() to obtain
 *                           caps that will be used in the sink pads.
 * @preprocess:              Optional. Subclasses may implement this function to
 *                           perform operations before processing
 * @postprocess:             Optional. Subclasses may implement this function to
 *                           perform operations after processing.
 * @add_outbuf_meta:         Optional. Subclasses may implement this function to
 *                           add custom meta to output buffer
 *
 * Subclasses can override any of the available virtual methods.
 */
struct _GstTIOVXMisoClass
{
  GstAggregatorClass parent_class;

  /*< public >*/
  /* virtual methods for subclasses */
  gboolean      (*init_module)              (GstTIOVXMiso *agg, vx_context context, GList* sink_pads_list, GstPad * src_pad, guint num_channels);

  gboolean      (*create_graph)             (GstTIOVXMiso *agg, vx_context context, vx_graph graph);

  gboolean      (*get_node_info)            (GstTIOVXMiso *agg, GList* sink_pads_list, GstPad * src_pad, vx_node * node, GList **queueable_objects);

  gboolean      (*configure_module)         (GstTIOVXMiso *agg);

  gboolean      (*release_buffer)           (GstTIOVXMiso *agg);

  gboolean      (*deinit_module)            (GstTIOVXMiso *agg);

  GstCaps *     (*fixate_caps)              (GstTIOVXMiso *self, GList * sink_caps_list, GstCaps *src_caps, gint *num_channels);

  gboolean      (*preprocess)               (GstTIOVXMiso *self);

  gboolean      (*postprocess)              (GstTIOVXMiso * self);

  gboolean      (*add_outbuf_meta)          (GstTIOVXMiso * self, GstBuffer * outbuf);
};

/* TIOVX Miso Pad */

#define GST_TYPE_TIOVX_MISO_PAD (gst_tiovx_miso_pad_get_type())
G_DECLARE_DERIVABLE_TYPE (GstTIOVXMisoPad, gst_tiovx_miso_pad, GST, TIOVX_MISO_PAD,
    GstAggregatorPad)

struct _GstTIOVXMisoPadClass
{
  GstAggregatorPad parent_class;
};

/**
 * gst_tiovx_miso_pad_set_params:
 * @pad: Pad where the parameters will be added
 * @array: Array to be used for processing
 * @exemplar: VX reference that this pad should use as reference for allocation
 * @graph_param_id: Parameter id that will be used to enqueue this parameter to the Vx Graph
 * @node_param_id: Parameter id that will be used to configure the node
 *
 * Sets an exemplar and a param id to the pad, these will be used for future configuration of the
 * given pad.
 *
 * @graph_param_id and @node_param_id can be both set to -1 in order to not use these for node
 * configuration.
 * 
 * If @array is null, then the transfer handle will be performed to @exemplar will be used instead.
 *
 * Returns: nothing
 */
void gst_tiovx_miso_pad_set_params (GstTIOVXMisoPad *pad, vx_object_array array,
  vx_reference* exemplar, gint graph_param_id, gint node_param_id);

/**
 * gst_tiovx_miso_pad_get_pool_size:
 * @pad: Pad to get pool size for
 *
 * get the pool size for a given pad
 *
 * Returns: pool size
 */
gint gst_tiovx_miso_pad_get_pool_size(GstTIOVXMisoPad *pad);

G_END_DECLS

#endif /* _GST_TIOVX_MISO_H_ */
