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
 * *	Any redistribution and use are licensed by TI for use only with TI Devices.
 * 
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 * 
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
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

#ifndef __GST_TIOVX_TENSOR_META__
#define __GST_TIOVX_TENSOR_META__

#include <gst/gst.h>
#include <TI/tivx.h>

#define MODULE_MAX_NUM_DIMS 4
#define MODULE_MAX_NUM_TENSORS 1

G_BEGIN_DECLS 

#define GST_TYPE_TIOVX_TENSOR_META_API (gst_tiovx_tensor_meta_api_get_type())
#define GST_TIOVX_TENSOR_META_INFO  (gst_tiovx_tensor_meta_get_info())

/**
 * GstTIOVXTensorInfo:
 * @num_dims: Number of dimensions in the tensor
 * @dim_sizes: Array with the size of each dim
 * @dim_strides: Array with the stride of each dim
 * @data_type: Tensor data type
 * @tensor_size: Tensor full size in memory
 * 
 * Structure with the tensor information
 * 
 */
typedef struct _GstTIOVXTensorInfo GstTIOVXTensorInfo;
struct _GstTIOVXTensorInfo {
  vx_size num_dims;
  vx_size dim_sizes[MODULE_MAX_NUM_DIMS];
  vx_size dim_strides[MODULE_MAX_NUM_DIMS];
  vx_size dim_offsets[MODULE_MAX_NUM_DIMS];
  vx_enum data_type;
  vx_size tensor_size;
};

/**
 * GstTIOVXTensorMeta:
 * @meta: parent #GstMeta
 * @array: VX Object Array holding the number of tensors in the batch
 * @tensor_info: Information for the held tensors
 *
 * TIOVX Tensor Meta hold OpenVX related information
 */
typedef struct _GstTIOVXTensorMeta GstTIOVXTensorMeta;
struct _GstTIOVXTensorMeta {
  GstMeta meta;

  vx_object_array array;
  GstTIOVXTensorInfo tensor_info;
};

/**
 * gst_tiovx_tensor_meta_api_get_type:
 * 
 * Gets the type for the TIOVX Tensor Meta
 * 
 * Returns: type of TIOVX Tensor Meta
 * 
 */
GType gst_tiovx_tensor_meta_api_get_type (void);

/**
 * gst_tiovx_tensor_meta_get_info:
 * 
 * Gets the TIOXV Tensor Meta's GstMetaInfo
 * 
 * Returns: MetaInfo for TIOVX Tensor Meta
 * 
 */
const GstMetaInfo *gst_tiovx_tensor_meta_get_info (void);

/**
 * gst_buffer_add_tiovx_tensor_meta:
 * @buffer: Buffer where the meta will be added
 * @exemplar: Exemplar to be added to the meta
 * @array_length: Length of the tensor array to create
 * @mem_start: Pointer where the memory for the tensor starts 
 * 
 * Adds a meta to the buffer and initializes the related structures
 * 
 * Returns: Meta that was added to the buffer
 * 
 */
GstTIOVXTensorMeta* gst_buffer_add_tiovx_tensor_meta(GstBuffer* buffer, const vx_reference exemplar, const gint array_length, guint64 mem_start);

G_END_DECLS


#endif /* __GST_TIOVX_TENSOR_META__ */
