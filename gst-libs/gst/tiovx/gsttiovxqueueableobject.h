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

#ifndef _GST_TIOVX_QUEUEABLE_OBJECT_H_
#define _GST_TIOVX_QUEUEABLE_OBJECT_H_

#include <gst/base/base-prelude.h>
#include <gst/gst.h>
#include <TI/tivx.h>

G_BEGIN_DECLS

#define GST_TYPE_TIOVX_QUEUEABLE gst_tiovx_queueable_get_type ()

G_DECLARE_FINAL_TYPE(GstTIOVXQueueable, gst_tiovx_queueable, GST_TIOVX, QUEUEABLE, GObject);

/**
 * gst_tiovx_queueable_object_set_params:
 *
 * @obj: Object where the data will be stored
 * @array: Vx object array that contains the objects to be processed by the graph.
 * @exemplar: VX reference that this pad should use as reference for allocation
 * @graph_param_id: Parameter id that will be used to enqueue this parameter
 * to the Vx Graph
 * @node_param_id: Parameter id that will be used to configure the node
 *
 * Sets an exemplar and a param id to the pad, these will be used for future
 * configuration of the given object. This should be used for objects that
 * need to be queued/dequeued but don't belong to a pad.
 *
 * Returns: nothing
 *
 */
void gst_tiovx_queueable_set_params (GstTIOVXQueueable* obj, vx_object_array array, vx_reference exemplar, gint graph_param_id, gint node_param_id);
/**
 * gst_tiovx_queueable_object_get_params:
 *
 * @obj: Object where the data will be stored
 * @array: Pointer to the object array
 * @exemplar: Pointer to the exemplar array
 * @graph_param_id: Graph id for the corresponding array
 * @node_param_id: Node id for the corresponding array
 *
 * Returns: nothing
 *
 */
void gst_tiovx_queueable_get_params (GstTIOVXQueueable* obj, vx_object_array *array, vx_reference **exemplar, gint *graph_param_id, gint *node_param_id);

G_END_DECLS


#endif /* _GST_TIOVX_QUEUEABLE_OBJECT_H_ */