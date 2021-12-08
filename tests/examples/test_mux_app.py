#!/usr/bin/env python3

# Copyright (c) [2021] Texas Instruments Incorporated
#
# All rights reserved not granted herein.
#
# Limited License.
#
# Texas Instruments Incorporated grants a world-wide, royalty-free,
# non-exclusive license under copyrights and patents it now or hereafter
# owns or controls to make, have made, use, import, offer to sell and sell
# ("Utilize") this software subject to the terms herein.  With respect to
# the foregoing patent license, such license is granted  solely to the extent
# that any such patent is necessary to Utilize the software alone.
# The patent license shall not apply to any combinations which include
# this software, other than combinations with devices manufactured by or
# for TI (“TI Devices”).  No hardware patent is licensed hereunder.
#
# Redistributions must preserve existing copyright notices and reproduce
# this license (including the above copyright notice and the disclaimer
# and (if applicable) source code license limitations below) in the
# documentation and/or other materials provided with the distribution
#
# Redistribution and use in binary form, without modification, are permitted
# provided that the following conditions are met:
#
# *	No reverse engineering, decompilation, or disassembly of this software
#      is permitted with respect to any software provided in binary form.
#
# *	Any redistribution and use are licensed by TI for use only with TI Devices.
#
# *	Nothing shall obligate TI to provide you with source code for the
#      software licensed and provided to you in object code.
#
# If software source code is provided to you, modification and redistribution
# of the source code are permitted provided that the following conditions are met:
#
# *	Any redistribution and use of the source code, including any resulting
#      derivative works, are licensed by TI for use only with TI Devices.
#
# *	Any redistribution and use of any object code compiled from the source
#      code and any resulting derivative works, are licensed by TI for use
#      only with TI Devices.
#
# Neither the name of Texas Instruments Incorporated nor the names of its
# suppliers may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# DISCLAIMER.
#
# THIS SOFTWARE IS PROVIDED BY TI AND TI’S LICENSORS "AS IS" AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL TI AND TI’S LICENSORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import time
import unittest
from unittest.mock import MagicMock

from gi.repository import Gst as gst
from gi.repository import GLib as glib

SECONDS_TO_NANOSECONDS = 1000000000

gsttiovx_mux_appsink_desc = "videotestsrc is-live=true ! video/x-raw,format=RGB,width=320,height=240,framerate=30/1,pixel-aspect-ratio=1/1 ! queue ! mux. tiovxmux name=mux sink_0::pool-size=4 ! perf ! appsink name=appsink sync=true qos=false emit-signals=true drop=true max-buffers=2"
gsttiovx_mux_appsrc_test_desc = "appsrc is-live=true name=appsrc ! fakesink"
gsttiovx_mux_appsrc_app_desc = "appsrc is-live=true name=appsrc do-timestamp=true format=time ! video/x-raw,format=RGB,width=320,height=240,framerate=30/1,pixel-aspect-ratio=1/1 ! videoconvert ! videoscale ! kmssink force-modesetting=true sync=false async=false qos=false"


class GstMediaError(RuntimeError):
    pass


class GstMedia():
    def __init__(self):
        gst.init(None)

        self._name = None
        self._pipeline = None

    def make_media(self, name, desc):
        try:
            self._pipeline = gst.parse_launch(desc)
            self._name = name
        except glib.GError as e:
            raise GstMediaError("Unable to make the media") from e

    def play_media(self):
        ret = self._pipeline.set_state(gst.State.PLAYING)
        if gst.StateChangeReturn.FAILURE == ret:
            raise GstMediaError("Unable to play the media")

    def stop_media(self):
        ret, current, pending = self._pipeline.get_state(gst.CLOCK_TIME_NONE)
        if gst.State.PLAYING == current:
            timeout = 2 * SECONDS_TO_NANOSECONDS
            self._pipeline.send_event(gst.Event.new_eos())
            self._pipeline.get_bus().timed_pop_filtered(timeout, gst.MessageType.EOS)
            ret = self._pipeline.set_state(gst.State.NULL)
            if gst.StateChangeReturn.FAILURE == ret:
                raise GstMediaError("Unable to stop the media")


