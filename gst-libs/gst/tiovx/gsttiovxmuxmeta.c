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

#include "gsttiovxmuxmeta.h"

#include <TI/tivx.h>

#include "gsttiovxutils.h"

static gboolean gst_tiovx_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);
static void gst_tiovx_meta_free (GstMeta * meta, GstBuffer * buffer);

GType
gst_tiovx_mux_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTIOVXMuxMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_tiovx_mux_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_TYPE_TIOVX_MUX_META_API,
        "GstTIOVXMuxMeta",
        sizeof (GstTIOVXMuxMeta),
        gst_tiovx_meta_init,
        gst_tiovx_meta_free,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

static gboolean
gst_tiovx_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstTIOVXMuxMeta *tiovx_meta = (GstTIOVXMuxMeta *) meta;

  tiovx_meta->array = NULL;

  return TRUE;
}

static void
gst_tiovx_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstTIOVXMuxMeta *tiovx_meta = (GstTIOVXMuxMeta *) meta;

  if (NULL != tiovx_meta->array) {
    vx_size array_size = 0;
    gint i = 0;
    vx_status status = VX_FAILURE;

    status =
        vxQueryObjectArray (tiovx_meta->array, VX_OBJECT_ARRAY_NUMITEMS,
        &array_size, sizeof (array_size));
    if (VX_SUCCESS != status) {
      GST_ERROR_OBJECT (meta, "Failed reading the array size: %d", status);
    }

    /* Empty the memory for all references to avoid a double-free  */
    for (i = 0; i < array_size; i++) {
      vx_reference ref = NULL;

      ref = vxGetObjectArrayItem (tiovx_meta->array, i);
      gst_tiovx_empty_exemplar (ref);
      vxReleaseReference (&ref);
    }

    vxReleaseObjectArray (&tiovx_meta->array);
  }
}

GstTIOVXMuxMeta *
gst_buffer_add_tiovx_mux_meta (GstBuffer * buffer, const vx_reference exemplar)
{
  GstTIOVXMuxMeta *tiovx_meta = NULL;

  tiovx_meta =
      (GstTIOVXMuxMeta *) gst_buffer_add_meta (buffer,
      gst_tiovx_mux_meta_get_info (), NULL);
  tiovx_meta->array = (vx_object_array) exemplar;
  vxRetainReference (exemplar);

  return tiovx_meta;
}
