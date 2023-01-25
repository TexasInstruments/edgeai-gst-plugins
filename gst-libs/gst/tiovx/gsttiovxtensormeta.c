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

#include "gsttiovxtensormeta.h"

#include <TI/tivx.h>

static gboolean gst_tiovx_tensor_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);

GType
gst_tiovx_tensor_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      /* No Video tag needed */
  { GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTIOVXTensorMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_tiovx_tensor_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_TYPE_TIOVX_TENSOR_META_API,
        "GstTIOVXTensorMeta",
        sizeof (GstTIOVXTensorMeta),
        gst_tiovx_tensor_meta_init,
        NULL,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

static gboolean
gst_tiovx_tensor_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  /* Gst requires this func to be implemented, even if it is empty */
  return TRUE;
}

GstTIOVXTensorMeta *
gst_buffer_add_tiovx_tensor_meta (GstBuffer * buffer,
    const vx_reference exemplar, const gint array_length, guint64 mem_start)
{
  GstTIOVXTensorMeta *tiovx_tensor_meta = NULL;
  void *addr[MODULE_MAX_NUM_TENSORS] = { NULL };
  void *tensor_addr[MODULE_MAX_NUM_TENSORS] = { NULL };
  vx_uint32 tensor_size[MODULE_MAX_NUM_TENSORS];
  vx_uint32 num_tensors = 0;
  vx_object_array array = NULL;
  vx_status status = VX_SUCCESS;
  vx_size dim_sizes[MODULE_MAX_NUM_DIMS];
  vx_size dim_strides[MODULE_MAX_NUM_DIMS] = { 0 };
  vx_size num_dims = 0;
  vx_uint32 dim_idx = 0;
  vx_enum data_type = 0;
  vx_size last_offset = 0;
  gint prev_size = 0;
  gint i = 0;

  g_return_val_if_fail (buffer, NULL);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) exemplar),
      NULL);
  g_return_val_if_fail (array_length > 0, NULL);

  /* Get addr and size information */
  tivxReferenceExportHandle ((vx_reference) exemplar,
      tensor_addr, tensor_size, MODULE_MAX_NUM_DIMS, &num_tensors);

  g_return_val_if_fail (MODULE_MAX_NUM_TENSORS == num_tensors, NULL);

  /* Create new array based on exemplar */
  array = vxCreateObjectArray (vxGetContext (exemplar), exemplar, array_length);

  for (i = 0; i < array_length; i++) {
    vx_reference ref = NULL;

    ref = vxGetObjectArrayItem (array, i);
    addr[0] = (void *) (mem_start + prev_size);

    status =
        tivxReferenceImportHandle ((vx_reference) ref, (const void **) addr,
        tensor_size, num_tensors);

    if (ref != NULL) {
      vxReleaseReference ((vx_reference *) & ref);
    }

    if (status != VX_SUCCESS) {
      GST_ERROR_OBJECT (buffer,
          "Unable to import tivx_shared_mem_ptr to a vx_image: %"
          G_GINT32_FORMAT, status);
      goto err_out;
    }

    prev_size += tensor_size[0];
  }

  /* Add tensor meta to the buffer */
  tiovx_tensor_meta =
      (GstTIOVXTensorMeta *) gst_buffer_add_meta (buffer,
      gst_tiovx_tensor_meta_get_info (), NULL);

  /* Retrieve tensor info from exemplar */
  vxQueryTensor ((vx_tensor) exemplar, VX_TENSOR_NUMBER_OF_DIMS,
      &num_dims, sizeof (vx_size));
  vxQueryTensor ((vx_tensor) exemplar, VX_TENSOR_DIMS, dim_sizes,
      num_dims * sizeof (vx_size));
  vxQueryTensor ((vx_tensor) exemplar, VX_TENSOR_DATA_TYPE, &data_type,
      sizeof (vx_enum));
  vxQueryTensor ((vx_tensor) exemplar, TIVX_TENSOR_STRIDES, dim_strides,
      sizeof (dim_strides));

  /* Fill tensor info */
  tiovx_tensor_meta->array = array;
  tiovx_tensor_meta->tensor_info.num_dims = num_dims;
  tiovx_tensor_meta->tensor_info.data_type = data_type;
  tiovx_tensor_meta->tensor_info.tensor_size = tensor_size[0];
  for (dim_idx = 0; dim_idx < num_dims; dim_idx++) {
    tiovx_tensor_meta->tensor_info.dim_strides[dim_idx] = last_offset;
    tiovx_tensor_meta->tensor_info.dim_strides[dim_idx] = dim_strides[dim_idx];
    tiovx_tensor_meta->tensor_info.dim_sizes[dim_idx] = dim_sizes[dim_idx];

    last_offset += dim_sizes[dim_idx];
  }

  goto out;

err_out:
  vxReleaseObjectArray (&array);
out:
  return tiovx_tensor_meta;
}
