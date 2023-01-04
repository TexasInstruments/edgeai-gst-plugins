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

#ifndef _GST_TIOVX_SIMO_H_
#define _GST_TIOVX_SIMO_H_

#include <gst/base/base-prelude.h>
#include <gst/gst.h>
#include <TI/tivx.h>

#include "gsttiovxpad.h"

G_BEGIN_DECLS

#define GST_TYPE_TIOVX_SIMO   (gst_tiovx_simo_get_type())
G_DECLARE_DERIVABLE_TYPE (GstTIOVXSimo, gst_tiovx_simo, GST,
	TIOVX_SIMO, GstElement)

/**
 * GstTIOVXSimoClass:
 * @parent_class:   Element parent class
 * @init_module:        Required. Subclasses must override to init
 *                      the element-specific module.
 * @create_graph:       Required. Subclasses must override to init
 *                      the element-specific graph.
 * @get_node_info:      Required. Subclasses must override to return
                        node information
 *                      on the element-specific node parameters.
 * @configure_module:   Optional. Subclasses may override to release
 *                      vx_image memory allocated and do module configuration
 *                      prior to starting the process graph if needed.
 * @deinit_module:      Required. Subclasses must override to deinit
 *                      the element-specific module.
 *
 * @get_src_caps:       Optional. Subclasses may override to manage custom
 *                      implementation of src caps queries. Given the list of
 *                      sink caps what should be the src caps? Default
 *                      implementation is to fully intersect filter and sink
 *                      caps.
 * @get_sink_caps:      Optional. Subclasses may override to manage custom
 *                      implementation of sink caps queries. Given the list of
 *                      source caps what should be the sink caps? Default
 *                      implementation is to fully intersect filter and source
 *                      caps list.
 * @fixate_caps:        Optional. Subclasses may override to manage custom
 *                      implementation of caps events. Default
 *                      implementation is to use gst_caps_fixate() to obtain
 *                      caps that will be used in the src pads.
 * @compare_caps:       Optional. Used in renegotiation cases. Subclasses can
 *                      use this interface to implement a custom function that
 *                      compares former sink caps and current sink caps.
 * @preprocess:         Optional. Subclasses may implement this function to
 *                      perform operations before processing
 * @postprocess:        Optional. Subclasses may implement this function to
 *                      perform operations after processing.
 *
 * Subclasses can override any of the available virtual methods.
 */

struct _GstTIOVXSimoClass
{
  GstElementClass parent_class;

  gboolean      (*init_module)              (GstTIOVXSimo *self, vx_context context, GstTIOVXPad *sink_pad,
                                             GList *src_pads, GstCaps * sink_caps, GList * src_caps_list, guint num_channels);

  gboolean      (*create_graph)             (GstTIOVXSimo *self, vx_context context, vx_graph graph);

  gboolean      (*get_node_info)            (GstTIOVXSimo *self, vx_node *node, GstTIOVXPad *sink_pad, GList *src_pads, GList **queueable_objects);

  gboolean      (*configure_module)         (GstTIOVXSimo *self);

  gboolean      (*deinit_module)            (GstTIOVXSimo *self);

  GstCaps *     (*get_sink_caps)            (GstTIOVXSimo *self, GstCaps *filter, GList *src_caps_list, GList *src_pads);

  GstCaps *     (*get_src_caps)             (GstTIOVXSimo *self, GstCaps *filter, GstCaps *sink_caps, GstTIOVXPad *src_pad);

  GList *       (*fixate_caps)              (GstTIOVXSimo *self, GstCaps *sink_caps, GList * src_caps_list);

  gboolean      (*compare_caps)             (GstTIOVXSimo *self, GstCaps *caps1, GstCaps *caps2, GstPadDirection direction);

  gboolean (*preprocess)               (GstTIOVXSimo *self);

  gboolean (*postprocess)              (GstTIOVXSimo *self);
};

/**
 * @gst_tiovx_simo_get_num_pads:
 * @self: the #GstTIOVXSimo to query
 *
 * Get number of source pads.
 *
 * Returns: Returns the internal counter of requested source pads
 */
guint	gst_tiovx_simo_get_num_pads      (GstTIOVXSimo *self);

G_END_DECLS

#endif /* _GST_TIOVX_SIMO_H_ */
