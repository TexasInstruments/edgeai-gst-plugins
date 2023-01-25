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

#ifndef __GST_TIOVX_UTILS_H__
#define __GST_TIOVX_UTILS_H__

#include <gst/video/video.h>
#include <TI/tivx.h>
#include <TI/tivx_ext_raw_image.h>
#include <VX/vx_types.h>
#include <VX/vx.h>

#define MIN_NUM_CHANNELS 1
#define MAX_NUM_CHANNELS 16

#define MODULE_MAX_NUM_ADDRS 16
#define MODULE_MAX_NUM_TENSORS 1

#define GST_CAPS_FEATURE_BATCHED_MEMORY "memory:batched"

/**
 * vx_format_to_gst_format:
 * @format: format to convert
 *
 * Converts a vx_df_image to a #GstVideoFormat
 *
 * Returns: Converted format
 *
 */
GstVideoFormat vx_format_to_gst_format (const vx_df_image format);

/**
 * gst_format_to_vx_format:
 * @gst_format: format to convert
 *
 * Converts a #GstVideoFormat to a vx_df_image
 *
 * Returns: Converted format
 *
 */
vx_df_image gst_format_to_vx_format (const GstVideoFormat gst_format);

/**
 * tivx_raw_format_to_gst_format:
 * @format: format to convert
 *
 * Converts a tivx_raw_image_pixel_container_e to a #GstVideoFormat
 *
 * Returns: Converted format
 *
 */
const gchar * tivx_raw_format_to_gst_format (const enum tivx_raw_image_pixel_container_e format);

/**
 * gst_format_to_tivx_raw_format:
 * @gst_format: format to convert
 *
 * Converts a #GstVideoFormat to a tivx_raw_image_pixel_container_e
 *
 * Returns: Converted format
 *
 */
enum tivx_raw_image_pixel_container_e gst_format_to_tivx_raw_format (const gchar * gst_format);

/**
 * gst_tiovx_transfer_handle:
 * @self: Object using this function
 * @src: Reference where the handles will be transfer from.
 * @dest: Reference where the handles will be transfer to.
 *
 * Transfers handles between vx_references
 *
 * Returns: VX_SUCCESS if the data was successfully transfered
 *
 */
vx_status
gst_tiovx_transfer_handle (GstDebugCategory * category, vx_reference src,
    vx_reference dest);

/**
 * gst_tiovx_tensor_get_bit_depth:
 * @data_type: tensor data type
 *
 * Gets bit depth from a tensor data type
 *
 * Returns: Tensor bit depth
 *
 */
vx_uint32 gst_tiovx_tensor_get_tensor_bit_depth (vx_enum data_type);

/**
 * gst_tiovx_empty_exemplar:
 * @ref: reference to empty
 *
 * Sets NULL to every address in ref
 *
 * Returns: VX_SUCCESS if ref was successfully emptied
 *
 */
vx_status gst_tiovx_empty_exemplar (vx_reference ref);

/**
 * gst_tiovx_get_exemplar_type:
 * @exemplar: Exemplar to get type from
 *
 * Gets exemplar type
 *
 * Returns: vx_enum with exemplar type
 *
 */
vx_enum gst_tiovx_get_exemplar_type (vx_reference exemplar);

/**
 * gst_tiovx_get_size_from_exemplar:
 * @exemplar: vx_reference describing the buffer
 *
 * Gets size from exemplar
 *
 * Returns: gsize with buffer's size
 *
 */
gsize gst_tiovx_get_size_from_exemplar (vx_reference exemplar);

/**
 * gst_tiovx_init_debug:
 *
 * Initializes GstInfo debug categories
 */
void gst_tiovx_init_debug (void);

/**
 * add_graph_parameter_by_node_index:
 * @debug_category: Category to be used for logging
 * @gobj: GObject calling this function
 * @graph: Graph where the parameter given by node_parameter_index will be added
 * @node: Node where a parameter according to node_parameter_index will be extracted
 * @parameter_index: Parameter to be used to for enqueuing & dequeueing  refs_list from the graph
 * @node_parameter_index: Node index of the parameter to be added to the graph
 * @parameters_list: List of parameters to be used to configure the graph
 * @refs_list: Pointer to head of array of enqueued vx_references
 *
 * Configure the VX graph
 *
 * Returns: VX_SUCCESS if the graph was successfully configured
 */
vx_status
add_graph_parameter_by_node_index (GstDebugCategory *debug_category, GObject *gobj, vx_graph graph, vx_node node,
    vx_uint32 parameter_index, vx_uint32 node_parameter_index,
    vx_graph_parameter_queue_params_t * parameters_list,
    vx_reference * refs_list);

/**
 * gst_tiovx_demux_get_exemplar_mem:
 * @exemplar: Exemplar where the information will be extracted
 * @data: Output pointer
 * @size: Output size
 * 
 * Returns the data pointer and the memory size of exemplar
 * 
 * Returns: VX_SUCCESS if the data could be extracted
 */
vx_status
gst_tiovx_demux_get_exemplar_mem (GObject * object, GstDebugCategory * category,
    vx_reference exemplar, void** data, gsize* size);

/**
 * gst_tiovx_get_exemplar_from_caps:
 * @context: Context to where the exemplar belongs to
 * @caps: Caps from which the exemplar will be based on
 * 
 * Returns an exemplar based on the caps
 */
vx_reference
gst_tiovx_get_exemplar_from_caps (GObject * object, GstDebugCategory * category,
    vx_context context, GstCaps * caps);

/**
 * gst_tiovx_get_batched_memory_feature:
 * 
 * Returns the caps feature for memory:batched buffers.
 * If the feature doesn't exists it creates it
 * 
 * Returns the memory:batched capsfeature
 */
GstCapsFeatures *
gst_tiovx_get_batched_memory_feature (void);

/**
 * gst_tioxv_get_pyramid_caps_info:
 * 
 * Returns the pyramid's information for a given caps
 * 
 * @object: Object calling this function
 * @category: Category to be used for logging
 * @caps: Caps from where the information will be extracted from
 * @levels: [Out] Levels found in the caps
 * @scale: [Out] Scale found in the caps
 * @width: [Out] Width found in the caps
 * @height: [Out] Height found in the caps
 * @format: [Out] Format found in the caps
 *
 */ 
gboolean
gst_tioxv_get_pyramid_caps_info (GObject * object, GstDebugCategory * category,
    const GstCaps * caps, gint * levels, gdouble * scale, gint * width, gint * height,
    GstVideoFormat * format);

#endif /* __GST_TIOVX_UTILS_H__ */
