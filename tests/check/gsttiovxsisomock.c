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

#include "gsttiovxsisomock.h"

struct _GstTIOVXSisoMock
{
  GstTIOVXSiso parent;

  GstPad *sinkpad;
  GstPad *srcpad;

  GstBufferPool *bufferPool;
};

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_siso_mock_debug_category);

G_DEFINE_TYPE_WITH_CODE (GstTIOVXSisoMock, gst_tiovx_siso_mock,
    GST_TIOVX_SISO_TYPE,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_siso_mock_debug_category,
        "tiovxsisomock", 0, "debug category for tiovxsisomock element"));

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static gboolean gst_tiovx_siso_mock_create_node (GstTIOVXSiso * trans,
    vx_graph graph, vx_node node, vx_reference input, vx_reference output);

static gboolean gst_tiovx_siso_mock_get_exemplar_refs (GstTIOVXSiso *
    trans, GstVideoInfo * in_caps_info, GstVideoInfo * out_caps_info,
    vx_reference input, vx_reference output);

static void
gst_tiovx_siso_mock_class_init (GstTIOVXSisoMockClass * klass)
{
  GstTIOVXSisoClass *tiovx_siso_class = GST_TIOVX_SISO_CLASS (klass);
  GstBaseTransformClass *bt_class = GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_pad_template ((GstElementClass *) klass,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_add_pad_template ((GstElementClass *) klass,
      gst_static_pad_template_get (&src_factory));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "TIOVX SISO MOCK", "Filter/Video",
      "TIOVX SISO MOCK element for testing purposes.",
      "Jafet Chaves <jafet.chaves@ridgerun.com>");

  tiovx_siso_class->create_node =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_mock_create_node);
  tiovx_siso_class->get_exemplar_refs =
      GST_DEBUG_FUNCPTR (gst_tiovx_siso_mock_get_exemplar_refs);

  bt_class->passthrough_on_same_caps = TRUE;
}

static void
gst_tiovx_siso_mock_init (GstTIOVXSisoMock * self)
{

}

static gboolean
gst_tiovx_siso_mock_create_node (GstTIOVXSiso * trans, vx_graph graph,
    vx_node node, vx_reference input, vx_reference output)
{
  return TRUE;
}

static gboolean
gst_tiovx_siso_mock_get_exemplar_refs (GstTIOVXSiso * trans,
    GstVideoInfo * in_caps_info, GstVideoInfo * out_caps_info,
    vx_reference input, vx_reference output)
{
  return TRUE;
}
