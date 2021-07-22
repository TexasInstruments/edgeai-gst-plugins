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

#include "gsttiovxmeta.h"

#include <gst/video/video.h>
#include <TI/tivx.h>

#include "gsttiovxutils.h"

static const vx_size k_tiovx_array_lenght = 1;

static gboolean gst_tiovx_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);

GType
gst_tiovx_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTIOVXMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_tiovx_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_TIOVX_META_API_TYPE,
        "GstTIOVXMeta",
        sizeof (GstTIOVXMeta),
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
  /* Gst requieres this func to be implemented, even if it is empty */
  return TRUE;
}



static gint
gst_tiovx_buffer_pool_get_plane_stride (const vx_image image,
    const gint plane_index)
{
  vx_status status;
  vx_rectangle_t rect;
  vx_map_id map_id;
  vx_imagepatch_addressing_t addr;
  void *ptr;
  vx_enum usage = VX_READ_ONLY;
  vx_enum mem_type = VX_MEMORY_TYPE_NONE;
  vx_uint32 flags = VX_NOGAP_X;
  guint img_width = 0, img_height = 0;

  vxQueryImage (image, VX_IMAGE_WIDTH, &img_width, sizeof (img_width));
  vxQueryImage (image, VX_IMAGE_HEIGHT, &img_height, sizeof (img_height));

  /* Create a rectangle that encompasses the complete image */
  rect.start_x = 0;
  rect.start_y = 0;
  rect.end_x = img_width;
  rect.end_y = img_height;

  status = vxMapImagePatch (image, &rect, plane_index,
      &map_id, &addr, &ptr, usage, mem_type, flags);
  if (status != VX_SUCCESS) {
    return -1;
  }

  return addr.stride_y;
}


GstTIOVXMeta *
gst_buffer_add_tiovx_meta (GstBuffer * buffer, const vx_reference exemplar,
    guint64 mem_start)
{
  GstTIOVXMeta *tiovx_meta = NULL;
  void *addr[MODULE_MAX_NUM_PLANES] = { NULL };
  void *plane_addr[MODULE_MAX_NUM_PLANES] = { NULL };
  gsize plane_offset[MODULE_MAX_NUM_PLANES] = { 0 };
  gint plane_strides[MODULE_MAX_NUM_PLANES] = { 0 };
  vx_uint32 plane_sizes[MODULE_MAX_NUM_PLANES];
  guint num_planes = 0;
  guint plane_idx = 0;
  gint prev_size = 0;
  vx_object_array array;
  vx_image ref = NULL;
  vx_df_image vx_format = VX_DF_IMAGE_VIRT;
  vx_status status;

  g_return_val_if_fail (buffer, NULL);

  /* Get plane and size information */
  tivxReferenceExportHandle ((vx_reference) exemplar,
      plane_addr, plane_sizes, MODULE_MAX_NUM_PLANES, &num_planes);

  for (plane_idx = 0; plane_idx < num_planes; plane_idx++) {
    addr[plane_idx] = (void *) (mem_start + prev_size);
    plane_offset[plane_idx] = prev_size;
    plane_strides[plane_idx] =
        gst_tiovx_buffer_pool_get_plane_stride ((vx_image) exemplar, plane_idx);

    prev_size = plane_sizes[plane_idx];
  }

  array =
      vxCreateObjectArray (vxGetContext (exemplar), exemplar,
      k_tiovx_array_lenght);

  /* Import memory into the meta's vx reference */
  ref = (vx_image) vxGetObjectArrayItem (array, 0);
  status =
      tivxReferenceImportHandle ((vx_reference) ref, (const void **) addr,
      plane_sizes, num_planes);

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
      (GstTIOVXMeta *) gst_buffer_add_meta (buffer,
      gst_tiovx_meta_get_info (), NULL);
  tiovx_meta->array = array;

  tiovx_meta->image_info.num_planes = num_planes;
  for (plane_idx = 0; plane_idx < num_planes; plane_idx++) {
    tiovx_meta->image_info.plane_offset[plane_idx] = plane_offset[plane_idx];
    tiovx_meta->image_info.plane_strides[plane_idx] = plane_strides[plane_idx];
    tiovx_meta->image_info.plane_sizes[plane_idx] = plane_sizes[plane_idx];
  }

  /* Retrieve width, height and format from exemplar */
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_WIDTH,
      &tiovx_meta->image_info.width, sizeof (tiovx_meta->image_info.width));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_HEIGHT,
      &tiovx_meta->image_info.height, sizeof (tiovx_meta->image_info.height));
  vxQueryImage ((vx_image) exemplar, VX_IMAGE_FORMAT, &vx_format,
      sizeof (vx_format));
  tiovx_meta->image_info.format = vx_format_to_gst_format (vx_format);

  goto out;

err_out:
  vxReleaseObjectArray (&array);
out:
  return tiovx_meta;
}
