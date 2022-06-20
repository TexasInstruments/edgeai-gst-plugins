/*
 * Copyright (c) [2022] Texas Instruments Incorporated
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
#  include "config.h"
#endif

#include "gsttiovxmemalloc.h"

#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferpoolutils.h"
#include "gst-libs/gst/tiovx/gsttiovxcontext.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"


static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_mem_alloc_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_mem_alloc_debug_category

typedef struct _GstTIOVXMemAlloc
{
  GstBaseTransform parent;
  GstCaps *in_caps;
  vx_context context;
  GstTIOVXContext *tiovx_context;

} GstTIOVXMemAlloc;

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXMemAlloc, gst_tiovx_mem_alloc,
    GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_mem_alloc_debug_category,
        "tiovxmemalloc", 0, "debug category for tiovxmemalloc base class"));

static gboolean gst_tiovx_mem_alloc_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean
gst_tiovx_mem_alloc_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static void gst_tiovx_mem_alloc_finalize (GObject * obj);

static void
gst_tiovx_mem_alloc_class_init (GstTIOVXMemAllocClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "TIOVX Mem Alloc",
      "Generic",
      "Exports a tiovxbufferpool to be used by elements upstream.",
      "RidgeRun support@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  base_transform_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_tiovx_mem_alloc_propose_allocation);
  base_transform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_mem_alloc_set_caps);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_tiovx_mem_alloc_finalize);

}

static void
gst_tiovx_mem_alloc_init (GstTIOVXMemAlloc * self)
{
  self->in_caps = NULL;
  self->context = NULL;
  self->tiovx_context = NULL;
}

static gboolean
gst_tiovx_mem_alloc_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps)
{
  GstTIOVXMemAlloc *self = GST_TIOVX_MEM_ALLOC (trans);
  gboolean ret = TRUE;

  GST_LOG_OBJECT (self, "Set caps");

  self->in_caps = gst_caps_copy (incaps);

  return ret;
}

static gboolean
gst_tiovx_mem_alloc_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstTIOVXMemAlloc *self = GST_TIOVX_MEM_ALLOC (trans);
  vx_reference exemplar = NULL;
  vx_status status = VX_SUCCESS;
  GstBufferPool *pool = NULL;
  gsize size = 0;
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "Propose allocation");

  /* Create context */
  self->tiovx_context = gst_tiovx_context_new ();
  if (NULL == self->tiovx_context) {
    GST_ERROR_OBJECT (self, "Failed to do common initialization");
    goto exit;
  }
  self->context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) self->context);
  if (VX_SUCCESS != status) {
    GST_ERROR_OBJECT (self,
        "Context creation failed, vx_status %" G_GINT32_FORMAT, status);
    goto release_tiovx_context;
  }

  exemplar =
      gst_tiovx_get_exemplar_from_caps ((GObject *) self, GST_CAT_DEFAULT,
      self->context, self->in_caps);

  size = gst_tiovx_get_size_from_exemplar (exemplar);
  if (0 >= size) {
    GST_ERROR_OBJECT (self, "Failed to get size from input");
    goto release_context;
  }

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, 2,
      exemplar, size, 1, &pool);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to add new pool in propose allocation");
    goto release_context;
  }

  vxReleaseReference (&exemplar);
  gst_object_unref (pool);
  return ret;

release_context:
  vxReleaseContext (&self->context);

release_tiovx_context:
  g_object_unref (self->tiovx_context);

exit:
  return ret;
}

static void
gst_tiovx_mem_alloc_finalize (GObject * obj)
{
  GstTIOVXMemAlloc *self = GST_TIOVX_MEM_ALLOC (obj);

  GST_LOG_OBJECT (self, "finalize");

  if (NULL != self->in_caps) {
    gst_caps_unref (self->in_caps);
  }

  /* Release context */
  if (VX_SUCCESS == vxGetStatus ((vx_reference) self->context)) {
    vxReleaseContext (&self->context);
  }

  /* App common deinit */
  GST_DEBUG_OBJECT (self, "Running TIOVX common deinit");
  if (self->tiovx_context) {
    g_object_unref (self->tiovx_context);
  }

  G_OBJECT_CLASS (gst_tiovx_mem_alloc_parent_class)->finalize (obj);
}
