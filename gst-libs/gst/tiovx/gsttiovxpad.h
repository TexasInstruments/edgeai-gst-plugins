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


#ifndef __GST_TIOVX_PAD_H__
#define __GST_TIOVX_PAD_H__

#include <gst/gst.h>
#include <TI/tivx.h>

G_BEGIN_DECLS 

#define GST_TYPE_TIOVX_PAD gst_tiovx_pad_get_type ()

/**
 * GstTIOVXPad:
 *
 * The opaque #GstTIOVXPad data structure.
 */
G_DECLARE_DERIVABLE_TYPE(GstTIOVXPad, gst_tiovx_pad, GST_TIOVX, PAD, GstPad);

struct _GstTIOVXPadClass
{
  GstPadClass parent_class;
};

/**
 * gst_tiovx_pad_set_exemplar:
 * @pad: Pad to be used as a reference
 * @exemplar: Exemplar to be added to the pad
 *
 * Sets an exemplar to the pad to be used as a reference for the generated
 * buffers 
 *
 * Returns: nothing
 *
 */
void gst_tiovx_pad_set_exemplar(GstTIOVXPad *self, vx_reference exemplar);

/**
 * gst_tiovx_pad_acquire_buffer:
 * @pad: Pad where the buffer will be retrieved from
 * @buffer: (out) Location for a #GstBuffer
 * @params: parameters
 * 
 * Acquires a buffer from the pad's internal buffer pool
 * 
 * Returns: GST_FLOW_OK if the buffer was pulled correctly
 * 
 */
GstFlowReturn gst_tiovx_pad_acquire_buffer(GstTIOVXPad* self, GstBuffer **buffer, GstBufferPoolAcquireParams *params);

/**
 * gst_tiovx_pad_peer_query_allocation:
 * @pad: Pad whose peer will be queried
 * @caps: Caps to be used for the peer allocation
 *
 * Performs an allocation query to the pad's peer. If the query returns an TIOVX
 * bufferpool use it as this @pad 's bufferpoll
 *
 * Returns: TRUE if the allocation was successful
 *
 */
gboolean
gst_tiovx_pad_peer_query_allocation (GstTIOVXPad * self, GstCaps * caps);

/**
 * gst_tiovx_pad_query:
 * @pad: Pad where the query will be performed
 * @parent: Parent of the pad or NULL
 * @query: The #GstQuery to handle
 *
 * If this is an allocation query it will create and configure a pool and add it
 * to the query, otherwise it calls the default pad query
 *
 * Returns: TRUE if the query was successful
 *
 */
gboolean
gst_tiovx_pad_query (GstPad * pad, GstObject * parent, GstQuery * query);

/**
 * gst_tiovx_pad_chain:
 * @pad: sink pad to chain the buffer to
 * @parent: Parent of the pad or NULL
 * @buffer: Location of an existing input buffer
 *
 * Performs the necessary preparation on buffer before being chained to pad. If
 * @buffer is not TIOVX its data will be copied to a new TIOVX buffer and it
 * will be returned in @buffer. If it is and its bufferpool doesn't match pad's,
 * the pad's will be replaced by buffers'.
 *
 * Returns: GST_FLOW_OK if the buffer has been correctly prepared.
 *
 */
GstFlowReturn
gst_tiovx_pad_chain (GstPad * pad, GstObject * parent, GstBuffer ** buffer);

/**
 * gst_tiovx_pad_get_exemplar:
 * @pad: Pad where the exemplar will be retrieved from
 * 
 * Retrives the saved exemplar from @pad
 * 
 * Returns: Reference saved on the pad
 * 
 */
vx_reference
gst_tiovx_pad_get_exemplar (GstTIOVXPad * pad);

/**
 * gst_tiovx_pad_set_params:
 * @pad: Pad to be used as a reference
 * @array: Array where the incoming buffer's references will be transferred to
 * @reference: VX reference that this pad should use as reference for allocation
 * @graph_param_id: Graph parameter to be added to the pad
 * @node_param_id: Node parameter to be added to the pad
 *
 * Sets the graph and node parameters to be used for the pad 
 *
 * Returns: nothing
 *
 */
void
gst_tiovx_pad_set_params (GstTIOVXPad * pad, vx_object_array array, vx_reference reference, gint graph_param_id, gint node_param_id);

/**
 * gst_tiovx_pad_get_params:
 * @pad: Pad to be used as a reference
 * @array: Array where the incoming buffers are transferred to.
 * @reference: (out) Pointer to the vx reference that this pad used for allocation
 * @graph_param_id: (out) Holder for the pad graph parameter
 * @node_param_id: (out) Holder for the  pad node parameter
 *
 * Gets the graph and node parameters from the pad
 *
 * Returns: nothing
 *
 */
void
gst_tiovx_pad_get_params (GstTIOVXPad * pad, vx_object_array *array, vx_reference **reference, gint* graph_param_id, gint* node_param_id);

G_END_DECLS

#endif /* __GST_TIOVX_PAD_H__ */

