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

#include "gsttiovxutils.h"

#include <TI/j7.h>

#include "gsttiovxallocator.h"
#include "gsttiovxbufferpool.h"
#include "gsttiovxmeta.h"
#include "gsttiovxtensorbufferpool.h"
#include "gsttiovxtensormeta.h"

#define MAX_NUMBER_OF_PLANES 4

static const gsize kcopy_all_size = -1;

/* Convert VX Image Format to GST Image Format */
GstVideoFormat
vx_format_to_gst_format (const vx_df_image format)
{
  GstVideoFormat gst_format = GST_VIDEO_FORMAT_UNKNOWN;

  switch (format) {
    case VX_DF_IMAGE_RGB:
      gst_format = GST_VIDEO_FORMAT_RGB;
      break;
    case VX_DF_IMAGE_RGBX:
      gst_format = GST_VIDEO_FORMAT_RGBx;
      break;
    case VX_DF_IMAGE_NV12:
      gst_format = GST_VIDEO_FORMAT_NV12;
      break;
    case VX_DF_IMAGE_NV21:
      gst_format = GST_VIDEO_FORMAT_NV21;
      break;
    case VX_DF_IMAGE_UYVY:
      gst_format = GST_VIDEO_FORMAT_UYVY;
      break;
    case VX_DF_IMAGE_YUYV:
      gst_format = GST_VIDEO_FORMAT_YUY2;
      break;
    case VX_DF_IMAGE_IYUV:
      gst_format = GST_VIDEO_FORMAT_I420;
      break;
    case VX_DF_IMAGE_YUV4:
      gst_format = GST_VIDEO_FORMAT_Y444;
      break;
    case VX_DF_IMAGE_U8:
      gst_format = GST_VIDEO_FORMAT_GRAY8;
      break;
    case VX_DF_IMAGE_U16:
      gst_format = GST_VIDEO_FORMAT_GRAY16_LE;
      break;
    default:
      gst_format = -1;
      break;
  }

  return gst_format;
}

/* Convert GST Image Format to VX Image Format */
vx_df_image
gst_format_to_vx_format (const GstVideoFormat gst_format)
{
  vx_df_image vx_format = VX_DF_IMAGE_VIRT;

  switch (gst_format) {
    case GST_VIDEO_FORMAT_RGB:
      vx_format = VX_DF_IMAGE_RGB;
      break;
    case GST_VIDEO_FORMAT_RGBx:
      vx_format = VX_DF_IMAGE_RGBX;
      break;
    case GST_VIDEO_FORMAT_NV12:
      vx_format = VX_DF_IMAGE_NV12;
      break;
    case GST_VIDEO_FORMAT_NV21:
      vx_format = VX_DF_IMAGE_NV21;
      break;
    case GST_VIDEO_FORMAT_UYVY:
      vx_format = VX_DF_IMAGE_UYVY;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      vx_format = VX_DF_IMAGE_YUYV;
      break;
    case GST_VIDEO_FORMAT_I420:
      vx_format = VX_DF_IMAGE_IYUV;
      break;
    case GST_VIDEO_FORMAT_Y444:
      vx_format = VX_DF_IMAGE_YUV4;
      break;
    case GST_VIDEO_FORMAT_GRAY8:
      vx_format = VX_DF_IMAGE_U8;
      break;
    case GST_VIDEO_FORMAT_GRAY16_LE:
      vx_format = VX_DF_IMAGE_U16;
      break;
    default:
      vx_format = -1;
      break;
  }

  return vx_format;
}

/* Transfers handles between vx_references */
vx_status
gst_tiovx_transfer_handle (GstDebugCategory * category, vx_reference src,
    vx_reference dest)
{
  vx_status status = VX_SUCCESS;
  uint32_t num_entries = 0;
  vx_size src_num_planes = 0;
  vx_size dest_num_planes = 0;
  void *addr[MAX_NUMBER_OF_PLANES] = { NULL };
  uint32_t bufsize[MAX_NUMBER_OF_PLANES] = { 0 };

  g_return_val_if_fail (category, VX_FAILURE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) src), VX_FAILURE);
  g_return_val_if_fail (VX_SUCCESS ==
      vxGetStatus ((vx_reference) dest), VX_FAILURE);

  status =
      vxQueryImage ((vx_image) dest, VX_IMAGE_PLANES, &dest_num_planes,
      sizeof (dest_num_planes));
  if (VX_SUCCESS != status) {
    GST_CAT_ERROR (category,
        "Get number of planes in dest image failed %" G_GINT32_FORMAT, status);
    return status;
  }

  status =
      vxQueryImage ((vx_image) src, VX_IMAGE_PLANES, &src_num_planes,
      sizeof (src_num_planes));
  if (VX_SUCCESS != status) {
    GST_CAT_ERROR (category,
        "Get number of planes in src image failed %" G_GINT32_FORMAT, status);
    return status;
  }

  if (src_num_planes != dest_num_planes) {
    GST_CAT_ERROR (category,
        "Incompatible number of planes in src and dest images. src: %ld and dest: %ld",
        src_num_planes, dest_num_planes);
    return VX_FAILURE;
  }

  status =
      tivxReferenceExportHandle (src, addr, bufsize, src_num_planes,
      &num_entries);
  if (VX_SUCCESS != status) {
    GST_CAT_ERROR (category, "Export handle failed %" G_GINT32_FORMAT, status);
    return status;
  }

  GST_CAT_LOG (category, "Number of planes to transfer: %ld", src_num_planes);

  if (src_num_planes != num_entries) {
    GST_CAT_ERROR (category,
        "Incompatible number of planes and handles entries. planes: %ld and entries: %d",
        src_num_planes, num_entries);
    return VX_FAILURE;
  }

  status =
      tivxReferenceImportHandle (dest, (const void **) addr, bufsize,
      dest_num_planes);
  if (VX_SUCCESS != status) {
    GST_CAT_ERROR (category, "Import handle failed %" G_GINT32_FORMAT, status);
    return status;
  }

  return status;
}

