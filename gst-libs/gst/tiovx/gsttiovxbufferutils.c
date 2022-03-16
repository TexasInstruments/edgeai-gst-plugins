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
#include "gsttiovximagemeta.h"
#include "gsttiovxmuxmeta.h"
#include "gsttiovxpyramidbufferpool.h"
#include "gsttiovxpyramidmeta.h"
#include "gsttiovxrawimagemeta.h"
#include "gsttiovxtensorbufferpool.h"
#include "gsttiovxtensormeta.h"
#include "gsttiovxutils.h"

static const gsize copy_all_size = -1;

GST_DEBUG_CATEGORY (gst_tiovx_buffer_performance);

/* Copies buffer data into the provided pool */
static GstBuffer *
gst_tiovx_buffer_copy (GstDebugCategory * category, GstBufferPool * pool,
    GstBuffer * in_buffer, vx_reference exemplar)
{
  GstBuffer *out_buffer = NULL;
  GstBufferCopyFlags flags =
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_META;
  GstFlowReturn flow_return = GST_FLOW_ERROR;
  gboolean ret = FALSE;
  GstMapInfo in_info;
  GstTIOVXMemoryData *ti_memory = NULL;
  GstMemory *memory = NULL;
  gsize out_size = 0;
  vx_enum type = VX_TYPE_INVALID;

  gint num_planes = 0;
  gint plane_idx = 0;
  gsize plane_offset[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_stride_x[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_stride_y[MODULE_MAX_NUM_PLANES] = { 0 };
  guint plane_steps_x[MODULE_MAX_NUM_PLANES] = { 0 };
  guint plane_steps_y[MODULE_MAX_NUM_PLANES] = { 0 };
  guint plane_widths[MODULE_MAX_NUM_PLANES] = { 0 };
  guint plane_heights[MODULE_MAX_NUM_PLANES] = { 0 };
  guint8 *in_data_ptr = 0;
  gint total_copied = 0;

  g_return_val_if_fail (category, NULL);
  g_return_val_if_fail (pool, NULL);
  g_return_val_if_fail (in_buffer, NULL);

  /* Activate the buffer pool */
  if (!gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), TRUE)) {
    GST_CAT_ERROR (category, "Failed to activate bufferpool");
    out_buffer = NULL;
    goto out;
  }
  flow_return =
      gst_buffer_pool_acquire_buffer (GST_BUFFER_POOL (pool),
      &out_buffer, NULL);
  if (GST_FLOW_OK != flow_return) {
    GST_CAT_ERROR (category, "Unable to acquire buffer from internal pool");
    goto out;
  }

  ret = gst_buffer_copy_into (out_buffer, in_buffer, flags, 0, copy_all_size);
  if (!ret) {
    GST_CAT_ERROR (category,
        "Error copying flags from in buffer: %" GST_PTR_FORMAT
        " to out buffer: %" GST_PTR_FORMAT, in_buffer, out_buffer);
    gst_buffer_unref (out_buffer);
    out_buffer = NULL;
    goto out;
  }

  /* Getting reference type */
  type = gst_tiovx_get_exemplar_type (exemplar);

  if (VX_TYPE_IMAGE == type) {
    GstTIOVXImageMeta *tiovxmeta = NULL;
    gint i = 0;

    tiovxmeta =
        (GstTIOVXImageMeta *) gst_buffer_get_meta (out_buffer,
        GST_TYPE_TIOVX_IMAGE_META_API);

    num_planes = tiovxmeta->image_info.num_planes;

    for (i = 0; i < num_planes; i++) {
      plane_offset[i] = tiovxmeta->image_info.plane_offset[i];
      plane_stride_x[i] = tiovxmeta->image_info.plane_stride_x[i];
      plane_stride_y[i] = tiovxmeta->image_info.plane_stride_y[i];
      plane_steps_x[i] = tiovxmeta->image_info.plane_steps_x[i];
      plane_steps_y[i] = tiovxmeta->image_info.plane_steps_y[i];
      plane_widths[i] = tiovxmeta->image_info.plane_widths[i];
      plane_heights[i] = tiovxmeta->image_info.plane_heights[i];
    }
  } else if (VX_TYPE_TENSOR == type) {
    GstTIOVXTensorMeta *tiovx_tensor_meta = NULL;

    tiovx_tensor_meta =
        (GstTIOVXTensorMeta *) gst_buffer_get_meta (out_buffer,
        GST_TYPE_TIOVX_TENSOR_META_API);

    /* Tensor are mapped as a single block, just copy the whole size */
    num_planes = 1;

    plane_widths[0] =
        tiovx_tensor_meta->tensor_info.dim_sizes[0] *
        tiovx_tensor_meta->tensor_info.dim_sizes[1] *
        tiovx_tensor_meta->tensor_info.dim_sizes[2] *
        gst_tiovx_tensor_get_tensor_bit_depth (tiovx_tensor_meta->
        tensor_info.data_type);
    plane_stride_x[0] = 1;
    plane_steps_x[0] = 1;

    plane_heights[0] = 1;
    plane_steps_y[0] = 1;
  } else if (TIVX_TYPE_RAW_IMAGE == type) {
    GstTIOVXRawImageMeta *tiovx_raw_image_meta = NULL;
    gint i = 0;

    tiovx_raw_image_meta =
        (GstTIOVXRawImageMeta *) gst_buffer_get_meta (out_buffer,
        GST_TYPE_TIOVX_RAW_IMAGE_META_API);

    num_planes = tiovx_raw_image_meta->image_info.num_exposures;

    for (i = 0; i < num_planes; i++) {
      plane_offset[i] = tiovx_raw_image_meta->image_info.exposure_offset[i];
      plane_stride_x[i] = tiovx_raw_image_meta->image_info.exposure_stride_x[i];
      plane_stride_y[i] = tiovx_raw_image_meta->image_info.exposure_stride_y[i];
      plane_steps_x[i] = tiovx_raw_image_meta->image_info.exposure_steps_x[i];
      plane_steps_y[i] = tiovx_raw_image_meta->image_info.exposure_steps_y[i];
      plane_widths[i] = tiovx_raw_image_meta->image_info.exposure_widths[i];
      plane_heights[i] = tiovx_raw_image_meta->image_info.exposure_heights[i];
    }
  } else if (VX_TYPE_PYRAMID == type) {
    GstTIOVXPyramidMeta *tiovx_pyramid_meta = NULL;

    tiovx_pyramid_meta =
        (GstTIOVXPyramidMeta *) gst_buffer_get_meta (out_buffer,
        GST_TYPE_TIOVX_PYRAMID_META_API);

    num_planes = 1;
    plane_widths[0] = tiovx_pyramid_meta->pyramid_info.size;
    plane_stride_x[0] = 1;
    plane_steps_x[0] = 1;

    plane_heights[0] = 1;
    plane_steps_y[0] = 1;
  } else {
    GST_CAT_ERROR (category,
        "Type %d not supported, buffer pool was not created", type);
  }

  gst_buffer_map (in_buffer, &in_info, GST_MAP_READ);

  memory = gst_buffer_get_memory (out_buffer, 0);

  ti_memory = gst_tiovx_memory_get_data (memory);

  out_size = gst_memory_get_sizes (memory, NULL, NULL);

  if (NULL == in_info.data) {
    GST_CAT_ERROR (category, "In buffer is empty, aborting copy");
    goto free;
  }

  if (in_info.size == out_size) {
    GST_CAT_LOG (gst_tiovx_buffer_performance,
        "Both buffers have the same size, copying as is");
    memcpy ((void *) ti_memory->mem_ptr.host_ptr, in_info.data, out_size);
    goto free;
  }

  in_data_ptr = in_info.data;
  for (plane_idx = 0; plane_idx < num_planes; plane_idx++) {
    gint j = 0;
    guint64 out_data_ptr =
        ti_memory->mem_ptr.host_ptr + plane_offset[plane_idx];
    gint copy_size =
        plane_widths[plane_idx] * plane_stride_x[plane_idx] /
        plane_steps_x[plane_idx];

    for (j = 0; j < plane_heights[plane_idx] / plane_steps_y[plane_idx]; j++) {
      memcpy ((void *) out_data_ptr, in_data_ptr, copy_size);
      total_copied += copy_size;

      in_data_ptr += copy_size;
      out_data_ptr += plane_stride_y[plane_idx];
    }
  }

  if (total_copied > in_info.size) {
    GST_CAT_ERROR (gst_tiovx_buffer_performance,
        "Copy size is larger than input size. Copy size is :%d and input size is : %lu.",
        total_copied, in_info.size);
    if (NULL != out_buffer) {
      gst_buffer_unref (out_buffer);
      out_buffer = NULL;
    }
  } else if (total_copied < in_info.size) {
    GST_CAT_WARNING (gst_tiovx_buffer_performance,
        "Copy size is smaller than input size. Copy size is :%d and input size is : %lu."
        " Ignoring remaining lines and processing", total_copied, in_info.size);
  }

free:
  if (NULL != in_buffer) {
    gst_buffer_unmap (in_buffer, &in_info);
    in_buffer = NULL;
  }
  if (NULL != memory) {
    gst_memory_unref (memory);
    memory = NULL;
  }

out:
  return out_buffer;
}

