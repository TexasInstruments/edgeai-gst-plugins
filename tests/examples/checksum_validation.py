import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst

import hashlib
import os
import yaml


load_file = "checksums.yaml"
config = yaml.safe_load(open(load_file))

tmp_file = config["temporary_file"]

Gst.init(None)
if not Gst.init_check():
    print("Error loading gstreamer")
    exit()

for test in config["tests"]:
    files = []
    for output in test["output"]:
        files.append(tmp_file.format(output["file_id"]))

    pipeline = Gst.parse_launch(test["pipeline"].format(*files))
    pipeline.set_state(Gst.State.PLAYING)
    pipeline.get_bus().timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.EOS)
    pipeline.set_state(Gst.State.NULL)
    pipeline.get_state(Gst.CLOCK_TIME_NONE)

    status = "PASSED"
    for output in test["output"]:
        checksum = None
        file = tmp_file.format(output["file_id"])
        with open(file, "rb") as f:
            sha256_hash = hashlib.sha256()
            # Read and update hash string value in blocks of 4K
            for byte_block in iter(lambda: f.read(4096), b""):
                sha256_hash.update(byte_block)
            checksum = sha256_hash.hexdigest()
        os.remove(file)
        if (output["checksum"] != checksum):
            status = "FAILED"

    print("TEST: {} - {}".format(test["id"], status))

Gst.deinit()
