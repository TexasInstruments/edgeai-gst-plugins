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

#include "gsttiovxrawimagemeta.h"

#include <TI/tivx.h>

#include "gsttiovxutils.h"

static const vx_size tiovx_array_lenght = 1;

static gboolean gst_tiovx_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);

GType
gst_tiovx_raw_image_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTIOVXRawImageMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_tiovx_raw_image_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_TYPE_TIOVX_RAW_IMAGE_META_API,
        "GstTIOVXRawImageMeta",
        sizeof (GstTIOVXRawImageMeta),
        gst_tiovx_meta_init,
        NULL,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

static gboolean
gst_tiovx_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  /* Gst requires this func to be implemented, even if it is empty */
  return TRUE;
}

static void
gst_tiovx_raw_meta_extract_image_params (const tivx_raw_image image,
    const gint index, gint * stride_x, gint * stride_y, gint * step_x,
    gint * step_y, gint * width, gint * height)
{
  vx_imagepatch_addressing_t img_addr[MODULE_MAX_NUM_EXPOSURES];

  g_return_if_fail (NULL != image);
  g_return_if_fail (NULL != stride_x);
  g_return_if_fail (NULL != stride_y);
  g_return_if_fail (NULL != step_x);
  g_return_if_fail (NULL != width);
  g_return_if_fail (NULL != height);

  tivxQueryRawImage (image, TIVX_RAW_IMAGE_IMAGEPATCH_ADDRESSING,
      img_addr, sizeof (img_addr));

  *stride_x = img_addr[index].stride_x;
  *stride_y = img_addr[index].stride_y;
  *step_x = img_addr[index].step_x;
  *step_y = img_addr[index].step_y;
  *width = img_addr[index].dim_x;
  *height = img_addr[index].dim_y;
}

GstTIOVXRawImageMeta *
gst_buffer_add_tiovx_raw_image_meta (GstBuffer * buffer,
    const vx_reference exemplar, guint64 mem_start)
{
  GstTIOVXRawImageMeta *tiovx_meta = NULL;
  void *addr[MODULE_MAX_NUM_EXPOSURES] = { NULL };
  uint32_t sizes[MODULE_MAX_NUM_EXPOSURES] = { 0 };
  gsize offset[MODULE_MAX_NUM_EXPOSURES] = { 0 };
  gint stride_x[MODULE_MAX_NUM_EXPOSURES] = { 0 };
  gint stride_y[MODULE_MAX_NUM_EXPOSURES] = { 0 };
  gint steps_x[MODULE_MAX_NUM_EXPOSURES] = { 0 };
  gint steps_y[MODULE_MAX_NUM_EXPOSURES] = { 0 };
  gint widths[MODULE_MAX_NUM_EXPOSURES] = { 0 };
  gint heights[MODULE_MAX_NUM_EXPOSURES] = { 0 };
  guint num_exposures = 0;
  guint exposure_idx = 0;
  gint prev_size = 0;
  vx_object_array array;
  vx_image ref = NULL;
  vx_status status;

  g_return_val_if_fail (buffer, NULL);
  g_return_val_if_fail (VX_SUCCESS == vxGetStatus ((vx_reference) exemplar),
      NULL);

  /* Get exposure and size information */
  tivxReferenceExportHandle ((vx_reference) exemplar,
      addr, sizes, MODULE_MAX_NUM_EXPOSURES, &num_exposures);

  for (exposure_idx = 0; exposure_idx < num_exposures; exposure_idx++) {
    addr[exposure_idx] = (void *) (mem_start + prev_size);
    offset[exposure_idx] = prev_size;

    gst_tiovx_raw_meta_extract_image_params ((tivx_raw_image) exemplar,
        exposure_idx, &stride_x[exposure_idx], &stride_y[exposure_idx],
        &steps_x[exposure_idx], &steps_y[exposure_idx], &widths[exposure_idx],
        &heights[exposure_idx]);

    prev_size += sizes[exposure_idx];
  }

  array =
      vxCreateObjectArray (vxGetContext (exemplar), exemplar,
      tiovx_array_lenght);

  /* Import memory into the meta's vx reference */
  ref = (vx_image) vxGetObjectArrayItem (array, 0);
  status =
      tivxReferenceImportHandle ((vx_reference) ref, (const void **) addr,
      sizes, num_exposures);

  if (ref != NULL) {
    vxReleaseReference ((vx_reference *) & ref);
  }
  if (status != VX_SUCCESS) {
    GST_ERROR_OBJECT (buffer,
        "Unable to import tivx_shared_mem_ptr to a vx_image: %" G_GINT32_FORMAT,
        status);
    goto err_out;
  }

  tiovx_meta =
      (GstTIOVXRawImageMeta *) gst_buffer_add_meta (buffer,
      gst_tiovx_raw_image_meta_get_info (), NULL);
  tiovx_meta->array = array;

  tiovx_meta->image_info.num_exposures = num_exposures;
  for (exposure_idx = 0; exposure_idx < num_exposures; exposure_idx++) {
    tiovx_meta->image_info.exposure_offset[exposure_idx] = offset[exposure_idx];
    tiovx_meta->image_info.exposure_stride_x[exposure_idx] =
        stride_x[exposure_idx];
    tiovx_meta->image_info.exposure_stride_y[exposure_idx] =
        stride_y[exposure_idx];
    tiovx_meta->image_info.exposure_sizes[exposure_idx] = sizes[exposure_idx];
    tiovx_meta->image_info.exposure_steps_x[exposure_idx] =
        steps_x[exposure_idx];
    tiovx_meta->image_info.exposure_steps_y[exposure_idx] =
        steps_y[exposure_idx];
    tiovx_meta->image_info.exposure_widths[exposure_idx] = widths[exposure_idx];
    tiovx_meta->image_info.exposure_heights[exposure_idx] =
        heights[exposure_idx];
  }

  goto out;

err_out:
  vxReleaseObjectArray (&array);
out:
  return tiovx_meta;
}