class SinkCallback():
    def __init__(self):
        pass

    def __call__(self):
        pass


class BufferingMaster():
    def __init__(self, obj_sinker, obj_sourcer):
        self.obj_sinker = obj_sinker
        self.obj_sourcer = obj_sourcer

    def _sinker_callback_buffers_emitter(self, obj_sinker, data):
        sample = obj_sinker.emit("pull-sample")
        buffer = sample.get_buffer()

        self.obj_sourcer.emit("push-buffer", buffer)

        print("New sinker buffer emition")
        self.sinker_user_callback()

        return gst.FlowReturn.OK

    def sinker_user_callback(self, sinker_user_callback):
        self.sinker_user_callback = sinker_user_callback
        print("Installed the sinker user callback")

        try:
            self.obj_sinker.connect(
                "new-sample",
                self._sinker_callback_buffers_emitter,
                self.obj_sinker)
        except AttributeError as e:
            raise GstMediaError(
                "Unable to install sinker buffers emitter callback")


class TestGstMedia(unittest.TestCase):
    def setUp(self):
        self.desc = "videotestsrc ! fakesink"
        self.name = "test_media"

        self.obj_media = GstMedia()
        self.obj_media.make_media(self.name, self.desc)

    def test_play_media(self):
        media_state = self.obj_media._pipeline.get_state(gst.CLOCK_TIME_NONE)[
            1]
        self.assertEqual(gst.State.NULL, media_state)

        self.obj_media.play_media()

        media_state = self.obj_media._pipeline.get_state(gst.CLOCK_TIME_NONE)[
            1]
        self.assertEqual(gst.State.PLAYING, media_state)

        self.obj_media.stop_media()

        media_state = self.obj_media._pipeline.get_state(gst.CLOCK_TIME_NONE)[
            1]
        self.assertEqual(gst.State.NULL, media_state)


class TestBufferingMaster(unittest.TestCase):
    def setUp(self):
        self.desc_sinker = gsttiovx_mux_appsink_desc
        self.desc_sourcer = gsttiovx_mux_appsrc_test_desc

        self.obj_media_sinker = GstMedia()
        self.obj_media_sinker.make_media("media_sinker", self.desc_sinker)

        self.obj_media_sourcer = GstMedia()
        self.obj_media_sourcer.make_media("media_sourcer", self.desc_sourcer)

    def test_sinker_callback_buffers_emitter(self):
        sinker_appsink = self.obj_media_sinker._pipeline.get_by_name("appsink")
        sourcer_appsrc = self.obj_media_sourcer._pipeline.get_by_name("appsrc")
        obj_buffering_master = BufferingMaster(sinker_appsink, sourcer_appsrc)

        sink_callback = SinkCallback()
        sink_callback.__call__ = MagicMock()

        obj_buffering_master.sinker_user_callback(sink_callback.__call__())

        self.obj_media_sinker.play_media()
        self.obj_media_sourcer.play_media()

        sink_callback.__call__.assert_called_once()


class GstTIOVXMuxAppSinkAppSrcApp():
    def __init__(self):
        self.desc_sinker = gsttiovx_mux_appsink_desc
        self.desc_sourcer = gsttiovx_mux_appsrc_app_desc

        self.obj_media_sinker = GstMedia()
        self.obj_media_sinker.make_media("media_sinker", self.desc_sinker)

        self.obj_media_sourcer = GstMedia()
        self.obj_media_sourcer.make_media("media_sourcer", self.desc_sourcer)

    def run(self):
        sinker_appsink = self.obj_media_sinker._pipeline.get_by_name("appsink")
        sourcer_appsrc = self.obj_media_sourcer._pipeline.get_by_name("appsrc")
        obj_buffering_master = BufferingMaster(sinker_appsink, sourcer_appsrc)

        sink_callback = SinkCallback()

        obj_buffering_master.sinker_user_callback(sink_callback)

        self.obj_media_sinker.play_media()
        self.obj_media_sourcer.play_media()

        time.sleep(25)

        self.obj_media_sinker.stop_media()
        self.obj_media_sourcer.stop_media()


if __name__ == '__main__':
    app = GstTIOVXMuxAppSinkAppSrcApp()
    app.run()

    unittest.main()