/* Gets a vx_object_array from buffer meta */
vx_object_array
gst_tiovx_get_vx_array_from_buffer (GstDebugCategory * category,
    vx_reference exemplar, GstBuffer * buffer)
{
  vx_object_array array = NULL;
  vx_enum type = VX_TYPE_INVALID;
  GstTIOVXMuxMeta *mux_meta = NULL;

  g_return_val_if_fail (category, NULL);
  g_return_val_if_fail (exemplar, NULL);
  g_return_val_if_fail (buffer, NULL);

  type = gst_tiovx_get_exemplar_type (exemplar);


  /* If a muxer generated this buffer this will be a muxmeta, regardless of
   * the exemplar type
   */
  mux_meta =
      (GstTIOVXMuxMeta *) gst_buffer_get_meta (buffer,
      GST_TYPE_TIOVX_MUX_META_API);
  if (mux_meta) {
    array = mux_meta->array;
    goto exit;
  }

  if (VX_TYPE_IMAGE == type) {
    GstTIOVXImageMeta *meta = NULL;
    meta =
        (GstTIOVXImageMeta *) gst_buffer_get_meta (buffer,
        GST_TYPE_TIOVX_IMAGE_META_API);
    if (!meta) {
      GST_CAT_LOG (category, "TIOVX Image Meta was not found in buffer");
      goto exit;
    }

    array = meta->array;
  } else if (VX_TYPE_TENSOR == type) {
    GstTIOVXTensorMeta *meta = NULL;
    meta =
        (GstTIOVXTensorMeta *) gst_buffer_get_meta (buffer,
        GST_TYPE_TIOVX_TENSOR_META_API);
    if (!meta) {
      GST_CAT_LOG (category, "TIOVX Tensor Meta was not found in buffer");
      goto exit;
    }

    array = meta->array;
  } else if (TIVX_TYPE_RAW_IMAGE == type) {
    GstTIOVXRawImageMeta *meta = NULL;
    meta =
        (GstTIOVXRawImageMeta *) gst_buffer_get_meta (buffer,
        GST_TYPE_TIOVX_RAW_IMAGE_META_API);
    if (!meta) {
      GST_CAT_LOG (category, "TIOVX Raw Image Meta was not found in buffer");
      goto exit;
    }

    array = meta->array;
  } else if (VX_TYPE_PYRAMID == type) {
    GstTIOVXPyramidMeta *meta = NULL;
    meta =
        (GstTIOVXPyramidMeta *) gst_buffer_get_meta (buffer,
        GST_TYPE_TIOVX_PYRAMID_META_API);
    if (!meta) {
      GST_CAT_LOG (category, "TIOVX Pyramid Meta was not found in buffer");
      goto exit;
    }

    array = meta->array;
  } else {
    GST_CAT_LOG (category, "Object type %d is not supported", type);
  }

exit:
  return array;
}

