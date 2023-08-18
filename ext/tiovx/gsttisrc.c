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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsttisrc.h"

/* Pads definitions */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-bayer"));

struct _GstTISrc
{
  GstBaseSrc source;
};

GST_DEBUG_CATEGORY_STATIC (gst_ti_src_debug);
#define GST_CAT_DEFAULT gst_ti_src_debug

#define gst_ti_src_parent_class parent_class
G_DEFINE_TYPE (GstTISrc, gst_ti_src,
    GST_TYPE_BASE_SRC);

static GstFlowReturn gst_ti_src_fill (GstBaseSrc *src, guint64 offset,
        guint size, GstBuffer *buf);
static gboolean gst_ti_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps);

/* Initialize the plugin's class */
static void
gst_ti_src_class_init (GstTISrcClass * klass)
{
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstPadTemplate *pad_template = NULL;

  gst_element_class_set_details_simple (gstelement_class,
      "TI Src",
      "Filter/Converter/Video",
      "Src frames",
      "Rahul T R <r-ravikumar@ti.com>");

  pad_template =
      gst_pad_template_new_from_static_pad_template_with_gtype (&src_template,
      GST_TYPE_PAD);
  gst_element_class_add_pad_template (gstelement_class, pad_template);
  gstbasesrc_class->fill = GST_DEBUG_FUNCPTR (gst_ti_src_fill);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_ti_src_setcaps);

  GST_DEBUG_CATEGORY_INIT (gst_ti_src_debug,
      "tisrc", 0, "debug category for the tisrc element");
}

/* Initialize the new element */
static void
gst_ti_src_init (GstTISrc * self)
{
  GST_LOG_OBJECT (self, "init");
}

GstFlowReturn gst_ti_src_fill (GstBaseSrc *src, guint64 offset,
        guint size, GstBuffer *buf)
{
    return GST_FLOW_OK;
}

static gboolean gst_ti_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
    return TRUE;
}
