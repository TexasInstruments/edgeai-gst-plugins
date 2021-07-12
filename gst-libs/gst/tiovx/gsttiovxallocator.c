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

#include <TI/tivx_mem.h>

#include "gsttiovxallocator.h"

/**
 * SECTION:gsttiovxallocator
 * @short_description: GStreamer allocator for GstTIOVX based elements
 *
 * This class implements a GStreamer standard allocator for GstTIOVX
 * based elements.
 */

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_allocator_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_allocator_debug_category

struct _GstTIOVXAllocator
{
  GstDmaBufAllocator base;
};

G_DEFINE_TYPE_WITH_CODE (GstTIOVXAllocator, gst_tiovx_allocator,
    GST_TYPE_DMABUF_ALLOCATOR,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_allocator_debug_category,
        "tiovxallocator", 0, "debug category for TIOVX allocator class"));

/* prototypes */
static GstMemory *gst_tiovx_allocator_alloc (GstAllocator * allocator,
    gsize size, GstAllocationParams * params);
static void gst_tiovx_allocator_free (GstAllocator * allocator,
    GstMemory * memory);

static void
gst_tiovx_allocator_class_init (GstTIOVXAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = GST_DEBUG_FUNCPTR (gst_tiovx_allocator_alloc);
  allocator_class->free = GST_DEBUG_FUNCPTR (gst_tiovx_allocator_free);
}

static void
gst_tiovx_allocator_init (GstTIOVXAllocator * self)
{
  GstAllocator *allocator = GST_ALLOCATOR_CAST (self);

  GST_INFO_OBJECT (self, "New TIOVX allocator");

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static GstMemory *
gst_tiovx_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstMemory *mem = NULL;
  tivx_shared_mem_ptr_t *mem_ptr = NULL;
  vx_status status = VX_SUCCESS;

  g_return_val_if_fail (GST_TIOVX_IS_ALLOCATOR (allocator), NULL);

  if (size < 0) {
    GST_ERROR_OBJECT (allocator, "Negative size received for allocation");
    goto out;
  }

  GST_LOG_OBJECT (allocator, "Allocating TIOVX memory of size %" G_GSIZE_FORMAT,
      size);

  mem_ptr = g_malloc (sizeof (tivx_shared_mem_ptr_t));
  if (NULL == mem_ptr) {
    GST_ERROR_OBJECT (allocator, "Unable to allocate memory for TIOVX mem_ptr");
    goto out;
  }

  status = tivxMemBufferAlloc (mem_ptr, size, TIVX_MEM_EXTERNAL);
  if (status != VX_SUCCESS) {
    GST_ERROR_OBJECT (allocator, "Unable to allocate dma memory buffer: %d",
        status);
    g_free (mem_ptr);
    goto out;
  }

  mem =
      gst_dmabuf_allocator_alloc_with_flags (allocator, mem_ptr->dma_buf_fd,
      size, GST_FD_MEMORY_FLAG_DONT_CLOSE);

  /* Save the mem_ptr for latter deletion */
  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (mem), TIOVX_MEM_PTR_QUARK,
      mem_ptr, g_free);

  GST_LOG_OBJECT (allocator, "Allocated TIOVX memory %" GST_PTR_FORMAT, mem);

out:
  return mem;
}

static void
gst_tiovx_allocator_free (GstAllocator * allocator, GstMemory * memory)
{
  tivx_shared_mem_ptr_t *mem_ptr = NULL;
  gsize size = 0;

  g_return_if_fail (allocator);
  g_return_if_fail (memory);

  GST_LOG_OBJECT (allocator, "Freeing TIOVX memory %" GST_PTR_FORMAT, memory);

  mem_ptr =
      gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (memory),
      TIOVX_MEM_PTR_QUARK);
  size = gst_memory_get_sizes (memory, NULL, NULL);

  if (NULL != mem_ptr) {
    tivxMemBufferFree (mem_ptr, size);
  }
}