/* Validates if the buffer was allocated using a TIOVX pool */
GstBuffer *
gst_tiovx_validate_tiovx_buffer (GstDebugCategory * category,
    GstBufferPool ** pool, GstBuffer * buffer, vx_reference exemplar,
    GstCaps * caps, guint pool_size, gint num_channels)
{
  GstBufferPool *new_pool = NULL;
  gsize size = 0;

  g_return_val_if_fail (category, NULL);
  g_return_val_if_fail (pool, NULL);
  g_return_val_if_fail (buffer, NULL);
  g_return_val_if_fail (num_channels >= 0, FALSE);

  /* Propose allocation did not happen, there is no upstream pool therefore
   * the element has to create one */
  if (NULL == *pool) {
    GST_CAT_INFO (category,
        "Propose allocation did not occur creating new pool");

    /* We use input vx_reference to create a pool */
    size = gst_tiovx_get_size_from_exemplar (exemplar);
    if (0 >= size) {
      GST_CAT_ERROR (category, "Failed to get size from input");
      return NULL;
    }

    new_pool = gst_tiovx_create_new_pool (category, exemplar);
    if (NULL == new_pool) {
      GST_CAT_ERROR (category,
          "Failed to create new pool in transform function");
      return NULL;
    }

    if (!gst_tiovx_configure_pool (category, new_pool, exemplar,
            caps, size, pool_size, num_channels)) {
      GST_CAT_ERROR (category,
          "Unable to configure pool in transform function");
      return NULL;
    }

    if (!gst_buffer_pool_set_active (GST_BUFFER_POOL (new_pool), TRUE)) {
      GST_CAT_ERROR (category, "Failed to activate bufferpool");
      return NULL;
    }
    /* Assign the new pool to the internal value */
    *pool = new_pool;
  }

  if ((buffer)->pool != GST_BUFFER_POOL (*pool)) {
    if (GST_TIOVX_IS_BUFFER_POOL ((buffer)->pool)) {
      GST_CAT_INFO (category,
          "Buffer's and Pad's buffer pools are different, replacing the Pad's");
      gst_object_unref (*pool);

      *pool = (buffer)->pool;
      gst_object_ref (*pool);
    } else if (NULL != gst_tiovx_get_vx_array_from_buffer (category, exemplar,
            buffer)) {
      GST_CAT_LOG (category, "Buffer contains TIOVX data, skipping copy");
    } else {
      GST_CAT_DEBUG (gst_tiovx_buffer_performance,
          "Buffer doesn't come from TIOVX, copying the buffer");

      buffer =
          gst_tiovx_buffer_copy (category, GST_BUFFER_POOL (*pool), buffer,
          exemplar);
      if (NULL == buffer) {
        GST_CAT_ERROR (category, "Failure when copying input buffer from pool");
      }
    }
  }

  return buffer;
}

/* Initializes debug categories for the buffer utils functions */
void
gst_tiovx_init_buffer_utils_debug (void)
{
  GST_DEBUG_CATEGORY_GET (gst_tiovx_buffer_performance, "GST_PERFORMANCE");
}
