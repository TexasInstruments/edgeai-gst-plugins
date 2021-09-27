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

#include "gsttiovxbufferutils.h"

#include "gsttiovxallocator.h"
#include "gsttiovxbufferpool.h"
#include "gsttiovxbufferpoolutils.h"
#include "gsttiovxmeta.h"
#include "gsttiovxtensorbufferpool.h"
#include "gsttiovxtensormeta.h"
#include "gsttiovxutils.h"

static const gsize kcopy_all_size = -1;

GST_DEBUG_CATEGORY (gst_tiovx_buffer_performance);

static GstBuffer *
gst_tiovx_buffer_copy (GstDebugCategory * category, GstBufferPool * pool,
    GstBuffer * in_buffer)
{
  GstBuffer *out_buffer = NULL;
  GstBufferCopyFlags flags =
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_META;
  GstFlowReturn flow_return = GST_FLOW_ERROR;
  gboolean ret = FALSE;
  GstMapInfo in_info;
  GstTIOVXMemoryData *ti_memory = NULL;
  GstMemory *memory;
  gsize size = 0;

  g_return_val_if_fail (category, NULL);
  g_return_val_if_fail (pool, NULL);
  g_return_val_if_fail (in_buffer, NULL);

  flow_return =
      gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (pool),
      &out_buffer, NULL);
  if (GST_FLOW_OK != flow_return) {
    GST_CAT_ERROR (category, "Unable to acquire buffer from internal pool");
    goto out;
  }

  ret = gst_buffer_copy_into (out_buffer, in_buffer, flags, 0, kcopy_all_size);
  if (!ret) {
    GST_CAT_ERROR (category,
        "Error copying flags from in buffer: %" GST_PTR_FORMAT
        " to out buffer: %" GST_PTR_FORMAT, in_buffer, out_buffer);
    gst_buffer_unref (out_buffer);
    out_buffer = NULL;
    goto out;
  }

  gst_buffer_map (in_buffer, &in_info, GST_MAP_READ);

  memory = gst_buffer_get_memory (out_buffer, 0);

  ti_memory = gst_tiovx_memory_get_data (memory);

  size = gst_memory_get_sizes (memory, NULL, NULL);

  if (NULL == in_info.data) {
    GST_CAT_ERROR (category, "In buffer is empty, aborting copy");
    goto free;
  }

  memcpy ((void *) ti_memory->mem_ptr.host_ptr, in_info.data, size);

free:
  gst_buffer_unmap (in_buffer, &in_info);
  gst_memory_unref (memory);

out:
  return out_buffer;
}

/* Gets a vx_object_array from buffer meta */
vx_object_array
gst_tiovx_get_vx_array_from_buffer (GstDebugCategory * category,
    vx_reference * exemplar, GstBuffer * buffer)
{
  vx_object_array array = NULL;
  vx_enum type = VX_TYPE_INVALID;

  g_return_val_if_fail (category, NULL);
  g_return_val_if_fail (exemplar, NULL);
  g_return_val_if_fail (buffer, NULL);

  type = gst_tiovx_get_exemplar_type (exemplar);

  if (VX_TYPE_IMAGE == type) {
    GstTIOVXMeta *meta = NULL;
    meta =
        (GstTIOVXMeta *) gst_buffer_get_meta (buffer, GST_TYPE_TIOVX_META_API);
    if (!meta) {
      GST_CAT_ERROR (category, "TIOVX Meta was not found in buffer");
      goto exit;
    }

    array = meta->array;
  } else if (VX_TYPE_TENSOR == type) {
    GstTIOVXTensorMeta *meta = NULL;
    meta =
        (GstTIOVXTensorMeta *) gst_buffer_get_meta (buffer,
        GST_TYPE_TIOVX_TENSOR_META_API);
    if (!meta) {
      GST_CAT_ERROR (category, "TIOVX Tensor Meta was not found in buffer");
      goto exit;
    }

    array = meta->array;
  } else {
    GST_CAT_ERROR (category, "Object type %d is not supported", type);
  }

exit:
  return array;
}

GstBuffer *
gst_tiovx_validate_tiovx_buffer (GstDebugCategory * category,
    GstBufferPool ** pool, GstBuffer * buffer, vx_reference * exemplar,
    GstCaps * caps, guint pool_size)
{
  GstBufferPool *new_pool = NULL;
  gsize size = 0;

  g_return_val_if_fail (category, NULL);
  g_return_val_if_fail (pool, NULL);
  g_return_val_if_fail (buffer, NULL);

  /* Propose allocation did not happen, there is no upstream pool therefore
   * the element has to create one */
  if (NULL == *pool) {
    GST_CAT_INFO (category,
        "Propose allocation did not occur creating new pool");

    /* We use input vx_reference to create a pool */
    size = gst_tiovx_get_size_from_exemplar (exemplar, caps);
    if (0 >= size) {
      GST_CAT_ERROR (category, "Failed to get size from input");
      return NULL;
    }

    new_pool = gst_tiovx_create_new_pool (GST_CAT_DEFAULT, exemplar);
    if (NULL == new_pool) {
      GST_CAT_ERROR (category,
          "Failed to create new pool in transform function");
      return NULL;
    }

    if (!gst_tiovx_configure_pool (GST_CAT_DEFAULT, new_pool, exemplar,
            caps, size, pool_size)) {
      GST_CAT_ERROR (category,
          "Unable to configure pool in transform function");
      return FALSE;
    }

    /* Assign the new pool to the internal value */
    *pool = new_pool;
  }

  if ((buffer)->pool != GST_BUFFER_POOL (*pool)) {
    if ((GST_TIOVX_IS_BUFFER_POOL ((buffer)->pool))
        || (GST_TIOVX_IS_TENSOR_BUFFER_POOL ((buffer)->pool))) {
      GST_CAT_INFO (category,
          "Buffer's and Pad's buffer pools are different, replacing the Pad's");
      gst_object_unref (*pool);

      *pool = (buffer)->pool;
      gst_object_ref (*pool);
    } else {
      GST_CAT_DEBUG (gst_tiovx_buffer_performance,
          "Buffer doesn't come from TIOVX, copying the buffer");

      buffer =
          gst_tiovx_buffer_copy (category, GST_BUFFER_POOL (*pool), buffer);
      if (!buffer) {
        GST_CAT_ERROR (category, "Failure when copying input buffer from pool");
      }
    }
  }

  return buffer;
}

void
gst_tiovx_init_buffer_utils_debug (void)
{
  GST_DEBUG_CATEGORY_GET (gst_tiovx_buffer_performance, "GST_PERFORMANCE");
}
