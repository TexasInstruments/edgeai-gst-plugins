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
 * *	No reverse engineering, decompilation, or disassembly of this software 
 *      is permitted with respect to any software provided in binary form.
 * 
 * *	Any redistribution and use are licensed by TI for use only with TI Devices.
 * 
 * *	Nothing shall obligate TI to provide you with source code for the
 *      software licensed and provided to you in object code.
 * 
 * If software source code is provided to you, modification and redistribution
 * of the source code are permitted provided that the following conditions are met:
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

#include <glib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <TI/tivx.h>
#include "gst-libs/gst/tiovx/gsttiovx.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferpool.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferutils.h"
#include "gst-libs/gst/tiovx/gsttiovxbufferpoolutils.h"
#include "gst-libs/gst/tiovx/gsttiovxtensorbufferpool.h"
#include "gst-libs/gst/tiovx/gsttiovxutils.h"
#include "gst-libs/gst/tiovx/gsttiovxcontext.h"

#include "gsttiisppreproc.h"

#define MIN_POOL_SIZE 2
#define MAX_POOL_SIZE 16
#define DEFAULT_POOL_SIZE MIN_POOL_SIZE

#define MIN_SHIFT 0
#define MAX_SHIFT 8
#define DEFAULT_SHIFT MIN_SHIFT

/* Properties definition */
enum
{
  PROP_0,
  PROP_SHIFT,
  PROP_IN_POOL_SIZE,
  PROP_OUT_POOL_SIZE,
};

/* Formats definition */
#define TIOVX_ISP_SUPPORTED_FORMATS "{ bggr, gbrg, grbg, rggb, bggr10, gbrg10, grbg10, rggb10, rggi10, grig10, bggi10, gbig10, girg10, iggr10, gibg10, iggb10, bggr12, gbrg12, grbg12, rggb12, bggr16, gbrg16, grbg16, rggb16 }"
#define TIOVX_ISP_SUPPORTED_WIDTH "[1 , 8192]"
#define TIOVX_ISP_SUPPORTED_HEIGHT "[1 , 8192]"
#define TIOVX_ISP_SUPPORTED_CHANNELS "[2 , 16]"

/* caps */
#define TI_ISP_PREPROC_STATIC_CAPS                               \
  "video/x-bayer, "                                           \
  "format = (string) " TIOVX_ISP_SUPPORTED_FORMATS ", " \
  "width = " TIOVX_ISP_SUPPORTED_WIDTH ", "                 \
  "height = " TIOVX_ISP_SUPPORTED_HEIGHT

/* Pads definitions */
static
    GstStaticPadTemplate
    sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_ISP_PREPROC_STATIC_CAPS)
    );

static
    GstStaticPadTemplate
    src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_ISP_PREPROC_STATIC_CAPS)
    );

struct _GstTIISPreproc
{
  GstBaseTransform element;
  guint in_pool_size;
  guint out_pool_size;
  guint shift;
  vx_reference ref;
  vx_context context;
  GstTIOVXContext *tiovx_context;
  GstBufferPool *sink_buffer_pool;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_isp_preproc_debug);
#define GST_CAT_DEFAULT gst_ti_isp_preproc_debug

#define gst_ti_isp_preproc_parent_class parent_class
G_DEFINE_TYPE (GstTIISPreproc, gst_ti_isp_preproc, GST_TYPE_BASE_TRANSFORM);

static void gst_ti_isp_preproc_finalize (GObject * obj);
static void gst_ti_isp_preproc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ti_isp_preproc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_ti_isp_preproc_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_ti_isp_preproc_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_ti_isp_preproc_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static gboolean gst_ti_isp_preproc_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);

