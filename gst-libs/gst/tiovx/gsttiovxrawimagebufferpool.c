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
 * *    No reverse engineering, decompilation, or disassembly of this software
 *      is permitted with respect to any software provided in binary form.
 *
 * *    Any redistribution and use are licensed by TI for use only with TI Devices.
 *
 * *    Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 *
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
 *
 * *    Any redistribution and use of the source code, including any resulting
 *      derivative works, are licensed by TI for use only with TI Devices.
 *
 * *    Any redistribution and use of any object code compiled from the source
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttiovxrawimagebufferpool.h"
#include "gsttiovxrawimagemeta.h"
#include "gsttiovxutils.h"

/* TIOVX RawImageBufferPool */

struct _GstTIOVXRawImageBufferPool
{
  GstTIOVXBufferPool parent;

  /* We use this variable to keep the format from the caps, ideally this could
   * be converted from the vx_format, however currently for bayer the same
   * vx_raw_format matches several gst_formats
   */
  const gchar *gst_format;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_raw_image_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_tiovx_raw_image_buffer_pool_debug

#define gst_tiovx_raw_image_buffer_pool_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstTIOVXRawImageBufferPool,
    gst_tiovx_raw_image_buffer_pool, GST_TYPE_TIOVX_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_raw_image_buffer_pool_debug,
        "tiovxrawimagebufferpool", 0,
        "debug category for the tiovxrawimagebufferpool element"));

static gboolean
gst_tiovx_raw_image_buffer_pool_validate_caps (GstTIOVXBufferPool * self,
    const GstCaps * caps, const vx_reference exemplar);
static void
gst_tiovx_raw_image_buffer_pool_add_meta_to_buffer (GstTIOVXBufferPool * self,
    GstBuffer * buffer, vx_reference reference, guint num_channels,
    GstTIOVXMemoryData * ti_memory);
static void gst_tiovx_raw_image_buffer_pool_free_buffer_meta (GstTIOVXBufferPool
    * self, GstBuffer * buffer);

static void
gst_tiovx_raw_image_buffer_pool_class_init (GstTIOVXRawImageBufferPoolClass *
    klass)
{
  GstTIOVXBufferPoolClass *gsttiovxbufferpool_class = NULL;

  gsttiovxbufferpool_class = GST_TIOVX_BUFFER_POOL_CLASS (klass);

  gsttiovxbufferpool_class->validate_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_raw_image_buffer_pool_validate_caps);
  gsttiovxbufferpool_class->add_meta_to_buffer =
      GST_DEBUG_FUNCPTR (gst_tiovx_raw_image_buffer_pool_add_meta_to_buffer);
  gsttiovxbufferpool_class->free_buffer_meta =
      GST_DEBUG_FUNCPTR (gst_tiovx_raw_image_buffer_pool_free_buffer_meta);
}

static void
gst_tiovx_raw_image_buffer_pool_init (GstTIOVXRawImageBufferPool * self)
{
}

static gboolean
gst_tiovx_raw_image_buffer_pool_validate_caps (GstTIOVXBufferPool * self,
    const GstCaps * caps, const vx_reference exemplar)
{
  const gchar *gst_format = NULL;
  GstTIOVXRawImageBufferPool *raw_buffer_pool = NULL;
  GstStructure *structure = NULL;
  gint gst_img_width = 0, gst_img_height = 0;
  gint gst_meta_height_before = 0, gst_meta_height_after = 0;
  tivx_raw_image_format_t tivx_format = { };
  uint32_t tivx_img_width = 0, tivx_img_height = 0;
  gboolean ret = FALSE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (caps, FALSE);
  g_return_val_if_fail (exemplar, FALSE);

  raw_buffer_pool = GST_TIOVX_RAW_IMAGE_BUFFER_POOL (self);

  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "width", &gst_img_width);
  gst_structure_get_int (structure, "height", &gst_img_height);
  gst_structure_get_int (structure, "meta-height-before",
      &gst_meta_height_before);
  gst_structure_get_int (structure, "meta-height-after",
      &gst_meta_height_after);

  gst_format = gst_structure_get_string (structure, "format");

  tivxQueryRawImage ((tivx_raw_image) exemplar, TIVX_RAW_IMAGE_WIDTH,
      &tivx_img_width, sizeof (tivx_img_width));
  tivxQueryRawImage ((tivx_raw_image) exemplar, TIVX_RAW_IMAGE_HEIGHT,
      &tivx_img_height, sizeof (tivx_img_height));
  tivxQueryRawImage ((tivx_raw_image) exemplar, TIVX_RAW_IMAGE_FORMAT,
      &tivx_format, sizeof (tivx_format));

  if (gst_img_width != tivx_img_width) {
    GST_ERROR_OBJECT (self, "Exemplar and caps's width don't match");
    goto out;
  }

  if (gst_img_height - gst_meta_height_before - gst_meta_height_after
        != tivx_img_height) {
    GST_ERROR_OBJECT (self, "Exemplar and caps's height don't match");
    goto out;
  }

  if (gst_format_to_tivx_raw_format (gst_format) != tivx_format.pixel_container) {
    GST_ERROR_OBJECT (self, "Exemplar and caps's format don't match");
    goto out;
  }

  raw_buffer_pool->gst_format = gst_format;

  ret = TRUE;

out:
  return ret;
}

static void
gst_tiovx_raw_image_buffer_pool_add_meta_to_buffer (GstTIOVXBufferPool * self,
    GstBuffer * buffer, vx_reference exemplar, guint num_channels,
    GstTIOVXMemoryData * ti_memory)
{

  g_return_if_fail (self);
  g_return_if_fail (buffer);
  g_return_if_fail (exemplar);
  g_return_if_fail (num_channels >= MIN_NUM_CHANNELS);
  g_return_if_fail (num_channels <= MAX_NUM_CHANNELS);
  g_return_if_fail (ti_memory);

  gst_buffer_add_tiovx_raw_image_meta (buffer, exemplar,
      ti_memory->mem_ptr.host_ptr);
}

static void
gst_tiovx_raw_image_buffer_pool_free_buffer_meta (GstTIOVXBufferPool * self,
    GstBuffer * buffer)
{
  GstTIOVXRawImageMeta *tiovx_meta = NULL;
  vx_reference ref = NULL;

  g_return_if_fail (self);
  g_return_if_fail (buffer);

  tiovx_meta =
      (GstTIOVXRawImageMeta *) gst_buffer_get_meta (buffer,
      GST_TYPE_TIOVX_RAW_IMAGE_META_API);
  if (NULL != tiovx_meta) {
    if (NULL != tiovx_meta->array) {
      /* We currently support a single channel */
      ref = vxGetObjectArrayItem (tiovx_meta->array, 0);
      gst_tiovx_empty_exemplar (ref);
      vxReleaseReference (&ref);

      vxReleaseObjectArray (&tiovx_meta->array);
    }
  }

}
