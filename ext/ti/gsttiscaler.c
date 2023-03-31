/*
 * Copyright (c) [2023] Texas Instruments Incorporated
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttiscaler.h"
#include <edgeai_dl_scaler_armv8_utils.h>

#define MAX_PLANE 4

/* Properties definition */
enum
{
  PROP_0,
};

/* Formats definition */
#define TI_SCALER_SUPPORTED_FORMATS_SRC "{NV12}"
#define TI_SCALER_SUPPORTED_FORMATS_SINK "{NV12}"
#define TI_SCALER_SUPPORTED_WIDTH "[1 , 8192]"
#define TI_SCALER_SUPPORTED_HEIGHT "[1 , 8192]"

/* Src caps */
#define TI_SCALER_STATIC_CAPS_SRC                           \
  "video/x-raw, "                                           \
  "format = (string) " TI_SCALER_SUPPORTED_FORMATS_SRC ", " \
  "width = " TI_SCALER_SUPPORTED_WIDTH ", "                 \
  "height = " TI_SCALER_SUPPORTED_HEIGHT                    \

/* Sink caps */
#define TI_SCALER_STATIC_CAPS_SINK                           \
  "video/x-raw, "                                            \
  "format = (string) " TI_SCALER_SUPPORTED_FORMATS_SINK ", " \
  "width = " TI_SCALER_SUPPORTED_WIDTH ", "                  \
  "height = " TI_SCALER_SUPPORTED_HEIGHT                     \

/* Pads definitions */
static
    GstStaticPadTemplate
    src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_SCALER_STATIC_CAPS_SRC)
    );

static
    GstStaticPadTemplate
    sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TI_SCALER_STATIC_CAPS_SINK)
    );

struct _GstTIScaler
{
  GstVideoFilter
      element;
  GstVideoInfo
    in_info;
  GstVideoInfo
    out_info;
  gboolean
    parse_in_video_meta;
  gboolean
    parse_out_video_meta;
  bufParams2D_t *
      in_y_buf_param;
  bufParams2D_t *
      in_uv_buf_param;
  bufParams2D_t *
      out_y_buf_param;
  bufParams2D_t *
      out_uv_buf_param;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_scaler_debug);
#define GST_CAT_DEFAULT gst_ti_scaler_debug

#define gst_ti_scaler_parent_class parent_class
G_DEFINE_TYPE (GstTIScaler, gst_ti_scaler, GST_TYPE_VIDEO_FILTER);

static void
gst_ti_scaler_finalize (GObject * obj);
static 
    GstCaps *
gst_ti_scaler_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static
    gboolean
gst_ti_scaler_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static
    GstFlowReturn
gst_ti_scaler_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

/* Initialize the plugin's class */
static void
gst_ti_scaler_class_init (GstTIScalerClass * klass)
{
  GObjectClass *
      gobject_class = NULL;
  GstBaseTransformClass *
      gstbasetransform_class = NULL;
  GstElementClass *
      gstelement_class = NULL;
  GstVideoFilterClass *
      gstvideofilter_class = NULL;
  
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;
  gstvideofilter_class = (GstVideoFilterClass *) klass;

  gst_element_class_set_details_simple (gstelement_class,
      "TI Scaler",
      "Filter",
      "Downscale or Upscale video frame",
      "Abhay Chirania <a-chirania@ti.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_ti_scaler_transform_caps);

  gstvideofilter_class->set_info =
      GST_DEBUG_FUNCPTR (gst_ti_scaler_set_info);
  gstvideofilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_ti_scaler_transform_frame);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ti_scaler_finalize);

  GST_DEBUG_CATEGORY_INIT (gst_ti_scaler_debug,
      "tiscaler", 0, "TI Scaler");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_ti_scaler_init (GstTIScaler * self)
{
  GST_LOG_OBJECT (self, "init");
  self->parse_in_video_meta = TRUE;
  self->parse_out_video_meta = TRUE;
  self->in_y_buf_param = (bufParams2D_t *) malloc(sizeof(bufParams2D_t));
  self->out_y_buf_param = (bufParams2D_t *) malloc(sizeof(bufParams2D_t));
  self->in_uv_buf_param = (bufParams2D_t *) malloc(sizeof(bufParams2D_t));
  self->out_uv_buf_param = (bufParams2D_t *) malloc(sizeof(bufParams2D_t));
  return;
}

