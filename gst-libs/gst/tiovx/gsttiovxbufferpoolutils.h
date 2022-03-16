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

#ifndef __GST_TIOVX_BUFFER_POOL_UTILS_H__
#define __GST_TIOVX_BUFFER_POOL_UTILS_H__

#include <gst/video/video.h>
#include <VX/vx.h>
#include <VX/vx_types.h>

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
gst_tiovx_create_new_pool (GstDebugCategory * category, vx_reference exemplar);

/**
 * gst_tiovx_add_new_pool:
 * @category: Category to use for debug messages
 * @query: Query where the pool will be added
 * @num_buffers: Number of buffers for the pool
 * @exemplar: Exemplar to be used as a reference for the pool
 * @size: Size of the buffers in the pool
 * @num_channels: Number of channels that each buffer will have
 * @pool: If non-null the pool will be saved here
 *
 * Adds a new pool to the query
 *
 * Returns: True if the pool was successfully added
 *
 */
gboolean
gst_tiovx_add_new_pool (GstDebugCategory * category, GstQuery * query,
    guint num_buffers, vx_reference exemplar, gsize size, gint num_channels,
    GstBufferPool **pool);

/**
 * gst_tiovx_configure_pool:
 * @category: Category to use for debug messages
 * @pool: Pool to configure
 * @exemplar: Exemplar to be used to configure the pool
 * @caps: Caps to be used to configure the pool
 * @size: Size for the created buffers
 * @num_buffers: Number of buffers for the pool
 * @num_channels: Number of channels that each buffer will have
 *
 * Returns: False if the pool cannot be configure
 *
 */
gboolean
gst_tiovx_configure_pool (GstDebugCategory * category, GstBufferPool * pool,
    vx_reference exemplar, GstCaps * caps, gsize size, guint num_buffers,
    gint num_channels);

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
 * gst_tiovx_buffer_pool_config_set_num_channels:
 * @config: BufferPool configuration
 * @num_channels: Number of channels to be set
 * 
 * Sets a number of channels to a TIOVX bufferpool configuration
 */
void
gst_tiovx_buffer_pool_config_set_num_channels (GstStructure * config,
    const guint num_channels);

/**
 * gst_tiovx_buffer_pool_config_get_num_channels:
 * @config: BufferPool configuration
 * @num_channels: Number of channels pointer for the return value
 * 
 * Gets a number of channels from a TIOVX bufferpool configuration
 */
void
gst_tiovx_buffer_pool_config_get_num_channels (GstStructure * config,
    guint * num_channels);

#endif /* __GST_TIOVX_BUFFER_POOL_UTILS_H__ */