gboolean
gst_tiovx_configure_pool (GstDebugCategory * category, GstBufferPool * pool,
    vx_reference * exemplar, GstCaps * caps, gsize size, guint num_buffers)
{
  GstStructure *config = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (category, FALSE);
  g_return_val_if_fail (pool, FALSE);
  g_return_val_if_fail (exemplar, FALSE);
  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (size > 0, FALSE);
  g_return_val_if_fail (num_buffers > 0, FALSE);

  config = gst_buffer_pool_get_config (pool);

  gst_tiovx_buffer_pool_config_set_exemplar (config, *exemplar);
  gst_buffer_pool_config_set_params (config, caps, size, num_buffers,
      num_buffers);

  if (!gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), FALSE)) {
    GST_CAT_ERROR (category,
        "Unable to set pool to inactive for configuration");
    gst_object_unref (pool);
    goto exit;
  }

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_CAT_ERROR (category, "Unable to set pool configuration");
    gst_object_unref (pool);
    goto exit;
  }
  gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), TRUE);

  ret = TRUE;

exit:
  return ret;
}

/* Gets exemplar type */
vx_enum
gst_tiovx_get_exemplar_type (vx_reference * exemplar)
{
  vx_enum type = VX_TYPE_INVALID;
  vx_status status = VX_FAILURE;

  g_return_val_if_fail (exemplar, -1);

  status =
      vxQueryReference ((vx_reference) * exemplar, (vx_enum) VX_REFERENCE_TYPE,
      &type, sizeof (vx_enum));
  if (VX_SUCCESS != status) {
    return VX_TYPE_INVALID;
  }

  return type;
}

/* Creates a new pool based on exemplar */
GstBufferPool *
gst_tiovx_create_new_pool (GstDebugCategory * category, vx_reference * exemplar)
{
  GstBufferPool *pool = NULL;
  vx_enum type = VX_TYPE_INVALID;

  g_return_val_if_fail (category, NULL);
  g_return_val_if_fail (exemplar, NULL);

  GST_CAT_INFO (category, "Creating new pool");

  /* Getting reference type */
  type = gst_tiovx_get_exemplar_type (exemplar);

  if (VX_TYPE_IMAGE == type) {
    GST_CAT_INFO (category, "Creating Image buffer pool");
    pool = g_object_new (GST_TIOVX_TYPE_BUFFER_POOL, NULL);
  } else if (VX_TYPE_TENSOR == type) {
    GST_CAT_INFO (category, "Creating Tensor buffer pool");
    pool = g_object_new (GST_TIOVX_TYPE_TENSOR_BUFFER_POOL, NULL);
  } else {
    GST_CAT_ERROR (category,
        "Type %d not supported, buffer pool was not created", type);
  }

  return pool;
}

/* Adds a pool to the query */
gboolean
gst_tiovx_add_new_pool (GstDebugCategory * category, GstQuery * query,
    guint num_buffers, vx_reference * exemplar, gsize size,
    GstBufferPool ** buffer_pool)
{
  GstCaps *caps = NULL;
  GstBufferPool *pool = NULL;

  g_return_val_if_fail (category, FALSE);
  g_return_val_if_fail (query, FALSE);
  g_return_val_if_fail (exemplar, FALSE);
  g_return_val_if_fail (size > 0, FALSE);

  GST_CAT_DEBUG (category, "Adding new pool");

  pool = gst_tiovx_create_new_pool (category, exemplar);

  if (!pool) {
    GST_CAT_ERROR (category, "Create TIOVX pool failed");
    return FALSE;
  }

  gst_query_parse_allocation (query, &caps, NULL);

  if (!gst_tiovx_configure_pool (category, pool, exemplar, caps, size,
          num_buffers)) {
    GST_CAT_ERROR (category, "Unable to configure pool");
    return FALSE;
  }

  GST_CAT_INFO (category, "Adding new TIOVX pool with %d buffers of %ld size",
      num_buffers, size);

  gst_query_add_allocation_pool (query, pool, size, num_buffers, num_buffers);

  if (buffer_pool) {
    *buffer_pool = pool;
  } else {
    gst_object_unref (pool);
  }

  return TRUE;
}

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

  ti_memory =
      gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (memory),
      _tiovx_mem_ptr_quark);
  size = gst_memory_get_sizes (memory, NULL, NULL);

  memcpy ((void *) ti_memory->mem_ptr.host_ptr, in_info.data, size);

  gst_buffer_unmap (in_buffer, &in_info);

