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
#include <stdlib.h>

#include "gsttiovxcontext.h"

#include <utils/app_init/include/app_init.h>

GST_DEBUG_CATEGORY_STATIC (gst_tiovx_context_debug_category);
#define GST_CAT_DEFAULT gst_tiovx_context_debug_category

static GObject *singleton = NULL;
static GMutex mutex = { 0 };

struct _GstTIOVXContext
{
  GObject base;
};

G_DEFINE_TYPE_WITH_CODE (GstTIOVXContext, gst_tiovx_context,
    G_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (gst_tiovx_context_debug_category,
        "tiovxcontext", 0, "debug category for TIOVX context class"));

static GObject *gst_tiovx_context_constructor (GType type,
    guint n_construct_properties, GObjectConstructParam * construct_properties);
static void gst_tiovx_context_finalize (GObject * object);

GstTIOVXContext *
gst_tiovx_context_new (void)
{
  return GST_TIOVX_CONTEXT (g_object_new (GST_TYPE_TIOVX_CONTEXT, NULL));
}

static void
gst_tiovx_context_class_init (GstTIOVXContextClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = gst_tiovx_context_constructor;
  gobject_class->finalize = gst_tiovx_context_finalize;
}

static GObject *
gst_tiovx_context_constructor (GType type, guint n_construct_properties,
    GObjectConstructParam * construct_properties)
{
  /* We cannot use the g_once trick here because it doesn't play
     nicely with objects being destroyed entirely and created again,
     which is a valid case of ours. A normal mutex needs to be used.
   */
  g_mutex_lock (&mutex);

  /* Just create a new instance the very first time, the remainder give a ref */
  if (NULL == singleton) {
    singleton =
        G_OBJECT_CLASS (gst_tiovx_context_parent_class)->constructor (type,
        n_construct_properties, construct_properties);
  } else {
    singleton = g_object_ref (singleton);
  }

  g_mutex_unlock (&mutex);

  return singleton;
}

static void
gst_tiovx_context_init (GstTIOVXContext * self)
{
  gint ret = 0;
  gint init_flag = 1;
  const gchar *strVal = NULL;

  strVal = g_getenv ("SKIP_TIOVX_INIT");

  if (strVal != NULL) {
    GST_INFO ("Skipping appInit() from GST!");
    init_flag = 0;
  } else {
    GST_INFO ("Calling appInit() from GST!");
  }

  if (init_flag == 1) {
    ret = appInit ();
    g_assert (0 == ret);
  }
}

static void
gst_tiovx_context_finalize (GObject * object)
{
  gint ret = 0;
  gint init_flag = 1;
  const gchar *strVal = NULL;

  strVal = g_getenv ("SKIP_TIOVX_INIT");

  if (strVal != NULL) {
    GST_INFO ("Skipping appDeInit() from GST!");
    init_flag = 0;
  } else {
    GST_INFO ("Calling appDeInit() from GST!");
  }

  if (init_flag == 1) {
    g_mutex_lock (&mutex);

    ret = appDeInit ();
    g_assert (0 == ret);

    singleton = NULL;
    g_mutex_unlock (&mutex);
  }

  G_OBJECT_CLASS (gst_tiovx_context_parent_class)->finalize (object);
}
