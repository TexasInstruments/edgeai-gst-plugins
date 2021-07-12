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

#include "gsttiovxsimo.h"

#include <app_init.h>
#include <TI/j7.h>

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_simo_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_simo_debug_category

typedef struct _GstTIOVXSimoPrivate
{
  vx_context context;
  vx_graph graph;
  vx_node *node;
} GstTIOVXSimoPrivate;

G_DEFINE_TYPE_WITH_CODE (GstTIOVXSimo, gst_tiovx_simo,
    GST_TYPE_ELEMENT, G_ADD_PRIVATE (GstTIOVXSimo)
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_simo_debug_category, "tiovxsimo", 0,
        "debug category for tiovxsimo base class"));

static GstCaps *gst_tiovx_simo_default_transform_caps (GstTIOVXSimo * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

static GstStateChangeReturn
gst_tiovx_simo_change_state (GstElement * trans, GstStateChange transition);

static void
gst_tiovx_simo_class_init (GstTIOVXSimoClass * klass)
{
  GstElementClass *gstelement_class;

  gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG ("gst_tiovx_simo_class_init");

  klass->transform_caps =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_default_transform_caps);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_tiovx_simo_change_state);
}

static void
gst_tiovx_simo_init (GstTIOVXSimo * self)
{
  GST_DEBUG ("gst_tiovx_simo_init");
}

static GstCaps *
gst_tiovx_simo_default_transform_caps (GstTIOVXSimo * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *ret;

  ret = NULL;

  if (NULL == caps)
    return NULL;

  return ret;
}

static GstStateChangeReturn
gst_tiovx_simo_change_state (GstElement * trans, GstStateChange transition)
{
  return GST_STATE_CHANGE_SUCCESS;
}
