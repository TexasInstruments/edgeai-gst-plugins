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
#  include "config.h"
#endif
#include "gsttiovxsiso.h"

#include <app_init.h>
#include <TI/tivx.h>
#include <TI/j7.h>

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_siso_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_siso_debug_category

typedef struct _GstTIOVXSisoPrivate
{
  gboolean dummy_member;
  vx_context context;
  vx_graph graph;
} GstTIOVXSisoPrivate;

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstTIOVXSiso, gst_tiovx_siso,
    GST_TYPE_BASE_TRANSFORM, G_ADD_PRIVATE (GstTIOVXSiso)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_siso_debug_category, "tiovxsiso", 0,
        "debug category for tiovxsiso base class"));

static gboolean gst_tiovx_siso_start (GstBaseTransform * trans);

static void
gst_tiovx_siso_class_init (GstTIOVXSisoClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_tiovx_siso_start);
}

static void
gst_tiovx_siso_init (GstTIOVXSiso * self)
{
}

static gboolean
gst_tiovx_siso_start (GstBaseTransform * trans)
{
  int init_status;
  vx_status status;

  GstTIOVXSiso *self = GST_TIOVX_SISO (trans);
  GstTIOVXSisoPrivate *priv = gst_tiovx_siso_get_instance_private (self);

  GST_DEBUG_OBJECT (self, "start");

  init_status = appCommonInit ();
  if (init_status != 0) {
    g_print ("Common init failed\n");
    return FALSE;
  }
  tivxInit ();
  tivxHostInit ();

  priv->context = vxCreateContext ();
  status = vxGetStatus ((vx_reference) priv->context);
  if (status != VX_SUCCESS) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create OpenVX context."), (NULL));
    return FALSE;
  }

  tivxHwaLoadKernels (priv->context);

  priv->graph = vxCreateGraph (priv->context);
  status = vxGetStatus ((vx_reference) priv->graph);
  if (status != VX_SUCCESS) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create OpenVX graph."), (NULL));
    return FALSE;
  }

  return TRUE;
}
