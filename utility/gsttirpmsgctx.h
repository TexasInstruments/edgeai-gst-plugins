/*
 * Copyright (c) [2026] Texas Instruments Incorporated
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
#ifndef __GST_TI_RPMSG_CTX_H__
#define __GST_TI_RPMSG_CTX_H__

#include <glib.h>

G_BEGIN_DECLS typedef struct _GstTiRpmsgChan GstTiRpmsgChan;

struct _GstTiRpmsgChan
{
  guint rproc_id;               /* remote processor core ID */
  guint remote_ep;              /* RPMsg endpoint on DSP */
  int fd;                       /* RPMsg character device file descriptor */
  gint refcount;                /* number of elements holding this channel */
  GMutex mutex;                 /* serializes one send/recv pair at a time */
};

/*
 * Returns a pointer to the shared channel, or NULL on failure.
 * Increments the reference count.  Thread-safe.
 */
GstTiRpmsgChan *gst_ti_rpmsg_chan_acquire (guint rproc_id, guint remote_ep);

/*
 * Decrements the reference count.  Closes and frees the channel when it
 * reaches zero.  Thread-safe.  Passing NULL is a no-op.
 */
void gst_ti_rpmsg_chan_release (GstTiRpmsgChan * chan);

/* Lock/unlock the channel's mutex around a send/recv pair. */
static inline void
gst_ti_rpmsg_chan_lock (GstTiRpmsgChan * chan)
{
  g_mutex_lock (&chan->mutex);
}

static inline void
gst_ti_rpmsg_chan_unlock (GstTiRpmsgChan * chan)
{
  g_mutex_unlock (&chan->mutex);
}

G_END_DECLS
#endif /* __GST_TI_RPMSG_CTX_H__ */
