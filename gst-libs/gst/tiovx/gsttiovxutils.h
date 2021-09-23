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
#include <VX/vx.h>
#include <VX/vx_types.h>

#define MODULE_MAX_NUM_ADDRS 4
#define MODULE_MAX_NUM_TENSORS 1

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
 * gst_tiovx_create_new_pool:
 * @category: Category to use for debug messages
 * @exemplar: Exemplar to be used as a reference for the pool
 *
 * Creates a new pool based on exemplar
 *
 * Returns: GstBufferPool created, NULL if error.
 *
 */
GstBufferPool *
gst_tiovx_create_new_pool (GstDebugCategory * category, vx_reference * exemplar);

/**
 * gst_tiovx_add_new_pool:
 * @category: Category to use for debug messages
 * @query: Query where the pool will be added
 * @num_buffers: Number of buffers for the pool
 * @exemplar: Exemplar to be used as a reference for the pool
 * @info: Video information to be used as a reference for the pool
 * @pool: If non-null the pool will be saved here
 *
 * Adds a new pool to the query
 *
 * Returns: True if the pool was successfully added
 *
 */
gboolean
gst_tiovx_add_new_pool (GstDebugCategory * category, GstQuery * query,
    guint num_buffers, vx_reference * exemplar, gsize size, GstBufferPool **pool);

/**
 * gst_tiovx_configure_pool:
 * @category: Category to use for debug messages
 * @pool: Pool to configure
 * @exemplar: Exemplar to be used to configure the pool
 * @caps: Caps to be used to configure the pool
 * @size: Size for the created buffers
 * @num_buffers: Number of buffers for the pool
 * 
 * Returns: False if the pool cannot be configure
 * 
 */
gboolean
gst_tiovx_configure_pool (GstDebugCategory * category, GstBufferPool * pool, vx_reference * exemplar, GstCaps * caps, gsize size, guint num_buffers);

/**
 * gst_tiovx_validate_tiovx_buffer:
 * @category: Category to use for debug messages
 * @pool: Pointer to the caller's pool
 * @buffer: Buffer to validate
 * @exemplar: Exemplar to be used for the pool if a new one is needed
 * @caps: Caps to be used for the pool if a new one is needed
 * @pool_size: pool size for the pool if a new one is needed
 * 
 * This functions checks if the buffer's pool and @pool match.
 * If that isn't the case and the pool is a TIOVX pool, @pool
 * will be replaced by the buffers. If the buffer doesn't come
 * from a TIOVX pool a new buffer will be created from pool and
 * the data will be copied.
 * If no pool provided a new one will be created and returned
 * in pool.
 * 
 * This function will not take ownership of @buffer. If a copy
 * isn't necessary it will return the same incoming buffer, if
 * it is the caller is responsible for unrefing the buffer after usage. 
 * 
 * Returns: A valid Buffer with TIOVX data
 */
GstBuffer *
gst_tiovx_validate_tiovx_buffer (GstDebugCategory * category, GstBufferPool ** pool,
    GstBuffer * buffer, vx_reference *exemplar, GstCaps* caps, guint pool_size);

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
 * gst_tiovx_buffer_pool_config_set_exemplar:
 * @config: BufferPool configuration
 * @exemplar: Exemplar to be set to the configuration
 *
 * Sets an exemplar to a TIOVX bufferpool configuration
 *
 * Returns: nothing
 *
 */
void gst_tiovx_buffer_pool_config_set_exemplar(GstStructure * config,
					       const vx_reference exemplar);

/**
 * gst_tiovx_buffer_pool_config_get_exemplar:
 * @config: BufferPool configuration
 * @exemplar: Exemplar pointer for the return value
 *
 * Gets an exemplar from a TIOVX bufferpool configuration
 *
 * Returns: nothing
 *
 */
void gst_tiovx_buffer_pool_config_get_exemplar (GstStructure * config,
						vx_reference * exemplar);

/**
 * gst_tiovx_get_exemplar_type:
 * @exemplar: Exemplar to get type from
 *
 * Gets exemplar type
 *
 * Returns: vx_enum with exemplar type
 *
 */
vx_enum gst_tiovx_get_exemplar_type (vx_reference * exemplar);

/**
 * gst_tiovx_get_vx_array_from_buffer:
 * @category: Category to use for debug messages
 * @exemplar: vx_reference describing the buffer meta
 * @buffer: GstBuffer to get the vx_array from
 *
 * Gets a vx_array from buffer meta
 *
 * Returns: vx_object_array obtained from buffer meta
 *
 */
vx_object_array gst_tiovx_get_vx_array_from_buffer (GstDebugCategory * category,
						    vx_reference * exemplar,
						    GstBuffer * buffer);

/**
 * gst_tiovx_get_size_from_exemplar:
 * @exemplar: vx_reference describing the buffer
 * @caps: GstCaps describing the buffer
 *
 * Gets size from exemplar and caps
 *
 * Returns: gsize with buffer's size
 *
 */
gsize gst_tiovx_get_size_from_exemplar (vx_reference * exemplar,
					GstCaps * caps);

void gst_tiovx_init_debug (void);

#endif /* __GST_TIOVX_UTILS_H__ */