/* Initialize the plugin's class */
static void
gst_ti_isp_preproc_class_init (GstTIISPreprocClass * klass)
{
  GObjectClass *gobject_class = NULL;
  GstBaseTransformClass *gstbasetransform_class = NULL;
  GstElementClass *gstelement_class = NULL;

  gobject_class = (GObjectClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gst_element_class_set_details_simple (gstelement_class,
      "TI DL ISP Preproc",
      "Filter",
      "Right shift the bits by specified amount via property",
      "Rahul T R <r-ravikumar@ti.com>. Shreyash Sinha <s-sinha@ti.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_ti_isp_preproc_set_property;
  gobject_class->get_property = gst_ti_isp_preproc_get_property;

  g_object_class_install_property (gobject_class, PROP_IN_POOL_SIZE,
      g_param_spec_uint ("in-pool-size", "Input Pool Size",
          "Number of buffers to allocate in input pool", MIN_POOL_SIZE,
          MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_OUT_POOL_SIZE,
      g_param_spec_uint ("out-pool-size", "Output Pool Size",
          "Number of buffers to allocate in output pool", MIN_POOL_SIZE,
          MAX_POOL_SIZE, DEFAULT_POOL_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SHIFT,
      g_param_spec_uint ("shift", "Shift",
          "Number of bits to shift", MIN_SHIFT, MAX_SHIFT, DEFAULT_SHIFT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gstbasetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_ti_isp_preproc_set_caps);
  gstbasetransform_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_ti_isp_preproc_decide_allocation);
  gstbasetransform_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_ti_isp_preproc_propose_allocation);
  gstbasetransform_class->transform =
      GST_DEBUG_FUNCPTR (gst_ti_isp_preproc_transform);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_isp_preproc_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_isp_preproc_debug,
      "tiisppreproc", 0, "TI ISP Preproc");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_ti_isp_preproc_init (GstTIISPreproc * self)
{
  GST_LOG_OBJECT (self, "init");

  self->in_pool_size = DEFAULT_POOL_SIZE;
  self->out_pool_size = DEFAULT_POOL_SIZE;
  self->shift = DEFAULT_SHIFT;
  self->ref = NULL;
  self->context = NULL;
  self->tiovx_context = gst_tiovx_context_new ();

  return;
}

static void
gst_ti_isp_preproc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTIISPreproc *
      self = GST_TI_ISP_PREPROC (object);

  GST_LOG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_IN_POOL_SIZE:
      self->in_pool_size = g_value_get_uint (value);
      break;
    case PROP_OUT_POOL_SIZE:
      self->out_pool_size = g_value_get_uint (value);
      break;
    case PROP_SHIFT:
      self->shift = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_ti_isp_preproc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTIISPreproc *
      self = GST_TI_ISP_PREPROC (object);

  GST_LOG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case PROP_IN_POOL_SIZE:
      g_value_set_uint (value, self->in_pool_size);
      break;
    case PROP_OUT_POOL_SIZE:
      g_value_set_uint (value, self->out_pool_size);
      break;
    case PROP_SHIFT:
      g_value_set_uint (value, self->shift);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_ti_isp_preproc_finalize (GObject * obj)
{
  GstTIISPreproc *
      self = GST_TI_ISP_PREPROC (obj);

  GST_LOG_OBJECT (self, "finalize");

  if (NULL != self->sink_buffer_pool) {
    gst_object_unref (self->sink_buffer_pool);
  }

  if (NULL != self->ref) {
    vxReleaseReference (&(self->ref));
  }

  vxReleaseContext (&self->context);

  if (self->tiovx_context)
  {
    g_object_unref (self->tiovx_context);
  }

  G_OBJECT_CLASS (gst_ti_isp_preproc_parent_class)->finalize (obj);
}

static
    GstFlowReturn
gst_ti_isp_preproc_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstTIISPreproc *
      self = GST_TI_ISP_PREPROC (trans);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstMapInfo input_mapinfo, output_mapinfo;
  GstClockTime pts = 0, dts = 0, duration = 0;

  GST_LOG_OBJECT (self, "transform");

  pts = GST_BUFFER_PTS (inbuf);
  dts = GST_BUFFER_DTS (inbuf);
  duration = GST_BUFFER_DURATION (inbuf);

  if (!gst_buffer_map (inbuf, &input_mapinfo, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "failed to map input buffer");
    goto exit;
  }

  if (!gst_buffer_map (outbuf, &output_mapinfo, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (self, "failed to map input buffer");
    goto exit;
  }

  gst_buffer_unmap (inbuf, &input_mapinfo);
  gst_buffer_unmap (outbuf, &output_mapinfo);

  GST_BUFFER_PTS (outbuf) = pts;
  GST_BUFFER_DTS (outbuf) = dts;
  GST_BUFFER_DURATION (outbuf) = duration;

  ret = GST_FLOW_OK;

exit:
  return ret;
}

static
    gboolean
gst_ti_isp_preproc_set_caps (GstBaseTransform * trans, GstCaps * incaps,
        GstCaps * outcaps)
{
    GstTIISPreproc *self = GST_TI_ISP_PREPROC (trans);

    self->context = vxCreateContext ();
    self->ref = gst_tiovx_get_exemplar_from_caps ((GObject *) self,
            GST_CAT_DEFAULT, self->context, incaps);
    return true;
}

static
    gboolean
gst_ti_isp_preproc_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstTIISPreproc *
      self = GST_TI_ISP_PREPROC (trans);
  GstBufferPool *
      pool = NULL;
  gboolean ret = TRUE;
  guint npool = 0;
  gboolean pool_needed = TRUE;

  GST_LOG_OBJECT (self, "Decide allocation");

  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); ++npool) {
    GstBufferPool *
        pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);

    if (NULL == pool) {
      GST_DEBUG_OBJECT (self, "No pool in query position: %d, ignoring", npool);
      gst_query_remove_nth_allocation_pool (query, npool);
      continue;
    }

    /* Use TIOVX pool if found */
    if (GST_TIOVX_IS_BUFFER_POOL (pool)) {
      GST_INFO_OBJECT (self, "TIOVX pool found, using this one: \"%s\"",
          GST_OBJECT_NAME (pool));

      pool_needed = FALSE;
    } else {
      GST_INFO_OBJECT (self, "No TIOVX pool, discarding: \"%s\"",
          GST_OBJECT_NAME (pool));

      gst_query_remove_nth_allocation_pool (query, npool);
    }

    gst_object_unref (pool);
  }

  if (pool_needed) {
    /* Create our own pool if a TIOVX was not found.
       We use output vx_reference to decide a pool to use downstream. */
    gsize size = 0;

    size = gst_tiovx_get_size_from_exemplar ((vx_reference) self->ref);
    if (0 >= size) {
      GST_ERROR_OBJECT (self, "Failed to get size from exemplar");
      ret = FALSE;
      goto exit;
    }

    ret =
        gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, self->out_pool_size,
        (vx_reference) self->ref, size, 1, &pool);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to add new pool in decide allocation");
      return ret;
    }

    ret = gst_buffer_pool_set_active (GST_BUFFER_POOL (pool), TRUE);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to activate bufferpool");
      goto exit;
    }
    gst_object_unref (pool);
    pool = NULL;
  }

exit:
  return ret;
}

static
    gboolean
gst_ti_isp_preproc_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstTIISPreproc *
      self = GST_TI_ISP_PREPROC (trans);
  GstBufferPool *
      pool = NULL;
  gsize size = 0;
  gboolean ret = TRUE;

  GST_LOG_OBJECT (self, "Propose allocation");

  /* We use input vx_reference to propose a pool upstream */
  size = gst_tiovx_get_size_from_exemplar ((vx_reference) self->ref);
  if (0 >= size) {
    GST_ERROR_OBJECT (self, "Failed to get size from input");
    ret = FALSE;
    goto exit;
  }

  ret =
      gst_tiovx_add_new_pool (GST_CAT_DEFAULT, query, self->in_pool_size,
      (vx_reference) self->ref, size, 1, &pool);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to add new pool in propose allocation");
    return ret;
  }
  /* Unref the old stored pool */
  if (NULL != self->sink_buffer_pool) {
    gst_object_unref (self->sink_buffer_pool);
  }
  /* Assign the new pool to the internal value */
  self->sink_buffer_pool = GST_BUFFER_POOL (pool);

exit:
  return ret;
}
