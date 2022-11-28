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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>

#include "gsttiovxdelay.h"
#include "gsttiovxdemux.h"
#include "gsttiovxisp.h"
#include "gsttiovxldc.h"
#include "gsttiovxmemalloc.h"
#include "gsttiovxmosaic.h"
#include "gsttiovxmultiscaler.h"
#include "gsttiovxmux.h"
#include "gsttiovxpyramid.h"
#include "gsttiovxdlcolorconvert.h"
#include "gsttiovxdlcolorblend.h"
#include "gsttiovxdlpreproc.h"

#if defined(DL_PLUGINS)
#include "gsttidlinferer.h"
#include "gsttidlpostproc.h"
#include "gsttiperfoverlay.h"
#endif

#if defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4)
#include "gsttiovxcolorconvert.h"
#include "gsttiovxdof.h"
#include "gsttiovxdofviz.h"
#include "gsttiovxsde.h"
#include "gsttiovxsdeviz.h"
#endif

#include "gst-libs/gst/tiovx/gsttiovxutils.h"

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
ti_ovx_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret = gst_element_register (plugin, "tiovxdemux", GST_RANK_NONE,
      GST_TYPE_TIOVX_DEMUX);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxdemux element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxisp", GST_RANK_NONE,
      GST_TYPE_GST_TIOVX_ISP);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxisp element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxldc", GST_RANK_NONE,
      GST_TYPE_TIOVX_LDC);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxldc element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxmosaic", GST_RANK_NONE,
      GST_TYPE_TIOVX_MOSAIC);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxmosaic element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxmultiscaler", GST_RANK_NONE,
      GST_TYPE_TIOVX_MULTI_SCALER);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxmultiscaler element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxmux", GST_RANK_NONE,
      GST_TYPE_TIOVX_MUX);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxmux element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxpyramid", GST_RANK_NONE,
      GST_TYPE_TIOVX_PYRAMID);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxpyramid element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxdelay", GST_RANK_NONE,
      GST_TYPE_TIOVX_DELAY);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxdelay element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxmemalloc", GST_RANK_NONE,
      GST_TYPE_TIOVX_MEM_ALLOC);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxmemalloc element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxdlcolorconvert", GST_RANK_NONE,
      GST_TYPE_TIOVX_DL_COLOR_CONVERT);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxdlcolorconvert element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxdlcolorblend", GST_RANK_NONE,
      GST_TYPE_TIOVX_DL_COLOR_BLEND);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxdlcolorblend element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxdlpreproc", GST_RANK_NONE,
      GST_TYPE_TIOVX_DL_PRE_PROC);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxdlpreproc element");
    goto out;
  }
#if defined(DL_PLUGINS)
  ret = gst_element_register (plugin, "tidlinferer", GST_RANK_NONE,
      GST_TYPE_TI_DL_INFERER);
  if (!ret) {
    GST_ERROR ("Failed to register the tidlinferer element");
    goto out;
  }

  ret = gst_element_register (plugin, "tidlpostproc", GST_RANK_NONE,
      GST_TYPE_TI_DL_POST_PROC);
  if (!ret) {
    GST_ERROR ("Failed to register the tidlpostproc element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiperfoverlay", GST_RANK_NONE,
      GST_TYPE_TI_PERF_OVERLAY);
  if (!ret) {
    GST_ERROR ("Failed to register the tiperfoverlay element");
    goto out;
  }
#endif

#if defined(SOC_J721E) || defined(SOC_J721S2) || defined(SOC_J784S4)
  ret = gst_element_register (plugin, "tiovxcolorconvert", GST_RANK_NONE,
      GST_TYPE_TIOVX_COLOR_CONVERT);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxcolorconvert element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxdof", GST_RANK_NONE,
      GST_TYPE_TIOVX_DOF);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxdof element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxdofviz", GST_RANK_NONE,
      GST_TYPE_TIOVX_DOF_VIZ);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxdofviz element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxsde", GST_RANK_NONE,
      GST_TYPE_TIOVX_SDE);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxsde element");
    goto out;
  }

  ret = gst_element_register (plugin, "tiovxsdeviz", GST_RANK_NONE,
      GST_TYPE_TIOVX_SDE_VIZ);
  if (!ret) {
    GST_ERROR ("Failed to register the tiovxsdeviz element");
    goto out;
  }
#endif

  gst_tiovx_init_debug ();

  ret = TRUE;

out:
  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, tiovx,
    "GStreamer plugin for TIOVX", ti_ovx_init, PACKAGE_VERSION, "Proprietary",
    GST_PACKAGE_NAME, "http://ti.com")