static void
gst_ti_scaler_finalize (GObject * obj)
{
  GstTIScaler *
      self = GST_TI_SCALER (obj);
  GST_LOG_OBJECT (self, "finalize");

  free(self->in_y_buf_param);
  free(self->out_y_buf_param);
  free(self->in_uv_buf_param);
  free(self->out_uv_buf_param);

  G_OBJECT_CLASS (gst_ti_scaler_parent_class)->finalize (obj);
}

static GstCaps *
gst_ti_scaler_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstTIScaler *self = GST_TI_SCALER (trans);
  GstCaps *result_caps = NULL;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  if (GST_PAD_SINK == direction) {
    result_caps = gst_caps_from_string (TI_SCALER_STATIC_CAPS_SINK);
  } else {
    result_caps = gst_caps_from_string (TI_SCALER_STATIC_CAPS_SRC);
  }

  if (filter) {
    GstCaps *tmp = result_caps;
    result_caps = gst_caps_intersect (result_caps, filter);
    gst_caps_unref (tmp);
  }

  GST_DEBUG_OBJECT (self, "Resulting caps are %" GST_PTR_FORMAT, result_caps);

  return result_caps;
}

static
    gboolean
gst_ti_scaler_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstTIScaler *
        self = GST_TI_SCALER (filter);

  self->in_y_buf_param->dim_x = GST_VIDEO_INFO_WIDTH (in_info);
  self->in_y_buf_param->dim_y = GST_VIDEO_INFO_HEIGHT (in_info);
  self->in_y_buf_param->stride_y = GST_VIDEO_INFO_PLANE_STRIDE (in_info,0);

  self->in_uv_buf_param->dim_x = GST_VIDEO_INFO_WIDTH (in_info);
  self->in_uv_buf_param->dim_y = GST_VIDEO_INFO_HEIGHT (in_info) / 2;
  self->in_uv_buf_param->stride_y = GST_VIDEO_INFO_PLANE_STRIDE (in_info,1);

  self->out_y_buf_param->dim_x = GST_VIDEO_INFO_WIDTH (out_info);
  self->out_y_buf_param->dim_y = GST_VIDEO_INFO_HEIGHT (out_info);
  self->out_y_buf_param->stride_y = GST_VIDEO_INFO_PLANE_STRIDE (out_info,0);

  self->out_uv_buf_param->dim_x = GST_VIDEO_INFO_WIDTH (out_info);
  self->out_uv_buf_param->dim_y = GST_VIDEO_INFO_HEIGHT (out_info) / 2;
  self->out_uv_buf_param->stride_y = GST_VIDEO_INFO_PLANE_STRIDE (out_info,1);

  return TRUE;
}

static
    GstFlowReturn
gst_ti_scaler_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
    GstTIScaler *
      self = GST_TI_SCALER (filter);
    
     // Change stride if meta is available
    if (self->parse_in_video_meta) {
        GstVideoMeta *in_video_meta;
        in_video_meta = gst_buffer_get_video_meta (in_frame->buffer);
        if (in_video_meta) {
            self->in_y_buf_param->stride_y = in_video_meta->stride[0];
            self->in_uv_buf_param->stride_y = in_video_meta->stride[1];
        }
        self->parse_in_video_meta = FALSE;
    }

    if (self->parse_out_video_meta) {
        GstVideoMeta *out_video_meta;
        out_video_meta = gst_buffer_get_video_meta (out_frame->buffer);
        if (out_video_meta) {
            self->out_y_buf_param->stride_y = out_video_meta->stride[0];
            self->out_uv_buf_param->stride_y = out_video_meta->stride[1];
        }
        self->parse_out_video_meta = FALSE;
    }

    GST_LOG_OBJECT (self, "transform_frame");

    scaleNNNV12 (GST_VIDEO_FRAME_PLANE_DATA (in_frame,0),
                self->in_y_buf_param,
                GST_VIDEO_FRAME_PLANE_DATA (in_frame,1),
                self->in_uv_buf_param,
                GST_VIDEO_FRAME_PLANE_DATA (out_frame,0),
                self->out_y_buf_param,
                GST_VIDEO_FRAME_PLANE_DATA (out_frame,1),
                self->out_y_buf_param
                );

    return GST_FLOW_OK;
}