out:
  return out_buffer;
}

/* Get bit depth from a tensor data type */
vx_uint32
gst_tiovx_tensor_get_tensor_bit_depth (vx_enum data_type)
{
  vx_uint32 bit_depth = 0;

  switch (data_type) {
    case VX_TYPE_UINT8:
      bit_depth = sizeof (vx_uint8);
      break;
    case VX_TYPE_INT8:
      bit_depth = sizeof (vx_int8);
      break;
    case VX_TYPE_UINT16:
      bit_depth = sizeof (vx_uint16);
      break;
    case VX_TYPE_INT16:
      bit_depth = sizeof (vx_int16);
      break;
    case VX_TYPE_UINT32:
      bit_depth = sizeof (vx_uint32);
      break;
    case VX_TYPE_INT32:
      bit_depth = sizeof (vx_int32);
      break;
    case VX_TYPE_FLOAT32:
      bit_depth = sizeof (vx_float32);
      break;
    default:
      bit_depth = -1;
      break;
  }

  return bit_depth;
}

/**
 * This function clears the memory pointers from the exemplars.
 * The actual memory will be cleared by the allocator, this avoid a double
 * free of the memory
 * */
vx_status
gst_tiovx_empty_exemplar (vx_reference ref)
{
  vx_status status = VX_SUCCESS;
  void *addr[MODULE_MAX_NUM_ADDRS] = { NULL };
  vx_uint32 sizes[MODULE_MAX_NUM_ADDRS];
  uint32_t num_addrs;

  g_return_val_if_fail (ref, VX_FAILURE);

  tivxReferenceExportHandle (ref,
      addr, sizes, MODULE_MAX_NUM_ADDRS, &num_addrs);

  for (int i = 0; i < num_addrs; i++) {
    addr[i] = NULL;
  }

  status = tivxReferenceImportHandle (ref,
      (const void **) addr, (const uint32_t *) sizes, num_addrs);

  return status;
}

/* Sets an exemplar to a TIOVX bufferpool configuration */
void
gst_tiovx_buffer_pool_config_set_exemplar (GstStructure * config,
    const vx_reference exemplar)
{
  g_return_if_fail (config != NULL);

  gst_structure_set (config, "vx-exemplar", G_TYPE_INT64, exemplar, NULL);
}

/* Gets an exemplar from a TIOVX bufferpool configuration */
void
gst_tiovx_buffer_pool_config_get_exemplar (GstStructure * config,
    vx_reference * exemplar)
{
  g_return_if_fail (config != NULL);
  g_return_if_fail (exemplar != NULL);

  gst_structure_get (config, "vx-exemplar", G_TYPE_INT64, exemplar, NULL);
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
    /* TODO add case for tensor buffer pool */
    if (GST_TIOVX_IS_BUFFER_POOL ((buffer)->pool)) {
      GST_CAT_INFO (category,
          "Buffer's and Pad's buffer pools are different, replacing the Pad's");
      gst_object_unref (*pool);

      *pool = (buffer)->pool;
      gst_object_ref (*pool);
    } else {
      GST_CAT_INFO (category,
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
        (GstTIOVXMeta *) gst_buffer_get_meta (buffer, GST_TIOVX_META_API_TYPE);
    if (!meta) {
      GST_CAT_ERROR (category, "TIOVX Meta was not found in buffer");
      goto exit;
    }

    array = meta->array;
  } else if (VX_TYPE_TENSOR == type) {
    GstTIOVXTensorMeta *meta = NULL;
    meta =
        (GstTIOVXTensorMeta *) gst_buffer_get_meta (buffer,
        GST_TIOVX_TENSOR_META_API_TYPE);
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

/* Gets size from exemplar and caps */
gsize
gst_tiovx_get_size_from_exemplar (vx_reference * exemplar, GstCaps * caps)
{
  gsize size = 0;
  vx_enum type = VX_TYPE_INVALID;

  g_return_val_if_fail (exemplar, 0);
  g_return_val_if_fail (caps, 0);

  type = gst_tiovx_get_exemplar_type (exemplar);

  if (VX_TYPE_IMAGE == type) {
    GstVideoInfo info;

    if (gst_video_info_from_caps (&info, caps)) {
      size = GST_VIDEO_INFO_SIZE (&info);
    }
  } else if (VX_TYPE_TENSOR == type) {
    void *dim_addr[MODULE_MAX_NUM_TENSORS] = { NULL };
    vx_uint32 dim_sizes[MODULE_MAX_NUM_TENSORS] = { 0 };
    vx_uint32 num_dims = 0;

    /* Check memory size */
    tivxReferenceExportHandle ((vx_reference) * exemplar,
        dim_addr, dim_sizes, MODULE_MAX_NUM_TENSORS, &num_dims);

    /* TI indicated tensors have 1 single block of memory */
    size = dim_sizes[0];
  }

  return size;
}
