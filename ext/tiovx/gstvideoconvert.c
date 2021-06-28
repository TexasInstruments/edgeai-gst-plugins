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

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>

#include "gstvideoconvert.h"

GST_DEBUG_CATEGORY_STATIC (gst_videoconvert_debug);
#define GST_CAT_DEFAULT gst_videoconvert_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_videoconvert_parent_class parent_class
G_DEFINE_TYPE (GstVideoConvert, gst_videoconvert, GST_TYPE_BASE_TRANSFORM);

static gboolean create_node(GstTIOVXSiso *trans, vx_context context, vx_graph graph, vx_node node, vx_reference input,
                                           vx_reference output);
static gboolean      get_exemplar_refs        (GstTIOVXSiso *trans, GstVideoInfo *in_caps_info, GstVideoInfo *out_caps_info,
                                           vx_context context, vx_reference input, vx_reference output);
static GstCaps *
gst_videoconvert_transform_caps (GstBaseTransform * base, GstPadDirection direction,
                                 GstCaps *caps, GstCaps *filter);

/* Initialize the plugin's class */
static void
gst_videoconvert_template_class_init (GstVideoConvertClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstTIOVXSisoClass *gst_tiovx_siso_class = NULL;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  gst_element_class_set_details_simple (gstelement_class,
      "Video Convert",
      "Generic/Filter",
      "Converts video from one colorspace to another using the OVX API", "Daniel Chaves daniel.chaves@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gst_tiovx_siso_class->create_node = GST_DEBUG_FUNCPTR (gst_videoconvert_create_node);
  gst_tiovx_siso_class->get_exemplar_refs = GST_DEBUG_FUNCPTR (gst_videoconvert_get_exemplar_refs);

  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_videoconvert_transform_caps);

  GST_DEBUG_CATEGORY_INIT (gst_videoconvert_debug, "gstvideoconvert", 0,
      "debug category for the gstvideoconvert element");
}

/* Initialize the new element
 * Initialize instance structure
 */
static void
gst_videoconvert_init (GstVideoConvert * filter)
{
  filter->silent = FALSE;
}

static gboolean      gst_videoconvert_create_node    (GstTIOVXSiso *trans, vx_context context, vx_graph graph, vx_node node, vx_reference input,
                                                vx_reference output) {
    GstTIOVXSiso *self = NULL;
    GstTIOVXSisoPrivate *priv = NULL;
    vx_status status;

    self = GST_TIOVX_SISO(trans);
    priv = gst_tiovx_siso_get_instance_private (self);

    status = vxuColorConvert (priv->context, in_frame, out_frame);
    if (VX_SUCCESS != status) {
        GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
                           ("Error, status = %d. ", status),
                           ("Unable to perform format conversion."));
    }

    return status;
}


static gboolean      gst_videoconvert_get_exemplar_refs        (GstTIOVXSiso *trans, GstVideoInfo *in_caps_info, GstVideoInfo *out_caps_info,
                                               vx_context context, vx_reference input, vx_reference output) {}


static GstCaps *
gst_videoconvert_transform_caps (GstBaseTransform * base, GstPadDirection direction,
                                 GstCaps *caps, GstCaps *filter) {
   GstCaps * clone_caps = NULL;

   clone_caps = gst_caps_copy(caps);

   for (i = 0; i < gst_caps_get_size (clone_caps); i++) {
       GstStruct *st = gst_caps_get_structure (clone_caps, i);

       gst_structure_remove_field (st, "format");
   }

   clone_caps = GST_BASE_TRANSFORM_CLASS (gst_videoconvert_parent_class)->transform_caps (base, direction, clone_caps, filter);

   return clone_caps;
}
