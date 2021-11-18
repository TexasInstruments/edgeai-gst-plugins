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


#ifndef __GST_TIOVX_BUFFER_POOL_H__
#define __GST_TIOVX_BUFFER_POOL_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <TI/tivx.h>

#include "gst-libs/gst/tiovx/gsttiovxallocator.h"

G_BEGIN_DECLS 

#define GST_TYPE_TIOVX_BUFFER_POOL gst_tiovx_buffer_pool_get_type ()

/**
 * GST_TIOVX_IS_BUFFER_POOL:
 * @ptr: pointer to check if its a TIOVX BufferPool
 * 
 * Checks if a pointer is a TIOVX buffer pool
 * 
 * Returns: TRUE if @ptr is a TIOVX bufferpool
 * 
 */
/**
 * GstTIOVXBufferPool:
 *
 * The opaque #GstTIOVXBufferPool data structure.
 */
G_DECLARE_DERIVABLE_TYPE(GstTIOVXBufferPool, gst_tiovx_buffer_pool, GST_TIOVX, BUFFER_POOL, GstBufferPool);

/**
 * GstTIOVXBufferPoolClass:
 * @parent_class: Element parent class
 *
 * @validate_caps:      Required. Checks that the current caps and the exemplar
 *                      have a matching format.
 * @add_meta_to_buffer: Required. Adds the required TIOVX meta according
 *                      the exemplar type to the buffer.
 * @free_buffer_meta:   Required. Frees the TIOVX meta from the buffer
 */
struct _GstTIOVXBufferPoolClass
{
  GstBufferPoolClass parent_class;

  /*< public >*/
  /* virtual methods for subclasses */
  gboolean (*validate_caps) (GstTIOVXBufferPool * self, const GstCaps * caps,
      const vx_reference exemplar);

  void (*add_meta_to_buffer) (GstTIOVXBufferPool * self, GstBuffer* buffer,
      vx_reference reference, guint num_channels, GstTIOVXMemoryData *ti_memory);

  void (*free_buffer_meta) (GstTIOVXBufferPool * self, GstBuffer* buffer);
};

G_END_DECLS

#endif /* __GST_TIOVX_BUFFER_POOL_H__ */

